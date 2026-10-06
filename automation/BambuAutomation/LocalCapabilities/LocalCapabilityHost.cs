using System.Net;
using System.Text.Json;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Hosting.Server;
using Microsoft.AspNetCore.Hosting.Server.Features;

namespace BambuAutomation.LocalCapabilities;

public sealed record SchoolSnapshot(bool Enabled, string DisplayName);
public sealed record CapabilityResult(SchoolSnapshot? School = null, bool Opened = false);

/// <summary>Adapters own GUI-thread marshalling and native-window lifetime. Management
/// actions open the real native editor; no credentials or arbitrary paths cross this API.</summary>
public interface ILocalCapabilityAdapter
{
    LocalCapability Capability { get; }
    Task<CapabilityResult> InvokeAsync(CancellationToken cancellationToken);
}

/// <summary>Opt-in dedicated listener. Never mapped into the MCP app or WebView bridge.</summary>
public sealed class LocalCapabilityHost : IAsyncDisposable
{
    private readonly IReadOnlyDictionary<LocalCapability, ILocalCapabilityAdapter> adapters;
    private readonly CapabilitySession session;
    private readonly CancellationTokenSource lifetime = new();
    private WebApplication? app;
    private string? authority;
    private int admitted;
    private int disposed;
    public Uri? Endpoint { get; private set; }

    public LocalCapabilityHost(IEnumerable<ILocalCapabilityAdapter> adapters)
    {
        this.adapters = adapters.ToDictionary(adapter => adapter.Capability);
        session = new CapabilitySession(this.adapters.Keys);
    }

    // The embedding native UI calls this only after showing origin and capabilities.
    // Deliberately absent from HTTP routing, reflection dispatch, and MCP tools.
    public PairingOffer BeginPairing(string exactOrigin, IEnumerable<LocalCapability> grants) =>
        session.BeginPairing(exactOrigin, grants);

    public void Revoke() => session.Revoke();

    public async Task StartAsync(CancellationToken cancellationToken = default)
    {
        if (app is not null || lifetime.IsCancellationRequested) throw new InvalidOperationException("Listener is not restartable.");
        if (adapters.Count == 0) throw new InvalidOperationException("At least one real adapter is required.");
        var builder = WebApplication.CreateSlimBuilder(new WebApplicationOptions { Args = [], ContentRootPath = AppContext.BaseDirectory });
        builder.Logging.ClearProviders();
        builder.WebHost.ConfigureKestrel(options =>
        {
            options.AddServerHeader = false;
            options.Limits.MaxRequestBodySize = CapabilitySession.MaximumBodyBytes;
            options.Limits.MaxRequestHeaderCount = 16;
            options.Limits.MaxRequestHeadersTotalSize = 8192;
            options.Limits.RequestHeadersTimeout = TimeSpan.FromSeconds(5);
            options.Limits.KeepAliveTimeout = TimeSpan.FromSeconds(5);
            options.Limits.MaxConcurrentConnections = 4;
            options.Listen(IPAddress.Loopback, 0);
        });
        app = builder.Build();
        app.Run(HandleAsync);
        await app.StartAsync(cancellationToken);
        var addresses = app.Services.GetRequiredService<IServer>().Features.Get<IServerAddressesFeature>()!;
        Endpoint = new Uri(addresses.Addresses.Single());
        authority = Endpoint.Authority;
    }

    private async Task HandleAsync(HttpContext context)
    {
        context.Response.Headers.CacheControl = "no-store";
        context.Response.Headers["X-Content-Type-Options"] = "nosniff";
        context.Response.Headers["Referrer-Policy"] = "no-referrer";
        // Literal host only: no DNS resolution, proxy headers, localhost aliases, or redirects.
        var origin = context.Request.Headers.Origin.ToString();
        if (context.Connection.RemoteIpAddress is not { } peer || !IPAddress.IsLoopback(peer) ||
            authority is null || context.Request.Host.Value != authority ||
            context.Request.QueryString.HasValue || !session.AcceptsOrigin(origin))
        { context.Response.StatusCode = 403; return; }
        context.Response.Headers.AccessControlAllowOrigin = origin;
        context.Response.Headers.Vary = "Origin";
        if (context.Request.Method == "OPTIONS")
        {
            var headers = context.Request.Headers.AccessControlRequestHeaders.ToString().Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
            if (context.Request.Headers.AccessControlRequestMethod != "POST" ||
                headers.Any(header => !new[] { "content-type", "authorization" }.Contains(header, StringComparer.OrdinalIgnoreCase)) ||
                context.Request.Path != "/v1/pair" && context.Request.Path != "/v1/invoke")
            { context.Response.StatusCode = 403; return; }
            context.Response.Headers.AccessControlAllowMethods = "POST";
            context.Response.Headers.AccessControlAllowHeaders = "Content-Type, Authorization";
            context.Response.StatusCode = 204;
            return;
        }
        if (context.Request.Method != "POST" || context.Request.ContentType != "application/json" ||
            context.Request.Headers.ContentEncoding.Count != 0 || context.Request.ContentLength > CapabilitySession.MaximumBodyBytes)
        { context.Response.StatusCode = 400; return; }
        if (Interlocked.CompareExchange(ref admitted, 1, 0) != 0) { context.Response.StatusCode = 429; return; }
        try
        {
            using var timeout = CancellationTokenSource.CreateLinkedTokenSource(context.RequestAborted, lifetime.Token);
            timeout.CancelAfter(TimeSpan.FromSeconds(10));
            using var bytes = new MemoryStream();
            var buffer = new byte[1024];
            int read;
            while ((read = await context.Request.Body.ReadAsync(buffer, timeout.Token)) > 0)
            {
                if (bytes.Length + read > CapabilitySession.MaximumBodyBytes) throw new CapabilityDenied("request_too_large");
                bytes.Write(buffer, 0, read);
            }
            using var document = JsonDocument.Parse(bytes.ToArray(), new JsonDocumentOptions { MaxDepth = 3 });
            var root = document.RootElement;
            if (context.Request.Path == "/v1/pair")
            {
                CheckFields(root, "version", "nonce");
                var grant = session.CompletePairing(origin, root.GetProperty("nonce").GetString() ?? "");
                await context.Response.WriteAsJsonAsync(new { version = 1, bearer = grant.Bearer, expires = grant.Expires }, timeout.Token);
            }
            else if (context.Request.Path == "/v1/invoke")
            {
                CheckFields(root, "version", "sequence", "capability");
                var capability = ParseCapability(root.GetProperty("capability").GetString());
                var authorization = context.Request.Headers.Authorization.ToString();
                if (!authorization.StartsWith("Bearer ", StringComparison.Ordinal)) throw new CapabilityDenied("unauthorized");
                using var lease = session.Authorize(origin, authorization[7..], root.GetProperty("sequence").GetInt64(), capability);
                using var invocation = CancellationTokenSource.CreateLinkedTokenSource(timeout.Token, lease.CancellationToken);
                var result = await adapters[capability].InvokeAsync(invocation.Token);
                // Only this fixed response shape crosses the boundary. No arbitrary adapter JSON.
                if (result.School is { } school && (capability != LocalCapability.SchoolState || school.DisplayName.Length > 128 || school.DisplayName.Any(char.IsControl)))
                    throw new CapabilityDenied("invalid_native_result");
                if (capability == LocalCapability.SchoolState && result.School is null) throw new CapabilityDenied("invalid_native_result");
                invocation.Token.ThrowIfCancellationRequested();
                await context.Response.WriteAsJsonAsync(new { version = 1, result }, invocation.Token);
            }
            else context.Response.StatusCode = 404;
        }
        catch (CapabilityDenied) { context.Response.StatusCode = 403; }
        catch (Exception exception) when (exception is JsonException or InvalidOperationException or FormatException or KeyNotFoundException or OverflowException)
        { context.Response.StatusCode = 400; }
        catch (OperationCanceledException) { context.Response.StatusCode = 408; }
        catch (Exception) { context.Response.StatusCode = 503; }
        finally { Interlocked.Exchange(ref admitted, 0); }
    }

    private static void CheckFields(JsonElement root, params string[] fields)
    {
        if (root.ValueKind != JsonValueKind.Object) throw new CapabilityDenied("invalid_request");
        var actual = root.EnumerateObject().Select(property => property.Name).ToArray();
        if (actual.Length != fields.Length || actual.Distinct(StringComparer.Ordinal).Count() != actual.Length ||
            actual.Except(fields, StringComparer.Ordinal).Any() || root.GetProperty("version").GetInt32() != 1)
            throw new CapabilityDenied("invalid_request");
    }
    private static LocalCapability ParseCapability(string? value) => value switch
    {
        "school.state" => LocalCapability.SchoolState,
        "school.manage" => LocalCapability.SchoolManage,
        "vault.manage" => LocalCapability.VaultManage,
        "converter.manage" => LocalCapability.ConverterManage,
        "ollama.manage" => LocalCapability.OllamaManage,
        _ => throw new CapabilityDenied("unknown_capability")
    };

    public async ValueTask DisposeAsync()
    {
        if (Interlocked.Exchange(ref disposed, 1) != 0) return;
        session.Dispose();
        await lifetime.CancelAsync();
        if (app is not null) { await app.StopAsync(); await app.DisposeAsync(); }
        lifetime.Dispose();
    }
}
