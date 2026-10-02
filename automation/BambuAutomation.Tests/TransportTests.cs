using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.Json.Nodes;
using BambuAutomation;
using Xunit;

namespace BambuAutomation.Tests;

/// <summary>Actual protocol exchanges against the compiled companion, without a native/printer substitute.</summary>
public sealed class TransportTests : IDisposable
{
    private readonly string root = Path.Combine(Path.GetTempPath(), "bambu-mcp-transport-" + Guid.NewGuid().ToString("N"));
    public TransportTests() => Directory.CreateDirectory(root);
    public void Dispose() => Directory.Delete(root, true);
    private static JsonObject Request(int id, string method, JsonObject? parameters = null) => new()
    { ["jsonrpc"] = "2.0", ["id"] = id, ["method"] = method, ["params"] = parameters ?? new JsonObject() };
    private static JsonObject Initialize() => Request(1, "initialize", new JsonObject
    {
        ["protocolVersion"] = "2025-03-26", ["capabilities"] = new JsonObject(),
        ["clientInfo"] = new JsonObject { ["name"] = "automation-transport-tests", ["version"] = "1" }
    });

    private Process Start(params string[] arguments)
    {
        var manifest = Path.Combine(AppContext.BaseDirectory, "companion-path.txt");
        Assert.True(File.Exists(manifest), "The build must record the actual companion apphost path.");
        var executable = File.ReadAllText(manifest).Trim();
        Assert.True(Path.IsPathFullyQualified(executable) && File.Exists(executable), "The recorded actual companion apphost must exist.");
        var info = new ProcessStartInfo(executable) { UseShellExecute = false, CreateNoWindow = true,
            RedirectStandardInput = true, RedirectStandardOutput = true, RedirectStandardError = true };
        // A self-contained Web companion must use its own runtime/dependency tree, not a DLL
        // copied into the test project's output with an unrelated test runtime configuration.
        info.WorkingDirectory = Path.GetDirectoryName(executable)!;
        foreach (var arg in arguments) info.ArgumentList.Add(arg);
        info.ArgumentList.Add("--workspace"); info.ArgumentList.Add(root);
        info.Environment.Remove("BAMBU_AUTOMATION");
        return Process.Start(info) ?? throw new InvalidOperationException("Companion did not start.");
    }

    [Fact]
    public async Task StdioNegotiatesListsAndCallsActualTools()
    {
        using var process = Start("serve");
        var stderr = process.StandardError.ReadToEndAsync();
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(30));
        try
        {
            await process.StandardInput.WriteLineAsync(Initialize().ToJsonString()); await process.StandardInput.FlushAsync();
            var initialized = await ReadResponse(process, stderr, 1, timeout.Token);
            Assert.NotNull(initialized["result"]!["protocolVersion"]);
            await process.StandardInput.WriteLineAsync("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}");
            await process.StandardInput.WriteLineAsync(Request(2, "tools/list").ToJsonString()); await process.StandardInput.FlushAsync();
            var listed = await ReadResponse(process, stderr, 2, timeout.Token);
            var tools = listed["result"]!["tools"]!.AsArray();
            Assert.Contains(tools, tool => tool?["name"]?.GetValue<string>() == "bambu_capabilities");
            await process.StandardInput.WriteLineAsync(Request(3, "tools/call", new JsonObject
            { ["name"] = "bambu_capabilities", ["arguments"] = new JsonObject { ["arguments"] = new JsonObject() } }).ToJsonString());
            await process.StandardInput.FlushAsync();
            var called = await ReadResponse(process, stderr, 3, timeout.Token);
            Assert.False(called["result"]!["isError"]!.GetValue<bool>());
            Assert.True(called["result"]!["structuredContent"]!["ok"]!.GetValue<bool>());
        }
        finally
        {
            process.StandardInput.Close();
            if (!process.HasExited) process.Kill(true);
            await process.WaitForExitAsync(); await stderr;
        }
    }

    private static async Task<JsonObject> ReadResponse(Process process, Task<string> stderr, int id, CancellationToken cancellationToken)
    {
        while (true)
        {
            var line = await process.StandardOutput.ReadLineAsync(cancellationToken);
            if (line is null)
            {
                await process.WaitForExitAsync(cancellationToken);
                Assert.Fail($"The actual companion exited before MCP response {id}, exit={process.ExitCode}. Startup diagnostics: {await stderr}");
            }
            var response = JsonNode.Parse(line!)!.AsObject();
            if (response["id"] is JsonValue value && value.TryGetValue<int>(out var actual) && actual == id) return response;
        }
    }

    [Fact]
    public async Task HttpNegotiatesAndRejectsUnauthorizedOrigin()
    {
        var listener = new TcpListener(IPAddress.Loopback, 0); listener.Start();
        var port = ((IPEndPoint)listener.LocalEndpoint).Port; listener.Stop();
        var origin = $"http://127.0.0.1:{port}";
        using var process = Start("serve", "--transport", "http", "--listen", origin, "--bearer-stdin");
        var stderr = process.StandardError.ReadToEndAsync();
        var stdout = process.StandardOutput.ReadToEndAsync();
        var secret = Convert.ToHexString(System.Security.Cryptography.RandomNumberGenerator.GetBytes(32));
        await process.StandardInput.WriteLineAsync(secret); process.StandardInput.Close();
        using var client = new HttpClient { Timeout = TimeSpan.FromSeconds(5) };
        try
        {
            var deadline = DateTime.UtcNow.AddSeconds(20);
            while (true)
            {
                try
                {
                    using var probe = await client.GetAsync(origin + "/mcp");
                    Assert.Equal(HttpStatusCode.Unauthorized, probe.StatusCode); break;
                }
                catch (HttpRequestException) when (DateTime.UtcNow < deadline && !process.HasExited) { await Task.Delay(100); }
                catch (HttpRequestException)
                {
                    if (process.HasExited)
                        Assert.Fail($"The actual HTTP companion exited before binding, exit={process.ExitCode}. Startup diagnostics: {await stderr}");
                    throw;
                }
            }
            using var forbidden = new HttpRequestMessage(HttpMethod.Post, origin + "/mcp");
            forbidden.Headers.Authorization = new("Bearer", secret); forbidden.Headers.Add("Origin", "https://evil.example");
            forbidden.Content = new StringContent(Initialize().ToJsonString(), Encoding.UTF8, "application/json");
            using var rejected = await client.SendAsync(forbidden);
            Assert.Equal(HttpStatusCode.Forbidden, rejected.StatusCode);
            var initialized = await Post(Initialize());
            Assert.NotNull(initialized["result"]!["protocolVersion"]);
            var tools = await Post(Request(2, "tools/list"));
            Assert.Contains(tools["result"]!["tools"]!.AsArray(), tool => tool?["name"]?.GetValue<string>() == "bambu_capabilities");
            var call = await Post(Request(3, "tools/call", new JsonObject
            { ["name"] = "bambu_capabilities", ["arguments"] = new JsonObject { ["arguments"] = new JsonObject() } }));
            Assert.True(call["result"]!["structuredContent"]!["ok"]!.GetValue<bool>());

            async Task<JsonObject> Post(JsonObject request)
            {
                using var message = new HttpRequestMessage(HttpMethod.Post, origin + "/mcp");
                message.Headers.Authorization = new("Bearer", secret); message.Headers.Add("Origin", origin);
                message.Headers.Add("MCP-Protocol-Version", "2025-03-26");
                message.Headers.Accept.ParseAdd("application/json"); message.Headers.Accept.ParseAdd("text/event-stream");
                message.Content = new StringContent(request.ToJsonString(), Encoding.UTF8, "application/json");
                using var response = await client.SendAsync(message);
                response.EnsureSuccessStatusCode();
                var text = await response.Content.ReadAsStringAsync();
                if (response.Content.Headers.ContentType?.MediaType == "text/event-stream")
                    text = string.Join("\n", text.Split('\n').Where(line => line.StartsWith("data:", StringComparison.Ordinal)).Select(line => line[5..].Trim()));
                return JsonNode.Parse(text)!.AsObject();
            }
        }
        finally
        {
            if (!process.HasExited) process.Kill(true);
            await process.WaitForExitAsync(); await stderr; await stdout;
        }
    }
}
