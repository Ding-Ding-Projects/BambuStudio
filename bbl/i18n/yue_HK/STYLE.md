# Hong Kong Cantonese localization style

This catalog uses Traditional Chinese characters and natural written Hong Kong Cantonese. It is not a relabelled `zh_TW` catalog.

## Terms

`glossary.json` (regenerate with `py -3 build_glossary.py`) lists the fixed product terms, the curated short terms already in this catalog, and the formal-Taiwan or Simplified terms a draft must not use.

| English | Cantonese | Notes |
| --- | --- | --- |
| Filament, ink | 墨水 | The product calls filament "ink" (see `docs/features/windows/ink-terminology.md`); never 線材 or 耗材 |
| AMS, Ink Dispenser | 墨水機 | The Automatic Material System is shown as the Ink Dispenser |
| Project | 項目 | Not 專案 |
| Printer, print | 打印機, 打印 | Not 印表機 |
| Software, network | 軟件, 網絡 | Not 軟體, 網路 |
| File, folder | 檔案, 資料夾 | |
| Save, Delete, Remove, Overwrite, Cancel | 儲存, 刪除, 移除, 覆寫, 取消 | |

Keep product and protocol names unchanged: Bambu Studio, Bambu Lab, Bambu Cloud, MakerWorld, Wi-Fi, WLAN, LAN, FTP, HTTP, G-code, 3MF, STL, STEP, OBJ, USB, SD.

## Tone

- Safe, friendly guidance may sound conversational:「一齊開始啦」、「載入緊」or「試多次」.
- Destructive, irreversible, error, account, privacy, certificate, plug-in and security-related copy stays restrained and explicit. State the affected item, the consequence and the recovery action. Do not joke or soften the risk.
- Prefer short labels that stay clear without surrounding context; a button label is a verb phrase, not a sentence.

## Mechanics

- Preserve every placeholder exactly, including order and repetition (`%s`, `%d`, `%1%`, `%1$d`, `%%`, `{{name}}`, `{}`); the compiler rejects a mismatch.
- Keep the same number of line breaks (`\n`) as the English, so layouts that stack or wrap lines keep their shape.
- A menu mnemonic stays with the translation as a suffix: `&File` becomes `檔案(&F)`. A `\t` accelerator suffix is copied unchanged.
- Do not translate identifiers, config keys, file extensions, URLs, units or code.
- Plural entries have one form in Cantonese (`Plural-Forms: nplurals=1; plural=0;`); write it for the plural English.

## Entry metadata

Every entry carries exactly one `#. reviewed-category:` comment (`connection-offline`, `destructive-dirty-error-security`, `file-actions`, `navigation`, `preferences`, `slicing-printing`). Two optional markers exist:

- `#. review-status: agent-drafted` marks a draft that passed the mechanical checks but still needs a fluent human review. `coverage.json` counts these as `agent_drafted_messages`.
- `#. source-pending: <where>` marks a translation written ahead of source code that has not reached this branch; it is exempt from the source-membership check only.

## Validation

Run `py -3 compile_translation.py --check` (strict source membership) and `py -3 compile_translation.py --check --require-complete` (every English message has a Cantonese entry). The build compiles the MO itself; no compiled catalog is tracked.
