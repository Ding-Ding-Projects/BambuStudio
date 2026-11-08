#ifndef slic3r_GUI_GameServers_ServerModel_hpp_
#define slic3r_GUI_GameServers_ServerModel_hpp_

// The rules of File > Minecraft servers, without doing anything: reading and
// writing server.properties so that every line the person or another tool
// wrote stays exactly as written, what each known setting is and which
// versions have it, version ids and what each version needs (Java, level
// types, game rules), the server folder's own files (EULA, allowlist,
// operators, bans), what the console prints, the Mojang version list, the
// command line a server starts with, backups, schedules, the world
// generator's settings and the batches it pre-generates an area in, the
// library of servers and its history, and SHA-1 for checking downloads.
// No wxWidgets and nothing that touches a process, the network or a zip
// file; tested by tests/game_servers_test.cpp.

#include <cstdint>
#include <ctime>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace GameServers {

// A message for a person: an English template marked for translation, in
// which {1}, {2} ... stand for the values. The page translates the template
// and fills it in; fill(text, values) gives the English.
struct Message
{
    std::string              text;
    std::vector<std::string> values;
    bool                     empty() const { return text.empty(); }
};
std::string fill(const std::string &templ, const std::vector<std::string> &values);
inline std::string english(const Message &m) { return fill(m.text, m.values); }

// ---------------------------------------------------------------------------
// Versions

struct VersionId
{
    enum class Kind { Release, PreRelease, ReleaseCandidate, Snapshot, OldBeta, OldAlpha, Other };
    Kind        kind  = Kind::Other;
    int         major = 0, minor = 0, patch = 0; // 1.21.4 -> 1, 21, 4 (also for a pre-release or candidate)
    int         number = 0;                      // pre-release or candidate number
    int         year = 0, week = 0;              // a snapshot: 25w04a -> 2025, 4
    char        letter = 0;                      // a snapshot's letter
    std::string text;
};
VersionId parse_version(const std::string &id);

// What is known about the version a server runs. The data version (the
// world_version in version.json) orders snapshots and releases together; the
// release time (from the Mojang list) places a snapshot when that is missing.
struct ServerVersion
{
    std::string id;
    int         data_version = 0;
    std::string release_time; // "2024-12-03T10:12:57+00:00"
};

// True when the version has what `release` ("1.18", "1.20.5") brought.
// A pre-release or candidate counts as its release; a snapshot counts as the
// release it leads to.
bool at_least(const ServerVersion &v, const std::string &release);
bool at_least(const std::string &id, const std::string &release);
// -1, 0 or 1 for two release ids ("1.9" < "1.10").
int compare_releases(const std::string &a, const std::string &b);

// The Java feature version a server needs: from the version's own record when
// known (version.json java_version, or javaVersion.majorVersion in the Mojang
// version record), otherwise by release: 8 before 1.17, 16 for 1.17, 17 up to
// 1.20.4, 21 from 1.20.5.
int java_needed(const ServerVersion &v, int recorded = 0);
// "17.0.10" -> 17, "1.8.0_402" -> 8, "21" -> 21; 0 when it cannot be read.
int java_feature_version(const std::string &version_text);
// The quoted version in `java -version` output ('openjdk version "21.0.5" 2024-10-15').
std::string java_version_from_output(const std::string &output);

// ---------------------------------------------------------------------------
// server.properties

struct PropertyLine
{
    enum class Kind { Blank, Comment, Entry };
    Kind        kind = Kind::Blank;
    std::string raw;       // exactly as read (without the line break); rewritten only when the value changes
    std::string key, value; // decoded
};

struct PropertiesFile
{
    std::vector<PropertyLine> lines;
    std::string               newline = "\n"; // what the file used

    std::optional<std::string> get(const std::string &key) const;
    // Changes the value in place (the line is re-encoded) or appends the key.
    void set(const std::string &key, const std::string &value);
    bool has(const std::string &key) const { return get(key).has_value(); }
    std::vector<std::string> keys() const;
};

PropertiesFile parse_properties(const std::string &text);
std::string    write_properties(const PropertiesFile &file);
// Java properties escaping, as the server writes it: a key escapes = : # ! and
// spaces, a value only a leading space; both escape \, line breaks and tabs.
std::string escape_property(const std::string &text, bool is_key);
std::string unescape_property(const std::string &text);

enum class PropertyType { Bool, Int, Enum, Text, Port, Password };
enum class PropertyGroup { Gameplay, World, Players, Network, Performance, Packs, Advanced };

struct PropertySpec
{
    std::string              key;
    std::string              label;       // English; the panel translates it
    std::string              help;        // English, one sentence
    PropertyType             type = PropertyType::Text;
    PropertyGroup            group = PropertyGroup::Advanced;
    std::string              def;         // the server's own default
    long long                min = 0, max = 0;
    std::vector<std::string> values;      // an Enum's values, as written in the file
    std::string              since;       // the release that brought it ("" since always)
    std::string              until;       // the release that removed it ("" still there)
    // Applied while running by a console command; {} stands for the value.
    std::string              live_command;
};
const std::vector<PropertySpec> &property_specs();
const PropertySpec              *find_property(const std::string &key);
bool                             property_known_in(const PropertySpec &spec, const ServerVersion &v);
// Why a value cannot be used for this setting; empty when it can.
Message property_problem(const PropertySpec &spec, const std::string &value);
// The live command for a value ("difficulty hard"), or "" when it needs a restart.
std::string live_command_for(const PropertySpec &spec, const std::string &value);
// A gamemode/difficulty written as a number by an older version ("1") read as its name.
std::string normalise_enum(const PropertySpec &spec, const std::string &value);

// ---------------------------------------------------------------------------
// The server folder's own files

struct Eula
{
    bool present  = false;
    bool accepted = false;
};
Eula        parse_eula(const std::string &text);
// The file a person accepting the EULA writes: the link, the time, eula=true.
std::string eula_text(bool accepted, const std::string &when);

struct Player
{
    std::string uuid, name;
    int         level  = 0;     // operators: 1 to 4
    bool        bypass = false; // operators: bypassesPlayerLimit
    std::string created, source, expires, reason; // bans
};
// whitelist.json, ops.json, banned-players.json: the entries, and false when
// the text is not a JSON list (an unreadable file is never treated as empty).
bool        parse_players(const std::string &json, std::vector<Player> &out);
std::string write_allowlist(const std::vector<Player> &players);
std::string write_operators(const std::vector<Player> &players);
std::string write_bans(const std::vector<Player> &players);
bool        valid_player_name(const std::string &name); // 3 to 16 of A-Z a-z 0-9 _
const char *operator_level_meaning(int level);           // what level 1 to 4 allows, English

// What a server jar is, from its name and the version.json and manifest inside it.
enum class Flavour { Vanilla, Paper, Purpur, Spigot, Fabric, Forge, NeoForge, Velocity, Unknown };
const char *flavour_name(Flavour f);
struct JarInfo
{
    Flavour     flavour = Flavour::Unknown;
    std::string version;      // "1.21.4" when it could be read
    int         data_version = 0;
    int         java_version = 0;
    std::string build;        // "232" for paper-1.21.4-232.jar
};
JarInfo identify_jar(const std::string &file_name, const std::string &version_json, const std::string &manifest);
// Of the jars in a folder, the one that starts the server ("" when none looks like one).
std::string pick_server_jar(const std::vector<std::string> &jar_names);

// ---------------------------------------------------------------------------
// The console

struct LogEvent
{
    enum class Kind { Line, Started, Joined, Left, Saved, Stopping, PlayerList, TestPassed, TestFailed,
                      ForceLoaded, PortInUse, EulaNeeded, Warning, Error, Chat, Advancement };
    Kind                     kind = Kind::Line;
    std::string              time;  // "00:50:02" when the line has one
    std::string              level; // "INFO", "WARN", "ERROR"
    std::string              text;  // the message after the prefix
    std::string              player;
    std::vector<std::string> players; // PlayerList: everyone online
    int                      count = 0, max = 0; // PlayerList
    double                   seconds = 0;        // Started: "Done (12.345s)!"
};
LogEvent parse_log_line(const std::string &line);
// Splits what a process printed into whole lines, keeping a partial tail.
std::vector<std::string> take_lines(std::string &pending, const std::string &chunk);

// ---------------------------------------------------------------------------
// The Mojang version list

struct ManifestVersion
{
    std::string id, type, url, sha1, time, release_time; // type: release, snapshot, old_beta, old_alpha
};
struct Manifest
{
    std::string                  latest_release, latest_snapshot;
    std::vector<ManifestVersion> versions; // newest first, as listed
};
bool parse_manifest(const std::string &json, Manifest &out);
struct VersionRecord
{
    std::string   id, server_url, server_sha1;
    std::uint64_t server_size = 0;
    int           java = 0; // javaVersion.majorVersion
    std::string   release_time;
};
// False when the record cannot be read; server_url stays empty for a version
// Mojang publishes no server for.
bool parse_version_record(const std::string &json, VersionRecord &out);

// ---------------------------------------------------------------------------
// Starting a server

struct LaunchSettings
{
    std::string java;           // java.exe (or java) to run
    int         java_version = 0; // what that Java reported
    std::string jar;            // the server jar, a file name inside the folder
    int         memory_min_mb = 1024, memory_max_mb = 4096;
    std::string jvm_args;       // extra JVM arguments, as typed
    bool        nogui = true;
    // The console in UTF-8 whatever Java and Windows would pick, and the
    // log4j lookup that older versions shipped turned off.
    bool        safe_console = true;
};
struct LaunchCheck
{
    std::vector<Message> errors; // nothing starts while any is left
    std::vector<Message> notes;
    bool ok() const { return errors.empty(); }
};
LaunchCheck check_launch(const LaunchSettings &s, int java_needed, bool eula_accepted, bool jar_exists, bool port_free,
                         int port, std::uint64_t physical_memory_mb);
std::vector<std::string> launch_arguments(const LaunchSettings &s); // argv, the Java executable first
// Splits typed JVM arguments, honouring double quotes.
std::vector<std::string> split_arguments(const std::string &typed);

// ---------------------------------------------------------------------------
// Backups and schedules

// "Family survival 2026-11-08 06-30-00.zip": safe for every file system.
std::string backup_file_name(const std::string &server_name, std::time_t when, bool automatic);
// What goes into a backup, relative to the server folder: the worlds, then
// every settings and player list file that exists.
std::vector<std::string> backup_entries(const std::string &level_name, const std::vector<std::string> &existing);
// The automatic backups to remove so that `keep` remain (oldest first).
std::vector<std::string> backups_to_prune(const std::vector<std::string> &automatic_names, int keep);
bool                     is_automatic_backup(const std::string &file_name);

struct Schedule
{
    bool backup_on     = false;
    int  backup_hours  = 6;  // 1 to 168
    int  backup_keep   = 8;  // 1 to 100
    bool restart_on    = false;
    int  restart_hour  = 5, restart_minute = 0;
    int  warn_minutes  = 5;  // 0 to 30
};
// The next automatic backup after `last` (or `now` when there was none).
std::time_t next_backup(const Schedule &s, std::time_t last, std::time_t now);
// The next daily restart in local time after `now`.
std::time_t next_restart(const Schedule &s, std::time_t now);

// ---------------------------------------------------------------------------
// The world generator

enum class WorldPreset { Normal, Flat, LargeBiomes, Amplified, SingleBiome };
struct FlatLayer
{
    std::string block  = "minecraft:stone";
    int         height = 1;
};
struct GeneratorSettings
{
    std::string              version;   // the server version that makes the world
    std::string              seed;      // "" for a random one
    WorldPreset              preset = WorldPreset::Normal;
    std::vector<FlatLayer>   layers;    // top first, as shown
    std::string              biome = "minecraft:plains"; // flat and single biome
    bool                     structures = true;
    std::vector<std::string> structure_sets; // flat: the sets to place
    bool                     lakes = false, features = false; // flat
    bool                     hardcore = false;
    std::string              gamemode = "survival", difficulty = "normal";
    std::map<std::string, std::string> game_rules; // only the ones changed from the default
    int                      width_chunks = 0, depth_chunks = 0; // 0: only the spawn area
    int                      centre_x = 0, centre_z = 0;        // blocks
};
// The level-type value this version writes for a preset, or "" when it has none.
std::string level_type_for(const std::string &version, WorldPreset preset);
// The generator-settings value (JSON for 1.19 and newer), or "" when the preset
// needs none or the version has no such form.
std::string generator_settings_for(const GeneratorSettings &g);
// Why these settings cannot make a world with this version (empty when they can).
std::vector<Message> generator_problems(const GeneratorSettings &g);
// server.properties entries for the generation run.
std::map<std::string, std::string> generator_properties(const GeneratorSettings &g, int port);

struct ChunkBatch
{
    int chunk_x1 = 0, chunk_z1 = 0, chunk_x2 = 0, chunk_z2 = 0; // inclusive
    int chunks() const { return (chunk_x2 - chunk_x1 + 1) * (chunk_z2 - chunk_z1 + 1); }
    std::string add_command() const;    // "forceload add x1 z1 x2 z2", in blocks
    std::vector<std::string> probe_commands() const; // "execute if loaded ..." at the corners and the middle
};
// Batches of at most 16 x 16 chunks (the server's limit is 256 per command)
// covering the area centred on the centre, row by row.
std::vector<ChunkBatch> plan_batches(const GeneratorSettings &g);
std::uint64_t           estimate_bytes(int chunks);   // about 12 KB a chunk
int                     estimate_seconds(int chunks); // about 40 chunks a second
// Pre-generating an area polls with "execute if loaded", which 1.19.4 brought.
bool can_pregenerate(const std::string &version);

struct GameRule
{
    std::string name;
    bool        is_bool = true;
    std::string def;
    long long   min = 0, max = 0;
    std::string since;
    std::string help; // English
};
const std::vector<GameRule> &game_rules();
std::vector<const GameRule *> game_rules_for(const std::string &version);

struct NamedId
{
    std::string id, label, since;
};
const std::vector<NamedId> &overworld_biomes();
const std::vector<NamedId> &structure_sets();
const std::vector<NamedId> &flat_blocks();
// "grass_block" -> "minecraft:grass_block"; "" when it is not a valid id.
std::string block_id(const std::string &typed);
std::vector<FlatLayer> classic_flat(); // grass block, 2 dirt, bedrock (top first)
std::string            random_seed(std::uint64_t entropy); // a decimal 64-bit seed

// ---------------------------------------------------------------------------
// The library of servers and the history

struct ServerRecord
{
    std::string id;     // stable, made when added
    std::string name;
    std::string folder; // UTF-8
    LaunchSettings launch;
    std::string java_choice = "auto"; // "auto" or a path
    Schedule    schedule;
    std::time_t last_backup = 0;
    std::string version;    // what the jar reported, for the list
    std::string flavour;
};
struct HistoryEntry
{
    std::time_t when = 0;
    std::string server, action, detail, outcome; // outcome: Done, Failed, Cancelled
};
struct Library
{
    std::vector<ServerRecord> servers;
    std::vector<HistoryEntry> history; // newest last, at most 300
    std::string               backup_folder; // "" is <server folder>/backups

    void record(HistoryEntry e);
    ServerRecord       *find(const std::string &id);
    const ServerRecord *find(const std::string &id) const;
    std::string         unique_id(const std::string &name) const;
};
std::string to_json(const Library &library);
bool        from_json(const std::string &json, Library &out);
// A folder name for a new server or world that does not exist yet:
// "Family survival", then "Family survival (1)".
std::string unique_name(const std::string &wanted, const std::vector<std::string> &taken);
// A name safe as a file or folder name on Windows.
std::string safe_file_name(const std::string &name);

// ---------------------------------------------------------------------------
// SHA-1, for checking a server jar against the Mojang record.

class Sha1
{
public:
    Sha1();
    void        update(const void *data, std::size_t size);
    std::string hex(); // finishes; call once
private:
    void          block(const std::uint8_t *p);
    std::uint32_t m_h[5];
    std::uint8_t  m_buffer[64];
    std::size_t   m_used = 0;
    std::uint64_t m_length = 0;
};
std::string sha1_hex(const std::string &data);

}}} // namespace Slic3r::GUI::GameServers

#endif // slic3r_GUI_GameServers_ServerModel_hpp_
