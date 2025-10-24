#!/usr/bin/env python3
"""
Sentio Lite - Sigor Strategy Optuna Optimization

Optimizes Sigor (Signal-OR ensemble) strategy parameters:
- 7 detector weights
- Fusion sharpness (k)
- Window sizes
- Signal thresholds

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
from typing import Dict, List

class SigorOptimizer:
    def __init__(self, test_dates: List[str], n_trials: int = 200):
        """
        Args:
            test_dates: List of 5 test dates (most recent trading days)
            n_trials: Number of Optuna trials (default: 200)
        """
        self.test_dates = test_dates
        self.n_trials = n_trials
        self.test_sigor_bin = "./build/test_sigor"
        self.test_symbol = "SPXL"  # Use SPXL as it has the most data

        # Track all results
        self.all_results = []

        print(f"=" * 80)
        print(f"Sigor Strategy Optuna Optimization")
        print(f"=" * 80)
        print(f"  Test dates: {', '.join(test_dates)}")
        print(f"  Trials: {n_trials}")
        print(f"  Strategy: Rule-based ensemble (7 detectors)")
        print(f"  Target: Maximize average MRD")
        print(f"=" * 80)

    def run_single_test(self, test_date: str, params: Dict) -> Dict:
        """
        Run Sigor strategy test for a single date with given parameters

        Returns:
            Dictionary with MRD, win_rate, total_trades, etc.
        """
        # Write parameters to temporary config file
        config_file = f"sigor_config_{test_date}.json"
        with open(config_file, 'w') as f:
            json.dump(params, f, indent=2)

        # Run test_sigor with data directory for this date
        # NOTE: test_sigor loads from data/{date}/ directory
        cmd = [
            self.test_sigor_bin,
            test_date,
            "--symbol", self.test_symbol
        ]

        try:
            # Set environment variable with config path
            env = os.environ.copy()
            env['SIGOR_CONFIG'] = config_file

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=60,  # 1 minute timeout
                env=env
            )

            # Clean up config file
            if os.path.exists(config_file):
                os.remove(config_file)

            if result.returncode != 0:
                print(f"    ❌ Test failed for {test_date}: {result.stderr[:100]}")
                return None

            # Parse output for performance metrics
            output = result.stdout

            # Extract metrics from output
            mrd = self._extract_metric(output, "MRD:")
            win_rate = self._extract_metric(output, "Win rate:")
            total_trades = self._extract_metric(output, "Total trades:", is_int=True)
            profit_factor = self._extract_metric(output, "Profit factor:")

            if mrd is None or total_trades == 0:
                return None

            return {
                'date': test_date,
                'mrd': mrd / 100.0,  # Convert from percentage
                'win_rate': win_rate / 100.0 if win_rate else 0.0,
                'total_trades': total_trades,
                'profit_factor': profit_factor if profit_factor else 0.0
            }

        except subprocess.TimeoutExpired:
            print(f"    ⏱️  Timeout for {test_date}")
            if os.path.exists(config_file):
                os.remove(config_file)
            return None
        except Exception as e:
            print(f"    ❌ Error for {test_date}: {e}")
            if os.path.exists(config_file):
                os.remove(config_file)
            return None

    def _extract_metric(self, output: str, key: str, is_int: bool = False):
        """Extract metric value from test output"""
        for line in output.split('\n'):
            if key in line:
                try:
                    # Extract number after key
                    parts = line.split(key)
                    if len(parts) > 1:
                        value_str = parts[1].strip().split()[0].replace('%', '').replace(',', '')
                        if is_int:
                            return int(value_str)
                        else:
                            return float(value_str)
                except:
                    continue
        return None

    def objective(self, trial: optuna.Trial) -> float:
        """
        Optuna objective function - returns average MRD across all test days
        """
        # Sample Sigor strategy parameters
        params = {
            # Fusion sharpness (controls signal amplification)
            'k': trial.suggest_float('k', 1.0, 3.0, step=0.1),

            # Detector weights (reliability of each detector)
            'w_boll': trial.suggest_float('w_boll', 0.5, 2.0, step=0.1),
            'w_rsi': trial.suggest_float('w_rsi', 0.5, 2.0, step=0.1),
            'w_mom': trial.suggest_float('w_mom', 0.5, 2.0, step=0.1),
            'w_vwap': trial.suggest_float('w_vwap', 0.5, 2.0, step=0.1),
            'w_orb': trial.suggest_float('w_orb', 0.1, 1.5, step=0.1),
            'w_ofi': trial.suggest_float('w_ofi', 0.1, 1.5, step=0.1),
            'w_vol': trial.suggest_float('w_vol', 0.1, 1.5, step=0.1),

            # Window sizes (lookback periods)
            'win_boll': trial.suggest_int('win_boll', 15, 30),
            'win_rsi': trial.suggest_int('win_rsi', 10, 20),
            'win_mom': trial.suggest_int('win_mom', 5, 15),
            'win_vwap': trial.suggest_int('win_vwap', 15, 30),
            'orb_opening_bars': trial.suggest_int('orb_opening_bars', 20, 45),
            'vol_window': trial.suggest_int('vol_window', 15, 30),

            # Warmup period
            'warmup_bars': 50  # Fixed
        }

        # Test on all dates
        mrds = []
        trial_results = []

        print(f"\n  Trial {trial.number + 1}/{self.n_trials}:")
        print(f"    Params: k={params['k']:.1f}, "
              f"w_boll={params['w_boll']:.1f}, "
              f"w_rsi={params['w_rsi']:.1f}, "
              f"win_boll={params['win_boll']}")

        for test_date in self.test_dates:
            result = self.run_single_test(test_date, params)

            if result is None:
                # Failed test - penalize
                mrds.append(-0.05)  # -5% penalty
            else:
                mrds.append(result['mrd'])
                trial_results.append(result)
                print(f"    {test_date}: MRD={result['mrd']:>7.2%}, "
                      f"trades={result['total_trades']:>3}, "
                      f"win%={result['win_rate']:>5.1%}")

        # Calculate average MRD
        avg_mrd = np.mean(mrds)

        # Store results
        self.all_results.append({
            'trial': trial.number,
            'params': params.copy(),
            'avg_mrd': avg_mrd,
            'individual_results': trial_results,
            'mrds': mrds
        })

        print(f"    Average MRD: {avg_mrd:.4%}")

        # Return avg MRD (Optuna maximizes)
        return avg_mrd

    def run_optimization(self):
        """Run Optuna optimization"""
        print(f"\n🔍 Starting Sigor optimization...\n")

        # Create study
        study = optuna.create_study(
            direction='maximize',
            study_name='sigor_optimization'
        )

        # Run optimization
        study.optimize(self.objective, n_trials=self.n_trials, show_progress_bar=True)

        print(f"\n✅ Optimization complete!")
        print(f"\n{'='*80}")
        print(f"BEST SIGOR PARAMETERS FOUND")
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
                      f"k={trial.params['k']:.1f}, "
                      f"w_boll={trial.params['w_boll']:.1f}")

        # Save detailed results
        self.save_detailed_results(study)

        return study

    def save_best_config(self, params: Dict, avg_mrd: float):
        """Save best Sigor configuration"""
        config_path = "config/sigor_params.json"

        config = {
            'description': 'Sigor Strategy Optimized Parameters',
            'last_updated': datetime.now().strftime("%Y-%m-%d"),
            'optimization_date': datetime.now().strftime("%Y-%m-%d"),
            'test_performance': {
                'avg_mrd': avg_mrd,
                'test_dates': self.test_dates
            },
            'parameters': params
        }

        # Create config directory if it doesn't exist
        os.makedirs('config', exist_ok=True)

        with open(config_path, 'w') as f:
            json.dump(config, f, indent=2)

        print(f"\n✅ Best Sigor configuration saved to {config_path}")

    def save_detailed_results(self, study: optuna.Study):
        """Save all trial results"""
        results_file = "optuna_sigor_results.json"

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
    """Get list of N most recent trading days"""
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
    parser = argparse.ArgumentParser(description='Sigor Strategy Optuna Optimization')
    parser.add_argument('--end-date', type=str, required=True,
                       help='End date (most recent test date) in YYYY-MM-DD format')
    parser.add_argument('--trials', type=int, default=200,
                       help='Number of trials (default: 200)')

    args = parser.parse_args()

    # Get 5 most recent trading days
    test_dates = get_recent_trading_days(args.end_date, n_days=5)

    print(f"\n{'='*80}")
    print(f"Sentio Lite - Sigor Strategy Optimization")
    print(f"{'='*80}")
    print(f"End date: {args.end_date}")
    print(f"Test dates: {test_dates}")
    print(f"Trials: {args.trials}")
    print(f"{'='*80}\n")

    # Create optimizer
    optimizer = SigorOptimizer(test_dates=test_dates, n_trials=args.trials)

    # Run optimization
    study = optimizer.run_optimization()

    print(f"\n{'='*80}")
    print(f"🎉 SIGOR OPTIMIZATION COMPLETE!")
    print(f"{'='*80}")
    print(f"Best average MRD: {study.best_value:.4%}")
    print(f"Configuration saved to: config/sigor_params.json")
    print(f"{'='*80}\n")


if __name__ == "__main__":
    main()
