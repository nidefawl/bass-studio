import os
import pathlib
import subprocess

__pathScript = pathlib.Path(__file__).parent.resolve()

shellEnv=os.environ

magickPath = f'C:\Program Files\ImageMagick-7.1.0-Q16'

shellEnv['PATH'] = magickPath + ';' + shellEnv['PATH']

if __name__ == '__main__':
    icons512Path = pathlib.Path(__pathScript.parent.joinpath('icons-512')).resolve()
    icons64Path = pathlib.Path(__pathScript.parent.joinpath('icons')).resolve()
    fileList = icons512Path.glob('*.png')


    commandConvertTo64Px = [
        'magick', 'mogrify', 
        '-background', 'none', 
        '-path', f'{icons64Path}',
        '-filter', 'cubic', 
        '-define', 'png:color-type=6', 
        '-define', 'png:compression-level=9', 
        '-resize', '64x64', 
        f'{icons512Path}\\*.png'
    ]
    proc = subprocess.run(commandConvertTo64Px, cwd=str(__pathScript), env=shellEnv, capture_output=True)
