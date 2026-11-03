Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!ifndef STAGE_DIR
 !error "STAGE_DIR is required; run scripts/package.ps1"
!endif
!ifndef OUTPUT_FILE
 !error "OUTPUT_FILE is required"
!endif
Name "4x4 Eye Contact 0.1.0 Development Preview"
BrandingText "© 2026 4x4Tools"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES64\4x4Tools\EyeContact"
RequestExecutionLevel admin
SetCompressor /FINAL zlib
ShowInstDetails show
ShowUninstDetails show
VIProductVersion "0.1.0.1"
VIAddVersionKey /LANG=1033 "ProductName" "4x4 Eye Contact"
VIAddVersionKey /LANG=1033 "FileDescription" "4x4 Eye Contact development installer"
VIAddVersionKey /LANG=1033 "FileVersion" "0.1.0.1"
VIAddVersionKey /LANG=1033 "CompanyName" "4x4Tools"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Copyright © 2026 4x4Tools"
!define MUI_ABORTWARNING
!ifdef PROJECT_ROOT
 !define MUI_ICON "${PROJECT_ROOT}\resources\branding\4x4Tools.ico"
 !define MUI_UNICON "${PROJECT_ROOT}\resources\branding\4x4Tools.ico"
 !define MUI_WELCOMEFINISHPAGE_BITMAP "${PROJECT_ROOT}\resources\branding\Welcome.bmp"
 !define MUI_HEADERIMAGE
 !define MUI_HEADERIMAGE_RIGHT
 !define MUI_HEADERIMAGE_BITMAP "${PROJECT_ROOT}\resources\branding\Header.bmp"
!endif
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
Var PluginDir
Function .onInit
 ${IfNot} ${RunningX64}
  MessageBox MB_OK|MB_ICONSTOP "This plugin requires Windows x64 and a supported NVIDIA GPU."
  Abort
 ${EndIf}
 SetRegView 64
 StrCpy $PluginDir "$PROGRAMFILES64\Adobe\Common\Plug-ins\7.0\MediaCore\4x4Tools\EyeContact"
 InitPluginsDir
 SetOutPath "$PLUGINSDIR"
 File /oname=SupportCheck.exe "${STAGE_DIR}\SupportCheck.exe"
 nsExec::ExecToStack '"$PLUGINSDIR\SupportCheck.exe" --install'
 Pop $0
 Pop $1
 ${If} $0 != 0
  MessageBox MB_OK|MB_ICONSTOP "Installation stopped before plugin files were installed.$\r$\n$\r$\n$1"
  Abort
 ${EndIf}
FunctionEnd
Section "Eye Contact"
 SetOutPath "$INSTDIR"
 File "${STAGE_DIR}\SupportCheck.exe"
 File "${STAGE_DIR}\EyeContactCli.exe"
 File "${STAGE_DIR}\README.md"
 File "${STAGE_DIR}\THIRD-PARTY-NOTICES.md"
 File "${STAGE_DIR}\runtime-manifest.json"
 SetOutPath "$INSTDIR\runtime"
 File /r "${STAGE_DIR}\runtime\*.*"
 SetOutPath "$PluginDir"
 ClearErrors
 File "${STAGE_DIR}\4x4EyeContact.aex"
 IfErrors install_failed
 WriteINIStr "$PluginDir\4x4EyeContact.ini" "NVIDIA" "ARSDK_ROOT" "$INSTDIR\runtime\ARSDK"
 IfErrors install_failed
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4ToolsEyeContact" "DisplayName" "4x4 Eye Contact (Development Preview)"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4ToolsEyeContact" "DisplayVersion" "0.1.0-dev.1"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4ToolsEyeContact" "Publisher" "4x4Tools"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4ToolsEyeContact" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
 WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4ToolsEyeContact" "NoModify" 1
 WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4ToolsEyeContact" "NoRepair" 1
 Goto install_done
 install_failed:
  MessageBox MB_OK|MB_ICONSTOP "The Adobe plugin could not be written. Close Adobe apps and retry setup."
  SetErrorLevel 1
  Abort
 install_done:
SectionEnd
Function un.onInit
 ${If} $INSTDIR != "$PROGRAMFILES64\4x4Tools\EyeContact"
  MessageBox MB_OK|MB_ICONSTOP "The uninstaller is outside its expected installation directory."
  Abort
 ${EndIf}
 SetRegView 64
 nsExec::ExecToStack '"$INSTDIR\SupportCheck.exe" --check-hosts'
 Pop $0
 Pop $1
 ${If} $0 != 0
  MessageBox MB_OK|MB_ICONSTOP "$1"
  Abort
 ${EndIf}
FunctionEnd
Section "Uninstall"
 StrCpy $PluginDir "$PROGRAMFILES64\Adobe\Common\Plug-ins\7.0\MediaCore\4x4Tools\EyeContact"
 ClearErrors
 Delete "$PluginDir\4x4EyeContact.aex"
 IfErrors uninstall_failed
 Delete "$PluginDir\4x4EyeContact.ini"
 RMDir "$PluginDir"
 RMDir "$PROGRAMFILES64\Adobe\Common\Plug-ins\7.0\MediaCore\4x4Tools"
 ; un.onInit restricts the absolute installation path before recursive removal.
 RMDir /r "$INSTDIR\runtime"
 Delete "$INSTDIR\SupportCheck.exe"
 Delete "$INSTDIR\EyeContactCli.exe"
 Delete "$INSTDIR\README.md"
 Delete "$INSTDIR\THIRD-PARTY-NOTICES.md"
 Delete "$INSTDIR\runtime-manifest.json"
 Delete "$INSTDIR\Uninstall.exe"
 RMDir "$INSTDIR"
 DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4ToolsEyeContact"
 Goto uninstall_done
 uninstall_failed:
  MessageBox MB_OK|MB_ICONSTOP "The plugin is still in use. Close Adobe apps and run the uninstaller again."
  Abort
 uninstall_done:
SectionEnd
