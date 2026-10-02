using System.Collections.Concurrent;
using System.Diagnostics;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text.Json.Nodes;

namespace BambuAutomation;

/// <summary>Runs only the bundled, fixed native slicer executable, never caller-supplied commands.</summary>
public sealed class SliceJobs(Workspace workspace, string nativeExecutable) : IDisposable
{
    private sealed class Job(string id)
    {
        public readonly object Sync = new();
        public string Id { get; } = id;
        public string State = "queued";
        public string? Error;
        public string? Output;
        public string? Hash;
        public string? InputHash;
        public string? Destination;
        public long Bytes;
        public int? ExitCode;
        public readonly CancellationTokenSource Cancellation = new();
    }
    private readonly ConcurrentDictionary<string, Job> jobs = new();
    private readonly SemaphoreSlim slots = new(1, 1);
    private readonly object admission = new();
    private bool disposed;
    public bool NativeAvailable => File.Exists(nativeExecutable);
    public JsonObject Start(JsonObject args)
    {
        var input = workspace.Resolve(Responses.Required(args, "path"));
        if (!Path.GetExtension(input).Equals(".3mf", StringComparison.OrdinalIgnoreCase))
            throw new CommandException("invalid_format", "Headless slicing requires a configured 3MF project.");
        var overwrite = Responses.Flag(args, "overwrite");
        var output = workspace.Resolve(Responses.Required(args, "output"), true, overwrite);
        if (!output.EndsWith(".3mf", StringComparison.OrdinalIgnoreCase))
            throw new CommandException("invalid_format", "Headless output must be a .3mf file containing sliced G-code.");
        if (input.Equals(output, StringComparison.OrdinalIgnoreCase))
            throw new CommandException("invalid_path", "Input and output must be different files.");
        var plate = args["plate"]?.GetValue<int>() ?? 0;
        if (plate < 0 || plate > 1000) throw new CommandException("invalid_arguments", "plate must be 0 (all) or a positive plate number up to 1000.");
        if (!File.Exists(nativeExecutable)) throw new CommandException("native_missing", "The packaged bambu-studio.exe is unavailable.");
        Workspace.CheckAncestors(nativeExecutable);
        lock (admission)
        {
            if (disposed) throw new CommandException("service_stopping", "The service is stopping.");
            if (jobs.Values.Count(job => job.State is "queued" or "running") >= 4)
                throw new CommandException("queue_full", "The bounded headless queue is full (one running, three waiting).");
            if (jobs.Count >= 128) throw new CommandException("job_limit", "The service has retained 128 job records; restart after collecting results.");
            if (jobs.Values.Any(existing => existing.Destination == output && existing.State is "queued" or "running"))
                throw new CommandException("output_busy", "An active headless job already owns that output path.");
            var job = new Job("headless-" + Guid.NewGuid().ToString("N"));
            job.Destination = output;
            var directory = Path.Combine(Path.GetDirectoryName(output)!, ".bambu-automation-jobs", job.Id);
            Workspace.CheckAncestors(directory);
            Directory.CreateDirectory(directory);
            Workspace.CheckAncestors(directory);
            var snapshot = Path.Combine(directory, "input.3mf");
            // Hold the original against concurrent writes/deletion while making an immutable queued input.
            using (var source = new FileStream(input, FileMode.Open, FileAccess.Read, FileShare.Read))
            using (var destination = new FileStream(snapshot, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                source.CopyTo(destination);
            using (var source = File.OpenRead(snapshot))
                job.InputHash = Convert.ToHexString(SHA256.HashData(source)).ToLowerInvariant();
            jobs[job.Id] = job;
            _ = RunAsync(job, snapshot, output, overwrite, plate);
            return Snapshot(job);
        }
    }
    public JsonObject Status(string id) => Snapshot(Find(id));
    public JsonObject Cancel(string id)
    {
        var job = Find(id);
        lock (job.Sync)
        {
            if (job.State is "queued" or "running") job.Cancellation.Cancel();
            return Snapshot(job);
        }
    }
    private Job Find(string id) => jobs.TryGetValue(id, out var job) ? job : throw new CommandException("job_not_found", "Unknown headless job ID.");
    private static JsonObject Snapshot(Job job)
    {
        lock (job.Sync) return new JsonObject { ["jobId"] = job.Id, ["state"] = job.State,
            ["progress"] = job.State == "completed" ? 100 : 0, ["progressKind"] = "indeterminate_until_complete",
            ["exitCode"] = job.ExitCode, ["output"] = job.Output, ["sha256"] = job.Hash, ["inputSha256"] = job.InputHash, ["bytes"] = job.Bytes, ["error"] = job.Error };
    }
    private async Task RunAsync(Job job, string input, string output, bool overwrite, int plate)
    {
        var acquired = false;
        var directory = Path.Combine(Path.GetDirectoryName(output)!, ".bambu-automation-jobs", job.Id);
        try
        {
            await slots.WaitAsync(job.Cancellation.Token);
            acquired = true;
            lock (job.Sync) job.State = "running";
            Workspace.CheckAncestors(directory);
            Directory.CreateDirectory(directory);
            Workspace.CheckAncestors(directory);
            workspace.Resolve(input);
            var nativeOutput = Path.Combine(directory, "sliced.3mf");
            var info = new ProcessStartInfo(nativeExecutable) { UseShellExecute = false, CreateNoWindow = true,
                RedirectStandardOutput = true, RedirectStandardError = true, WorkingDirectory = directory };
            // A fresh data directory isolates settings and keeps headless execution out of the interactive profile.
            foreach (var arg in new[] { "--datadir", Path.Combine(directory, "profile"), "--outputdir", directory,
                "--slice", plate.ToString(System.Globalization.CultureInfo.InvariantCulture), "--export-3mf", "sliced.3mf", input })
                info.ArgumentList.Add(arg);
            info.Environment.Remove("BAMBU_AUTOMATION");
            using var process = Process.Start(info) ?? throw new CommandException("launch_failed", "The bundled slicer could not start.");
            using var registration = job.Cancellation.Token.Register(() => { try { if (!process.HasExited) process.Kill(true); } catch (InvalidOperationException) { } });
            using var limit = CancellationTokenSource.CreateLinkedTokenSource(job.Cancellation.Token);
            limit.CancelAfter(TimeSpan.FromMinutes(30));
            var stdout = DrainAsync(process.StandardOutput, limit.Token);
            var stderr = DrainAsync(process.StandardError, limit.Token);
            try { await process.WaitForExitAsync(limit.Token); }
            catch (OperationCanceledException) { try { if (!process.HasExited) process.Kill(true); } catch (InvalidOperationException) { } throw; }
            await Task.WhenAll(stdout, stderr);
            lock (job.Sync) job.ExitCode = process.ExitCode;
            job.Cancellation.Token.ThrowIfCancellationRequested();
            if (process.ExitCode != 0) throw new CommandException("slice_failed", "Native slicer returned a nonzero exit code; inspect the isolated job result.json.");
            ValidateSlicedArchive(nativeOutput);
            workspace.Resolve(output, true, overwrite);
            File.Move(nativeOutput, output, overwrite);
            var file = new FileInfo(output);
            using var data = File.OpenRead(output);
            var hash = Convert.ToHexString(await SHA256.HashDataAsync(data, job.Cancellation.Token)).ToLowerInvariant();
            lock (job.Sync) { job.Output = output; job.Hash = hash; job.Bytes = file.Length; job.State = "completed"; }
        }
        catch (OperationCanceledException)
        { lock (job.Sync) { job.State = job.Cancellation.IsCancellationRequested ? "cancelled" : "failed"; job.Error = job.Cancellation.IsCancellationRequested ? "cancelled" : "slice_timeout"; } }
        catch (Exception exception) when (exception is CommandException or IOException or UnauthorizedAccessException or InvalidDataException or System.ComponentModel.Win32Exception)
        { lock (job.Sync) { job.State = "failed"; job.Error = exception is CommandException command ? command.Code : "slice_io_error"; } }
        finally { if (acquired) slots.Release(); }
    }
    private static async Task DrainAsync(StreamReader reader, CancellationToken cancellationToken)
    {
        var buffer = new char[4096];
        while (await reader.ReadAsync(buffer, cancellationToken) != 0) { }
    }
    public static void ValidateSlicedArchive(string path)
    {
        if (!File.Exists(path) || new FileInfo(path).Length == 0) throw new CommandException("missing_output", "Native slicing produced no output.");
        using var archive = ZipFile.OpenRead(path);
        var gcode = archive.Entries.Where(entry => entry.FullName.EndsWith(".gcode", StringComparison.OrdinalIgnoreCase)).ToArray();
        if (gcode.Length == 0 || gcode.Any(entry => entry.Length == 0))
            throw new CommandException("invalid_output", "The exported archive contains no nonempty sliced G-code.");
        // Read every G-code entry completely so corrupted/truncated compressed output is rejected.
        foreach (var entry in gcode) { using var stream = entry.Open(); stream.CopyTo(Stream.Null); }
    }
    public void Dispose()
    {
        lock (admission) { disposed = true; foreach (var job in jobs.Values) job.Cancellation.Cancel(); }
    }
}
