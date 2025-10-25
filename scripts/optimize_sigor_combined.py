#!/usr/bin/env python3
"""
SIGOR Combined Optimization with Optuna
Optimizes BOTH detector weights AND window sizes simultaneously to maximize MRD

Uses Optuna's TPE sampler for efficient hyperparameter search
Total parameters: 13 (7 weights + 6 windows)
"""

import json
import subprocess
import optuna
from typing import Dict, List, Tuple
import sys
from datetime import datetime
import numpy as np

# Optimization configuration
TRAIN_DATES = ["2025-10-14", "2025-10-15", "2025-10-16", "2025-10-17", "2025-10-18"]
VAL_DATES = ["2025-10-20", "2025-10-21", "2025-10-22", "2025-10-23", "2025-10-24"]

CONFIG_FILE = "config/sigor_params.json"
BACKUP_FILE = "config/sigor_params.json.bak"
RESULTS_FILE = "results/combined_optimization/optuna_results.json"

class SigorCombinedOptimizer:
    def __init__(self, train_dates: List[str], val_dates: List[str], n_trials: int = 200):
        self.train_dates = train_dates
        self.val_dates = val_dates
        self.n_trials = n_trials
        self.sentio_bin = "./build/sentio_lite"

        # Track all results
        self.all_results = []

        print("=" * 80)
        print("SIGOR COMBINED OPTIMIZATION - Weights + Windows - Optuna TPE Sampler")
        print("=" * 80)
        print(f"  Training dates: {', '.join(train_dates)}")
        print(f"  Validation dates: {', '.join(val_dates)}")
        print(f"  Trials: {n_trials}")
        print(f"  Strategy: Optimize 7 weights + 6 windows (13 parameters total)")
        print(f"  Target: Maximize average MRD on validation set")
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
        """Evaluate a parameter configuration on multiple dates, return avg MRD and total trades"""
        # Update config file
        with open(CONFIG_FILE) as f:
            config = json.load(f)

        for param, value in params.items():
            config["parameters"][param] = value

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
        """Optuna objective function - returns average MRD on validation set"""

        # Sample BOTH weights AND windows
        params = {
            # Detector weights
            'w_boll': trial.suggest_float('w_boll', 0.1, 2.0, step=0.1),
            'w_rsi': trial.suggest_float('w_rsi', 0.1, 2.0, step=0.1),
            'w_mom': trial.suggest_float('w_mom', 0.1, 2.0, step=0.1),
            'w_vwap': trial.suggest_float('w_vwap', 0.1, 2.0, step=0.1),
            'w_orb': trial.suggest_float('w_orb', 0.1, 1.5, step=0.1),
            'w_ofi': trial.suggest_float('w_ofi', 0.1, 1.5, step=0.1),
            'w_vol': trial.suggest_float('w_vol', 0.1, 1.5, step=0.1),

            # Window sizes
            'win_boll': trial.suggest_int('win_boll', 10, 50),
            'win_rsi': trial.suggest_int('win_rsi', 5, 30),
            'win_mom': trial.suggest_int('win_mom', 5, 30),
            'win_vwap': trial.suggest_int('win_vwap', 10, 50),
            'orb_opening_bars': trial.suggest_int('orb_opening_bars', 10, 60),
            'vol_window': trial.suggest_int('vol_window', 10, 50),
        }

        print(f"\n  Trial {trial.number + 1}/{self.n_trials}:")
        print(f"    Weights: boll={params['w_boll']:.1f}, rsi={params['w_rsi']:.1f}, "
              f"mom={params['w_mom']:.1f}, vwap={params['w_vwap']:.1f}")
        print(f"    Windows: boll={params['win_boll']}, rsi={params['win_rsi']}, "
              f"mom={params['win_mom']}, vwap={params['win_vwap']}")

        # Evaluate on validation set
        val_mrd, val_trades = self.evaluate_params(params, self.val_dates)

        print(f"    Val MRD: {val_mrd:.3f}%, Trades: {val_trades}")

        # Store results
        trial_result = {
            'trial': trial.number,
            'params': params.copy(),
            'val_mrd': val_mrd,
            'val_trades': val_trades
        }
        self.all_results.append(trial_result)

        # Return validation MRD (Optuna will maximize)
        return val_mrd

    def run_optimization(self):
        """Run Optuna optimization"""
        print("\n🔍 Starting combined optimization (weights + windows)...\n")

        # Create study
        study = optuna.create_study(
            direction='maximize',
            study_name='sigor_combined_optimization',
            sampler=optuna.samplers.TPESampler(seed=42)
        )

        # Run optimization
        study.optimize(self.objective, n_trials=self.n_trials, show_progress_bar=True)

        print(f"\n✅ Optimization complete!")
        print(f"\n{'='*80}")
        print(f"BEST SIGOR PARAMETERS FOUND (WEIGHTS + WINDOWS)")
        print(f"{'='*80}")

        best_trial = study.best_trial
        print(f"  Trial number: {best_trial.number}")
        print(f"  Validation MRD: {best_trial.value:.3f}%")

        print(f"\n  Detector Weights:")
        for key in ['w_boll', 'w_rsi', 'w_mom', 'w_vwap', 'w_orb', 'w_ofi', 'w_vol']:
            if key in best_trial.params:
                print(f"    {key}: {best_trial.params[key]:.1f}")

        print(f"\n  Window Sizes:")
        for key in ['win_boll', 'win_rsi', 'win_mom', 'win_vwap', 'orb_opening_bars', 'vol_window']:
            if key in best_trial.params:
                print(f"    {key}: {best_trial.params[key]}")

        # Compare to baseline
        print(f"\n{'='*80}")
        print(f"BASELINE COMPARISON")
        print(f"{'='*80}")

        baseline_params = {
            "w_boll": self.original_config["parameters"]["w_boll"],
            "w_rsi": self.original_config["parameters"]["w_rsi"],
            "w_mom": self.original_config["parameters"]["w_mom"],
            "w_vwap": self.original_config["parameters"]["w_vwap"],
            "w_orb": self.original_config["parameters"]["w_orb"],
            "w_ofi": self.original_config["parameters"]["w_ofi"],
            "w_vol": self.original_config["parameters"]["w_vol"],
            "win_boll": self.original_config["parameters"]["win_boll"],
            "win_rsi": self.original_config["parameters"]["win_rsi"],
            "win_mom": self.original_config["parameters"]["win_mom"],
            "win_vwap": self.original_config["parameters"]["win_vwap"],
            "orb_opening_bars": self.original_config["parameters"]["orb_opening_bars"],
            "vol_window": self.original_config["parameters"]["vol_window"],
        }

        print(f"  Baseline params: {baseline_params}")
        baseline_val_mrd, _ = self.evaluate_params(baseline_params, self.val_dates)
        print(f"  Baseline Val MRD: {baseline_val_mrd:.3f}%")
        print()

        improvement = best_trial.value - baseline_val_mrd
        print(f"  IMPROVEMENT: {improvement:+.3f}%")
        print()

        if improvement > 0:
            print("  ✅ Optimized parameters are BETTER than baseline")
        else:
            print("  ❌ Baseline parameters are still BETTER")

        print("=" * 80)

        # Save results
        import os
        os.makedirs("results/combined_optimization", exist_ok=True)

        output = {
            "timestamp": datetime.now().isoformat(),
            "train_dates": self.train_dates,
            "val_dates": self.val_dates,
            "n_trials": self.n_trials,
            "best_params": best_trial.params,
            "best_val_mrd": best_trial.value,
            "baseline_params": baseline_params,
            "baseline_val_mrd": baseline_val_mrd,
            "improvement": improvement,
            "all_trials": [
                {
                    "trial": r['trial'],
                    "params": r['params'],
                    "val_mrd": r['val_mrd'],
                    "val_trades": r['val_trades']
                }
                for r in sorted(self.all_results, key=lambda x: x['val_mrd'], reverse=True)[:10]
            ]
        }

        with open(RESULTS_FILE, 'w') as f:
            json.dump(output, f, indent=2)

        print(f"\n📊 Results saved to: {RESULTS_FILE}")

        # Save best config if it's better
        if improvement > 0:
            best_config = self.original_config.copy()
            best_config["parameters"].update(best_trial.params)
            best_config["last_updated"] = datetime.now().strftime("%Y-%m-%d")
            best_config["optimization_note"] = f"Combined (weights+windows) optimized on {', '.join(self.val_dates)}"

            # Include validation date range in filename
            val_start = self.val_dates[0].replace("-", "")
            val_end = self.val_dates[-1].replace("-", "")[4:]  # Just MMDD for end
            config_filename = f"config/sigor_params_combined_{val_start}-{val_end}.json"

            with open(config_filename, 'w') as f:
                json.dump(best_config, f, indent=2)

            print(f"✅ Best config saved to: {config_filename}")

        # Restore original config
        with open(BACKUP_FILE) as f:
            original_config = json.load(f)

        with open(CONFIG_FILE, 'w') as f:
            json.dump(original_config, f, indent=2)

        print(f"✅ Original config restored from backup")

        return study

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Optimize SIGOR weights + windows with Optuna")
    parser.add_argument("--trials", type=int, default=200,
                      help="Number of Optuna trials (default: 200)")

    args = parser.parse_args()

    # Create optimizer
    optimizer = SigorCombinedOptimizer(
        train_dates=TRAIN_DATES,
        val_dates=VAL_DATES,
        n_trials=args.trials
    )

    # Run optimization
    study = optimizer.run_optimization()

    print(f"\n{'='*80}")
    print(f"🎉 OPTIMIZATION COMPLETE!")
    print(f"{'='*80}")
    print(f"Best validation MRD: {study.best_value:.3f}%")
    print(f"Results saved to: {RESULTS_FILE}")
    print(f"{'='*80}\n")
