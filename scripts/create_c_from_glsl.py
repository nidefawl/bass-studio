# glsl_to_c_src.py
import bin2c

def main():
    strDawDataPath = "./res"
    shaderFileList = [ 
      {
        "PATH": "nanovg.fsh",
        "VAR_NAME": "NVG_GLSL_FRAG"
      },
      {
        "PATH": "nanovg.vsh",
        "VAR_NAME": "NVG_GLSL_VERT"
      },
    ]

    cVarDefs = ['#include <stdint.h>\n']
    for shaderFileEntry in shaderFileList:
        print("{} = {}".format(strDawDataPath+"/"+shaderFileEntry["PATH"], shaderFileEntry["VAR_NAME"]))
        with open(strDawDataPath+"/"+shaderFileEntry["PATH"], "rb") as input:
            data = input.read()
            if True:
                strCArrayData = bin2c.toU64Array(data)
                strCArrayDef = f'const uint64_t {shaderFileEntry["VAR_NAME"]}_au64[] = {{\n{strCArrayData}\n}};'
                cVarDefs.append(strCArrayDef)
                cVarDefs.append(f'const char* const {shaderFileEntry["VAR_NAME"]} = (const char*) {shaderFileEntry["VAR_NAME"]}_au64;')
            # if True:
            #     strCArrayData = toU32Array(data)
            #     strCArrayDef = f'const uint32_t {shaderFileEntry["VAR_NAME"]}_au32[] = {{\n{strCArrayData}\n}};'
            #     cVarDefs.append(strCArrayDef)
            # if True:
            #     strCArrayData = toU8Array(data)
            #     strCArrayDef = f'const uint8_t {shaderFileEntry["VAR_NAME"]}_au8[] = {{\n{strCArrayData}\n}};'
            #     cVarDefs.append(strCArrayDef)
            # if True:
            #     strCArrayData = toHexString(data)
            #     strCArrayDef = f'const char* const {shaderFileEntry["VAR_NAME"]}_sz = \n"{strCArrayData}";'
            #     cVarDefs.append(strCArrayDef)
    
    cVarDefs.append('\n')
    outputCSourceFile = '\n'.join(cVarDefs)
    with open("nanovg_shaders.c", "wb") as ouput:
        ouput.write(outputCSourceFile.encode("UTF-8"))

if __name__ == "__main__":
    # execute only if run as a script
    main()
