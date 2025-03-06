# glsl_to_c_src.py
import os
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
      {
        "PATH": "textured.fsh",
        "VAR_NAME": "TEXTURED_GLSL_FRAG"
      },
      {
        "PATH": "textured.vsh",
        "VAR_NAME": "TEXTURED_GLSL_VERT"
      },
      {
        "PATH": "perfgraph.fsh",
        "VAR_NAME": "PERFGRAPH_GLSL_FRAG"
      },
      {
        "PATH": "dash-lines-2D.fsh",
        "VAR_NAME": "DASH_LINES_2D_GLSL_FRAG"
      },
      {
        "PATH": "dash-lines-2D.vsh",
        "VAR_NAME": "DASH_LINES_2D_GLSL_VERT"
      },
      {
        "PATH": "blit_final.fsh",
        "VAR_NAME": "BLIT_FINAL_GLSL_FRAG"
      },
      {
        "PATH": "blit_texture.fsh",
        "VAR_NAME": "BLIT_TEXTURE_GLSL_FRAG"
      },
      {
        "PATH": "blur.fsh",
        "VAR_NAME": "BLUR_GLSL_FRAG"
      },
      {
        "PATH": "darken.fsh",
        "VAR_NAME": "DARKEN_GLSL_FRAG"
      },
      {
        "PATH": "polyline2d.fsh",
        "VAR_NAME": "POLYLINE2D_GLSL_FRAG"
      },
      {
        "PATH": "polyline2d.vsh",
        "VAR_NAME": "POLYLINE2D_GLSL_VERT"
      },
      {
        "PATH": "textured_fullscreen.vsh",
        "VAR_NAME": "TEXTURED_FULLSCREEN_GLSL_VERT"
      },
      {
        "PATH": "shaders/gpu_synth.glsl",
        "VAR_NAME": "GPUSYNTH_GLSL_COMPUTE"
      }
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
    # check if src is subdirectory of current directory, if not look in parent directory
    path = os.path.join(os.getcwd(), "src")
    if not os.path.isdir(path):
        path = os.path.join(os.path.dirname(os.getcwd()), "src")
    # with open("src/gl/builtin_shaders.c", "wb") as ouput:
    #     ouput.write(outputCSourceFile.encode("UTF-8"))
    pathBuiltinShadersC = os.path.join(path, "gl", "builtin_shaders.c")
    with open(pathBuiltinShadersC, "wb") as ouput:
        ouput.write(outputCSourceFile.encode("UTF-8"))

    # create header file with declarations
    cVarDecls = ['#pragma once\n']
    cVarDecls.append('#ifdef __cplusplus')
    cVarDecls.append('extern "C" {')
    cVarDecls.append('#endif\n')
    for shaderFileEntry in shaderFileList:
        cVarDecls.append(f'extern const char* const {shaderFileEntry["VAR_NAME"]};')
    cVarDecls.append('\n#ifdef __cplusplus')
    cVarDecls.append('}')
    cVarDecls.append('#endif')
    cVarDecls.append('')
    outputCHeaderFile = '\n'.join(cVarDecls)
    pathBuiltinShadersH = os.path.join(path, "gl", "builtin_shaders.h")
    with open(pathBuiltinShadersH, "wb") as ouput:
        ouput.write(outputCHeaderFile.encode("UTF-8"))


if __name__ == "__main__":
    # execute only if run as a script
    main()
