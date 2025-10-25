#!/usr/bin/env python3
"""
SIGOR Detector Weight Optimization
Optimizes the 7 detector weights to maximize MRD on validation data
"""

import json
import subprocess
import itertools
from typing import Dict, List, Tuple
import sys
from datetime import datetime

# Optimization configuration
TRAIN_DATES = ["2025-10-14", "2025-10-15", "2025-10-16", "2025-10-17", "2025-10-18"]
VAL_DATES = ["2025-10-20", "2025-10-21", "2025-10-22", "2025-10-23", "2025-10-24"]

CONFIG_FILE = "config/sigor_params.json"
BACKUP_FILE = "config/sigor_params.json.bak"
RESULTS_FILE = "results/weight_optimization/optimization_results.json"

# Weight search space (coarse grid first)
WEIGHT_OPTIONS = [0.1, 0.5, 1.0, 1.5, 2.0]

# Detector names
DETECTORS = ["w_boll", "w_rsi", "w_mom", "w_vwap", "w_orb", "w_ofi", "w_vol"]

def run_backtest(date: str) -> Dict:
    """Run backtest on a single date and return metrics"""
    cmd = ["./build/sentio_lite", "mock", "--date", date, "--no-dashboard"]

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
        print(f"  ⚠️  Timeout on {date}")
        return {"mrd": -999, "winrate": 0, "trades": 0}
    except Exception as e:
        print(f"  ⚠️  Error on {date}: {e}")
        return {"mrd": -999, "winrate": 0, "trades": 0}

def evaluate_weights(weights: Dict[str, float], dates: List[str]) -> float:
    """Evaluate a weight configuration on multiple dates, return avg MRD"""
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
        metrics = run_backtest(date)
        total_mrd += metrics["mrd"]
        total_trades += metrics["trades"]

    avg_mrd = total_mrd / len(dates)
    return avg_mrd, total_trades

def random_search(n_iterations: int = 50):
    """Random search over weight space"""
    import random

    print("="*70)
    print("SIGOR WEIGHT OPTIMIZATION - Random Search")
    print("="*70)
    print(f"Training dates: {', '.join(TRAIN_DATES)}")
    print(f"Validation dates: {', '.join(VAL_DATES)}")
    print(f"Iterations: {n_iterations}")
    print()

    # Backup original config
    with open(CONFIG_FILE) as f:
        original_config = json.load(f)

    with open(BACKUP_FILE, 'w') as f:
        json.dump(original_config, f, indent=2)

    best_weights = None
    best_train_mrd = -999
    best_val_mrd = -999

    results = []

    for i in range(n_iterations):
        # Generate random weights
        weights = {
            detector: random.choice(WEIGHT_OPTIONS)
            for detector in DETECTORS
        }

        print(f"[{i+1}/{n_iterations}] Testing: {weights}")

        # Evaluate on training set
        train_mrd, train_trades = evaluate_weights(weights, TRAIN_DATES)
        print(f"  Train: MRD={train_mrd:.3f}%, Trades={train_trades}")

        # Evaluate on validation set
        val_mrd, val_trades = evaluate_weights(weights, VAL_DATES)
        print(f"  Val:   MRD={val_mrd:.3f}%, Trades={val_trades}")

        # Track results
        result = {
            "iteration": i + 1,
            "weights": weights,
            "train_mrd": train_mrd,
            "val_mrd": val_mrd,
            "train_trades": train_trades,
            "val_trades": val_trades
        }
        results.append(result)

        # Update best
        if val_mrd > best_val_mrd:
            best_val_mrd = val_mrd
            best_train_mrd = train_mrd
            best_weights = weights.copy()
            print(f"  🎯 NEW BEST! Val MRD={val_mrd:.3f}%")

        print()

    # Report results
    print("="*70)
    print("OPTIMIZATION COMPLETE")
    print("="*70)
    print()
    print("BEST CONFIGURATION:")
    print(f"  Weights: {best_weights}")
    print(f"  Train MRD: {best_train_mrd:.3f}%")
    print(f"  Val MRD: {best_val_mrd:.3f}%")
    print()

    # Compare to baseline
    baseline_weights = {
        "w_boll": 0.7, "w_rsi": 1.8, "w_mom": 0.8,
        "w_vwap": 1.6, "w_orb": 0.1, "w_ofi": 1.1, "w_vol": 0.1
    }

    print("BASELINE (from config/baseline_best_config.json):")
    baseline_train_mrd, _ = evaluate_weights(baseline_weights, TRAIN_DATES)
    baseline_val_mrd, _ = evaluate_weights(baseline_weights, VAL_DATES)
    print(f"  Weights: {baseline_weights}")
    print(f"  Train MRD: {baseline_train_mrd:.3f}%")
    print(f"  Val MRD: {baseline_val_mrd:.3f}%")
    print()

    improvement = best_val_mrd - baseline_val_mrd
    print(f"IMPROVEMENT: {improvement:+.3f}%")

    if improvement > 0:
        print("✅ Optimized weights are BETTER than baseline")
    else:
        print("❌ Baseline weights are still BETTER")

    print("="*70)

    # Save results
    import os
    os.makedirs("results/weight_optimization", exist_ok=True)

    output = {
        "timestamp": datetime.now().isoformat(),
        "train_dates": TRAIN_DATES,
        "val_dates": VAL_DATES,
        "n_iterations": n_iterations,
        "best_weights": best_weights,
        "best_train_mrd": best_train_mrd,
        "best_val_mrd": best_val_mrd,
        "baseline_weights": baseline_weights,
        "baseline_train_mrd": baseline_train_mrd,
        "baseline_val_mrd": baseline_val_mrd,
        "improvement": improvement,
        "all_results": results
    }

    with open(RESULTS_FILE, 'w') as f:
        json.dump(output, f, indent=2)

    print(f"\n📊 Results saved to: {RESULTS_FILE}")

    # Restore original config
    with open(BACKUP_FILE) as f:
        original_config = json.load(f)

    with open(CONFIG_FILE, 'w') as f:
        json.dump(original_config, f, indent=2)

    print(f"✅ Original config restored from backup")

    return best_weights, best_val_mrd

def grid_search_top_detectors():
    """Grid search focusing on top 4 detectors (RSI, VWAP, Boll, OFI)"""
    print("="*70)
    print("SIGOR WEIGHT OPTIMIZATION - Focused Grid Search")
    print("Optimizing: RSI, VWAP, Bollinger, OFI (top 4 detectors)")
    print("Fixed: Momentum=0.8, ORB=0.1, Vol=0.1")
    print("="*70)
    print()

    # Backup original config
    with open(CONFIG_FILE) as f:
        original_config = json.load(f)

    with open(BACKUP_FILE, 'w') as f:
        json.dump(original_config, f, indent=2)

    # Fixed weights for low-impact detectors
    fixed_weights = {"w_mom": 0.8, "w_orb": 0.1, "w_vol": 0.1}

    # Variable weights for high-impact detectors
    variable_detectors = ["w_boll", "w_rsi", "w_vwap", "w_ofi"]
    weight_options = [0.5, 1.0, 1.5, 2.0]

    total_configs = len(weight_options) ** len(variable_detectors)
    print(f"Testing {total_configs} configurations...")
    print()

    best_weights = None
    best_val_mrd = -999

    config_num = 0

    for combo in itertools.product(weight_options, repeat=len(variable_detectors)):
        config_num += 1

        # Build weight dict
        weights = fixed_weights.copy()
        for i, detector in enumerate(variable_detectors):
            weights[detector] = combo[i]

        print(f"[{config_num}/{total_configs}] {weights}")

        # Evaluate
        val_mrd, val_trades = evaluate_weights(weights, VAL_DATES)
        print(f"  Val MRD: {val_mrd:.3f}%, Trades: {val_trades}")

        if val_mrd > best_val_mrd:
            best_val_mrd = val_mrd
            best_weights = weights.copy()
            print(f"  🎯 NEW BEST!")

        print()

    print("="*70)
    print("BEST CONFIGURATION:")
    print(f"  {best_weights}")
    print(f"  Val MRD: {best_val_mrd:.3f}%")
    print("="*70)

    # Restore original config
    with open(BACKUP_FILE) as f:
        original_config = json.load(f)

    with open(CONFIG_FILE, 'w') as f:
        json.dump(original_config, f, indent=2)

    return best_weights, best_val_mrd

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Optimize SIGOR detector weights")
    parser.add_argument("--mode", choices=["random", "grid"], default="random",
                      help="Optimization mode (default: random)")
    parser.add_argument("--iterations", type=int, default=50,
                      help="Number of random search iterations (default: 50)")

    args = parser.parse_args()

    if args.mode == "random":
        random_search(args.iterations)
    else:
        grid_search_top_detectors()
