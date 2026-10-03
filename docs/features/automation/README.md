# Automation

The bundled automation companion exposes the running Bambu Studio instance and
isolated headless slicing through MCP and a matching command-line interface.

- [MCP and CLI setup](mcp-and-cli.md)
- [Command and transport reference](../../../automation/README.md)
- [Hosted native interface verification](native-interface-verification.md),
  parameterized native-input scopes with encrypted evidence and explicit gaps.
- [Hosted managed verification receipt](hosted-verification.json), 24 passing
  cases at the recorded source SHA, with native runtime and hardware excluded.
- [Expanded managed verification receipt](hosted-verification-ce61d22e.json),
  27 passing cases at `ce61d22e`, with native runtime and hardware excluded.

The HTTP surface implements MCP rather than a general REST API. Use an MCP
client for protocol negotiation and tool discovery. A Postman collection is not
provided because it would duplicate the protocol client and does not cover stdio.

Related: [Windows release supply chain](../releases/windows-release-supply-chain.md).

- [Hosted display resolution receipt](hosted-display-resolution-37091384649.json),
  seven passing contracts plus measured 1920x1080/125% provisioning and full
  restoration; application DPI and pixels are excluded.
- [Contained source-query receipts](hosted-source-query-b5468208.json), the failed
  creation case and repaired 10/10, 11/11 and 13/13 hosted lifecycle results,
  with original receipt hashes; the last provisioning job failed independently;
  no Settings mutation or application-rendering proof is claimed.
- [Cancellation observation contract](hosted-cancellation-contract-37095515540.json),
  18 passing hosted cases including deliberate epoch-validation mutation;
  this verifies evidence rules, not an installed cancellation interaction.
- [Hosted 150% display receipt](hosted-display-resolution-37096062899.json),
  13 passing lifecycle cases, measured 144 DPI at 1920x1080, and verified
  restoration to the original 1024x768 and 96 DPI. Application rendering is excluded.
- [Hosted 200% display receipt](hosted-display-resolution-37096376814.json),
  14 passing lifecycle cases, measured192 DPI at1600x1200, and original-state
  restoration. Product rendering remains independently unverified.
- [Bounded launcher diagnosis](hosted-startup-diagnostic-37096825183.json),
  native initialization error 1114 after Mesa loaded, before the product entry point;
  the failing module remains unidentified and no runtime success is claimed.
