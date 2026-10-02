using System.Text.Json.Nodes;

namespace BambuAutomation;

public sealed class CommandException(string code, string message) : Exception(message)
{
    public string Code { get; } = code;
}

public static class Responses
{
    public static JsonObject Success(JsonObject result) => new() { ["ok"] = true, ["result"] = result };
    public static JsonObject Error(string code, string message) => new()
    {
        ["ok"] = false, ["error"] = new JsonObject { ["code"] = code, ["message"] = message }
    };
    public static string Required(JsonObject args, string key)
    {
        if (args[key] is not JsonValue value || !value.TryGetValue<string>(out var text) || string.IsNullOrWhiteSpace(text))
            throw new CommandException("invalid_arguments", $"A nonempty {key} is required.");
        if (text.Length > 4096 || text.Any(char.IsControl))
            throw new CommandException("invalid_arguments", $"Invalid {key}.");
        return text;
    }
    public static bool Flag(JsonObject args, string key)
    {
        if (args[key] is null) return false;
        if (args[key] is JsonValue value && value.TryGetValue<bool>(out var flag)) return flag;
        throw new CommandException("invalid_arguments", $"{key} must be a boolean.");
    }
}

public interface INativeBridge
{
    IReadOnlyList<int> Instances();
    Task<JsonObject> InvokeAsync(int instance, string operation, JsonObject arguments, CancellationToken cancellationToken);
}
