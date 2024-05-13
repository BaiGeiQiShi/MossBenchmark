#!/usr/bin/env python3.8
from sys import argv
input_file_path= argv[1]
input_file_path = 'tmp/test.sparse.lcov'
output_file_path = 'filtered_large_file.txt'

'''
with open(input_file_path, 'r', encoding='utf-8') as input_file, open(output_file_path, 'w', encoding='utf-8') as output_file:
    for line in input_file:
        if not line.startswith('FN'):
            output_file.write(line)
'''
with open(input_file_path, 'rb') as input_file, open(output_file_path, 'wb') as output_file:
    output_file.writelines(line for line in input_file if not line.startswith(b'FN'))

