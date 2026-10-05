# Printer incident lifecycle tests

This standalone C++17 executable checks ten local-ledger scenarios: repeated
report deduplication, partial empty reports, disconnect unknown state, recurrence,
category isolation, bounded retention, restart persistence, corrupt-file
preservation, complete empty HMS recovery and stale-report rejection.

It deliberately disables transition snapshots and provides minimal
`ProjectHistoryManager` stubs. A passing result proves ledger lifecycle and JSON
persistence, not libgit2 integration, GUI behavior or printer connectivity.

From a Visual Studio x64 developer command prompt, create an output directory
outside the source tree and compile:

```bat
cl /nologo /std:c++17 /EHsc /W4 /DNOMINMAX /I src tests\printer_history\printer_history_tests.cpp src\slic3r\GUI\PrinterHistory.cpp /Fe"<output>\printer_history_tests.exe" /Fo"<output>\\"
"<output>\printer_history_tests.exe" "<output>\test-data"
```

The test-data directory must be disposable. Only the named `incidents.json`,
`corrupt.json` and `strict.json` fixtures below that directory are replaced.
No printer commands or network calls occur.
