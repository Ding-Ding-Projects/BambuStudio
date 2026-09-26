# Plate print actions and material mapping

The Prepare action bar offers **Slice plate**, **Slice and print**, and **Print plate** separately. Slice and print uses the same version, material, and build-plate preflight as Slice plate. It starts a slice of the current plate, then opens print setup only when that same plate has a successful, current, printable result. It never submits a job. The final **Print** action in print setup remains explicit.

The continuation is one-shot. A different slice, cancellation, failed or unstarted slice, plate or project replacement, or a changed plate result prevents print setup from opening. When the plate already has a finished, unchanged, printable slice, **Slice and print** reuses that result and opens setup once without waiting for a completion event. It is cleared before the setup dialog appears. A changed material mapping invalidates the sliced result and returns to Prepare unless the user chose **Swap and reslice**.

The Custom material page retains drag-and-drop and adds a material selector with **Move to left nozzle** and **Move to right nozzle** controls. **Swap groups** exchanges the two nozzle groups for review. **Swap and reslice** applies the exchange and requests a fresh slice if the resulting mapping passes the existing nozzle validation. These are project and plate choices; they do not change an active physical print or the printer's default settings.

A saved automatic mapping preference belongs to the selected printer preset. A fresh project applies it only after reset. On a later printer change, the preference applies only while the project is still owned by that fresh-project path and every plate inherits the project mode. An explicit project mapping choice or imported 3MF settings clear that ownership. An explicit plate mode, including Custom, also takes precedence. Manual material assignments are never reconstructed from a printer preference alone.

The Import menu exposes Model Creator. The command palette indexes the same menu command. Its validated STL reaches `Plater::load_files` only after the dialog's **Add to plate** action.

The focused C++ regression covers saved-preference precedence and the one-shot slice continuation predicate. A full Windows build, native interaction capture, real printer setup, and hardware print remain unverified until those checks are run against a built application.
