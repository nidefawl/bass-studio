#!/usr/bin/env python3
import subprocess
import sys, os
from enum import Enum

# os.add_dll_directory(r"C:\dev\bin\llvm-mingw-20240606-ucrt-x86_64\bin")
# sys.path.insert(0, "C:/dev/build-llvm/llvm-project/clang/bindings/python")
from clang import cindex


def tokens_to_str(n):
    return " ".join([t.spelling for t in n])


def fully_qualified(c):
    if c is None:
        return ""
    elif c.kind == cindex.CursorKind.TRANSLATION_UNIT:
        return ""
    else:
        res = fully_qualified(c.semantic_parent)
        if res != "":
            return res + "::" + c.displayname
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
log = open("include-files.txt", "w", encoding="utf-8")
compilation_database_path = "C:/Users/Michael/daw/build"
source_path = "/data/dev/daw/"
compilation_database_path = "/data/dev/daw/build"


def findTypeDef(n, typename):
    if n.kind in [cindex.CursorKind.CLASS_DECL, cindex.CursorKind.STRUCT_DECL]:
        td_def = n.get_definition()
        if td_def is None:
            return
        name = fully_qualified(n)
        if name not in visited:
            visited.add(name)
            td_type = n.type
            td_size = td_type.get_size()
            # tDefT = n.get_tokens()
            # tDefTStr = tokens_to_str(tDefT)
            # print(n.location.file.name, n.kind, name, tDefTStr)
            print(td_size, name)
            log.write(f"{td_size},{name}\n")
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


class IncludeForm(Enum):
    Quoted = 0
    AngleBracket = 1


class IncludeInfo:
    def __init__(self, path, form, file=None):
        self.path = path
        self.form = form
        self.file = file

    def __str__(self):
        open_bracket, close_bracket = ('<', '>') if self.form == IncludeForm.AngleBracket else ('"', '"')
        return f'#include {open_bracket}{self.path}{close_bracket} // {self.file}'

def extract_definition(cursor):
    filename = cursor.location.file.name
    with open(filename, 'r') as fh:
        contents = fh.read()
    return contents[cursor.extent.start.offset: cursor.extent.end.offset]

def try_get_included_file(node):
    try:
        return node.get_included_file()
    except:
        return None

includes_found = set()
rename_locs = []
user_data = {
    "includes_found": includes_found,
    "rename_locs": rename_locs
}
black_list_paths = [
    "src/thirdparty/",
    "src/nanovg/"
]
#only rename these files, and update the referencing includes
process_files = ["benchmark", "src", "test"]
def findIncludeDirective(node, user_data):
    global process_files, black_list_paths
    if node.kind in [cindex.CursorKind.INCLUSION_DIRECTIVE]:
        include_info = IncludeInfo(
                    node.displayname,
                    IncludeForm.AngleBracket if list(node.get_tokens())[-1].spelling == '>' else IncludeForm.Quoted,
                    try_get_included_file(node)
                )
        
        # skip files not starting with source path
        if include_info.file is None:
          return
        if not include_info.file.name.startswith(source_path):
          return
        rel_path = include_info.file.name[len(source_path):]
        first_dir = rel_path.split("/")[0]
        if first_dir not in process_files:
            return
        if any([rel_path.startswith(bl) for bl in black_list_paths]):
            return
        black_list_endings = [ 
            ".hpp",
            "assert_dbg.h",
            "buildinfo.h",
            "CalcKaiserWindow.h",
            "builtin_shaders.h",
            "glheaders.h",
        ]
        if any([rel_path.endswith(bl) for bl in black_list_endings]):
            return
        # we found an include to rename
        search_str = extract_definition(node)
        # replace file ending .h with .hpp
        replace_str = search_str.replace(".h", ".hpp")
        file_from_node = node.location.file.name
        if not file_from_node.startswith(source_path):
            print("ERROR: file not in source path", file_from_node)
            sys.exit(1)
        # sed_command = ['sed', '-i', f"'s/{search_str}/{replace_str}/'", str(file_from_node)];
        # print("sed command: ", sed_command)

        user_data["rename_locs"].append((str(file_from_node), search_str, replace_str))

        file_str = str(include_info.file)
        if file_str not in user_data["includes_found"]:
            user_data["includes_found"].add(file_str)

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

CXTranslationUnit_None = 0x0
CXTranslationUnit_DetailedPreprocessingRecord = 0x01
CXTranslationUnit_Incomplete = 0x02
CXTranslationUnit_PrecompiledPreamble = 0x04
CXTranslationUnit_CacheCompletionResults = 0x08
CXTranslationUnit_ForSerialization = 0x10
CXTranslationUnit_CXXChainedPCH = 0x20
CXTranslationUnit_SkipFunctionBodies = 0x40
CXTranslationUnit_IncludeBriefCommentsInCodeCompletion = 0x80
CXTranslationUnit_CreatePreambleOnFirstParse = 0x100
CXTranslationUnit_KeepGoing = 0x200
CXTranslationUnit_SingleFileParse = 0x400
CXTranslationUnit_LimitSkipFunctionBodiesToPreamble = 0x800
CXTranslationUnit_IncludeAttributedTypes = 0x1000
CXTranslationUnit_VisitImplicitAttributes = 0x2000
CXTranslationUnit_IgnoreNonErrorsFromIncludedFiles = 0x4000
CXTranslationUnit_RetainExcludedConditionalBlocks = 0x8000

default_parser_options = (
    CXTranslationUnit_DetailedPreprocessingRecord |  # needed for preprocessing parsing
    CXTranslationUnit_SkipFunctionBodies |  # for faster parsing
    CXTranslationUnit_SingleFileParse |  # don't parse include files recursively
    CXTranslationUnit_RetainExcludedConditionalBlocks |  # keep includes inside ifdef blocks
    CXTranslationUnit_KeepGoing  # don't stop on errors
)
first_command = None
for idx, cc in enumerate(all_cmds):
    clang_invocation_args = list(cc.arguments)
    if not clang_invocation_args[-1].startswith(source_path):
        continue
    rel_path = clang_invocation_args[-1][len(source_path):]
    first_dir = rel_path.split("/")[0]
    if first_dir not in process_files:
        continue
    if any([rel_path.startswith(bl) for bl in black_list_paths]):
        continue
    if not first_command:
        first_command = clang_invocation_args
    print(f"Parse {idx}/{total}: {clang_invocation_args[-1:]}")
    translation_unit = index.parse(
        None,
        args=clang_invocation_args,
        # options=0
        # options=cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD | cindex.TranslationUnit.PARSE_CACHE_COMPLETION_RESULTS
        options = default_parser_options
    )

    visit_src_path_nodes(translation_unit.cursor, source_path, findIncludeDirective, user_data)

# this is a bit hacky:
# we want to also parse include files. but includes are not listed in the compilation database
# so we craft our own command line arguments
copy_of_includes_found = includes_found.copy()
for include_path in copy_of_includes_found:
    print(f"Parse include {include_path}")
    clang_invocation_args = first_command.copy()
    clang_invocation_args[-1] = include_path
    translation_unit = index.parse(
        None,
        args=clang_invocation_args,
        options = default_parser_options
    )

    visit_src_path_nodes(translation_unit.cursor, source_path, findIncludeDirective, user_data)

APPLY_RENAMES = False

for to_replace in user_data["rename_locs"]:
  file_from_node, search_str, replace_str = to_replace
  print("replace in file ", file_from_node)
  if APPLY_RENAMES:
    with open(file_from_node, 'r') as f:
      contents = f.read()
    new_contents = contents.replace(search_str, replace_str)
    with open(file_from_node, 'w') as f:
      f.write(new_contents)

for include in includes_found:
  log.write(f"{include}\n")
log.close()

  # subprocess.call(sed_command)
# turn set to list then sort and write to log
includes_sorted = list(includes_found)
includes_sorted.sort()
if APPLY_RENAMES:
  # now rename all files with .h to .hpp
  for include in includes_sorted:
    if not include.endswith(".h"):
      continue
    include_hpp = include.replace(".h", ".hpp")
    try:
      os.rename(include, include_hpp)
      print(f"renamed {include} to {include_hpp}")
    except Exception as e:
      # maybe renamed before
      print(f"error renaming {include} to {include_hpp}: {e}")
