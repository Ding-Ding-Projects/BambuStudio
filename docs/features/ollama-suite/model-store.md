# Model Store filters and explanations

The Models section lists every variant of the last verified official catalog together with every model installed on this computer. Neither set hides the other. Search, filters, grouping and sorting work on that whole variant-level inventory.

## Search

The search field matches the model name, its architecture family, its quantization and its verified capabilities. Plain text is the default; the adjacent regex builder switches the same field to an anchored regular expression.

## Filters

Every filter is a picker filled from the inventory itself, never a free-text field. Each starts at **Any**, and **Clear filters** returns them all to **Any** in one action.

| Filter | Values |
| --- | --- |
| State | Running (loaded in memory now), Installed (includes running models) or Catalog only. |
| Family | The model name before the colon in `family:tag`, for every entry in the inventory. |
| Variant | The published tags of the chosen family. With no family chosen the picker is disabled, and its tooltip says to choose a family first: one family has tens of tags, the whole catalog thousands. |
| Capability | Every capability Ollama reported for an installed model, plus **Capabilities not verified**. |
| Quantization | Every quantization Ollama reported for an installed model, plus **Quantization not verified**. |
| Size | Under 2 GiB, 2 to 8 GiB, 8 to 32 GiB, over 32 GiB, or **Size not known yet**. |
| Hardware fit | Runs well, Runs with limits, Unlikely or Unknown, computed for every entry from the current hardware measurements and estimate settings (see [Hardware fit evidence](hardware-fit.md)). |

Capability, quantization and size come only from verified metadata. **Refresh runtime** reads `/api/show` for every installed model so their capabilities, quantization and attention layout are known. A catalog entry has no such metadata until it is installed, so it is listed under the **not verified** values and its size is unknown until you select it, when its exact size is read from the official registry. Nothing is inferred from a model's name: a catalog entry whose name suggests images is still **Capabilities not verified**.

## Grouping and sorting

**Group** arranges the list under headings by family, state, hardware fit, quantization or size. Each heading shows its group and how many variants it holds. Groups appear in a useful order: running before installed before catalog only, best fit first, smallest size first, families and quantizations alphabetically with unverified values last.

**Sort** orders the variants within each group by name, family, size (smallest or largest first) or hardware fit (best first). Unknown sizes always sort last in both directions, because they are unmeasured rather than small.

## Explanations before you choose

- The line under the filters says how many variants are shown out of the whole inventory, then explains each active filter: what the state means, what a family and variant are, what the chosen capability does, how many bits per weight the chosen quantization uses and what that costs in quality, how much space the size band needs, and what the chosen verdict means.
- Selecting a heading explains its group the same way.
- Selecting a variant immediately explains it before any further step: its family and exact tag, its state, each verified capability, its quantization, its size and the free space it needs in the model folder (the size plus a ten percent allowance), and what its current hardware-fit verdict means. Inspection then adds the full evidence behind the verdict.

## Verification

`tests/ollama_suite_model/store_query_tests.cpp` covers every filter alone and combined with search, the unverified facets, each grouping with its counted headings and order, the sort orders with unknown sizes last, and the picker values drawn from the inventory. `tests/ollama_suite_model/ollama_store_wiring.test.mjs` checks the native pickers, their accessible names, the disabled variant picker's stated condition, the explanations and that no model name is hard-coded or searched for meaning.
