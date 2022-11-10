######################################################################
# DAW installer script
######################################################################

Unicode True
!ifndef PROJ_LOC
!error "PROJ_LOC not defined"
!endif
!ifndef DIST_LOC
!error "DIST_LOC not defined"
!endif
!ifndef PROJECT_BINARY_PATH
!error "PROJECT_BINARY_PATH not defined"
!endif
!ifndef PRODUCT_VERSION
!error "PRODUCT_VERSION not defined"
!endif
!ifndef PRODUCT_NAME_DISPLAY
!error "PRODUCT_NAME_DISPLAY not defined"
!endif
!ifndef PROJECT_BINARY_NAME
!error "PROJECT_BINARY_NAME not defined"
!endif
!ifndef PROJECT_VENDOR_NAME
!error "PROJECT_VENDOR_NAME not defined"
!endif
!ifndef PRODUCT_URL_VENDOR
!error "PRODUCT_URL_VENDOR not defined"
!endif
!ifndef PRODUCT_COPYRIGHT
!error "PRODUCT_COPYRIGHT not defined"
!endif

!define LICENSE_TXT "${PROJ_LOC}\installer\license_en.rtf"
!define INSTALLER_NAME "/mnt/srv-private\${PROJECT_BINARY_NAME}-v${PRODUCT_VERSION}-setup.exe"
!define MAIN_APP_EXE ${PROJECT_BINARY_NAME}-${PRODUCT_VERSION}.exe
!define INSTALL_TYPE "SetShellVarContext current"
!define REG_ROOT "HKCU"
!define REG_APP_PATH "Software\Microsoft\Windows\CurrentVersion\App Paths\${PROJECT_BINARY_NAME}\${PROJECT_BINARY_NAME}-${PRODUCT_VERSION}"
!define UNINSTALL_PATH "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PROJECT_BINARY_NAME}\${PROJECT_BINARY_NAME}-${PRODUCT_VERSION}"
!define REG_START_MENU "Start Menu Folder"
!define STARTMENU_LINK_NAME "${PRODUCT_NAME_DISPLAY} ${PRODUCT_VERSION}"

var SM_Folder

######################################################################

VIProductVersion "${PRODUCT_VERSION}"
VIAddVersionKey "ProductName"  "${PRODUCT_NAME_DISPLAY}"
VIAddVersionKey "CompanyName"  "${PROJECT_VENDOR_NAME}"
VIAddVersionKey "LegalCopyright"  "${PRODUCT_COPYRIGHT}"
VIAddVersionKey "FileDescription"  "${PROJECT_PRODUCT_NAME} ${PRODUCT_VERSION}"
VIAddVersionKey "FileVersion"  "${PRODUCT_VERSION}"

######################################################################

SetCompressor ZLIB
Name "${PRODUCT_NAME_DISPLAY}"
Caption "${PRODUCT_NAME_DISPLAY}"
OutFile "${INSTALLER_NAME}"
BrandingText "${PRODUCT_NAME_DISPLAY}"
XPStyle on
InstallDirRegKey "${REG_ROOT}" "${REG_APP_PATH}" ""
InstallDir "$PROGRAMFILES64\${PROJECT_BINARY_NAME}\${PROJECT_BINARY_NAME}-${PRODUCT_VERSION}"

######################################################################

!include "MUI.nsh"

!define MUI_ABORTWARNING
!define MUI_UNABORTWARNING

!insertmacro MUI_PAGE_WELCOME

!insertmacro MUI_PAGE_LICENSE "${LICENSE_TXT}"

!insertmacro MUI_PAGE_DIRECTORY

!ifdef REG_START_MENU
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "${PRODUCT_NAME_DISPLAY}\${PRODUCT_NAME_DISPLAY}-${PRODUCT_VERSION}"
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
File /oname=${MAIN_APP_EXE} "${PROJECT_BINARY_PATH}"
File /oname=daw-pluginscanner.exe "${DIST_LOC}\pluginscanner.exe"
File "${DIST_LOC}\libsoxr-clang-release.dll"
File "${DIST_LOC}\libc++.dll"
SetOutPath "$INSTDIR\res"
File "${PROJ_LOC}\res\led.png"
File "${PROJ_LOC}\res\led_glow.png"
File "${PROJ_LOC}\res\led_off.png"
SetOutPath "$INSTDIR\res\icons"
File "${PROJ_LOC}\res\icons\adjust.png"
File "${PROJ_LOC}\res\icons\arr_down.png"
File "${PROJ_LOC}\res\icons\arr_left.png"
File "${PROJ_LOC}\res\icons\arr_right.png"
File "${PROJ_LOC}\res\icons\arr_up.png"
File "${PROJ_LOC}\res\icons\automation.png"
File "${PROJ_LOC}\res\icons\bypass.png"
File "${PROJ_LOC}\res\icons\close.png"
File "${PROJ_LOC}\res\icons\copy.png"
File "${PROJ_LOC}\res\icons\cut.png"
File "${PROJ_LOC}\res\icons\daw_icon.png"
File "${PROJ_LOC}\res\icons\duplicate.png"
File "${PROJ_LOC}\res\icons\duplicate2.png"
File "${PROJ_LOC}\res\icons\effect.png"
File "${PROJ_LOC}\res\icons\file.png"
File "${PROJ_LOC}\res\icons\file2.png"
File "${PROJ_LOC}\res\icons\folder.png"
File "${PROJ_LOC}\res\icons\folder_open.png"
File "${PROJ_LOC}\res\icons\loop.png"
File "${PROJ_LOC}\res\icons\midiplug.png"
File "${PROJ_LOC}\res\icons\minus.png"
File "${PROJ_LOC}\res\icons\modulation.png"
File "${PROJ_LOC}\res\icons\opt_locked.png"
File "${PROJ_LOC}\res\icons\opt_unlocked.png"
File "${PROJ_LOC}\res\icons\paste.png"
File "${PROJ_LOC}\res\icons\plus.png"
File "${PROJ_LOC}\res\icons\save.png"
File "${PROJ_LOC}\res\icons\speaker.png"
File "${PROJ_LOC}\res\icons\synth.png"
File "${PROJ_LOC}\res\icons\synth_small.png"
File "${PROJ_LOC}\res\icons\warning.png"
File "${PROJ_LOC}\res\icons\x.png"
File "${PROJ_LOC}\res\icons\daw.ico"
SetOutPath "$INSTDIR\res\fonts"
File "${PROJ_LOC}\res\fonts\icons.ttf"
SetOutPath "$INSTDIR\res\fonts\gui"
File "${PROJ_LOC}\res\fonts\gui\OpenSans-Regular.ttf"
File "${PROJ_LOC}\res\fonts\gui\Roboto-Black.ttf"
File "${PROJ_LOC}\res\fonts\gui\Roboto-Bold.ttf"
File "${PROJ_LOC}\res\fonts\gui\Roboto-Medium.ttf"
File "${PROJ_LOC}\res\fonts\gui\Roboto-Regular.ttf"
SetOutPath "$INSTDIR\res\cursors"
File "${PROJ_LOC}\res\cursors\cursor00.png"
File "${PROJ_LOC}\res\cursors\cursor01.png"
File "${PROJ_LOC}\res\cursors\cursor02.png"
File "${PROJ_LOC}\res\cursors\cursor03.png"
File "${PROJ_LOC}\res\cursors\cursor04.png"
File "${PROJ_LOC}\res\cursors\cursor05.png"
File "${PROJ_LOC}\res\cursors\cursor06.png"
File "${PROJ_LOC}\res\cursors\cursor07.png"
File "${PROJ_LOC}\res\cursors\cursor08.png"
File "${PROJ_LOC}\res\cursors\cursor09.png"
File "${PROJ_LOC}\res\cursors\cursor10.png"
File "${PROJ_LOC}\res\cursors\cursor11.png"
File "${PROJ_LOC}\res\cursors\cursor12.png"
File "${PROJ_LOC}\res\cursors\cursor13.png"
File "${PROJ_LOC}\res\cursors\cursor14.png"
File "${PROJ_LOC}\res\cursors\cursor15.png"
SetOutPath "$APPDATA\daw\data"
File "${PROJ_LOC}\dist\userdata\view0.layout"
File "${PROJ_LOC}\dist\userdata\view1.layout"
File "${PROJ_LOC}\dist\userdata\view2.layout"
File "${PROJ_LOC}\dist\userdata\view3.layout"
File "${PROJ_LOC}\dist\userdata\view4.layout"
File "${PROJ_LOC}\dist\userdata\view5.layout"
File "${PROJ_LOC}\dist\userdata\view6.layout"
File "${PROJ_LOC}\dist\userdata\view7.layout"
File "${PROJ_LOC}\dist\userdata\view8.layout"
File "${PROJ_LOC}\dist\userdata\view9.layout"
File "${PROJ_LOC}\dist\userdata\settings.json"
File "${PROJ_LOC}\dist\userdata\theme.json"

SectionEnd

######################################################################

Section -Icons_Reg
SetOutPath "$INSTDIR"
WriteUninstaller "$INSTDIR\uninstall.exe"

!ifdef REG_START_MENU
!insertmacro MUI_STARTMENU_WRITE_BEGIN Application
CreateDirectory "$SMPROGRAMS\$SM_Folder"
CreateShortCut "$SMPROGRAMS\$SM_Folder\${STARTMENU_LINK_NAME}.lnk" "$INSTDIR\${MAIN_APP_EXE}"
CreateShortCut "$SMPROGRAMS\$SM_Folder\Uninstall ${STARTMENU_LINK_NAME}.lnk" "$INSTDIR\uninstall.exe"

!ifdef PRODUCT_URL_VENDOR
WriteIniStr "$INSTDIR\${STARTMENU_LINK_NAME} Website.url" "InternetShortcut" "URL" "${PRODUCT_URL_VENDOR}"
CreateShortCut "$SMPROGRAMS\$SM_Folder\${STARTMENU_LINK_NAME} Website.lnk" "$INSTDIR\${STARTMENU_LINK_NAME} Website.url"
!endif
!insertmacro MUI_STARTMENU_WRITE_END
!endif

WriteRegStr ${REG_ROOT} "${REG_APP_PATH}" "" "$INSTDIR\${MAIN_APP_EXE}"
WriteRegStr ${REG_ROOT} "${UNINSTALL_PATH}"  "DisplayName" "${PRODUCT_NAME_DISPLAY}"
WriteRegStr ${REG_ROOT} "${UNINSTALL_PATH}"  "UninstallString" "$INSTDIR\uninstall.exe"
WriteRegStr ${REG_ROOT} "${UNINSTALL_PATH}"  "DisplayIcon" "$INSTDIR\${MAIN_APP_EXE}"
WriteRegStr ${REG_ROOT} "${UNINSTALL_PATH}"  "DisplayVersion" "${PRODUCT_VERSION}"
WriteRegStr ${REG_ROOT} "${UNINSTALL_PATH}"  "Publisher" "${PROJECT_VENDOR_NAME}"

!ifdef PRODUCT_URL_VENDOR
WriteRegStr ${REG_ROOT} "${UNINSTALL_PATH}"  "URLInfoAbout" "${PRODUCT_URL_VENDOR}"
!endif
SectionEnd

######################################################################

Section Uninstall
${INSTALL_TYPE}
Delete "$INSTDIR\${MAIN_APP_EXE}"
Delete "$INSTDIR\daw-pluginscanner.exe"
Delete "$INSTDIR\libsoxr-clang-release.dll"
Delete "$INSTDIR\libc++.dll"
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
Delete "$INSTDIR\res\icons\daw_icon.png"
Delete "$INSTDIR\res\icons\duplicate.png"
Delete "$INSTDIR\res\icons\duplicate2.png"
Delete "$INSTDIR\res\icons\effect.png"
Delete "$INSTDIR\res\icons\file.png"
Delete "$INSTDIR\res\icons\file2.png"
Delete "$INSTDIR\res\icons\folder.png"
Delete "$INSTDIR\res\icons\folder_open.png"
Delete "$INSTDIR\res\icons\loop.png"
Delete "$INSTDIR\res\icons\midiplug.png"
Delete "$INSTDIR\res\icons\minus.png"
Delete "$INSTDIR\res\icons\modulation.png"
Delete "$INSTDIR\res\icons\opt_locked.png"
Delete "$INSTDIR\res\icons\opt_unlocked.png"
Delete "$INSTDIR\res\icons\paste.png"
Delete "$INSTDIR\res\icons\plus.png"
Delete "$INSTDIR\res\icons\save.png"
Delete "$INSTDIR\res\icons\speaker.png"
Delete "$INSTDIR\res\icons\synth.png"
Delete "$INSTDIR\res\icons\synth_small.png"
Delete "$INSTDIR\res\icons\warning.png"
Delete "$INSTDIR\res\icons\x.png"
Delete "$INSTDIR\res\icons\daw.ico"
Delete "$INSTDIR\res\fonts\icons.ttf"
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
!ifdef PRODUCT_URL_VENDOR
Delete "$INSTDIR\${PRODUCT_NAME_DISPLAY} Website.url"
!endif

RmDir "$INSTDIR"

!ifdef REG_START_MENU
!insertmacro MUI_STARTMENU_GETFOLDER "Application" $SM_Folder
Delete "$SMPROGRAMS\$SM_Folder\${STARTMENU_LINK_NAME}.lnk"
Delete "$SMPROGRAMS\$SM_Folder\Uninstall ${STARTMENU_LINK_NAME}.lnk"
!ifdef PRODUCT_URL_VENDOR
Delete "$SMPROGRAMS\$SM_Folder\${STARTMENU_LINK_NAME} Website.lnk"
!endif
Delete "$DESKTOP\${PRODUCT_NAME_DISPLAY}.lnk"
RmDir "$SMPROGRAMS\$SM_Folder"
!endif

DeleteRegKey ${REG_ROOT} "${REG_APP_PATH}"
DeleteRegKey ${REG_ROOT} "${UNINSTALL_PATH}"
SectionEnd

######################################################################

