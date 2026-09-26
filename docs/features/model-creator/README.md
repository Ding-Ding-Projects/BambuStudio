# Native Model Creator

Model Creator converts a natural-language description into a bounded version 1 scene specification, renders a mesh with OpenSCAD or Blender, and retains each completed revision under the user's local application-data directory. The user explicitly selects a provider and model. The dialog offers Claude CLI, Codex CLI, Anthropic API, and OpenAI API. It never imports the generated mesh until **Add to plate** is pressed.

## Scene contract

The model response must be a single JSON object with `version: 1`, a title of at most 80 bytes, and 1 to 32 parts. Each part is a `box`, `cylinder`, or `sphere`, with millimeter dimensions and a three-number position. Boxes take a three-number size, cylinders take a height and radius, and spheres take a radius. Round primitives may specify bounded facets. Dimensions and positions have fixed bounds. Solids must fit above the build plate. Unexpected keys, executable content, URLs, and other schema versions are rejected. The response is capped at 64 KiB. Provider text never becomes a script: the application emits fixed OpenSCAD or Blender code from accepted numeric values.

The generated STL is capped at 100 MiB. A completed revision keeps its validated source, mesh, and bounded local metadata for reopening after restart. The dialog reloads up to 100 retained revisions and revalidates each mesh. Canceled requests never reach the plate. The renderer executable is an explicit local selection. Model Creator launches it with a fixed argument list and a timeout. A renderer failure leaves its files for inspection without offering the mesh for import.

## Provider credentials and privacy

API keys are stored only through Windows Credential Manager, with Save and Clear controls. The key is not written to a project, process argument, generated scene, response file, or diagnostic output. API requests use HTTPS, bounded responses, a connection timeout, a total timeout, and cancellation. Claude Code CLI and Codex CLI processes run in an isolated revision directory, receive the request through standard input, and use restricted tool or sandbox flags. The temporary request and response files are removed after the process ends. Local CLI configuration and provider policies still govern what data the selected provider receives. The dialog identifies the selected provider and model before submission.

## Integration

`ModelCreatorDialog` is a self-contained native dialog. Add a navigation command that constructs it with an `AddToPlate` callback. The callback receives a validated local STL path only after the user presses **Add to plate**. The host can pass that path to `Plater::load_files` and refresh the 3D view. Keep import and project-history updates in the existing application owners. The dialog's **Preview mesh** action opens the retained mesh with the system's registered viewer; an embedded preview can be connected at the navigation seam when a reusable viewer is available.

## Verification and limits

The focused C++ test covers valid scenes, dimension and version rejection, unexpected executable fields, build-plate bounds, and fixed renderer output. A complete app build and real renderer drive are required before claiming the entire flow works. Models composed of disconnected primitives may need support, orientation, or boolean operations before printing. The preview is a geometry review aid, not a printability guarantee. No arbitrary provider-generated source code is executed.
