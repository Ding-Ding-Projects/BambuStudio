# Element locks and the unlock ladder

**Surface:** right-click any element that offers Edit appearance and choose
**Lock this element...** (or **Unlock this element...** on a locked one);
File > Element locks lists every lock; also the command palette
(`Ctrl+Shift+F`, type "Element locks") and the Every feature page →
`src/slic3r/GUI/ElementLocks/LockService.{hpp,cpp}`. The rules live in the
wx-free `LockModel` (`src/slic3r/GUI/ElementLocks/LockModel.{hpp,cpp}`).

A lock keeps one part of the window from being changed by a slip or an
impulse, for example the funny level during exams or a setting a child should
leave alone. It is not a security boundary: anyone who can edit the files in
the data folder can remove it.

## Locking

**Lock this element...** asks for:

- **Why**, optional, shown to whoever meets the lock.
- A **code** of 4 to 32 characters, typed twice.

The lock names you (your Windows user name) and the time. The code is never
stored: only a salted SHA-256 hash, repeated 20,000 times, is kept.

## What a lock does

A locked element ignores clicks, the mouse wheel and keys, and so does
everything inside it. `Tab` still moves past it, and its right-click menu
(the mouse, the Menu key or `Shift+F10`) still opens, with **Unlock this
element...**. Trying to use it shows a short note: what is locked, by whom,
why, and that a right-click unlocks it.

## Unlocking

**Unlock this element...** shows who locked it, when and why, and asks for the
code. A wrong code costs nothing twice; from the third wrong code in a row the
next try waits:

| Wrong codes in a row | Wait before the next try |
| --- | --- |
| 1 or 2 | none |
| 3 | 30 s |
| 4 | 2 min |
| 5 | 10 min |
| 6 or more | 1 hour |

## The unlock ladder

For a forgotten code. It is offered after three wrong codes, or under
**Forgot the code?**:

1. Wait 1 minute.
2. Type the name of what is locked (case and spaces do not matter).
3. Wait 5 more minutes.

Then the lock comes off. The ladder keeps its place when you close the window
or Bambu Studio, and a finished wait moves on by itself while the window is
open.

## Every lock

File > Element locks lists every lock with who, when and why, and **Unlock...**
beside each.

## Stored values

`element-locks.json` in the data folder (`%APPDATA%\BambuStudio`): per lock
the element id, its name, why, who, when, the salt and hash, wrong codes in a
row, the wait, and the ladder's step.

## Checks

`tests/element_locks.test.mjs` compiles `tests/element_locks_test.cpp`: codes
that are refused, the hash against an independent computation, wrong codes and
their waits, the ladder, saving and loading. Its negative build hashes a code
once instead of 20,000 times and must fail.

## Not done yet

Locks on rows of lists (object list rows, filament rows) and a lock badge drawn
on the locked element are not built; those menus offer the lock only where a
window stands behind the element.
