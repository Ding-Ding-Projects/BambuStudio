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
!ifndef UNINSTALL_INCLUDE
  !error "UNINSTALL_INCLUDE must point to the generated owned-file removal list"
!endif

!define PRODUCT_NAME "Bambu Studio MD3"
!define PRODUCT_PUBLISHER "codingmachineedge"
!define PRODUCT_EXE "bambu-studio.exe"
!define PRODUCT_INSTALLER_ID "codingmachineedge.BambuStudioMD3.owned-v1"
!define PRODUCT_REG_KEY "Software\codingmachineedge\BambuStudioMD3"
!define PRODUCT_UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\BambuStudioMD3"
!define PRODUCT_INSTALL_DIR "$LOCALAPPDATA\Programs\Bambu Studio MD3"

!macro AssertNotReparse PATH
  System::Call 'kernel32::GetFileAttributesW(w "${PATH}") i.r8'
  ${If} $R8 != -1
    IntOp $R9 $R8 & 0x400
    ${If} $R9 != 0
      SetErrorLevel 2
      MessageBox MB_ICONSTOP|MB_OK "Bambu Studio MD3 stopped because a protected install path is a junction or symbolic link: ${PATH}" /SD IDOK
      Abort
    ${EndIf}
  ${EndIf}
!macroend

; Defines macros for destination guards, payload-file deletion, and deepest-first
; non-recursive directory removal. The generated file is derived from PAYLOAD_DIR.
!include "${UNINSTALL_INCLUDE}"

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

  ; Refuse directory junctions before reading prior ownership or writing files.
  !insertmacro BambuMD3AssertDestinationPaths

  ; Upgrade only a marker-owned installation. Running its prior uninstaller
  ; first removes files that no longer exist in the new payload.
  ReadRegStr $0 HKCU "${PRODUCT_REG_KEY}" "InstallerId"
  ${If} $0 == "${PRODUCT_INSTALLER_ID}"
    ${If} ${FileExists} "${PRODUCT_INSTALL_DIR}\Uninstall.exe"
      ExecWait '"${PRODUCT_INSTALL_DIR}\Uninstall.exe" /S' $1
      ${If} $1 != 0
        SetErrorLevel 2
        MessageBox MB_ICONSTOP|MB_OK "The previous Bambu Studio MD3 installation could not be removed. Close the application and retry. No new files were installed." /SD IDOK
        Abort
      ${EndIf}
    ${ElseIf} ${FileExists} "${PRODUCT_INSTALL_DIR}\*"
      SetErrorLevel 2
      MessageBox MB_ICONSTOP|MB_OK "The owned Bambu Studio MD3 directory is missing its uninstaller. No files were changed." /SD IDOK
      Abort
    ${Else}
      DeleteRegKey HKCU "${PRODUCT_UNINSTALL_KEY}"
      DeleteRegKey HKCU "${PRODUCT_REG_KEY}"
    ${EndIf}
  ${ElseIf} ${FileExists} "${PRODUCT_INSTALL_DIR}\*"
    SetErrorLevel 2
    MessageBox MB_ICONSTOP|MB_OK "The Bambu Studio MD3 target directory already contains files not owned by this installer. No files were changed." /SD IDOK
    Abort
  ${EndIf}

  ; Recheck after any prior uninstaller completed. The target is fixed and
  ; per-user, and only an empty or marker-owned directory is accepted.
  !insertmacro BambuMD3AssertDestinationPaths
  SetOutPath "${PRODUCT_INSTALL_DIR}"
  File /r "${PAYLOAD_DIR}\*.*"

  WriteUninstaller "${PRODUCT_INSTALL_DIR}\Uninstall.exe"

  CreateDirectory "$SMPROGRAMS\Bambu Studio MD3"
  CreateShortcut "$SMPROGRAMS\Bambu Studio MD3\Bambu Studio MD3.lnk" "${PRODUCT_INSTALL_DIR}\${PRODUCT_EXE}" "" "${PRODUCT_INSTALL_DIR}\${PRODUCT_EXE}" 0
  CreateShortcut "$SMPROGRAMS\Bambu Studio MD3\Uninstall Bambu Studio MD3.lnk" "${PRODUCT_INSTALL_DIR}\Uninstall.exe"

  WriteRegStr HKCU "${PRODUCT_REG_KEY}" "InstallerId" "${PRODUCT_INSTALLER_ID}"
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

  ; Never remove files from a relocated or unexpected directory.
  ${If} $INSTDIR != "${PRODUCT_INSTALL_DIR}"
    SetErrorLevel 2
    MessageBox MB_ICONSTOP|MB_OK "The uninstaller is not running from the expected Bambu Studio MD3 directory. No files were removed." /SD IDOK
    Abort
  ${EndIf}

  ReadRegStr $0 HKCU "${PRODUCT_REG_KEY}" "InstallerId"
  ${If} $0 != "${PRODUCT_INSTALLER_ID}"
    SetErrorLevel 2
    MessageBox MB_ICONSTOP|MB_OK "The Bambu Studio MD3 ownership marker is missing or invalid. No files were removed." /SD IDOK
    Abort
  ${EndIf}

  !insertmacro BambuMD3AssertDestinationPaths

  ; Stop on a locked payload file and retain the uninstaller registration so
  ; the user can close the application and retry.
  ClearErrors
  !insertmacro BambuMD3DeletePayloadFiles
  IfErrors uninstall_owned_files_failed

  ClearErrors
  Delete "${PRODUCT_INSTALL_DIR}\Uninstall.exe"
  IfErrors uninstall_owned_files_failed

  ; Directory removal is intentionally best-effort. Unknown paths keep their
  ; non-empty parent directories, while owned empty directories are removed.
  !insertmacro BambuMD3RemovePayloadDirectories

  Delete "$SMPROGRAMS\Bambu Studio MD3\Bambu Studio MD3.lnk"
  Delete "$SMPROGRAMS\Bambu Studio MD3\Uninstall Bambu Studio MD3.lnk"
  RMDir "$SMPROGRAMS\Bambu Studio MD3"

  DeleteRegKey HKCU "${PRODUCT_UNINSTALL_KEY}"
  DeleteRegKey HKCU "${PRODUCT_REG_KEY}"
  SetErrorLevel 0
  Goto uninstall_done

uninstall_owned_files_failed:
  SetErrorLevel 2
  MessageBox MB_ICONSTOP|MB_OK "Some Bambu Studio MD3 files are still in use. Close the application and run the uninstaller again. The uninstall entry was preserved." /SD IDOK
  Abort

uninstall_done:
SectionEnd
