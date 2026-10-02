using System.ComponentModel;
using System.Text.Json.Nodes;
using System.Text.Json;
using ModelContextProtocol.Server;
using ModelContextProtocol.Protocol;

namespace BambuAutomation;

[McpServerToolType]
public sealed class AutomationTools(CommandService service)
{
    private async Task<CallToolResult> Invoke(string operation, JsonObject arguments, CancellationToken cancellationToken)
    {
        var result = await service.ExecuteAsync(operation, arguments, cancellationToken);
        return new CallToolResult
        {
            IsError = result["ok"]?.GetValue<bool>() != true,
            StructuredContent = JsonSerializer.SerializeToElement(result),
            Content = [new TextContentBlock { Text = result.ToJsonString() }]
        };
    }
    [McpServerTool(Name = "bambu_capabilities"), Description("Report native capabilities and explicit unsupported operations.")]
    public async Task<CallToolResult> Capabilities([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("capabilities", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_instances"), Description("List enabled native automation instances by process ID.")]
    public async Task<CallToolResult> Instances([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("instances", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_project_inspect"), Description("Inspect the current native project.")]
    public async Task<CallToolResult> ProjectInspect([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("project_inspect", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_project_new"), Description("Create a new native project.")]
    public async Task<CallToolResult> ProjectNew([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("project_new", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_project_open"), Description("Open a workspace project. Arguments: path, optional instanceId.")]
    public async Task<CallToolResult> ProjectOpen([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("project_open", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_project_save"), Description("Save a project. Arguments: path, overwrite, optional instanceId.")]
    public async Task<CallToolResult> ProjectSave([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("project_save", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_model_import"), Description("Import a workspace model. Arguments: path, optional instanceId.")]
    public async Task<CallToolResult> ModelImport([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("model_import", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_presets_list"), Description("List native presets.")]
    public async Task<CallToolResult> PresetsList([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("presets_list", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_settings_get"), Description("Read current project settings.")]
    public async Task<CallToolResult> SettingsGet([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("settings_get", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_settings_update"), Description("Update supported settings. Arguments: values object, optional instanceId.")]
    public async Task<CallToolResult> SettingsUpdate([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("settings_update", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_slice_start"), Description("Start native slicing; headless:true runs packaged CLI using path,output .3mf,plate,overwrite.")]
    public async Task<CallToolResult> SliceStart([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("slice_start", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_export_file"), Description("Export a native project file. Arguments: path,format,overwrite,optional instanceId.")]
    public async Task<CallToolResult> ExportFile([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("export_file", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_printer_list"), Description("List printers connected through the selected native instance.")]
    public async Task<CallToolResult> PrinterList([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("printer_list", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_printer_status"), Description("Read a connected printer status. Arguments: printerId,optional instanceId.")]
    public async Task<CallToolResult> PrinterStatus([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("printer_status", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_printer_start"), Description("Start a print through native readiness checks. Requires printerId and idempotent requestId.")]
    public async Task<CallToolResult> PrinterStart([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("printer_start", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_printer_pause"), Description("Pause a connected printer. Requires printerId.")]
    public async Task<CallToolResult> PrinterPause([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("printer_pause", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_printer_resume"), Description("Resume a connected printer. Requires printerId.")]
    public async Task<CallToolResult> PrinterResume([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("printer_resume", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_printer_cancel"), Description("Cancel a connected printer job. Requires printerId.")]
    public async Task<CallToolResult> PrinterCancel([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("printer_cancel", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_job_status"), Description("Read native or service-owned headless job status. Requires jobId.")]
    public async Task<CallToolResult> JobStatus([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("job_status", arguments, cancellationToken);

    [McpServerTool(Name = "bambu_job_cancel"), Description("Cancel native or service-owned headless job. Requires jobId.")]
    public async Task<CallToolResult> JobCancel([Description("Operation arguments as a JSON object; see the command contract.")] JsonObject arguments, CancellationToken cancellationToken)
        => await Invoke("job_cancel", arguments, cancellationToken);
}
