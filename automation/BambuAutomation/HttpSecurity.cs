using System.Net;
using System.Security.AccessControl;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Text;

namespace BambuAutomation;

public sealed class HttpSecurity(Uri endpoint, string bearer)
{
    private readonly byte[] expected = SHA256.HashData(Encoding.UTF8.GetBytes(bearer));
    public bool AcceptHost(string? host) => host is not null && host.Equals(endpoint.Authority, StringComparison.OrdinalIgnoreCase);
    public bool AcceptOrigin(string? origin) => string.IsNullOrEmpty(origin) || origin.Equals(endpoint.GetLeftPart(UriPartial.Authority), StringComparison.OrdinalIgnoreCase);
    public bool AcceptBearer(string? authorization)
    {
        if (authorization is null || !authorization.StartsWith("Bearer ", StringComparison.Ordinal) || authorization.Length > 4096) return false;
        var actual = SHA256.HashData(Encoding.UTF8.GetBytes(authorization[7..]));
        return CryptographicOperations.FixedTimeEquals(expected, actual);
    }
    public static Uri ValidateEndpoint(string text)
    {
        if (!Uri.TryCreate(text, UriKind.Absolute, out var endpoint) || endpoint.Scheme is not ("http" or "https") ||
            endpoint.AbsolutePath != "/" || endpoint.UserInfo.Length != 0 || endpoint.Query.Length != 0 || endpoint.Fragment.Length != 0)
            throw new CommandException("invalid_endpoint", "Use an HTTP(S) origin with no path, credentials, query, or fragment.");
        var loopback = endpoint.Host.Equals("localhost", StringComparison.OrdinalIgnoreCase) ||
            (IPAddress.TryParse(endpoint.Host.Trim('[', ']'), out var address) && IPAddress.IsLoopback(address));
        if (!loopback && endpoint.Scheme != "https")
            throw new CommandException("https_required", "Non-loopback listeners require HTTPS and a configured server certificate.");
        return endpoint;
    }
    public static async Task<string> ReadProtectedBearerAsync(string path)
    {
        Workspace.CheckAncestors(path);
        var file = new FileInfo(Path.GetFullPath(path));
        if (!file.Exists || file.Length > 4096) throw new CommandException("invalid_bearer_file", "A protected bearer file of at most 4096 bytes is required.");
        if (OperatingSystem.IsWindows())
        {
            var current = WindowsIdentity.GetCurrent().User ?? throw new CommandException("identity_unavailable", "Current Windows identity is unavailable.");
            var acl = file.GetAccessControl();
            if (!current.Equals(acl.GetOwner(typeof(SecurityIdentifier))))
                throw new CommandException("unprotected_bearer_file", "The bearer file must be owned by the current user.");
            foreach (FileSystemAccessRule rule in acl.GetAccessRules(true, true, typeof(SecurityIdentifier)))
            {
                if (rule.AccessControlType != AccessControlType.Allow || (rule.FileSystemRights & FileSystemRights.ReadData) == 0) continue;
                var sid = (SecurityIdentifier)rule.IdentityReference;
                if (!sid.Equals(current) && !sid.IsWellKnown(WellKnownSidType.LocalSystemSid) && !sid.IsWellKnown(WellKnownSidType.BuiltinAdministratorsSid))
                    throw new CommandException("unprotected_bearer_file", "The bearer file grants read access outside the current user, SYSTEM, or administrators.");
            }
        }
        else
        {
            var mode = File.GetUnixFileMode(file.FullName);
            if ((mode & (UnixFileMode.GroupRead | UnixFileMode.GroupWrite | UnixFileMode.GroupExecute | UnixFileMode.OtherRead | UnixFileMode.OtherWrite | UnixFileMode.OtherExecute)) != 0)
                throw new CommandException("unprotected_bearer_file", "The bearer file must have owner-only Unix permissions.");
        }
        return ValidateBearer((await File.ReadAllTextAsync(file.FullName)).TrimEnd('\r', '\n'));
    }
    public static string ValidateBearer(string value)
    {
        if (value.Length < 32 || value.Length > 4096 || value.Any(char.IsControl) || value.Any(char.IsWhiteSpace))
            throw new CommandException("invalid_bearer", "Use a randomly generated bearer secret with no whitespace.");
        return value;
    }
}
