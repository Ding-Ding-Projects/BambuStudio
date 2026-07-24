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

; ---- From-source build defines (safe defaults; CI may override but need not) ----
!ifndef PRODUCT_SOURCE_REPO_URL
  !define PRODUCT_SOURCE_REPO_URL "https://github.com/codingmachineedge/BambuStudio.git"
!endif
!ifndef PRODUCT_SOURCE_TAG
  !define PRODUCT_SOURCE_TAG "v${PRODUCT_VERSION}"
!endif

; ---- MD3 light-scheme tokens (hex mirrors MD3Tokens.hpp / ui-md3 colors.css) ----
!define MD3_SURFACE "faf8fd"
!define MD3_ON_SURFACE "1a1b1f"
!define MD3_ON_SURFACE_VARIANT "44464e"
!define MD3_OUTLINE_VARIANT "c5c6d0"
!define MD3_PRIMARY "146c2e"
!define MD3_ON_PRIMARY "ffffff"
!define MD3_SC_LOW "f4f2f9"
!define MD3_ERROR "ba1a1a"
!define MD3_ON_PRIMARY_CONTAINER "00210c"
; COLORREF (0x00BBGGRR) variants for common-control messages.
!define MD3_PRIMARY_COLORREF 0x002E6C14
!define MD3_SURFACE_COLORREF 0x00FDF8FA
!define MD3_SC_LOW_COLORREF 0x00F9F2F4

; ---- Win32 message / style constants for MD3 theming + the build UI ----
; Guarded: several are already provided by WinMessages.nsh (pulled in via MUI2).
!ifndef WM_SETFONT
  !define WM_SETFONT 0x0030
!endif
!ifndef WM_COMMAND
  !define WM_COMMAND 0x0111
!endif
!ifndef SW_HIDE
  !define SW_HIDE 0
!endif
!ifndef SW_SHOW
  !define SW_SHOW 5
!endif
!ifndef SC_CLOSE
  !define SC_CLOSE 0xF060
!endif
!ifndef SS_CENTERIMAGE
  !define SS_CENTERIMAGE 0x00000200
!endif
!ifndef PBM_SETMARQUEE
  !define PBM_SETMARQUEE 0x040A
!endif
!ifndef PBM_SETBARCOLOR
  !define PBM_SETBARCOLOR 0x0409
!endif
!ifndef PBM_SETBKCOLOR
  !define PBM_SETBKCOLOR 0x2001
!endif
!ifndef EM_SETSEL
  !define EM_SETSEL 0x00B1
!endif
!ifndef EM_SCROLLCARET
  !define EM_SCROLLCARET 0x00B7
!endif
!ifndef EM_SETBKGNDCOLOR
  !define EM_SETBKGNDCOLOR 0x0443
!endif
!ifndef STILL_ACTIVE
  !define STILL_ACTIVE 259
!endif
; Read-only, multiline log-tail edit style bundle + client-edge exstyle.
!define MD3_LOGEDIT_STYLE 0x50210844
!define MD3_LOGEDIT_EXSTYLE 0x00000200
; Marquee progress bar style bundle (WS_CHILD|WS_VISIBLE|PBS_MARQUEE).
!define MD3_MARQUEE_STYLE 0x50000008

Var LanguageMode
Var LanguageModeDialog
Var LanguageModeEnglish
Var LanguageModeCantonese
Var LanguageModeBilingual

Var InstallMode
Var MD3Dialog
Var LogPixelsY
Var BiText
Var FontHero
Var FontSection
Var FontCard
Var FontBody
Var FontCaption
Var FontMono
Var InstallModePrebuilt
Var InstallModeFromSource
Var FinishLaunchCheck
Var BuildSessionDir
Var BuildPayloadDir
Var BuildProcHandle
Var BuildExitCode
Var BuildActive
Var BuildPolling
Var BuildStepLabel
Var BuildRepairLabel
Var BuildLogEdit
Var BuildBar
Var BuildStep
Var BuildAttempt
Var BuildMaxAttempts

; Create one GDI font. Height is DPI-aware: -MulDiv(pt, LOGPIXELSY, 72).
!macro MD3MakeFont OUTVAR PT WEIGHT FACE
  System::Call 'kernel32::MulDiv(i ${PT}, i $LogPixelsY, i 72) i .r0'
  IntOp $0 0 - $0
  System::Call 'gdi32::CreateFontW(i $0, i 0, i 0, i 0, i ${WEIGHT}, i 0, i 0, i 0, i 1, i 0, i 0, i 5, i 0, w "${FACE}") p .r0'
  StrCpy ${OUTVAR} $0
!macroend

; Apply a created font to a control (WM_SETFONT, redraw).
!macro MD3Font HWND FONTVAR
  SendMessage ${HWND} ${WM_SETFONT} ${FONTVAR} 1
!macroend

; Full-width MD3 hero band drawn at the top of every custom page (solid primary
; band, on-primary wordmark, Segoe UI Semibold 15pt, vertically centered).
!macro MD3HeroBand
  ${NSD_CreateLabel} 0 0 100% 40u "${PRODUCT_NAME}"
  Pop $0
  ${NSD_AddStyle} $0 ${SS_CENTERIMAGE}
  SetCtlColors $0 ${MD3_ON_PRIMARY} ${MD3_PRIMARY}
  !insertmacro MD3Font $0 $FontHero
!macroend

; Compose bilingual text into VAR following the ShowLanguageStop precedent:
; yue-only, EN + blank line + yue for bilingual, otherwise EN.
!macro MD3BiText VAR EN YUE
  ${If} $LanguageMode == "yue_HK"
    StrCpy ${VAR} "${YUE}"
  ${ElseIf} $LanguageMode == "bilingual_en_yue_HK"
    StrCpy ${VAR} "${EN}$\r$\n$\r$\n${YUE}"
  ${Else}
    StrCpy ${VAR} "${EN}"
  ${EndIf}
!macroend

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

; MUI_ABORTWARNING is intentionally NOT defined so its English-only warning does
; not fire; MUI still generates .onUserAbort, which delegates to our
; MUI_CUSTOMFUNCTION_ABORT hook (bilingual quit confirm + non-closable build lock).
!define MUI_CUSTOMFUNCTION_ABORT MD3OnUserAbort
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

  ; Default install source; the interactive install-mode page may change it.
  ; Silent (/S) never reaches that page, so it stays "prebuilt" (see spec 2.1/4.1).
  StrCpy $InstallMode "prebuilt"

  ; DPI-aware font metrics: read LOGPIXELSY once, then build the MD3 face set.
  System::Call 'user32::GetDC(p 0) p .r0'
  System::Call 'gdi32::GetDeviceCaps(p r0, i 90) i .r1'
  System::Call 'user32::ReleaseDC(p 0, p r0)'
  StrCpy $LogPixelsY $1
  ${If} $LogPixelsY <= 0
    StrCpy $LogPixelsY 96
  ${EndIf}
  !insertmacro MD3MakeFont $FontHero 15 700 "Segoe UI"
  !insertmacro MD3MakeFont $FontSection 12 700 "Segoe UI"
  !insertmacro MD3MakeFont $FontCard 11 600 "Segoe UI Semibold"
  !insertmacro MD3MakeFont $FontBody 9 400 "Segoe UI"
  !insertmacro MD3MakeFont $FontCaption 8 400 "Segoe UI"
  !insertmacro MD3MakeFont $FontMono 8 400 "Consolas"
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
  SetCtlColors $LanguageModeDialog ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3HeroBand

  ${NSD_CreateLabel} 12u 48u 88% 26u "Choose the Bambu Studio UI language.$\r$\n揀選 Bambu Studio 介面語言。"
  Pop $0
  SetCtlColors $0 ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3Font $0 $FontSection

  ${NSD_CreateRadioButton} 12u 80u 88% 14u "English"
  Pop $LanguageModeEnglish
  SetCtlColors $LanguageModeEnglish ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3Font $LanguageModeEnglish $FontCard
  ${NSD_CreateRadioButton} 12u 100u 88% 14u "廣東話（香港，預覽版）"
  Pop $LanguageModeCantonese
  SetCtlColors $LanguageModeCantonese ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3Font $LanguageModeCantonese $FontCard
  ${NSD_CreateRadioButton} 12u 120u 88% 14u "English + 廣東話（香港，預覽版）"
  Pop $LanguageModeBilingual
  SetCtlColors $LanguageModeBilingual ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3Font $LanguageModeBilingual $FontCard

  ; MD3 divider hairline (outline-variant).
  ${NSD_CreateLabel} 12u 142u 88% 1u ""
  Pop $0
  SetCtlColors $0 ${MD3_OUTLINE_VARIANT} ${MD3_OUTLINE_VARIANT}

  ${NSD_CreateLabel} 12u 150u 88% 30u "You can change this later in Preferences. Existing Bambu Studio locales remain available there.$\r$\n之後可以喺偏好設定更改；其他現有語言亦會保留。"
  Pop $0
  SetCtlColors $0 ${MD3_ON_SURFACE_VARIANT} ${MD3_SURFACE}
  !insertmacro MD3Font $0 $FontCaption

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

; ===========================================================================
;  MD3 custom pages: Welcome, Install-mode chooser, non-closable Build progress,
;  Finish. License + INSTFILES stay MUI with a themed SHOW callback.
; ===========================================================================

Function WelcomePageCreate
  ${If} ${Silent}
    Abort
  ${EndIf}
  nsDialogs::Create 1018
  Pop $MD3Dialog
  ${If} $MD3Dialog == error
    Abort
  ${EndIf}
  SetCtlColors $MD3Dialog ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3HeroBand

  !insertmacro MD3BiText $BiText \
    "Welcome to Bambu Studio MD3" \
    "歡迎使用 Bambu Studio MD3"
  ${NSD_CreateLabel} 12u 52u 88% 20u "$BiText"
  Pop $0
  SetCtlColors $0 ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3Font $0 $FontSection

  !insertmacro MD3BiText $BiText \
    "This installer sets up Bambu Studio MD3 for your account only, in your local application folder. It does not need administrator rights. Choose Next to continue." \
    "呢個安裝程式只會為你嘅帳戶，喺你本機嘅應用程式資料夾設定 Bambu Studio MD3，唔需要管理員權限。㩒「下一步」繼續。"
  ${NSD_CreateLabel} 12u 82u 88% 70u "$BiText"
  Pop $0
  SetCtlColors $0 ${MD3_ON_SURFACE_VARIANT} ${MD3_SURFACE}
  !insertmacro MD3Font $0 $FontBody

  nsDialogs::Show
FunctionEnd

Function LicensePageShow
  ; RichEdit honours bg only (partial MD3, deviation D). Control 1000 on the
  ; inner license dialog.
  FindWindow $0 "#32770" "" $HWNDPARENT
  GetDlgItem $1 $0 1000
  SetCtlColors $1 ${MD3_ON_SURFACE} ${MD3_SURFACE}
  SendMessage $1 ${EM_SETBKGNDCOLOR} 0 ${MD3_SURFACE_COLORREF}
FunctionEnd

Function InstallModePageCreate
  ; Silent guard: from-source is never reachable under /S (spec 2.1 / 4.1).
  ${If} ${Silent}
    StrCpy $InstallMode "prebuilt"
    Abort
  ${EndIf}
  nsDialogs::Create 1018
  Pop $MD3Dialog
  ${If} $MD3Dialog == error
    Abort
  ${EndIf}
  SetCtlColors $MD3Dialog ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3HeroBand

  !insertmacro MD3BiText $BiText \
    "How would you like to install?" \
    "你想點樣安裝？"
  ${NSD_CreateLabel} 12u 52u 88% 20u "$BiText"
  Pop $0
  SetCtlColors $0 ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3Font $0 $FontSection

  !insertmacro MD3BiText $BiText \
    "Install prebuilt (recommended)" \
    "安裝預先建置版本（建議）"
  ${NSD_CreateRadioButton} 12u 80u 88% 14u "$BiText"
  Pop $InstallModePrebuilt
  SetCtlColors $InstallModePrebuilt ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3Font $InstallModePrebuilt $FontCard

  !insertmacro MD3BiText $BiText \
    "Build from source (advanced — can take hours)" \
    "由原始碼建置（進階 — 可能需時數個鐘）"
  ${NSD_CreateRadioButton} 12u 100u 88% 14u "$BiText"
  Pop $InstallModeFromSource
  SetCtlColors $InstallModeFromSource ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3Font $InstallModeFromSource $FontCard

  ${NSD_CreateLabel} 12u 122u 88% 1u ""
  Pop $0
  SetCtlColors $0 ${MD3_OUTLINE_VARIANT} ${MD3_OUTLINE_VARIANT}

  !insertmacro MD3BiText $BiText \
    "Building from source downloads and installs developer tools, then compiles the app on this computer. It needs internet access and a lot of time and disk space." \
    "由原始碼建置會下載並安裝開發者工具，然後喺呢部電腦編譯應用程式。需要網絡連線、大量時間同磁碟空間。"
  ${NSD_CreateLabel} 12u 130u 88% 44u "$BiText"
  Pop $0
  SetCtlColors $0 ${MD3_ON_SURFACE_VARIANT} ${MD3_SURFACE}
  !insertmacro MD3Font $0 $FontCaption

  ${If} $InstallMode == "from-source"
    ${NSD_Check} $InstallModeFromSource
  ${Else}
    ${NSD_Check} $InstallModePrebuilt
  ${EndIf}
  nsDialogs::Show
FunctionEnd

Function InstallModePageLeave
  ${NSD_GetState} $InstallModeFromSource $0
  ${If} $0 == ${BST_CHECKED}
    StrCpy $InstallMode "from-source"
    Return
  ${EndIf}
  StrCpy $InstallMode "prebuilt"
FunctionEnd

; ---- Build progress page (from-source only; non-closable while building) ----

Function BuildProgressPageCreate
  ${If} ${Silent}
    Abort
  ${EndIf}
  ${If} $InstallMode != "from-source"
    Abort
  ${EndIf}

  nsDialogs::Create 1018
  Pop $MD3Dialog
  ${If} $MD3Dialog == error
    Abort
  ${EndIf}
  SetCtlColors $MD3Dialog ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3HeroBand

  !insertmacro MD3BiText $BiText \
    "Building Bambu Studio MD3 from source" \
    "正在由原始碼建置 Bambu Studio MD3"
  ${NSD_CreateLabel} 12u 52u 88% 20u "$BiText"
  Pop $0
  SetCtlColors $0 ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3Font $0 $FontSection

  !insertmacro MD3BiText $BiText \
    "This can take a long time — often one to several hours — and installs developer tools on this computer. Keep this window open. It cannot be closed until the build finishes or stops." \
    "呢個過程可能需要好長時間，通常一至數個鐘，並會喺呢部電腦安裝開發者工具。請保持呢個視窗開啟。喺建置完成或停止之前，唔可以關閉。"
  ${NSD_CreateLabel} 12u 80u 88% 44u "$BiText"
  Pop $0
  SetCtlColors $0 ${MD3_ON_SURFACE_VARIANT} ${MD3_SURFACE}
  !insertmacro MD3Font $0 $FontBody

  ; Marquee progress bar (shape/marquee OS-drawn; color settable, deviation D7).
  nsDialogs::CreateControl /NOUNLOAD "msctls_progress32" ${MD3_MARQUEE_STYLE} 0 12u 128u 76% 10u ""
  Pop $BuildBar
  SendMessage $BuildBar ${PBM_SETMARQUEE} 1 30
  SendMessage $BuildBar ${PBM_SETBARCOLOR} 0 ${MD3_PRIMARY_COLORREF}
  SendMessage $BuildBar ${PBM_SETBKCOLOR} 0 ${MD3_SURFACE_COLORREF}

  !insertmacro MD3BiText $BiText "Preparing…" "準備緊…"
  ${NSD_CreateLabel} 12u 144u 60% 12u "$BiText"
  Pop $BuildStepLabel
  SetCtlColors $BuildStepLabel ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3Font $BuildStepLabel $FontCard

  ${NSD_CreateLabel} 72% 144u 26% 12u ""
  Pop $BuildRepairLabel
  SetCtlColors $BuildRepairLabel ${MD3_PRIMARY} ${MD3_SURFACE}
  !insertmacro MD3Font $BuildRepairLabel $FontCaption

  nsDialogs::CreateControl /NOUNLOAD "EDIT" ${MD3_LOGEDIT_STYLE} ${MD3_LOGEDIT_EXSTYLE} 12u 160u 88% 40u ""
  Pop $BuildLogEdit
  SetCtlColors $BuildLogEdit ${MD3_ON_SURFACE} ${MD3_SC_LOW}
  !insertmacro MD3Font $BuildLogEdit $FontMono

  StrCpy $BuildStep ""
  StrCpy $BuildAttempt 0
  StrCpy $BuildMaxAttempts 5
  StrCpy $BuildExitCode 0

  Call BuildStartSession
  Call BuildLockClose
  StrCpy $BuildActive 1
  StrCpy $BuildPolling 0
  ${NSD_CreateTimer} BuildPoll 500

  nsDialogs::Show
FunctionEnd

Function BuildProgressPageLeave
  ${If} $BuildActive == 1
    ; Do not allow leaving while the build is still running.
    Abort
  ${EndIf}
  ; Only two terminal states may enter INSTFILES: a successful source build, or
  ; the explicit prebuilt fallback chosen after a bounded failure. After a
  ; declined fallback (bfb_stay) $InstallMode is still "from-source" with a
  ; nonzero exit code; advancing would copy a partial payload with no owned
  ; manifest and strand a state the fail-closed uninstaller cannot clean.
  ${If} $InstallMode == "from-source"
  ${AndIfNot} $BuildExitCode == 0
    Abort
  ${EndIf}
FunctionEnd

Function BuildStartSession
  ; Session dir: user-writable and OUTSIDE the fixed install dir, so it never
  ; trips the ownership / reparse guards.
  ${GetTime} "" "L" $2 $3 $4 $5 $6 $7 $8
  StrCpy $BuildSessionDir "$LOCALAPPDATA\codingmachineedge\BambuStudioMD3-FromSource\$4$3$2-$6$7$8"
  StrCpy $BuildPayloadDir "$BuildSessionDir\install-dir"
  CreateDirectory "$BuildSessionDir"

  ; Extract the PS helpers to a private temp dir; they never enter the payload.
  InitPluginsDir
  CreateDirectory "$PLUGINSDIR\bfs"
  SetOutPath "$PLUGINSDIR\bfs"
  File "${__FILEDIR__}\build-from-source\Build-FromSource.ps1"
  File "${__FILEDIR__}\build-from-source\Toolchain.ps1"
  File "${__FILEDIR__}\build-from-source\Opencode.ps1"

  StrCpy $9 'powershell.exe -NoProfile -ExecutionPolicy Bypass -NonInteractive -WindowStyle Hidden -File "$PLUGINSDIR\bfs\Build-FromSource.ps1" -SessionDir "$BuildSessionDir" -CloneUrl "${PRODUCT_SOURCE_REPO_URL}" -Tag "${PRODUCT_SOURCE_TAG}" -LanguageMode "$LanguageMode" -PayloadOut "$BuildPayloadDir"'

  ; STARTUPINFOW (68 bytes, cb set) + PROCESS_INFORMATION (16 bytes), async launch.
  System::Alloc 68
  Pop $1
  System::Call "*$1(i 68)"
  System::Alloc 16
  Pop $2
  ; CREATE_NO_WINDOW (0x08000000): no console flashes up.
  System::Call 'kernel32::CreateProcessW(w 0, w r9, i 0, i 0, i 0, i 0x08000000, i 0, w 0, p r1, p r2) i .r3'
  ${If} $3 == 0
    System::Free $1
    System::Free $2
    StrCpy $BuildProcHandle 0
    Return
  ${EndIf}
  System::Call "*$2(p .r4, p .r5, i .r6, i .r7)"
  StrCpy $BuildProcHandle $4
  System::Call 'kernel32::CloseHandle(p r5)'
  System::Free $1
  System::Free $2
FunctionEnd

Function BuildLockClose
  ; Hide + disable Cancel; disable Back/Next; strip SC_CLOSE from the system menu
  ; (greys the titlebar X and blocks Alt+F4).
  GetDlgItem $0 $HWNDPARENT 2
  EnableWindow $0 0
  ShowWindow $0 ${SW_HIDE}
  GetDlgItem $0 $HWNDPARENT 3
  EnableWindow $0 0
  GetDlgItem $0 $HWNDPARENT 1
  EnableWindow $0 0
  System::Call 'user32::GetSystemMenu(p $HWNDPARENT, i 0) p .r0'
  System::Call 'user32::DeleteMenu(p r0, i ${SC_CLOSE}, i 0)'
  System::Call 'user32::DrawMenuBar(p $HWNDPARENT)'
FunctionEnd

Function BuildUnlockClose
  ; Restore the default system menu (GetSystemMenu revert=1) and re-enable buttons.
  System::Call 'user32::GetSystemMenu(p $HWNDPARENT, i 1)'
  System::Call 'user32::DrawMenuBar(p $HWNDPARENT)'
  GetDlgItem $0 $HWNDPARENT 2
  EnableWindow $0 1
  ShowWindow $0 ${SW_SHOW}
  GetDlgItem $0 $HWNDPARENT 3
  EnableWindow $0 1
  GetDlgItem $0 $HWNDPARENT 1
  EnableWindow $0 1
FunctionEnd

Function BuildPoll
  ${If} $BuildPolling == 1
    Return
  ${EndIf}
  StrCpy $BuildPolling 1

  ${If} $BuildProcHandle == 0
    ${NSD_KillTimer} BuildPoll
    StrCpy $BuildExitCode 40
    Call BuildFinishBranch
    StrCpy $BuildPolling 0
    Return
  ${EndIf}

  Call BuildReadStatus
  Call BuildReadLogTail

  System::Call 'kernel32::GetExitCodeProcess(p $BuildProcHandle, *i .r1)'
  ${If} $1 == ${STILL_ACTIVE}
    StrCpy $BuildPolling 0
    Return
  ${EndIf}

  ${NSD_KillTimer} BuildPoll
  StrCpy $BuildExitCode $1
  System::Call 'kernel32::CloseHandle(p $BuildProcHandle)'
  StrCpy $BuildProcHandle 0
  Call BuildFinishBranch
  StrCpy $BuildPolling 0
FunctionEnd

Function BuildReadStatus
  ClearErrors
  FileOpen $4 "$BuildSessionDir\status.json" r
  IfErrors brs_done
  brs_loop:
    ClearErrors
    FileReadUTF16LE $4 $5
    IfErrors brs_close
    Push $5
    Call TrimLine
    Pop $5
    StrCpy $6 $5 7
    StrCmp $6 '"step":' 0 brs_notstep
      StrCpy $7 $5 "" 7
      Push $7
      Call StripJsonVal
      Pop $7
      StrCpy $BuildStep $7
      Goto brs_loop
    brs_notstep:
    StrCpy $6 $5 10
    StrCmp $6 '"attempt":' 0 brs_notattempt
      StrCpy $7 $5 "" 10
      Push $7
      Call StripJsonVal
      Pop $7
      StrCpy $BuildAttempt $7
      Goto brs_loop
    brs_notattempt:
    StrCpy $6 $5 14
    StrCmp $6 '"maxAttempts":' 0 brs_loop
      StrCpy $7 $5 "" 14
      Push $7
      Call StripJsonVal
      Pop $7
      StrCpy $BuildMaxAttempts $7
      Goto brs_loop
  brs_close:
    FileClose $4
  brs_done:
    ${If} $BuildStep != ""
      ${NSD_SetText} $BuildStepLabel "$BuildStep"
    ${EndIf}
    ${If} $BuildAttempt > 0
      ${If} $LanguageMode == "yue_HK"
        StrCpy $9 "修復 $BuildAttempt/$BuildMaxAttempts"
      ${ElseIf} $LanguageMode == "bilingual_en_yue_HK"
        StrCpy $9 "Repair $BuildAttempt/$BuildMaxAttempts · 修復 $BuildAttempt/$BuildMaxAttempts"
      ${Else}
        StrCpy $9 "Repair $BuildAttempt/$BuildMaxAttempts"
      ${EndIf}
      ${NSD_SetText} $BuildRepairLabel "$9"
    ${EndIf}
FunctionEnd

Function BuildReadLogTail
  ClearErrors
  FileOpen $4 "$BuildSessionDir\build.log" r
  IfErrors brl_done
  FileSeek $4 0 END $R0
  IntOp $R1 $R0 - 2000
  IntCmp $R1 2 brl_seek brl_setmin brl_seek
  brl_setmin:
    StrCpy $R1 2
  brl_seek:
  ; Keep an even byte offset for UTF-16LE.
  IntOp $R2 $R1 % 2
  IntOp $R1 $R1 - $R2
  FileSeek $4 $R1 SET
  StrCpy $9 ""
  brl_loop:
    ClearErrors
    FileReadUTF16LE $4 $R3
    IfErrors brl_close
    StrCpy $9 "$9$R3"
    StrLen $R4 $9
    ${If} $R4 > 950
      IntOp $R5 $R4 - 900
      StrCpy $9 $9 "" $R5
    ${EndIf}
    Goto brl_loop
  brl_close:
    FileClose $4
    ${NSD_SetText} $BuildLogEdit "$9"
    SendMessage $BuildLogEdit ${EM_SETSEL} -1 -1
    SendMessage $BuildLogEdit ${EM_SCROLLCARET} 0 0
  brl_done:
FunctionEnd

Function BuildFinishBranch
  ; All three terminal transitions re-enable close before the next/terminal page.
  StrCpy $BuildActive 0
  Call BuildUnlockClose

  ${If} $BuildExitCode == 0
    ; Success: advance into the shared ownership CopyFiles install (INSTFILES).
    GetDlgItem $0 $HWNDPARENT 1
    EnableWindow $0 1
    SendMessage $HWNDPARENT ${WM_COMMAND} 1 0
    Return
  ${EndIf}

  ${If} $BuildExitCode == 20
    ; Bounded failure: offer the prebuilt fallback (restrained, explicit copy).
    ${If} $LanguageMode == "yue_HK"
      MessageBox MB_ICONEXCLAMATION|MB_YESNO "由原始碼建置經過多次修復嘗試後仍然失敗。建置紀錄喺：$\r$\n$BuildSessionDir\build.log$\r$\n$\r$\n要改為安裝預先建置版本嗎？" /SD IDYES IDYES bfb_fallback IDNO bfb_stay
    ${ElseIf} $LanguageMode == "bilingual_en_yue_HK"
      MessageBox MB_ICONEXCLAMATION|MB_YESNO "Building from source failed after the maximum repair attempts. The build log is at:$\r$\n$BuildSessionDir\build.log$\r$\n$\r$\n由原始碼建置經過多次修復嘗試後仍然失敗。建置紀錄喺：$\r$\n$BuildSessionDir\build.log$\r$\n$\r$\nInstall the prebuilt version instead? / 要改為安裝預先建置版本嗎？" /SD IDYES IDYES bfb_fallback IDNO bfb_stay
    ${Else}
      MessageBox MB_ICONEXCLAMATION|MB_YESNO "Building from source failed after the maximum repair attempts. The build log is at:$\r$\n$BuildSessionDir\build.log$\r$\n$\r$\nInstall the prebuilt version instead?" /SD IDYES IDYES bfb_fallback IDNO bfb_stay
    ${EndIf}
    bfb_fallback:
      StrCpy $InstallMode "prebuilt"
      GetDlgItem $0 $HWNDPARENT 1
      EnableWindow $0 1
      SendMessage $HWNDPARENT ${WM_COMMAND} 1 0
      Return
    bfb_stay:
      ; Window is now closable; the user may cancel/quit.
      Return
  ${EndIf}

  ; Fatal (10/11/12/30/40): show code + log path, then quit. No payload written.
  ${If} $LanguageMode == "yue_HK"
    StrCpy $2 "由原始碼建置失敗（代碼 $BuildExitCode）。建置紀錄喺：$\r$\n$BuildSessionDir\build.log$\r$\n$\r$\n未有安裝任何檔案。"
  ${ElseIf} $LanguageMode == "bilingual_en_yue_HK"
    StrCpy $2 "Building from source failed (code $BuildExitCode). The build log is at:$\r$\n$BuildSessionDir\build.log$\r$\n$\r$\n由原始碼建置失敗（代碼 $BuildExitCode）。建置紀錄喺：$\r$\n$BuildSessionDir\build.log$\r$\n$\r$\nNo files were installed. / 未有安裝任何檔案。"
  ${Else}
    StrCpy $2 "Building from source failed (code $BuildExitCode). The build log is at:$\r$\n$BuildSessionDir\build.log$\r$\n$\r$\nNo files were installed."
  ${EndIf}
  SetErrorLevel 2
  MessageBox MB_ICONSTOP|MB_OK "$2" /SD IDOK
  Quit
FunctionEnd

; Strip trailing CR/LF/space/tab and leading space/tab from a stack string.
Function TrimLine
  Exch $R5
  Push $R6
  tl_trail:
    StrCpy $R6 $R5 1 -1
    StrCmp $R6 "$\r" tl_chop
    StrCmp $R6 "$\n" tl_chop
    StrCmp $R6 " " tl_chop
    StrCmp $R6 "$\t" tl_chop
    Goto tl_lead
  tl_chop:
    StrCpy $R5 $R5 -1
    Goto tl_trail
  tl_lead:
    StrCpy $R6 $R5 1
    StrCmp $R6 " " tl_choplead
    StrCmp $R6 "$\t" tl_choplead
    Goto tl_end
  tl_choplead:
    StrCpy $R5 $R5 "" 1
    Goto tl_lead
  tl_end:
  Pop $R6
  Exch $R5
FunctionEnd

; Strip trailing comma/space and surrounding double quotes from a JSON value.
Function StripJsonVal
  Exch $R5
  Push $R6
  sjv_trail:
    StrCpy $R6 $R5 1 -1
    StrCmp $R6 "," sjv_chop
    StrCmp $R6 " " sjv_chop
    StrCmp $R6 "$\r" sjv_chop
    StrCmp $R6 "$\n" sjv_chop
    Goto sjv_q
  sjv_chop:
    StrCpy $R5 $R5 -1
    Goto sjv_trail
  sjv_q:
    StrCpy $R6 $R5 1
    StrCmp $R6 '"' 0 sjv_end
    StrCpy $R5 $R5 "" 1
    StrCpy $R6 $R5 1 -1
    StrCmp $R6 '"' 0 sjv_end
    StrCpy $R5 $R5 -1
  sjv_end:
  Pop $R6
  Exch $R5
FunctionEnd

Function InstFilesPageShow
  ; Prebuilt path: recolor the progress bar (1004) + log list (1016).
  FindWindow $0 "#32770" "" $HWNDPARENT
  GetDlgItem $1 $0 1004
  SendMessage $1 ${PBM_SETBARCOLOR} 0 ${MD3_PRIMARY_COLORREF}
  SendMessage $1 ${PBM_SETBKCOLOR} 0 ${MD3_SURFACE_COLORREF}
  GetDlgItem $2 $0 1016
  SetCtlColors $2 ${MD3_ON_SURFACE} ${MD3_SURFACE}
FunctionEnd

Function FinishPageCreate
  ${If} ${Silent}
    Abort
  ${EndIf}
  nsDialogs::Create 1018
  Pop $MD3Dialog
  ${If} $MD3Dialog == error
    Abort
  ${EndIf}
  SetCtlColors $MD3Dialog ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3HeroBand

  !insertmacro MD3BiText $BiText \
    "Bambu Studio MD3 is installed" \
    "Bambu Studio MD3 已安裝完成"
  ${NSD_CreateLabel} 12u 52u 88% 20u "$BiText"
  Pop $0
  SetCtlColors $0 ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3Font $0 $FontSection

  !insertmacro MD3BiText $BiText \
    "Setup is complete. You can start Bambu Studio MD3 from the Start menu at any time." \
    "安裝已完成。你隨時可以喺開始功能表啟動 Bambu Studio MD3。"
  ${NSD_CreateLabel} 12u 82u 88% 40u "$BiText"
  Pop $0
  SetCtlColors $0 ${MD3_ON_SURFACE_VARIANT} ${MD3_SURFACE}
  !insertmacro MD3Font $0 $FontBody

  !insertmacro MD3BiText $BiText \
    "Launch Bambu Studio MD3 now" \
    "立即啟動 Bambu Studio MD3"
  ${NSD_CreateCheckBox} 12u 128u 88% 14u "$BiText"
  Pop $FinishLaunchCheck
  SetCtlColors $FinishLaunchCheck ${MD3_ON_SURFACE} ${MD3_SURFACE}
  !insertmacro MD3Font $FinishLaunchCheck $FontCard
  ${NSD_Check} $FinishLaunchCheck

  nsDialogs::Show
FunctionEnd

Function FinishPageLeave
  ${NSD_GetState} $FinishLaunchCheck $0
  ${If} $0 == ${BST_CHECKED}
    ${If} ${FileExists} "${PRODUCT_INSTALL_DIR}\${PRODUCT_EXE}"
      SetOutPath "${PRODUCT_INSTALL_DIR}"
      Exec '"${PRODUCT_INSTALL_DIR}\${PRODUCT_EXE}"'
    ${EndIf}
  ${EndIf}
FunctionEnd

Function MD3OnUserAbort
  ${If} $BuildActive == 1
    ; Belt-and-braces: refuse to quit while the build is running.
    Abort
  ${EndIf}
  ; Otherwise confirm the quit (bilingual, restrained), replacing MUI_ABORTWARNING.
  ${If} $LanguageMode == "yue_HK"
    MessageBox MB_ICONQUESTION|MB_YESNO "確定要結束 Bambu Studio MD3 安裝程式嗎？" /SD IDYES IDYES +2
    Abort
  ${ElseIf} $LanguageMode == "bilingual_en_yue_HK"
    MessageBox MB_ICONQUESTION|MB_YESNO "Quit the Bambu Studio MD3 setup? / 確定要結束 Bambu Studio MD3 安裝程式嗎？" /SD IDYES IDYES +2
    Abort
  ${Else}
    MessageBox MB_ICONQUESTION|MB_YESNO "Quit the Bambu Studio MD3 setup?" /SD IDYES IDYES +2
    Abort
  ${EndIf}
FunctionEnd

Page custom WelcomePageCreate
!define MUI_PAGE_CUSTOMFUNCTION_SHOW LicensePageShow
!insertmacro MUI_PAGE_LICENSE "${PAYLOAD_DIR}\LICENSE.txt"
Page custom LanguageModePageCreate LanguageModePageLeave
Page custom InstallModePageCreate InstallModePageLeave
Page custom BuildProgressPageCreate BuildProgressPageLeave
!define MUI_PAGE_CUSTOMFUNCTION_SHOW InstFilesPageShow
!insertmacro MUI_PAGE_INSTFILES
Page custom FinishPageCreate FinishPageLeave

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
  WriteRegStr HKCU "${PRODUCT_REG_KEY}" "InstallSource" "$InstallMode"
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
  WriteRegStr HKCU "${PRODUCT_REG_KEY}" "InstallSource" "$InstallMode"
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
  ${If} $InstallMode == "from-source"
    ; The built payload was staged by the non-closable build page, outside the
    ; fixed install dir. Copy it into the same ownership-managed target and record
    ; an owned manifest for the source-build uninstall branch. No prebuilt files
    ; are extracted on this path.
    ; Defense in depth behind the BuildProgressPageLeave gate: never copy unless
    ; the staged payload is complete (main exe + owned manifest both present).
    ${IfNot} ${FileExists} "$BuildPayloadDir\${PRODUCT_EXE}"
      SetErrors
    ${ElseIfNot} ${FileExists} "$BuildSessionDir\owned-manifest.txt"
      SetErrors
    ${EndIf}
    IfErrors install_payload_failed
    CopyFiles /SILENT "$BuildPayloadDir\*.*" "${PRODUCT_INSTALL_DIR}"
    IfErrors install_payload_failed
    CopyFiles /SILENT "$BuildSessionDir\owned-manifest.txt" "${PRODUCT_INSTALL_DIR}\.md3-owned-manifest.txt"
    IfErrors install_payload_failed
  ${Else}
    ; The payload includes the "mesa" subfolder (Mesa llvmpipe opengl32.dll +
    ; libgallium_wgl.dll, staged by CI from the hash-pinned pal1000
    ; mesa-dist-win release). It installs inert into <app>\mesa; the app copies
    ; the DLLs beside bambu-studio.exe only when the host lacks OpenGL 2.0
    ; (see docs/features/windows/software-gl-fallback.md). Like every other
    ; payload path it is enumerated by GenerateUninstallInclude.ps1, so the
    ; uninstaller removes it through the generated owned-file macros.
    File /r "${PAYLOAD_DIR}\*.*"
    IfErrors install_payload_failed
  ${EndIf}

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

; ---- From-source uninstall helpers (manifest-driven, fail-closed) ----

Function un.TrimLine
  Exch $R5
  Push $R6
  utl_trail:
    StrCpy $R6 $R5 1 -1
    StrCmp $R6 "$\r" utl_chop
    StrCmp $R6 "$\n" utl_chop
    StrCmp $R6 " " utl_chop
    StrCmp $R6 "$\t" utl_chop
    Goto utl_lead
  utl_chop:
    StrCpy $R5 $R5 -1
    Goto utl_trail
  utl_lead:
    StrCpy $R6 $R5 1
    StrCmp $R6 " " utl_choplead
    StrCmp $R6 "$\t" utl_choplead
    Goto utl_end
  utl_choplead:
    StrCpy $R5 $R5 "" 1
    Goto utl_lead
  utl_end:
  Pop $R6
  Exch $R5
FunctionEnd

; $R7 holds a relative manifest path. Abort fail-closed on drive/absolute/traversal.
Function un.AssertSafeRel
  Push $R5
  Push $R6
  StrCpy $R5 $R7 1
  StrCmp $R5 "\" un.asr_bad
  StrCmp $R5 "/" un.asr_bad
  StrCpy $R6 0
  un.asr_loop:
    StrCpy $R5 $R7 2 $R6
    StrCmp $R5 "" un.asr_ok
    StrCmp $R5 ".." un.asr_bad
    StrCpy $R5 $R7 1 $R6
    StrCmp $R5 ":" un.asr_bad
    IntOp $R6 $R6 + 1
    Goto un.asr_loop
  un.asr_bad:
    Pop $R6
    Pop $R5
    SetErrorLevel 2
    !insertmacro ShowLanguageStop \
      "The Bambu Studio MD3 ownership manifest contains an unsafe path and was rejected. No files were removed." \
      "Bambu Studio MD3 擁有權清單載有不安全路徑，已拒絕處理。未有移除任何檔案。"
    Abort
  un.asr_ok:
    Pop $R6
    Pop $R5
FunctionEnd

; Delete each F| file entry with the same reparse guard as the prebuilt path.
; Sets the error flag (fail-closed) on a locked file so the caller preserves the
; ownership marker and registration.
Function un.DeleteManifestFiles
  ClearErrors
  FileOpen $4 "${PRODUCT_INSTALL_DIR}\.md3-owned-manifest.txt" r
  ${If} ${Errors}
    SetErrorLevel 2
    !insertmacro ShowLanguageStop \
      "The Bambu Studio MD3 ownership manifest is missing. No files were removed." \
      "Bambu Studio MD3 擁有權清單遺失。未有移除任何檔案。"
    Abort
  ${EndIf}
  un.dmf_loop:
    ClearErrors
    FileReadUTF16LE $4 $5
    IfErrors un.dmf_done
    Push $5
    Call un.TrimLine
    Pop $5
    StrCmp $5 "" un.dmf_loop
    StrCpy $6 $5 2
    StrCmp $6 "F|" 0 un.dmf_loop
    StrCpy $R7 $5 "" 2
    Call un.AssertSafeRel
    !insertmacro AssertNotReparse "${PRODUCT_INSTALL_DIR}\$R7"
    ClearErrors
    Delete "${PRODUCT_INSTALL_DIR}\$R7"
    ${If} ${Errors}
      FileClose $4
      SetErrors
      Return
    ${EndIf}
    Goto un.dmf_loop
  un.dmf_done:
    FileClose $4
FunctionEnd

; RMDir each D| directory entry (manifest lists them deepest-first). Best-effort:
; unknown paths keep their non-empty parents, mirroring the prebuilt behavior.
Function un.RemoveManifestDirs
  ClearErrors
  FileOpen $4 "${PRODUCT_INSTALL_DIR}\.md3-owned-manifest.txt" r
  IfErrors un.rmd_done
  un.rmd_loop:
    ClearErrors
    FileReadUTF16LE $4 $5
    IfErrors un.rmd_close
    Push $5
    Call un.TrimLine
    Pop $5
    StrCmp $5 "" un.rmd_loop
    StrCpy $6 $5 2
    StrCmp $6 "D|" 0 un.rmd_loop
    StrCpy $R7 $5 "" 2
    Call un.AssertSafeRel
    !insertmacro AssertNotReparse "${PRODUCT_INSTALL_DIR}\$R7"
    RMDir "${PRODUCT_INSTALL_DIR}\$R7"
    Goto un.rmd_loop
  un.rmd_close:
    FileClose $4
  un.rmd_done:
FunctionEnd

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

  ; The install source decides how owned files are enumerated. A source build's
  ; file set can drift from the compiled prebuilt list, so it is driven by the
  ; owned manifest instead. Fixed-ancestor reparse asserts run first either way.
  ReadRegStr $3 HKCU "${PRODUCT_REG_KEY}" "InstallSource"

  !insertmacro BambuMD3AssertDestinationPaths

  ; Stop on a locked payload file and retain the uninstaller registration so
  ; the user can close the application and retry.
  ClearErrors
  ${If} $3 == "from-source"
    Call un.DeleteManifestFiles
  ${Else}
    !insertmacro BambuMD3DeletePayloadFiles
  ${EndIf}
  IfErrors uninstall_owned_files_failed

  ClearErrors
  Delete "${PRODUCT_INSTALL_DIR}\Uninstall.exe"
  IfErrors uninstall_owned_files_failed

  ; Directory removal is intentionally best-effort. Unknown paths keep their
  ; non-empty parent directories, while owned empty directories are removed.
  ${If} $3 == "from-source"
    Call un.RemoveManifestDirs
    ; The manifest is itself owned and removed last, before the shortcut cleanup.
    !insertmacro AssertNotReparse "${PRODUCT_INSTALL_DIR}\.md3-owned-manifest.txt"
    Delete "${PRODUCT_INSTALL_DIR}\.md3-owned-manifest.txt"
  ${Else}
    !insertmacro BambuMD3RemovePayloadDirectories
  ${EndIf}

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
