Unicode true
RequestExecutionLevel user

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "nsDialogs.nsh"

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
!define PRODUCT_PREF_KEY "Software\codingmachineedge\BambuStudioMD3Preferences"
!define PRODUCT_UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\BambuStudioMD3"
!ifndef PRODUCT_INSTALL_ROOT
  !define PRODUCT_INSTALL_ROOT "$LOCALAPPDATA"
!endif
!ifndef PRODUCT_PROGRAMS_DIR
  !define PRODUCT_PROGRAMS_DIR "${PRODUCT_INSTALL_ROOT}\Programs"
!endif
!ifndef PRODUCT_INSTALL_DIR
  !define PRODUCT_INSTALL_DIR "${PRODUCT_PROGRAMS_DIR}\Bambu Studio MD3"
!endif
!ifndef PRODUCT_SHORTCUT_ROOT
  !define PRODUCT_SHORTCUT_ROOT "$SMPROGRAMS"
!endif
!ifndef PRODUCT_SHORTCUT_DIR
  !define PRODUCT_SHORTCUT_DIR "${PRODUCT_SHORTCUT_ROOT}\Bambu Studio MD3"
!endif

Var LanguageMode
Var LanguageModeDialog
Var LanguageModeEnglish
Var LanguageModeCantonese
Var LanguageModeBilingual

!macro ShowLanguageStop ENGLISH CANTONESE
  ${If} $LanguageMode == "yue_HK"
    MessageBox MB_ICONSTOP|MB_OK "${CANTONESE}" /SD IDOK
  ${ElseIf} $LanguageMode == "bilingual_en_yue_HK"
    MessageBox MB_ICONSTOP|MB_OK "${ENGLISH}$\r$\n$\r$\n${CANTONESE}" /SD IDOK
  ${Else}
    MessageBox MB_ICONSTOP|MB_OK "${ENGLISH}" /SD IDOK
  ${EndIf}
!macroend

!macro AssertNotReparse PATH
  System::Call 'kernel32::GetFileAttributesW(w "${PATH}") i.R8'
  ${If} $R8 != -1
    IntOp $R9 $R8 & 0x400
    ${If} $R9 != 0
      SetErrorLevel 2
      !insertmacro ShowLanguageStop \
        "Bambu Studio MD3 stopped because a protected install path is a junction or symbolic link: ${PATH}" \
        "Bambu Studio MD3 已停止，因為受保護嘅安裝路徑係接合點或者符號連結：${PATH}"
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
AllowSkipFiles off

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

Function .onInit
  StrCpy $LanguageMode "en"
  ReadRegStr $0 HKCU "${PRODUCT_PREF_KEY}" "LanguageMode"
  ${If} $0 == ""
    ReadRegStr $0 HKCU "${PRODUCT_REG_KEY}" "LanguageMode"
  ${EndIf}
  ${If} $0 == "en"
  ${OrIf} $0 == "yue_HK"
  ${OrIf} $0 == "bilingual_en_yue_HK"
    StrCpy $LanguageMode $0
  ${EndIf}

  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/LANGMODE=" $R1
  ${IfNot} ${Errors}
    ${If} $R1 == "en"
    ${OrIf} $R1 == "yue_HK"
    ${OrIf} $R1 == "bilingual_en_yue_HK"
      StrCpy $LanguageMode $R1
    ${Else}
      SetErrorLevel 2
      MessageBox MB_ICONSTOP|MB_OK "Invalid /LANGMODE value. Use en, yue_HK, or bilingual_en_yue_HK." /SD IDOK
      Abort
    ${EndIf}
  ${EndIf}
FunctionEnd

Function un.onInit
  StrCpy $LanguageMode "en"
  ReadRegStr $0 HKCU "${PRODUCT_PREF_KEY}" "LanguageMode"
  ${If} $0 == ""
    ReadRegStr $0 HKCU "${PRODUCT_REG_KEY}" "LanguageMode"
  ${EndIf}
  ${If} $0 == "yue_HK"
  ${OrIf} $0 == "bilingual_en_yue_HK"
    StrCpy $LanguageMode $0
  ${EndIf}
FunctionEnd

Function LanguageModePageCreate
  nsDialogs::Create 1018
  Pop $LanguageModeDialog
  ${If} $LanguageModeDialog == error
    Abort
  ${EndIf}

  ${NSD_CreateLabel} 0 0 100% 28u "Choose the Bambu Studio UI language.$\r$\n揀選 Bambu Studio 介面語言。"
  Pop $0
  ${NSD_CreateRadioButton} 8u 38u 92% 14u "English"
  Pop $LanguageModeEnglish
  ${NSD_CreateRadioButton} 8u 60u 92% 14u "廣東話（香港，預覽版）"
  Pop $LanguageModeCantonese
  ${NSD_CreateRadioButton} 8u 82u 92% 14u "English + 廣東話（香港，預覽版）"
  Pop $LanguageModeBilingual
  ${NSD_CreateLabel} 8u 108u 92% 30u "You can change this later in Preferences. Existing Bambu Studio locales remain available there.$\r$\n之後可以喺偏好設定更改；其他現有語言亦會保留。"
  Pop $0

  ${If} $LanguageMode == "yue_HK"
    ${NSD_Check} $LanguageModeCantonese
  ${ElseIf} $LanguageMode == "bilingual_en_yue_HK"
    ${NSD_Check} $LanguageModeBilingual
  ${Else}
    ${NSD_Check} $LanguageModeEnglish
  ${EndIf}
  nsDialogs::Show
FunctionEnd

Function LanguageModePageLeave
  ${NSD_GetState} $LanguageModeCantonese $0
  ${If} $0 == ${BST_CHECKED}
    StrCpy $LanguageMode "yue_HK"
    Return
  ${EndIf}
  ${NSD_GetState} $LanguageModeBilingual $0
  ${If} $0 == ${BST_CHECKED}
    StrCpy $LanguageMode "bilingual_en_yue_HK"
    Return
  ${EndIf}
  StrCpy $LanguageMode "en"
FunctionEnd

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${PAYLOAD_DIR}\LICENSE.txt"
Page custom LanguageModePageCreate LanguageModePageLeave
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
    ReadRegStr $2 HKCU "${PRODUCT_REG_KEY}" "RecoveryState"
    ${If} $2 == "bootstrap_cleanup"
      ; A prior bootstrap could not remove a partial Uninstall.exe. The
      ; ownership marker makes this retryable without adopting unknown paths.
      ClearErrors
      Delete "${PRODUCT_INSTALL_DIR}\Uninstall.exe"
      ${If} ${Errors}
        SetErrorLevel 2
        !insertmacro ShowLanguageStop \
          "A partial Bambu Studio MD3 recovery file is still in use. Close security tools using it and retry this installer." \
          "部分 Bambu Studio MD3 復原檔案仍然使用緊。請關閉使用緊佢嘅保安工具，再重試安裝。"
        Abort
      ${EndIf}
      ; FileExists "directory\\*" also reports an empty directory. Remove an
      ; empty, guarded remnant before treating what remains as unknown.
      ClearErrors
      RMDir "${PRODUCT_INSTALL_DIR}"
      ${If} ${FileExists} "${PRODUCT_INSTALL_DIR}\*"
        SetErrorLevel 2
        !insertmacro ShowLanguageStop \
          "Unknown paths remain beside a partial Bambu Studio MD3 recovery file. Move those paths elsewhere and retry." \
          "部分 Bambu Studio MD3 復原檔案旁邊仍有未知路徑。請將嗰啲路徑移去其他位置再試。"
        Abort
      ${EndIf}
      RMDir "${PRODUCT_INSTALL_DIR}"
      DeleteRegKey HKCU "${PRODUCT_UNINSTALL_KEY}"
      DeleteRegKey HKCU "${PRODUCT_REG_KEY}"
    ${ElseIf} ${FileExists} "${PRODUCT_INSTALL_DIR}\Uninstall.exe"
      ; NSIS uninstallers normally return from their copy stub before the temp
      ; child finishes. Copy it ourselves, then _?= disables the second copy so
      ; ExecWait observes the real cleanup and exit code.
      InitPluginsDir
      ClearErrors
      CopyFiles /SILENT "${PRODUCT_INSTALL_DIR}\Uninstall.exe" "$PLUGINSDIR\BambuStudioMD3-Previous-Uninstall.exe"
      ${If} ${Errors}
        SetErrorLevel 2
        !insertmacro ShowLanguageStop \
          "The previous Bambu Studio MD3 uninstaller could not be staged safely. No files were changed." \
          "無法安全準備上一個 Bambu Studio MD3 解除安裝程式。未有變更任何檔案。"
        Abort
      ${EndIf}

      ClearErrors
      ExecWait '"$PLUGINSDIR\BambuStudioMD3-Previous-Uninstall.exe" /S _?=${PRODUCT_INSTALL_DIR}' $1
      ${If} ${Errors}
        StrCpy $1 2
      ${EndIf}
      Delete "$PLUGINSDIR\BambuStudioMD3-Previous-Uninstall.exe"

      ${If} $1 != 0
        SetErrorLevel 2
        !insertmacro ShowLanguageStop \
          "The previous Bambu Studio MD3 installation could not be removed. Close the application and retry. No new files were installed." \
          "無法移除上一個 Bambu Studio MD3。請關閉程式再試。未有安裝新檔案。"
        Abort
      ${EndIf}
      ; The prior uninstaller can leave an empty root after removing its
      ; payload. Remove it before the wildcard existence guard below.
      ClearErrors
      RMDir "${PRODUCT_INSTALL_DIR}"
      ${If} ${FileExists} "${PRODUCT_INSTALL_DIR}\*"
        SetErrorLevel 2
        !insertmacro ShowLanguageStop \
          "Unknown paths remain in the previous Bambu Studio MD3 directory. The old application was removed, but the new version was not installed. Move those paths elsewhere and retry." \
          "上一個 Bambu Studio MD3 資料夾仍有未知路徑。舊程式已移除，但未有安裝新版本。請將嗰啲路徑移去其他位置再試。"
        Abort
      ${EndIf}
    ${ElseIf} $2 == "directory_cleanup"
      ; A prior owned uninstall preserved this marker because unknown paths
      ; blocked root removal. Recovery may proceed only after the guarded,
      ; non-recursive removal succeeds.
      ClearErrors
      RMDir "${PRODUCT_INSTALL_DIR}"
      ${If} ${FileExists} "${PRODUCT_INSTALL_DIR}\*"
        SetErrorLevel 2
        !insertmacro ShowLanguageStop \
          "Unknown paths remain in the Bambu Studio MD3 directory. No new files were installed." \
          "Bambu Studio MD3 資料夾仍有未知路徑。未有安裝新檔案。"
        Abort
      ${EndIf}
      DeleteRegKey HKCU "${PRODUCT_UNINSTALL_KEY}"
      DeleteRegKey HKCU "${PRODUCT_REG_KEY}"
    ${ElseIf} ${FileExists} "${PRODUCT_INSTALL_DIR}\*"
      SetErrorLevel 2
      !insertmacro ShowLanguageStop \
        "The owned Bambu Studio MD3 directory is missing its uninstaller. No files were changed." \
        "由安裝程式管理嘅 Bambu Studio MD3 資料夾欠缺解除安裝程式。未有變更任何檔案。"
      Abort
    ${Else}
      DeleteRegKey HKCU "${PRODUCT_UNINSTALL_KEY}"
      DeleteRegKey HKCU "${PRODUCT_REG_KEY}"
    ${EndIf}
  ${ElseIf} ${FileExists} "${PRODUCT_INSTALL_DIR}\*"
    SetErrorLevel 2
    !insertmacro ShowLanguageStop \
      "The Bambu Studio MD3 target directory already contains files not owned by this installer. No files were changed." \
      "Bambu Studio MD3 目標資料夾已有唔屬於呢個安裝程式嘅檔案。未有變更任何檔案。"
    Abort
  ${EndIf}

  ; Recheck after any prior uninstaller completed. The target is fixed and
  ; per-user, and only an empty or marker-owned directory is accepted.
  !insertmacro BambuMD3AssertDestinationPaths
  ${If} ${FileExists} "${PRODUCT_SHORTCUT_DIR}\*"
    SetErrorLevel 2
    !insertmacro ShowLanguageStop \
      "The Bambu Studio MD3 Start menu directory contains unknown paths. No shortcuts or application files were changed." \
      "Bambu Studio MD3 開始功能表資料夾有未知路徑。未有變更捷徑或者應用程式檔案。"
    Abort
  ${EndIf}
  SetOutPath "${PRODUCT_INSTALL_DIR}"

  ; Establish a cleanup-capable partial-install state before extraction. If a
  ; write fails or the user cancels, retrying the installer (or running the
  ; registered uninstaller) can safely remove every owned partial path.
  ClearErrors
  WriteRegStr HKCU "${PRODUCT_REG_KEY}" "InstallerId" "${PRODUCT_INSTALLER_ID}"
  WriteRegStr HKCU "${PRODUCT_REG_KEY}" "InstallDir" "${PRODUCT_INSTALL_DIR}"
  WriteRegStr HKCU "${PRODUCT_REG_KEY}" "LanguageMode" "$LanguageMode"
  WriteRegStr HKCU "${PRODUCT_REG_KEY}" "RecoveryState" "bootstrap_cleanup"
  IfErrors install_bootstrap_metadata_failed

  ClearErrors
  WriteUninstaller "${PRODUCT_INSTALL_DIR}\Uninstall.exe"
  IfErrors install_bootstrap_failed

  ; CI-only failure injection: hold the just-created recovery file without
  ; delete sharing, then exercise the ownership-preserving failure path. This
  ; define is never used for the published installer.
  !ifdef TEST_FORCE_BOOTSTRAP_CLEANUP_FAILURE
    System::Call 'kernel32::CreateFileW(w "${PRODUCT_INSTALL_DIR}\Uninstall.exe", i 0x80000000, i 0, p 0, i 3, i 0x80, p 0) p.R7'
    ${If} $R7 == -1
      ; Make a failure to establish the CI fixture distinguishable from the
      ; cleanup failure that the fixture is intended to prove.
      WriteRegStr HKCU "${PRODUCT_REG_KEY}" "RecoveryState" "bootstrap_test_setup_failed"
      SetErrorLevel 2
      Abort
    ${EndIf}
    Goto install_bootstrap_failed
  !endif

  ClearErrors
  WriteRegStr HKCU "${PRODUCT_REG_KEY}" "InstallerId" "${PRODUCT_INSTALLER_ID}"
  WriteRegStr HKCU "${PRODUCT_REG_KEY}" "InstallDir" "${PRODUCT_INSTALL_DIR}"
  WriteRegStr HKCU "${PRODUCT_REG_KEY}" "LanguageMode" "$LanguageMode"
  ; This user preference intentionally survives uninstall, just like app
  ; profiles/projects, and keeps the selected mode across failed upgrades.
  WriteRegStr HKCU "${PRODUCT_PREF_KEY}" "LanguageMode" "$LanguageMode"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "DisplayName" "${PRODUCT_NAME} (incomplete installation)"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "InstallLocation" "${PRODUCT_INSTALL_DIR}"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "UninstallString" '"${PRODUCT_INSTALL_DIR}\Uninstall.exe"'
  WriteRegDWORD HKCU "${PRODUCT_UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${PRODUCT_UNINSTALL_KEY}" "NoRepair" 1
  IfErrors install_bootstrap_failed
  ClearErrors
  WriteRegStr HKCU "${PRODUCT_REG_KEY}" "RecoveryState" "ready"
  IfErrors install_bootstrap_failed

  ClearErrors
  File /r "${PAYLOAD_DIR}\*.*"
  IfErrors install_payload_failed

  ClearErrors
  CreateDirectory "${PRODUCT_SHORTCUT_DIR}"
  CreateShortcut "${PRODUCT_SHORTCUT_DIR}\Bambu Studio MD3.lnk" "${PRODUCT_INSTALL_DIR}\${PRODUCT_EXE}" "" "${PRODUCT_INSTALL_DIR}\${PRODUCT_EXE}" 0
  CreateShortcut "${PRODUCT_SHORTCUT_DIR}\Uninstall Bambu Studio MD3.lnk" "${PRODUCT_INSTALL_DIR}\Uninstall.exe"

  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "DisplayIcon" "${PRODUCT_INSTALL_DIR}\${PRODUCT_EXE},0"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "InstallLocation" "${PRODUCT_INSTALL_DIR}"
  WriteRegStr HKCU "${PRODUCT_UNINSTALL_KEY}" "UninstallString" '"${PRODUCT_INSTALL_DIR}\Uninstall.exe"'
  WriteRegDWORD HKCU "${PRODUCT_UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${PRODUCT_UNINSTALL_KEY}" "NoRepair" 1
  IfErrors install_registration_failed
  SetErrorLevel 0
  Goto install_done

install_bootstrap_failed:
  ClearErrors
  Delete "${PRODUCT_INSTALL_DIR}\Uninstall.exe"
  ${If} ${Errors}
    ; Keep the ownership marker and recovery state so a later installer can
    ; safely retry deletion instead of treating the partial file as unowned.
    DeleteRegKey HKCU "${PRODUCT_UNINSTALL_KEY}"
    WriteRegStr HKCU "${PRODUCT_REG_KEY}" "InstallerId" "${PRODUCT_INSTALLER_ID}"
    WriteRegStr HKCU "${PRODUCT_REG_KEY}" "InstallDir" "${PRODUCT_INSTALL_DIR}"
    WriteRegStr HKCU "${PRODUCT_REG_KEY}" "LanguageMode" "$LanguageMode"
    WriteRegStr HKCU "${PRODUCT_REG_KEY}" "RecoveryState" "bootstrap_cleanup"
    SetErrorLevel 2
    !insertmacro ShowLanguageStop \
      "Bambu Studio MD3 could not remove a partial recovery file. Its ownership marker was preserved; close security tools using the file and retry this installer." \
      "Bambu Studio MD3 無法移除部分復原檔案。擁有權標記已保留；請關閉使用緊檔案嘅保安工具，再重試安裝。"
    Abort
  ${EndIf}
  ${If} ${FileExists} "${PRODUCT_INSTALL_DIR}\*"
    DeleteRegKey HKCU "${PRODUCT_UNINSTALL_KEY}"
    WriteRegStr HKCU "${PRODUCT_REG_KEY}" "RecoveryState" "bootstrap_cleanup"
    SetErrorLevel 2
    !insertmacro ShowLanguageStop \
      "Bambu Studio MD3 could not clean its partial recovery directory. Its ownership marker was preserved; remove unknown paths and retry." \
      "Bambu Studio MD3 無法清理部分復原資料夾。擁有權標記已保留；請移除未知路徑再試。"
    Abort
  ${EndIf}
  RMDir "${PRODUCT_INSTALL_DIR}"
  DeleteRegKey HKCU "${PRODUCT_UNINSTALL_KEY}"
  DeleteRegKey HKCU "${PRODUCT_REG_KEY}"
  SetErrorLevel 2
  !insertmacro ShowLanguageStop \
    "Bambu Studio MD3 could not create its recovery metadata. No application payload was installed." \
    "Bambu Studio MD3 無法建立復原資料。未有安裝應用程式檔案。"
  Abort

install_bootstrap_metadata_failed:
  DeleteRegKey HKCU "${PRODUCT_UNINSTALL_KEY}"
  DeleteRegKey HKCU "${PRODUCT_REG_KEY}"
  RMDir "${PRODUCT_INSTALL_DIR}"
  SetErrorLevel 2
  !insertmacro ShowLanguageStop \
    "Bambu Studio MD3 could not establish its ownership metadata. No application payload was installed." \
    "Bambu Studio MD3 無法建立擁有權資料。未有安裝應用程式檔案。"
  Abort

install_payload_failed:
  SetErrorLevel 2
  !insertmacro ShowLanguageStop \
    "Bambu Studio MD3 could not extract every application file. The recovery uninstaller was preserved; retry this installer or remove the incomplete installation from Windows Settings." \
    "Bambu Studio MD3 無法解壓全部應用程式檔案。復原用解除安裝程式已保留；請重試，或者喺 Windows 設定移除未完成嘅安裝。"
  Abort

install_registration_failed:
  SetErrorLevel 2
  !insertmacro ShowLanguageStop \
    "Bambu Studio MD3 files were installed, but Windows registration did not complete. The recovery uninstaller was preserved in the install directory." \
    "Bambu Studio MD3 檔案已安裝，但 Windows 登記未完成。復原用解除安裝程式已保留喺安裝資料夾。"
  Abort

install_done:
SectionEnd

Section "Uninstall"
  SetShellVarContext current

  ; Never remove files from a relocated or unexpected directory.
  ${If} $INSTDIR != "${PRODUCT_INSTALL_DIR}"
    SetErrorLevel 2
    !insertmacro ShowLanguageStop \
      "The uninstaller is not running from the expected Bambu Studio MD3 directory. No files were removed." \
      "解除安裝程式唔係由預期嘅 Bambu Studio MD3 資料夾執行。未有移除任何檔案。"
    Abort
  ${EndIf}

  ReadRegStr $0 HKCU "${PRODUCT_REG_KEY}" "InstallerId"
  ${If} $0 != "${PRODUCT_INSTALLER_ID}"
    SetErrorLevel 2
    !insertmacro ShowLanguageStop \
      "The Bambu Studio MD3 ownership marker is missing or invalid. No files were removed." \
      "Bambu Studio MD3 擁有權標記遺失或者無效。未有移除任何檔案。"
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

  Delete "${PRODUCT_SHORTCUT_DIR}\Bambu Studio MD3.lnk"
  Delete "${PRODUCT_SHORTCUT_DIR}\Uninstall Bambu Studio MD3.lnk"
  RMDir "${PRODUCT_SHORTCUT_DIR}"

  ; Do not discard ownership when a user-owned path keeps the fixed install
  ; directory non-empty.  The next installer must fail closed while that path
  ; exists, then be able to complete recovery after the user removes it.
  ClearErrors
  RMDir "${PRODUCT_INSTALL_DIR}"
  ${If} ${FileExists} "${PRODUCT_INSTALL_DIR}\*"
    DeleteRegKey HKCU "${PRODUCT_UNINSTALL_KEY}"
    WriteRegStr HKCU "${PRODUCT_REG_KEY}" "InstallerId" "${PRODUCT_INSTALLER_ID}"
    WriteRegStr HKCU "${PRODUCT_REG_KEY}" "InstallDir" "${PRODUCT_INSTALL_DIR}"
    WriteRegStr HKCU "${PRODUCT_REG_KEY}" "LanguageMode" "$LanguageMode"
    WriteRegStr HKCU "${PRODUCT_REG_KEY}" "RecoveryState" "directory_cleanup"
    SetErrorLevel 2
    !insertmacro ShowLanguageStop \
      "Unknown paths remain in the Bambu Studio MD3 directory. They were preserved; remove them and retry the installer to complete recovery." \
      "Bambu Studio MD3 資料夾仍有未知路徑。佢哋已被保留；請移除後重試安裝程式以完成復原。"
    Abort
  ${EndIf}

  DeleteRegKey HKCU "${PRODUCT_UNINSTALL_KEY}"
  DeleteRegKey HKCU "${PRODUCT_REG_KEY}"
  SetErrorLevel 0
  Goto uninstall_done

uninstall_owned_files_failed:
  SetErrorLevel 2
  !insertmacro ShowLanguageStop \
    "Some Bambu Studio MD3 files are still in use. Close the application and run the uninstaller again. The uninstall entry was preserved." \
    "部分 Bambu Studio MD3 檔案仲使用緊。請關閉程式，再執行解除安裝程式。解除安裝項目已保留。"
  Abort

uninstall_done:
SectionEnd
