# Hardware fit evidence

The Models section of the local Ollama suite gives every model one of four verdicts: **Runs well**, **Runs with limits**, **Unlikely** or **Unknown**. A verdict is an estimate built only from measured facts and published model metadata. It never promises that a download will run, and it never reads anything from a model's name.

## What is measured

Choose **Refresh runtime** or **Measure hardware again** in the Models section. The panel under the buttons then lists every fact a verdict uses, with the time it was measured:

- **System memory**: the available and total physical memory reported by Windows.
- **Processor architecture**: x86_64 or arm64.
- **Graphics adapters**: each hardware adapter's name, driver version and dedicated memory, read through DXGI. Software adapters are ignored. Usable memory is the dedicated memory capped by the budget Windows currently allows, so memory other programs hold is not counted.
- **GPU backend**: whether Ollama can use the GPU. Only Ollama itself can prove this, so the suite records what Ollama reports for a loaded model (GET `/api/ps`): how many bytes of that model it holds in GPU memory. The record is kept with the adapters, driver versions and Ollama version it was seen with. A driver update, a different adapter or a new Ollama version makes it no longer apply.
- **Model folder and free space**: where Ollama stores models and how much space is free there.

To give Ollama a chance to report GPU use, load a model (for example, send one chat message), then choose **Refresh runtime** while it is still loaded.

## Finding the model folder

Ollama stores models in the folder named by the `OLLAMA_MODELS` setting, or in `.ollama\models` inside your user profile when that setting is absent. The suite checks the user setting, the computer setting and this application's own environment, and the default location.

A folder counts as confirmed when the manifests of the installed models that Ollama listed are found inside it. When nothing is installed yet, the folder named by the setting is used; if the setting's views name different folders, the smallest free space among them is used. When models are installed but none of those folders holds them, the folder stays unconfirmed and storage fit is **Unknown**. Check `OLLAMA_MODELS` for the running Ollama, restart Ollama, and choose **Refresh runtime**.

## Estimate settings

Two pickers next to the hardware panel set the context memory the estimate includes:

- **Context for estimates**: 2048 to 131072 tokens. 4096 is the recommended starting point. A value above a model's verified context limit is reduced to that limit for that model.
- **Context cache precision**: 16-bit (the default and the largest), or the 8-bit and 4-bit values of `OLLAMA_KV_CACHE_TYPE`. Choose the value you run Ollama with.

The settings are saved in the suite's own data folder. Changing either one recomputes every verdict immediately.

## How a verdict is reached

1. The model's exact size and the model folder's free space are required. A model that is not installed needs its size plus a ten percent allowance; less free space than that is **Unlikely**.
2. Memory evidence is required next: measured system memory, the processor architecture, and the model's parameter count, quantization, context limit and attention layout from `/api/show`. A model that is not installed has none of the runtime metadata yet, so its memory fit is **Unknown** until it is installed and inspected.
3. The memory estimate is the model size, plus twenty percent for the runtime, plus the context cache. The context cache is the number of tokens × layers × key/value heads × (key + value dimensions) × bytes per value of the chosen precision.
4. With Ollama's GPU evidence: the estimate fits in usable GPU memory → **Runs well**; it fits in system memory → **Runs with limits** (partial offload, speed not predicted); otherwise **Unlikely**.
5. Without that evidence the GPU is not counted: the estimate fits in system memory → **Runs with limits** on the processor; otherwise **Unknown**.

Missing metadata is never treated as zero. The detail pane lists the verdict, the storage and memory figures, the context and its cache size, every reason that applied and the measurement time. Launch preflight for a local server profile uses the same assessment with the profile's own context.

## Verification

`tests/ollama_suite_model/ollama_suite_model_tests.cpp` covers the context-cache arithmetic, manifest locations, folder confirmation, backend evidence and its invalidation, all four verdicts across memory, GPU, disk and setting changes, and that a name alone never changes a verdict. `tests/ollama_suite_model/ollama_fit_wiring.test.mjs` checks that the dialog and client feed those measurements to every verdict. The DXGI and registry reads run only on Windows and still need to be observed in a built application.
