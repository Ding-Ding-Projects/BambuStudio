using System.IO.Pipes;
using System.Text;
using System.Text.Json.Nodes;

namespace BambuAutomation;

public sealed class NativeBridge : INativeBridge
{
    public const string Prefix = "BambuStudio.Automation.v1.";
    public const int MaximumFrame = 1024 * 1024;
    public IReadOnlyList<int> Instances()
    {
        if (!OperatingSystem.IsWindows()) return [];
        try
        {
            return Directory.GetFiles(@"\\.\pipe\", Prefix + "*")
                .Select(Path.GetFileName).Select(name => int.TryParse(name?[Prefix.Length..], out var pid) ? pid : 0)
                .Where(pid => pid > 0).Order().ToArray();
        }
        catch (IOException) { throw new CommandException("discovery_failed", "Unable to enumerate native automation pipes."); }
        catch (UnauthorizedAccessException) { throw new CommandException("discovery_failed", "Native automation discovery was denied."); }
    }

    public async Task<JsonObject> InvokeAsync(int instance, string operation, JsonObject arguments, CancellationToken cancellationToken)
    {
        using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeout.CancelAfter(TimeSpan.FromSeconds(30));
        await using var pipe = new NamedPipeClientStream(".", Prefix + instance, PipeDirection.InOut,
            PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
        var id = Guid.NewGuid().ToString("N");
        var request = new JsonObject { ["version"] = 1, ["id"] = id, ["operation"] = operation, ["arguments"] = arguments.DeepClone() };
        var frame = Encoding.UTF8.GetBytes(request.ToJsonString() + "\n");
        if (frame.Length > MaximumFrame) throw new CommandException("request_too_large", "Native request exceeds 1 MiB.");
        try
        {
            await pipe.ConnectAsync(5000, timeout.Token);
            await pipe.WriteAsync(frame, timeout.Token);
            await pipe.FlushAsync(timeout.Token);
            var response = await ReadFrameAsync(pipe, timeout.Token);
            return ValidateResponse(response, id);
        }
        catch (TimeoutException) { throw new CommandException("instance_unavailable", "The native instance did not accept the connection."); }
        catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
        { throw new CommandException("native_timeout", "Native operation timed out; query state before retrying a mutation."); }
        catch (IOException) { throw new CommandException("native_disconnected", "Native connection closed; query state before retrying a mutation."); }
    }

    public static async Task<JsonObject> ReadFrameAsync(Stream stream, CancellationToken cancellationToken)
    {
        using var bytes = new MemoryStream();
        var single = new byte[1];
        while (true)
        {
            if (await stream.ReadAsync(single, cancellationToken) == 0)
                throw new CommandException("invalid_response", "Native response ended before its newline.");
            if (single[0] == (byte)'\n') break;
            if (bytes.Length >= MaximumFrame) throw new CommandException("response_too_large", "Native response exceeds 1 MiB.");
            bytes.WriteByte(single[0]);
        }
        try
        {
            return JsonNode.Parse(new UTF8Encoding(false, true).GetString(bytes.ToArray())) as JsonObject
                ?? throw new CommandException("invalid_response", "Native response must be an object.");
        }
        catch (System.Text.Json.JsonException) { throw new CommandException("invalid_response", "Native response is malformed JSON."); }
        catch (DecoderFallbackException) { throw new CommandException("invalid_response", "Native response is not UTF-8."); }
    }

    public static JsonObject ValidateResponse(JsonObject response, string id)
    {
        if (response["version"]?.GetValue<int>() != 1 || response["id"]?.GetValue<string>() != id || response["ok"] is not JsonValue flag || !flag.TryGetValue<bool>(out var ok))
            throw new CommandException("invalid_response", "Native response identity or version mismatch.");
        if (ok) return response["result"] as JsonObject ?? throw new CommandException("invalid_response", "Native result must be an object.");
        var error = response["error"] as JsonObject ?? throw new CommandException("invalid_response", "Native error must be an object.");
        throw new CommandException(Responses.Required(error, "code"), Responses.Required(error, "message"));
    }
}
