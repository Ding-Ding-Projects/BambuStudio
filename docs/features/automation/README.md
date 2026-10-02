# Automation

The bundled automation companion exposes the running Bambu Studio instance and
isolated headless slicing through MCP and a matching command-line interface.

- [MCP and CLI setup](mcp-and-cli.md)
- [Command and transport reference](../../../automation/README.md)
- [Hosted managed verification receipt](hosted-verification.json), 24 passing
  cases at the recorded source SHA, with native runtime and hardware excluded.
- [Expanded managed verification receipt](hosted-verification-ce61d22e.json),
  27 passing cases at `ce61d22e`, with native runtime and hardware excluded.

The HTTP surface implements MCP rather than a general REST API. Use an MCP
client for protocol negotiation and tool discovery. A Postman collection is not
provided because it would duplicate the protocol client and does not cover stdio.

Related: [Windows release supply chain](../releases/windows-release-supply-chain.md).
