# Browser event narration

Settings contains this page's own event narrator. It is off by default. The language choice is English, Hong Kong Cantonese, or Both. Both queues English followed by Cantonese and never starts the second track while the first is still speaking. Informational repeats have a 15-second category cooldown; queued superseded informational copy is replaced. Errors and warnings are not dropped by that cooldown. The queue has a 12-track resource bound, and an individual track is accepted only at 2,000 characters or fewer; long copy is not silently truncated into a misleading spoken claim.

Each language has its own live voice picker, with an independent adjacent search and anchored regex builder. Choose automatically is the default. Installed voices are enumerated at runtime and refreshed on `voiceschanged`, so an initially empty enumeration does not become a permanent no-voices verdict. The saved choice uses `voiceURI`, not a display name. If the voice is removed, the choice remains saved and the status identifies the fallback. Cantonese accepts `yue` language tags and `zh-HK`; it does not substitute a Mandarin voice.

Rate ranges from 0.1 to 10 and pitch from 0 to 2, both defaulting to 1. Invalid persisted values fall back to normal delivery and numerical bounds are clamped before playback. Local voices are preferred by automatic selection. Network-backed voices are clearly disclosed beneath the picker, including that the platform service handles spoken text and may be unavailable offline. The page itself makes no speech-related fetch and does not claim that a network-backed platform voice works offline.

Pause cancels current and queued speech. Browsers do not reliably expose an active-screen-reader detector, so this explicit accessible control is the documented browser equivalent for yielding narration to assistive technology or quiet time. Stop clears the queue without changing the enabled preference. Turning narration off clears the queue. Late completion callbacks from cancelled speech cannot restart it. Teardown removes the voice event subscription.

The settings search indexes narrator labels, descriptions and current controls. User choices persist in the site's existing per-visitor preference store. Command-palette indexing, shared School-mode integration and local version-history recording are separate unfinished browser-surface requirements. Native operating-system speech settings and shared credentials are not read through an invented network bridge.

Verification: `node --test ui-md3/tests/site-narration.test.mjs ui-md3/tests/site.test.mjs` passed 33 tests when this increment was prepared, including 9 narrator behavior cases. Real-browser playback, keyboard, responsive-layout and capture evidence remain unverified because the approved hidden launch seam exited before a target or live process could be observed. Source tests are not substituted for that evidence.

## Suggested articles

- [Local personal wording](personal-wording.md)
- [Language modes and tone](language-and-funny-levels.md)
- [Settings and appearance](settings-and-appearance.md)
