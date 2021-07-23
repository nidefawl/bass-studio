

def toCStyleString(strDataEnc):
        strData = strDataEnc.decode(encoding="UTF-8")
        strCompact = strData.replace("\t", " ").replace("  ", " ")
        #with open("S:\\res\\nanovg-compact.fsh", "wb") as ouput:
        #    ouput.write(strCompact.encode("UTF-8"))
        strCStringDef = "{\""
        strDataHex = "".join(["\\x"+"%02X"%(min(255, max(0, ord(c)))) for c in strCompact])
        strCString = strCStringDef + strDataHex + "\"};"
        return strCString

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
    strCSourceFile = ""
    
    for shaderFileEntry in shaderFileList:
        print("{} = {}".format(shaderFileEntry["PATH"], shaderFileEntry["VAR_NAME"]))
        with open(strDawDataPath+"/"+shaderFileEntry["PATH"], "rb") as input:
            data = input.read()
            strCString = toCStyleString(data)
            strCSourceFile += "const char* "
            strCSourceFile += shaderFileEntry["VAR_NAME"]
            strCSourceFile += " = "
            strCSourceFile += strCString + "\n\n"
        
    with open("nanovg_shaders.c", "wb") as ouput:
        ouput.write(strCSourceFile.encode("UTF-8"))

if __name__ == "__main__":
    # execute only if run as a script
    main()
