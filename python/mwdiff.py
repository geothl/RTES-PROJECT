import pandas as pd
import mplfinance as mpf

input_file = ['C:\\project_embedded\\csv\\ethereum_period.txt' ]
output_file = ['C:\\project_embedded\\csv\\ethereum_period.txt']


with open(input_file, 'r') as file:
        lines = file.readlines()

    # Process each line to remove the semicolon at the end, if present
with open(output_file, 'w') as file:
    for line in lines:
        # Strip the semicolon if it exists at the end
        modified_line = line.rstrip().rstrip(';') + '\n'
        # Write the modified line to the output file
        file.write(modified_line)

print("Semicolons removed and saved to {output_file}")

