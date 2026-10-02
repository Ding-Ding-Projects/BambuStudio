using System.ComponentModel;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Serialization;
using ModelContextProtocol.Server;
using ModelContextProtocol.Protocol;

namespace BambuAutomation;

[McpServerToolType]
public sealed class AutomationTools(CommandService service)
{
    private static readonly JsonSerializerOptions Options = new() { PropertyNamingPolicy = JsonNamingPolicy.CamelCase, DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull };
    private async Task<CallToolResult> Invoke<T>(string operation, T arguments, CancellationToken cancellationToken)
    {
        var input = JsonSerializer.SerializeToNode(arguments, Options)!.AsObject();
        var result = await service.ExecuteAsync(operation, input, cancellationToken);
        return new CallToolResult
        {
            IsError = result["ok"]?.GetValue<bool>() != true,
            StructuredContent = JsonSerializer.SerializeToElement(result),
            Content = [new TextContentBlock { Text = result.ToJsonString() }]
        };
    }
    [McpServerTool(Name = "bambu_capabilities"), Description("Report service transports, headless availability, attached native instances, and supported native capabilities.")]
    public Task<CallToolResult> Capabilities([Description("Typed capabilities arguments, including required fields and safety constraints.")] InstanceArguments arguments, CancellationToken cancellationToken)
        => Invoke("capabilities", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_instances"), Description("List enabled native automation instances by process ID.")]
    public Task<CallToolResult> Instances([Description("Typed instances arguments, including required fields and safety constraints.")] InstanceArguments arguments, CancellationToken cancellationToken)
        => Invoke("instances", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_project_inspect"), Description("Inspect the current native project.")]
    public Task<CallToolResult> ProjectInspect([Description("Typed project_inspect arguments, including required fields and safety constraints.")] InstanceArguments arguments, CancellationToken cancellationToken)
        => Invoke("project_inspect", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_project_new"), Description("Create a new native project.")]
    public Task<CallToolResult> ProjectNew([Description("Typed project_new arguments, including required fields and safety constraints.")] InstanceArguments arguments, CancellationToken cancellationToken)
        => Invoke("project_new", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_project_open"), Description("Open an existing workspace project.")]
    public Task<CallToolResult> ProjectOpen([Description("Typed project_open arguments, including required fields and safety constraints.")] InputArguments arguments, CancellationToken cancellationToken)
        => Invoke("project_open", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_project_save"), Description("Save the current project atomically to a workspace .3mf file.")]
    public Task<CallToolResult> ProjectSave([Description("Typed project_save arguments, including required fields and safety constraints.")] OutputArguments arguments, CancellationToken cancellationToken)
        => Invoke("project_save", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_model_import"), Description("Import an existing workspace model into the native project.")]
    public Task<CallToolResult> ModelImport([Description("Typed model_import arguments, including required fields and safety constraints.")] InputArguments arguments, CancellationToken cancellationToken)
        => Invoke("model_import", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_presets_list"), Description("List native print, filament, and printer presets.")]
    public Task<CallToolResult> PresetsList([Description("Typed presets_list arguments, including required fields and safety constraints.")] InstanceArguments arguments, CancellationToken cancellationToken)
        => Invoke("presets_list", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_settings_get"), Description("Read current project settings.")]
    public Task<CallToolResult> SettingsGet([Description("Typed settings_get arguments, including required fields and safety constraints.")] InstanceArguments arguments, CancellationToken cancellationToken)
        => Invoke("settings_get", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_settings_update"), Description("Update supported numeric project settings through native validation.")]
    public Task<CallToolResult> SettingsUpdate([Description("Typed settings_update arguments, including required fields and safety constraints.")] SettingsArguments arguments, CancellationToken cancellationToken)
        => Invoke("settings_update", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_slice_start"), Description("Start current native slicing or an isolated packaged native CLI job. Native completion is not fabricated.")]
    public Task<CallToolResult> SliceStart([Description("Typed slice_start arguments, including required fields and safety constraints.")] SliceArguments arguments, CancellationToken cancellationToken)
        => Invoke("slice_start", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_export_file"), Description("Export the current native project to a workspace .3mf file atomically.")]
    public Task<CallToolResult> ExportFile([Description("Typed export_file arguments, including required fields and safety constraints.")] OutputArguments arguments, CancellationToken cancellationToken)
        => Invoke("export_file", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_printer_list"), Description("List printers connected through the selected native instance.")]
    public Task<CallToolResult> PrinterList([Description("Typed printer_list arguments, including required fields and safety constraints.")] InstanceArguments arguments, CancellationToken cancellationToken)
        => Invoke("printer_list", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_printer_status"), Description("Read a connected printer status.")]
    public Task<CallToolResult> PrinterStatus([Description("Typed printer_status arguments, including required fields and safety constraints.")] PrinterArguments arguments, CancellationToken cancellationToken)
        => Invoke("printer_status", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_printer_start"), Description("Start the current sliced plate through native readiness checks, no extra confirmation. Only one known reliable nozzle, one used filament, matching target model/diameter, online idle printer, non-AMS [-1] mapping supported.")]
    public Task<CallToolResult> PrinterStart([Description("Typed printer_start arguments, including required fields and safety constraints.")] PrintStartArguments arguments, CancellationToken cancellationToken)
        => Invoke("printer_start", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_printer_pause"), Description("Pause a connected printer.")]
    public Task<CallToolResult> PrinterPause([Description("Typed printer_pause arguments, including required fields and safety constraints.")] PrinterArguments arguments, CancellationToken cancellationToken)
        => Invoke("printer_pause", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_printer_resume"), Description("Resume a connected printer.")]
    public Task<CallToolResult> PrinterResume([Description("Typed printer_resume arguments, including required fields and safety constraints.")] PrinterArguments arguments, CancellationToken cancellationToken)
        => Invoke("printer_resume", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_printer_cancel"), Description("Cancel a connected printer job.")]
    public Task<CallToolResult> PrinterCancel([Description("Typed printer_cancel arguments, including required fields and safety constraints.")] PrinterArguments arguments, CancellationToken cancellationToken)
        => Invoke("printer_cancel", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_job_status"), Description("Read native or service-owned headless job status.")]
    public Task<CallToolResult> JobStatus([Description("Typed job_status arguments, including required fields and safety constraints.")] JobArguments arguments, CancellationToken cancellationToken)
        => Invoke("job_status", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_job_cancel"), Description("Cancel a service-owned headless job. Native cancellation returns cancellation_not_safe until a safe native operation ID exists.")]
    public Task<CallToolResult> JobCancel([Description("Typed job_cancel arguments, including required fields and safety constraints.")] JobArguments arguments, CancellationToken cancellationToken)
        => Invoke("job_cancel", arguments, cancellationToken);
}

