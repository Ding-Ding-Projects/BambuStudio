# Element locks and the unlock ladder

**Surface:** right-click any element that offers Edit appearance and choose
**Lock this element...** (or **Unlock this element...** on a locked one); the
same item on an object's menu (in the object list or the 3D view) and on an
ink row's menu locks that one object or that one row; File > Element locks
lists every lock; also the command palette
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

A small lock badge sits on the top right corner of every locked element and
follows it as the window moves, scrolls or resizes. Clicking the badge shows
who locked it and why, with **Unlock...**. Elements that appear later (a
dialog that opens, a row that is added) get their badge within a second.

## Rows of lists

Two kinds of row are locked one by one, though every row of their list looks
the same:

- **An ink row** in the sidebar (the row's **...** menu, **Lock this
  element...**). Its ink choice, colour and nozzle buttons stop answering. Its
  menu still opens, with Unlock, and **Delete** and **Merge with** are off
  while it is locked; deleting it from anywhere else (Bulk ink actions, a sync
  with the AMS that would remove it) is refused with the note. A lock
  belongs to the row's number: delete an earlier row and the lock stays on
  the row that now has that number.
- **An object** (its right-click menu in the object list or the 3D view). Its
  rows in the object list carry the lock on their icon. A click on it, in the
  list or in the 3D view, stops with the note, and a right-click shows the note
  with **Unlock...**. It stays out of the selection: Select all, a selection
  rectangle or the keyboard in the list let it go again at once, so nothing that
  works on the selection (move, rotate, scale, delete, its settings, its ink)
  can reach it. **Arrange all** leaves it where it is and arranges the others
  around it; arranging only its plate and emptying its plate are refused, and
  **Delete all** stays off until it is unlocked. A lock belongs to the object's
  name. A gizmo handle in front of it still works.

The scrolling wheel and the keys go on as usual over a locked row, so the list
scrolls and the 3D view zooms. A drag that started elsewhere still ends over a
locked row.

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

`tests/element_locks_rows_wiring.test.mjs` checks the sources: clicks on a
locked row stop and a right-click offers Unlock, a release is held back only
after its press, ink rows are locked by their number and refuse delete and
merge, a locked object is let go from the selection in the list and the 3D
view, carries its badge, is arranged around and stops Delete all, and badges
follow locked elements. Each check also fails on a broken copy of the source.

## Not done yet

Undo and redo still step through changes made to an object before it was
locked, and changing an ink row's ink from Bulk ink actions or a sync with
the AMS still reaches a locked row (only deleting it is refused).
The 3D view shows no badge on a locked object; its rows in the object list do.
