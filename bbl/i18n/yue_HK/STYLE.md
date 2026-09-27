# Hong Kong Cantonese localization style

This catalog uses Traditional Chinese characters and natural written Hong Kong Cantonese. It is not a relabelled `zh_TW` catalog.

- Use familiar Hong Kong terms such as「檔案」、「項目」、「網絡」、「打印機」、「軟件」and「耗材」consistently.
- Safe, friendly guidance may sound conversational:「一齊開始啦」、「載入緊」or「試多次」.
- Destructive, irreversible, error, account, privacy, certificate, plug-in and security-related copy must stay restrained and explicit. State the affected item, consequence and recovery action; do not joke or soften the risk.
- Preserve every placeholder exactly, including order and repetition (`%s`, `%d`, `%1%`, `%1$d`, `{{name}}`). Do not translate identifiers, file extensions, menu accelerators or protocol names.
- Prefer short labels that remain clear without surrounding context. Use「儲存」for Save,「刪除」for Delete,「移除」for Remove,「覆寫」for Overwrite and「取消」for Cancel.
- Keep product names such as Bambu Studio, Bambu Lab, AMS, WLAN, FTP, G-code and 3MF unchanged.

Run `py compile_translation.py` after editing. Use `py compile_translation.py --check` in validation to prove that the checked-in MO is reproducible and current.
