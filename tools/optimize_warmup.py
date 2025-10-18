#!/usr/bin/env python3
"""
Optuna-based parameter optimization for Sentio Lite warmup system.

This script optimizes trading and warmup parameters on historical data,
then applies the optimal configuration to a target test date.

Usage:
    # Optimize for a specific test date
    python3 tools/optimize_warmup.py --test-date 2025-10-18 --n-trials 100

    # Optimize for a date range
    python3 tools/optimize_warmup.py --start-date 2025-10-15 --end-date 2025-10-18 --n-trials 50

    # Use optimized parameters for live trading
    python3 tools/optimize_warmup.py --test-date 2025-10-18 --mode live --use-best
"""

import os
import sys
import json
import subprocess
import argparse
from datetime import datetime, timedelta
from pathlib import Path
from typing import Dict, Any, Optional, Tuple
import optuna
from optuna.pruners import MedianPruner
from optuna.samplers import TPESampler

# Constants
BINARY_PATH = "./build/sentio_lite"
RESULTS_FILE = "results.json"
BEST_PARAMS_FILE = "optimal_params.json"


class SentioOptimizer:
    """Optimize Sentio Lite parameters using Optuna."""

    def __init__(self, test_date: str, mode: str = "mock",
                 optimization_days: int = 30, warmup_days: int = 1):
        """
        Initialize optimizer.

        Args:
            test_date: Target test date (YYYY-MM-DD)
            mode: 'mock' or 'live'
            optimization_days: Days of historical data for optimization
            warmup_days: Predictor warmup days
        """
        self.test_date = datetime.strptime(test_date, "%Y-%m-%d")
        self.mode = mode
        self.optimization_days = optimization_days
        self.warmup_days = warmup_days

        # Calculate optimization period (before test date)
        self.opt_end_date = self.test_date - timedelta(days=1)
        # Add extra days to account for weekends (multiply by 1.4 to account for weekends)
        calendar_days = int(optimization_days * 1.4)
        self.opt_start_date = self.opt_end_date - timedelta(days=calendar_days)

        # Ensure we don't go before available data (April 1, 2025)
        min_date = datetime(2025, 4, 1)
        if self.opt_start_date < min_date:
            self.opt_start_date = min_date
            print(f"⚠️  Adjusted start date to data availability: {self.opt_start_date.strftime('%Y-%m-%d')}")

        print(f"Optimization Configuration:")
        print(f"  Test Date: {self.test_date.strftime('%Y-%m-%d')}")
        print(f"  Optimization Period: {self.opt_start_date.strftime('%Y-%m-%d')} to {self.opt_end_date.strftime('%Y-%m-%d')}")
        print(f"  Mode: {mode}")
        print(f"  Warmup Days: {warmup_days}")

    def run_backtest(self, params: Dict[str, Any]) -> Optional[Dict[str, float]]:
        """
        Run backtest with given parameters.

        Args:
            params: Dictionary of parameters

        Returns:
            Dictionary with metrics or None if failed
        """
        cmd = [
            BINARY_PATH,
            "mock",
            "--start-date", self.opt_start_date.strftime("%Y-%m-%d"),
            "--end-date", self.opt_end_date.strftime("%Y-%m-%d"),
            "--warmup-days", str(self.warmup_days),
            "--no-dashboard",
            "--results-file", RESULTS_FILE,

            # Trading parameters
            "--capital", str(params["capital"]),
            "--max-positions", str(params["max_positions"]),
            "--stop-loss", str(params["stop_loss"]),
            "--profit-target", str(params["profit_target"]),
            "--lambda", str(params["lambda"]),
        ]

        # Add warmup parameters
        if params.get("enable_warmup", False):
            cmd.extend([
                "--enable-warmup",
                "--warmup-obs-days", str(params["warmup_obs_days"]),
                "--warmup-sim-days", str(params["warmup_sim_days"]),
            ])

        try:
            # Run backtest
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=120
            )

            if result.returncode != 0:
                print(f"  ❌ Backtest failed: {result.stderr[:200]}")
                return None

            # Parse results from JSON file
            if os.path.exists(RESULTS_FILE):
                with open(RESULTS_FILE, 'r') as f:
                    results = json.load(f)
                return results
            else:
                # Parse from stdout if JSON file not available
                return self._parse_output(result.stdout)

        except subprocess.TimeoutExpired:
            print("  ⏱️  Backtest timeout")
            return None
        except Exception as e:
            print(f"  ❌ Error: {e}")
            return None

    def _parse_output(self, output: str) -> Dict[str, float]:
        """Parse backtest output when JSON not available."""
        metrics = {}

        for line in output.split('\n'):
            if "Total Return:" in line:
                try:
                    metrics['total_return'] = float(line.split(':')[1].strip().rstrip('%'))
                except:
                    pass
            elif "Win Rate:" in line:
                try:
                    metrics['win_rate'] = float(line.split(':')[1].strip().rstrip('%'))
                except:
                    pass
            elif "Profit Factor:" in line:
                try:
                    metrics['profit_factor'] = float(line.split(':')[1].strip())
                except:
                    pass
            elif "Total Trades:" in line:
                try:
                    metrics['total_trades'] = int(line.split(':')[1].strip())
                except:
                    pass
            elif "MRD" in line and "Daily" in line:
                try:
                    parts = line.split(':')
                    if len(parts) >= 2:
                        metrics['mrd'] = float(parts[1].strip().split()[0].rstrip('%'))
                except:
                    pass

        return metrics

    def objective(self, trial: optuna.Trial) -> float:
        """
        Optuna objective function.

        Args:
            trial: Optuna trial

        Returns:
            Objective value (higher is better)
        """
        # Suggest parameters
        # NOTE: Wider ranges for volatile leveraged ETFs (TQQQ, SOXL, etc.)
        params = {
            # Capital and risk management
            "capital": 100000.0,  # Fixed
            "max_positions": trial.suggest_int("max_positions", 2, 8),

            # WIDER ranges for volatile instruments
            "stop_loss": trial.suggest_float("stop_loss", -0.05, -0.01, step=0.001),  # Was -0.03 to -0.01
            "profit_target": trial.suggest_float("profit_target", 0.01, 0.10, step=0.005),  # Was 0.02 to 0.08

            # EWRLS learning rate - wider range for adaptation
            "lambda": trial.suggest_float("lambda", 0.950, 0.999, step=0.001),  # Was 0.980 to 0.998

            # Warmup configuration
            "enable_warmup": trial.suggest_categorical("enable_warmup", [True, False]),
            "warmup_obs_days": trial.suggest_int("warmup_obs_days", 1, 5),
            "warmup_sim_days": trial.suggest_int("warmup_sim_days", 3, 10),
        }

        # Run backtest
        results = self.run_backtest(params)

        if results is None:
            return -1000.0  # Penalty for failed runs

        # Extract metrics from 'performance' section
        # JSON structure: {"metadata": {...}, "performance": {...}, "config": {...}}
        perf = results.get('performance', {})

        total_return = perf.get('total_return', -1.0) * 100  # Convert to percentage
        profit_factor = perf.get('profit_factor', 0.0)
        win_rate = perf.get('win_rate', 0.0) * 100  # Convert to percentage
        total_trades = perf.get('total_trades', 0)
        mrd = perf.get('mrd', -1.0) * 100  # Convert to percentage

        # Calculate Sharpe proxy (if not available)
        # Sharpe ≈ MRD / (volatility), we'll use MRD as proxy
        sharpe_proxy = mrd if mrd > -50 else -10.0

        # Multi-objective score
        # Primary: Sharpe/MRD (50%)
        # Secondary: Profit Factor (25%)
        # Tertiary: Win Rate (15%)
        # Penalty: Too few trades (10%)

        score = (
            sharpe_proxy * 0.5 +                          # Sharpe proxy
            (profit_factor - 1.0) * 10 * 0.25 +           # Profit factor (normalize)
            (win_rate - 50) * 0.15 +                      # Win rate (normalize)
            (min(total_trades, 100) / 100) * 10 * 0.1     # Trade count (up to 100)
        )

        # Report intermediate results
        trial.set_user_attr("total_return", total_return)
        trial.set_user_attr("profit_factor", profit_factor)
        trial.set_user_attr("win_rate", win_rate)
        trial.set_user_attr("sharpe_proxy", sharpe_proxy)
        trial.set_user_attr("total_trades", total_trades)

        print(f"  Trial {trial.number}: Score={score:.3f} | Return={total_return:.2f}% | "
              f"PF={profit_factor:.2f} | WR={win_rate:.1f}% | Trades={total_trades}")

        return score

    def optimize(self, n_trials: int = 100, n_jobs: int = 1) -> optuna.Study:
        """
        Run optimization.

        Args:
            n_trials: Number of trials
            n_jobs: Number of parallel jobs

        Returns:
            Optuna study object
        """
        print(f"\n🔍 Starting optimization with {n_trials} trials...")
        print(f"=" * 70)

        study = optuna.create_study(
            direction="maximize",
            sampler=TPESampler(seed=42),
            pruner=MedianPruner(n_startup_trials=10, n_warmup_steps=20)
        )

        study.optimize(
            self.objective,
            n_trials=n_trials,
            n_jobs=n_jobs,
            show_progress_bar=True
        )

        print(f"\n{'=' * 70}")
        print(f"✅ Optimization complete!")
        print(f"\nBest trial: {study.best_trial.number}")
        print(f"Best score: {study.best_value:.3f}")
        print(f"\nBest parameters:")
        for key, value in study.best_params.items():
            print(f"  {key}: {value}")

        print(f"\nBest metrics:")
        for key, value in study.best_trial.user_attrs.items():
            print(f"  {key}: {value}")

        return study

    def save_best_params(self, study: optuna.Study, filepath: str = BEST_PARAMS_FILE):
        """Save best parameters to JSON file."""
        best_params = {
            "test_date": self.test_date.strftime("%Y-%m-%d"),
            "optimization_period": {
                "start": self.opt_start_date.strftime("%Y-%m-%d"),
                "end": self.opt_end_date.strftime("%Y-%m-%d")
            },
            "parameters": study.best_params,
            "metrics": study.best_trial.user_attrs,
            "score": study.best_value,
            "trial_number": study.best_trial.number,
            "timestamp": datetime.now().isoformat()
        }

        with open(filepath, 'w') as f:
            json.dump(best_params, f, indent=2)

        print(f"\n💾 Best parameters saved to {filepath}")

    def run_final_test(self, params: Dict[str, Any],
                      start_date: Optional[str] = None,
                      end_date: Optional[str] = None) -> Dict[str, Any]:
        """
        Run final test with optimized parameters.

        Args:
            params: Optimized parameters
            start_date: Test start date (defaults to test_date)
            end_date: Test end date (optional, for multi-day)

        Returns:
            Test results
        """
        test_start = start_date or self.test_date.strftime("%Y-%m-%d")

        print(f"\n🚀 Running final test with optimized parameters...")
        print(f"  Test period: {test_start}" + (f" to {end_date}" if end_date else ""))

        cmd = [
            BINARY_PATH,
            self.mode,
            "--warmup-days", str(self.warmup_days),
            "--results-file", "final_results.json",

            # Trading parameters
            "--capital", str(params["capital"]),
            "--max-positions", str(params["max_positions"]),
            "--stop-loss", str(params["stop_loss"]),
            "--profit-target", str(params["profit_target"]),
            "--lambda", str(params["lambda"]),
        ]

        # Add date parameters
        if end_date:
            cmd.extend(["--start-date", test_start, "--end-date", end_date])
        else:
            cmd.extend(["--date", test_start])

        # Add warmup parameters
        if params.get("enable_warmup", False):
            cmd.extend([
                "--enable-warmup",
                "--warmup-obs-days", str(params["warmup_obs_days"]),
                "--warmup-sim-days", str(params["warmup_sim_days"]),
            ])

        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode == 0:
            print(f"✅ Final test completed successfully!")
            print(result.stdout)
        else:
            print(f"❌ Final test failed!")
            print(result.stderr)

        # Parse results
        if os.path.exists("final_results.json"):
            with open("final_results.json", 'r') as f:
                return json.load(f)
        else:
            return self._parse_output(result.stdout)


def main():
    parser = argparse.ArgumentParser(
        description="Optimize Sentio Lite warmup and trading parameters"
    )

    # Test configuration
    parser.add_argument("--test-date", type=str,
                       help="Target test date (YYYY-MM-DD)")
    parser.add_argument("--start-date", type=str,
                       help="Test start date for multi-day (YYYY-MM-DD)")
    parser.add_argument("--end-date", type=str,
                       help="Test end date for multi-day (YYYY-MM-DD)")
    parser.add_argument("--mode", type=str, default="mock",
                       choices=["mock", "live"],
                       help="Testing mode (mock or live)")

    # Optimization configuration
    parser.add_argument("--n-trials", type=int, default=100,
                       help="Number of optimization trials")
    parser.add_argument("--optimization-days", type=int, default=30,
                       help="Days of historical data for optimization")
    parser.add_argument("--warmup-days", type=int, default=1,
                       help="Predictor warmup days")
    parser.add_argument("--n-jobs", type=int, default=1,
                       help="Number of parallel jobs")

    # Execution mode
    parser.add_argument("--use-best", action="store_true",
                       help="Use previously optimized parameters (skip optimization)")
    parser.add_argument("--params-file", type=str, default=BEST_PARAMS_FILE,
                       help="Path to saved parameters file")

    args = parser.parse_args()

    # Determine test date
    if args.test_date:
        test_date = args.test_date
    elif args.start_date:
        test_date = args.start_date
    else:
        # Default to tomorrow
        test_date = (datetime.now() + timedelta(days=1)).strftime("%Y-%m-%d")
        print(f"No test date specified, using tomorrow: {test_date}")

    # Create optimizer
    optimizer = SentioOptimizer(
        test_date=test_date,
        mode=args.mode,
        optimization_days=args.optimization_days,
        warmup_days=args.warmup_days
    )

    # Load or optimize parameters
    if args.use_best and os.path.exists(args.params_file):
        print(f"\n📂 Loading previously optimized parameters from {args.params_file}")
        with open(args.params_file, 'r') as f:
            saved = json.load(f)
            best_params = saved["parameters"]
            best_params["capital"] = 100000.0  # Ensure capital is set

        print(f"\nLoaded parameters:")
        for key, value in best_params.items():
            print(f"  {key}: {value}")
    else:
        # Run optimization
        study = optimizer.optimize(n_trials=args.n_trials, n_jobs=args.n_jobs)

        # Save best parameters
        optimizer.save_best_params(study, args.params_file)

        best_params = study.best_params
        best_params["capital"] = 100000.0  # Ensure capital is set

    # Run final test
    if args.mode == "mock":
        results = optimizer.run_final_test(
            best_params,
            start_date=args.start_date or test_date,
            end_date=args.end_date
        )

        print(f"\n📊 Final Test Results:")
        for key, value in results.items():
            print(f"  {key}: {value}")
    else:
        print(f"\n⚠️  Live mode detected. Parameters optimized and ready.")
        print(f"    Run the final test manually with the parameters in {args.params_file}")


if __name__ == "__main__":
    main()
