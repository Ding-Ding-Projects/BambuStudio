using System.Net;
using System.Security.Cryptography.X509Certificates;
using System.Text.Json.Nodes;
using BambuAutomation;
using ModelContextProtocol.Server;

try
{
    if (args.Length > 0 && args[0] == "local-capabilities")
    {
        if (args.Length != 3 || args[1] != "--instance" || !int.TryParse(args[2], out var nativeInstance) || nativeInstance <= 0)
            throw new CommandException("invalid_arguments", "Use local-capabilities --instance <native PID> after native pairing consent.");
        using var lifetime = new CancellationTokenSource();
        Console.CancelKeyPress += (_, eventArgs) => { eventArgs.Cancel = true; lifetime.Cancel(); };
        await BambuAutomation.LocalCapabilities.NativeComposition.RunAsync(nativeInstance, lifetime.Token);
        return 0;
    }
    var options = Options.Parse(args);
    if (options.Help)
    {
        Console.Error.WriteLine("bambu-automation serve --workspace <directory> [--transport stdio|http] [--listen <origin>] [--bearer-file <protected-file>|--bearer-stdin] [--certificate-thumbprint <CurrentUser/My thumbprint>]\nbambu-automation command <operation> --workspace <directory> [--instance <pid>] [--arguments <JSON|@file>|--arguments-stdin] --json");
        return 0;
    }
    var workspace = new Workspace(options.Roots);
    var executable = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "bambu-studio.exe"));
    using var jobs = new SliceJobs(workspace, executable);
    var service = new CommandService(workspace, new NativeBridge(), jobs);
    if (options.Mode == "command")
    {
        var input = "{}";
        if (options.ArgumentsStdin) input = await ReadBoundedAsync(Console.OpenStandardInput());
        else if (options.Arguments is { } value)
        {
            if (value.StartsWith('@'))
            {
                var path = workspace.Resolve(value[1..]);
                await using var file = File.OpenRead(path);
                input = await ReadBoundedAsync(file);
            }
            else input = value;
        }
        if (System.Text.Encoding.UTF8.GetByteCount(input) > NativeBridge.MaximumFrame)
            throw new CommandException("request_too_large", "Arguments exceed 1 MiB.");
        var arguments = JsonNode.Parse(input) as JsonObject ?? throw new CommandException("invalid_arguments", "Arguments must be a JSON object.");
        if (options.Instance.HasValue) arguments["instanceId"] = options.Instance.Value;
        using var cancellation = new CancellationTokenSource();
        Console.CancelKeyPress += (_, eventArgs) => { eventArgs.Cancel = true; cancellation.Cancel(); };
        var response = await service.ExecuteAsync(options.Operation!, arguments, cancellation.Token);
        // A one-shot CLI cannot retain an asynchronous job registry. Wait for this job before exiting.
        if (options.Operation == "slice_start" && Responses.Flag(arguments, "headless") && response["ok"]?.GetValue<bool>() == true)
        {
            var id = response["result"]!["jobId"]!.GetValue<string>();
            while (true)
            {
                if (cancellation.IsCancellationRequested) jobs.Cancel(id);
                var state = jobs.Status(id);
                if (state["state"]!.GetValue<string>() is "completed" or "cancelled" or "failed")
                {
                    response = state["state"]!.GetValue<string>() == "completed" ? Responses.Success(state) : Responses.Error(state["error"]!.GetValue<string>(), "Headless slicing did not complete.");
                    break;
                }
                await Task.Delay(200);
            }
        }
        Console.Out.WriteLine(response.ToJsonString());
        return response["ok"]?.GetValue<bool>() == true ? 0 : 1;
    }

    if (options.Transport == "stdio")
    {
        var builder = Host.CreateApplicationBuilder(new HostApplicationBuilderSettings { Args = [], DisableDefaults = true });
        builder.Logging.ClearProviders();
        builder.Logging.AddConsole(logging => logging.LogToStandardErrorThreshold = LogLevel.Trace);
        builder.Services.AddSingleton(service);
        builder.Services.AddMcpServer().WithStdioServerTransport().WithTools<AutomationTools>();
        await builder.Build().RunAsync();
    }
    else
    {
        var endpoint = HttpSecurity.ValidateEndpoint(options.Endpoint);
        var bearer = options.BearerFile is not null ? await HttpSecurity.ReadProtectedBearerAsync(options.BearerFile) :
            options.BearerStdin ? HttpSecurity.ValidateBearer((await ReadBoundedAsync(Console.OpenStandardInput(), 4096)).TrimEnd('\r', '\n')) :
            throw new CommandException("bearer_required", "HTTP mode requires a protected bearer file or bearer stdin.");
        var security = new HttpSecurity(endpoint, bearer);
        var builder = WebApplication.CreateBuilder(new WebApplicationOptions { Args = [], ContentRootPath = AppContext.BaseDirectory });
        builder.Logging.ClearProviders();
        builder.Logging.AddConsole(logging => logging.LogToStandardErrorThreshold = LogLevel.Trace);
        builder.WebHost.ConfigureKestrel(kestrel =>
        {
            kestrel.Limits.MaxRequestBodySize = NativeBridge.MaximumFrame;
            kestrel.Limits.RequestHeadersTimeout = TimeSpan.FromSeconds(10);
            kestrel.Limits.MaxConcurrentConnections = 32;
            var address = endpoint.Host.Equals("localhost", StringComparison.OrdinalIgnoreCase) ? IPAddress.Loopback :
                IPAddress.TryParse(endpoint.Host.Trim('[', ']'), out var literal) ? literal :
                throw new CommandException("invalid_endpoint", "Listener host must be localhost or a literal local IP address.");
            kestrel.Listen(address, endpoint.Port, listen =>
            {
                if (endpoint.Scheme == "https")
                {
                    if (options.CertificateThumbprint is null) throw new CommandException("certificate_required", "HTTPS requires a CurrentUser/My certificate thumbprint.");
                    using var store = new X509Store(StoreName.My, StoreLocation.CurrentUser);
                    store.Open(OpenFlags.ReadOnly);
                    var matches = store.Certificates.Find(X509FindType.FindByThumbprint, options.CertificateThumbprint, true);
                    if (matches.Count != 1 || !matches[0].HasPrivateKey) throw new CommandException("certificate_unavailable", "A valid unique server certificate with a private key is required.");
                    listen.UseHttps(matches[0]);
                }
            });
        });
        builder.Services.AddSingleton(service);
        builder.Services.AddMcpServer().WithHttpTransport().WithTools<AutomationTools>();
        var app = builder.Build();
        app.Use(async (context, next) =>
        {
            if (!security.AcceptHost(context.Request.Host.Value) || !security.AcceptOrigin(context.Request.Headers.Origin.ToString()))
            { context.Response.StatusCode = 403; await context.Response.WriteAsJsonAsync(Responses.Error("invalid_origin", "Host or Origin is not allowed.")); return; }
            if (!security.AcceptBearer(context.Request.Headers.Authorization.ToString()))
            { context.Response.StatusCode = 401; await context.Response.WriteAsJsonAsync(Responses.Error("unauthorized", "A valid bearer authorization is required.")); return; }
            await next(context);
        });
        app.MapMcp("/mcp");
        await app.RunAsync();
    }
    return 0;
}
catch (BambuAutomation.LocalCapabilities.CapabilityDenied)
{
    Console.Error.WriteLine("local_capability_unavailable");
    return 1;
}
catch (OperationCanceledException)
{
    return 0;
}
catch (CommandException exception)
{
    Console.Error.WriteLine(exception.Code);
    if (args.Contains("--json", StringComparer.Ordinal)) Console.Out.WriteLine(Responses.Error(exception.Code, exception.Message).ToJsonString());
    return 1;
}
catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or System.Text.Json.JsonException or ArgumentException or InvalidOperationException)
{
    Console.Error.WriteLine("startup_failed");
    if (args.Contains("--json", StringComparer.Ordinal)) Console.Out.WriteLine(Responses.Error("startup_failed", "The service could not start safely.").ToJsonString());
    return 1;
}

static async Task<string> ReadBoundedAsync(Stream stream, int maximum = NativeBridge.MaximumFrame)
{
    using var bytes = new MemoryStream();
    var buffer = new byte[4096];
    int read;
    while ((read = await stream.ReadAsync(buffer)) != 0)
    {
        if (bytes.Length + read > maximum) throw new CommandException("request_too_large", "Input exceeds the permitted size.");
        bytes.Write(buffer, 0, read);
    }
    return new System.Text.UTF8Encoding(false, true).GetString(bytes.ToArray());
}
