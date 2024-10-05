import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

"""
# Step 1: Read the CSV data from a .txt file
# Replace 'your_file.txt' with the path to your .txt file

txt_file = 'C:\\project_embedded\\csv\\ethereum.txt'  
data = pd.read_csv(txt_file, sep=';')  

csv_file = 'C:\project_embedded\\csv\\ethereum.csv' 
data.to_csv(csv_file,sep=';', index=False)

print(f"Converted {txt_file} to {csv_file}")
df = pd.read_csv('C:\\project_embedded\\csv\\ethereum.csv',sep=';')  
timestamp_trade=df.iloc[:, 1]
timestamp_receive=df.iloc[:, 2]
drf=(timestamp_receive-timestamp_trade)
print(drf)

plt.plot(drf)  # 'o' to mark points on the line

# Add labels and title
plt.xlabel('Index')
plt.ylabel('')
plt.title('Ethereum')

# Show the grid
plt.grid()

# Display the plot
plt.show()



# Define the list of file paths for each symbol
symbols = ['ethereum', 'tesla', 'amazon', 'apple']
files = [f'C:\\project_embedded\\csv\\{symbol}.txt' for symbol in symbols]  # Assuming the file names follow this pattern

# Create a figure with subplots
fig1, axs1 = plt.subplots(2, 2, figsize=(8, 8)) 
axs1 = axs1.flatten()  # Flatten to easily iterate through

fig2, axs2 = plt.subplots(2, 2, figsize=(8, 8)) 
axs2 = axs2.flatten()  # Flatten to easily iterate through
"""
# Loop through each symbol and process the data
for i, (symbol, file) in enumerate(zip(symbols, files)):
    # Step 1: Read the CSV data from the .txt file
    data = pd.read_csv(file, sep=';')
    
    # Convert timestamp columns to datetime if necessary (adjust column indices as needed)
    timestamp_trade = data.iloc[:, 0] # Adjust column index if needed
    timestamp_receive = data.iloc[:, 1] # Adjust column index if needed
    
    #print(pd.to_datetime(data.iloc[:, 0]) )
    # Step 2: Calculate the difference
    drf = timestamp_receive - timestamp_trade
    
    # Step 3: Plot the data
    axs1[i].plot(drf)
    axs1[i].set_title(f'{symbol.capitalize()} Finnhub-Reception Delay')
    axs1[i].set_xlabel('Number of Trades')
    axs1[i].set_ylabel('Delay Finnhub-R in milliseconds (ms)')
    axs1[i].grid()

# Adjust layout for better spacing
plt.tight_layout()

# Display the plot
plt.show()



# Loop through each symbol and process the data
for i, (symbol, file) in enumerate(zip(symbols, files)):
    # Step 1: Read the CSV data from the .txt file
    data = pd.read_csv(file, sep=';')
    
    # Convert timestamp columns to datetime if necessary (adjust column indices as needed)
    timestamp_receive = data.iloc[:, 1]  # Adjust column index if needed
    timestamp_stored = data.iloc[:, 2] # Adjust column index if needed
    
    # Step 2: Calculate the difference
    drf = timestamp_stored - timestamp_receive
    
    # Step 3: Plot the data
    axs2[i].plot(drf)
    axs2[i].set_title(f'{symbol.capitalize()} Reception-Store Delay')
    axs2[i].set_xlabel('Number of Trades')
    axs2[i].set_ylabel('Delay R-S in milliseconds (ms)')
    axs2[i].grid()

# Adjust layout for better spacing
plt.tight_layout()

# Display the plot
plt.show()
