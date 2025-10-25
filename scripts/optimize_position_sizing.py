#!/usr/bin/env python3
"""
Position Sizing Optimization with Optuna

Optimizes position sizing parameters to maximize MRD while controlling risk:
- fractional_kelly: How conservative to be with Kelly sizing
- expected_win_pct / expected_loss_pct: Risk/reward calibration
- volatility adjustment parameters

Uses Optuna's TPE sampler for efficient hyperparameter search.
"""

import json
import subprocess
import optuna
from typing import Dict, List, Tuple
import sys
from datetime import datetime
import numpy as np
import os

# Optimization configuration - MAXIMIZE MRD AGGRESSIVELY
# Use 5-day training set, then validate on 10-day test set with 20% overfitting check
TRAIN_DATES = ["2025-10-14", "2025-10-15", "2025-10-16", "2025-10-17", "2025-10-18"]
TEST_DATES = ["2025-10-20", "2025-10-21", "2025-10-22", "2025-10-23", "2025-10-24",
              "2025-10-27", "2025-10-28", "2025-10-29", "2025-10-30", "2025-10-31"]

OVERFITTING_THRESHOLD = 0.20  # Reject if test MRD drops >20% from train MRD

CONFIG_FILE = "config/trading_params.json"
BACKUP_FILE = "config/trading_params.json.bak"
RESULTS_FILE = "results/position_sizing_optimization/optuna_results.json"

class PositionSizingOptimizer:
    def __init__(self, train_dates: List[str], test_dates: List[str], n_trials: int = 100):
        self.train_dates = train_dates
        self.test_dates = test_dates
        self.n_trials = n_trials
        self.sentio_bin = "./build/sentio_lite"

        # Track all results
        self.all_results = []

        print("=" * 80)
        print("POSITION SIZING OPTIMIZATION - MAXIMIZE MRD")
        print("=" * 80)
        print(f"  Training dates: {', '.join(train_dates)} ({len(train_dates)} days)")
        print(f"  Test dates: {', '.join(test_dates)} ({len(test_dates)} days)")
        print(f"  Trials: {n_trials}")
        print(f"  Strategy: AGGRESSIVE position sizing for MAX MRD")
        print(f"  Overfitting check: Reject if test MRD < train MRD * 0.8")
        print(f"  Goal: WIN BIG, not lose less")
        print("=" * 80)
        print()

        # Backup original config
        with open(CONFIG_FILE) as f:
            self.original_config = json.load(f)

        with open(BACKUP_FILE, 'w') as f:
            json.dump(self.original_config, f, indent=2)

    def run_backtest(self, date: str) -> Dict:
        """Run backtest on a single date and return metrics"""
        cmd = [self.sentio_bin, "mock", "--date", date, "--no-dashboard"]

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            output = result.stdout

            # Parse metrics
            mrd = 0.0
            winrate = 0.0
            trades = 0

            for line in output.split('\n'):
                if "MRD (Daily):" in line:
                    mrd = float(line.split()[2].rstrip('%'))
                elif "Win Rate:" in line:
                    winrate = float(line.split()[2].rstrip('%'))
                elif "Total Trades:" in line:
                    trades = int(line.split()[2])

            return {"mrd": mrd, "winrate": winrate, "trades": trades}

        except subprocess.TimeoutExpired:
            return {"mrd": -999, "winrate": 0, "trades": 0}
        except Exception as e:
            return {"mrd": -999, "winrate": 0, "trades": 0}

    def evaluate_params(self, params: Dict, dates: List[str]) -> Tuple[float, int]:
        """Evaluate position sizing parameters on multiple dates"""
        # Update config file
        with open(CONFIG_FILE) as f:
            config = json.load(f)

        # Update position_sizing section
        if "position_sizing" not in config["parameters"]:
            config["parameters"]["position_sizing"] = {}

        for param, value in params.items():
            config["parameters"]["position_sizing"][param] = value

        with open(CONFIG_FILE, 'w') as f:
            json.dump(config, f, indent=2)

        # Rebuild (silent)
        subprocess.run(["cmake", "--build", "build"],
                      capture_output=True, timeout=60)

        # Test on all dates
        total_mrd = 0.0
        total_trades = 0

        for date in dates:
            metrics = self.run_backtest(date)
            total_mrd += metrics["mrd"]
            total_trades += metrics["trades"]

        avg_mrd = total_mrd / len(dates)
        return avg_mrd, total_trades

    def objective(self, trial: optuna.Trial) -> float:
        """Optuna objective function - MAXIMIZE MRD on TRAINING set"""

        # Sample position sizing parameters - WIDER RANGES for aggressive optimization
        params = {
            # Kelly Criterion parameters - allow MORE aggressive
            'fractional_kelly': trial.suggest_float('fractional_kelly', 0.1, 1.0, step=0.05),
            'expected_win_pct': trial.suggest_float('expected_win_pct', 0.005, 0.06, step=0.005),
            'expected_loss_pct': trial.suggest_float('expected_loss_pct', 0.005, 0.04, step=0.005),

            # Position limits - allow LARGER positions
            'min_position_pct': trial.suggest_float('min_position_pct', 0.01, 0.15, step=0.01),
            'max_position_pct': trial.suggest_float('max_position_pct', 0.15, 0.50, step=0.05),

            # Volatility adjustment - keep or drop
            'enable_volatility_adjustment': trial.suggest_categorical('enable_volatility_adjustment', [True, False]),
            'volatility_lookback': trial.suggest_int('volatility_lookback', 5, 40, step=5),
            'max_volatility_reduce': trial.suggest_float('max_volatility_reduce', 0.2, 0.9, step=0.1),
        }

        print(f"\n  Trial {trial.number + 1}/{self.n_trials}:")
        print(f"    Kelly: {params['fractional_kelly']:.2f}, "
              f"Win/Loss: {params['expected_win_pct']:.3f}/{params['expected_loss_pct']:.3f}")
        print(f"    Limits: {params['min_position_pct']:.2f}-{params['max_position_pct']:.2f}, "
              f"Vol Adj: {params['enable_volatility_adjustment']}")

        # Evaluate on TRAINING set (maximize MRD here)
        train_mrd, train_trades = self.evaluate_params(params, self.train_dates)

        print(f"    Train MRD: {train_mrd:.3f}%, Trades: {train_trades}")

        # NO penalty for fewer trades - we want MAX MRD, period
        # Trade count is just for info

        # Store results (will evaluate test set later for best trial)
        trial_result = {
            'trial': trial.number,
            'params': params.copy(),
            'train_mrd': train_mrd,
            'train_trades': train_trades
        }
        self.all_results.append(trial_result)

        # Return TRAINING MRD (Optuna will maximize)
        return train_mrd

    def run_optimization(self):
        """Run Optuna optimization - maximize TRAINING MRD, then validate on TEST set"""
        print("\n🔍 Starting position sizing optimization...\n")

        # Create study
        study = optuna.create_study(
            direction='maximize',
            study_name='position_sizing_optimization',
            sampler=optuna.samplers.TPESampler(seed=42)
        )

        # Run optimization on TRAINING set
        study.optimize(self.objective, n_trials=self.n_trials, show_progress_bar=True)

        print(f"\n✅ Optimization complete!")
        print(f"\n{'='*80}")
        print(f"BEST PARAMETERS - OPTIMIZED ON TRAINING SET")
        print(f"{'='*80}")

        best_trial = study.best_trial
        print(f"  Trial number: {best_trial.number}")
        print(f"  Train MRD: {best_trial.value:.3f}%")

        print(f"\n  Kelly Criterion:")
        print(f"    fractional_kelly: {best_trial.params['fractional_kelly']:.2f}")
        print(f"    expected_win_pct: {best_trial.params['expected_win_pct']:.3f}")
        print(f"    expected_loss_pct: {best_trial.params['expected_loss_pct']:.3f}")
        print(f"    win/loss ratio: {best_trial.params['expected_win_pct']/best_trial.params['expected_loss_pct']:.2f}")

        print(f"\n  Position Limits:")
        print(f"    min_position_pct: {best_trial.params['min_position_pct']:.2f}")
        print(f"    max_position_pct: {best_trial.params['max_position_pct']:.2f}")

        print(f"\n  Volatility Adjustment:")
        print(f"    enabled: {best_trial.params['enable_volatility_adjustment']}")
        print(f"    lookback: {best_trial.params['volatility_lookback']}")
        print(f"    max_reduce: {best_trial.params['max_volatility_reduce']:.1f}")

        # NOW test on 10-day TEST set
        print(f"\n{'='*80}")
        print(f"OVERFITTING CHECK - Testing on 10-day test set")
        print(f"{'='*80}")

        test_mrd, test_trades = self.evaluate_params(best_trial.params, self.test_dates)
        print(f"  Test MRD (10 days): {test_mrd:.3f}%")
        print(f"  Test Trades: {test_trades}")
        print()

        # Calculate overfitting percentage
        train_mrd = best_trial.value
        if train_mrd > 0:
            overfit_pct = (train_mrd - test_mrd) / train_mrd
        else:
            overfit_pct = 0.0

        print(f"  Train MRD: {train_mrd:.3f}%")
        print(f"  Test MRD:  {test_mrd:.3f}%")
        print(f"  Overfitting: {overfit_pct*100:.1f}%")
        print()

        overfitting_ok = overfit_pct <= OVERFITTING_THRESHOLD
        if overfitting_ok:
            print(f"  ✅ PASSED: Overfitting ≤ {OVERFITTING_THRESHOLD*100:.0f}% threshold")
        else:
            print(f"  ❌ FAILED: Overfitting > {OVERFITTING_THRESHOLD*100:.0f}% threshold (REJECT)")

        # Compare to baseline on TEST set
        print(f"\n{'='*80}")
        print(f"BASELINE COMPARISON (on TEST set)")
        print(f"{'='*80}")

        baseline_params = {
            "fractional_kelly": self.original_config["parameters"]["position_sizing"]["fractional_kelly"],
            "expected_win_pct": self.original_config["parameters"]["position_sizing"]["expected_win_pct"],
            "expected_loss_pct": self.original_config["parameters"]["position_sizing"]["expected_loss_pct"],
            "min_position_pct": self.original_config["parameters"]["position_sizing"]["min_position_pct"],
            "max_position_pct": self.original_config["parameters"]["position_sizing"]["max_position_pct"],
            "enable_volatility_adjustment": self.original_config["parameters"]["position_sizing"]["enable_volatility_adjustment"],
            "volatility_lookback": self.original_config["parameters"]["position_sizing"]["volatility_lookback"],
            "max_volatility_reduce": self.original_config["parameters"]["position_sizing"]["max_volatility_reduce"],
        }

        baseline_test_mrd, _ = self.evaluate_params(baseline_params, self.test_dates)
        print(f"  Baseline Test MRD: {baseline_test_mrd:.3f}%")
        print(f"  Optimized Test MRD: {test_mrd:.3f}%")
        print()

        improvement = test_mrd - baseline_test_mrd
        print(f"  IMPROVEMENT: {improvement:+.3f}%")
        print()

        # Final decision
        if overfitting_ok and improvement > 0:
            print("  ✅✅ ADOPT: Passes overfitting check AND beats baseline")
        elif overfitting_ok and improvement <= 0:
            print("  ⚠️  SKIP: Passes overfitting but doesn't beat baseline")
        else:
            print("  ❌❌ REJECT: Failed overfitting check")

        print("=" * 80)

        # Save results
        os.makedirs("results/position_sizing_optimization", exist_ok=True)

        output = {
            "timestamp": datetime.now().isoformat(),
            "train_dates": self.train_dates,
            "test_dates": self.test_dates,
            "n_trials": self.n_trials,
            "best_params": best_trial.params,
            "train_mrd": train_mrd,
            "test_mrd": test_mrd,
            "overfitting_pct": overfit_pct,
            "overfitting_ok": overfitting_ok,
            "baseline_params": baseline_params,
            "baseline_test_mrd": baseline_test_mrd,
            "improvement": improvement,
            "adopt": overfitting_ok and improvement > 0,
            "all_trials": [
                {
                    "trial": r['trial'],
                    "params": r['params'],
                    "train_mrd": r['train_mrd'],
                    "train_trades": r['train_trades']
                }
                for r in sorted(self.all_results, key=lambda x: x['train_mrd'], reverse=True)[:10]
            ]
        }

        with open(RESULTS_FILE, 'w') as f:
            json.dump(output, f, indent=2)

        print(f"\n📊 Results saved to: {RESULTS_FILE}")

        # Save best config ONLY if it passes overfitting check AND beats baseline
        if overfitting_ok and improvement > 0:
            best_config = self.original_config.copy()
            best_config["parameters"]["position_sizing"].update(best_trial.params)
            best_config["last_updated"] = datetime.now().strftime("%Y-%m-%d")

            # Include test date range in filename
            test_start = self.test_dates[0].replace("-", "")
            test_end = self.test_dates[-1].replace("-", "")[4:]  # Just MMDD for end
            config_filename = f"config/trading_params_pos_sizing_test_{test_start}-{test_end}.json"

            with open(config_filename, 'w') as f:
                json.dump(best_config, f, indent=2)

            print(f"✅ Best config saved to: {config_filename}")
        else:
            print(f"⚠️  Config NOT saved (failed overfitting check or doesn't beat baseline)")

        # Restore original config
        with open(BACKUP_FILE) as f:
            original_config = json.load(f)

        with open(CONFIG_FILE, 'w') as f:
            json.dump(original_config, f, indent=2)

        print(f"✅ Original config restored from backup")

        return study

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Optimize position sizing for MAX MRD with overfitting check")
    parser.add_argument("--trials", type=int, default=100,
                      help="Number of Optuna trials (default: 100)")

    args = parser.parse_args()

    # Create optimizer
    optimizer = PositionSizingOptimizer(
        train_dates=TRAIN_DATES,
        test_dates=TEST_DATES,
        n_trials=args.trials
    )

    # Run optimization
    study = optimizer.run_optimization()

    print(f"\n{'='*80}")
    print(f"🎉 OPTIMIZATION COMPLETE!")
    print(f"{'='*80}")
    print(f"Best TRAINING MRD: {study.best_value:.3f}%")
    print(f"Results saved to: {RESULTS_FILE}")
    print(f"{'='*80}\n")
