#!/usr/bin/env python3
"""
Config Evaluation Script with Overfitting Detection

Given an end date, evaluates a config file on:
- Evaluation Set: 5 most recent trading days (up to end_date)
- Validation Set: 10 trading days prior to evaluation set

Reports average MRD on both sets and checks for overfitting (>20% degradation).

Usage:
    ./scripts/evaluate_config.py --end-date 2025-10-24 --config config/trading_params.json
"""

import json
import subprocess
import argparse
from datetime import datetime, timedelta
from typing import Dict, List, Tuple
import os


def get_trading_days(end_date: str, n_days: int) -> List[str]:
    """
    Get the n most recent trading days up to and including end_date.
    Excludes weekends.
    """
    end = datetime.strptime(end_date, "%Y-%m-%d")
    trading_days = []

    current = end
    while len(trading_days) < n_days:
        # Skip weekends (Saturday=5, Sunday=6)
        if current.weekday() < 5:
            trading_days.append(current.strftime("%Y-%m-%d"))
        current -= timedelta(days=1)

    return list(reversed(trading_days))


def get_evaluation_and_validation_sets(end_date: str) -> Tuple[List[str], List[str]]:
    """
    Get evaluation (5 most recent) and validation (10 prior) trading day sets.

    Returns:
        (evaluation_dates, validation_dates)
    """
    # Get 15 trading days total (5 eval + 10 validation)
    all_days = get_trading_days(end_date, 15)

    # Split: first 10 are validation, last 5 are evaluation
    validation_set = all_days[:10]
    evaluation_set = all_days[10:]

    return evaluation_set, validation_set


def run_backtest(date: str, sentio_bin: str = "./build/sentio_lite") -> Dict:
    """Run backtest on a single date and return metrics"""
    cmd = [sentio_bin, "mock", "--date", date, "--no-dashboard"]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        output = result.stdout

        # Parse metrics
        mrd = 0.0
        winrate = 0.0
        trades = 0
        pnl = 0.0
        wins = 0
        losses = 0

        for line in output.split('\n'):
            if "MRD (Daily):" in line:
                mrd = float(line.split()[2].rstrip('%'))
            elif "Win Rate:" in line:
                winrate = float(line.split()[2].rstrip('%'))
            elif "Total Trades:" in line:
                trades = int(line.split()[2])
            elif "Total P&L:" in line:
                pnl_str = line.split()[2].replace('$', '').replace(',', '')
                pnl = float(pnl_str)
            elif "Wins:" in line and "(" not in line:
                wins = int(line.split()[1])
            elif "Losses:" in line and "(" not in line:
                losses = int(line.split()[1])

        return {
            "mrd": mrd,
            "winrate": winrate,
            "trades": trades,
            "pnl": pnl,
            "wins": wins,
            "losses": losses
        }

    except subprocess.TimeoutExpired:
        return {"mrd": -999, "winrate": 0, "trades": 0, "pnl": 0, "wins": 0, "losses": 0}
    except Exception as e:
        print(f"Error running backtest for {date}: {e}")
        return {"mrd": -999, "winrate": 0, "trades": 0, "pnl": 0, "wins": 0, "losses": 0}


def evaluate_on_dates(dates: List[str], set_name: str) -> Dict:
    """Evaluate config on a set of dates"""
    print(f"\n{'='*80}")
    print(f"Evaluating on {set_name} ({len(dates)} days)")
    print(f"{'='*80}")

    total_mrd = 0.0
    total_trades = 0
    total_pnl = 0.0
    total_wins = 0
    total_losses = 0
    daily_results = []

    for date in dates:
        print(f"  Running {date}...", end=" ", flush=True)
        metrics = run_backtest(date)

        if metrics["mrd"] == -999:
            print(f"FAILED (skipping)")
            continue

        print(f"MRD: {metrics['mrd']:+.2f}%, Trades: {metrics['trades']}, P&L: ${metrics['pnl']:,.2f}")

        daily_results.append({
            "date": date,
            "mrd": metrics["mrd"],
            "trades": metrics["trades"],
            "pnl": metrics["pnl"],
            "wins": metrics["wins"],
            "losses": metrics["losses"]
        })

        total_mrd += metrics["mrd"]
        total_trades += metrics["trades"]
        total_pnl += metrics["pnl"]
        total_wins += metrics["wins"]
        total_losses += metrics["losses"]

    n_days = len(daily_results)
    avg_mrd = total_mrd / n_days if n_days > 0 else 0
    avg_trades = total_trades / n_days if n_days > 0 else 0
    overall_winrate = (total_wins / (total_wins + total_losses) * 100) if (total_wins + total_losses) > 0 else 0

    print(f"\n{set_name} Summary:")
    print(f"  Average MRD:     {avg_mrd:+.3f}%")
    print(f"  Average Trades:  {avg_trades:.1f} per day")
    print(f"  Total P&L:       ${total_pnl:,.2f}")
    print(f"  Win Rate:        {overall_winrate:.1f}% ({total_wins}W / {total_losses}L)")

    return {
        "avg_mrd": avg_mrd,
        "avg_trades": avg_trades,
        "total_pnl": total_pnl,
        "total_trades": total_trades,
        "total_wins": total_wins,
        "total_losses": total_losses,
        "winrate": overall_winrate,
        "daily_results": daily_results
    }


def check_overfitting(eval_mrd: float, val_mrd: float, threshold: float = 0.20) -> Tuple[bool, float]:
    """
    Check if validation MRD degrades more than threshold from evaluation MRD.

    Returns:
        (is_overfit, degradation_pct)
    """
    if eval_mrd <= 0:
        # If evaluation is negative, can't calculate meaningful degradation
        # Consider any further drop as overfitting
        is_overfit = val_mrd < eval_mrd
        degradation_pct = 0.0
    else:
        degradation_pct = (eval_mrd - val_mrd) / eval_mrd
        is_overfit = degradation_pct > threshold

    return is_overfit, degradation_pct


def main():
    parser = argparse.ArgumentParser(
        description="Evaluate config on evaluation and validation sets with overfitting detection"
    )
    parser.add_argument("--end-date", required=True,
                       help="End date for evaluation period (MM-DD), e.g., 10-24 (year is fixed to 2025)")
    parser.add_argument("--config", default="config/trading_params.json",
                       help="Config file to evaluate (default: config/trading_params.json)")
    parser.add_argument("--overfitting-threshold", type=float, default=0.20,
                       help="Overfitting threshold as decimal (default: 0.20 = 20%%)")
    parser.add_argument("--output", default=None,
                       help="Output JSON file for results (optional)")

    args = parser.parse_args()

    # Validate end date and add year
    try:
        # Parse MM-DD format and add year 2025
        # Validate format by parsing as full date
        full_end_date = f"2025-{args.end_date}"
        datetime.strptime(full_end_date, "%Y-%m-%d")
    except ValueError:
        print(f"ERROR: Invalid date format '{args.end_date}'. Use MM-DD (e.g., 10-24)")
        return 1

    # Check if config exists
    if not os.path.exists(args.config):
        print(f"ERROR: Config file not found: {args.config}")
        return 1

    # Load config to show what we're testing
    with open(args.config) as f:
        config = json.load(f)

    print("="*80)
    print("CONFIG EVALUATION WITH OVERFITTING DETECTION")
    print("="*80)
    print(f"  Config File:      {args.config}")
    print(f"  End Date:         {full_end_date}")
    print(f"  Overfit Thresh:   {args.overfitting_threshold*100:.0f}%")

    if "last_updated" in config:
        print(f"  Config Updated:   {config['last_updated']}")

    print("="*80)

    # Get evaluation and validation sets
    eval_dates, val_dates = get_evaluation_and_validation_sets(full_end_date)

    print(f"\nEvaluation Set (5 most recent): {', '.join(eval_dates)}")
    print(f"Validation Set (10 prior):      {', '.join(val_dates)}")

    # Rebuild with current config (ensure binary uses this config)
    print("\nRebuilding binary with current config...")
    result = subprocess.run(["cmake", "--build", "build"],
                          capture_output=True, text=True, timeout=120)
    if result.returncode != 0:
        print(f"ERROR: Build failed")
        print(result.stderr)
        return 1
    print("✅ Build complete")

    # Evaluate on both sets
    eval_results = evaluate_on_dates(eval_dates, "EVALUATION SET")
    val_results = evaluate_on_dates(val_dates, "VALIDATION SET")

    # Check for overfitting
    print(f"\n{'='*80}")
    print("OVERFITTING CHECK")
    print(f"{'='*80}")

    eval_mrd = eval_results["avg_mrd"]
    val_mrd = val_results["avg_mrd"]

    is_overfit, degradation_pct = check_overfitting(
        eval_mrd, val_mrd, args.overfitting_threshold
    )

    print(f"  Evaluation Avg MRD:   {eval_mrd:+.3f}%")
    print(f"  Validation Avg MRD:   {val_mrd:+.3f}%")
    print(f"  Degradation:          {degradation_pct*100:+.1f}%")
    print()

    if is_overfit:
        print(f"  ❌ REJECT: Validation degrades >{args.overfitting_threshold*100:.0f}% from evaluation")
        print(f"     This config is OVERFIT to the evaluation period.")
        verdict = "REJECT"
    else:
        print(f"  ✅ ACCEPT: Validation within {args.overfitting_threshold*100:.0f}% degradation threshold")
        print(f"     This config generalizes well.")
        verdict = "ACCEPT"

    print("="*80)

    # Summary
    print(f"\nFINAL VERDICT: {verdict}")
    print(f"  Evaluation MRD: {eval_mrd:+.3f}%  ({len(eval_dates)} days)")
    print(f"  Validation MRD: {val_mrd:+.3f}%  ({len(val_dates)} days)")
    print(f"  Overfitting:    {degradation_pct*100:+.1f}%")
    print()

    # Save results if requested
    if args.output:
        output_data = {
            "timestamp": datetime.now().isoformat(),
            "config_file": args.config,
            "end_date": args.end_date,
            "overfitting_threshold": args.overfitting_threshold,
            "evaluation_set": {
                "dates": eval_dates,
                "avg_mrd": eval_mrd,
                "avg_trades": eval_results["avg_trades"],
                "total_pnl": eval_results["total_pnl"],
                "winrate": eval_results["winrate"],
                "daily_results": eval_results["daily_results"]
            },
            "validation_set": {
                "dates": val_dates,
                "avg_mrd": val_mrd,
                "avg_trades": val_results["avg_trades"],
                "total_pnl": val_results["total_pnl"],
                "winrate": val_results["winrate"],
                "daily_results": val_results["daily_results"]
            },
            "overfitting": {
                "degradation_pct": degradation_pct,
                "is_overfit": is_overfit,
                "verdict": verdict
            }
        }

        with open(args.output, 'w') as f:
            json.dump(output_data, f, indent=2)

        print(f"📊 Results saved to: {args.output}")

    # Return exit code based on verdict
    return 0 if verdict == "ACCEPT" else 1


if __name__ == "__main__":
    import sys
    sys.exit(main())
