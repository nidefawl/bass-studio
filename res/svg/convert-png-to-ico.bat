PATH=C:\Program Files\ImageMagick-7.1.0-Q16;%PATH%
@REM SVG:
@REM magick.exe convert -background none -size 512x512 icons\3d.svg -define icon:auto-resize="256,128,96,64,48,32,16" "out\icon.ico"
convert daw_icon.png -define icon:auto-resize="256,128,96,64,48,32,16" "daw.ico"
pause