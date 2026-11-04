Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "Sections.nsh"
!ifndef PROJECT_ROOT
 !error "Run scripts/package-suite.ps1"
!endif
!ifndef OUTPUT_FILE
 !error "OUTPUT_FILE is required"
!endif
!ifndef SUITE_VERSION
 !define SUITE_VERSION "0.2.0"
!endif
!ifndef SUITE_MANIFEST
 !define SUITE_MANIFEST "${PROJECT_ROOT}\build\release-manifest.json"
!endif
!ifndef SUITE_LICENSE
 !define SUITE_LICENSE "${PROJECT_ROOT}\build\release-license.txt"
!endif
Name "4x4Tools Broadcast Suite"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES64\4x4Tools\BroadcastSuite"
!ifdef UI_PREVIEW
 RequestExecutionLevel user
 Caption "4x4Tools Broadcast Suite Setup (Layout Preview)"
!else
 RequestExecutionLevel admin
 Caption "4x4Tools Broadcast Suite Setup"
!endif
BrandingText "© 2026 4x4Tools"
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUninstDetails show
VIProductVersion "0.2.0.4"
VIAddVersionKey /LANG=1033 "ProductName" "4x4Tools Broadcast Suite"
VIAddVersionKey /LANG=1033 "CompanyName" "4x4Tools"
VIAddVersionKey /LANG=1033 "FileDescription" "4x4Tools Broadcast Suite Setup"
VIAddVersionKey /LANG=1033 "FileVersion" "0.2.0.4"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Copyright © 2026 4x4Tools"
!define MUI_ICON "${PROJECT_ROOT}\resources\branding\4x4Tools.ico"
!define MUI_UNICON "${PROJECT_ROOT}\resources\branding\4x4Tools.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${PROJECT_ROOT}\resources\branding\Welcome.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "${PROJECT_ROOT}\resources\branding\Header.bmp"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "4x4Tools Broadcast Suite"
!ifdef WEB_RELEASE
 !define MUI_WELCOMEPAGE_TEXT "One installer. Your picture and voice tools, together.$\r$\n$\r$\nSetup checks your NVIDIA GPU, downloads the right models and installs your selected Adobe effects automatically.$\r$\n$\r$\nNo extra files to choose, SDKs to install or developer accounts to sign in to.$\r$\n$\r$\nSave and close Adobe apps before continuing."
!else
 !define MUI_WELCOMEPAGE_TEXT "Bring NVIDIA-accelerated video and voice tools into Adobe.$\r$\n$\r$\nHighest-quality models and local processing, with separate video and audio components.$\r$\n$\r$\nKeep 4x4Tools-Setup.exe and its Data folder together."
!endif
!define MUI_FINISHPAGE_TEXT "The selected 4x4Tools effects are installed.$\r$\n$\r$\nFind video effects under 4x4Tools. In Premiere, scan VST3 effects using Audio Plug-In Manager.$\r$\n$\r$\nPowered by NVIDIA Maxine."
!insertmacro MUI_PAGE_WELCOME
!ifdef WEB_RELEASE
 !define MUI_LICENSEPAGE_CHECKBOX
 !insertmacro MUI_PAGE_LICENSE "${SUITE_LICENSE}"
!endif
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
Var Architecture
Var Components
Var SetupLog
Function .onInit
!ifdef UI_PREVIEW
 StrCpy $Architecture "120"
 Return
!endif
 ${IfNot} ${RunningX64}
  MessageBox MB_OK|MB_ICONSTOP "4x4Tools Broadcast Suite requires Windows x64 and a supported NVIDIA GPU."
  Abort
 ${EndIf}
 SetRegView 64
 InitPluginsDir
 SetOutPath "$PLUGINSDIR"
 File /oname=SupportCheck.exe "${PROJECT_ROOT}\build\bin\SupportCheck.exe"
 File /oname=install-suite.ps1 "${PROJECT_ROOT}\installer\install-suite.ps1"
 File /oname=setup-launcher.ps1 "${PROJECT_ROOT}\installer\setup-launcher.ps1"
!ifdef WEB_RELEASE
 File /oname=suite-manifest.json "${SUITE_MANIFEST}"
 File /oname=payload-crypto.cs "${PROJECT_ROOT}\installer\payload-crypto.cs"
 File /oname=release-key.bin "${PROJECT_ROOT}\build\release-key.bin"
!else
 File /oname=suite-manifest.json "${PROJECT_ROOT}\build\suite-manifest.json"
!endif
 nsExec::ExecToStack '"$PLUGINSDIR\SupportCheck.exe" --install'
 Pop $0
 Pop $1
 ${If} $0 != 0
  MessageBox MB_OK|MB_ICONSTOP "Setup stopped before installation.$\r$\n$\r$\n$1"
  Abort
 ${EndIf}
 nsExec::ExecToStack '"$PLUGINSDIR\SupportCheck.exe" --architecture'
 Pop $0
 Pop $Architecture
 ${If} $0 != 0
  MessageBox MB_OK|MB_ICONSTOP "The NVIDIA GPU model could not be selected."
  Abort
 ${EndIf}
FunctionEnd
Section "Video tools for Premiere and After Effects" SecVideo
SectionEnd
Section "Voice tools for Premiere and After Effects" SecAudio
SectionEnd
Section -Install
!ifdef UI_PREVIEW
 MessageBox MB_OK "Layout preview only. No plugin files will be installed."
 Abort
!endif
 StrCpy $Components "all"
 ${IfNot} ${SectionIsSelected} ${SecVideo}
  StrCpy $Components "audio"
  ${IfNot} ${SectionIsSelected} ${SecAudio}
   MessageBox MB_OK|MB_ICONSTOP "Choose at least one component."
   Abort
  ${EndIf}
 ${ElseIfNot} ${SectionIsSelected} ${SecAudio}
  StrCpy $Components "video"
 ${EndIf}
 DetailPrint "Downloading and installing your selected 4x4Tools effects..."
 GetTempFileName $SetupLog "$TEMP"
 DetailPrint "Setup log: $SetupLog"
 ; Keep child processes away from NSIS's native System.dll in $PLUGINSDIR.
 SetOutPath "$WINDIR"
!ifdef WEB_RELEASE
 nsExec::ExecToLog /TIMEOUT=3600000 '"$WINDIR\Sysnative\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\setup-launcher.ps1" -PackageRoot "$EXEDIR" -Manifest "$PLUGINSDIR\suite-manifest.json" -Architecture "$Architecture" -InstallRoot "$INSTDIR" -Components "$Components" -KeyFile "$PLUGINSDIR\release-key.bin" -LogFile "$SetupLog"'
!else
 nsExec::ExecToLog /TIMEOUT=3600000 '"$WINDIR\Sysnative\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\setup-launcher.ps1" -PackageRoot "$EXEDIR" -Manifest "$PLUGINSDIR\suite-manifest.json" -Architecture "$Architecture" -InstallRoot "$INSTDIR" -Components "$Components" -LogFile "$SetupLog"'
!endif
 Pop $0
 ${If} $0 != 0
  MessageBox MB_OK|MB_ICONSTOP "Setup could not finish. The details have been saved to:$\r$\n$\r$\n$SetupLog$\r$\n$\r$\nRun Setup again to retry. Interrupted downloads resume automatically."
  SetErrorLevel 1
  Abort
 ${EndIf}
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4ToolsBroadcastSuite" "DisplayName" "4x4Tools Broadcast Suite"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4ToolsBroadcastSuite" "DisplayVersion" "${SUITE_VERSION}"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4ToolsBroadcastSuite" "Publisher" "4x4Tools"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4ToolsBroadcastSuite" "DisplayIcon" "$INSTDIR\4x4Tools.ico"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4ToolsBroadcastSuite" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
 WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4ToolsBroadcastSuite" "NoModify" 1
 WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4ToolsBroadcastSuite" "NoRepair" 1
SectionEnd
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
 !insertmacro MUI_DESCRIPTION_TEXT ${SecVideo} "Eye Contact, background removal and blur, studio lighting, video denoising, auto framing and detail restoration."
 !insertmacro MUI_DESCRIPTION_TEXT ${SecAudio} "Noise and echo cleanup, Studio Voice preview and HQ, and voice restoration. Native After Effects effects plus Premiere VST3."
!insertmacro MUI_FUNCTION_DESCRIPTION_END
Function un.onInit
 SetRegView 64
 ${If} $INSTDIR != "$PROGRAMFILES64\4x4Tools\BroadcastSuite"
  MessageBox MB_OK|MB_ICONSTOP "The uninstaller is outside the expected 4x4Tools installation directory."
  Abort
 ${EndIf}
 nsExec::ExecToStack '"$INSTDIR\Tools\SupportCheck.exe" --check-hosts'
 Pop $0
 Pop $1
 ${If} $0 != 0
  MessageBox MB_OK|MB_ICONSTOP "$1"
  Abort
 ${EndIf}
FunctionEnd
Section "Uninstall"
 SetOutPath "$WINDIR"
 nsExec::ExecToLog '"$WINDIR\Sysnative\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$INSTDIR\uninstall-suite.ps1" -InstallRoot "$INSTDIR"'
 Pop $0
 ${If} $0 != 0
  MessageBox MB_OK|MB_ICONSTOP "Removal could not finish. Review the details above."
  Abort
 ${EndIf}
 DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\4x4ToolsBroadcastSuite"
 Delete "$INSTDIR\Uninstall.exe"
 RMDir "$INSTDIR"
SectionEnd
