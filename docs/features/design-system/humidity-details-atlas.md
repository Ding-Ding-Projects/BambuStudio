# Studio Atlas humidity detail surfaces

This source-only unit covers `AmsHumidityTipPopup` and `AmsHumidityLevelList` in `AmsMappingPopup.cpp`, and `uiAmsPercentHumidityDryPopup` in `DeviceTab/uiAmsHumidityPopup.cpp`, with their declarations. It does not change mapping, nozzle selection, drying commands, printer capabilities or the callers that choose a humidity surface.

## Reachable routes and preserved state

The native `Widgets/AMSControl.cpp` handler for `EVT_AMS_SHOW_HUMIDITY_TIPS` and embedded `DeviceWeb/ViewModels/DevicePage/AmsControlWeb/ViewModelActions.cpp::open_humidity` both retain their existing routes: classic AMS uses the level popup; remote-drying-capable N3F/N3S uses the independently owned drying dialog; other supported types use the percentage/temperature/time dialog. Both caller files are unchanged.

The valid-level condition, actual level assignment, image choice, percentage, rounded temperature, remaining-hours/minutes formatting, Drying/Idle conditions and stored AMS identity are unchanged. No new readings, units, message keys, timer, callback, command or persistence route are introduced. Existing initial placeholder behavior is preserved, not reclassified as confirmed telemetry.

## Composition and layout ownership

The classic popup has a persistent measured heading and its existing close affordance outside a real `MD3ScrolledWindow` body. The body holds the current-level image, the five-level legend and the existing explanatory text. It is bounded to the owning display work area with 32 DIP total clearance. Width is at most 740 DIP. Text wraps to the available body width, allowing for density padding and the vertical scrollbar. Creation, reopening and DPI rescale use the same layout method. Existing close hit geometry and dismissal remain unchanged.

The legend replaces its fixed 680-by-104 DIP bitmap-backed strip with a neutral container, separate measured Dry/Wet endpoint lines and rows of the original five level images. Column count comes from the available width, actual bitmap width and active-density gap. Images retain the original five-to-one sequence and light/dark selection. The measured height follows row count and actual endpoint text height. Downward scale and width changes replace the earlier size constraints rather than retaining an old physical minimum.

The percentage dialog uses the existing caption outside a scroll body. Three label/value rows replace the equal-width three-column table; humidity, temperature and remaining time retain their original order and values. Supporting labels wrap while values receive heading emphasis. The actual icon/state row uses measured horizontal composition. The viewport is at most 440 DIP wide and bounded by the display work area; content may scroll vertically. Content refresh and DPI rescale update fonts, row gaps, padding, bitmap bounds, measured height and scroll extent without issuing a device action.

Display-area fallback, when no owning display is reported, remains an explicit layout assumption of 800 by 600 DIP. It is not evidence of an actual display. Tiny displays below the intrinsic bitmap/control width and extreme customized fonts remain runtime verification boundaries.

## Source verification and limits

Run `node --test ui-md3/tests/humidity-atlas.test.mjs`. Four checks cover unchanged telemetry/formatting methods, byte-identical native and embedded capability routing, production-derived five-image geometry across 32 width/scale/density combinations, and actual scroll-owner/content/reopen/DPI wiring. The geometry adapter preserves C++ integer-division semantics. A deliberate percentage-source substitution is rejected by the preservation comparison.

Using `ATLAS_SOURCE_REF=0f034f2fed3c2b913f4c308d0130cce9eeee763f` against the pre-change source produces two passes and two failures. Current source produces four passes. The preserved baseline must exist locally. These are source/model checks, not native compilation, event-loop or screenshot evidence.

No application, browser, installer or printer was launched; no full build or hardware operation occurred. Native font metrics, bilingual label wrapping, scrollbar reachability, work-area position, keyboard dismissal, live density changes, theme changes and real DPI transitions remain unverified. Validate the actual reachable states at normal/minimum size in English, Cantonese and bilingual modes, both themes/densities and 100%, 125%, 150%, 200% scale before accepting rendered completion. Mapping, nozzle/material editors and the drying-command surface retain their independent ownership and evidence requirements.
