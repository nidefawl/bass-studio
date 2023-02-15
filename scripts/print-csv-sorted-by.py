import os
import sys

def print_csv_sorted_by(filename, column, reverse=False, numeric=False):
    """Print a CSV file sorted by a column.

    Args:
        filename (str): The name of the CSV file.
        column (int): The column to sort by.
        reverse (bool): Whether to sort in reverse order.
    """
    columns = []
    with open(filename, 'r') as f:
        first_line = f.readline().strip()
        columns = first_line.split(',')
        lines = f.readlines()
    idx = 0
    if column in columns:
        # get idx of str column in columns
        idx = columns.index(column)
    # sort lines by column
    if not numeric:
        lines.sort(key=lambda x: x.split(',')[idx], reverse=reverse)
    else:
        lines.sort(key=lambda x: float(x.split(',')[idx]), reverse=reverse)
    # print first line
    print(first_line, end='')
    # print lines
    for line in lines:
        print(line, end='')
    
if __name__ == '__main__':
    print_csv_sorted_by(
        filename='/data/dev/cpp-indexer-libclang/cpp-index.class.csv',
        column='size',
        reverse=True,
        numeric=True
    )