import json
from pprint import pprint


themePath = '/home/michael/daw/data/theme.json'

with open(themePath, 'r') as infile:
    themeData = json.load(infile)
    themesDefined = themeData['value0']['themes']

    themeNidefawl = [x for x in themesDefined if x['name']=='User 1'][0]
    themeColors = themeNidefawl['data']['colors']
    themeConstants = themeNidefawl['data']['properties']
    themeFonts = themeNidefawl['data']['fonts']
    colorDefs = {entry['key']: entry['value']&0xFFFFFFFF for entry in themeColors}
    colorDefNamesSorted = [entry['key'] for entry in themeColors]
    colorDefNamesSorted.sort()
    constantsDefs = {entry['key']: entry['value']&0xFFFFFFFF for entry in themeConstants}
    constantsDefNamesSorted = [entry['key'] for entry in themeConstants]
    constantsDefNamesSorted.sort()
        
# for k, v in colorDefs.items():
#     print(k, '%08x'%v)
with open('color-defs.cpp', 'w') as outfile:
    for key in colorDefNamesSorted:
        colorVal = colorDefs[key]
        colDefCpp = 'constant_t %s("%s", 0x%08x);\n'%(key,key,colorVal);
        outfile.write(colDefCpp)
with open('constant-defs.cpp', 'w') as outfile:
    for key in constantsDefNamesSorted:
        constantval = constantsDefs[key]
        constantDefCpp = 'constant_t %s("%s", %d);\n'%(key,key,constantval);
        outfile.write(constantDefCpp)