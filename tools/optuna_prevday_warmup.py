#!/usr/bin/env python3
"""
Sentio Lite - Previous Day Warmup Optuna Optimization

Strategy:
1. Use previous day 50-bar warmup (bars 342-391 of previous day)
2. Test on 5 most recent trading days
3. 200 trials to find optimal parameters
4. Optimize for average MRD across 5 days
5. Save best config to config/trading_params.json

Author: Claude Code
Date: 2025-10-24
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

class PrevDayWarmupOptimizer:
    def __init__(self, test_dates: List[str], n_trials: int = 200):
        """
        Args:
            test_dates: List of 5 test dates (most recent trading days)
            n_trials: Number of Optuna trials (default: 200)
        """
        self.test_dates = test_dates
        self.n_trials = n_trials
        self.sentio_bin = "./build/sentio_lite"
        self.warmup_bars = 50

        # Track all results for final selection
        self.all_results = []

        print(f"=" * 80)
        print(f"Previous Day Warmup Optuna Search")
        print(f"=" * 80)
        print(f"  Test dates: {', '.join(test_dates)}")
        print(f"  Warmup: {self.warmup_bars} bars (previous day, bars 342-391)")
        print(f"  Simulation: 0 days (warmup-only mode)")
        print(f"  Trials: {n_trials}")
        print(f"  Total runs per trial: {len(test_dates)} days")
        print(f"  Data per day: ~2 days (warmup from prev + test day)")
        print(f"=" * 80)

    def run_single_test(self, test_date: str, params: Dict) -> Dict:
        """
        Run sentio_lite for a single test date with given parameters

        Returns:
            Dictionary with MRD, win_rate, total_trades, etc.
        """
        # Build results filename
        results_file = f"results_optuna_{test_date}.json"

        cmd = [
            self.sentio_bin,
            "mock",
            "--date", test_date,
            
            "--warmup-bars", str(self.warmup_bars),
            "--no-dashboard",
            "--results-file", results_file
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

            # Read results
            with open(results_file, 'r') as f:
                results = json.load(f)

            perf = results['performance']

            # Clean up
            if os.path.exists(results_file):
                os.remove(results_file)

            return {
                'date': test_date,
                'mrd': perf['mrd'],
                'win_rate': perf['win_rate'],
                'total_trades': perf['total_trades'],
                'profit_factor': perf['profit_factor'],
                'final_equity': perf['final_equity'],
                'total_return': perf['total_return']
            }

        except subprocess.TimeoutExpired:
            print(f"    ⏱️  Timeout for {test_date}")
            return None
        except Exception as e:
            print(f"    ❌ Error for {test_date}: {e}")
            return None

    def objective(self, trial: optuna.Trial) -> float:
        """
        Optuna objective function - returns average MRD across all test days
        """
        # Sample parameters - max_positions limited to 2, 3, 4
        params = {
            'max_positions': trial.suggest_categorical('max_positions', [2, 3, 4]),
            'lambda_1bar': trial.suggest_float('lambda_1bar', 0.95, 0.995, step=0.005),
            'lambda_5bar': trial.suggest_float('lambda_5bar', 0.98, 0.998, step=0.002),
            'lambda_10bar': trial.suggest_float('lambda_10bar', 0.990, 0.999, step=0.001),
            'lambda_20bar': trial.suggest_float('lambda_20bar', 0.995, 0.9995, step=0.0005),
            'min_prediction_for_entry': trial.suggest_float('min_prediction_for_entry', 0.0001, 0.005, step=0.0001),
            'min_prediction_increase_on_trade': trial.suggest_float('min_prediction_increase_on_trade', 0.00001, 0.001, step=0.00001),
            'min_prediction_decrease_on_no_trade': trial.suggest_float('min_prediction_decrease_on_no_trade', 0.00001, 0.0005, step=0.00001),
            'win_multiplier': trial.suggest_float('win_multiplier', 1.1, 2.0, step=0.1),
            'loss_multiplier': trial.suggest_float('loss_multiplier', 0.5, 0.9, step=0.05),
            'rotation_strength_delta': trial.suggest_float('rotation_strength_delta', 0.001, 0.02, step=0.001),
            'min_rank_strength': trial.suggest_float('min_rank_strength', 0.0001, 0.005, step=0.0001),
            'min_bars_to_hold': trial.suggest_int('min_bars_to_hold', 2, 10),  # Prevent churning
            # Profit Target & Stop Loss (from online_trader v2.0 - CRITICAL for 5.41% MRD)
            'profit_target_pct': trial.suggest_float('profit_target_pct', 0.01, 0.05, step=0.005),  # 1%-5% profit target
            'stop_loss_pct': trial.suggest_float('stop_loss_pct', 0.005, 0.03, step=0.0025),  # 0.5%-3% stop loss
        }

        # First, update config file with these params (sentio reads from config file)
        self.update_config_file(params)

        # Test on all dates
        mrds = []
        trial_results = []

        print(f"\n  Trial {trial.number + 1}/{self.n_trials}:")
        print(f"    Params: max_pos={params['max_positions']}, "
              f"λ1={params['lambda_1bar']:.3f}, "
              f"min_pred={params['min_prediction_for_entry']:.4f}")

        for test_date in self.test_dates:
            result = self.run_single_test(test_date, params)

            if result is None:
                # Failed test - penalize heavily
                mrds.append(-0.10)  # -10% penalty
            else:
                mrds.append(result['mrd'])
                trial_results.append(result)
                print(f"    {test_date}: MRD={result['mrd']:>7.2%}, "
                      f"trades={result['total_trades']:>3}, "
                      f"win%={result['win_rate']:>5.1%}")

        # Calculate average MRD
        avg_mrd = np.mean(mrds)

        # Store results for this trial
        self.all_results.append({
            'trial': trial.number,
            'params': params.copy(),
            'avg_mrd': avg_mrd,
            'individual_results': trial_results,
            'mrds': mrds
        })

        print(f"    Average MRD: {avg_mrd:.4%}")

        # Optuna maximizes, so return avg MRD (higher is better)
        return avg_mrd

    def update_config_file(self, params: Dict):
        """Update config/trading_params.json with trial parameters"""
        config_path = "config/trading_params.json"

        # Read existing config
        with open(config_path, 'r') as f:
            config = json.load(f)

        # Update parameters
        config['parameters']['max_positions'] = params['max_positions']
        config['parameters']['lambda_1bar'] = params['lambda_1bar']
        config['parameters']['lambda_5bar'] = params['lambda_5bar']
        config['parameters']['lambda_10bar'] = params['lambda_10bar']
        config['parameters']['lambda_20bar'] = params['lambda_20bar']
        config['parameters']['min_prediction_for_entry'] = params['min_prediction_for_entry']
        config['parameters']['min_prediction_increase_on_trade'] = params['min_prediction_increase_on_trade']
        config['parameters']['min_prediction_decrease_on_no_trade'] = params['min_prediction_decrease_on_no_trade']
        config['parameters']['win_multiplier'] = params['win_multiplier']
        config['parameters']['loss_multiplier'] = params['loss_multiplier']
        config['parameters']['rotation_strength_delta'] = params['rotation_strength_delta']
        config['parameters']['min_rank_strength'] = params['min_rank_strength']
        config['parameters']['min_bars_to_hold'] = params['min_bars_to_hold']
        config['parameters']['profit_target_pct'] = params['profit_target_pct']
        config['parameters']['stop_loss_pct'] = params['stop_loss_pct']

        # Write back
        with open(config_path, 'w') as f:
            json.dump(config, f, indent=2)

    def run_optimization(self):
        """Run Optuna optimization"""
        print(f"\n🔍 Starting optimization...")

        # Create study
        study = optuna.create_study(
            direction='maximize',  # Maximize average MRD
            study_name='prevday_warmup_optimization'
        )

        # Run optimization
        study.optimize(self.objective, n_trials=self.n_trials, show_progress_bar=True)

        print(f"\n✅ Optimization complete!")
        print(f"\n{'='*80}")
        print(f"BEST PARAMETERS FOUND")
        print(f"{'='*80}")

        best_trial = study.best_trial
        print(f"  Trial number: {best_trial.number}")
        print(f"  Average MRD: {best_trial.value:.4%}")
        print(f"\n  Parameters:")
        for key, value in best_trial.params.items():
            print(f"    {key}: {value}")

        # Save best config
        self.save_best_config(best_trial.params, best_trial.value)

        # Show top 10 trials
        print(f"\n{'='*80}")
        print(f"TOP 10 TRIALS")
        print(f"{'='*80}")

        sorted_trials = sorted(study.trials, key=lambda t: t.value if t.value is not None else -1, reverse=True)
        for i, trial in enumerate(sorted_trials[:10], 1):
            if trial.value is not None:
                print(f"#{i:>2}: Trial {trial.number:>3} - MRD={trial.value:>7.4%} - "
                      f"max_pos={trial.params['max_positions']}, "
                      f"λ1={trial.params['lambda_1bar']:.3f}")

        # Save detailed results
        self.save_detailed_results(study)

        return study

    def save_best_config(self, params: Dict, avg_mrd: float):
        """Save best configuration to config file"""
        config_path = "config/trading_params.json"

        with open(config_path, 'r') as f:
            config = json.load(f)

        # Update metadata
        config['description'] = "Previous Day Warmup Optimized Parameters (50 bars)"
        config['last_updated'] = datetime.now().strftime("%Y-%m-%d")
        config['optimization_date'] = datetime.now().strftime("%Y-%m-%d")
        config['test_performance'] = {
            'avg_mrd': avg_mrd,
            'test_dates': self.test_dates,
            'warmup_mode': 'prevday_50_bars'
        }

        # Update parameters
        for key, value in params.items():
            config['parameters'][key] = value

        # Write back
        with open(config_path, 'w') as f:
            json.dump(config, f, indent=2)

        print(f"\n✅ Best configuration saved to {config_path}")

    def save_detailed_results(self, study: optuna.Study):
        """Save all trial results to a file"""
        results_file = "optuna_prevday_results.json"

        trials_data = []
        for trial in study.trials:
            if trial.value is not None:
                trials_data.append({
                    'number': trial.number,
                    'value': trial.value,
                    'params': trial.params,
                    'state': trial.state.name
                })

        output = {
            'optimization_date': datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            'test_dates': self.test_dates,
            'n_trials': self.n_trials,
            'warmup_bars': self.warmup_bars,
            'warmup_mode': 'prevday',
            'best_trial': {
                'number': study.best_trial.number,
                'avg_mrd': study.best_trial.value,
                'params': study.best_trial.params
            },
            'all_trials': trials_data
        }

        with open(results_file, 'w') as f:
            json.dump(output, f, indent=2)

        print(f"✅ Detailed results saved to {results_file}")


def get_recent_trading_days(end_date_str: str, n_days: int = 5) -> List[str]:
    """
    Get list of N most recent trading days before (and including) end_date

    For simplicity, assumes weekdays are trading days (excludes Sat/Sun)
    In production, you'd check actual market calendar
    """
    end_date = datetime.strptime(end_date_str, "%Y-%m-%d")
    trading_days = []

    current = end_date
    while len(trading_days) < n_days:
        # Skip weekends
        if current.weekday() < 5:  # Monday=0, Sunday=6
            trading_days.append(current.strftime("%Y-%m-%d"))
        current -= timedelta(days=1)

    return list(reversed(trading_days))


def main():
    parser = argparse.ArgumentParser(description='Previous Day Warmup Optuna Optimization')
    parser.add_argument('--end-date', type=str, required=True,
                       help='End date (most recent test date) in YYYY-MM-DD format')
    parser.add_argument('--trials', type=int, default=200,
                       help='Number of trials (default: 200)')

    args = parser.parse_args()

    # Get 5 most recent trading days
    test_dates = get_recent_trading_days(args.end_date, n_days=5)

    print(f"\n{'='*80}")
    print(f"Sentio Lite - Previous Day Warmup Optimization")
    print(f"{'='*80}")
    print(f"End date: {args.end_date}")
    print(f"Test dates: {test_dates}")
    print(f"Trials: {args.trials}")
    print(f"{'='*80}\n")

    # Create optimizer
    optimizer = PrevDayWarmupOptimizer(test_dates=test_dates, n_trials=args.trials)

    # Run optimization
    study = optimizer.run_optimization()

    print(f"\n{'='*80}")
    print(f"🎉 OPTIMIZATION COMPLETE!")
    print(f"{'='*80}")
    print(f"Best average MRD: {study.best_value:.4%}")
    print(f"Configuration saved to: config/trading_params.json")
    print(f"{'='*80}\n")


if __name__ == "__main__":
    main()
