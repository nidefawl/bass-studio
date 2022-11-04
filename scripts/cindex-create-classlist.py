#!/usr/bin/env python3
import sys, os
compilation_database_path = "./build"
source_path = "./src"
outfile = 'classlist.csv'
if sys.platform.startswith('win'):
    os.add_dll_directory(r'C:\dev\bin\llvmmingw-native-16\bin')
    sys.path.insert(0, 'C:/dev/build-llvm/llvm-project/clang/bindings/python')
    compilation_database_path = "C:/Users/Michael/daw/build"
    source_path = "C:/Users/Michael/daw/src"

from clang import cindex

def tokens_to_str(n):
    return ' '.join([t.spelling for t in n])

def fully_qualified(c):
    if c is None:
        return ''
    elif c.kind == cindex.CursorKind.TRANSLATION_UNIT:
        return ''
    else:
        res = fully_qualified(c.semantic_parent)
        if res != '':
            return res + '::' + c.displayname
    return c.spelling


def visit_nodes(node, filename, user_function, user_data):
    for n in node.get_children():
        if n.location.file is None:
            continue
        if n.location.file.name == filename:
            user_function(n, user_data)
            visit_nodes(n, filename, user_function, user_data)


def findConstructorCalls(n, cstrDeclSignature):
    if n.kind != cindex.CursorKind.CALL_EXPR:
        return
    funcDef = n.get_definition()
    if not funcDef:
        return
    if funcDef.kind != cindex.CursorKind.CONSTRUCTOR:
        return
    fqFnName = fully_qualified(funcDef)
    if fqFnName == cstrDeclSignature:
        invocationStr = tokens_to_str(n.get_tokens())
        print(invocationStr)


visited = set()
log = open(outfile, 'w', encoding='utf-8')
def findTypeDef(n, typename):
    if n.kind in [cindex.CursorKind.CLASS_DECL, cindex.CursorKind.STRUCT_DECL]:
        td_def = n.get_definition()
        if td_def is None:
            return
        name = fully_qualified(n)
        if name not in visited:
            visited.add(name)
            td_type = n.type;
            td_size = td_type.get_size()
            # tDefT = n.get_tokens()
            # tDefTStr = tokens_to_str(tDefT)
            # print(n.location.file.name, n.kind, name, tDefTStr)
            print(td_size, name)
            log.write(f'{td_size},{name}\n')
            log.flush()
    # print(n.kind)
    # if funcDef.spelling != 'constant_t': return)
    # args = [a for a in n.get_arguments()]
    # print (args[1].type)
    # argsStr = ', '.join([tokens_to_str(a.get_tokens()) for a in args])
    # print('call constructor', fqFnName)
    # print('argsStr', argsStr)
    # print('invocationStr', invocationStr)
    # print()


def visit_src_path_nodes(node, src_path, user_function, user_data):
    for n in node.get_children():
        if n.location.file is None:
            continue
        if n.location.file.name.startswith(src_path):
            user_function(n, user_data)
            visit_src_path_nodes(n, src_path, user_function, user_data)


index = cindex.Index.create()
compdb = cindex.CompilationDatabase.fromDirectory(compilation_database_path)

# source_file_path = "/home/michael/dev/daw/src/app/guicolors.cpp"
# file_args = compdb.getCompileCommands(source_file_path)
# clang_invocation_args = [arg for arg in file_args[0].arguments][0:]
# translation_unit = index.parse(None, args=clang_invocation_args, options=1 | 8)
# visit_nodes(translation_unit.cursor, source_file_path, findTypeDef, "guibase")

all_cmds = compdb.getAllCompileCommands()

total = len(all_cmds)
for idx, cc in enumerate(all_cmds):
    clang_invocation_args = list(cc.arguments)
    print(f'Parse {idx}/{total}: {clang_invocation_args[-1:]}')
    translation_unit = index.parse(
            None, 
            args=clang_invocation_args, 
            options = 0
            # options=cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD | cindex.TranslationUnit.PARSE_CACHE_COMPLETION_RESULTS
        )
    visit_src_path_nodes(translation_unit.cursor, source_path, findTypeDef, "guibase")
log.close()
