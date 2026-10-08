# Unlock ladder

A lockout leaves a person watching a countdown. The unlock ladder gives them
something to do instead: a dim-sum question, then ten easy sums, then
whack-a-mole, then the clock. Winning a rung ends the wait early. It never signs
anybody in. The person goes back to the ordinary prompt and still needs their
PIN, password or code.

The trusted service is `Slic3r::LocalSecurity::UnlockLadder` in
`src/libslic3r/LocalSecurity/UnlockLadder.hpp`. It generates every question,
keeps every expected answer and grades every answer. A surface only shows what
the service hands it and passes answers back with the challenge's nonce.

## The rungs

1. **Dim sum.** One photo and four dish names. The right name ends the wait.
   Five wrong dishes move the ladder to the sums.
2. **Ten easy sums.** Additions and subtractions with numbers from 1 to 60, never
   a negative result. All ten must be right. A single wrong sum moves the ladder
   to whack-a-mole.
3. **Whack-a-mole.** A 3 by 3 grid. After a 3-second lead-in the round lasts
   24 seconds. Ten moles appear one at a time, each for 2 seconds, never twice
   running in the same cell and never overlapping. Six hits end the wait. A lost
   round moves the ladder to the clock.
4. **The clock.** The ladder is not offered again for that lockout. The person
   serves the wait they were already serving.

Every new lockout starts a fresh ladder. Falling to the clock leaves a person
exactly where they started.

## What it never does

- **It clears the wait, never the credential.** A win calls
  `AttemptBudget::clear_wait`. That removes the wait and nothing else: a
  `LockSession` stays locked and asks for its first factor again.
- **It never refunds the attempt budget.** A win restores exactly the five
  attempts that ordinary expiry restores. Wrong ladder answers never change the
  wait or the attempts.
- **It is capped.** At most three waits can be cleared per rolling hour. After
  that no challenge is issued and the clock is the only way through.
- **It never slows the escalation.** The next lockout after a skipped one is
  still longer: 30, 60, 120, 240, 480 and then 900 seconds.
- **It is graded by the service against a single-use nonce.** Each challenge has
  a fresh random 128-bit nonce. The service consumes the nonce before grading,
  so a wrong answer cannot be retried against the same question and a right one
  cannot be replayed. Asking for a challenge again returns the same live
  challenge, so a question cannot be re-rolled. A dish question expires after
  2 minutes and the sums after 5 minutes; an expired answer is not graded. An
  answer of the wrong kind, such as sums handed to a dish question, is graded
  wrong. A nonce from an earlier lockout is rejected.

## Timed round rules

- **A round cannot be won faster than it lasts.** Handing a round in before its
  24 seconds have passed loses it.
- **Each mole counts once.** The service times every strike itself. A hit
  counts only for a mole that is visible in that cell at that moment, and only
  the first time. Strikes before the round starts or after it ends do not count.
- **Tapping everything is not hitting moles.** A round accepts 20 strikes. Misses
  spend strikes, so striking every cell runs out long before the moles do.
- **An issued round is spent.** Walking away, restarting the application or
  answering with the wrong kind of answer loses the round. A round handed in
  more than 15 seconds after it ended has expired and is lost.

## School mode

`ladder_start(school_mode, dim_sum_ready)` is the one decision of where a
ladder starts. Under School mode the dim-sum rung is absent and the ladder
starts at the sums. No dish, photo or name reaches the surface. If School mode
switches on while a dish question is open, the question is withdrawn and never
graded. Rungs only move down, so switching School mode off later does not bring
the dish back for that lockout.

## Dish data

The dim-sum rung uses the same public catalog as the startup dim sum surprise.
`DimSum::ladder_dishes` in `DimSumSurpriseModel.hpp` copies each dish's names,
alt text and photo file name unchanged. Only a dish whose photo is already
cached can be the question; any dish can be a wrong choice. The ladder fetches
nothing. The four choices always have four different English and four different
Chinese names. Without a cached photo and three other names, the ladder starts
at the sums.

## Using the service

Create one `UnlockLadder` per lockout surface with that surface's
`AttemptBudget`, a function that reads School mode, and the catalog dishes.
`LockSession::budget()` gives the budget of a lock session.

| Call | Use |
| --- | --- |
| `status(now)` | Wait, attempts, current rung, wrong dishes, skips left and whether the ladder is offered. |
| `challenge(now)` | The live challenge for the current rung, or nothing when no ladder is offered. |
| `answer_dish(nonce, choice, now)` | Grades a dish answer. |
| `answer_sums(nonce, answers, now)` | Grades all ten sums. |
| `strike(nonce, cell, now)` | Grades one mole strike when it arrives and returns the live score. |
| `finish_round(nonce, now)` | Grades the round after its duration has passed. |

Pass `std::chrono::steady_clock::now()` as `now`.

## Verification

`tests/local_security/unlock_ladder_tests.cpp` covers each rung cleared and
lost, the fall to the clock, the hourly cap running out and refilling, a
replayed nonce, a forged nonce, expired questions, answers of the wrong kind,
strikes on empty cells, outside the round and repeated on one mole, the strike
limit, an early hand-in, abandoned rounds, School mode starting at the sums and
switching on mid-ladder, and that a cleared ladder leaves a lock session locked.
Build and run it on its own:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Isrc src/libslic3r/LocalSecurity/LocalSecurity.cpp src/libslic3r/LocalSecurity/UnlockLadder.cpp tests/local_security/unlock_ladder_tests.cpp -lcrypto -o unlock_ladder_tests
./unlock_ladder_tests
```

The `[DimSum][ladder]` case in `tests/dim_sum` checks the catalog adapter.

## Remaining work

No lockout surface shows the ladder yet. The School mode credential in
Preferences, the identity history password and the element lock prompt each
need the ladder UI with keyboard play, screen-reader names and live score
announcements, a countdown that does not rely on colour or motion, reduced
motion for the moles, and copy in all three language modes. None of this has
been run in a built Windows application.

Related articles: [local security components](README.md),
[native integration](native-integration.md).
