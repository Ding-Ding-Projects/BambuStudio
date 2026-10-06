using System.Security.Cryptography;
using System.Text;

namespace BambuAutomation.LocalCapabilities;

// This boundary is independent of the MCP listener and accepts no command names or paths.
public enum LocalCapability { SchoolState, SchoolManage, VaultManage, ConverterManage, OllamaManage }

public sealed class CapabilityDenied(string code) : Exception(code)
{
    public string Code { get; } = code;
}

public sealed class PairingOffer
{
    internal PairingOffer(string nonce, DateTimeOffset expires) { Nonce = nonce; Expires = expires; }
    public string Nonce { get; }
    public DateTimeOffset Expires { get; }
    public override string ToString() => nameof(PairingOffer);
}

public sealed class PairingGrant
{
    internal PairingGrant(string bearer, DateTimeOffset expires) { Bearer = bearer; Expires = expires; }
    public string Bearer { get; }
    public DateTimeOffset Expires { get; }
    public override string ToString() => nameof(PairingGrant);
}

/// <summary>
/// Single paired origin, single native instance, bounded lifetime. The native UI must call
/// BeginPairing only from its explicit confirmation handler after showing the exact origin
/// and grant list. No HTTP route may call BeginPairing or supply its consent decision.
/// </summary>
public sealed class CapabilitySession : IDisposable
{
    public const int MaximumBodyBytes = 4096;
    public const int MaximumRequestsPerMinute = 30;
    private readonly object sync = new();
    private readonly TimeProvider clock;
    private readonly HashSet<LocalCapability> registered;
    private HashSet<LocalCapability> grants = [];
    private byte[]? nonceHash, bearerHash;
    private string? origin;
    private DateTimeOffset pairingExpiry, sessionExpiry;
    private long pairingStart, sessionStart, window;
    private long sequence;
    private int requests;
    private bool executing, disposed;
    private CancellationTokenSource revocation = new();

    public CapabilitySession(IEnumerable<LocalCapability> registered, TimeProvider? clock = null)
    {
        this.registered = registered.ToHashSet();
        if (this.registered.Any(value => !Enum.IsDefined(value))) throw new ArgumentException("Unknown capability.");
        this.clock = clock ?? TimeProvider.System;
    }

    public static bool IsExactHttpsOrigin(string value)
    {
        return value.Length <= 253 && Uri.TryCreate(value, UriKind.Absolute, out var uri) &&
            uri.Scheme == "https" && uri.UserInfo.Length == 0 && uri.Query.Length == 0 &&
            uri.Fragment.Length == 0 && uri.AbsolutePath == "/" &&
            uri.GetLeftPart(UriPartial.Authority).Equals(value, StringComparison.Ordinal) &&
            uri.HostNameType == UriHostNameType.Dns && !uri.IsLoopback;
    }

    public PairingOffer BeginPairing(string exactOrigin, IEnumerable<LocalCapability> requested)
    {
        lock (sync)
        {
            CheckAlive();
            if (executing) throw new CapabilityDenied("busy");
            var selected = requested.ToHashSet();
            if (!IsExactHttpsOrigin(exactOrigin) || selected.Count == 0 || !selected.IsSubsetOf(registered))
                throw new CapabilityDenied("invalid_pairing");
            RevokeCore();
            origin = exactOrigin;
            grants = selected;
            var nonce = NewSecret();
            nonceHash = Hash(nonce);
            pairingExpiry = clock.GetUtcNow().AddMinutes(1);
            pairingStart = clock.GetTimestamp();
            return new PairingOffer(nonce, pairingExpiry);
        }
    }

    public bool AcceptsOrigin(string? candidate)
    {
        lock (sync) return !disposed && origin is not null && candidate == origin &&
            (nonceHash is not null && clock.GetElapsedTime(pairingStart) < TimeSpan.FromMinutes(1) ||
             bearerHash is not null && clock.GetElapsedTime(sessionStart) < TimeSpan.FromMinutes(10));
    }

    public PairingGrant CompletePairing(string candidateOrigin, string nonce)
    {
        lock (sync)
        {
            CheckAlive();
            if (origin != candidateOrigin || nonceHash is null || clock.GetElapsedTime(pairingStart) >= TimeSpan.FromMinutes(1))
                throw new CapabilityDenied("pairing_unavailable");
            // Each attempted redemption consumes the offer; guessing cannot be retried.
            var expected = nonceHash;
            nonceHash = null;
            var valid = Matches(expected, nonce);
            CryptographicOperations.ZeroMemory(expected);
            if (!valid) { RevokeCore(); throw new CapabilityDenied("pairing_unavailable"); }
            var bearer = NewSecret();
            bearerHash = Hash(bearer);
            sessionExpiry = clock.GetUtcNow().AddMinutes(10);
            sessionStart = window = clock.GetTimestamp();
            sequence = 0;
            requests = 0;
            return new PairingGrant(bearer, sessionExpiry);
        }
    }

    public CapabilityLease Authorize(string candidateOrigin, string bearer, long nextSequence, LocalCapability capability)
    {
        lock (sync)
        {
            CheckAlive();
            if (origin != candidateOrigin || bearerHash is null || clock.GetElapsedTime(sessionStart) >= TimeSpan.FromMinutes(10) || !Matches(bearerHash, bearer))
                throw new CapabilityDenied("unauthorized");
            if (!grants.Contains(capability)) throw new CapabilityDenied("capability_not_granted");
            if (sequence == long.MaxValue || nextSequence != sequence + 1) throw new CapabilityDenied("invalid_sequence");
            if (executing) throw new CapabilityDenied("busy");
            if (clock.GetElapsedTime(window) >= TimeSpan.FromMinutes(1)) { window = clock.GetTimestamp(); requests = 0; }
            if (requests >= MaximumRequestsPerMinute) throw new CapabilityDenied("rate_limited");
            ++requests;
            sequence = nextSequence;
            executing = true;
            return new CapabilityLease(this, revocation.Token);
        }
    }

    public void Revoke() { lock (sync) { if (!disposed) RevokeCore(); } }
    public void Dispose() { lock (sync) { if (disposed) return; RevokeCore(); revocation.Dispose(); disposed = true; } }
    private void CheckAlive() { if (disposed) throw new CapabilityDenied("session_closed"); }
    private void RevokeCore()
    {
        if (nonceHash is not null) CryptographicOperations.ZeroMemory(nonceHash);
        if (bearerHash is not null) CryptographicOperations.ZeroMemory(bearerHash);
        nonceHash = bearerHash = null;
        origin = null;
        grants.Clear();
        try { revocation.Cancel(); }
        catch (AggregateException) { /* A callback cannot prevent credential invalidation. */ }
        finally { revocation.Dispose(); revocation = new CancellationTokenSource(); }
    }
    private static string NewSecret() => Convert.ToHexString(RandomNumberGenerator.GetBytes(32));
    private static byte[] Hash(string value) => SHA256.HashData(Encoding.UTF8.GetBytes(value));
    private static bool Matches(byte[] expected, string candidate) => candidate.Length == 64 &&
        candidate.All(char.IsAsciiHexDigit) && CryptographicOperations.FixedTimeEquals(expected, Hash(candidate));
    public sealed class CapabilityLease : IDisposable
    {
        private CapabilitySession? session;
        internal CapabilityLease(CapabilitySession owner, CancellationToken cancellationToken)
        { session = owner; CancellationToken = cancellationToken; }
        public CancellationToken CancellationToken { get; }
        public void Dispose()
        {
            var value = Interlocked.Exchange(ref session, null);
            if (value is not null) lock (value.sync) value.executing = false;
        }
    }
}
