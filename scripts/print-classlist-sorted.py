

with open('classlist.csv', 'r', encoding='utf-8') as f:
    lines = f.readlines()
    lines.sort(key=lambda x: int(x.split(',')[0]))
    for line in lines:
        print(line, end='')