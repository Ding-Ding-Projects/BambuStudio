using System.ComponentModel;

namespace BambuAutomation;

public class InstanceArguments
{
    [Description("Positive native process ID. Required when multiple enabled native instances exist.")]
    public int? InstanceId { get; set; }
}
public sealed class InputArguments : InstanceArguments
{
    [Description("Required absolute existing file within an explicit workspace root. Reparse points, alternate streams, and network/device paths are rejected.")]
    public required string Path { get; set; }
}
public class OutputArguments : InstanceArguments
{
    [Description("Required absolute output .3mf path within an explicit workspace root. Parent must exist.")]
    public required string Path { get; set; }
    [Description("Explicit consent to replace an existing output. Defaults false.")]
    public bool Overwrite { get; set; }
}
public sealed class SettingsArguments : InstanceArguments
{
    [Description("Required numeric settings. Supported keys: layer_height, sparse_infill_density, wall_loops, top_shell_layers, bottom_shell_layers. Native validates ranges and integer fields.")]
    public required Dictionary<string, double> Values { get; set; }
}
public sealed class SliceArguments : InstanceArguments
{
    [Description("False: slice current GUI project. True: isolated bundled native CLI job.")]
    public bool Headless { get; set; }
    [Description("Required for headless mode: absolute configured input .3mf workspace path.")]
    public string? Path { get; set; }
    [Description("Required for headless mode: distinct absolute output .3mf workspace path.")]
    public string? Output { get; set; }
    [Description("Headless mode only: 0 for all plates, otherwise positive plate index up to 1000.")]
    public int Plate { get; set; }
    [Description("Headless mode only: explicit permission to replace an existing output. Defaults false.")]
    public bool Overwrite { get; set; }
}
public class PrinterArguments : InstanceArguments
{
    [Description("Required exact ID of an online printer already connected through the selected native instance. No raw printer commands are accepted.")]
    public required string PrinterId { get; set; }
}
public sealed class PrintStartArguments : PrinterArguments
{
    [Description("Required idempotency identity for this print intent, preserved by native process-lifetime deduplication. Never auto-replay after restart, submission_unknown, or operation_in_progress.")]
    public required string RequestId { get; set; }
    [Description("Required new absolute workspace .3mf staging output. Native exports the currently sliced plate itself; this is never an input archive.")]
    public required string Path { get; set; }
    [Description("Must remain false for native print staging. Existing staging output is rejected.")]
    public bool Overwrite { get; set; }
    [Description("Must be false. AMS submission is unsupported in this bridge.")]
    public bool UseAms { get; set; }
    [Description("Single-filament non-AMS mapping only: [-1]. Other mappings are rejected by native readiness checks.")]
    public int[] AmsMapping { get; set; } = [-1];
    [Description("Must be an empty object. Dual-nozzle and alternate nozzle mapping are unsupported.")]
    public Dictionary<string, int> NozzleMapping { get; set; } = [];
    [Description("Optional native bed-leveling selection. Native online/idle, known nozzle, model, diameter, and sliced-plate readiness checks still apply.")]
    public bool? BedLeveling { get; set; }
}
public sealed class JobArguments : InstanceArguments
{
    [Description("Required native job identity or service-owned headless-... identity. Native jobs remain with their original instance.")]
    public required string JobId { get; set; }
}

