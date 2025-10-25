#!/usr/bin/env python3
"""
Morning Market Preparation Routine

Standard workflow for SIGOR weight optimization and validation:
1. Optimize on previous 5 trading days (validation set)
2. Validate on 10 trading days prior to validation period (test set)
3. If MRD difference is reasonable (<20% degradation), deploy for today

Usage:
    ./scripts/morning_prep.py --end-date 2025-10-24 --trials 200
"""

import json
import subprocess
import optuna
from typing import Dict, List, Tuple
import sys
from datetime import datetime, timedelta
import numpy as np
import argparse

CONFIG_FILE = "config/sigor_params.json"
BACKUP_FILE = "config/sigor_params.json.bak"

class MorningPrep:
    def __init__(self, end_date: str, n_trials: int = 200, max_degradation: float = 0.20):
        """
        Args:
            end_date: Most recent trading day (YYYY-MM-DD)
            n_trials: Number of Optuna trials (default: 200)
            max_degradation: Maximum acceptable performance degradation (default: 20%)
        """
        self.end_date = end_date
        self.n_trials = n_trials
        self.max_degradation = max_degradation
        self.sentio_bin = "./build/sentio_lite"

        # Get trading days
        self.val_dates = self.get_trading_days(end_date, 5)  # Last 5 days
        self.test_dates = self.get_trading_days(self.val_dates[0], 10, before=True)  # 10 days before

        self.all_results = []

        print("=" * 80)
        print("MORNING MARKET PREPARATION - SIGOR OPTIMIZATION")
        print("=" * 80)
        print(f"  End date: {end_date}")
        print(f"  Optimization set (5 days): {self.val_dates[0]} to {self.val_dates[-1]}")
        print(f"  Validation set (10 days):  {self.test_dates[0]} to {self.test_dates[-1]}")
        print(f"  Trials: {n_trials}")
        print(f"  Max acceptable degradation: {max_degradation:.0%}")
        print("=" * 80)
        print()

        # Backup original config
        with open(CONFIG_FILE) as f:
            self.original_config = json.load(f)

        with open(BACKUP_FILE, 'w') as f:
            json.dump(self.original_config, f, indent=2)

    def get_trading_days(self, end_date: str, n_days: int, before: bool = False) -> List[str]:
        """
        Get N trading days ending at (or before) end_date

        Args:
            end_date: Reference date (YYYY-MM-DD)
            n_days: Number of trading days
            before: If True, get days BEFORE end_date (not including it)
        """
        end = datetime.strptime(end_date, "%Y-%m-%d")

        if before:
            # Start from day before end_date
            end -= timedelta(days=1)

        trading_days = []
        current = end

        while len(trading_days) < n_days:
            # Skip weekends (Saturday=5, Sunday=6)
            if current.weekday() < 5:
                trading_days.append(current.strftime("%Y-%m-%d"))
            current -= timedelta(days=1)

        return list(reversed(trading_days))

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

    def evaluate_weights(self, weights: Dict[str, float], dates: List[str]) -> Tuple[float, int]:
        """Evaluate a weight configuration on multiple dates, return avg MRD and total trades"""
        # Update config file
        with open(CONFIG_FILE) as f:
            config = json.load(f)

        for detector, weight in weights.items():
            config["parameters"][detector] = weight

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
        """Optuna objective function - returns average MRD on optimization set"""

        # Sample detector weights AND fusion sharpness k
        weights = {
            'k': trial.suggest_float('k', 1.0, 3.0, step=0.1),
            'w_boll': trial.suggest_float('w_boll', 0.1, 2.0, step=0.1),
            'w_rsi': trial.suggest_float('w_rsi', 0.1, 2.0, step=0.1),
            'w_mom': trial.suggest_float('w_mom', 0.1, 2.0, step=0.1),
            'w_vwap': trial.suggest_float('w_vwap', 0.1, 2.0, step=0.1),
            'w_orb': trial.suggest_float('w_orb', 0.1, 1.5, step=0.1),
            'w_ofi': trial.suggest_float('w_ofi', 0.1, 1.5, step=0.1),
            'w_vol': trial.suggest_float('w_vol', 0.1, 1.5, step=0.1),
        }

        # Evaluate on optimization set (5 days)
        opt_mrd, opt_trades = self.evaluate_weights(weights, self.val_dates)

        # Store results
        trial_result = {
            'trial': trial.number,
            'weights': weights.copy(),
            'opt_mrd': opt_mrd,
            'opt_trades': opt_trades
        }
        self.all_results.append(trial_result)

        return opt_mrd

    def run(self):
        """Run full morning preparation workflow"""

        print("\n📊 STEP 1: Baseline Performance")
        print("=" * 80)

        baseline_weights = {
            "k": self.original_config["parameters"]["k"],
            "w_boll": self.original_config["parameters"]["w_boll"],
            "w_rsi": self.original_config["parameters"]["w_rsi"],
            "w_mom": self.original_config["parameters"]["w_mom"],
            "w_vwap": self.original_config["parameters"]["w_vwap"],
            "w_orb": self.original_config["parameters"]["w_orb"],
            "w_ofi": self.original_config["parameters"]["w_ofi"],
            "w_vol": self.original_config["parameters"]["w_vol"],
        }

        baseline_opt_mrd, _ = self.evaluate_weights(baseline_weights, self.val_dates)
        baseline_test_mrd, _ = self.evaluate_weights(baseline_weights, self.test_dates)

        print(f"  Current weights: {baseline_weights}")
        print(f"  Optimization set MRD: {baseline_opt_mrd:.3f}%")
        print(f"  Validation set MRD:   {baseline_test_mrd:.3f}%")
        print()

        print("\n🔍 STEP 2: Optimize Weights on Recent 5 Days")
        print("=" * 80)

        # Create study
        study = optuna.create_study(
            direction='maximize',
            study_name='morning_prep_optimization',
            sampler=optuna.samplers.TPESampler(seed=42)
        )

        # Run optimization
        study.optimize(self.objective, n_trials=self.n_trials, show_progress_bar=True)

        best_trial = study.best_trial
        best_weights = best_trial.params
        best_opt_mrd = best_trial.value

        print(f"\n  Best weights found: {best_weights}")
        print(f"  Optimization set MRD: {best_opt_mrd:.3f}%")
        print()

        print("\n✅ STEP 3: Validate on Prior 10 Days")
        print("=" * 80)

        # Test best weights on validation set
        best_test_mrd, _ = self.evaluate_weights(best_weights, self.test_dates)

        print(f"  Validation set MRD: {best_test_mrd:.3f}%")
        print()

        # Calculate degradation
        if best_opt_mrd != 0:
            degradation = (best_opt_mrd - best_test_mrd) / abs(best_opt_mrd)
        else:
            degradation = 0.0

        print("\n📈 STEP 4: Deployment Decision")
        print("=" * 80)
        print(f"  Optimization set (recent 5 days): {best_opt_mrd:+.3f}%")
        print(f"  Validation set (prior 10 days):   {best_test_mrd:+.3f}%")
        print(f"  Performance degradation:          {degradation:+.1%}")
        print(f"  Acceptable threshold:             {self.max_degradation:.1%}")
        print()

        # Improvement vs baseline
        opt_improvement = best_opt_mrd - baseline_opt_mrd
        test_improvement = best_test_mrd - baseline_test_mrd

        print(f"  Improvement vs baseline (opt):    {opt_improvement:+.3f}%")
        print(f"  Improvement vs baseline (test):   {test_improvement:+.3f}%")
        print()

        # Decision logic
        deploy = False
        reason = ""

        if best_opt_mrd > baseline_opt_mrd and degradation <= self.max_degradation:
            deploy = True
            reason = "Optimized weights are better AND validated on prior data"
        elif best_opt_mrd > baseline_opt_mrd and degradation > self.max_degradation:
            deploy = False
            reason = f"Optimized weights show {degradation:.1%} degradation (>threshold)"
        else:
            deploy = False
            reason = "Optimized weights are NOT better than baseline"

        print("=" * 80)
        if deploy:
            print("  ✅ DEPLOY: Using optimized weights for today's market")
            print(f"  Reason: {reason}")

            # Save optimized config
            val_start = self.val_dates[0].replace("-", "")
            val_end = self.val_dates[-1].replace("-", "")[4:]
            config_filename = f"config/sigor_params_optimized_{val_start}-{val_end}.json"

            new_config = self.original_config.copy()
            new_config["parameters"].update(best_weights)
            new_config["last_updated"] = datetime.now().strftime("%Y-%m-%d")
            new_config["optimization_note"] = f"Morning prep: opt on {self.val_dates[0]} to {self.val_dates[-1]}, validated on {self.test_dates[0]} to {self.test_dates[-1]}"

            with open(config_filename, 'w') as f:
                json.dump(new_config, f, indent=2)

            print(f"  📁 Saved to: {config_filename}")

            # Update main config
            with open(CONFIG_FILE, 'w') as f:
                json.dump(new_config, f, indent=2)

            print(f"  📁 Updated: {CONFIG_FILE}")

        else:
            print("  ❌ DO NOT DEPLOY: Keep baseline weights")
            print(f"  Reason: {reason}")

            # Restore original config
            with open(BACKUP_FILE) as f:
                original_config = json.load(f)

            with open(CONFIG_FILE, 'w') as f:
                json.dump(original_config, f, indent=2)

            print(f"  📁 Restored: {CONFIG_FILE}")

        print("=" * 80)

        # Save detailed results
        import os
        os.makedirs("results/morning_prep", exist_ok=True)

        results_file = f"results/morning_prep/prep_{self.end_date}.json"
        output = {
            "timestamp": datetime.now().isoformat(),
            "end_date": self.end_date,
            "optimization_dates": self.val_dates,
            "validation_dates": self.test_dates,
            "n_trials": self.n_trials,
            "baseline": {
                "weights": baseline_weights,
                "opt_mrd": baseline_opt_mrd,
                "test_mrd": baseline_test_mrd
            },
            "optimized": {
                "weights": best_weights,
                "opt_mrd": best_opt_mrd,
                "test_mrd": best_test_mrd,
                "degradation": degradation
            },
            "decision": {
                "deploy": deploy,
                "reason": reason,
                "opt_improvement": opt_improvement,
                "test_improvement": test_improvement
            }
        }

        with open(results_file, 'w') as f:
            json.dump(output, f, indent=2)

        print(f"\n📊 Detailed results saved to: {results_file}")
        print()

        return deploy, best_weights, best_test_mrd

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Morning market preparation routine")
    parser.add_argument("--end-date", type=str, required=True,
                      help="Most recent trading day (YYYY-MM-DD)")
    parser.add_argument("--trials", type=int, default=200,
                      help="Number of Optuna trials (default: 200)")
    parser.add_argument("--max-degradation", type=float, default=0.20,
                      help="Maximum acceptable degradation (default: 0.20 = 20%%)")

    args = parser.parse_args()

    # Run morning prep
    prep = MorningPrep(
        end_date=args.end_date,
        n_trials=args.trials,
        max_degradation=args.max_degradation
    )

    deploy, weights, test_mrd = prep.run()

    print("\n" + "=" * 80)
    print("🎯 MORNING PREP COMPLETE")
    print("=" * 80)

    if deploy:
        print(f"✅ Ready for market open with optimized weights")
        print(f"   Expected MRD (based on 10-day validation): {test_mrd:+.3f}%")
    else:
        print(f"⚠️  Using baseline weights - optimization did not meet criteria")

    print("=" * 80)
    print()
