# MCP automation continuation

Implement Bambu Studio MCP support under issue #53 and progress discussion #54.
The feature branch is `feature/mcp-integration`; the primary branch is `main`.
The verified remote candidate before this record is
`65dc4577fb29f2d00fd525a1de11e370784c5b3e`.

Implemented: native current-user pipe bridge; project/model/settings/preset
control; generation-bound native slice jobs; durable printer-start intent
deduplication; native printer controls; self-contained .NET companion; 20 typed
tools; stdio and authenticated HTTP; shared JSON CLI; isolated headless slicing;
Squirrel payload staging; source/hash package checks; focused hosted verification;
strict encrypted runtime evidence producer and reader.

Hosted run 37045639569 passed 24 managed checks at that exact candidate, plus
documentation parsing and self-contained publication. Native compilation was
still running when this record was written. Earlier focused failures were fixed;
superseded native runs were canceled before unverified release publication.
The release job now requires both native and managed checks.

Additional source in this checkpoint introduces a dedicated automation evidence
recipient, preserving historical GUI recipients. Its protected private key is
kept outside version control. Only the public PEM is tracked. Hosted native
runtime, capture integrity and pixel review remain pending. Physical printer
behavior is unverified; AMS and dual-nozzle starts are explicitly unsupported.

Constraints: no local product build, test, packaging, installation, slicing or
application execution. Perform product verification only on hosted Windows
runners. Administrative cryptographic evidence handling uses the protected
current-user key, never secret arguments or published key material. No arbitrary
shell or raw printer-command interface. No automatic print replay after uncertain
submission. Preserve unrelated work and do not force-push.

Next: read the exact hosted native result, repair only evidenced failures,
publish the coherent candidate, verify the installed package at its exact SHA,
inspect genuine restricted captures, update factual documentation, integrate
completed work into main and prove its remote SHA. Keep incomplete work retained.
The wiki update is published at 205bfff417ee03c8fe864b80f7c5cd113542db9d on its
host-required master branch. Project-board access lacks read:project scope;
authenticated shared-status enrollment is unavailable. Neither limits source
implementation or hosted verification. Do not claim final completion yet.
