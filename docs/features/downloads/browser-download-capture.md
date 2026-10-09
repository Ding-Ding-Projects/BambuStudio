# Browser download capture

The `browser-extension` folder holds a Manifest V3 extension for Chromium browsers (Google Chrome and Microsoft Edge). When a 3D model download starts in the browser, the extension pauses it and asks Bambu Studio MD3 to take it over. The browser cancels its own copy only after Bambu Studio MD3 answers that it has queued the download for its Start download dialog, where the file, the source and the destination are shown and nothing is transferred until you confirm. Any other answer, or no answer, lets the browser finish the download as usual. The application's side is not in this build yet; see the next section.

## What this build contains

- The extension, its settings page, its capture rules and its handoff messages, in English, Hong Kong Cantonese and both together.
- A Windows installation places the extension in a `browser-extension` folder beside `bambu-studio.exe`.

The application side of the handoff is not in this build yet: the browser connection (the native messaging host and its registration for Chrome and Edge), the Start download dialog, the Downloading window and the Download complete notice. Until they exist, **Check connection** reports that the browser connection is not installed, and every captured download continues in the browser after the extension records why. No download is lost and none is claimed as handed over.

## Install the extension

1. Open `chrome://extensions` in Chrome or `edge://extensions` in Edge.
2. Turn on **Developer mode**.
3. Choose **Load unpacked** and select the `browser-extension` folder. In an installed copy it is `%LOCALAPPDATA%\BambuStudioMD3\app-<version>\browser-extension`. Each update installs a new `app-<version>` folder and later removes the old one, so copy the folder somewhere permanent and load the copy, or load the folder again after an update.

The extension ID is always `beapempohkjjpcjdfojdlngcbamofiok`. The manifest carries a public key, so every browser derives the same ID, and the browser connection lists exactly this ID as an allowed origin. The extension cannot run in private windows (`"incognito": "not_allowed"`).

## What happens when a download starts

The service worker listens to `chrome.downloads.onDeterminingFilename`. While a listener holds that event, the browser cannot finish the download, so the decision is made before any file is saved.

1. **Decide.** A download is captured when capture is on, it is in progress, it is not in a private window, no other extension started it, its address is `http` or `https` without a user name or password and at most 8,192 characters long, neither the page's site nor the file's site is excluded, and its name ends in a file type that is turned on. When the name has no known ending, a model MIME type (`model/3mf`, `model/stl`, `model/step`, `model/obj` and similar) decides; `application/octet-stream` never does. Anything else is released at once and leaves no trace.
2. **Pause.** The browser's own transfer is paused before anything is sent. If the browser refuses to pause, nothing is sent and the browser keeps the download.
3. **Hand over.** The extension opens a native messaging port to `io.github.ding_ding_projects.bambustudio_md3` and sends one capture message (below).
4. **Queued.** Only the answer `queued` for the same capture makes the extension cancel the browser's download and remove it from the browser's download list. Bambu Studio MD3 then owns the download and must ask before transferring anything.
5. **Kept.** A missing or refusing browser connection, a declined capture, an answer for another capture, an answer the extension cannot read, or no answer within 30 seconds resumes the browser's transfer. The browser finishes it as usual (including its own Save As question, if you turned that on), and the extension shows a notification naming the file and the reason.

The extension never starts a transfer itself and never sends cookies, page content or the page's query string. Bambu Studio MD3 downloads the address itself, without the browser's cookies or sign-in, so a site that serves files only to a signed-in browser session, or whose signed addresses expire within moments, may fail there. Add such a site to the excluded list to keep its downloads in the browser.

### Link menu

Right-clicking a link whose address ends in a model file type offers **Open link in Bambu Studio MD3**. This explicit request ignores the capture switch, the file type choices and the excluded sites, but keeps the address checks. A link that cannot be handed over always shows a notification, whatever the notification setting.

## Settings page

Click the extension's toolbar button, or choose **Extension options**, to open the settings page. Every change is saved at once and announced in the page's status line.

| Setting | Default | Effect |
| --- | --- | --- |
| Hand model downloads to Bambu Studio MD3 | On | Off leaves every download in the browser; the toolbar button then shows an "off" badge |
| File types to hand over | 3MF, STL, STEP, OBJ, AMF and OLTP on; G-code, SVG, glTF and FBX off | Websites also offer drawings, scenes and G-code for other programs, so those start off |
| Never hand over downloads from these sites | Empty | One site per line; `*.example.com` covers the site and every address under it; pasted addresses are reduced to their site; lines that are not sites are named and not saved |
| Tell me when the browser keeps a download | On | Notifications for automatic captures that stay in the browser |
| Language of this extension | Same as the browser | English, Hong Kong Cantonese, or English with Cantonese below it |

The **Connection** section shows the browser connection name and the extension ID with a copy button, and **Check connection** asks the browser connection to answer. The **Recent handoffs** list keeps the last 25 decisions (time, file, site, outcome and reason) in the browser's local extension storage only; **Clear the list** empties it.

The page follows the system's light or dark appearance, high-contrast colours and reduced-motion setting, reflows to a single column in narrow windows, and gives every control a visible label and keyboard focus ring. The capture switch is announced as a switch.

### Language

Chrome and Edge choose an extension's language from their own interface language, and neither offers Cantonese, so the extension reads both of its catalogues (`_locales/en` and `_locales/yue_HK`) itself and follows the choice on the settings page. **Same as the browser** uses Cantonese for a Cantonese or Hong Kong Chinese browser and English otherwise. The choice applies to the settings page, the link menu, the toolbar title and badge, and notifications. The browser's extension list shows the English name and description.

## Permissions and privacy

The extension asks for `downloads`, `nativeMessaging`, `contextMenus`, `notifications` and `storage`, and for no site access. It loads nothing from the network, contains no analytics and runs no generated code. Settings and the recent handoffs list stay in `chrome.storage.local` in the browser profile.

## Handoff messages

Every message is JSON over Chrome native messaging, one message and one reply per port.

A capture message:

```json
{
  "type": "capture", "protocol": 1, "captureId": "2b0f…", "origin": "download",
  "url": "https://cdn.example/files/benchy.3mf?sig=abc",
  "source": "https://models.example/item/123",
  "fileName": "benchy.3mf", "modelType": "3mf", "mime": "application/octet-stream",
  "totalBytes": 1048576, "capturedAt": "2026-10-09T08:30:00.000Z"
}
```

`origin` is `link` for the link menu. `url` has its fragment removed. `source` is the page's scheme, host and path only, or `null`. `fileName` has no folder parts, no control characters and none of `<>:"/\|?*`, is at most 200 characters, and gains the type's suffix when the browser's name lacks one. `totalBytes` is `null` when unknown.

The reply names the same `captureId`:

```json
{ "type": "capture-result", "protocol": 1, "captureId": "2b0f…", "status": "queued", "queueItemId": "…" }
{ "type": "capture-result", "protocol": 1, "captureId": "2b0f…", "status": "declined", "reason": "busy" }
```

`queued` means the item waits behind the Start download dialog and nothing has been transferred. Decline reasons are `busy`, `disabled`, `invalid`, `shutting-down` and `unsupported`; any other reason is reported as a refusal without a known cause.

**Check connection** sends `{ "type": "hello", "protocol": 1 }` and expects `{ "type": "hello", "protocol": 1, "app": "Bambu Studio MD3", "version": "…" }`. A different protocol number is reported as a version mismatch.

## Verification

- `node --test ui-md3/tests/browser-extension.test.mjs` checks the manifest, the fixed extension ID, the icons, the absence of network loading and generated code, both catalogues (same keys and substitutions, written Cantonese, no unused or missing messages), every capture rule, the messages, the replies, and the exact order of pause, handoff, cancel and resume against a recording stand-in for the `chrome` API, including timeouts, refusals, logging and notifications.
- `node --test ui-md3/tests/browser-extension-chromium.test.mjs` loads the extension into a real Chromium with a stand-in browser connection registered for that test profile only, and clicks real download links. It checks that a queued model leaves no file in the browser, that a declined handoff or a missing connection lets the browser save the whole file, that other downloads never reach the connection, that switching capture off leaves models alone, and that the settings page checks the connection and switches to Cantonese. Set `CHROMIUM_PATH`, or `PLAYWRIGHT_BROWSERS_PATH` to a Playwright browser folder; without one the test is skipped. Branded Chrome builds ignore `--load-extension` and are not used.

Neither test exercises Bambu Studio MD3 itself. Loading the extension in Chrome or Edge on Windows and the full flow through the application's Start download, Downloading and Download complete surfaces still have to be observed once the application side exists.
