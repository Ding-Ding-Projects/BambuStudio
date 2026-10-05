namespace BambuAutomation;

public sealed class Options
{
    public string Mode { get; private set; } = "serve";
    public string? Operation { get; private set; }
    public string Transport { get; private set; } = "stdio";
    public List<string> Roots { get; } = [];
    public string Endpoint { get; private set; } = "http://127.0.0.1:8766";
    public string? BearerFile { get; private set; }
    public bool BearerStdin { get; private set; }
    public string? CertificateThumbprint { get; private set; }
    public string? Arguments { get; private set; }
    public bool ArgumentsStdin { get; private set; }
    public int? Instance { get; private set; }
    public bool Json { get; private set; }
    public bool Help { get; private set; }
    public static Options Parse(string[] args)
    {
        var result = new Options();
        var index = 0;
        if (args.Length != 0 && args[0] is "serve" or "command") result.Mode = args[index++];
        if (result.Mode == "command")
        {
            if (index >= args.Length || args[index].StartsWith("--", StringComparison.Ordinal))
                throw new CommandException("invalid_arguments", "command requires an operation name.");
            result.Operation = args[index++];
        }
        string Value()
        {
            if (++index >= args.Length || args[index].StartsWith("--", StringComparison.Ordinal))
                throw new CommandException("invalid_arguments", "An option value is missing.");
            return args[index];
        }
        for (; index < args.Length; index++)
        {
            switch (args[index])
            {
                case "--workspace": result.Roots.Add(Value()); break;
                case "--transport": result.Transport = Value(); break;
                case "--listen": result.Endpoint = Value(); break;
                case "--bearer-file": result.BearerFile = Value(); break;
                case "--bearer-stdin": result.BearerStdin = true; break;
                case "--certificate-thumbprint": result.CertificateThumbprint = Value(); break;
                case "--arguments": result.Arguments = Value(); break;
                case "--arguments-stdin": result.ArgumentsStdin = true; break;
                case "--instance":
                    if (!int.TryParse(Value(), out var pid) || pid <= 0) throw new CommandException("invalid_arguments", "--instance requires a positive process ID.");
                    result.Instance = pid; break;
                case "--json": result.Json = true; break;
                case "--help": case "-h": result.Help = true; break;
                default: throw new CommandException("invalid_arguments", "Unknown option.");
            }
        }
        if (result.Transport is not ("stdio" or "http")) throw new CommandException("invalid_arguments", "transport must be stdio or http.");
        if (result.BearerFile is not null && result.BearerStdin || result.Arguments is not null && result.ArgumentsStdin)
            throw new CommandException("invalid_arguments", "Choose one input source for each value.");
        if (result.BearerStdin && (result.ArgumentsStdin || result.Transport == "stdio"))
            throw new CommandException("invalid_arguments", "Bearer stdin is reserved for HTTP mode and cannot share stdin with other protocols.");
        return result;
    }
}
