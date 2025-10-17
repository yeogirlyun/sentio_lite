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
                'TNA', 'TQQQ', 'TZA', 'UVXY', 'AAPL', 'MSFT', 'AMZN',
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

                bars.append({
                    'timestamp': datetime.fromtimestamp(ts_epoch),
                    'open': open_price,
                    'high': high,
                    'low': low,
                    'close': close,
                    'volume': volume
                })

            if not bars:
                return None

            df = pd.DataFrame(bars)
            df['timestamp'] = pd.to_datetime(df['timestamp'])
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
    df['timestamp'] = pd.to_datetime(df['ts_utc'])
    return df

def generate_html_dashboard(trades_path, output_path, config_path='config/rotation_strategy.json',
                            data_dir='data/equities', start_equity=100000.0, start_date=None, end_date=None):
    """Generate HTML dashboard with Plotly charts and HTML tables"""

    print(f"\n{'='*60}")
    print("HTML ROTATION TRADING DASHBOARD GENERATOR")
    print(f"{'='*60}")

    # Load configuration
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

    # Calculate totals
    total_pnl = sum(p['pnl'] for p in performance.values())
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
    html = f"""<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Rotation Trading Dashboard</title>
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
    </style>
</head>
<body>
    <div class="header">
        <h1>📊 Rotation Trading Dashboard</h1>
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
            # Filter to only the test date range
            if start_date and end_date:
                price_data['date'] = price_data['timestamp'].dt.strftime('%Y-%m-%d')
                # Filter to include both start_date and end_date
                price_data = price_data[(price_data['date'] >= start_date) & (price_data['date'] <= end_date)]
            elif len(symbol_trades) > 0:
                # Fallback: use first trade timestamp
                first_trade_ts = symbol_trades[0].get('timestamp_ms', 0)
                test_date_str = utc_ms_to_et_string(first_trade_ts, fmt='%Y-%m-%d')
                price_data['date'] = price_data['timestamp'].dt.strftime('%Y-%m-%d')
                price_data = price_data[price_data['date'] == test_date_str]
            else:
                # Fallback: use last 390 bars if no trades
                price_data = price_data.tail(390)

            timestamps = price_data['timestamp'].dt.strftime('%Y-%m-%d %H:%M').tolist()
            prices = price_data['close'].tolist()

            # Get entry and exit points (convert UTC to ET) - filter to test date range
            entries = []
            for t in symbol_trades:
                if t.get('action') == 'ENTRY':
                    ts_str = utc_ms_to_et_string(t['timestamp_ms'])
                    trade_date = utc_ms_to_et_string(t['timestamp_ms'], fmt='%Y-%m-%d')
                    # Only include if within date range
                    if start_date and end_date:
                        if start_date <= trade_date <= end_date:
                            entries.append((ts_str, t['price']))
                    else:
                        entries.append((ts_str, t['price']))

            exits = []
            for t in symbol_trades:
                if t.get('action') == 'EXIT':
                    ts_str = utc_ms_to_et_string(t['timestamp_ms'])
                    trade_date = utc_ms_to_et_string(t['timestamp_ms'], fmt='%Y-%m-%d')
                    # Only include if within date range
                    if start_date and end_date:
                        if start_date <= trade_date <= end_date:
                            exits.append((ts_str, t['price']))
                    else:
                        exits.append((ts_str, t['price']))

            html += f"""
    <script>
        var trace1 = {{
            x: {timestamps},
            y: {prices},
            type: 'scatter',
            mode: 'lines',
            name: 'Price',
            line: {{color: '{color}', width: 2}}
        }};

        var trace2 = {{
            x: {[e[0] for e in entries]},
            y: {[e[1] for e in entries]},
            type: 'scatter',
            mode: 'markers',
            name: 'Entry',
            marker: {{color: 'green', size: 12, symbol: 'triangle-up'}}
        }};

        var trace3 = {{
            x: {[e[0] for e in exits]},
            y: {[e[1] for e in exits]},
            type: 'scatter',
            mode: 'markers',
            name: 'Exit',
            marker: {{color: 'red', size: 12, symbol: 'triangle-down'}}
        }};

        var layout = {{
            title: '{symbol} Price & Trades',
            xaxis: {{title: 'Time (ET)'}},
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

    args = parser.parse_args()

    generate_html_dashboard(
        args.trades,
        args.output,
        args.config,
        args.data_dir,
        args.start_equity,
        args.start_date,
        args.end_date
    )

if __name__ == '__main__':
    main()
