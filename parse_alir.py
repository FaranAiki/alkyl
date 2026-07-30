import sys

def parse():
    with open('alir_out.txt', 'r') as f:
        for line in f:
            if 'debug: func=' in line:
                print(line.strip())

parse()
