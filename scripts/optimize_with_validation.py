#!/usr/bin/env python3
"""
Optuna Optimization with Overfitting Validation

Optimizes SIGOR parameters (weights + window sizes) while ensuring the config
passes overfitting validation (max 20% degradation from evaluation to validation).

Strategy:
- Evaluation Set: 5 most recent trading days (optimize on this)
- Validation Set: 10 prior trading days (validate generalization)
- Only accept configs where validation MRD doesn't degrade >20% from evaluation MRD

Usage:
    python3 scripts/optimize_with_validation.py --end-date 10-24 --trials 200
"""

import optuna
import subprocess
import json
import argparse
from datetime import datetime
from pathlib import Path
import sys


def run_evaluation(end_date: str, config_path: str) -> dict:
    """
    Run evaluate_config.py and return results.

    Returns dict with:
        - eval_mrd: float
        - val_mrd: float
        - degradation_pct: float
        - is_overfit: bool
        - verdict: str ("ACCEPT" or "REJECT")
    """
    output_file = "/tmp/optuna_eval_temp.json"

    cmd = [
        "python3", "scripts/evaluate_config.py",
        "--end-date", end_date,
        "--config", config_path,
        "--output", output_file
    ]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)

        # Load results
        with open(output_file, 'r') as f:
            data = json.load(f)

        return {
            "eval_mrd": data["evaluation_set"]["avg_mrd"],
            "val_mrd": data["validation_set"]["avg_mrd"],
            "eval_trades": data["evaluation_set"]["avg_trades"],
            "val_trades": data["validation_set"]["avg_trades"],
            "degradation_pct": data["overfitting"]["degradation_pct"],
            "is_overfit": data["overfitting"]["is_overfit"],
            "verdict": data["overfitting"]["verdict"]
        }
    except Exception as e:
        print(f"ERROR in evaluation: {e}")
        return None


def update_config(params: dict, config_path: str):
    """Update config file with new parameters."""
    with open(config_path, 'r') as f:
        config = json.load(f)

    config["parameters"].update(params)
    config["last_updated"] = datetime.now().strftime("%Y-%m-%d")

    with open(config_path, 'w') as f:
        json.dump(config, f, indent=2)


def objective(trial, end_date: str, config_path: str):
    """
    Optuna objective function.

    Returns:
        - eval_mrd if validation PASSES
        - -999 if validation FAILS (overfitting detected)
    """
    # Sample parameters
    params = {
        # Detector weights (0.1 to 2.0)
        "w_boll": trial.suggest_float("w_boll", 0.1, 2.0),
        "w_rsi": trial.suggest_float("w_rsi", 0.1, 2.0),
        "w_mom": trial.suggest_float("w_mom", 0.1, 2.0),
        "w_vwap": trial.suggest_float("w_vwap", 0.1, 2.0),
        "w_orb": trial.suggest_float("w_orb", 0.1, 2.0),
        "w_ofi": trial.suggest_float("w_ofi", 0.1, 2.0),
        "w_vol": trial.suggest_float("w_vol", 0.1, 2.0),

        # Window sizes
        "win_boll": trial.suggest_int("win_boll", 10, 50),
        "win_rsi": trial.suggest_int("win_rsi", 5, 30),
        "win_mom": trial.suggest_int("win_mom", 3, 20),
        "win_vwap": trial.suggest_int("win_vwap", 10, 50),
        "orb_opening_bars": trial.suggest_int("orb_opening_bars", 10, 50),
        "vol_window": trial.suggest_int("vol_window", 10, 50),
    }

    # Update config
    update_config(params, config_path)

    # Run evaluation
    results = run_evaluation(end_date, config_path)

    if results is None:
        print(f"Trial {trial.number}: FAILED (evaluation error)")
        return -999

    # Check if validation passes
    if results["verdict"] == "REJECT":
        print(f"Trial {trial.number}: REJECT (overfit {results['degradation_pct']*100:.1f}%) "
              f"- Eval: {results['eval_mrd']:.3f}%, Val: {results['val_mrd']:.3f}%")
        return -999  # Penalize overfitting
    else:
        print(f"Trial {trial.number}: ACCEPT (degrade {results['degradation_pct']*100:.1f}%) "
              f"- Eval: {results['eval_mrd']:.3f}%, Val: {results['val_mrd']:.3f}%")
        return results["eval_mrd"]  # Maximize evaluation MRD


def main():
    parser = argparse.ArgumentParser(
        description="Optuna optimization with overfitting validation"
    )
    parser.add_argument("--end-date", required=True,
                       help="End date for evaluation (MM-DD)")
    parser.add_argument("--trials", type=int, default=200,
                       help="Number of trials (default: 200)")
    parser.add_argument("--config", default="config/sigor_params.json",
                       help="Config file to optimize (default: config/sigor_params.json)")
    parser.add_argument("--output", default="results/validated_optimization.json",
                       help="Output file for results (default: results/validated_optimization.json)")

    args = parser.parse_args()

    print("="*80)
    print("OPTUNA OPTIMIZATION WITH VALIDATION")
    print("="*80)
    print(f"  End Date:         {args.end_date}")
    print(f"  Trials:           {args.trials}")
    print(f"  Config File:      {args.config}")
    print(f"  Output File:      {args.output}")
    print(f"  Strategy:         Maximize Eval MRD + Pass Validation (<20% degradation)")
    print("="*80)
    print()

    # Save original config
    config_backup = args.config + ".backup"
    subprocess.run(["cp", args.config, config_backup])
    print(f"✅ Backed up original config to {config_backup}")

    # Create Optuna study
    study = optuna.create_study(
        direction="maximize",
        sampler=optuna.samplers.TPESampler(seed=42)
    )

    # Run optimization
    print(f"\nStarting optimization ({args.trials} trials)...\n")
    study.optimize(
        lambda trial: objective(trial, args.end_date, args.config),
        n_trials=args.trials,
        show_progress_bar=True
    )

    # Get best trial
    best_trial = study.best_trial

    print("\n" + "="*80)
    print("OPTIMIZATION COMPLETE")
    print("="*80)

    if best_trial.value == -999:
        print("❌ NO VALID CONFIGS FOUND")
        print("   All trials failed validation (overfitting detected)")
        print(f"\n   Restoring original config from {config_backup}")
        subprocess.run(["cp", config_backup, args.config])
        return 1

    print(f"✅ Best Trial: {best_trial.number}")
    print(f"   Evaluation MRD: {best_trial.value:.3f}%")
    print(f"\n   Parameters:")
    for key, value in best_trial.params.items():
        print(f"     {key}: {value}")

    # Update config with best params
    update_config(best_trial.params, args.config)
    print(f"\n✅ Updated config: {args.config}")

    # Run final validation
    print(f"\nRunning final validation...")
    final_results = run_evaluation(args.end_date, args.config)

    print(f"\n{'='*80}")
    print("FINAL VALIDATION RESULTS")
    print(f"{'='*80}")
    print(f"  Evaluation MRD:   {final_results['eval_mrd']:+.3f}%")
    print(f"  Validation MRD:   {final_results['val_mrd']:+.3f}%")
    print(f"  Degradation:      {final_results['degradation_pct']*100:+.1f}%")
    print(f"  Verdict:          {final_results['verdict']}")
    print(f"{'='*80}\n")

    # Save results
    output_data = {
        "timestamp": datetime.now().isoformat(),
        "end_date": args.end_date,
        "n_trials": args.trials,
        "best_trial": best_trial.number,
        "best_params": best_trial.params,
        "best_eval_mrd": best_trial.value,
        "final_validation": final_results,
        "all_trials": [
            {
                "trial": t.number,
                "params": t.params,
                "value": t.value,
                "passed_validation": t.value != -999
            }
            for t in study.trials
        ]
    }

    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, 'w') as f:
        json.dump(output_data, f, indent=2)

    print(f"📊 Results saved to: {args.output}")

    # Summary statistics
    passed_trials = [t for t in study.trials if t.value != -999]
    print(f"\nSummary:")
    print(f"  Total Trials:     {len(study.trials)}")
    print(f"  Passed:           {len(passed_trials)} ({len(passed_trials)/len(study.trials)*100:.1f}%)")
    print(f"  Failed:           {len(study.trials) - len(passed_trials)} ({(len(study.trials)-len(passed_trials))/len(study.trials)*100:.1f}%)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
