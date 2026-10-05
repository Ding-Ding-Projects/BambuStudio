namespace BambuAutomation;

/// <summary>All callers use the same deny-by-default workspace boundary.</summary>
public sealed class Workspace
{
    public IReadOnlyList<string> Roots { get; }
    public Workspace(IEnumerable<string> roots)
    {
        Roots = roots.Select(Path.GetFullPath).Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        if (Roots.Count == 0) throw new CommandException("workspace_required", "At least one explicit --workspace directory is required.");
        foreach (var root in Roots)
        {
            if (!Directory.Exists(root)) throw new CommandException("workspace_missing", "A configured workspace does not exist.");
            CheckAncestors(root);
        }
    }

    public string Resolve(string path, bool output = false, bool overwrite = false)
    {
        if (!Path.IsPathFullyQualified(path) || path.Any(char.IsControl))
            throw new CommandException("invalid_path", "Use an absolute workspace path.");
        var full = Path.GetFullPath(path);
        // Alternate data streams and device namespaces must never reach the native loader.
        if (full.StartsWith(@"\\", StringComparison.Ordinal) || full[(Path.GetPathRoot(full)?.Length ?? 0)..].Contains(':'))
            throw new CommandException("invalid_path", "Network, device, and alternate-stream paths are unsupported.");
        if (!Roots.Any(root => full.StartsWith(Path.TrimEndingDirectorySeparator(root) + Path.DirectorySeparatorChar,
                StringComparison.OrdinalIgnoreCase)))
            throw new CommandException("outside_workspace", "The path is outside the configured workspace roots.");
        CheckAncestors(full);
        if (output)
        {
            if (!Directory.Exists(Path.GetDirectoryName(full)))
                throw new CommandException("parent_missing", "The output parent directory must already exist.");
            if (Directory.Exists(full)) throw new CommandException("invalid_path", "An output file is required.");
            if (File.Exists(full) && !overwrite)
                throw new CommandException("overwrite_required", "The output exists; set overwrite=true explicitly.");
        }
        else if (!File.Exists(full)) throw new CommandException("file_missing", "The input file does not exist.");
        return full;
    }

    public static void CheckAncestors(string path)
    {
        string? current = Path.GetFullPath(path);
        while (current is not null)
        {
            if (File.Exists(current) || Directory.Exists(current))
            {
                var attributes = File.GetAttributes(current);
                if ((attributes & FileAttributes.ReparsePoint) != 0)
                    throw new CommandException("reparse_point", "Reparse points and symbolic links are not permitted in workspace paths.");
            }
            current = Path.GetDirectoryName(current);
        }
    }
}
