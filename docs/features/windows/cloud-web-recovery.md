# Cloud web-page failure recovery

## Behaviour

MakerWorld, MakerLab, and Print History no longer replace a usable page with the legacy full-page
`disconnect.html` document when a cloud sign-in ticket request or navigation fails. The last usable
page remains mounted. A persistent, non-modal native information bar identifies whether the failure
was caused by the network, cloud authentication, a missing route, secure-connection validation, or
a temporarily unavailable service. Its keyboard-accessible **Retry** action repeats the failed
request, and a successful navigation dismisses the warning.

If the first remote load fails and there is no usable remote document to retain, the local Home
surface remains visible under the same information bar. Other local navigation therefore remains
usable while the cloud is unavailable. The compatibility disconnect document remains in the
resource bundle for older callers, but its retry control is now a semantic 44-pixel button and its
error region is announced to assistive technology.

## Configuration and data handling

There is no preference or credential stored by this feature. Retry keeps only the failed URL or the
ticket-request callback in the owning `WebViewPanel`; both disappear with the panel. Error details
are not sent to another service, and no authentication ticket is written to the notification.

## Failure modes

- A connection failure reports that the network is unavailable.
- A failed ticket or authentication navigation reports a cloud sign-in failure.
- `wxWEBVIEW_NAV_ERR_NOT_FOUND` reports that the requested cloud page was not found; it is not
  mislabeled as a network outage.
- Certificate and security failures use secure-connection wording and retain the current page.
- User-cancelled navigation remains silent and does not raise a false error.

Warnings remain visible until dismissed or until the affected WebView completes a non-blank
navigation. Retry failures update the same bar instead of stacking dialogs or replacing content.

## Verification

- `deviceweb_home_webview_failure_policy_tests` exercises every policy category.
- `deviceweb_home_webview_failure_contract` proves the native integration has no
  `MakeDisconnectUrl`/`LoadURL(disconnect.html)` replacement path, includes network/auth/not-found
  copy, installs the nonblocking Retry action, and preserves accessible compatibility markup.
- `node resources/web/data/validate-text-locales.mjs` validates the bundled Home-page localization
  tables, including English/Cantonese bilingual network copy.
