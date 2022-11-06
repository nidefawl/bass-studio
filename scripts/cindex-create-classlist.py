#!/usr/bin/env python3
import json
import sys, os
import logging

compilation_database_path = "./build"
source_path = "./src"
outfileClassList = 'classlist.csv'
outfileFuncList = 'funclist.csv'
if sys.platform.startswith('win'):
    pathDll = r"C:\dev\bin\llvmmingw-native-16\bin"
    os.add_dll_directory(pathDll)
    sys.path.insert(0, 'C:/dev/build-llvm/llvm-project/clang/bindings/python')
    import clang.cindex as cindex
    cindex.Config.set_library_path(pathDll)
    compilation_database_path = "C:/Users/Michael/daw/build"
    source_path = "C:/Users/Michael/daw/src"
else:
    sys.path.insert(0, '/data/dev/llvm-project/clang/bindings/python')
    import clang.cindex as cindex
    cindex.Config.set_library_path("/data/dev/llvm-install-nounwinder4/lib")
    
# turn path into absolute path
compilation_database_path = os.path.abspath(compilation_database_path)
source_path = os.path.abspath(source_path)

class ClassDef:
    def __init__(self):
        self.fqn = None
        self.sizeOf = 0
        self.parents = []
        self.publicFields = []
        self.privateFields = []
        self.protectedFields = []
        self.publicMethods = []
        self.privateMethods = []
        self.protectedMethods = []
    
    def dump_all(self):
        logging.info("UmlClass: " + self.fqn)
        logging.info("  sizeOf: " + str(self.sizeOf))
        logging.info("  parents: " + str(self.parents))
        logging.info("  publicFields: " + str(self.publicFields))
        logging.info("  privateFields: " + str(self.privateFields))
        logging.info("  protectedFields: " + str(self.protectedFields))
        logging.info("  publicMethods: " + str(self.publicMethods))
        logging.info("  privateMethods: " + str(self.privateMethods))
        logging.info("  protectedMethods: " + str(self.protectedMethods))

    def __str__(self):
        return "ClassDef: " + self.fqn
    
    def __repr__(self):
        return self.__str__()
    
    def __eq__(self, other):
        return self.fqn == other.fqn
    
    def __ne__(self, other):
        return not self.__eq__(other)
    
    def __hash__(self):
        return hash(self.fqn)



def processClassField(cursor):
    """ Returns the name and the type of the given class field.
    The cursor must be of kind CursorKind.FIELD_DECL"""
    type = None
    fieldChilds = list(cursor.get_children())
    if len(fieldChilds) == 0:  # if there are not cursorchildren, the type is some primitive datatype
        type = cursor.type.spelling
    else:  # if there are cursorchildren, the type is some non-primitive datatype (a class or class template)
        for cc in fieldChilds:
            if cc.kind == cindex.CursorKind.TEMPLATE_REF:
                type = cc.spelling
            elif cc.kind == cindex.CursorKind.TYPE_REF:
                type = cursor.type.spelling
    name = cursor.spelling
    return name, type

def appendMember(c, n, m):
    if not n in c:
        c[n] = []
    c[n].append(m)
def processClassMemberDeclaration(cursor, classDict):
    """ Processes a cursor corresponding to a class member declaration and
    appends the extracted information to the given umlClass """
    if cursor.kind == cindex.CursorKind.CXX_BASE_SPECIFIER:
        for baseClass in cursor.get_children():
            if baseClass.kind == cindex.CursorKind.TEMPLATE_REF:
                appendMember(classDict, "parents", baseClass.spelling)
            elif baseClass.kind == cindex.CursorKind.TYPE_REF:
                appendMember(classDict, "parents", baseClass.type.spelling)
    elif cursor.kind == cindex.CursorKind.FIELD_DECL:  # non static data member
        name, type = processClassField(cursor)
        if name is not None and type is not None:
            # clang < 3.5: needs patched cindex.py to have
            # cindex.AccessSpecifier available:
            # https://gitorious.org/clang-mirror/clang-mirror/commit/e3d4e7c9a45ed9ad4645e4dc9f4d3b4109389cb7
            if cursor.access_specifier == cindex.AccessSpecifier.PUBLIC:
                appendMember(classDict, "publicFields", (name, type))
            elif cursor.access_specifier == cindex.AccessSpecifier.PRIVATE:
                appendMember(classDict, "privateFields", (name, type))
            elif cursor.access_specifier == cindex.AccessSpecifier.PROTECTED:
                appendMember(classDict, "protectedFields", (name, type))
    elif cursor.kind == cindex.CursorKind.CXX_METHOD:
        try:
            returnType, argumentTypes = cursor.type.spelling.split(' ', 1)
            if cursor.access_specifier == cindex.AccessSpecifier.PUBLIC:
                appendMember(classDict, "publicMethods", (returnType, cursor.spelling, argumentTypes))
            elif cursor.access_specifier == cindex.AccessSpecifier.PRIVATE:
                appendMember(classDict, "privateMethods", (returnType, cursor.spelling, argumentTypes))
            elif cursor.access_specifier == cindex.AccessSpecifier.PROTECTED:
                appendMember(classDict, "protectedMethods", (returnType, cursor.spelling, argumentTypes))
        except:
            logging.error("Invalid CXX_METHOD declaration! " + str(cursor.type.spelling))
    elif cursor.kind == cindex.CursorKind.FUNCTION_TEMPLATE:
        returnType, argumentTypes = cursor.type.spelling.split(' ', 1)
        if cursor.access_specifier == cindex.AccessSpecifier.PUBLIC:
            appendMember(classDict, "publicMethods", (returnType, cursor.spelling, argumentTypes))
        elif cursor.access_specifier == cindex.AccessSpecifier.PRIVATE:
            appendMember(classDict, "privateMethods", (returnType, cursor.spelling, argumentTypes))
        elif cursor.access_specifier == cindex.AccessSpecifier.PROTECTED:
            appendMember(classDict, "protectedMethods", (returnType, cursor.spelling, argumentTypes))


def fully_qualified(c):
    if c is None:
        return ''
    elif c.kind == cindex.CursorKind.TRANSLATION_UNIT:
        return ''
    res = fully_qualified(c.semantic_parent)
    if res != '':
        res += '::'
    if c.kind == cindex.CursorKind.NAMESPACE and c.is_anonymous():
        res += '<anonymous namespace>'
    else:
        res += c.spelling
    return res

def processFunction(cursor):
    bstatic = cursor.is_static_method()
    # args = cursor.get_arguments()
    # tokens = cursor.get_tokens()
    return_type = cursor.result_type.spelling
    namespace = fully_qualified(cursor.semantic_parent)
    name = cursor.spelling
    fqn = name
    if namespace != '':
        fqn = namespace + '::' + fqn
    params = []
    for childCursor in cursor.get_children():
        if childCursor.kind == cindex.CursorKind.PARM_DECL:
            params.append({'name': childCursor.spelling, 'type': childCursor.type.spelling})
    funcSig = f'{return_type} {fqn}('
    params = [f'{p["type"]} {p["name"]}' for p in params]
    funcSig += ', '.join(params)
    funcSig += ')'
    return fqn, {
        "name": fqn,
        "namespace": namespace,
        "file": cursor.location.file.name,
        "static": 1 if bstatic else 0,
        "sig": funcSig,
        "return_type": return_type,
        "params": params,
    }
def processClass(cursor):
    td_def = cursor.get_definition()
    if td_def is None:
        return None
    namespace = fully_qualified(cursor.semantic_parent)
    name = cursor.spelling
    fqn = name
    if namespace != '':
        fqn = namespace + '::' + fqn
    td_size = cursor.type.get_size()
    classDict = {
        "name": fqn,
        "namespace": namespace,
        "file": cursor.location.file.name,
        "size": td_size,
    }
    return fqn, classDict

def findDefs(cursor, user_data):
    if (cursor.kind == cindex.CursorKind.CLASS_DECL
            or cursor.kind == cindex.CursorKind.STRUCT_DECL
            or cursor.kind == cindex.CursorKind.CLASS_TEMPLATE):
        ret = processClass(cursor)
        if ret is not None: 
            className, classDef = ret
            for c in cursor.get_children():
                processClassMemberDeclaration(c, classDef)
            dictC = user_data[0]
            if not className in dictC or len(classDef.keys()) > len(dictC[className].keys()):
                dictC[className] = classDef
                # logging.info(classDef)
    if (cursor.kind == cindex.CursorKind.FUNCTION_DECL):
        ret = processFunction(cursor)
        if ret is not None:
            fqn, funcDef = ret
            dictF = user_data[1]
            if not fqn in dictF:
                dictF[fqn] = funcDef
                # logging.info("func", fqn)


def traverseAstSrcPath(cursor, src_path, user_function, user_data):
    for childCursor in cursor.get_children():
        if childCursor.location.file is None:
            continue
        if childCursor.location.file.name.startswith(src_path):
            user_function(childCursor, user_data)
            traverseAstSrcPath(childCursor, src_path, user_function, user_data)
        else:
            continue

if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    index = cindex.Index.create()
    compdb = cindex.CompilationDatabase.fromDirectory(compilation_database_path)
    all_cmds = compdb.getAllCompileCommands()
    total = len(all_cmds)
    logging.info("Database contains " + str(len(all_cmds)) + " source files.")
    dictFunctions = {}
    dictClasses = {}
    dicts = (dictClasses, dictFunctions);
    for idx, cc in enumerate(all_cmds):
        clang_invocation_args = list(cc.arguments)
        trFile = clang_invocation_args[-1:][0]
        # if "/data/dev/daw/src/app/mousecursor.cpp" not in trFile:
        #     continue
        logging.info(f'Parse {idx}/{total}: {clang_invocation_args[-1:]}')
        tu = index.parse(
                None, 
                args=clang_invocation_args, 
                options = 0
                # options=cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD | cindex.TranslationUnit.PARSE_CACHE_COMPLETION_RESULTS
        )
        for diagnostic in tu.diagnostics:
            logging.debug(diagnostic)
        # traverseAst(set_result, tu.cursor, source_path)
        sizeBefore = [len(d) for d in dicts]
        traverseAstSrcPath(tu.cursor, source_path, findDefs, dicts)
        sizeAfter = [len(d) for d in dicts]
        logging.info(f'Added {sizeAfter[0] - sizeBefore[0]} functions and {sizeAfter[1] - sizeBefore[1]} classes')

        dictClassesSorted = []
        for key in dictClasses:
            dictClassesSorted.append(dictClasses[key])
        dictClassesSorted.sort(key=lambda x: x["name"])
        dictFunctionsSorted = []
        for key in dictFunctions:
            dictFunctionsSorted.append(dictFunctions[key])
        dictFunctionsSorted.sort(key=lambda x: x["name"])
        dictsSorted = (dictClassesSorted, dictFunctionsSorted)
        outputNamesJson = ("classes.json", "functions.json")
        outputNamesCsv = ("classes.csv", "functions.csv")
        # replace source_path in file if file starts with it
        zipped = zip(dictsSorted, outputNamesJson, outputNamesCsv);
        for data, jsonName, csvName in zipped:
            for v in data:
                if v["file"].startswith(source_path):
                    newFile = v["file"][len(source_path):]
                    if newFile.startswith('/') or newFile.startswith('\\'):
                        v["file"] = newFile[1:]

            with open(csvName, 'w') as f:
                # funcKeys = [key for key in dictFunctionsSorted[0].keys()]
                numKeys = 4
                vKeys = [key for key in data[0].keys()][0:numKeys]
                f.write(','.join(vKeys) + '\n')
                for v in data:
                    vals = [str(v[key]) if not isinstance(v[key], str) else f'"{str(v[key])}"' for key in vKeys]
                    vals = vals
                    f.write(','.join(vals) + '\n')
            # write the dict as an array to json file "function-list.json"
            with open(jsonName, 'w') as f:
                json.dump(data, f, indent=4)