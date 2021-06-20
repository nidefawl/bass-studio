############################################################################################
#      NSIS Installation Script created by NSIS Quick Setup Script Generator v1.09.18
#               Entirely Edited with NullSoft Scriptable Installation System                
#              by Vlasis K. Barkas aka Red Wine red_wine@freemail.gr Sep 2006               
############################################################################################
Unicode True

!define VERSION "00.04.03.01"
!define APP_VERSION "0.4.3"
!define APP_SHORTNAME "daw"
!define APP_NAME "${APP_SHORTNAME}-${APP_VERSION}"
!define COMP_NAME "Hept"
!define WEB_SITE "tbd"
!define COPYRIGHT "Michael Hept © 2021"
!define DESCRIPTION "DAW"
!define LICENSE_TXT "C:\Users\Michael\daw\run\res\installer\license_en.rtf"
!define INSTALLER_NAME "C:\Users\Michael\daw\${APP_NAME}-setup.exe"
!define MAIN_APP_EXE ${APP_NAME}.exe
!define INSTALL_TYPE "SetShellVarContext current"
!define REG_ROOT "HKCU"
!define REG_APP_PATH "Software\Microsoft\Windows\CurrentVersion\App Paths\${MAIN_APP_EXE}"
!define UNINSTALL_PATH "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"

!define REG_START_MENU "Start Menu Folder"

var SM_Folder

######################################################################

VIProductVersion  "${VERSION}"
VIAddVersionKey "ProductName"  "${APP_NAME}"
VIAddVersionKey "CompanyName"  "${COMP_NAME}"
VIAddVersionKey "LegalCopyright"  "${COPYRIGHT}"
VIAddVersionKey "FileDescription"  "${DESCRIPTION}"
VIAddVersionKey "FileVersion"  "${VERSION}"

######################################################################

SetCompressor ZLIB
Name "${APP_NAME}"
Caption "${APP_NAME}"
OutFile "${INSTALLER_NAME}"
BrandingText "${APP_NAME}"
XPStyle on
InstallDirRegKey "${REG_ROOT}" "${REG_APP_PATH}" ""
InstallDir "$PROGRAMFILES64\${APP_SHORTNAME}\${APP_NAME}"

######################################################################

!include "MUI.nsh"

!define MUI_ABORTWARNING
!define MUI_UNABORTWARNING

!insertmacro MUI_PAGE_WELCOME

!ifdef LICENSE_TXT
!insertmacro MUI_PAGE_LICENSE "${LICENSE_TXT}"
!endif

!insertmacro MUI_PAGE_DIRECTORY

!ifdef REG_START_MENU
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "${APP_SHORTNAME}\${APP_NAME}"
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "${REG_ROOT}"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "${UNINSTALL_PATH}"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "${REG_START_MENU}"
!insertmacro MUI_PAGE_STARTMENU Application $SM_Folder
!endif

!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM

!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

######################################################################

Section -MainProgram
${INSTALL_TYPE}
SetOverwrite ifnewer
SetOutPath "$INSTDIR"
File /oname=${MAIN_APP_EXE} "C:\Users\Michael\daw\run\DAW-MSVC-relwithdebinfo.exe"
File /oname=daw-vstscanner.exe "C:\Users\Michael\daw\run\vstscanner-MSVC-relwithdebinfo.exe"
File "C:\Users\Michael\daw\dist\soxr.dll"
File "C:\Users\Michael\daw\dist\dash-lines-2D.fsh"
File "C:\Users\Michael\daw\dist\dash-lines-2D.vsh"
File "C:\Users\Michael\daw\dist\daw_context_init.js"
File "C:\Users\Michael\daw\dist\daw_dummy.js"
File "C:\Users\Michael\daw\dist\daw_init.js"
File "C:\Users\Michael\daw\dist\polyline2d.fsh"
File "C:\Users\Michael\daw\dist\polyline2d.vsh"
File "C:\Users\Michael\daw\dist\test.fsh"
File "C:\Users\Michael\daw\dist\test.vsh"
File "C:\Users\Michael\daw\dist\textured.fsh"
File "C:\Users\Michael\daw\dist\textured.vsh"
SetOutPath "$INSTDIR\res"
File "C:\Users\Michael\daw\dist\res\cursors.png"
File "C:\Users\Michael\daw\dist\res\led.png"
File "C:\Users\Michael\daw\dist\res\led_glow.png"
File "C:\Users\Michael\daw\dist\res\led_off.png"
SetOutPath "$INSTDIR\res\icons"
File "C:\Users\Michael\daw\run\res\icons\adjust.png"
File "C:\Users\Michael\daw\run\res\icons\arr_down.png"
File "C:\Users\Michael\daw\run\res\icons\arr_left.png"
File "C:\Users\Michael\daw\run\res\icons\arr_right.png"
File "C:\Users\Michael\daw\run\res\icons\arr_up.png"
File "C:\Users\Michael\daw\run\res\icons\automation.png"
File "C:\Users\Michael\daw\run\res\icons\bypass.png"
File "C:\Users\Michael\daw\run\res\icons\close.png"
File "C:\Users\Michael\daw\run\res\icons\copy.png"
File "C:\Users\Michael\daw\run\res\icons\cut.png"
File "C:\Users\Michael\daw\run\res\icons\duplicate.png"
File "C:\Users\Michael\daw\run\res\icons\duplicate2.png"
File "C:\Users\Michael\daw\run\res\icons\effect.png"
File "C:\Users\Michael\daw\run\res\icons\file.png"
File "C:\Users\Michael\daw\run\res\icons\file2.png"
File "C:\Users\Michael\daw\run\res\icons\folder.png"
File "C:\Users\Michael\daw\run\res\icons\folder_open.png"
File "C:\Users\Michael\daw\run\res\icons\loop.png"
File "C:\Users\Michael\daw\run\res\icons\minus.png"
File "C:\Users\Michael\daw\run\res\icons\paste.png"
File "C:\Users\Michael\daw\run\res\icons\plus.png"
File "C:\Users\Michael\daw\run\res\icons\save.png"
File "C:\Users\Michael\daw\run\res\icons\speaker.png"
File "C:\Users\Michael\daw\run\res\icons\synth.png"
File "C:\Users\Michael\daw\run\res\icons\synth_32px.png"
File "C:\Users\Michael\daw\run\res\icons\x.png"
File "C:\Users\Michael\daw\run\res\icons\daw.ico"
File "C:\Users\Michael\daw\run\res\icons\daw_icon.png"
SetOutPath "$INSTDIR\res\fonts"
File "C:\Users\Michael\daw\run\res\fonts\icons.ttf"
SetOutPath "$INSTDIR\res\fonts\gui"
File "C:\Users\Michael\daw\run\res\fonts\gui\JetBrainsMono-Regular.ttf"
File "C:\Users\Michael\daw\run\res\fonts\gui\OpenSans-Regular.ttf"
File "C:\Users\Michael\daw\run\res\fonts\gui\Roboto-Black.ttf"
File "C:\Users\Michael\daw\run\res\fonts\gui\Roboto-Bold.ttf"
File "C:\Users\Michael\daw\run\res\fonts\gui\Roboto-Medium.ttf"
File "C:\Users\Michael\daw\run\res\fonts\gui\Roboto-Regular.ttf"
SetOutPath "$INSTDIR\res\cursors"
File "C:\Users\Michael\daw\run\res\cursors\cursor00.png"
File "C:\Users\Michael\daw\run\res\cursors\cursor01.png"
File "C:\Users\Michael\daw\run\res\cursors\cursor02.png"
File "C:\Users\Michael\daw\run\res\cursors\cursor03.png"
File "C:\Users\Michael\daw\run\res\cursors\cursor04.png"
File "C:\Users\Michael\daw\run\res\cursors\cursor05.png"
File "C:\Users\Michael\daw\run\res\cursors\cursor06.png"
File "C:\Users\Michael\daw\run\res\cursors\cursor07.png"
File "C:\Users\Michael\daw\run\res\cursors\cursor08.png"
File "C:\Users\Michael\daw\run\res\cursors\cursor09.png"
File "C:\Users\Michael\daw\run\res\cursors\cursor10.png"
File "C:\Users\Michael\daw\run\res\cursors\cursor11.png"
File "C:\Users\Michael\daw\run\res\cursors\cursor12.png"
File "C:\Users\Michael\daw\run\res\cursors\cursor13.png"
File "C:\Users\Michael\daw\run\res\cursors\cursor14.png"
File "C:\Users\Michael\daw\run\res\cursors\cursor15.png"
SetOutPath "$APPDATA\daw\data"
File "C:\Users\Michael\daw\run\data\view0.layout"
File "C:\Users\Michael\daw\run\data\view1.layout"
File "C:\Users\Michael\daw\run\data\view2.layout"
File "C:\Users\Michael\daw\run\data\view3.layout"
File "C:\Users\Michael\daw\run\data\view4.layout"
File "C:\Users\Michael\daw\run\data\view5.layout"
File "C:\Users\Michael\daw\run\data\view6.layout"
File "C:\Users\Michael\daw\run\data\view7.layout"
File "C:\Users\Michael\daw\run\data\view8.layout"
File "C:\Users\Michael\daw\run\data\view9.layout"
File "C:\Users\Michael\AppData\Roaming\daw\data\theme.json"

SectionEnd

######################################################################

Section -Icons_Reg
SetOutPath "$INSTDIR"
WriteUninstaller "$INSTDIR\uninstall.exe"

!ifdef REG_START_MENU
!insertmacro MUI_STARTMENU_WRITE_BEGIN Application
#CreateDirectory "$SMPROGRAMS/${APP_SHORTNAME}"
#CreateDirectory "$SMPROGRAMS/${APP_SHORTNAME}/${APP_NAME}"
CreateDirectory "$SMPROGRAMS\$SM_Folder"
CreateShortCut "$SMPROGRAMS\$SM_Folder\${APP_NAME}.lnk" "$INSTDIR\${MAIN_APP_EXE}"
CreateShortCut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${MAIN_APP_EXE}"
CreateShortCut "$SMPROGRAMS\$SM_Folder\Uninstall ${APP_NAME}.lnk" "$INSTDIR\uninstall.exe"

!ifdef WEB_SITE
WriteIniStr "$INSTDIR\${APP_NAME} website.url" "InternetShortcut" "URL" "${WEB_SITE}"
CreateShortCut "$SMPROGRAMS\$SM_Folder\${APP_NAME} Website.lnk" "$INSTDIR\${APP_NAME} website.url"
!endif
!insertmacro MUI_STARTMENU_WRITE_END
!endif


WriteRegStr ${REG_ROOT} "${REG_APP_PATH}" "" "$INSTDIR\${MAIN_APP_EXE}"
WriteRegStr ${REG_ROOT} "${UNINSTALL_PATH}"  "DisplayName" "${APP_NAME}"
WriteRegStr ${REG_ROOT} "${UNINSTALL_PATH}"  "UninstallString" "$INSTDIR\uninstall.exe"
WriteRegStr ${REG_ROOT} "${UNINSTALL_PATH}"  "DisplayIcon" "$INSTDIR\${MAIN_APP_EXE}"
WriteRegStr ${REG_ROOT} "${UNINSTALL_PATH}"  "DisplayVersion" "${VERSION}"
WriteRegStr ${REG_ROOT} "${UNINSTALL_PATH}"  "Publisher" "${COMP_NAME}"

!ifdef WEB_SITE
WriteRegStr ${REG_ROOT} "${UNINSTALL_PATH}"  "URLInfoAbout" "${WEB_SITE}"
!endif
SectionEnd

######################################################################

Section Uninstall
${INSTALL_TYPE}
Delete "$INSTDIR\${MAIN_APP_EXE}"
Delete "$INSTDIR\daw-vstscanner.exe"
Delete "$INSTDIR\soxr.dll"
Delete "$INSTDIR\dash-lines-2D.fsh"
Delete "$INSTDIR\dash-lines-2D.vsh"
Delete "$INSTDIR\daw_context_init.js"
Delete "$INSTDIR\daw_dummy.js"
Delete "$INSTDIR\daw_init.js"
Delete "$INSTDIR\polyline2d.fsh"
Delete "$INSTDIR\polyline2d.vsh"
Delete "$INSTDIR\test.fsh"
Delete "$INSTDIR\test.vsh"
Delete "$INSTDIR\textured.fsh"
Delete "$INSTDIR\textured.vsh"
Delete "$INSTDIR\res\cursors.png"
Delete "$INSTDIR\res\led.png"
Delete "$INSTDIR\res\led_glow.png"
Delete "$INSTDIR\res\led_off.png"
Delete "$INSTDIR\res\icons\adjust.png"
Delete "$INSTDIR\res\icons\arr_down.png"
Delete "$INSTDIR\res\icons\arr_left.png"
Delete "$INSTDIR\res\icons\arr_right.png"
Delete "$INSTDIR\res\icons\arr_up.png"
Delete "$INSTDIR\res\icons\automation.png"
Delete "$INSTDIR\res\icons\bypass.png"
Delete "$INSTDIR\res\icons\close.png"
Delete "$INSTDIR\res\icons\copy.png"
Delete "$INSTDIR\res\icons\cut.png"
Delete "$INSTDIR\res\icons\duplicate.png"
Delete "$INSTDIR\res\icons\duplicate2.png"
Delete "$INSTDIR\res\icons\effect.png"
Delete "$INSTDIR\res\icons\file.png"
Delete "$INSTDIR\res\icons\file2.png"
Delete "$INSTDIR\res\icons\folder.png"
Delete "$INSTDIR\res\icons\folder_open.png"
Delete "$INSTDIR\res\icons\loop.png"
Delete "$INSTDIR\res\icons\minus.png"
Delete "$INSTDIR\res\icons\paste.png"
Delete "$INSTDIR\res\icons\plus.png"
Delete "$INSTDIR\res\icons\save.png"
Delete "$INSTDIR\res\icons\speaker.png"
Delete "$INSTDIR\res\icons\synth.png"
Delete "$INSTDIR\res\icons\synth_32px.png"
Delete "$INSTDIR\res\icons\x.png"
Delete "$INSTDIR\res\icons\daw_icon.png"
Delete "$INSTDIR\res\icons\daw.ico"
Delete "$INSTDIR\res\fonts\icons.ttf"
Delete "$INSTDIR\res\fonts\gui\JetBrainsMono-Regular.ttf"
Delete "$INSTDIR\res\fonts\gui\OpenSans-Regular.ttf"
Delete "$INSTDIR\res\fonts\gui\Roboto-Black.ttf"
Delete "$INSTDIR\res\fonts\gui\Roboto-Bold.ttf"
Delete "$INSTDIR\res\fonts\gui\Roboto-Medium.ttf"
Delete "$INSTDIR\res\fonts\gui\Roboto-Regular.ttf"
Delete "$INSTDIR\res\cursors\cursor00.png"
Delete "$INSTDIR\res\cursors\cursor01.png"
Delete "$INSTDIR\res\cursors\cursor02.png"
Delete "$INSTDIR\res\cursors\cursor03.png"
Delete "$INSTDIR\res\cursors\cursor04.png"
Delete "$INSTDIR\res\cursors\cursor05.png"
Delete "$INSTDIR\res\cursors\cursor06.png"
Delete "$INSTDIR\res\cursors\cursor07.png"
Delete "$INSTDIR\res\cursors\cursor08.png"
Delete "$INSTDIR\res\cursors\cursor09.png"
Delete "$INSTDIR\res\cursors\cursor10.png"
Delete "$INSTDIR\res\cursors\cursor11.png"
Delete "$INSTDIR\res\cursors\cursor12.png"
Delete "$INSTDIR\res\cursors\cursor13.png"
Delete "$INSTDIR\res\cursors\cursor14.png"
Delete "$INSTDIR\res\cursors\cursor15.png"
 
RmDir "$INSTDIR\res\cursors"
RmDir "$INSTDIR\res\fonts\gui"
RmDir "$INSTDIR\res\fonts"
RmDir "$INSTDIR\res\icons"
RmDir "$INSTDIR\res"
 
Delete "$INSTDIR\uninstall.exe"
!ifdef WEB_SITE
Delete "$INSTDIR\${APP_NAME} website.url"
!endif

RmDir "$INSTDIR"

!ifdef REG_START_MENU
!insertmacro MUI_STARTMENU_GETFOLDER "Application" $SM_Folder
Delete "$SMPROGRAMS\$SM_Folder\${APP_NAME}.lnk"
Delete "$SMPROGRAMS\$SM_Folder\Uninstall ${APP_NAME}.lnk"
!ifdef WEB_SITE
Delete "$SMPROGRAMS\$SM_Folder\${APP_NAME} Website.lnk"
!endif
Delete "$DESKTOP\${APP_NAME}.lnk"

RmDir "$SMPROGRAMS\$SM_Folder"
!endif

!ifndef REG_START_MENU
Delete "$SMPROGRAMS\$APP_NAME\${APP_NAME}.lnk"
Delete "$SMPROGRAMS\$APP_NAME\Uninstall ${APP_NAME}.lnk"
!ifdef WEB_SITE
Delete "$SMPROGRAMS\$APP_NAME\${APP_NAME} Website.lnk"
!endif
Delete "$DESKTOP\${APP_NAME}.lnk"

RmDir "$SMPROGRAMS\"${APP_NAME}""
!endif

DeleteRegKey ${REG_ROOT} "${REG_APP_PATH}"
DeleteRegKey ${REG_ROOT} "${UNINSTALL_PATH}"
SectionEnd

######################################################################

