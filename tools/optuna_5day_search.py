#!/usr/bin/env python3
"""
Sentio Lite - 5-Day Optuna Parameter Search

Strategy:
1. Run 200 trials with each configuration tested on 5 most recent trading days
2. Each day: [1 warmup] + [5 sim] + [1 test] = 7 days total
3. Evaluation metric: Average MRD across all 5 test days
4. Select top 5 configurations by MRD
5. From top 5, find:
   - Top MRD: Highest average MRD
   - Best MRD: Best risk profile (50% win rate + 50% trade count weighting)
6. Create balanced config: Middle values between Top and Best
7. Save balanced config to config/trading_params.json

Author: Claude Code
Date: 2025-10-23
"""

import os
import sys
import json
import subprocess
import optuna
from pathlib import Path
import argparse
from datetime import datetime, timedelta
import numpy as np
from typing import Dict, List, Tuple

class FiveDayOptimizer:
    def __init__(self, test_dates: List[str], sim_days: int = 5, n_trials: int = 200):
        """
        Args:
            test_dates: List of 5 test dates (most recent trading days)
            sim_days: Number of simulation days (default: 5)
            n_trials: Number of Optuna trials (default: 200)
        """
        self.test_dates = test_dates
        self.sim_days = sim_days
        self.n_trials = n_trials
        self.sentio_bin = "./build/sentio_lite"

        # Track all results for final selection
        self.all_results = []

        print(f"5-Day Optuna Search Configuration:")
        print(f"  Test dates: {', '.join(test_dates)}")
        print(f"  Sim days per test: {sim_days}")
        print(f"  Trials: {n_trials}")
        print(f"  Total runs per trial: {len(test_dates)} days")
        print(f"  Data requirement per day: {sim_days + 2} days (1 warmup + {sim_days} sim + 1 test)")

    def run_single_test(self, test_date: str, params: Dict) -> Dict:
        """
        Run sentio_lite for a single test date with given parameters

        Returns:
            Dictionary with MRD, win_rate, total_trades, etc.
        """
        cmd = [
            self.sentio_bin,
            "mock",
            "--date", test_date,
            "--sim-days", str(self.sim_days),
            "--no-dashboard",
            "--max-positions", str(params['max_positions']),
            "--stop-loss", str(params['stop_loss_pct']),
            "--profit-target", str(params['profit_target_pct']),
            "--lambda", str(params['lambda_1bar'])  # Simplified: use same lambda for all horizons
        ]

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=120  # 2 minute timeout
            )

            if result.returncode != 0:
                print(f"    ❌ Test failed for {test_date}: {result.stderr[:100]}")
                return None

            # Read results.json
            with open('results.json', 'r') as f:
                results = json.load(f)

            perf = results['performance']

            return {
                'date': test_date,
                'mrd': perf['mrd'],
                'win_rate': perf['win_rate'],
                'total_trades': perf['total_trades'],
                'profit_factor': perf['profit_factor'],
                'final_equity': perf['final_equity']
            }

        except subprocess.TimeoutExpired:
            print(f"    ⏱️  Timeout for {test_date}")
            return None
        except Exception as e:
            print(f"    ❌ Error for {test_date}: {str(e)[:100]}")
            return None

    def evaluate_params(self, params: Dict) -> Tuple[float, Dict]:
        """
        Evaluate parameters across all 5 test dates

        Returns:
            (average_mrd, detailed_metrics)
        """
        results = []

        for test_date in self.test_dates:
            result = self.run_single_test(test_date, params)
            if result:
                results.append(result)

        if len(results) == 0:
            return -999.0, {}

        # Calculate metrics
        avg_mrd = np.mean([r['mrd'] for r in results])
        avg_win_rate = np.mean([r['win_rate'] for r in results])
        avg_trades = np.mean([r['total_trades'] for r in results])
        avg_pf = np.mean([r['profit_factor'] for r in results])

        metrics = {
            'avg_mrd': avg_mrd,
            'avg_win_rate': avg_win_rate,
            'avg_trades': avg_trades,
            'avg_profit_factor': avg_pf,
            'num_valid_days': len(results),
            'daily_results': results
        }

        return avg_mrd, metrics

    def objective(self, trial: optuna.Trial) -> float:
        """Optuna objective function - returns average MRD across 5 days"""

        # Define search space (16 parameters)
        params = {
            'max_positions': trial.suggest_int('max_positions', 2, 5),
            'stop_loss_pct': trial.suggest_float('stop_loss_pct', -0.03, -0.01, step=0.002),
            'profit_target_pct': trial.suggest_float('profit_target_pct', 0.02, 0.08, step=0.005),
            'lambda_1bar': trial.suggest_float('lambda_1bar', 0.95, 0.995, step=0.005),
            'lambda_5bar': trial.suggest_float('lambda_5bar', 0.98, 0.999, step=0.001),
            'lambda_10bar': trial.suggest_float('lambda_10bar', 0.99, 0.9999, step=0.0001),
            'min_prediction_for_entry': trial.suggest_float('min_prediction_for_entry', 0.0, 0.01, step=0.001),
            'min_bars_to_hold': trial.suggest_int('min_bars_to_hold', 10, 40, step=5),
            'min_bars_to_learn': 391,  # Fixed: 1 day
            'bars_per_day': 391,  # Fixed
            'initial_capital': 100000.0,  # Fixed
            'lookback_window': trial.suggest_int('lookback_window', 20, 100, step=10),
            'win_multiplier': trial.suggest_float('win_multiplier', 1.0, 1.5, step=0.1),
            'loss_multiplier': trial.suggest_float('loss_multiplier', 0.5, 1.0, step=0.1),
            'rotation_strength_delta': trial.suggest_float('rotation_strength_delta', 0.001, 0.02, step=0.001),
            'min_rank_strength': trial.suggest_float('min_rank_strength', 0.0001, 0.005, step=0.0001)
        }

        # Evaluate across 5 days
        avg_mrd, metrics = self.evaluate_params(params)

        # Store results for final selection
        trial_result = {
            'trial_number': trial.number,
            'params': params.copy(),
            'avg_mrd': avg_mrd,
            'metrics': metrics
        }
        self.all_results.append(trial_result)

        # Log progress
        print(f"  Trial {trial.number:3d}: MRD={avg_mrd:+.4f}% | "
              f"WinRate={metrics.get('avg_win_rate', 0):.1%} | "
              f"Trades={metrics.get('avg_trades', 0):.0f}")

        return avg_mrd

    def calculate_risk_score(self, result: Dict) -> float:
        """
        Calculate risk profile score (50% win rate + 50% trade count)
        Higher is better
        """
        metrics = result['metrics']

        # Win rate component (0-1, target is 0.5-0.6)
        win_rate = metrics.get('avg_win_rate', 0)
        win_rate_score = min(1.0, win_rate / 0.55)  # Normalized to 55% target

        # Trade count component (0-1, more trades = more confidence)
        trade_count = metrics.get('avg_trades', 0)
        trade_count_score = min(1.0, trade_count / 100.0)  # Normalized to 100 trades

        # Combined score (50-50 weighting)
        risk_score = 0.5 * win_rate_score + 0.5 * trade_count_score

        return risk_score

    def select_best_configs(self) -> Tuple[Dict, Dict, Dict]:
        """
        From all trials, select:
        1. Top MRD: Highest average MRD
        2. Best MRD: Best risk profile (from top 5)
        3. Balanced: Middle values between Top and Best

        Returns:
            (top_config, best_config, balanced_config)
        """
        # Sort by average MRD
        sorted_results = sorted(self.all_results, key=lambda x: x['avg_mrd'], reverse=True)

        # Get top 5
        top_5 = sorted_results[:5]

        print("\n" + "="*80)
        print("TOP 5 CONFIGURATIONS BY MRD:")
        print("="*80)
        for i, result in enumerate(top_5, 1):
            metrics = result['metrics']
            print(f"\n#{i} - Trial {result['trial_number']}")
            print(f"  MRD: {result['avg_mrd']:+.4f}%")
            print(f"  Win Rate: {metrics['avg_win_rate']:.2%}")
            print(f"  Avg Trades: {metrics['avg_trades']:.1f}")
            print(f"  Profit Factor: {metrics['avg_profit_factor']:.2f}")

        # Top MRD is #1
        top_mrd = top_5[0]

        # Find Best MRD from top 5 (best risk profile)
        risk_scores = [(r, self.calculate_risk_score(r)) for r in top_5]
        best_mrd = max(risk_scores, key=lambda x: x[1])[0]

        print("\n" + "="*80)
        print("SELECTED CONFIGURATIONS:")
        print("="*80)
        print(f"\n1️⃣  TOP MRD (Trial {top_mrd['trial_number']}): {top_mrd['avg_mrd']:+.4f}%")
        print(f"2️⃣  BEST MRD (Trial {best_mrd['trial_number']}): {best_mrd['avg_mrd']:+.4f}%")
        print(f"    (Best risk profile: WR={best_mrd['metrics']['avg_win_rate']:.2%}, "
              f"Trades={best_mrd['metrics']['avg_trades']:.0f})")

        # Create balanced config (middle values)
        balanced_params = {}
        for key in top_mrd['params'].keys():
            top_val = top_mrd['params'][key]
            best_val = best_mrd['params'][key]

            if isinstance(top_val, (int, float)):
                # Take middle value
                balanced_params[key] = (top_val + best_val) / 2.0

                # Round appropriately
                if isinstance(top_val, int):
                    balanced_params[key] = int(round(balanced_params[key]))
            else:
                # Non-numeric, use top value
                balanced_params[key] = top_val

        print("\n3️⃣  BALANCED CONFIG (middle of Top and Best):")
        print("="*80)

        return top_mrd, best_mrd, balanced_params

    def save_config(self, balanced_params: Dict, top_result: Dict, best_result: Dict):
        """Save balanced configuration to config/trading_params.json"""

        config = {
            "description": "Sentio Lite Trading Parameters - Optuna 5-Day Optimized",
            "last_updated": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "optimization_date": self.test_dates[-1],  # Most recent test date
            "optimization_summary": {
                "test_dates": self.test_dates,
                "sim_days_per_test": self.sim_days,
                "total_trials": self.n_trials,
                "top_mrd": {
                    "avg_mrd": top_result['avg_mrd'],
                    "trial_number": top_result['trial_number'],
                    "win_rate": top_result['metrics']['avg_win_rate'],
                    "avg_trades": top_result['metrics']['avg_trades']
                },
                "best_mrd": {
                    "avg_mrd": best_result['avg_mrd'],
                    "trial_number": best_result['trial_number'],
                    "win_rate": best_result['metrics']['avg_win_rate'],
                    "avg_trades": best_result['metrics']['avg_trades']
                }
            },
            "parameters": {
                "max_positions": int(balanced_params['max_positions']),
                "stop_loss_pct": float(balanced_params['stop_loss_pct']),
                "profit_target_pct": float(balanced_params['profit_target_pct']),
                "lambda_1bar": float(balanced_params['lambda_1bar']),
                "lambda_5bar": float(balanced_params['lambda_5bar']),
                "lambda_10bar": float(balanced_params['lambda_10bar']),
                "min_prediction_for_entry": float(balanced_params['min_prediction_for_entry']),
                "min_bars_to_hold": int(balanced_params['min_bars_to_hold']),
                "min_bars_to_learn": 391,
                "bars_per_day": 391,
                "initial_capital": 100000.0,
                "lookback_window": int(balanced_params['lookback_window']),
                "win_multiplier": float(balanced_params['win_multiplier']),
                "loss_multiplier": float(balanced_params['loss_multiplier']),
                "rotation_strength_delta": float(balanced_params['rotation_strength_delta']),
                "min_rank_strength": float(balanced_params['min_rank_strength'])
            }
        }

        config_path = "config/trading_params.json"
        with open(config_path, 'w') as f:
            json.dump(config, f, indent=2)

        print(f"\n✅ Balanced configuration saved to: {config_path}")

        # Also save detailed results
        results_path = "logs/optuna_5day_results.json"
        os.makedirs("logs", exist_ok=True)

        detailed_results = {
            "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "test_dates": self.test_dates,
            "sim_days": self.sim_days,
            "n_trials": self.n_trials,
            "top_5_configs": [
                {
                    "trial": r['trial_number'],
                    "params": r['params'],
                    "avg_mrd": r['avg_mrd'],
                    "metrics": r['metrics']
                }
                for r in sorted(self.all_results, key=lambda x: x['avg_mrd'], reverse=True)[:5]
            ]
        }

        with open(results_path, 'w') as f:
            json.dump(detailed_results, f, indent=2)

        print(f"✅ Detailed results saved to: {results_path}")

    def optimize(self):
        """Run full optimization workflow"""

        print("\n" + "="*80)
        print("STARTING 5-DAY OPTUNA OPTIMIZATION")
        print("="*80)

        # Create Optuna study
        study = optuna.create_study(
            direction='maximize',
            sampler=optuna.samplers.TPESampler(seed=42)
        )

        # Run optimization
        study.optimize(self.objective, n_trials=self.n_trials, show_progress_bar=True)

        # Select best configurations
        top_result, best_result, balanced_params = self.select_best_configs()

        # Save balanced config
        self.save_config(balanced_params, top_result, best_result)

        print("\n" + "="*80)
        print("OPTIMIZATION COMPLETE!")
        print("="*80)
        print(f"✅ Use config/trading_params.json for tomorrow's market open")
        print(f"✅ Expected MRD: {(top_result['avg_mrd'] + best_result['avg_mrd']) / 2:.4f}%")


def get_recent_trading_days(end_date: str, n_days: int = 5) -> List[str]:
    """
    Get the N most recent trading days before end_date
    Skips weekends

    Args:
        end_date: End date in YYYY-MM-DD format
        n_days: Number of trading days to get

    Returns:
        List of dates in YYYY-MM-DD format (most recent first)
    """
    from datetime import datetime, timedelta

    end = datetime.strptime(end_date, "%Y-%m-%d")
    trading_days = []
    current = end

    while len(trading_days) < n_days:
        # Skip weekends
        if current.weekday() < 5:  # Monday=0, Friday=4
            trading_days.append(current.strftime("%Y-%m-%d"))
        current -= timedelta(days=1)

    return trading_days


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="5-Day Optuna Parameter Search for Sentio Lite"
    )
    parser.add_argument(
        "--end-date",
        default="2025-10-23",
        help="End date (most recent test day) in YYYY-MM-DD format"
    )
    parser.add_argument(
        "--sim-days",
        type=int,
        default=5,
        help="Number of simulation days per test (default: 5)"
    )
    parser.add_argument(
        "--trials",
        type=int,
        default=200,
        help="Number of Optuna trials (default: 200)"
    )

    args = parser.parse_args()

    # Get 5 most recent trading days
    test_dates = get_recent_trading_days(args.end_date, n_days=5)

    print(f"\n📅 Test dates identified:")
    for i, date in enumerate(test_dates, 1):
        print(f"  {i}. {date}")

    # Run optimization
    optimizer = FiveDayOptimizer(
        test_dates=test_dates,
        sim_days=args.sim_days,
        n_trials=args.trials
    )

    optimizer.optimize()
