# Native Model Creator

Model Creator converts a natural-language description into a bounded version 1 scene specification, renders a mesh with OpenSCAD or Blender, and retains each completed revision under the user's local application-data directory. The user explicitly selects a provider and model. The dialog offers Claude CLI, Codex CLI, Anthropic API, and OpenAI API. It never imports the generated mesh until **Add to plate** is pressed.

## Scene contract

The model response must be a single JSON object with `version: 1`, a title of at most 80 bytes, and 1 to 32 parts. Each part is a `box`, `cylinder`, or `sphere`, with millimeter dimensions and a three-number position. Boxes take a three-number size, cylinders take a height and radius, and spheres take a radius. Round primitives may specify bounded facets. Dimensions and positions have fixed bounds. Solids must fit above the build plate. Unexpected keys, executable content, URLs, and other schema versions are rejected. The response is capped at 64 KiB. Provider text never becomes a script: the application emits fixed OpenSCAD or Blender code from accepted numeric values.

The generated STL is capped at 100 MiB. A completed revision keeps its validated source, mesh, and bounded local metadata for reopening after restart. The dialog reloads up to 100 retained revisions and revalidates each mesh. Canceled requests never reach the plate. The renderer executable is an explicit local selection. Model Creator launches it with a fixed argument list and a timeout. A renderer failure leaves its files for inspection without offering the mesh for import.

## Provider credentials and privacy

API keys are stored only through Windows Credential Manager, with Save and Clear controls. The key is not written to a project, process argument, generated scene, response file, or diagnostic output. API requests use HTTPS, bounded responses, a connection timeout, a total timeout, and cancellation. Both APIs request the shared JSON schema through their structured-output options, and the bounded parser checks the result again.

Claude Code CLI and Codex CLI processes use an isolated job home and configuration folders. They receive the request through standard input and use their structured-output schema options. Claude Code CLI runs in safe mode with tools and MCP configuration disabled. Codex CLI ignores user configuration and rules, disables available external features, and uses a read-only sandbox. Each CLI retains its existing sign-in location through its supported configuration-directory interface, without copying credential bytes. The Codex CLI does not offer a single documented flag that disables every built-in capability, so its read-only sandbox and disabled external features remain defense in depth rather than a claim of zero available tools. The dialog identifies the selected provider and model before submission.

Every CLI and renderer process starts suspended, joins a Windows Job Object, and is resumed only after process-tree containment succeeds. Cancellation and timeout terminate the job and wait for its descendants. Temporary request, response, schema, and diagnostic files are removed on normal completion, error, and cancellation. An abrupt application exit can leave them behind; reopening the dialog removes scratch files older than 24 hours from revision directories, while leaving recent files alone to avoid disturbing an active generation in another dialog.

## Integration

`ModelCreatorDialog` is a self-contained native dialog. Add a navigation command that constructs it with an `AddToPlate` callback. The callback receives a validated local STL path only after the user presses **Add to plate**. The host can pass that path to `Plater::load_files` and refresh the 3D view. Keep import and project-history updates in the existing application owners. The dialog's **Preview mesh** action opens the retained mesh with the system's registered viewer; an embedded preview can be connected at the navigation seam when a reusable viewer is available.

## Verification and limits

The focused C++ tests cover valid scenes, dimension and version rejection, unexpected executable fields, build-plate bounds, fixed renderer output, isolated process environments, and descendant termination on timeout and cancellation. A complete app build and real provider and renderer drive are required before claiming the entire flow works. Models composed of disconnected primitives may need support, orientation, or boolean operations before printing. The preview is a geometry review aid, not a printability guarantee. No arbitrary provider-generated source code is executed.
