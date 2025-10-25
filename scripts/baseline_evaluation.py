#!/usr/bin/env python3
"""
Baseline SIGOR Evaluation & VWAP Bands Comparison

Workflow:
1. Run baseline SIGOR optimization (200 trials) on Oct 20-24
2. Save best config as baseline_best_config.json
3. Integrate VWAP Bands detector
4. Run VWAP-enhanced optimization (200 trials) on Oct 20-24
5. Save VWAP config as vwap_enhanced_config.json
6. Compare both configs on same 5-day test period
"""

import subprocess
import json
import sys
from pathlib import Path
from datetime import datetime
import optuna
from typing import Dict, List, Tuple

# Test dates
TEST_DATES = ["2024-10-20", "2024-10-21", "2024-10-22", "2024-10-23", "2024-10-24"]
N_TRIALS = 200

class SIGOROptimizer:
    def __init__(self, binary_path: str = "./build/sentio_lite"):
        self.binary_path = binary_path

    def run_sigor_mock(self, date: str, config_override: Dict = None) -> Dict:
        """Run SIGOR on a single date and return metrics"""
        cmd = [
            self.binary_path,
            "mock",
            "--strategy", "sigor",
            "--date", date,
            
        ]

        # If config override provided, write temp config
        if config_override:
            config_path = "/tmp/sigor_optuna_config.json"
            with open(config_path, 'w') as f:
                json.dump(config_override, f)
            cmd.extend(["--config", config_path])

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=120
            )

            # Parse output for metrics
            metrics = self._parse_metrics(result.stdout)
            return metrics

        except subprocess.TimeoutExpired:
            print(f"Timeout for date {date}")
            return {"mrd": 0.0, "sharpe": 0.0, "total_pnl": 0.0}
        except Exception as e:
            print(f"Error running mock for {date}: {e}")
            return {"mrd": 0.0, "sharpe": 0.0, "total_pnl": 0.0}

    def _parse_metrics(self, output: str) -> Dict:
        """Parse metrics from sentio_lite output"""
        metrics = {
            "mrd": 0.0,
            "sharpe": 0.0,
            "total_pnl": 0.0,
            "win_rate": 0.0,
            "num_trades": 0
        }

        for line in output.split('\n'):
            if "MRD:" in line or "Mean Reversion Deviation:" in line:
                try:
                    metrics["mrd"] = float(line.split(':')[-1].strip().rstrip('%'))
                except:
                    pass
            elif "Sharpe:" in line or "Sharpe Ratio:" in line:
                try:
                    metrics["sharpe"] = float(line.split(':')[-1].strip())
                except:
                    pass
            elif "Total P&L:" in line or "Total PnL:" in line:
                try:
                    pnl_str = line.split(':')[-1].strip().rstrip('%$')
                    metrics["total_pnl"] = float(pnl_str)
                except:
                    pass
            elif "Win Rate:" in line:
                try:
                    metrics["win_rate"] = float(line.split(':')[-1].strip().rstrip('%'))
                except:
                    pass
            elif "Trades:" in line or "Total Trades:" in line:
                try:
                    metrics["num_trades"] = int(line.split(':')[-1].strip())
                except:
                    pass

        return metrics

    def evaluate_multi_day(self, dates: List[str], config: Dict = None) -> Dict:
        """Run SIGOR on multiple dates and return averaged metrics"""
        all_metrics = []

        for date in dates:
            print(f"  Testing {date}...", flush=True)
            metrics = self.run_sigor_mock(date, config)
            all_metrics.append(metrics)

        # Calculate averages
        avg_metrics = {
            "avg_mrd": sum(m["mrd"] for m in all_metrics) / len(all_metrics),
            "avg_sharpe": sum(m["sharpe"] for m in all_metrics) / len(all_metrics),
            "total_pnl": sum(m["total_pnl"] for m in all_metrics),
            "avg_win_rate": sum(m["win_rate"] for m in all_metrics) / len(all_metrics),
            "total_trades": sum(m["num_trades"] for m in all_metrics),
            "daily_metrics": all_metrics
        }

        return avg_metrics

def create_sigor_config(trial: optuna.Trial, enable_vwap_bands: bool = False) -> Dict:
    """Create SIGOR config from optuna trial"""

    config = {
        "strategy": "sigor",
        "detectors": {
            "orb": {
                "enabled": True,
                "breakout_threshold_pct": trial.suggest_float("orb_threshold", 0.3, 1.5),
                "min_volume_ratio": trial.suggest_float("orb_volume", 0.8, 2.0),
                "confirmation_bars": trial.suggest_int("orb_confirm", 1, 5)
            },
            "vwap": {
                "enabled": True,
                "z_threshold": trial.suggest_float("vwap_z", 1.5, 3.0),
                "lookback_bars": trial.suggest_int("vwap_lookback", 10, 50)
            },
            "momentum": {
                "enabled": True,
                "rsi_oversold": trial.suggest_int("mom_rsi_os", 25, 35),
                "rsi_overbought": trial.suggest_int("mom_rsi_ob", 65, 75),
                "macd_threshold": trial.suggest_float("mom_macd", 0.5, 2.0)
            },
            "regime": {
                "enabled": True,
                "vol_threshold": trial.suggest_float("regime_vol", 0.5, 2.0),
                "trend_strength": trial.suggest_float("regime_trend", 0.3, 0.8)
            }
        },
        "fusion": {
            "method": trial.suggest_categorical("fusion_method", ["weighted_avg", "majority_vote"]),
            "min_confidence": trial.suggest_float("min_confidence", 0.3, 0.7),
            "orb_weight": trial.suggest_float("weight_orb", 0.5, 1.5),
            "vwap_weight": trial.suggest_float("weight_vwap", 0.5, 1.5),
            "momentum_weight": trial.suggest_float("weight_mom", 0.3, 1.0),
            "regime_weight": trial.suggest_float("weight_regime", 0.2, 0.8)
        },
        "risk": {
            "max_position_size": trial.suggest_float("max_pos_size", 0.15, 0.30),
            "stop_loss_pct": trial.suggest_float("stop_loss", 1.5, 3.0),
            "take_profit_pct": trial.suggest_float("take_profit", 2.0, 5.0),
            "max_holding_bars": trial.suggest_int("max_holding", 10, 30)
        }
    }

    # Add VWAP Bands detector if enabled
    if enable_vwap_bands:
        config["detectors"]["vwap_bands"] = {
            "enabled": True,
            "entry_z_threshold": trial.suggest_float("vwap_bands_entry_z", 1.5, 2.5),
            "exit_z_threshold": trial.suggest_float("vwap_bands_exit_z", 0.3, 0.7),
            "no_go_threshold_pct": trial.suggest_float("vwap_bands_no_go", 1.0, 2.0)
        }
        config["fusion"]["vwap_bands_weight"] = trial.suggest_float("weight_vwap_bands", 0.5, 1.2)

    return config

def objective_baseline(trial: optuna.Trial, optimizer: SIGOROptimizer, dates: List[str]) -> float:
    """Optuna objective function for baseline SIGOR"""
    config = create_sigor_config(trial, enable_vwap_bands=False)

    metrics = optimizer.evaluate_multi_day(dates, config)

    # Objective: maximize average MRD
    # With penalty for low trade count
    mrd = metrics["avg_mrd"]
    trade_penalty = 0 if metrics["total_trades"] >= 20 else -10.0

    return mrd + trade_penalty

def objective_vwap_enhanced(trial: optuna.Trial, optimizer: SIGOROptimizer, dates: List[str]) -> float:
    """Optuna objective function for VWAP-enhanced SIGOR"""
    config = create_sigor_config(trial, enable_vwap_bands=True)

    metrics = optimizer.evaluate_multi_day(dates, config)

    mrd = metrics["avg_mrd"]
    trade_penalty = 0 if metrics["total_trades"] >= 20 else -10.0

    return mrd + trade_penalty

def run_optimization(
    optimizer: SIGOROptimizer,
    dates: List[str],
    n_trials: int,
    enable_vwap_bands: bool,
    study_name: str
) -> Tuple[Dict, optuna.Study]:
    """Run optuna optimization"""

    print(f"\n{'='*60}")
    print(f"Running optimization: {study_name}")
    print(f"VWAP Bands: {'ENABLED' if enable_vwap_bands else 'DISABLED'}")
    print(f"Trials: {n_trials}")
    print(f"Dates: {', '.join(dates)}")
    print(f"{'='*60}\n")

    study = optuna.create_study(
        study_name=study_name,
        direction="maximize",
        sampler=optuna.samplers.TPESampler(seed=42)
    )

    objective_func = objective_vwap_enhanced if enable_vwap_bands else objective_baseline

    study.optimize(
        lambda trial: objective_func(trial, optimizer, dates),
        n_trials=n_trials,
        show_progress_bar=True
    )

    print(f"\n✅ Optimization complete!")
    print(f"Best MRD: {study.best_value:.2f}%")
    print(f"Best params: {json.dumps(study.best_params, indent=2)}")

    # Create best config
    best_config = create_sigor_config(study.best_trial, enable_vwap_bands)

    return best_config, study

def compare_configs(
    optimizer: SIGOROptimizer,
    baseline_config: Dict,
    vwap_config: Dict,
    test_dates: List[str]
):
    """Compare baseline vs VWAP-enhanced on test period"""

    print(f"\n{'='*60}")
    print("FINAL COMPARISON: Baseline vs VWAP-Enhanced")
    print(f"{'='*60}\n")

    print("🔵 Testing BASELINE config...")
    baseline_metrics = optimizer.evaluate_multi_day(test_dates, baseline_config)

    print("\n🟢 Testing VWAP-ENHANCED config...")
    vwap_metrics = optimizer.evaluate_multi_day(test_dates, vwap_config)

    print(f"\n{'='*60}")
    print("RESULTS SUMMARY")
    print(f"{'='*60}\n")

    print(f"{'Metric':<25} {'Baseline':<15} {'VWAP-Enhanced':<15} {'Δ Improvement'}")
    print(f"{'-'*70}")

    metrics_to_compare = [
        ("Avg MRD (%)", "avg_mrd"),
        ("Avg Sharpe", "avg_sharpe"),
        ("Total P&L (%)", "total_pnl"),
        ("Avg Win Rate (%)", "avg_win_rate"),
        ("Total Trades", "total_trades")
    ]

    for label, key in metrics_to_compare:
        baseline_val = baseline_metrics[key]
        vwap_val = vwap_metrics[key]

        if key == "total_trades":
            delta_str = f"+{vwap_val - baseline_val:.0f}"
        else:
            delta = vwap_val - baseline_val
            delta_pct = (delta / baseline_val * 100) if baseline_val != 0 else 0
            delta_str = f"+{delta:.2f} ({delta_pct:+.1f}%)"

        print(f"{label:<25} {baseline_val:<15.2f} {vwap_val:<15.2f} {delta_str}")

    print(f"\n{'-'*70}")

    # Day-by-day comparison
    print(f"\n{'='*60}")
    print("DAY-BY-DAY BREAKDOWN")
    print(f"{'='*60}\n")

    for i, date in enumerate(test_dates):
        baseline_day = baseline_metrics["daily_metrics"][i]
        vwap_day = vwap_metrics["daily_metrics"][i]

        print(f"{date}:")
        print(f"  Baseline MRD: {baseline_day['mrd']:.2f}% | Sharpe: {baseline_day['sharpe']:.2f}")
        print(f"  VWAP-Enh MRD: {vwap_day['mrd']:.2f}% | Sharpe: {vwap_day['sharpe']:.2f}")
        print(f"  Δ MRD: {vwap_day['mrd'] - baseline_day['mrd']:+.2f}%\n")

    return baseline_metrics, vwap_metrics

def main():
    print("""
╔══════════════════════════════════════════════════════════════╗
║  SIGOR Baseline Evaluation & VWAP Bands Comparison          ║
║  Testing Period: Oct 20-24, 2024                            ║
╚══════════════════════════════════════════════════════════════╝
""")

    optimizer = SIGOROptimizer()

    # Step 1: Baseline optimization
    print("\n📊 PHASE 1: Baseline SIGOR Optimization")
    baseline_config, baseline_study = run_optimization(
        optimizer=optimizer,
        dates=TEST_DATES,
        n_trials=N_TRIALS,
        enable_vwap_bands=False,
        study_name="baseline_sigor"
    )

    # Save baseline config
    baseline_config_path = "config/baseline_best_config.json"
    Path(baseline_config_path).parent.mkdir(exist_ok=True)
    with open(baseline_config_path, 'w') as f:
        json.dump(baseline_config, f, indent=2)
    print(f"\n✅ Saved baseline config to {baseline_config_path}")

    # Step 2: VWAP-enhanced optimization
    print("\n📊 PHASE 2: VWAP-Enhanced SIGOR Optimization")
    vwap_config, vwap_study = run_optimization(
        optimizer=optimizer,
        dates=TEST_DATES,
        n_trials=N_TRIALS,
        enable_vwap_bands=True,
        study_name="vwap_enhanced_sigor"
    )

    # Save VWAP config
    vwap_config_path = "config/vwap_enhanced_config.json"
    with open(vwap_config_path, 'w') as f:
        json.dump(vwap_config, f, indent=2)
    print(f"\n✅ Saved VWAP-enhanced config to {vwap_config_path}")

    # Step 3: Final comparison
    print("\n📊 PHASE 3: Head-to-Head Comparison")
    baseline_metrics, vwap_metrics = compare_configs(
        optimizer=optimizer,
        baseline_config=baseline_config,
        vwap_config=vwap_config,
        test_dates=TEST_DATES
    )

    # Save comparison results
    results = {
        "test_period": {
            "start_date": TEST_DATES[0],
            "end_date": TEST_DATES[-1],
            "num_days": len(TEST_DATES)
        },
        "optimization": {
            "n_trials": N_TRIALS,
            "baseline_best_mrd": baseline_study.best_value,
            "vwap_enhanced_best_mrd": vwap_study.best_value
        },
        "baseline": {
            "config": baseline_config,
            "metrics": baseline_metrics
        },
        "vwap_enhanced": {
            "config": vwap_config,
            "metrics": vwap_metrics
        },
        "comparison": {
            "mrd_improvement": vwap_metrics["avg_mrd"] - baseline_metrics["avg_mrd"],
            "sharpe_improvement": vwap_metrics["avg_sharpe"] - baseline_metrics["avg_sharpe"],
            "pnl_improvement": vwap_metrics["total_pnl"] - baseline_metrics["total_pnl"]
        }
    }

    results_path = "results/baseline_vs_vwap_comparison.json"
    Path(results_path).parent.mkdir(exist_ok=True)
    with open(results_path, 'w') as f:
        json.dump(results, f, indent=2)

    print(f"\n✅ Saved detailed results to {results_path}")

    print(f"\n{'='*60}")
    print("✅ EVALUATION COMPLETE")
    print(f"{'='*60}\n")

    # Final verdict
    mrd_improvement = vwap_metrics["avg_mrd"] - baseline_metrics["avg_mrd"]
    if mrd_improvement > 1.0:
        print("🎉 VERDICT: VWAP Bands detector shows STRONG improvement!")
        print(f"   Recommend immediate integration")
    elif mrd_improvement > 0.3:
        print("✅ VERDICT: VWAP Bands detector shows MODERATE improvement")
        print(f"   Recommend integration with monitoring")
    elif mrd_improvement > 0:
        print("⚠️  VERDICT: VWAP Bands detector shows MARGINAL improvement")
        print(f"   Consider further testing")
    else:
        print("❌ VERDICT: VWAP Bands detector shows NO improvement")
        print(f"   Do NOT integrate")

    print(f"\nMRD Δ: {mrd_improvement:+.2f}%")
    print(f"Sharpe Δ: {vwap_metrics['avg_sharpe'] - baseline_metrics['avg_sharpe']:+.2f}")

    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n\n❌ Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
