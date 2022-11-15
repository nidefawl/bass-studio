from collections import namedtuple

# Use with https://github.com/nidefawl/cpp-indexer

cpp_index_file = "cpp-index.class.csv"

def getDepthInTree(cpp_class_list, cpp_class):
    """ Calculate depth of hierarchy """
    if len(cpp_class.baseclasses) == 0:
        return 0
    depth = 0
    for baseclass in cpp_class.baseclasses:
        for other_class in cpp_class_list:
            if other_class.name == baseclass:
                depth = max(depth, getDepthInTree(cpp_class_list, other_class))
    return depth + 1

def main():
    first_line = None
    rest_lines = []
    with open(cpp_index_file, "r") as f:
        first_line = f.readline().strip()
        rest_lines = [line.strip() for line in f.readlines()]

    if first_line is None:
        print("Error: first line is None")
        return
    
    column_names = first_line.split(",")
    empty_tuple = namedtuple("cppclass", field_names=column_names)
    cpp_class_list = []
    for lineIdx in rest_lines:
        lineIdx = lineIdx.strip()
        if lineIdx == "":
            continue
        data = lineIdx.split(",")
        data = [d.strip("\"").strip() for d in data]
        baseclass_list = data[2].split(";")
        data[2] = [] if baseclass_list == [""] else baseclass_list
        class_tuple = empty_tuple(*data)
        # print(class_tuple)
        cpp_class_list.append(class_tuple)

    print("Total number of classes: {}".format(len(cpp_class_list)))

    final_classes = []
    for cpp_class in cpp_class_list:
        if len(cpp_class.baseclasses) == 0:
            continue
        # check if any other class derives from cpp_class
        derived = False
        for other_class in cpp_class_list:
            if cpp_class.name in other_class.baseclasses:
                derived = True
                break
        if not derived:
            depth = getDepthInTree(cpp_class_list, cpp_class)
            final_classes.append((cpp_class, depth))

    # sort final_classes by depth
    final_classes = sorted(final_classes, key=lambda x: x[1], reverse=False)
    print("Total number of final classes: {}".format(len(final_classes)))
    editLocsPerFile = {}
    for cpp_class, depth in final_classes:
        # print(cpp_class, "Depth: {}".format(depth))
        # print(cpp_class.baseclasses, cpp_class.name, "Depth: {}".format(depth))
        file = f'/data/dev/daw/src/{cpp_class.file}'
        lineIdx, colIdx, fileOffset = cpp_class.location.split(":")
        lineIdx, colIdx, fileOffset = int(lineIdx), int(colIdx), int(fileOffset)
        # read in that line 
        with open(file, "r") as f:
            listLines = f.readlines()
            if lineIdx > len(listLines):
                print("Error: line number is too high")
                continue
            strLineCurrent = listLines[int(lineIdx) - 1].strip()
            # print("File: {}, Line: {}, Col: {}, Offset: {}".format(file, line, col, offset))
            if " : " in strLineCurrent and not 'final' in strLineCurrent:
                typeDeclSplit = strLineCurrent.split(" : ", 1)
                strLineNew = typeDeclSplit[0] + " final : " + typeDeclSplit[1]
                if not file in editLocsPerFile:
                    editLocsPerFile[file] = []
                editLocsPerFile[file].append((lineIdx, colIdx, fileOffset, strLineNew))
    
    print("Total number of files to edit: {}".format(len(editLocsPerFile)))

    for file, editLocs in editLocsPerFile.items():
        print(file)
        with open(file, "r") as f:
            listLines = f.readlines()
        for lineIdx, colIdx, fileOffset, strLineNew in editLocs:
            if lineIdx > len(listLines):
                print("Error: line number is too high")
                continue
            strLineCurrent = listLines[int(lineIdx) - 1].strip()
            print("-", strLineCurrent)
            print("+", strLineNew)
            listLines[lineIdx - 1] = strLineNew + "\n"
        with open(file, "w") as f:
            f.writelines(listLines)
                

if __name__ == "__main__":
    main()

