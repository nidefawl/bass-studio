import argparse
import glob
import os
import pathlib
import sys
import re


if __name__ == '__main__':
    fileList = glob.glob("**/*.h", recursive=True)
    fileList.extend(glob.glob("**/*.cpp", recursive=True))
    print(len(fileList), 'files matched', file=sys.stderr)

    for file in fileList:
        # read file line by line
        with open(file, 'r') as f:
            # print (file)
            lineNr = 0
            for line in f:
                lineNr += 1
                # search for log statements
                match = re.search(r'(log_.*\(.+".*\))', line)
                if match and not "\\n" in line and not "log_message" in line:
                    print(f'{file}:{lineNr} -> {line.strip()}')