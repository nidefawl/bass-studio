import os

def find_header_files_without_pragma_once():
    for root, dirs, files in os.walk('./src'):
        for file in files:
            if file.endswith('.h') or file.endswith('.hpp'):
                with open(os.path.join(root, file)) as f:
                    if not '#pragma once' in f.read():
                        print(os.path.join(root, file))

if __name__ == '__main__':
    find_header_files_without_pragma_once()