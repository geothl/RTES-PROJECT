"""
import plotly.graph_objects as go

from datetime import datetime
import pandas as pd

# Load the .txt file with a custom delimiter (e.g., pipe '|')
txt_file = 'C:\project_embedded\csv\data_test.txt'  # Replace with your .txt file path
data = pd.read_csv(txt_file, sep=';')  # Change '|' to your delimiter if different

# Save to a .csv file
csv_file = 'C:\project_embedded\csv\data_test.csv'  # Path to save the .csv file
data.to_csv(csv_file,sep=';', index=False)

print(f"Converted {txt_file} to {csv_file}")

d = pd.read_csv('https://raw.githubusercontent.com/plotly/datasets/master/finance-charts-apple.csv')
df = pd.read_csv('C:\project_embedded\csv\data_test.csv')

fig = go.Figure(data=[go.Candlestick(x=d['Date'],
                open=df['opening price'],
                high=df['max price'],
                low=df['min price'],
                close=df['closing price'])])


fig.show()



df = pd.read_csv('C:\project_embedded\csv\data_test.csv')

fig = go.Figure(data=[go.Candlestick(
                open=df['opening price'],
                high=df['max price'],
                low=df['min price'],
                close=df['closing price'])])

fig.show()
"""

import pandas as pd
import mplfinance as mpf

symbols = ['ethereum', 'tesla', 'amazon', 'apple']
input_paths = [f'C:\\project_embedded\\csv\\{symbol}_candle.txt' for symbol in symbols]
output_paths = [f'C:\\project_embedded\\csv\\{symbol}_candle.txt' for symbol in symbols]


for input_file, output_file in zip(input_paths, output_paths):
    # Open the input file and read its content
    with open(input_file, 'r') as file:
        lines = file.readlines()

    # Process each line to remove the semicolon at the end, if present
    with open(output_file, 'w') as file:
        for line in lines:
            # Strip the semicolon if it exists at the end
            modified_line = line.rstrip().rstrip(';') + '\n'
            # Write the modified line to the output file
            file.write(modified_line)

    print(f"Semicolons removed and saved to {output_file}")

dataframes = []
for symbol, output_file in zip(symbols, output_paths):
    # Load the cleaned .txt file with a custom delimiter (semicolon ';')
    df = pd.read_csv(output_file, sep=';', dtype=str)  # Read as strings initially to avoid format issues
    
    # Rename columns to match the expected format (Open, High, Low, Close, Volume)
    df.columns = ['Open', 'Close', 'High', 'Low', 'Volume']
    
    # Convert the columns to numeric (float) so that mplfinance can plot the data
    df['Open'] = pd.to_numeric(df['Open'], errors='coerce')
    df['Close'] = pd.to_numeric(df['Close'], errors='coerce')
    df['High'] = pd.to_numeric(df['High'], errors='coerce')
    df['Low'] = pd.to_numeric(df['Low'], errors='coerce')
    df['Volume'] = pd.to_numeric(df['Volume'], errors='coerce')
    
    # Create a synthetic date range to serve as the DatetimeIndex
    df['Date'] = pd.date_range(start='2024-10-02 16:59', end='2024-10-04 18:58', freq='T')
    
    # Set 'Date' as the index
    df.set_index('Date', inplace=True)
    
    # Rearrange columns to match the required order: Open, High, Low, Close, Volume
    df = df[['Open', 'High', 'Low', 'Close', 'Volume']]
    
    # Append to the list of dataframes
    dataframes.append(df)

# Step 3: Plot individual candlestick charts
for df, symbol in zip(dataframes, symbols):
    mc = mpf.make_marketcolors(up='green', down='red', edge='black', wick='white', volume='in')
    custom_style = mpf.make_mpf_style(marketcolors=mc)

    # Plot the candlestick chart for each symbol
    mpf.plot(df, type='candle', style='yahoo', volume=True, title=f'{symbol.upper()}',
             ylabel='Price', ylabel_lower='Volume', xrotation=0)


#mpf.plot(df, type='candle', style='yahoo', volume=True, title='COINBASE:ETH-USD',ylabel='Price', ylabel_lower='Volume', xrotation=0)
 