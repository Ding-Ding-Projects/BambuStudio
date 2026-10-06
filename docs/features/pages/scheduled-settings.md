# Scheduled visitor settings

[廣東話](scheduled-settings.yue_HK.md)

The Settings tab provides local rules with a name, enabled state, priority, optional inclusive
dates, selected weekdays or every day, and explicit all-day or start/end times. New rules are
disabled. All rules use the selected IANA time zone. Start is inclusive, end exclusive. Overnight
rules belong to their starting date and weekday. Repeated daylight-saving wall times match twice;
skipped wall times do not occur.

Typed targets cover language, independent tone levels, theme, density, accent, font family,
scale and weight, registered element-style snapshots, message decoration, five attention options,
and narration enablement, pause, language, rate and pitch. This initial registry does not claim
support for future settings or native controls. Matching rules apply ascending priority then
stored order, with the last value winning per setting. Base choices remain intact and return
when the schedule ends. Expired element CSS properties are removed. Evaluation happens at startup,
on changes and every 30 seconds while the page is open, never while the browser is closed.

Each list and selector has adjacent regular-expression search. Deletion requires scoped two-key
slider confirmation. Version-1 JSON import rejects duplicate/unknown fields, invalid dates,
times, zones or typed values, unregistered targets, more than 128 rules and files over 262144
bytes. Invalid import preserves the last valid state; failed storage rolls back before applying
overrides. Filenames and paths are not retained. Exports include rule labels and selected values,
so review them before sharing.

API and Home Assistant remain visibly unavailable pending integrated native pairing and a
credential vault. The browser creates no network bridge and stores no API secrets. Local storage
does not represent shared School mode or native credentials.

`node --test ui-md3/tests/site-schedule.test.mjs` has 13 focused cases covering overnight dates,
precedence, daylight saving, strict import, base restoration, stale CSS removal and storage
rollback. These are source-level checks; actual hidden-browser interaction and captures remain
pending the reviewed runtime route.
