using System.Text.Json.Nodes;

namespace BambuAutomation;

public sealed class CommandService(Workspace workspace, INativeBridge bridge, SliceJobs jobs)
{
    public static readonly string[] Operations = ["capabilities", "instances", "project_inspect", "project_new", "project_open", "project_save",
        "model_import", "presets_list", "settings_get", "settings_update", "slice_start", "export_file", "printer_list", "printer_status",
        "printer_start", "printer_pause", "printer_resume", "printer_cancel", "job_status", "job_cancel"];

    public async Task<JsonObject> ExecuteAsync(string operation, JsonObject arguments, CancellationToken cancellationToken = default)
    {
        try
        {
            if (!Operations.Contains(operation, StringComparer.Ordinal))
                throw new CommandException("unknown_operation", "The operation is not supported.");
            var args = (JsonObject)arguments.DeepClone();
            if (operation == "instances")
                return Responses.Success(new JsonObject { ["instances"] = new JsonArray(bridge.Instances().Select(pid => (JsonNode)new JsonObject { ["instanceId"] = pid }).ToArray()) });
            if (operation is "job_status" or "job_cancel" && args["jobId"]?.GetValue<string>() is { } jobId && jobId.StartsWith("headless-", StringComparison.Ordinal))
                return Responses.Success(operation == "job_status" ? jobs.Status(jobId) : jobs.Cancel(jobId));
            if (operation == "slice_start" && Responses.Flag(args, "headless"))
            {
                if (args["plateIndex"] is not null)
                    throw new CommandException("invalid_arguments", "Headless mode uses plate, not native plateIndex.");
                return Responses.Success(jobs.Start(args));
            }
            Validate(operation, args);
            var instances = bridge.Instances();
            if (operation == "capabilities" && args["instanceId"] is null && instances.Count != 1)
                return Responses.Success(Capabilities(instances, null));
            int instance;
            if (args["instanceId"] is JsonValue value && value.TryGetValue<int>(out var requested) && requested > 0)
                instance = requested;
            else if (args["instanceId"] is not null)
                throw new CommandException("invalid_arguments", "instanceId must be a positive process ID.");
            else if (instances.Count == 1) instance = instances[0];
            else if (instances.Count == 0) throw new CommandException("instance_unavailable", "No enabled Bambu Studio instance is running.");
            else throw new CommandException("ambiguous_instance", "Multiple native instances are running; specify instanceId.");
            args.Remove("instanceId");
            var result = await bridge.InvokeAsync(instance, operation, args, cancellationToken);
            return Responses.Success(operation == "capabilities" ? Capabilities(instances, result) : result);
        }
        catch (CommandException exception) { return Responses.Error(exception.Code, exception.Message); }
        catch (OperationCanceledException) { return Responses.Error("cancelled", "The request was cancelled; inspect native state before retrying."); }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or ArgumentException or InvalidOperationException or System.Text.Json.JsonException)
        { return Responses.Error("invalid_request", "The request could not be processed safely."); }
    }

    private JsonObject Capabilities(IReadOnlyList<int> instances, JsonObject? native) => new()
    {
        ["serviceVersion"] = "1",
        ["transports"] = new JsonArray("stdio", "streamable-http", "cli"),
        ["headlessSlicing"] = new JsonObject { ["available"] = jobs.NativeAvailable, ["input"] = "configured .3mf", ["output"] = "sliced .3mf", ["maxActiveJobs"] = 4 },
        ["nativeAttached"] = native is not null,
        ["instanceSelectionRequired"] = native is null && instances.Count > 1,
        ["enabledInstances"] = new JsonArray(instances.Select(pid => (JsonNode)JsonValue.Create(pid)!).ToArray()),
        ["nativeCapabilities"] = native?.DeepClone()
    };

    private void Validate(string operation, JsonObject args)
    {
        if (args["plateIndex"] is not null &&
            (operation is not ("slice_start" or "export_file" or "printer_start") ||
             args["plateIndex"] is not JsonValue plate || !plate.TryGetValue<int>(out var index) || index < 0))
            throw new CommandException("invalid_arguments", "plateIndex must be a nonnegative integer for a native plate operation.");
        if (operation is "project_open" or "model_import")
            args["path"] = workspace.Resolve(Responses.Required(args, "path"));
        if (operation is "project_save" or "export_file")
            args["path"] = workspace.Resolve(Responses.Required(args, "path"), true, Responses.Flag(args, "overwrite"));
        if (operation.StartsWith("printer_", StringComparison.Ordinal) && operation != "printer_list")
            Responses.Required(args, "printerId");
        if (operation == "printer_start")
        {
            Responses.Required(args, "requestId");
            var sliceJob = Responses.Required(args, "sliceJobId");
            if (!sliceJob.StartsWith("slice-", StringComparison.Ordinal))
                throw new CommandException("invalid_arguments", "sliceJobId must identify a native slice job from the selected instance.");
        }
        if (operation == "printer_start")
        {
            if (Responses.Flag(args, "overwrite"))
                throw new CommandException("invalid_arguments", "Print staging must not overwrite an existing file.");
            // Native deduplication must see retries after the first submission created its staging file.
            // Native rejects existing output for a new request ID before exporting the current sliced plate.
            args["path"] = workspace.Resolve(Responses.Required(args, "path"), true, true);
            if (!args["path"]!.GetValue<string>().EndsWith(".3mf", StringComparison.OrdinalIgnoreCase))
                throw new CommandException("invalid_format", "Print staging output must be a .3mf file.");
        }
        if (operation == "settings_update" && args["values"] is not JsonObject)
            throw new CommandException("invalid_arguments", "settings_update requires values as an object.");
        // No unvalidated alternate path can reach the native file operations.
        foreach (var key in new[] { "input", "output" })
            if (args[key] is not null) args[key] = workspace.Resolve(Responses.Required(args, key), key == "output", Responses.Flag(args, "overwrite"));
    }
}
