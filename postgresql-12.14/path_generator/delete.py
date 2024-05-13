#!/usr/bin/env python3.8
from sys import argv
import os
input_file_path= argv[1]
output_file_path = argv[2]

# BUFFER_SIZE=65536
'''
with open(input_file_path, 'rb') as input_file, open(output_file_path, 'wb') as output_file:
    while True:
        data = input_file.read(BUFFER_SIZE)
        if not data:
            break
        filtered_data = b''.join(line for line in data.splitlines(keepends=True) if not line.startswith(b'FN'))
        output_file.write(filtered_data)
'''


'''
os.system(f"sed -e '/^FN/d' {input_file_path} > {output_file_path}")
'''

with open(input_file_path, 'rb') as input_file, open(output_file_path, 'wb') as output_file:
    output_file.writelines(line for line in input_file if not line.startswith(b'FN'))


