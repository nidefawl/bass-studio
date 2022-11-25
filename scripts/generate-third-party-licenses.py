from collections import namedtuple
import glob
import os
import subprocess
import sys

def string_to_valid_href_anchor(string):
    return string.lower().replace(' ', '').replace('-', '').replace('_', '')

def run_subprocess(command_line):
    proc = subprocess.run(command_line, env=os.environ, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    if proc.returncode != 0:
        print('stderr', proc.stderr)
        raise Exception('Command failed', proc.returncode, command_line)
    output = proc.stdout.rstrip()
    return output

def file_is_in_directory(file_path, directory_path):
    file_path = os.path.realpath(file_path)
    while True:
        file_path, tail = os.path.split(file_path)
        if not tail:
            return False
        if file_path == directory_path:
            return True
    

def main(DAW_DEPS_PATH, DAW_SRC_PATH):
    # check if DAW_DEPS_PATH / DAW_SRC_PATH is an existing git repository
    if not os.path.exists(os.path.join(DAW_DEPS_PATH, '.git')):
        raise Exception('DAW_DEPS_PATH is not a git repository')
    if not os.path.exists(os.path.join(DAW_SRC_PATH, '.git')):
        raise Exception('DAW_SRC_PATH is not a git repository')
    # make paths absolute
    DAW_DEPS_PATH = os.path.abspath(DAW_DEPS_PATH)
    DAW_SRC_PATH = os.path.abspath(DAW_SRC_PATH)


    files = []
    # find all files with iname 'LICEN*', 'licen*' or 'COPYING' in DAW_DEPS_PATH
    for d in [f'{DAW_SRC_PATH}/src', DAW_DEPS_PATH]:
        files += glob.glob(d + '/**/LICEN*', recursive=True)
        files += glob.glob(d + '/**/licen*', recursive=True)
        files += glob.glob(d + '/**/COPYING', recursive=True)

    empty_tuple = namedtuple("liblic", field_names=[
        'name',
        'url',
        'libs',
    ])

    # Ignore parts that are not linked into the final binary which is distributed
    IGNORE_FILES = [
        'soxr/lsr-tests/COPYING', 
        'google-benchmark/LICENSE',
        'test/cutest/license.txt',
    ]
    IGNORE_PATH_PATTERNS = [
        'zlib/contrib/',
        'portaudio/bindings/',
        '*/test/*',
        '*/tests/*',
    ]
    FILE_TO_PROJECT_NAME = {
        'cereal/include/cereal/external/LICENSE': 'LICENSE cpp-base64',
        'cereal/include/cereal/external/rapidjson/LICENSE': 'LICENSE rapidjson',
        'cereal/include/cereal/external/rapidxml/license.txt': 'LICENSE rapidxml',
        'cereal/include/cereal/external/rapidjson/msinttypes/LICENSE': 'LICENSE msinttypes',
    }

    URL_REMAP = {
        'platform/linux/nfd/LICENSE': 'https://github.com/btzy/nativefiledialog-extended',
        'thirdparty/cereal_optional_nvp/LICENSE': 'https://github.com/Enhex/Cereal-Optional-NVP',
        'thirdparty/par/LICENSE': 'https://github.com/prideout/par',
        'thirdparty/polyline/LICENSE.md': 'https://github.com/CrushedPixel/Polyline2D',
        'thirdparty/readerwriterqueue/LICENSE.md': 'https://github.com/cameron314/readerwriterqueue',
        'thirdparty/stb/LICENSE': 'https://github.com/nothings/stb',
    }

    lib_lic_list = []
    for file in files:
        if os.path.isdir(file):
            continue
        file_is_ignored = False
        # TODO: don't check patterns against absolute path
        for pattern in IGNORE_PATH_PATTERNS:
            if pattern in file:
                print('Ignoring file', file)
                file_is_ignored = True
                break
        if file_is_ignored:
            continue
        for ignore_file in IGNORE_FILES:
            if file.endswith(ignore_file):
                print('Ignoring file', file)
                file_is_ignored = True
                break
        if file_is_ignored:
            continue
        
        repo_name = file.split('/')[-2]
        parent_dir = os.path.dirname(file)

        repo_url = None
        
        for n, u in URL_REMAP.items():
            if file.endswith(n):
                repo_url = u
                break

        if repo_url is None:
            repo_url = run_subprocess(['git', '-C', parent_dir, 'remote', 'get-url', 'origin'])
        # repo_filename = run_subprocess(['git', '-C', parent_dir, 'ls-files', '--full-name', file])

        license_title = "LICENSE"

        for key, value in FILE_TO_PROJECT_NAME.items():
            if file.endswith(key):
                license_title = value
                break
        
        # if repo url starts with  git@github.com: then convert to https
        if repo_url.startswith('git@github.com:'):
            repo_url = 'https://github.com/' + repo_url.split(':')[1]
        repo_url = repo_url.rstrip('.git')
        
        found = False
        if not file_is_in_directory(file, DAW_SRC_PATH):
            for liblic in lib_lic_list:
                if liblic.url == repo_url:
                    liblic.libs.append((file, license_title))
                    found = True
                    break
        
        if not found:
            if repo_name == 'src':
                repo_name = 'DAW'
            lib_lic_list.append(empty_tuple(
                name=repo_name,
                url=repo_url,
                libs=[(file, license_title)]
            ))

    # sort by name (case insensitive)
    lib_lic_list.sort(key=lambda x: x.name.lower())

    # create markdown
    with open('licenses-thirdparty.md', 'w') as f:
        f.write('This file contains the licenses of all third party libraries used by DAW.\n')
        for liblic in lib_lic_list:
            f.write(f'- [{liblic.name}](#lib-{string_to_valid_href_anchor(liblic.name)})\n')
        f.write('\n')
        for liblic in lib_lic_list:
            f.write(f'<a id="lib-{string_to_valid_href_anchor(liblic.name)}"></a>  \n')
            f.write(f'## {liblic.name}  \n\n')
            f.write(f'[{liblic.url}]({liblic.url})\n')
            for path_licence_abs, path_licence_repo in liblic.libs:
                with open(path_licence_abs, 'r') as f2:
                    f.write('### ' + path_licence_repo + '  \n')
                    f.write('\n```\n')
                    f.write(f2.read())
                    f.write('\n```\n\n')

    # convert markdown to html using python grip
    run_subprocess(['grip', '--export', 'licenses-thirdparty.md', 'licenses-thirdparty.html'])
    # convert to pdf using chrome print to pdf
    run_subprocess(['chromium', '--headless', '--print-to-pdf-no-header', '--print-to-pdf=dist/docs/licenses-thirdparty.pdf', 'licenses-thirdparty.html'])


if __name__ == '__main__':
    path = sys.argv[1:3] if len(sys.argv) > 1 else ['/data/dev/daw-deps', '/data/dev/daw']
    main(*path)