using BambuAutomation.LocalCapabilities;
using System.Net;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using BambuAutomation;

int checks = 0;
void Check(bool condition) { ++checks; if (!condition) throw new Exception($"Check {checks} failed."); }
void Denied(Action action, string code)
{
    try { action(); throw new Exception("Expected rejection."); }
    catch (CapabilityDenied exception) { Check(exception.Code == code); }
}
const string origin = "https://companion.example";
var clock = new FakeClock();
using var session = new CapabilitySession([LocalCapability.SchoolState], clock);
foreach (var invalid in new[] { "http://companion.example", "https://companion.example/", "https://companion.example/path", "https://companion.example?x", "https://user@companion.example", "https://localhost", "null", "https://127.0.0.1", "https://companion.example#x" })
    Check(!CapabilitySession.IsExactHttpsOrigin(invalid));
Check(CapabilitySession.IsExactHttpsOrigin(origin));
Check(!session.AcceptsOrigin(origin));
Denied(() => session.BeginPairing(origin, [LocalCapability.VaultManage]), "invalid_pairing");
var offer = session.BeginPairing(origin, [LocalCapability.SchoolState]);
Check(session.AcceptsOrigin(origin));
Check(!session.AcceptsOrigin("https://other.example"));
Denied(() => session.CompletePairing("https://other.example", offer.Nonce), "pairing_unavailable");
var grant = session.CompletePairing(origin, offer.Nonce);
Denied(() => session.CompletePairing(origin, offer.Nonce), "pairing_unavailable");
Denied(() => session.Authorize(origin, new string('A',64), 1, LocalCapability.SchoolState), "unauthorized");
Denied(() => session.Authorize(origin, grant.Bearer, 1, LocalCapability.VaultManage), "capability_not_granted");
Denied(() => session.Authorize(origin, grant.Bearer, 2, LocalCapability.SchoolState), "invalid_sequence");
using (session.Authorize(origin, grant.Bearer, 1, LocalCapability.SchoolState))
    Denied(() => session.Authorize(origin, grant.Bearer, 2, LocalCapability.SchoolState), "busy");
Denied(() => session.Authorize(origin, grant.Bearer, 1, LocalCapability.SchoolState), "invalid_sequence");
for (var sequence = 2; sequence <= 30; ++sequence)
    using (session.Authorize(origin, grant.Bearer, sequence, LocalCapability.SchoolState)) { }
Denied(() => session.Authorize(origin, grant.Bearer, 31, LocalCapability.SchoolState), "rate_limited");
clock.Advance(TimeSpan.FromMinutes(1));
using (session.Authorize(origin, grant.Bearer, 31, LocalCapability.SchoolState)) { }
clock.Advance(TimeSpan.FromMinutes(9));
Denied(() => session.Authorize(origin, grant.Bearer, 32, LocalCapability.SchoolState), "unauthorized");
Check(!session.AcceptsOrigin(origin));
offer = session.BeginPairing(origin, [LocalCapability.SchoolState]);
clock.Advance(TimeSpan.FromMinutes(1));
Denied(() => session.CompletePairing(origin, offer.Nonce), "pairing_unavailable");
offer = session.BeginPairing(origin, [LocalCapability.SchoolState]);
Denied(() => session.CompletePairing(origin, "invalid"), "pairing_unavailable");
Denied(() => session.CompletePairing(origin, offer.Nonce), "pairing_unavailable");
offer = session.BeginPairing(origin, [LocalCapability.SchoolState]);
grant = session.CompletePairing(origin, offer.Nonce);
session.Revoke();
Denied(() => session.Authorize(origin, grant.Bearer, 1, LocalCapability.SchoolState), "unauthorized");
offer = session.BeginPairing(origin, [LocalCapability.SchoolState]);
grant = session.CompletePairing(origin, offer.Nonce);
using (var pending = session.Authorize(origin, grant.Bearer, 1, LocalCapability.SchoolState))
{
    Denied(() => session.BeginPairing(origin, [LocalCapability.SchoolState]), "busy");
    Check(!pending.CancellationToken.IsCancellationRequested);
    session.Revoke();
    Check(pending.CancellationToken.IsCancellationRequested);
}
session.Dispose();
Denied(() => session.BeginPairing(origin, [LocalCapability.SchoolState]), "session_closed");

var adapter = new CountingAdapter();
await using var host = new LocalCapabilityHost([adapter]);
await host.StartAsync();
Check(host.Endpoint!.Host == "127.0.0.1");
using var client = new HttpClient { BaseAddress = host.Endpoint };
async Task<HttpResponseMessage> Send(string path, string body, string? bearer = null, string? requestOrigin = origin, string? requestHost = null)
{
    var request = new HttpRequestMessage(HttpMethod.Post, path) { Content = new StringContent(body, Encoding.UTF8) };
    request.Headers.ConnectionClose = true;
    request.Content.Headers.ContentType = new("application/json");
    if (requestOrigin is not null) request.Headers.Add("Origin", requestOrigin);
    if (bearer is not null) request.Headers.Add("Authorization", "Bearer " + bearer);
    if (requestHost is not null) request.Headers.Host = requestHost;
    return await client.SendAsync(request);
}
Check((await Send("/v1/pair", "{}")).StatusCode == HttpStatusCode.Forbidden);
offer = host.BeginPairing(origin, [LocalCapability.SchoolState]);
var paired = await Send("/v1/pair", JsonSerializer.Serialize(new { version = 1, nonce = offer.Nonce }));
Check(paired.IsSuccessStatusCode);
using var pairDocument = JsonDocument.Parse(await paired.Content.ReadAsStringAsync());
var bearer = pairDocument.RootElement.GetProperty("bearer").GetString()!;
const string invoke = "{\"version\":1,\"sequence\":1,\"capability\":\"school.state\"}";
Check((await Send("/v1/invoke", invoke, bearer, requestHost: "rebind.example")).StatusCode == HttpStatusCode.Forbidden);
Check((await Send("/v1/invoke", invoke, bearer, requestOrigin: null)).StatusCode == HttpStatusCode.Forbidden);
Check((await Send("/v1/invoke", invoke, bearer, requestOrigin: "https://other.example")).StatusCode == HttpStatusCode.Forbidden);
Check((await Send("/v1/invoke?anything=1", invoke, bearer)).StatusCode == HttpStatusCode.Forbidden);
Check((await Send("/v1/invoke", "{\"version\":1,\"version\":1,\"sequence\":1,\"capability\":\"school.state\"}", bearer)).StatusCode == HttpStatusCode.Forbidden);
Check((await Send("/v1/invoke", "{\"version\":1,\"sequence\":1,\"capability\":\"school.state\",\"path\":\"x\"}", bearer)).StatusCode == HttpStatusCode.Forbidden);
Check((await Send("/v1/invoke", new string(' ', 4097), bearer)).StatusCode == HttpStatusCode.BadRequest);
Check((await Send("/v1/invoke", "{\"version\":2,\"sequence\":1,\"capability\":\"school.state\"}", bearer)).StatusCode == HttpStatusCode.Forbidden);
Check((await Send("/v1/invoke", "{\"version\":1,\"sequence\":1,\"capability\":\"shell.execute\"}", bearer)).StatusCode == HttpStatusCode.Forbidden);
Check((await Send("/v1/invoke", "[]", bearer)).StatusCode == HttpStatusCode.Forbidden);
using (var preflight = new HttpRequestMessage(HttpMethod.Options, "/v1/invoke"))
{
    preflight.Headers.Add("Origin", origin);
    preflight.Headers.Add("Access-Control-Request-Method", "POST");
    preflight.Headers.Add("Access-Control-Request-Headers", "content-type,authorization");
    using var answer = await client.SendAsync(preflight);
    Check(answer.StatusCode == HttpStatusCode.NoContent);
    Check(answer.Headers.GetValues("Access-Control-Allow-Origin").Single() == origin);
    Check(!answer.Headers.Contains("Access-Control-Allow-Credentials"));
}
Check(adapter.Calls == 0);
var success = await Send("/v1/invoke", invoke, bearer);
Check(success.IsSuccessStatusCode);
Check(adapter.Calls == 1);
Check(success.Headers.GetValues("Access-Control-Allow-Origin").Single() == origin);
Check((await Send("/v1/invoke", invoke, bearer)).StatusCode == HttpStatusCode.Forbidden);
Check(adapter.Calls == 1);
host.Revoke();
Check((await Send("/v1/invoke", invoke, bearer)).StatusCode == HttpStatusCode.Forbidden);
var bridge = new RecordingBridge();
var nativeAdapter = new NativeComposition.NativeAdapter(bridge, 123, "approval", LocalCapability.SchoolState);
var nativeResult = await nativeAdapter.InvokeAsync(CancellationToken.None);
Check(nativeResult.School?.Enabled == true);
Check(bridge.Instance == 123 && bridge.Operation == "local_capabilities");
Check(bridge.Arguments?.Count == 3);
Check(bridge.Arguments?["action"]?.GetValue<string>() == "invoke");
Check(bridge.Arguments?["capability"]?.GetValue<string>() == "school.state");
using(var cancelled = new CancellationTokenSource())
{
    cancelled.Cancel();
    try { await nativeAdapter.InvokeAsync(cancelled.Token); throw new Exception("Expected cancellation."); }
    catch(OperationCanceledException) { Check(bridge.Calls == 1); }
}
Console.WriteLine($"Local capability boundary: {checks} checks passed.");

sealed class FakeClock : TimeProvider
{
    private DateTimeOffset now = DateTimeOffset.Parse("2026-01-01T00:00:00Z");
    private long timestamp;
    public override DateTimeOffset GetUtcNow() => now;
    public override long TimestampFrequency => TimeSpan.TicksPerSecond;
    public override long GetTimestamp() => timestamp;
    public void Advance(TimeSpan amount) { now += amount; timestamp += amount.Ticks; }
}
// This synthetic adapter proves transport boundaries only, never native integration.
sealed class CountingAdapter : ILocalCapabilityAdapter
{
    public LocalCapability Capability => LocalCapability.SchoolState;
    public int Calls { get; private set; }
    public Task<CapabilityResult> InvokeAsync(CancellationToken cancellationToken)
    { ++Calls; return Task.FromResult(new CapabilityResult(new SchoolSnapshot(false, "Example mode"))); }
}
sealed class RecordingBridge : INativeBridge
{
    public int Instance, Calls;
    public string? Operation;
    public JsonObject? Arguments;
    public IReadOnlyList<int> Instances() => [];
    public Task<JsonObject> InvokeAsync(int instance, string operation, JsonObject arguments, CancellationToken cancellationToken)
    {
        ++Calls; Instance = instance; Operation = operation; Arguments = arguments;
        return Task.FromResult(new JsonObject { ["enabled"] = true, ["displayName"] = "Example mode" });
    }
}
