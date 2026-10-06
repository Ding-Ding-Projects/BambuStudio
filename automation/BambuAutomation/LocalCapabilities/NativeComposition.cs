using System.Text.Json.Nodes;

namespace BambuAutomation.LocalCapabilities;

public static class NativeComposition
{
    public static string Name(LocalCapability capability) => capability switch
    {
        LocalCapability.SchoolState => "school.state",
        LocalCapability.SchoolManage => "school.manage",
        LocalCapability.VaultManage => "vault.manage",
        LocalCapability.ConverterManage => "converter.manage",
        LocalCapability.OllamaManage => "ollama.manage",
        _ => throw new CapabilityDenied("unknown_capability")
    };
    private static LocalCapability Parse(string name) => Enum.GetValues<LocalCapability>().Single(value => Name(value) == name);

    // Native owner launches the exact bundled executable with this fixed command and PID.
    // Nothing in this path approves consent or selects a different native instance.
    public static async Task RunAsync(int instance, CancellationToken cancellationToken)
    {
        if (!OperatingSystem.IsWindows() || instance <= 0) throw new CapabilityDenied("native_unavailable");
        var bridge = new NativeBridge();
        var claim = await bridge.InvokeAsync(instance, "local_capabilities", new JsonObject { ["action"] = "claim_pairing" }, cancellationToken);
        var approval = Responses.Required(claim, "approvalId");
        var origin = Responses.Required(claim, "origin");
        if (approval.Length != 64 || !approval.All(char.IsAsciiHexDigit) || !CapabilitySession.IsExactHttpsOrigin(origin) ||
            claim["grants"] is not JsonArray grants || grants.Count is < 1 or > 5) throw new CapabilityDenied("invalid_native_pairing");
        var selected = grants.Select(value => Parse(value!.GetValue<string>())).ToArray();
        await using var host = new LocalCapabilityHost(selected.Select(value => new NativeAdapter(bridge, instance, approval, value)));
        try
        {
            var offer = host.BeginPairing(origin, selected);
            await host.StartAsync(cancellationToken);
            var published = await bridge.InvokeAsync(instance, "local_capabilities", new JsonObject
            {
                ["action"] = "publish_offer", ["approvalId"] = approval,
                ["endpoint"] = host.Endpoint!.AbsoluteUri, ["nonce"] = offer.Nonce
            }, cancellationToken);
            if (published["published"]?.GetValue<bool>() != true) throw new CapabilityDenied("offer_unavailable");
            while (!cancellationToken.IsCancellationRequested)
            {
                await Task.Delay(TimeSpan.FromSeconds(1), cancellationToken);
                var status = await bridge.InvokeAsync(instance, "local_capabilities", new JsonObject { ["action"] = "status", ["approvalId"] = approval }, cancellationToken);
                if (status["active"]?.GetValue<bool>() != true) break;
            }
        }
        finally
        {
            host.Revoke();
            using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(2));
            try { await bridge.InvokeAsync(instance, "local_capabilities", new JsonObject { ["action"] = "revoke", ["approvalId"] = approval }, timeout.Token); }
            catch (Exception) { /* Native expiry and window teardown independently revoke consent. */ }
        }
    }

    public sealed class NativeAdapter(INativeBridge bridge, int instance, string approval, LocalCapability capability) : ILocalCapabilityAdapter
    {
        public LocalCapability Capability => capability;
        public async Task<CapabilityResult> InvokeAsync(CancellationToken cancellationToken)
        {
            cancellationToken.ThrowIfCancellationRequested();
            var result = await bridge.InvokeAsync(instance, "local_capabilities", new JsonObject
            { ["action"] = "invoke", ["approvalId"] = approval, ["capability"] = Name(capability) }, cancellationToken);
            cancellationToken.ThrowIfCancellationRequested();
            return capability == LocalCapability.SchoolState
                ? new CapabilityResult(new SchoolSnapshot(result["enabled"]!.GetValue<bool>(), result["displayName"]!.GetValue<string>()))
                : new CapabilityResult(Opened: result["opened"]!.GetValue<bool>());
        }
    }
}
