# MakerWorld OpenGL preview

## Behavior

When a MakerWorld model is downloaded through the "Download and Open" flow, the native application
shows an interactive OpenGL preview of the model before it is imported into Prepare. The preview is a
lightweight `wxGLCanvas` panel (`ModelPreviewCanvas`) that renders a single neutral-shaded mesh with:

- orbit rotate (left-drag);
- pan (right-drag or middle-drag);
- wheel zoom; and
- fit-to-view.

The dialog (`ModelPreviewDialog`) uses the MD3 dialog anatomy and offers two actions:

- **Open in Prepare** returns `wxID_OK` and continues the existing import unchanged.
- **Close** returns `wxID_CANCEL` and skips the import.

The preview is wired as a pre-import hook in `Plater::import_model_id`: geometry is extracted first,
and the dialog is only shown when a non-empty mesh is available.

Implementation: `src/slic3r/GUI/ModelPreviewDialog.hpp` and `src/slic3r/GUI/ModelPreviewDialog.cpp`;
the hook is in `src/slic3r/GUI/Plater.cpp` (`import_model_id`).

## Configuration

None. The preview is triggered by the MakerWorld download-and-open path; there is no app-config key
or user setting to configure it.

## Failure modes

- Geometry extraction is best-effort. `ModelPreviewDialog::load_geometry` never throws; it returns
  `false` on failure or an empty mesh.
- When geometry cannot be extracted (or is empty), the dialog is skipped and the flow falls through
  to the existing auto-open import behavior unchanged, so a preview failure never blocks import.

## Verification

- Built locally in the Release configuration. This feature is part of local commit `8d727d49d`,
  which was not yet pushed to the hosted CI at the time of writing, so there is no hosted-run
  evidence for it yet.
