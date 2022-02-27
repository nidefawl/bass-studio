

def toU64Array(strDataEnc):
        strDataEnc += b'\x00'   
        while len(strDataEnc) % 8 != 0:
            strDataEnc += b'\x00'
        u32Array = memoryview(strDataEnc).cast('Q')
        outputStr = ''
        for i,u in enumerate(u32Array):
            if not i%6:
                outputStr += '\n';
            outputStr += "0x%016X,"%u
        return outputStr[1:-1]
def toU32Array(strDataEnc):
        strDataEnc += b'\x00'   
        while len(strDataEnc) % 4 != 0:
            strDataEnc += b'\x00'
        u32Array = memoryview(strDataEnc).cast('I')
        return ",".join(["0x%08X"%u for u in u32Array])
def toU8Array(strDataEnc):
        strDataEnc += b'\x00'   
        # while len(strDataEnc) % 4 != 0:
        #     strDataEnc += b'\x00'
        u8Array = memoryview(strDataEnc).cast('B')
        return ",".join(["0x%02X"%u for u in u8Array])
def toHexString(strDataEnc):
        strDataEnc += b'\x00'   
        # while len(strDataEnc) % 4 != 0:
        #     strDataEnc += b'\x00'
        charArray = memoryview(strDataEnc).cast('c')
        return "".join(["\\x%02X" % ord(c) for c in charArray])

def main():
    strDawDataPath = "./res/"
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
        print("{} = {}".format(shaderFileEntry["PATH"], shaderFileEntry["VAR_NAME"]))
        with open(strDawDataPath+"/"+shaderFileEntry["PATH"], "rb") as input:
            data = input.read()
            if True:
                strCArrayData = toU64Array(data)
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
