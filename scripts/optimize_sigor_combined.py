#!/usr/bin/env python3
"""
SIGOR Combined Optimization with Optuna
Optimizes BOTH detector weights AND window sizes while enforcing generalization.

Protocol:
- Evaluation set: 5 most recent trading days up to --end-date (inclusive)
- Validation set: 10 trading days prior to the evaluation period
- Constraint: validation_avg_mrd must be within (1 - overfitting_threshold) of evaluation_avg_mrd

Objective: Maximize evaluation_avg_mrd subject to the validation constraint.
"""

import json
import subprocess
import optuna
from typing import Dict, List, Tuple
import sys
from datetime import datetime, timedelta
import numpy as np

CONFIG_FILE = "config/sigor_params.json"
BACKUP_FILE = "config/sigor_params.json.bak"
RESULTS_FILE = "results/combined_optimization/optuna_results.json"

class SigorCombinedOptimizer:
    def __init__(self, eval_dates: List[str], val_dates: List[str], n_trials: int = 200, overfitting_threshold: float = 0.20):
        self.eval_dates = eval_dates
        self.val_dates = val_dates
        self.n_trials = n_trials
        self.overfitting_threshold = overfitting_threshold
        self.sentio_bin = "./build/sentio_lite"

        # Track all results
        self.all_results = []

        print("=" * 80)
        print("SIGOR COMBINED OPTIMIZATION - Weights + Windows - Optuna TPE Sampler")
        print("=" * 80)
        print(f"  Evaluation dates (5): {', '.join(eval_dates)}")
        print(f"  Validation dates (10): {', '.join(val_dates)}")
        print(f"  Trials: {n_trials}")
        print(f"  Strategy: Optimize 8 weights + 9 windows (17 parameters total, including AWR)")
        print(f"  Target: Maximize evaluation MRD with validation within {int(self.overfitting_threshold*100)}%")
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
        """Optuna objective: maximize evaluation MRD subject to validation constraint."""

        # Sample BOTH weights AND windows (8 detectors including AWR)
        params = {
            # Detector weights
            'w_boll': trial.suggest_float('w_boll', 0.1, 2.0, step=0.1),
            'w_rsi': trial.suggest_float('w_rsi', 0.1, 2.0, step=0.1),
            'w_mom': trial.suggest_float('w_mom', 0.1, 2.0, step=0.1),
            'w_vwap': trial.suggest_float('w_vwap', 0.1, 2.0, step=0.1),
            'w_orb': trial.suggest_float('w_orb', 0.1, 1.5, step=0.1),
            'w_ofi': trial.suggest_float('w_ofi', 0.1, 1.5, step=0.1),
            'w_vol': trial.suggest_float('w_vol', 0.1, 1.5, step=0.1),
            'w_awr': trial.suggest_float('w_awr', 0.1, 2.0, step=0.1),

            # Window sizes
            'win_boll': trial.suggest_int('win_boll', 10, 50),
            'win_rsi': trial.suggest_int('win_rsi', 5, 30),
            'win_mom': trial.suggest_int('win_mom', 5, 30),
            'win_vwap': trial.suggest_int('win_vwap', 10, 50),
            'orb_opening_bars': trial.suggest_int('orb_opening_bars', 10, 60),
            'vol_window': trial.suggest_int('vol_window', 10, 50),
            'win_awr_williams': trial.suggest_int('win_awr_williams', 5, 30),
            'win_awr_rsi': trial.suggest_int('win_awr_rsi', 5, 30),
            'win_awr_bb': trial.suggest_int('win_awr_bb', 10, 50),
        }

        print(f"\n  Trial {trial.number + 1}/{self.n_trials}:")
        print(f"    Weights: boll={params['w_boll']:.1f}, rsi={params['w_rsi']:.1f}, "
              f"mom={params['w_mom']:.1f}, vwap={params['w_vwap']:.1f}")
        print(f"    Windows: boll={params['win_boll']}, rsi={params['win_rsi']}, "
              f"mom={params['win_mom']}, vwap={params['win_vwap']}")

        # Evaluate on evaluation and validation sets
        eval_mrd, eval_trades = self.evaluate_params(params, self.eval_dates)
        val_mrd, val_trades = self.evaluate_params(params, self.val_dates)

        print(f"    Eval MRD: {eval_mrd:.3f}%, Trades: {eval_trades}")
        print(f"    Val  MRD: {val_mrd:.3f}%, Trades: {val_trades}")

        # Validation must be within allowed degradation of evaluation
        if eval_mrd > 0:
            passes = val_mrd >= (1.0 - self.overfitting_threshold) * eval_mrd
        else:
            # If eval <= 0, require validation not worse than evaluation
            passes = val_mrd >= eval_mrd

        # Store results
        trial_result = {
            'trial': trial.number,
            'params': params.copy(),
            'eval_mrd': eval_mrd,
            'eval_trades': eval_trades,
            'val_mrd': val_mrd,
            'val_trades': val_trades,
            'passes_validation': passes
        }
        self.all_results.append(trial_result)

        if not passes:
            return -999.0
        return eval_mrd

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
        # Retrieve best eval/val MRD for reporting
        matched = next((r for r in self.all_results if r['trial'] == best_trial.number), None)
        if matched:
            best_eval_mrd = matched['eval_mrd']
            best_val_mrd = matched['val_mrd']
        else:
            best_eval_mrd = best_trial.value
            best_val_mrd, _ = self.evaluate_params(best_trial.params, self.val_dates)
        print(f"  Evaluation MRD: {best_eval_mrd:.3f}%")
        print(f"  Validation MRD: {best_val_mrd:.3f}%")

        print(f"\n  Detector Weights:")
        for key in ['w_boll', 'w_rsi', 'w_mom', 'w_vwap', 'w_orb', 'w_ofi', 'w_vol', 'w_awr']:
            if key in best_trial.params:
                print(f"    {key}: {best_trial.params[key]:.1f}")

        print(f"\n  Window Sizes:")
        for key in ['win_boll', 'win_rsi', 'win_mom', 'win_vwap', 'orb_opening_bars', 'vol_window',
                    'win_awr_williams', 'win_awr_rsi', 'win_awr_bb']:
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
            "w_awr": self.original_config["parameters"]["w_awr"],
            "win_boll": self.original_config["parameters"]["win_boll"],
            "win_rsi": self.original_config["parameters"]["win_rsi"],
            "win_mom": self.original_config["parameters"]["win_mom"],
            "win_vwap": self.original_config["parameters"]["win_vwap"],
            "orb_opening_bars": self.original_config["parameters"]["orb_opening_bars"],
            "vol_window": self.original_config["parameters"]["vol_window"],
            "win_awr_williams": self.original_config["parameters"]["win_awr_williams"],
            "win_awr_rsi": self.original_config["parameters"]["win_awr_rsi"],
            "win_awr_bb": self.original_config["parameters"]["win_awr_bb"],
        }

        print(f"  Baseline params: {baseline_params}")
        baseline_eval_mrd, _ = self.evaluate_params(baseline_params, self.eval_dates)
        baseline_val_mrd, _ = self.evaluate_params(baseline_params, self.val_dates)
        print(f"  Baseline Eval MRD: {baseline_eval_mrd:.3f}%")
        print(f"  Baseline Val  MRD: {baseline_val_mrd:.3f}%")
        print()

        improvement = best_eval_mrd - baseline_eval_mrd
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
            "evaluation_dates": self.eval_dates,
            "validation_dates": self.val_dates,
            "n_trials": self.n_trials,
            "best_params": best_trial.params,
            "best_eval_mrd": best_eval_mrd,
            "best_val_mrd": best_val_mrd,
            "baseline_params": baseline_params,
            "baseline_eval_mrd": baseline_eval_mrd,
            "baseline_val_mrd": baseline_val_mrd,
            "improvement": improvement,
            "all_trials": [
                {
                    "trial": r['trial'],
                    "params": r['params'],
                    "eval_mrd": r['eval_mrd'],
                    "eval_trades": r['eval_trades'],
                    "val_mrd": r['val_mrd'],
                    "val_trades": r['val_trades'],
                    "passes_validation": r['passes_validation']
                }
                for r in sorted(self.all_results, key=lambda x: (x['passes_validation'], x['eval_mrd']), reverse=True)[:10]
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

    def get_trading_days(end_date: str, n_days: int) -> List[str]:
        end = datetime.strptime(end_date, "%Y-%m-%d")
        days: List[str] = []
        cur = end
        while len(days) < n_days:
            if cur.weekday() < 5:
                days.append(cur.strftime("%Y-%m-%d"))
            cur -= timedelta(days=1)
        return list(reversed(days))

    parser = argparse.ArgumentParser(description="Optimize SIGOR weights + windows with Optuna (eval/validation constrained)")
    parser.add_argument("--end-date", required=True, help="End date MM-DD (inclusive, year fixed to 2025)")
    parser.add_argument("--trials", type=int, default=200, help="Number of Optuna trials (default: 200)")
    parser.add_argument("--overfitting-threshold", type=float, default=0.20,
                      help="Max allowed degradation from evaluation to validation (default: 0.20)")

    args = parser.parse_args()

    # Enforce 2025 and MM-DD input format
    try:
        full_end_date = f"2025-{args.end_date}"
        datetime.strptime(full_end_date, "%Y-%m-%d")
    except ValueError:
        print("ERROR: --end-date must be in MM-DD format (e.g., 10-24), year is fixed to 2025")
        sys.exit(1)

    all_days = get_trading_days(full_end_date, 15)
    val_dates = all_days[:10]
    eval_dates = all_days[10:]

    optimizer = SigorCombinedOptimizer(
        eval_dates=eval_dates,
        val_dates=val_dates,
        n_trials=args.trials,
        overfitting_threshold=args.overfitting_threshold
    )

    study = optimizer.run_optimization()
