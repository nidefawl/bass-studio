
import sys, re
from pprint import pprint as pp

linker_map = 'build/llvm-mingw-test/DAW_linker.map'
sections = {}
def readHeader(line):
  ln_addr = int(line[:8], base=16)
  ln_size = int(line[9:17], base=16)
  ln_align = int(line[18:23].strip())
  return {
    'size': ln_size,
    'address': ln_addr,
    'align': ln_align
  }
with open(linker_map, 'r') as f_in:
  header = f_in.readline()
  section_name = None
  symbol_name = None
  for line in f_in:
    data = readHeader(line)
    if line[24] != ' ':
      section_name = line[24:].rstrip()
      sections[section_name] = data
      sections[section_name]['symbols'] = {}
    if len(line) >= 32 and line[32] != ' ':
      symbol_name = line[32:].rstrip()
      data.update({'name': symbol_name})
      sections[section_name]['symbols'][symbol_name] = data
    # if len(line) >= 40 and line[40] != ' ':
    #   symbol_name = line[40:].rstrip()
    #   sections[section_name]['modules'][module_name]['symbols'][symbol_name] = data
    
print('%-24s%9s%9s' % ('Section', 'Size', 'Symbols'))

for s, d in sections.items():
  syms = d['symbols']
  num_symbols = len(syms)
  print('%-24s%9d%9d' % (s, d['size'], num_symbols))
  top = sorted(syms.values(), key=lambda sym: sym['size'], reverse=True)
  if not 'debug' in s:
    for i in range(min(len(top), 32)):
      print(top[i])
  # for m_n, m_d in d['symbols'].items():
  #   print(m_n, m_d['size'])
  # match = re.findall(r'\S+\s\s*', header)
  # re.compile('\S+')
  # if match:
  #   for line in f_in:
  #     entry = []
  #     for c in cols:
  #       entry.append(line[:c])
  #       line = line[c:]
  #       if not len(line):
  #         break
  #     if len(line):
  #       entry.append(line)
  #       break
  #     pp(entry)
  # header.split
  # for line in file:
