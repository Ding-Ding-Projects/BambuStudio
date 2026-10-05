# Scheduled settings

Preferences > Schedules lets a rule change the language mode, theme, density, accent color, font,
text size, the two funny levels or the app's display name at chosen times, from a versioned HTTPS
settings API, or from a Home Assistant switch. When a rule stops matching, the values you set
yourself come back. Every rule edit is an ordinary preferences save, so the preferences history
records it like any other settings change and can restore an earlier schedule.

Code: `src/slic3r/GUI/Schedule/ScheduledSettingsModel.hpp` (wx-free schema, matcher, precedence,
API contract, override bookkeeping), `ScheduledSettings.{hpp,cpp}` (timer, sources, application of
values), `ScheduledSettingsPanel.{hpp,cpp}` (the Preferences section and the rule editor).
Delivery status: recovered from the preserved implementation and adapted to the current interfaces.
This ultra-speed delivery runs no tests, lint, runtime launches, or screenshots.

## Behaviour

- Rules are evaluated when the app starts, every 60 seconds afterwards, and immediately after any
  rule edit or a toast's **Retry** action. The Schedules section shows when the last check ran.
- A rule matches when it is enabled, the current local time is inside its window, the day is one of
  its weekdays and the date is inside its optional date bounds.
- **Precedence:** rules are evaluated in list order and a later rule wins for any setting both set.
  Move a rule down to make it win. The list is the priority order; there is no separate priority
  field to drift from it.
- **Base settings:** the first time a rule takes a setting, the value you had is captured. When no
  rule claims that setting any more it is written back. The capture is stored beside the schedule
  (`scheduled_settings_override` in `BambuStudio.conf`), so a rule that ends while the app is closed
  is still unwound at the next launch. Remote and scheduled values are never written into that
  capture.
- While a rule owns a setting, that rule owns it: changing the value in Appearance or General while
  the rule is active is overwritten at the next check. Disable or edit the rule instead.
- An empty schedule, or no matching rule, changes nothing.

## Schema (`scheduled_settings`, schemaVersion 1)

```json
{
  "schemaVersion": 1,
  "rules": [
    {
      "id": "r-1a2b3c4d",
      "label": "Evening dark mode",
      "enabled": true,
      "startDate": "2026-09-01",
      "endDate": "2026-12-31",
      "startTime": "20:00",
      "endTime": "07:00",
      "weekdays": ["mon", "tue", "wed", "thu", "fri"],
      "source": { "kind": "local" },
      "values": { "dark_color_mode": "1", "ui_density": "compact" }
    }
  ]
}
```

- `id` is stable and never renumbered; `weekdays` is `"everyday"` or a list of `mon`..`sun`.
- `source.kind` is `local`, `api` (`url`, `allowLoopbackHttp`) or `homeAssistant` (`entityId`).
- `values` may only contain the allowlisted keys below. Anything else is dropped on read, so a
  hand-edited document cannot schedule a token, a path or a credential.
- Unknown fields on the document and on each rule survive a round trip, so a newer build's data is
  not lost when an older build saves.
- A document with a `schemaVersion` this build does not know, or that fails to parse, is left
  untouched on disk and the scheduler runs with no rules.

Allowed keys and accepted values:

| Key | Setting | Accepted |
| --- | --- | --- |
| `language` | Language mode | `en_US`, `yue_HK`, `bilingual_en_yue_HK` (any id of letters, digits, `_`, `-`) |
| `dark_color_mode` | Theme | `0` light, `1` dark |
| `ui_density` | Density | `comfortable`, `compact` |
| `ui_accent_seed` | Accent color | `#rrggbb`, or empty for the brand seed |
| `ui_font_family` | Font | installed face name, or empty for the default |
| `ui_font_scale` | Text size | number 0.8 to 1.4 (the picker offers 0.9 / 1.0 / 1.15) |
| `funny_level_en` | Funny level, English | `1` to `5` |
| `funny_level_yue` | Funny level, Cantonese | `1` to `5` |
| `app_display_name` | App name | up to 40 characters, no control characters; empty resets |

## Time semantics

All times are the computer's local wall clock, as the C runtime reports it. The section and the
editor show the current offset and whether daylight-saving time is in effect.

- `startTime < endTime`: the window is `[start, end)` on a listed weekday.
- `startTime > endTime`: the window crosses midnight. It starts on a listed weekday and runs into the
  next morning; the weekday and the date bounds are judged by the day the window **started**, so a
  Friday 22:00-06:00 rule covers Saturday 03:00 but not Sunday 03:00.
- `startTime == endTime`: all day on the listed weekdays.
- Date bounds are inclusive at both ends and optional at either end.
- **Daylight-saving changes** move the wall clock. A window that spans the change is one hour
  shorter or longer that night. A start minute that does not exist that night (the skipped hour) is
  treated as already passed, and an end minute that occurs twice ends the window at its first
  occurrence. Nothing is scheduled in UTC.
- A rule with no weekday selected never matches; the editor refuses to save it.

## Sources

**Local** rules apply their own values for the whole window.

**HTTPS API** rules read `GET <url>` every check while inside the window and apply the response:

```json
{ "schemaVersion": 1, "values": { "dark_color_mode": "1", "ui_density": "compact" } }
```

- Only `https://` is accepted, except `http://` to `localhost` / `127.x` / `::1` when the rule's
  *Allow plain http:// to localhost* option is on. Addresses carrying a user name or password are
  refused. Redirects are not followed and a 3xx answer is treated as a failure.
- The response is limited to 64 KiB and 10 seconds; it must be a JSON object with
  `schemaVersion: 1` and a `values` object. Keys outside the allowlist are ignored; a value that is
  the wrong type or outside its accepted range rejects the whole response.
- The rule's own ticked values are the fallback for keys the API leaves out; the API wins for keys
  it does send.
- Each request carries a generation number. An answer to an older request is dropped, so a slow
  reply can never overwrite a newer one.

**Home Assistant switch** rules read one entity (`input_boolean.*` or `binary_sensor.*`) through the
connection configured in Smart home. The existing bounded `GET /api/states` query filters to the
requested entity domain on its worker, and the scheduler selects the exact entity id. An entity
missing from a truncated query is treated as unavailable. `on`
applies the rule's ticked values; `off` releases them. The same generation guard applies.

A source that has not answered, answered with an error, or answered something other than `on`/`off`
contributes nothing, so a flaky server never flips settings back and forth on its own.

## Fallback and notifications

- The first failure in a streak posts a non-blocking warning toast naming the rule and the reason
  (HTTP status, refused redirect, rejected schema, missing Home Assistant token, unreachable host)
  with a **Retry** action. Later failures in the same streak update the rule's status line only.
- On source failure or an unknown state, that rule contributes no values and its affected settings
  return to their captured base unless another matching rule owns them.
- Remote values are never persisted as the user's base value. Turning a rule off, or its window
  ending, restores the captured base.
- The Schedules section shows, per rule, whether it is inside its window, what its source last said,
  and which settings it currently controls.

## Security

- Allowlist first: the model drops every key that is not one of the nine above before anything is
  stored or applied, on the document and on API responses alike.
- URL policy: HTTPS only, loopback HTTP only behind an explicit per-rule flag, no credentials in the
  URL, no redirects, bounded size and timeout.
- Home Assistant reads use the existing client: the token stays in `BambuStudio.conf` and is sent
  only over HTTPS or to a loopback address; entity ids are validated to `<domain>.<object_id>`
  before querying their domain.
- The evaluator writes only the nine allowlisted keys. Paths, tokens and printer credentials are not
  reachable from a schedule, an API answer or a Home Assistant state.
- Nothing about the schedule leaves the machine except the API `GET` and the Home Assistant
  `GET`; neither carries settings.

## Interaction with other features

- Language mode: the new catalogs load for windows built from then on; the running frame keeps its
  strings until the next launch, exactly as the language picker in General documents.
- Theme, density, accent, font and text size use the same live fan-out as the Appearance section.
- Funny levels and the app name apply live.
- The preferences history snapshots the schedule with every save; restore an older snapshot to get
  an older schedule back.

## Delivery evidence and remaining verification

The preserved source was recovered selectively, without the unrelated Home Assistant settings-sync
feature or its lifecycle hooks. The Schedules section is wired into Preferences, the scheduler starts
after preferences-history setup, and shutdown invalidates pending replies before Home Assistant teardown.

Tests, lint, runtime interaction, installer execution, and screenshots are intentionally not run in
this ultra-speed pass. Build and packaging results belong to the release coordinator's pinned candidate.
This article does not claim the restored interface has been runtime verified.

## 廣東話使用說明

喺偏好設定打開「Schedules」，新增或編輯規則，揀日期、星期同開始及結束時間。
結束時間早過開始時間代表跨午夜；兩個時間相同代表全日。清單較後嘅規則有較高優先權。
規則可以用本機設定、HTTPS 設定 API，或者 Smart home 入面已連接嘅 Home Assistant 開關。
來源未能回覆或狀態唔明確時，該規則唔會提供設定值；冇其他規則接管嘅設定會還原。
關閉規則或時間段結束時，原本設定會返嚟。設定同還原記錄會保存在本機設定檔。
今次快速交付冇跑測試、冇啟動介面、冇截圖；唔應該將原始碼恢復當成已驗證運行。
