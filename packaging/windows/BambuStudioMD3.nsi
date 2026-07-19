Unicode true
RequestExecutionLevel user

!include "MUI2.nsh"
!include "LogicLib.nsh"

!ifndef PRODUCT_VERSION
  !error "PRODUCT_VERSION must be provided by the build"
!endif
!ifndef FILE_VERSION
  !error "FILE_VERSION must be provided by the build"
!endif
!ifndef PAYLOAD_DIR
  !error "PAYLOAD_DIR must point to the installed BambuStudio payload"
!endif
!ifndef OUT_FILE
  !error "OUT_FILE must be provided by the build"
!endif
!ifndef INSTALLER_ICON
  !error "INSTALLER_ICON must be provided by the build"
!endif

!define PRODUCT_NAME "Bambu Studio MD3"
!define PRODUCT_PUBLISHER "codingmachineedge"
!define PRODUCT_EXE "bambu-studio.exe"
!define PRODUCT_REG_KEY "Software\codingmachineedge\BambuStudioMD3"
!define PRODUCT_UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\BambuStudioMD3"
!define PRODUCT_INSTALL_DIR "$LOCALAPPDATA\Programs\Bambu Studio MD3"

Name "${PRODUCT_NAME}"
OutFile "${OUT_FILE}"
InstallDir "${PRODUCT_INSTALL_DIR}"
Icon "${INSTALLER_ICON}"
UninstallIcon "${INSTALLER_ICON}"
BrandingText "${PRODUCT_NAME} ${PRODUCT_VERSION}"
SetCompressor /SOLID lzma
SetOverwrite on

VIProductVersion "${FILE_VERSION}"
VIAddVersionKey /LANG=1033 "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey /LANG=1033 "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey /LANG=1033 "FileDescription" "${PRODUCT_NAME} Windows installer"
VIAddVersionKey /LANG=1033 "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${PRODUCT_VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "GNU AGPL v3"

!define MUI_ABORTWARNING
!define MUI_ICON "${INSTALLER_ICON}"
!define MUI_UNICON "${INSTALLER_ICON}"
!define MUI_FINISHPAGE_NOAUTOCLOSE

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${PAYLOAD_DIR}\LICENSE.txt"
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "TradChinese"

Section "Bambu Studio MD3" SEC_MAIN
  SetShellVarContext current

  ; The install directory is deliberately fixed and per-user. This avoids an
  ; elevation prompt and makes the guarded uninstall below safe.
  RMDir /r "${PRODUCT_INSTALL_DIR}"
  SetOutPath "${PRODUCT_INSTALL_DIR}"
  File /r "${PAYLOAD_DIR}\*.*"

  WriteUninstaller "${PRODUCT_INSTALL_DIR}\Uninstall.exe"

  CreateDirectory "$SMPROGRAMS\Bambu Studio MD3"
  CreateShortcut "$SMPROGRAMS\Bambu Studio MD3\Bambu Studio MD3.lnk" "${PRODUCT_INSTALL_DIR}\${PRODUCT_EXE}" "" "${PRODUCT_INSTALL_DIR}\${PRODUCT_EXE}" 0
  CreateShortcut "$SMPROGRAMS\Bambu Studio MD3\Uninstall Bambu Studio MD3.lnk" "${PRODUCT_INSTALL_DIR}\Uninstall.exe"

  WriteRegStr HKCU "${PRODUCT_REG_KEY}" "InstallDir" "${PRODUCT_INSTALL_DIR}"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "DisplayIcon" "${PRODUCT_INSTALL_DIR}\${PRODUCT_EXE},0"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "InstallLocation" "${PRODUCT_INSTALL_DIR}"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "UninstallString" '"${PRODUCT_INSTALL_DIR}\Uninstall.exe"'
  WriteRegDWORD HKCU "${PRODUCT_UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${PRODUCT_UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  SetShellVarContext current

  ; Never recurse into a user-controlled or relocated directory.
  ${If} $INSTDIR != "${PRODUCT_INSTALL_DIR}"
    MessageBox MB_ICONSTOP|MB_OK "The uninstaller is not running from the expected Bambu Studio MD3 directory. No files were removed."
    Abort
  ${EndIf}

  Delete "$SMPROGRAMS\Bambu Studio MD3\Bambu Studio MD3.lnk"
  Delete "$SMPROGRAMS\Bambu Studio MD3\Uninstall Bambu Studio MD3.lnk"
  RMDir "$SMPROGRAMS\Bambu Studio MD3"

  DeleteRegKey HKCU "${PRODUCT_UNINSTALL_KEY}"
  DeleteRegKey HKCU "${PRODUCT_REG_KEY}"
  RMDir /r "${PRODUCT_INSTALL_DIR}"
SectionEnd
