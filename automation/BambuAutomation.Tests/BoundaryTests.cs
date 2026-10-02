using System.IO.Compression;
using System.IO.Pipes;
using System.Text;
using System.Text.Json.Nodes;
using BambuAutomation;
using Xunit;

namespace BambuAutomation.Tests;

public sealed class BoundaryTests : IDisposable
{
    private readonly string root = Path.Combine(Path.GetTempPath(), "bambu-automation-tests-" + Guid.NewGuid().ToString("N"));
    public BoundaryTests() => Directory.CreateDirectory(root);
    private Workspace Workspace => new([root]);
    public void Dispose() => Directory.Delete(root, true);

    [Fact]
    public void RootIsExplicit() => Assert.Equal("workspace_required", Assert.Throws<CommandException>(() => new Workspace([])).Code);

    [Fact]
    public void TraversalAndSiblingPrefixAreRejected()
    {
        Assert.Equal("outside_workspace", Assert.Throws<CommandException>(() => Workspace.Resolve(Path.Combine(root, "..", "outside.3mf"))).Code);
        Assert.Equal("outside_workspace", Assert.Throws<CommandException>(() => Workspace.Resolve(root + "-sibling/model.3mf")).Code);
    }

    [Fact]
    public void ExistingOutputRequiresExplicitOverwrite()
    {
        var path = Path.Combine(root, "existing.3mf"); File.WriteAllText(path, "existing");
        Assert.Equal("overwrite_required", Assert.Throws<CommandException>(() => Workspace.Resolve(path, true)).Code);
        Assert.Equal(path, Workspace.Resolve(path, true, true));
    }

    [Fact]
    public void AlternateStreamsAreRejected()
    {
        Assert.Equal("invalid_path", Assert.Throws<CommandException>(() => Workspace.Resolve(Path.Combine(root, "file.3mf:secret"))).Code);
    }

    [Theory]
    [InlineData("http://0.0.0.0:8766", "https_required")]
    [InlineData("http://192.168.1.5:8766", "https_required")]
    [InlineData("http://127.0.0.1:8766/path", "invalid_endpoint")]
    [InlineData("http://user:password@127.0.0.1:8766", "invalid_endpoint")]
    public void UnsafeListenerIsRejected(string endpoint, string code)
        => Assert.Equal(code, Assert.Throws<CommandException>(() => HttpSecurity.ValidateEndpoint(endpoint)).Code);

    [Fact]
    public void HttpRequiresExactHostOriginAndBearer()
    {
        var security = new HttpSecurity(HttpSecurity.ValidateEndpoint("http://127.0.0.1:8766"), new string('x', 48));
        Assert.True(security.AcceptHost("127.0.0.1:8766"));
        Assert.False(security.AcceptHost(null));
        Assert.False(security.AcceptHost("evil.example:8766"));
        Assert.True(security.AcceptOrigin(null));
        Assert.True(security.AcceptOrigin("http://127.0.0.1:8766"));
        Assert.False(security.AcceptOrigin("https://evil.example"));
        Assert.True(security.AcceptBearer("Bearer " + new string('x', 48)));
        Assert.False(security.AcceptBearer("Bearer " + new string('y', 48)));
        Assert.False(security.AcceptBearer(null));
    }

    [Theory]
    [InlineData("--bearer", "secret")]
    [InlineData("--token", "secret")]
    public void SecretsCannotBePassedAsOptions(string option, string value)
        => Assert.Throws<CommandException>(() => Options.Parse(["serve", option, value]));

    [Fact]
    public async Task ProtocolRequiresCorrelatedVersionedResponse()
    {
        var bytes = Encoding.UTF8.GetBytes("{\"version\":1,\"id\":\"request\",\"ok\":true,\"result\":{\"status\":\"ready\"}}\n");
        var response = await NativeBridge.ReadFrameAsync(new MemoryStream(bytes), default);
        Assert.Equal("ready", NativeBridge.ValidateResponse(response, "request")["status"]!.GetValue<string>());
        Assert.Equal("invalid_response", Assert.Throws<CommandException>(() => NativeBridge.ValidateResponse(response, "other")).Code);
    }

    [Fact]
    public async Task OversizedNativeResponseIsRejected()
    {
        var bytes = Enumerable.Repeat((byte)'x', NativeBridge.MaximumFrame + 1).Append((byte)'\n').ToArray();
        var error = await Assert.ThrowsAsync<CommandException>(() => NativeBridge.ReadFrameAsync(new MemoryStream(bytes), default));
        Assert.Equal("response_too_large", error.Code);
    }

    [Fact]
    public async Task TruncatedNativeResponseIsRejected()
    {
        var error = await Assert.ThrowsAsync<CommandException>(() => NativeBridge.ReadFrameAsync(new MemoryStream(Encoding.UTF8.GetBytes("{}")), default));
        Assert.Equal("invalid_response", error.Code);
    }

    [Fact]
    public async Task RealNamedPipeCarriesFramedCorrelatedRequest()
    {
        var instance = Random.Shared.Next(100000, 999999);
        await using var server = new NamedPipeServerStream(NativeBridge.Prefix + instance, PipeDirection.InOut, 1,
            PipeTransmissionMode.Byte, PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(10));
        var serving = Task.Run(async () =>
        {
            await server.WaitForConnectionAsync(timeout.Token);
            var request = await NativeBridge.ReadFrameAsync(server, timeout.Token);
            Assert.Equal(1, request["version"]!.GetValue<int>());
            Assert.Equal("printer_status", request["operation"]!.GetValue<string>());
            Assert.Equal("fixture-printer", request["arguments"]!["printerId"]!.GetValue<string>());
            var response = new JsonObject { ["version"] = 1, ["id"] = request["id"]!.GetValue<string>(), ["ok"] = true,
                ["result"] = new JsonObject { ["state"] = "idle" } };
            await server.WriteAsync(Encoding.UTF8.GetBytes(response.ToJsonString() + "\n"), timeout.Token);
        });
        var result = await new NativeBridge().InvokeAsync(instance, "printer_status", new JsonObject { ["printerId"] = "fixture-printer" }, timeout.Token);
        Assert.Equal("idle", result["state"]!.GetValue<string>());
        await serving;
    }

    [Fact]
    public async Task AmbiguityDoesNotReachPrinter()
    {
        var bridge = new RecordingBridge([1, 2]); using var jobs = new SliceJobs(Workspace, Path.Combine(root, "missing.exe"));
        var service = new CommandService(Workspace, bridge, jobs);
        var result = await service.ExecuteAsync("printer_start", new JsonObject { ["printerId"] = "printer", ["requestId"] = "once", ["sliceJobId"] = "slice-1", ["path"] = Path.Combine(root, "staging.3mf") });
        Assert.Equal("ambiguous_instance", result["error"]!["code"]!.GetValue<string>());
        Assert.Empty(bridge.Calls);
    }

    [Fact]
    public async Task StartPrintRequiresPrinterAndRequestIdentity()
    {
        var bridge = new RecordingBridge([1]); using var jobs = new SliceJobs(Workspace, Path.Combine(root, "missing.exe"));
        var service = new CommandService(Workspace, bridge, jobs);
        foreach (var args in new[] { new JsonObject(), new JsonObject { ["printerId"] = "printer" } })
        {
            var result = await service.ExecuteAsync("printer_start", args);
            Assert.False(result["ok"]!.GetValue<bool>());
        }
        Assert.Empty(bridge.Calls);
    }

    [Fact]
    public async Task StartPrintPreservesDeduplicationIdentityWithoutExtraConfirmation()
    {
        var bridge = new RecordingBridge([1]); using var jobs = new SliceJobs(Workspace, Path.Combine(root, "missing.exe"));
        var service = new CommandService(Workspace, bridge, jobs);
        var args = new JsonObject { ["printerId"] = "printer", ["requestId"] = "one-intent", ["sliceJobId"] = "slice-1", ["instanceId"] = 1, ["path"] = Path.Combine(root, "staging.3mf") };
        var first = await service.ExecuteAsync("printer_start", args);
        // The actual native first submission creates its staging output before returning.
        File.WriteAllText(args["path"]!.GetValue<string>(), "submitted staging archive");
        var second = await service.ExecuteAsync("printer_start", args);
        Assert.True(first["ok"]!.GetValue<bool>()); Assert.True(second["ok"]!.GetValue<bool>());
        Assert.Equal(1, bridge.PrintStarts);
        Assert.Equal("one-intent", bridge.Calls[0]["requestId"]!.GetValue<string>());
        Assert.Null(bridge.Calls[0]["instanceId"]);
    }

    [Fact]
    public async Task ServiceCapabilitiesWorkWithoutNativeInstance()
    {
        var bridge = new RecordingBridge([]); using var jobs = new SliceJobs(Workspace, Path.Combine(root, "missing.exe"));
        var result = await new CommandService(Workspace, bridge, jobs).ExecuteAsync("capabilities", new JsonObject());
        Assert.True(result["ok"]!.GetValue<bool>());
        Assert.False(result["result"]!["nativeAttached"]!.GetValue<bool>());
        Assert.False(result["result"]!["headlessSlicing"]!["available"]!.GetValue<bool>());
        Assert.Empty(bridge.Calls);
    }

    [Fact]
    public async Task PrintStagingCannotEscapeWorkspace()
    {
        var bridge = new RecordingBridge([1]); using var jobs = new SliceJobs(Workspace, Path.Combine(root, "missing.exe"));
        var result = await new CommandService(Workspace, bridge, jobs).ExecuteAsync("printer_start", new JsonObject
        { ["printerId"] = "printer", ["requestId"] = "once", ["sliceJobId"] = "slice-1", ["path"] = Path.Combine(root, "..", "outside.3mf") });
        Assert.Equal("outside_workspace", result["error"]!["code"]!.GetValue<string>());
        Assert.Empty(bridge.Calls);
    }

    [Fact]
    public void UnslicedArchiveIsNotAValidOutput()
    {
        var path = Path.Combine(root, "unsliced.3mf");
        using (var zip = ZipFile.Open(path, ZipArchiveMode.Create))
        { using var writer = new StreamWriter(zip.CreateEntry("3D/3dmodel.model").Open()); writer.Write("model"); }
        Assert.Equal("invalid_output", Assert.Throws<CommandException>(() => SliceJobs.ValidateSlicedArchive(path)).Code);
    }

    [Fact]
    public async Task NativePlateAndSliceIdentityAreValidatedBeforeDispatch()
    {
        var bridge = new RecordingBridge([1]); using var jobs = new SliceJobs(Workspace, Path.Combine(root, "missing.exe"));
        var service = new CommandService(Workspace, bridge, jobs);
        foreach (var index in new JsonNode[] { JsonValue.Create(-1)!, JsonValue.Create(1.5)!, JsonValue.Create("1")! })
        {
            var result = await service.ExecuteAsync("slice_start", new JsonObject { ["plateIndex"] = index });
            Assert.Equal("invalid_arguments", result["error"]!["code"]!.GetValue<string>());
        }
        foreach (var job in new string?[] { null, "headless-other" })
        {
            var result = await service.ExecuteAsync("printer_start", new JsonObject
            { ["printerId"] = "printer", ["requestId"] = "once", ["sliceJobId"] = job, ["path"] = Path.Combine(root, "staging.3mf") });
            Assert.Equal("invalid_arguments", result["error"]!["code"]!.GetValue<string>());
        }
        var wrongMode = await service.ExecuteAsync("slice_start", new JsonObject { ["headless"] = true, ["plateIndex"] = 0 });
        Assert.Equal("invalid_arguments", wrongMode["error"]!["code"]!.GetValue<string>());
        Assert.Empty(bridge.Calls);
        var slice = await service.ExecuteAsync("slice_start", new JsonObject { ["plateIndex"] = 2 });
        Assert.True(slice["ok"]!.GetValue<bool>());
        Assert.Equal(2, bridge.Calls.Single()["plateIndex"]!.GetValue<int>());
    }

    [Fact]
    public async Task StlExportPreservesValidatedPlateAndWorkspacePath()
    {
        var bridge = new RecordingBridge([1]); using var jobs = new SliceJobs(Workspace, Path.Combine(root, "missing.exe"));
        var destination = Path.Combine(root, "plate.stl");
        var result = await new CommandService(Workspace, bridge, jobs).ExecuteAsync("export_file", new JsonObject
        { ["path"] = destination, ["plateIndex"] = 1 });
        Assert.True(result["ok"]!.GetValue<bool>());
        Assert.Equal(destination, bridge.Calls.Single()["path"]!.GetValue<string>());
        Assert.Equal(1, bridge.Calls.Single()["plateIndex"]!.GetValue<int>());
    }

    [Fact]
    public void NonemptyGcodeArchiveIsValidated()
    {
        var path = Path.Combine(root, "sliced.3mf");
        using (var zip = ZipFile.Open(path, ZipArchiveMode.Create))
        { using var writer = new StreamWriter(zip.CreateEntry("Metadata/plate_1.gcode").Open()); writer.Write("G28\n"); }
        SliceJobs.ValidateSlicedArchive(path);
    }

    private sealed class RecordingBridge(int[] instances) : INativeBridge
    {
        public List<JsonObject> Calls { get; } = [];
        private readonly HashSet<string> started = [];
        public int PrintStarts => started.Count;
        public IReadOnlyList<int> Instances() => instances;
        public Task<JsonObject> InvokeAsync(int instance, string operation, JsonObject arguments, CancellationToken cancellationToken)
        {
            Calls.Add((JsonObject)arguments.DeepClone());
            if (operation == "printer_start") started.Add(arguments["requestId"]!.GetValue<string>());
            return Task.FromResult(new JsonObject { ["state"] = "accepted" });
        }
    }
}
