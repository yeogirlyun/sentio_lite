#!/usr/bin/env python3
"""
HTML-based Rotation Trading Dashboard Generator
Uses HTML tables for full control over display
"""

import json
import pandas as pd
import argparse
from datetime import datetime
from pathlib import Path
import pytz
import struct
import bisect

def utc_ms_to_et_string(timestamp_ms, fmt='%Y-%m-%d %H:%M'):
    """Convert UTC millisecond timestamp to ET timezone string"""
    utc_tz = pytz.UTC
    et_tz = pytz.timezone('America/New_York')

    # Convert ms to seconds and create UTC datetime
    dt_utc = datetime.fromtimestamp(timestamp_ms / 1000, tz=utc_tz)

    # Convert to ET
    dt_et = dt_utc.astimezone(et_tz)

    return dt_et.strftime(fmt)

def load_config(config_path='config/rotation_strategy.json'):
    """Load symbols from config"""
    try:
        with open(config_path, 'r') as f:
            config = json.load(f)
            return config.get('symbols', {}).get('active', [])
    except:
        return ['ERX', 'ERY', 'FAS', 'FAZ', 'SDS', 'SSO', 'SQQQ', 'SVIX',
                'TNA', 'TQQQ', 'TZA', 'UVXY', 'SOXL', 'SOXS', 'AAPL', 'MSFT', 'AMZN',
                'TSLA', 'NVDA', 'META', 'BRK.B', 'GOOGL']

def load_trades(trades_path):
    """Load trades from JSONL file"""
    trades = []
    with open(trades_path, 'r') as f:
        for line in f:
            trades.append(json.loads(line.strip()))
    return trades

def load_price_data_binary(symbol, data_dir='data'):
    """Load price data from sentio_lite binary format"""
    bin_path = Path(data_dir) / f"{symbol}.bin"
    if not bin_path.exists():
        return None

    try:
        with open(bin_path, 'rb') as f:
            # Read bar count (size_t = 8 bytes on 64-bit)
            count_bytes = f.read(8)
            if len(count_bytes) < 8:
                return None
            count = struct.unpack('Q', count_bytes)[0]  # unsigned long long

            if count == 0 or count > 100000000:
                return None

            bars = []
            for _ in range(count):
                # Read timestamp string length
                ts_len_bytes = f.read(4)
                if len(ts_len_bytes) < 4:
                    break
                ts_len = struct.unpack('I', ts_len_bytes)[0]  # uint32_t

                if ts_len == 0 or ts_len > 100:
                    break

                # Read timestamp string (skip it)
                ts_str = f.read(ts_len)
                if len(ts_str) < ts_len:
                    break

                # Read actual timestamp (int64_t, seconds since epoch)
                ts_bytes = f.read(8)
                if len(ts_bytes) < 8:
                    break
                ts_epoch = struct.unpack('q', ts_bytes)[0]  # signed long long

                # Read OHLCV (all doubles except volume)
                ohlc_bytes = f.read(8 * 4)  # 4 doubles
                if len(ohlc_bytes) < 32:
                    break
                open_price, high, low, close = struct.unpack('dddd', ohlc_bytes)

                # Read volume (uint64_t)
                vol_bytes = f.read(8)
                if len(vol_bytes) < 8:
                    break
                volume = struct.unpack('Q', vol_bytes)[0]

                # Use UTC timestamp to avoid tz ambiguities; convert later as needed
                bars.append({
                    'timestamp': datetime.utcfromtimestamp(ts_epoch),
                    'open': open_price,
                    'high': high,
                    'low': low,
                    'close': close,
                    'volume': volume
                })

            if not bars:
                return None

            df = pd.DataFrame(bars)
            # Ensure datetimelike and timezone-aware to avoid .dt errors downstream (UTC)
            df['timestamp'] = pd.to_datetime(df['timestamp'], utc=True, errors='coerce')
            return df

    except Exception as e:
        print(f"  ⚠️  Error loading {symbol}: {e}")
        return None

def load_price_data(symbol, data_dir='data/equities'):
    """Load price data for a symbol - try binary first, then CSV"""
    # Try binary format first (sentio_lite)
    bin_data = load_price_data_binary(symbol, data_dir)
    if bin_data is not None:
        return bin_data

    # Fallback to CSV format
    csv_path = Path(data_dir) / f"{symbol}_RTH_NH.csv"
    if not csv_path.exists():
        return None

    df = pd.read_csv(csv_path)
    # Robust datetime parsing with utc awareness
    if 'ts_utc' in df.columns:
        df['timestamp'] = pd.to_datetime(df['ts_utc'], utc=True, errors='coerce')
    elif 'timestamp' in df.columns:
        df['timestamp'] = pd.to_datetime(df['timestamp'], utc=True, errors='coerce')
    else:
        # Create empty timestamp column if missing to avoid attribute errors
        df['timestamp'] = pd.NaT
    return df

def generate_html_dashboard(trades_path, output_path, config_path='config/rotation_strategy.json',
                            data_dir='data/equities', start_equity=100000.0, start_date=None, end_date=None,
                            results_path=None):
    """Generate HTML dashboard with Plotly charts and HTML tables"""

    print(f"\n{'='*60}")
    print("HTML ROTATION TRADING DASHBOARD GENERATOR")
    print(f"{'='*60}")

    # Load symbols and trading config from results.json
    symbols = []
    trading_config = {}
    if results_path and Path(results_path).exists():
        try:
            with open(results_path, 'r') as f:
                results_data = json.load(f)
                # Get symbols from metadata
                symbols_str = results_data.get('metadata', {}).get('symbols', '')
                symbols = [s.strip() for s in symbols_str.split(',') if s.strip()]
                trading_config = results_data.get('config', {})
                print(f"✓ Loaded {len(symbols)} symbols from results")
                print(f"✓ Loaded trading config from results")
        except Exception as e:
            print(f"⚠️  Could not load from results: {e}")
            # Fallback to config file
            symbols = load_config(config_path)
            print(f"✓ Loaded {len(symbols)} symbols from config (fallback)")
    else:
        # Fallback to config file
        symbols = load_config(config_path)
        print(f"✓ Loaded {len(symbols)} symbols from config")

    # Load trades
    print(f"\n📊 Loading trades from: {trades_path}")
    trades = load_trades(trades_path)
    print(f"✓ Loaded {len(trades)} total trades")

    # Determine date range from trades if not provided
    if trades and (start_date is None or end_date is None):
        timestamps_ms = [t['timestamp_ms'] for t in trades if 'timestamp_ms' in t]
        if timestamps_ms:
            min_ts = min(timestamps_ms)
            max_ts = max(timestamps_ms)
            if start_date is None:
                start_date = datetime.fromtimestamp(min_ts / 1000).strftime('%Y-%m-%d')
            if end_date is None:
                end_date = datetime.fromtimestamp(max_ts / 1000).strftime('%Y-%m-%d')

    # Calculate trading days
    if start_date and end_date:
        start_dt = datetime.strptime(start_date, '%Y-%m-%d')
        end_dt = datetime.strptime(end_date, '%Y-%m-%d')
        trading_days = max(1, (end_dt - start_dt).days + 1)
    else:
        trading_days = 1

    # Group trades by symbol
    trades_by_symbol = {}
    for trade in trades:
        symbol = trade['symbol']
        if symbol not in trades_by_symbol:
            trades_by_symbol[symbol] = []
        trades_by_symbol[symbol].append(trade)

    # Calculate performance by symbol
    performance = {}
    for symbol in symbols:
        symbol_trades = trades_by_symbol.get(symbol, [])
        exit_trades = [t for t in symbol_trades if t.get('action') == 'EXIT']

        pnl = sum(t.get('pnl', 0) for t in exit_trades)
        num_trades = len(exit_trades)

        wins = [t for t in exit_trades if t.get('pnl', 0) > 0]
        losses = [t for t in exit_trades if t.get('pnl', 0) < 0]

        num_wins = len(wins)
        num_losses = len(losses)
        win_rate = (num_wins / num_trades * 100) if num_trades > 0 else 0

        avg_win = (sum(t['pnl'] for t in wins) / num_wins) if num_wins > 0 else 0
        avg_loss = (sum(t['pnl'] for t in losses) / num_losses) if num_losses > 0 else 0

        gross_profit = sum(t['pnl'] for t in wins)
        gross_loss = abs(sum(t['pnl'] for t in losses))
        profit_factor = (gross_profit / gross_loss) if gross_loss > 0 else (999 if gross_profit > 0 else 0)

        performance[symbol] = {
            'pnl': pnl,
            'num_trades': num_trades,
            'num_wins': num_wins,
            'num_losses': num_losses,
            'win_rate': win_rate,
            'avg_win': avg_win,
            'avg_loss': avg_loss,
            'profit_factor': profit_factor,
            'total_records': len(symbol_trades)
        }

    # Sort symbols by P&L
    sorted_symbols = sorted(symbols, key=lambda s: performance[s]['pnl'], reverse=True)

    # Calculate totals from results.json if available, otherwise from filtered trades
    total_pnl = sum(p['pnl'] for p in performance.values())  # Always calculate for P&L display

    if results_data and 'performance' in results_data:
        # Use test-day-only metrics from results.json
        total_trades = results_data['performance'].get('total_trades', len(trades))
        final_equity = results_data['performance'].get('final_equity', start_equity)
        total_return_pct = results_data['performance'].get('total_return', 0) * 100
        mrd_pct = results_data['performance'].get('mrd', 0) * 100
    else:
        # Fallback: calculate from trades
        final_equity = start_equity + total_pnl
        total_trades = len(trades)
        total_return_pct = (total_pnl / start_equity * 100) if start_equity > 0 else 0
        mrd_pct = (total_return_pct / trading_days) if trading_days > 0 else 0

    # Symbol colors - varied for visual distinction
    colors = {
        'ERX': '#FF4500', 'ERY': '#8B0000', 'FAS': '#00CED1', 'FAZ': '#4169E1',
        'SDS': '#FF6B6B', 'SSO': '#32CD32', 'SQQQ': '#FF1493', 'SVIX': '#7FFF00',
        'TNA': '#FF8C00', 'TQQQ': '#00BFFF', 'TZA': '#DC143C', 'UVXY': '#9370DB',
        'AAPL': '#A2AAAD', 'MSFT': '#00A4EF', 'AMZN': '#FF9900', 'TSLA': '#CC0000',
        'NVDA': '#76B900', 'META': '#0668E1', 'BRK.B': '#1A1A1A', 'GOOGL': '#4285F4'
    }

    # Start HTML
    strategy = trading_config.get('strategy_name', 'Unknown') if trading_config else 'Unknown'
    html = f"""<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Sentio Lite Dashboard</title>
    <script src="https://cdn.plot.ly/plotly-2.24.1.min.js"></script>
    <style>
        * {{
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }}

        body {{
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: #f8f9fa;
            padding: 20px;
        }}

        .header {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            border-radius: 10px;
            margin-bottom: 30px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }}

        .header h1 {{
            font-size: 2.5em;
            margin-bottom: 10px;
        }}

        .summary {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }}

        .summary-card {{
            background: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }}

        .summary-card h3 {{
            color: #666;
            font-size: 0.9em;
            margin-bottom: 10px;
            text-transform: uppercase;
        }}

        .summary-card .value {{
            font-size: 2em;
            font-weight: bold;
            color: #333;
        }}

        .summary-card.positive .value {{
            color: #28a745;
        }}

        .summary-card.negative .value {{
            color: #dc3545;
        }}

        .symbol-section {{
            background: white;
            margin-bottom: 30px;
            border-radius: 10px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            overflow: hidden;
        }}

        .symbol-header {{
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }}

        .symbol-header-left {{
            flex: 1;
        }}

        .symbol-header h2 {{
            font-size: 2.5em;
            margin: 0;
            font-weight: bold;
        }}

        .symbol-header-right {{
            text-align: right;
            line-height: 1.6;
        }}

        .stat-row {{
            font-size: 1.8em;
            font-weight: bold;
            margin: 5px 0;
        }}

        .stat-label {{
            font-size: 0.7em;
            font-weight: normal;
            opacity: 0.9;
            margin-right: 8px;
        }}


        .chart-container {{
            padding: 20px;
            min-height: 500px;
        }}

        .table-container {{
            padding: 20px;
            overflow-x: auto;
        }}

        table {{
            width: 100%;
            border-collapse: collapse;
            font-size: 0.95em;
        }}

        th {{
            background: #667eea;
            color: white;
            padding: 12px;
            text-align: left;
            font-weight: 600;
            position: sticky;
            top: 0;
        }}

        td {{
            padding: 10px 12px;
            border-bottom: 1px solid #e0e0e0;
        }}

        tr:hover {{
            background: #f5f5f5;
        }}

        .entry-row {{
            background: #fff3cd;
        }}

        .exit-row {{
            background: #d4edda;
        }}

        .profit {{
            color: #28a745;
            font-weight: bold;
        }}

        .loss {{
            color: #dc3545;
            font-weight: bold;
        }}

        .no-trades {{
            text-align: center;
            padding: 40px;
            color: #999;
            font-size: 1.2em;
        }}

        .config-section {{
            background: white;
            padding: 25px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            margin-bottom: 30px;
        }}

        .config-grid {{
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 15px;
            margin-top: 20px;
        }}

        .config-item {{
            display: flex;
            justify-content: space-between;
            padding: 10px;
            background: #f8f9fa;
            border-radius: 5px;
        }}

        .config-label {{
            font-weight: 600;
            color: #495057;
        }}

        .config-value {{
            color: #667eea;
            font-weight: 500;
        }}
    </style>
</head>
<body>
    <div class="header">
        <h1>📊 Sentio Lite - {strategy} Strategy</h1>
        <p>Multi-Symbol Trading Performance Report | {start_date} to {end_date}</p>
    </div>

    <div class="summary">
        <div class="summary-card">
            <h3>Starting Equity</h3>
            <div class="value">${start_equity:,.2f}</div>
        </div>
        <div class="summary-card">
            <h3>Final Equity</h3>
            <div class="value">${final_equity:,.2f}</div>
        </div>
        <div class="summary-card {'positive' if total_pnl >= 0 else 'negative'}">
            <h3>Total P&L</h3>
            <div class="value">${total_pnl:+,.2f}</div>
        </div>
        <div class="summary-card {'positive' if total_return_pct >= 0 else 'negative'}">
            <h3>Total Return</h3>
            <div class="value">{total_return_pct:+.2f}%</div>
        </div>
        <div class="summary-card {'positive' if mrd_pct >= 0 else 'negative'}">
            <h3>MRD (Mean Return/Day)</h3>
            <div class="value">{mrd_pct:+.2f}%</div>
        </div>
        <div class="summary-card">
            <h3>Total Trades</h3>
            <div class="value">{total_trades}</div>
        </div>
    </div>
"""

    # Add configuration section if available
    if trading_config:
        # Show full path for config if it's a relative path
        import os
        full_config_path = os.path.abspath(config_path) if config_path else "N/A"

        strategy_name = trading_config.get('strategy_name', 'Unknown')
        html += f"""
    <div class=\"config-section\">
        <h2 style=\"color: #667eea; margin-bottom: 20px;\">⚙️ {strategy_name} Strategy Parameters</h2>
        <p style=\"font-size: 0.95em; opacity: 0.85; margin-bottom: 10px;\"><strong>Config File:</strong> {full_config_path}</p>
        <div class=\"config-grid\">
"""

        if strategy_name.upper().startswith('SIGOR') and 'sigor' in trading_config:
            s = trading_config['sigor']
            def item(label, value):
                return f"""
            <div class=\"config-item\">
                <span class=\"config-label\">{label}</span>
                <span class=\"config-value\">{value}</span>
            </div>
            """
            html += (
                item('k (sharpness)', s.get('k')) +
                item('w_boll', s.get('w_boll')) +
                item('w_rsi', s.get('w_rsi')) +
                item('w_mom', s.get('w_mom')) +
                item('w_vwap', s.get('w_vwap')) +
                item('w_orb', s.get('w_orb')) +
                item('w_ofi', s.get('w_ofi')) +
                item('w_vol', s.get('w_vol')) +
                item('win_boll', s.get('win_boll')) +
                item('win_rsi', s.get('win_rsi')) +
                item('win_mom', s.get('win_mom')) +
                item('win_vwap', s.get('win_vwap')) +
                item('orb_opening_bars', s.get('orb_opening_bars')) +
                item('vol_window', s.get('vol_window')) +
                item('warmup_bars', s.get('warmup_bars'))
            )
        else:
            # Default EWRLS grid (existing parameters)
            html += f"""
            <div class=\"config-item\"><span class=\"config-label\">1. Max Positions</span><span class=\"config-value\">{trading_config.get('max_positions','N/A')}</span></div>
            <div class=\"config-item\"><span class=\"config-label\">2. Lambda (1-bar)</span><span class=\"config-value\">{trading_config.get('lambda_1bar','N/A')}</span></div>
            <div class=\"config-item\"><span class=\"config-label\">3. Lambda (5-bar)</span><span class=\"config-value\">{trading_config.get('lambda_5bar','N/A')}</span></div>
            <div class=\"config-item\"><span class=\"config-label\">4. Lambda (10-bar)</span><span class=\"config-value\">{trading_config.get('lambda_10bar','N/A')}</span></div>
            <div class=\"config-item\"><span class=\"config-label\">5. Lambda (20-bar)</span><span class=\"config-value\">{trading_config.get('lambda_20bar','N/A')}</span></div>
            <div class=\"config-item\"><span class=\"config-label\">6. Min Prediction (Initial)</span><span class=\"config-value\">{trading_config.get('min_prediction_for_entry',0)*100:.2f}%</span></div>
            <div class=\"config-item\"><span class=\"config-label\">7. Threshold +Trade</span><span class=\"config-value\">+{trading_config.get('min_prediction_increase_on_trade',0)*100:.2f}%</span></div>
            <div class=\"config-item\"><span class=\"config-label\">8. Threshold -NoTrade</span><span class=\"config-value\">-{trading_config.get('min_prediction_decrease_on_no_trade',0)*100:.2f}%</span></div>
            <div class=\"config-item\"><span class=\"config-label\">9. Min Bars to Learn</span><span class=\"config-value\">{trading_config.get('min_bars_to_learn','N/A')}</span></div>
            <div class=\"config-item\"><span class=\"config-label\">10. Bars per Day</span><span class=\"config-value\">{trading_config.get('bars_per_day','N/A')}</span></div>
            <div class=\"config-item\"><span class=\"config-label\">11. Initial Capital</span><span class=\"config-value\">${trading_config.get('initial_capital',0):,.0f}</span></div>
            <div class=\"config-item\"><span class=\"config-label\">12. Lookback Window</span><span class=\"config-value\">{trading_config.get('lookback_window','N/A')} bars</span></div>
            <div class=\"config-item\"><span class=\"config-label\">13. Win Multiplier</span><span class=\"config-value\">{trading_config.get('win_multiplier','N/A')}</span></div>
            <div class=\"config-item\"><span class=\"config-label\">14. Loss Multiplier</span><span class=\"config-value\">{trading_config.get('loss_multiplier','N/A')}</span></div>
            <div class=\"config-item\"><span class=\"config-label\">15. Rotation Delta</span><span class=\"config-value\">{trading_config.get('rotation_strength_delta',0)*100:.2f}%</span></div>
            <div class=\"config-item\"><span class=\"config-label\">16. Min Rank Strength</span><span class=\"config-value\">{trading_config.get('min_rank_strength',0)*100:.3f}%</span></div>
            """

        html += """
        </div>
    </div>
"""

    html += """
    <div class="container" style="margin-top: 30px;">
        <h2 style="color: #667eea; margin-bottom: 20px;">📈 Per-Symbol Performance Summary</h2>
        <div class="table-container">
            <table>
                <thead>
                    <tr>
                        <th>Symbol</th>
                        <th style="text-align: right;">Trades</th>
                        <th style="text-align: right;">Wins</th>
                        <th style="text-align: right;">Losses</th>
                        <th style="text-align: right;">Win Rate</th>
                        <th style="text-align: right;">Total P&L</th>
                        <th style="text-align: right;">Avg Win</th>
                        <th style="text-align: right;">Avg Loss</th>
                        <th style="text-align: right;">Profit Factor</th>
                    </tr>
                </thead>
                <tbody>
"""

    # Add per-symbol summary rows
    for symbol in sorted_symbols:
        perf = performance[symbol]
        if perf['num_trades'] == 0:
            continue

        pnl_sign = '+' if perf['pnl'] > 0 else ''
        avg_win_sign = '+' if perf['avg_win'] > 0 else ''
        avg_loss_sign = '' if perf['avg_loss'] >= 0 else ''  # avg_loss is already negative

        html += f"""
                    <tr>
                        <td style="font-weight: bold; color: {colors.get(symbol, '#667eea')};">{symbol}</td>
                        <td style="text-align: right;">{perf['num_trades']}</td>
                        <td style="text-align: right;">{perf['num_wins']}</td>
                        <td style="text-align: right;">{perf['num_losses']}</td>
                        <td style="text-align: right;">{perf['win_rate']:.1f}%</td>
                        <td style="text-align: right;">{pnl_sign}${perf['pnl']:,.2f}</td>
                        <td style="text-align: right;">{avg_win_sign}${perf['avg_win']:,.2f}</td>
                        <td style="text-align: right;">{avg_loss_sign}${perf['avg_loss']:,.2f}</td>
                        <td style="text-align: right;">{perf['profit_factor']:.2f}</td>
                    </tr>
"""

    html += """
                </tbody>
            </table>
        </div>
    </div>
"""

    # Calculate per-day performance
    daily_performance = {}
    for trade in trades:
        if trade.get('action') == 'EXIT':
            trade_date = utc_ms_to_et_string(trade.get('timestamp_ms', 0), fmt='%Y-%m-%d')

            # Filter to date range
            if start_date and end_date:
                if not (start_date <= trade_date <= end_date):
                    continue

            if trade_date not in daily_performance:
                daily_performance[trade_date] = {
                    'pnl': 0,
                    'num_trades': 0,
                    'num_wins': 0,
                    'num_losses': 0,
                    'gross_wins': 0,
                    'gross_losses': 0
                }

            pnl = trade.get('pnl', 0)
            daily_performance[trade_date]['pnl'] += pnl
            daily_performance[trade_date]['num_trades'] += 1

            if pnl > 0:
                daily_performance[trade_date]['num_wins'] += 1
                daily_performance[trade_date]['gross_wins'] += pnl
            elif pnl < 0:
                daily_performance[trade_date]['num_losses'] += 1
                daily_performance[trade_date]['gross_losses'] += abs(pnl)

    # Add per-day performance table
    html += """
    <div class="container" style="margin-top: 30px;">
        <h2 style="color: #667eea; margin-bottom: 20px;">📅 Per-Day Performance Summary</h2>
        <div class="table-container">
            <table>
                <thead>
                    <tr>
                        <th>Date</th>
                        <th style="text-align: right;">Trades</th>
                        <th style="text-align: right;">Wins</th>
                        <th style="text-align: right;">Losses</th>
                        <th style="text-align: right;">Win Rate</th>
                        <th style="text-align: right;">Daily P&L</th>
                        <th style="text-align: right;">Avg Win</th>
                        <th style="text-align: right;">Avg Loss</th>
                        <th style="text-align: right;">Profit Factor</th>
                    </tr>
                </thead>
                <tbody>
"""

    # Sort days chronologically
    sorted_days = sorted(daily_performance.keys())

    for day in sorted_days:
        perf = daily_performance[day]

        win_rate = (perf['num_wins'] / perf['num_trades'] * 100) if perf['num_trades'] > 0 else 0
        avg_win = (perf['gross_wins'] / perf['num_wins']) if perf['num_wins'] > 0 else 0
        avg_loss = -(perf['gross_losses'] / perf['num_losses']) if perf['num_losses'] > 0 else 0
        profit_factor = (perf['gross_wins'] / perf['gross_losses']) if perf['gross_losses'] > 0 else 0

        pnl_sign = '+' if perf['pnl'] > 0 else ''
        avg_win_sign = '+' if avg_win > 0 else ''
        avg_loss_sign = '' if avg_loss >= 0 else ''

        pnl_color = 'color: #10b981;' if perf['pnl'] > 0 else 'color: #ef4444;' if perf['pnl'] < 0 else ''

        html += f"""
                    <tr>
                        <td style="font-weight: bold;">{day}</td>
                        <td style="text-align: right;">{perf['num_trades']}</td>
                        <td style="text-align: right;">{perf['num_wins']}</td>
                        <td style="text-align: right;">{perf['num_losses']}</td>
                        <td style="text-align: right;">{win_rate:.1f}%</td>
                        <td style="text-align: right; {pnl_color} font-weight: bold;">{pnl_sign}${perf['pnl']:,.2f}</td>
                        <td style="text-align: right;">{avg_win_sign}${avg_win:,.2f}</td>
                        <td style="text-align: right;">{avg_loss_sign}${avg_loss:,.2f}</td>
                        <td style="text-align: right;">{profit_factor:.2f}</td>
                    </tr>
"""

    html += """
                </tbody>
            </table>
        </div>
    </div>
"""

    # Generate sections for each symbol
    for symbol in sorted_symbols:
        symbol_trades = trades_by_symbol.get(symbol, [])
        perf = performance[symbol]
        color = colors.get(symbol, '#667eea')

        if len(symbol_trades) == 0:
            # Skip symbols with no trades
            continue

        # Filter trades by date range for display
        filtered_trades = []
        for t in symbol_trades:
            trade_date_check = utc_ms_to_et_string(t.get('timestamp_ms', 0), fmt='%Y-%m-%d')
            if start_date and end_date:
                if start_date <= trade_date_check <= end_date:
                    filtered_trades.append(t)
            else:
                filtered_trades.append(t)

        # Skip if no trades after filtering
        if len(filtered_trades) == 0:
            continue

        # Calculate stats using filtered trades
        trade_date = utc_ms_to_et_string(filtered_trades[0]['timestamp_ms'], fmt='%Y-%m-%d (%A)') if filtered_trades else 'N/A'
        num_exits = len([t for t in filtered_trades if t.get('action') == 'EXIT'])
        wins = len([t for t in filtered_trades if t.get('action') == 'EXIT' and t.get('pnl', 0) > 0])
        losses = len([t for t in filtered_trades if t.get('action') == 'EXIT' and t.get('pnl', 0) < 0])

        # Use filtered trades for table display
        symbol_trades = filtered_trades

        html += f"""
    <div class="symbol-section">
        <div class="symbol-header" style="background: linear-gradient(135deg, {color} 0%, {color}dd 100%);">
            <div class="symbol-header-left">
                <h2>{symbol}</h2>
                <div style="font-size: 0.9em; margin-top: 8px; opacity: 0.9;">{trade_date}</div>
            </div>
            <div class="symbol-header-right">
                <div class="stat-row">
                    <span class="stat-label">P&L:</span>${perf['pnl']:+,.2f}
                </div>
                <div class="stat-row">
                    <span class="stat-label">Trades:</span>{len(symbol_trades)} ({num_exits} completed)
                </div>
                <div class="stat-row">
                    <span class="stat-label">Win/Loss:</span>{wins}W / {losses}L ({perf['win_rate']:.1f}%)
                </div>
            </div>
        </div>

        <div class="chart-container">
            <div id="chart_{symbol}" style="width:100%; height:500px;"></div>
        </div>

        <div class="table-container">
            <table>
                <thead>
                    <tr>
                        <th>Timestamp</th>
                        <th>Action</th>
                        <th>Price</th>
                        <th>Shares</th>
                        <th>Value</th>
                        <th>P&L</th>
                        <th>P&L %</th>
                        <th>Bars Held</th>
                        <th>Reason</th>
                    </tr>
                </thead>
                <tbody>
"""

        # Add trade rows (filter by date range)
        for trade in symbol_trades:
            timestamp_ms = trade.get('timestamp_ms', 0)
            trade_date = utc_ms_to_et_string(timestamp_ms, fmt='%Y-%m-%d')

            # Skip trades outside the test date range
            if start_date and end_date:
                if not (start_date <= trade_date <= end_date):
                    continue

            action = trade.get('action', '')
            row_class = 'entry-row' if action == 'ENTRY' else 'exit-row'
            timestamp = utc_ms_to_et_string(timestamp_ms)  # Convert UTC to ET
            price = trade.get('price', 0)
            shares = trade.get('shares', 0)
            value = trade.get('value', 0)
            pnl = trade.get('pnl', 0)
            pnl_pct = trade.get('pnl_pct', 0)
            bars_held = trade.get('bars_held', 0)
            reason = trade.get('reason', '')[:60]

            pnl_str = f"${pnl:+,.2f}" if action == 'EXIT' else '-'
            # pnl_pct is already in percentage format (C++ multiplies by 100)
            pnl_pct_str = f"{pnl_pct:+.2f}%" if action == 'EXIT' else '-'
            bars_str = str(bars_held) if action == 'EXIT' else '-'

            html += f"""
                    <tr class="{row_class}">
                        <td>{timestamp}</td>
                        <td><strong>{action}</strong></td>
                        <td>${price:.2f}</td>
                        <td>{shares}</td>
                        <td>${value:,.2f}</td>
                        <td>{pnl_str}</td>
                        <td>{pnl_pct_str}</td>
                        <td>{bars_str}</td>
                        <td>{reason}</td>
                    </tr>
"""

        html += """
                </tbody>
            </table>
        </div>
    </div>
"""

        # Load price data and generate chart script
        price_data = load_price_data(symbol, data_dir)
        if price_data is not None:
            # Ensure timestamp column is datetimelike (UTC), then derive ET view for all downstream logic
            if 'timestamp' in price_data.columns:
                price_data['timestamp'] = pd.to_datetime(price_data['timestamp'], utc=True, errors='coerce')
                price_data = price_data.dropna(subset=['timestamp'])
                price_data['timestamp_et'] = price_data['timestamp'].dt.tz_convert('America/New_York')

            # Filter to only the test date range (ET dates)
            if start_date and end_date:
                price_data['date_et'] = price_data['timestamp_et'].dt.strftime('%Y-%m-%d')
                price_data = price_data[(price_data['date_et'] >= start_date) & (price_data['date_et'] <= end_date)]
            elif len(symbol_trades) > 0:
                # Fallback: use first trade ET date
                first_trade_ts = symbol_trades[0].get('timestamp_ms', 0)
                test_date_str = utc_ms_to_et_string(first_trade_ts, fmt='%Y-%m-%d')
                price_data['date_et'] = price_data['timestamp_et'].dt.strftime('%Y-%m-%d')
                price_data = price_data[price_data['date_et'] == test_date_str]
            else:
                # Fallback: use last 390 bars if no trades
                price_data = price_data.tail(390)

            # CRITICAL: Filter to ONLY regular trading hours (9:30 AM - 4:00 PM ET) and weekdays, using ET clock
            price_data['hour_et'] = price_data['timestamp_et'].dt.hour
            price_data['minute_et'] = price_data['timestamp_et'].dt.minute
            price_data['weekday_et'] = price_data['timestamp_et'].dt.weekday  # 0=Monday, 6=Sunday

            # Keep only RTH: 9:30 AM to 4:00 PM ET, Monday-Friday
            price_data = price_data[
                (price_data['weekday_et'] < 5) &
                (
                    ((price_data['hour_et'] == 9) & (price_data['minute_et'] >= 30)) |
                    ((price_data['hour_et'] >= 10) & (price_data['hour_et'] < 16)) |
                    ((price_data['hour_et'] == 16) & (price_data['minute_et'] == 0))
                )
            ].copy()

            # Create sequential bar numbers for continuous x-axis (no gaps)
            price_data['bar_num'] = range(len(price_data))

            # Create readable timestamps for hover (ET)
            timestamps_str = price_data['timestamp_et'].dt.strftime('%Y-%m-%d %H:%M').tolist()
            bar_numbers = price_data['bar_num'].tolist()
            prices = price_data['close'].tolist()

            # Create custom tick labels (show date at market open of each day)
            tickvals = []
            ticktext = []
            prev_date = None
            for idx, row in price_data.iterrows():
                current_date = row['timestamp_et'].strftime('%Y-%m-%d')
                if current_date != prev_date:
                    tickvals.append(row['bar_num'])
                    ticktext.append(current_date)
                    prev_date = current_date

            # Get entry and exit points - map to bar numbers
            # Create a mapping from timestamp (ET minute) to bar number to match trade timestamps
            ts_to_bar = {}
            # Also build a sorted list of UTC millisecond timestamps for nearest-neighbor snapping
            bar_ms_list = []
            bar_num_list = []
            for idx, row in price_data.iterrows():
                # Convert bar timestamp (ET) to ET string at minute precision
                bar_ms = int(row['timestamp_et'].timestamp() * 1000)
                ts_str = utc_ms_to_et_string(bar_ms)
                ts_to_bar[ts_str] = row['bar_num']
                bar_ms_list.append(bar_ms)
                bar_num_list.append(row['bar_num'])

            # Ensure the lists are sorted by timestamp for bisect operations
            # price_data is already chronological, but we guard against accidental disorder
            if bar_ms_list and any(bar_ms_list[i] > bar_ms_list[i+1] for i in range(len(bar_ms_list)-1)):
                combined = sorted(zip(bar_ms_list, bar_num_list))
                bar_ms_list = [c[0] for c in combined]
                bar_num_list = [c[1] for c in combined]

            def snap_to_nearest_bar(trade_ms_utc: int, max_delta_ms: int = 60000):
                """Return bar_num nearest to trade_ms_utc within ±max_delta_ms, else None."""
                if not bar_ms_list:
                    return None
                idx = bisect.bisect_left(bar_ms_list, trade_ms_utc)
                candidates = []
                if idx < len(bar_ms_list):
                    candidates.append(idx)
                if idx > 0:
                    candidates.append(idx - 1)
                best_bar = None
                best_delta = None
                for c in candidates:
                    delta = abs(bar_ms_list[c] - trade_ms_utc)
                    if best_delta is None or delta < best_delta:
                        best_delta = delta
                        best_bar = bar_num_list[c]
                if best_delta is not None and best_delta <= max_delta_ms:
                    return best_bar
                return None

            entries = []  # (bar_num, price, timestamp_str)
            for t in symbol_trades:
                if t.get('action') == 'ENTRY':
                    ts_str = utc_ms_to_et_string(t['timestamp_ms'])
                    trade_date = utc_ms_to_et_string(t['timestamp_ms'], fmt='%Y-%m-%d')
                    # Only include if within date range
                    if start_date and end_date and not (start_date <= trade_date <= end_date):
                        continue
                    bar_num = ts_to_bar.get(ts_str)
                    if bar_num is None:
                        bar_num = snap_to_nearest_bar(t['timestamp_ms'])
                    if bar_num is not None:
                        entries.append((bar_num, t['price'], ts_str))

            exits = []  # (bar_num, price, timestamp_str)
            for t in symbol_trades:
                if t.get('action') == 'EXIT':
                    ts_str = utc_ms_to_et_string(t['timestamp_ms'])
                    trade_date = utc_ms_to_et_string(t['timestamp_ms'], fmt='%Y-%m-%d')
                    # Only include if within date range
                    if start_date and end_date and not (start_date <= trade_date <= end_date):
                        continue
                    bar_num = ts_to_bar.get(ts_str)
                    if bar_num is None:
                        bar_num = snap_to_nearest_bar(t['timestamp_ms'])
                    if bar_num is not None:
                        exits.append((bar_num, t['price'], ts_str))

            html += f"""
    <script>
        var trace1 = {{
            x: {bar_numbers},
            y: {prices},
            type: 'scatter',
            mode: 'lines',
            name: 'Price',
            line: {{color: '{color}', width: 2}},
            text: {timestamps_str},
            hovertemplate: '%{{text}}<br>Price: $%{{y:.2f}}<extra></extra>'
        }};

        var trace2 = {{
            x: {[e[0] for e in entries]},
            y: {[e[1] for e in entries]},
            type: 'scatter',
            mode: 'markers',
            name: 'Entry',
            marker: {{color: 'green', size: 12, symbol: 'triangle-up'}},
            text: {[e[2] for e in entries]},
            hovertemplate: 'ENTRY<br>%{{text}}<br>$%{{y:.2f}}<extra></extra>'
        }};

        var trace3 = {{
            x: {[e[0] for e in exits]},
            y: {[e[1] for e in exits]},
            type: 'scatter',
            mode: 'markers',
            name: 'Exit',
            marker: {{color: 'red', size: 12, symbol: 'triangle-down'}},
            text: {[e[2] for e in exits]},
            hovertemplate: 'EXIT<br>%{{text}}<br>$%{{y:.2f}}<extra></extra>'
        }};

        var layout = {{
            title: '{symbol} Price & Trades (RTH Only)',
            xaxis: {{
                title: 'Trading Days',
                tickmode: 'array',
                tickvals: {tickvals},
                ticktext: {ticktext}
            }},
            yaxis: {{title: 'Price ($)'}},
            hovermode: 'closest',
            showlegend: true
        }};

        Plotly.newPlot('chart_{symbol}', [trace1, trace2, trace3], layout, {{responsive: true}});
    </script>
"""

    html += """
</body>
</html>
"""

    # Write to file
    with open(output_path, 'w') as f:
        f.write(html)

    print(f"\n✅ Dashboard saved: {output_path}")
    print(f"\n{'='*60}")
    print("SUMMARY")
    print(f"{'='*60}")
    print(f"Test Period:      {start_date} to {end_date} ({trading_days} days)")
    print(f"Starting Equity:  ${start_equity:,.2f}")
    print(f"Final Equity:     ${final_equity:,.2f}")
    print(f"Total P&L:        ${total_pnl:+,.2f} ({total_return_pct:+.2f}%)")
    print(f"MRD:              {mrd_pct:+.2f}% per day")
    print(f"Total Trades:     {total_trades}")
    print(f"{'='*60}\n")

def main():
    parser = argparse.ArgumentParser(description='Generate HTML rotation trading dashboard')
    parser.add_argument('--trades', required=True, help='Path to trades.jsonl')
    parser.add_argument('--output', required=True, help='Output HTML path')
    parser.add_argument('--config', default='config/rotation_strategy.json', help='Config path')
    parser.add_argument('--data-dir', default='data/equities', help='Price data directory')
    parser.add_argument('--start-equity', type=float, default=100000.0, help='Starting equity')
    parser.add_argument('--start-date', default=None, help='Start date (YYYY-MM-DD)')
    parser.add_argument('--end-date', default=None, help='End date (YYYY-MM-DD)')
    parser.add_argument('--results', default=None, help='Path to results.json (for config)')

    args = parser.parse_args()

    generate_html_dashboard(
        args.trades,
        args.output,
        args.config,
        args.data_dir,
        args.start_equity,
        args.start_date,
        args.end_date,
        args.results
    )

if __name__ == '__main__':
    main()
