# this files lists all files in res and matches them against some filters, creates a zip, reads back that zip and creates a C file with the data
import os
import bin2c
import fnmatch

def main():
    resourceFiles = []
    for file in os.listdir("res/icons"):
        # check if regular file
        if os.path.isfile("res/icons/" + file):
            resourceFiles.append(os.path.realpath("res/icons/" + file))
    for file in os.listdir("res/cursors"):
        # check if regular file
        if os.path.isfile("res/cursors/" + file):
            resourceFiles.append(os.path.realpath("res/cursors/" + file))
    for file in os.listdir("res/shaders"):
        # check if regular file
        if os.path.isfile("res/shaders/" + file):
            resourceFiles.append(os.path.realpath("res/shaders/" + file))
    for file in os.listdir("res/fonts/gui"):
        # check if regular file
        if os.path.isfile("res/fonts/gui/" + file):
            resourceFiles.append(os.path.realpath("res/fonts/gui/" + file))
    for file in os.listdir("res"):
        # check if regular file
        if file in ["led.png", "led_glow.png", "led_off.png"]:
            resourceFiles.append(os.path.realpath("res/" + file))
    print("Found", len(resourceFiles), "resource files")
    # print(resourceFiles)
    # create zip file
    import zipfile
    with zipfile.ZipFile("res.zip", "w") as zip:
        for file in resourceFiles:
            # convert to relative file path (remove ../../res/)
            relativePath = os.path.relpath(file, "res")
            # replace backslashes with forward slashes
            relativePath = relativePath.replace("\\", "/")
            zip.write(file, relativePath)
    # read back zip as bytes
    with open("res.zip", "rb") as input:
        data = input.read()
        strCArrayData = bin2c.toU64Array(data)
        strCArrayDef = f'const uint64_t RES_DATA[] = {{\n{strCArrayData}\n}};'
        cVarDefs = []
        cVarDefs.append("#include \"renderresources_zip.hpp\"")
        cVarDefs.append("")
        cVarDefs.append("namespace RenderResources {")
        cVarDefs.append("")
        cVarDefs.append(strCArrayDef)
        cVarDefs.append("""
const std::vector<uint8_t> getResData() {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(RES_DATA);
    return std::vector<uint8_t>(p, p + sizeof(RES_DATA));
}
""")
        cVarDefs.append("} // namespace RenderResources")
        cVarDefs.append("")
        cVarDefs.append('\n')
        outputCSourceFile = '\n'.join(cVarDefs)
        # check if src is subdirectory of current directory, if not look in parent directory
        path = os.path.join(os.getcwd(), "src")
        if not os.path.isdir(path):
            path = os.path.join(os.path.dirname(os.getcwd()), "src")
        # with open("src/gl/builtin_shaders.c", "wb") as ouput:
        #     ouput.write(outputCSourceFile.encode("UTF-8"))
        pathResDataFile = os.path.join(path, "app", "renderresources_zip.cpp")
        with open(pathResDataFile, "wb") as ouput:
            ouput.write(outputCSourceFile.encode("UTF-8"))
        
        # create header
        cVarDefs = []
        cVarDefs.append("#pragma once")
        cVarDefs.append("#include <cstdint>")
        cVarDefs.append("#include <vector>")
        cVarDefs.append("")
        cVarDefs.append("namespace RenderResources {")
        cVarDefs.append("")
        cVarDefs.append("const std::vector<uint8_t> getResData();")
        cVarDefs.append("")
        cVarDefs.append("} // namespace RenderResources")
        cVarDefs.append("")
        outputCSourceFile = '\n'.join(cVarDefs)
        pathResDataFile = os.path.join(path, "app", "renderresources_zip.hpp")
        with open(pathResDataFile, "wb") as ouput:
            ouput.write(outputCSourceFile.encode("UTF-8"))


if __name__ == "__main__":
    # execute only if run as a script
    main()
