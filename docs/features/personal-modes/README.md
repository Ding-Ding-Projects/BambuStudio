# Personal presentation services

This directory describes the native personal-presentation implementation and the
remaining application integration. These services are not evidence that a settings
control or a workspace accommodation is already visible in the packaged application.

| Service | Implementation | Integration |
| --- | --- | --- |
| Shared presentation mode | `SchoolMode.hpp`, `SchoolStore.hpp`, `SchoolRuntime.hpp` | Application lifetime owner, credential adapter, settings and palette filtering |
| Attention accommodations | `AttentionModes.hpp` | Five settings controls, active workspace callbacks, persistence and history |
| Serialized narration | `SpeechQueue.hpp`, `SapiVoice.hpp`, `TtsNarrator.cpp` | Voice controls, quiet/screen-reader state, teardown and additional event producers |

- [Shared record and effective presentation](school-mode.md)
- [Attention accommodations](attention.md)
- [Narration and voice selection](narration.md)
- [Application integration checklist](integration.md)
- [Funny-level defaults and existing preferences](funny-default.md)

The services expose no HTTP API. A Postman collection does not apply.

## Verification

`tests/personal_modes/personal_modes_tests.cpp` contains executable behavioral
assertions for defaults, independent modes, callbacks, snoozing, persistence failure,
speech completion, track order, selection fallback, schema limits, suppression,
restoration, and real Windows file replacement and locking.

Run `tests/personal_modes/run_native.ps1`. Add `-Negative` to compile isolated mutated
copies that remove speech completion and presentation suppression checks. The script
requires the mutated executables to fail and the restored baseline to pass. Compiler
failure is not accepted as a successful negative regression.

The completed MSVC baseline run passed 62 assertions. Both deliberate behavioral
mutations failed as required, and the restored source passed all 62 assertions again.
Run the script with `-Msvc -Negative` from an initialized MSVC developer environment.
Earlier GCC 13.2 attempts stopped inside the compiler with
`internal compiler error: Illegal instruction`; they were not counted as negative
regression verdicts. Full wxWidgets application compilation and built-interface
interaction evidence remain pending. No capture or installer execution is claimed.
