// Standalone assertions for src/slic3r/GUI/GameServers/ServerModel.cpp: version
// ids and what each version has, server.properties read and written so that
// every line nobody changed stays exactly as written, the settings table, the
// EULA, player lists and server jars, console lines, the Mojang version list,
// the start command and its checks, backups and schedules, the world
// generator's settings and its batches, the library and SHA-1. Compiled and
// run by tests/game_servers.test.mjs.
#include "src/slic3r/GUI/GameServers/ServerModel.cpp"

#include <cstdio>
#include <set>

using namespace Slic3r::GUI::GameServers;

static int failures = 0, checks = 0;
#define CHECK(cond)                                                                                     \
    do {                                                                                                \
        ++checks;                                                                                       \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; }        \
    } while (0)

static void versions()
{
    CHECK(parse_version("1.21.4").kind == VersionId::Kind::Release);
    CHECK(parse_version("1.21.4").patch == 4);
    CHECK(parse_version("1.21-pre1").kind == VersionId::Kind::PreRelease && parse_version("1.21-pre1").number == 1);
    CHECK(parse_version("1.21.4-rc2").kind == VersionId::Kind::ReleaseCandidate);
    CHECK(parse_version("1.14 Pre-Release 3").kind == VersionId::Kind::PreRelease);
    CHECK(parse_version("25w04a").kind == VersionId::Kind::Snapshot && parse_version("25w04a").year == 2025 && parse_version("25w04a").week == 4);
    CHECK(parse_version("b1.7.3").kind == VersionId::Kind::OldBeta);
    CHECK(parse_version("a1.0.4").kind == VersionId::Kind::OldAlpha);
    CHECK(parse_version("rd-132211").kind == VersionId::Kind::Other);

    CHECK(compare_releases("1.9", "1.10") < 0);
    CHECK(compare_releases("1.21", "1.21.0") == 0);
    CHECK(compare_releases("1.20.6", "1.20.5") > 0);

    CHECK(at_least("1.21.4", "1.20.5"));
    CHECK(!at_least("1.20.4", "1.20.5"));
    CHECK(at_least("1.21-pre1", "1.21"));
    CHECK(at_least("24w14a", "1.20.5"));  // a 1.20.5 snapshot
    CHECK(!at_least("24w14a", "1.21"));
    CHECK(at_least("99w01a", "1.21.5"));  // newer than every known release
    CHECK(!at_least("b1.7.3", "1.13"));
    CHECK(at_least("b1.7.3", ""));
    ServerVersion placed;
    placed.id           = "some-odd-name";
    placed.data_version = 3837;
    CHECK(at_least(placed, "1.20.5"));
    placed.data_version = 3836;
    CHECK(!at_least(placed, "1.20.5"));
    ServerVersion timed;
    timed.id           = "21w37a";
    timed.release_time = "2021-09-15T15:00:00+00:00";
    CHECK(at_least(timed, "1.18") && !at_least(timed, "1.19"));

    const auto needs = [](const char *id) {
        ServerVersion v;
        v.id = id;
        return java_needed(v);
    };
    CHECK(needs("1.16.5") == 8);
    CHECK(needs("1.17.1") == 16);
    CHECK(needs("1.18.2") == 17);
    CHECK(needs("1.20.4") == 17);
    CHECK(needs("1.20.5") == 21);
    CHECK(needs("1.21.4") == 21);
    ServerVersion any;
    any.id = "1.12.2";
    CHECK(java_needed(any, 11) == 11);

    CHECK(java_feature_version("17.0.10") == 17);
    CHECK(java_feature_version("1.8.0_402") == 8);
    CHECK(java_feature_version("21") == 21);
    CHECK(java_feature_version("21-ea") == 21);
    CHECK(java_feature_version("") == 0);
    CHECK(java_version_from_output("openjdk version \"21.0.5\" 2024-10-15 LTS\nOpenJDK Runtime") == "21.0.5");
    CHECK(java_version_from_output("java version \"1.8.0_401\"") == "1.8.0_401");
    CHECK(java_version_from_output("nothing here") == "");
}

static void properties()
{
    const std::string text =
        "#Minecraft server properties\r\n"
        "#Sat Nov 08 06:30:00 GMT 2026\r\n"
        "level-type=minecraft\\:normal\r\n"
        "max-players=20\r\n"
        "motd=A Minecraft Server\r\n"
        "custom-key = keep me \\u00A7a exactly\r\n"
        "long-value=first \\\r\n"
        "    second\r\n"
        "\r\n"
        "! a bang comment\r\n"
        "view-distance=10\r\n"
        "view-distance=12\r\n";
    PropertiesFile file = parse_properties(text);
    CHECK(file.newline == "\r\n");
    CHECK(file.get("level-type").value_or("") == "minecraft:normal");
    CHECK(file.get("custom-key").value_or("") == "keep me \xC2\xA7" "a exactly");
    CHECK(file.get("long-value").value_or("") == "first second");
    CHECK(file.get("view-distance").value_or("") == "12"); // the last one counts, as for the server
    CHECK(!file.get("missing").has_value());
    CHECK(write_properties(file) == text); // nothing changed, nothing moves

    file.set("max-players", "10");
    file.set("motd", "Family \xC2\xA7" "6survival");
    file.set("new-key", "a=b");
    const std::string written = write_properties(file);
    CHECK(written.find("max-players=10\r\n") != std::string::npos);
    CHECK(written.find("motd=Family \\u00A76survival\r\n") != std::string::npos);
    CHECK(written.find("custom-key = keep me \\u00A7a exactly\r\n") != std::string::npos); // untouched, as written
    CHECK(written.find("long-value=first \\\r\n    second\r\n") != std::string::npos);
    CHECK(written.find("level-type=minecraft\\:normal\r\n") != std::string::npos);
    CHECK(written.find("new-key=a\\=b\r\n") != std::string::npos);
    CHECK(written.find("view-distance=10\r\nview-distance=12\r\n") != std::string::npos);
    PropertiesFile again = parse_properties(written);
    CHECK(again.get("motd").value_or("") == "Family \xC2\xA7" "6survival");
    CHECK(again.get("new-key").value_or("") == "a=b");
    file.set("max-players", "10"); // the same value: the line is left alone
    CHECK(write_properties(file) == written);

    CHECK(escape_property("a b", true) == "a\\ b");
    CHECK(escape_property(" lead", false) == "\\ lead");
    CHECK(escape_property("x:y#z!", false) == "x\\:y\\#z\\!");
    CHECK(escape_property("tab\there", false) == "tab\\there");
    CHECK(escape_property("\xF0\x9F\x98\x80", false) == "\\uD83D\\uDE00");
    CHECK(unescape_property("\\uD83D\\uDE00") == "\xF0\x9F\x98\x80");
    CHECK(unescape_property("\\u00e9t\\u00E9") == "\xC3\xA9t\xC3\xA9");
    CHECK(unescape_property("bad \\u12") == "bad u12");
    CHECK(unescape_property("a\\\\b") == "a\\b");

    PropertiesFile empty = parse_properties("");
    CHECK(empty.lines.empty() && write_properties(empty).empty());
    empty.set("eula", "true");
    CHECK(write_properties(empty) == "eula=true\n");
    CHECK(parse_properties("a=1\nb=2\n").keys() == std::vector<std::string>({"a", "b"}));
}

static void settings()
{
    const PropertySpec *view = find_property("view-distance");
    CHECK(view && view->min == 3 && view->max == 32);
    CHECK(property_problem(*view, "10").empty());
    CHECK(!property_problem(*view, "2").empty());
    CHECK(english(property_problem(*view, "2")) == "Use 3 to 32.");
    CHECK(fill("{2} before {1}, {3} missing, {0} kept", {"a", "b"}) == "b before a,  missing, {0} kept");
    CHECK(english(Message{"plain", {}}) == "plain" && Message().empty());
    CHECK(!property_problem(*view, "ten").empty());
    const PropertySpec *pvp = find_property("pvp");
    CHECK(pvp && property_problem(*pvp, "false").empty() && !property_problem(*pvp, "no").empty());
    const PropertySpec *difficulty = find_property("difficulty");
    CHECK(difficulty && normalise_enum(*difficulty, "1") == "easy");
    CHECK(property_problem(*difficulty, "Hard").empty());
    CHECK(property_problem(*difficulty, "3").empty());
    CHECK(!property_problem(*difficulty, "brutal").empty());
    CHECK(live_command_for(*difficulty, "hard") == "difficulty hard");
    CHECK(live_command_for(*difficulty, "brutal").empty());
    const PropertySpec *allow = find_property("white-list");
    CHECK(allow && live_command_for(*allow, "true") == "whitelist on" && live_command_for(*allow, "false") == "whitelist off");
    CHECK(live_command_for(*find_property("gamemode"), "creative") == "defaultgamemode creative");
    CHECK(live_command_for(*find_property("max-players"), "10").empty());
    const PropertySpec *net = find_property("network-compression-threshold");
    CHECK(net && property_problem(*net, "-1").empty() && !property_problem(*net, "-2").empty());
    const PropertySpec *port = find_property("server-port");
    CHECK(port && !property_problem(*port, "0").empty() && property_problem(*port, "25565").empty() && !property_problem(*port, "65536").empty());

    ServerVersion v117, v118, v1211, v1214, unknown;
    v117.id  = "1.17.1";
    v118.id  = "1.18";
    v1211.id = "1.21.1";
    v1214.id = "1.21.4";
    CHECK(!property_known_in(*find_property("simulation-distance"), v117));
    CHECK(property_known_in(*find_property("simulation-distance"), v118));
    CHECK(property_known_in(*find_property("spawn-animals"), v1211));
    CHECK(!property_known_in(*find_property("spawn-animals"), v1214));
    CHECK(property_known_in(*find_property("spawn-animals"), unknown));
    std::set<std::string> keys;
    for (const PropertySpec &s : property_specs()) {
        CHECK(keys.insert(s.key).second); // no setting twice
        CHECK(!s.label.empty() && !s.help.empty());
        if (s.type == PropertyType::Enum) CHECK(!s.values.empty() && property_problem(s, s.def).empty());
        if (s.type == PropertyType::Bool || s.type == PropertyType::Int || s.type == PropertyType::Port) CHECK(property_problem(s, s.def).empty());
    }
    CHECK(keys.size() >= 60);
}

static void folder_files()
{
    CHECK(parse_eula("#By changing...\neula=true\n").accepted);
    CHECK(!parse_eula("#By changing...\neula=false\n").accepted);
    CHECK(!parse_eula("# eula=true\n").accepted);
    CHECK(!parse_eula("").present);
    CHECK(parse_eula(eula_text(true, "Sat Nov 08 2026")).accepted);
    CHECK(eula_text(true, "x").find("https://aka.ms/MinecraftEULA") != std::string::npos);

    std::vector<Player> players;
    CHECK(parse_players("[{\"uuid\":\"u1\",\"name\":\"Mei\",\"level\":4,\"bypassesPlayerLimit\":false}]", players));
    CHECK(players.size() == 1 && players[0].name == "Mei" && players[0].level == 4);
    CHECK(!parse_players("{not a list", players));
    CHECK(!parse_players("{\"a\":1}", players));
    CHECK(parse_players("[]", players) && players.empty());
    std::vector<Player> round;
    Player              kit;
    kit.uuid  = "u2";
    kit.name  = "Kit";
    kit.level = 9;
    CHECK(parse_players(write_operators({kit}), round) && round.size() == 1 && round[0].level == 4); // clamped to 4
    CHECK(parse_players(write_allowlist({kit}), round) && round[0].uuid == "u2" && round[0].level == 0);
    kit.reason = "griefing";
    CHECK(parse_players(write_bans({kit}), round) && round[0].reason == "griefing" && round[0].expires == "forever");
    CHECK(valid_player_name("Mei_2"));
    CHECK(!valid_player_name("Me"));
    CHECK(!valid_player_name("seventeen_letters"));
    CHECK(!valid_player_name("bad name"));
    CHECK(std::string(operator_level_meaning(4)).find("stop the server") != std::string::npos);

    JarInfo paper = identify_jar("paper-1.21.4-232.jar", "", "");
    CHECK(paper.flavour == Flavour::Paper && paper.version == "1.21.4" && paper.build == "232");
    JarInfo vanilla = identify_jar("server.jar", "{\"id\":\"1.21.4\",\"name\":\"1.21.4\",\"world_version\":4189,\"java_version\":21}",
                                   "Manifest-Version: 1.0\r\nMain-Class: net.minecraft.bundler.Main\r\n");
    CHECK(vanilla.flavour == Flavour::Vanilla && vanilla.version == "1.21.4" && vanilla.data_version == 4189 && vanilla.java_version == 21);
    CHECK(identify_jar("minecraft_server.1.12.2.jar", "", "").version == "1.12.2");
    CHECK(identify_jar("minecraft_server.1.12.2.jar", "", "").flavour == Flavour::Vanilla);
    JarInfo fabric = identify_jar("fabric-server-mc.1.21.4-loader.0.16.9-launcher.1.0.1.jar", "", "");
    CHECK(fabric.flavour == Flavour::Fabric && fabric.version == "1.21.4");
    CHECK(identify_jar("neoforge-21.4.88.jar", "", "").flavour == Flavour::NeoForge);
    CHECK(identify_jar("neoforge-21.4.88.jar", "", "").version == "1.21.4");
    CHECK(identify_jar("velocity-3.4.0-SNAPSHOT-463.jar", "", "").flavour == Flavour::Velocity);
    CHECK(identify_jar("velocity-3.4.0-SNAPSHOT-463.jar", "", "").version.empty());
    CHECK(identify_jar("whatever.jar", "", "Main-Class: org.bukkit.craftbukkit.bootstrap.Main\n").flavour == Flavour::Spigot);
    CHECK(identify_jar("whatever.jar", "", "").flavour == Flavour::Unknown);
    CHECK(pick_server_jar({"forge-1.20.1-installer.jar", "paper-1.21.4-232.jar", "readme.txt"}) == "paper-1.21.4-232.jar");
    CHECK(pick_server_jar({"server.jar", "paper-1.21.4-232.jar"}) == "server.jar");
    CHECK(pick_server_jar({"my-own.jar"}) == "my-own.jar");
    CHECK(pick_server_jar({"a.jar", "b.jar"}).empty());
    CHECK(pick_server_jar({}).empty());
}

static void console()
{
    LogEvent e = parse_log_line("[12:34:56] [Server thread/INFO]: Done (3.456s)! For help, type \"help\"");
    CHECK(e.kind == LogEvent::Kind::Started && e.time == "12:34:56" && e.level == "INFO" && e.seconds > 3.4 && e.seconds < 3.5);
    e = parse_log_line("[00:50:02 INFO]: Mei joined the game");
    CHECK(e.kind == LogEvent::Kind::Joined && e.player == "Mei" && e.level == "INFO" && e.time == "00:50:02");
    e = parse_log_line("[00:51:02] [Server thread/INFO]: Kit left the game\r");
    CHECK(e.kind == LogEvent::Kind::Left && e.player == "Kit");
    e = parse_log_line("[00:51:30] [Server thread/INFO]: There are 2 of a max of 20 players online: Mei, Kit");
    CHECK(e.kind == LogEvent::Kind::PlayerList && e.count == 2 && e.max == 20 && e.players == std::vector<std::string>({"Mei", "Kit"}));
    e = parse_log_line("[10:00:00] [Server thread/INFO]: There are 0/20 players online:");
    CHECK(e.kind == LogEvent::Kind::PlayerList && e.count == 0 && e.max == 20 && e.players.empty());
    CHECK(parse_log_line("[10:00:00] [Server thread/INFO]: Saved the game").kind == LogEvent::Kind::Saved);
    CHECK(parse_log_line("[10:00:00] [Server thread/INFO]: Stopping server").kind == LogEvent::Kind::Stopping);
    CHECK(parse_log_line("[10:00:00] [Server thread/INFO]: Test passed").kind == LogEvent::Kind::TestPassed);
    CHECK(parse_log_line("[10:00:00] [Server thread/INFO]: Test failed").kind == LogEvent::Kind::TestFailed);
    CHECK(parse_log_line("[10:00:00] [Server thread/INFO]: Marked 256 chunks in minecraft:overworld from [0, 0] to [15, 15] to be force loaded").kind ==
          LogEvent::Kind::ForceLoaded);
    CHECK(parse_log_line("[10:00:00] [Server thread/WARN]: **** FAILED TO BIND TO PORT!").kind == LogEvent::Kind::PortInUse);
    CHECK(parse_log_line("[10:00:00] [ServerMain/INFO]: You need to agree to the EULA in order to run the server. Go to eula.txt for more info.").kind ==
          LogEvent::Kind::EulaNeeded);
    e = parse_log_line("[10:00:00] [Server thread/INFO]: <Mei> hello there");
    CHECK(e.kind == LogEvent::Kind::Chat && e.player == "Mei");
    e = parse_log_line("[00:52:05 INFO]: Kit has made the advancement [Stone Age]");
    CHECK(e.kind == LogEvent::Kind::Advancement && e.player == "Kit");
    CHECK(parse_log_line("[10:00:00] [Server thread/WARN]: Can't keep up!").kind == LogEvent::Kind::Warning);
    CHECK(parse_log_line("[10:00:00] [Server thread/ERROR]: Encountered an unexpected exception").kind == LogEvent::Kind::Error);
    e = parse_log_line("[12:00:00] [main/INFO] [cp.mo.mo.Launcher/MODLAUNCHER]: ModLauncher running");
    CHECK(e.level == "INFO" && e.text.find("ModLauncher running") != std::string::npos);
    CHECK(parse_log_line("no prefix at all").text == "no prefix at all");

    std::string pending;
    std::vector<std::string> lines = take_lines(pending, "first\r\nsec");
    CHECK(lines == std::vector<std::string>({"first"}) && pending == "sec");
    lines = take_lines(pending, "ond\nthird\n");
    CHECK(lines == std::vector<std::string>({"second", "third"}) && pending.empty());
    lines = take_lines(pending, std::string(70 * 1024, 'x'));
    CHECK(lines.size() == 1 && pending.empty()); // a runaway line is passed on
}

static void mojang()
{
    const std::string manifest =
        "{\"latest\":{\"release\":\"1.21.4\",\"snapshot\":\"25w04a\"},\"versions\":["
        "{\"id\":\"25w04a\",\"type\":\"snapshot\",\"url\":\"https://piston-meta.mojang.com/v1/packages/aa/25w04a.json\",\"time\":\"t\","
        "\"releaseTime\":\"2025-01-22T13:00:00+00:00\",\"sha1\":\"aa\",\"complianceLevel\":1},"
        "{\"id\":\"1.21.4\",\"type\":\"release\",\"url\":\"https://piston-meta.mojang.com/v1/packages/bb/1.21.4.json\","
        "\"releaseTime\":\"2024-12-03T10:12:57+00:00\",\"sha1\":\"bb\"},"
        "{\"id\":\"b1.7.3\",\"type\":\"old_beta\",\"url\":\"u\",\"releaseTime\":\"2011-07-07T22:00:00+00:00\"}]}";
    Manifest m;
    CHECK(parse_manifest(manifest, m));
    CHECK(m.latest_release == "1.21.4" && m.latest_snapshot == "25w04a" && m.versions.size() == 3);
    CHECK(m.versions[1].type == "release" && m.versions[1].sha1 == "bb" && m.versions[0].release_time.substr(0, 10) == "2025-01-22");
    CHECK(!parse_manifest("[]", m));
    CHECK(!parse_manifest("{\"versions\":5}", m));

    VersionRecord r;
    CHECK(parse_version_record("{\"id\":\"1.21.4\",\"downloads\":{\"server\":{\"sha1\":\"4707d00eb834b446575d89a61a11b5d548d8c001\",\"size\":57603246,"
                               "\"url\":\"https://piston-data.mojang.com/v1/objects/4707/server.jar\"}},\"javaVersion\":{\"component\":\"java-runtime-delta\","
                               "\"majorVersion\":21},\"releaseTime\":\"2024-12-03T10:12:57+00:00\"}",
                               r));
    CHECK(r.server_sha1.size() == 40 && r.server_size == 57603246 && r.java == 21 && !r.server_url.empty());
    CHECK(parse_version_record("{\"id\":\"a1.0.4\"}", r) && r.server_url.empty()); // no server for this one
    CHECK(!parse_version_record("nonsense", r));
}

static void launching()
{
    LaunchSettings s;
    s.java          = "C:/Java/bin/java.exe";
    s.java_version  = 21;
    s.jar           = "server.jar";
    s.memory_min_mb = 1024;
    s.memory_max_mb = 4096;
    s.jvm_args      = "-XX:+UseG1GC \"-Dname=a b\"";
    CHECK(check_launch(s, 21, true, true, true, 25565, 16384).ok());
    CHECK(launch_arguments(s) ==
          std::vector<std::string>({"C:/Java/bin/java.exe", "-Xms1024M", "-Xmx4096M", "-Dfile.encoding=UTF-8", "-Dstdout.encoding=UTF-8",
                                    "-Dstderr.encoding=UTF-8", "-Dlog4j2.formatMsgNoLookups=true", "-XX:+UseG1GC", "-Dname=a b", "-jar",
                                    "server.jar", "nogui"}));
    LaunchSettings plain = s;
    plain.safe_console   = false;
    plain.nogui          = false;
    CHECK(launch_arguments(plain) == std::vector<std::string>({"C:/Java/bin/java.exe", "-Xms1024M", "-Xmx4096M", "-XX:+UseG1GC", "-Dname=a b", "-jar", "server.jar"}));
    LaunchCheck c = check_launch(s, 21, false, true, true, 25565, 16384);
    CHECK(!c.ok() && c.errors.size() == 1);
    s.java_version = 17;
    CHECK(!check_launch(s, 21, true, true, true, 25565, 16384).ok());
    s.java_version = 0;
    c = check_launch(s, 21, true, true, true, 25565, 16384);
    CHECK(c.ok() && !c.notes.empty()); // an unread version is a note, not a stop
    s.java_version = 21;
    CHECK(!check_launch(s, 21, true, false, true, 25565, 16384).ok());
    CHECK(!check_launch(s, 21, true, true, false, 25565, 16384).ok());
    CHECK(!check_launch(s, 21, true, true, true, 0, 16384).ok());
    CHECK(!check_launch(s, 21, true, true, true, 25565, 4096).ok()); // leaves nothing for the PC
    CHECK(check_launch(s, 21, true, true, true, 25565, 0).ok());     // memory unknown: no memory check
    s.memory_max_mb = 512;
    CHECK(!check_launch(s, 21, true, true, true, 25565, 16384).ok()); // most below start
    s.memory_max_mb = 4096;
    s.java.clear();
    CHECK(!check_launch(s, 21, true, true, true, 25565, 16384).ok());
    s.java     = "java";
    s.jvm_args = "-Xmx8G";
    c          = check_launch(s, 21, true, true, true, 25565, 16384);
    CHECK(c.ok() && !c.notes.empty());
    CHECK(split_arguments("  a  \"b c\"  d\"e f\"g  \"\"  ") == std::vector<std::string>({"a", "b c", "de fg", ""}));
    CHECK(split_arguments("").empty());
}

static void backups_and_schedules()
{
    const std::time_t when = 1794100000; // November 2026
    const std::string manual = backup_file_name("Family: survival?", when, false);
    const std::string automatic = backup_file_name("Family: survival?", when, true);
    CHECK(manual.find("Family- survival-") == 0);
    CHECK(manual.size() == std::string("Family- survival- 2026-11-08 06-26-40.zip").size());
    CHECK(manual.find(':') == std::string::npos && manual.find('?') == std::string::npos);
    CHECK(!is_automatic_backup(manual) && is_automatic_backup(automatic));
    CHECK(backup_entries("world", {"world", "world_nether", "logs", "server.properties", "ops.json", "server.jar"}) ==
          std::vector<std::string>({"world", "world_nether", "server.properties", "ops.json"}));
    CHECK(backups_to_prune({"S 2026-11-08 06-00-00 auto.zip", "S 2026-11-07 06-00-00 auto.zip", "S 2026-11-09 06-00-00 auto.zip"}, 2) ==
          std::vector<std::string>({"S 2026-11-07 06-00-00 auto.zip"}));
    CHECK(backups_to_prune({"S 2026-11-08 06-00-00 auto.zip"}, 0).empty()); // the newest always stays
    CHECK(safe_file_name("CON") == "CON-");
    CHECK(safe_file_name("  trailing.  ") == "trailing");
    CHECK(safe_file_name("") == "server");

    Schedule s;
    CHECK(next_backup(s, 0, when) == 0);
    s.backup_on    = true;
    s.backup_hours = 6;
    CHECK(next_backup(s, 0, when) == when + 6 * 3600);
    CHECK(next_backup(s, when - 3600, when) == when + 5 * 3600);
    CHECK(next_backup(s, when - 7 * 3600, when) == when); // overdue: now
    CHECK(next_restart(s, when) == 0);
    s.restart_on     = true;
    s.restart_hour   = 5;
    s.restart_minute = 30;
    const std::time_t next = next_restart(s, when);
    CHECK(next > when && next <= when + 25 * 3600);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &next);
#else
    localtime_r(&next, &tm);
#endif
    CHECK(tm.tm_hour == 5 && tm.tm_min == 30 && tm.tm_sec == 0);
    CHECK(next_restart(s, next) > next); // exactly at the time: the next day
}

static void generator()
{
    CHECK(level_type_for("1.21.4", WorldPreset::Flat) == "minecraft:flat");
    CHECK(level_type_for("1.21.4", WorldPreset::SingleBiome) == "minecraft:single_biome_surface");
    CHECK(level_type_for("1.18.2", WorldPreset::LargeBiomes) == "largeBiomes");
    CHECK(level_type_for("1.18.2", WorldPreset::SingleBiome).empty());
    CHECK(level_type_for("1.12.2", WorldPreset::Amplified) == "AMPLIFIED");
    CHECK(level_type_for("", WorldPreset::Normal).empty());

    GeneratorSettings g;
    g.version        = "1.21.4";
    g.preset         = WorldPreset::Flat;
    g.layers         = classic_flat();
    g.structure_sets = {"minecraft:villages"};
    const std::string flat = generator_settings_for(g);
    // Bottom first for the server, whatever order the person sees.
    CHECK(flat.find("\"layers\":[{\"block\":\"minecraft:bedrock\",\"height\":1},{\"block\":\"minecraft:dirt\",\"height\":2},"
                    "{\"block\":\"minecraft:grass_block\",\"height\":1}]") != std::string::npos);
    CHECK(flat.find("\"structure_overrides\":[\"minecraft:villages\"]") != std::string::npos);
    g.structures = false;
    CHECK(generator_settings_for(g).find("\"structure_overrides\":[]") != std::string::npos);
    CHECK(generator_problems(g).empty());
    g.preset = WorldPreset::SingleBiome;
    g.biome  = "minecraft:desert";
    CHECK(generator_settings_for(g) == "{\"biome\":\"minecraft:desert\"}");
    g.preset = WorldPreset::Normal;
    CHECK(generator_settings_for(g).empty());

    GeneratorSettings bad = g;
    bad.preset = WorldPreset::Flat;
    bad.layers.clear();
    CHECK(!generator_problems(bad).empty());
    bad.layers = {{"minecraft:stone", 300}, {"minecraft:dirt", 100}};
    CHECK(!generator_problems(bad).empty()); // taller than 384
    bad.layers = {{"Not A Block!", 1}};
    CHECK(!generator_problems(bad).empty());
    GeneratorSettings old = g;
    old.version = "1.18.2";
    old.preset  = WorldPreset::Flat;
    old.layers  = classic_flat();
    CHECK(generator_problems(old).empty());
    old.layers = {{"minecraft:sand", 3}};
    CHECK(!generator_problems(old).empty()); // own layers need 1.19
    GeneratorSettings area = g;
    area.width_chunks = 100;
    area.depth_chunks = 100;
    CHECK(generator_problems(area).empty());
    area.version = "1.19.3";
    CHECK(!generator_problems(area).empty()); // probing needs 1.19.4
    area.version      = "1.21.4";
    area.depth_chunks = 0;
    CHECK(!generator_problems(area).empty()); // a width without a depth
    GeneratorSettings rules = g;
    rules.game_rules["keepInventory"]   = "true";
    rules.game_rules["randomTickSpeed"] = "3";
    CHECK(generator_problems(rules).empty());
    rules.game_rules["randomTickSpeed"] = "9999";
    CHECK(!generator_problems(rules).empty());
    rules.game_rules.erase("randomTickSpeed");
    rules.game_rules["noSuchRule"] = "true";
    CHECK(!generator_problems(rules).empty());
    GeneratorSettings older_rules = g;
    older_rules.version                         = "1.16.5";
    older_rules.game_rules["spawnChunkRadius"] = "2"; // 1.20.5 and newer
    CHECK(!generator_problems(older_rules).empty());
    GeneratorSettings none;
    CHECK(!generator_problems(none).empty());

    const std::map<std::string, std::string> props = generator_properties(g, 25599);
    CHECK(props.at("server-ip") == "127.0.0.1" && props.at("server-port") == "25599" && props.at("online-mode") == "false");
    CHECK(props.at("white-list") == "true" && props.at("level-type") == "minecraft:normal" && props.count("generator-settings") == 0);
    GeneratorSettings hard = g;
    hard.hardcore   = true;
    hard.difficulty = "easy";
    CHECK(generator_properties(hard, 1).at("difficulty") == "hard");

    // The board's 100 x 100 chunks: 49 batches, each within the server's 256.
    GeneratorSettings             hundred = g;
    hundred.width_chunks = hundred.depth_chunks = 100;
    const std::vector<ChunkBatch> plan = plan_batches(hundred);
    CHECK(plan.size() == 49);
    std::set<std::pair<int, int>> covered;
    int                           total = 0;
    for (const ChunkBatch &b : plan) {
        CHECK(b.chunks() <= 256);
        total += b.chunks();
        for (int x = b.chunk_x1; x <= b.chunk_x2; ++x)
            for (int z = b.chunk_z1; z <= b.chunk_z2; ++z) CHECK(covered.insert({x, z}).second);
    }
    CHECK(total == 10000 && covered.size() == 10000);
    CHECK(covered.count({-50, -50}) == 1 && covered.count({49, 49}) == 1 && covered.count({50, 0}) == 0);
    CHECK(plan.front().add_command() == "forceload add -800 -800 -545 -545");
    CHECK(plan.front().probe_commands().size() == 5);
    CHECK(plan.front().probe_commands().front() == "execute if loaded -792 64 -792");
    GeneratorSettings shifted = g;
    shifted.width_chunks = shifted.depth_chunks = 1;
    shifted.centre_x                            = -1; // block -1 is chunk -1
    shifted.centre_z                            = 17;
    const std::vector<ChunkBatch> one           = plan_batches(shifted);
    CHECK(one.size() == 1 && one[0].chunk_x1 == -1 && one[0].chunk_z1 == 1 && one[0].chunks() == 1);
    CHECK(one[0].probe_commands().size() == 1);
    CHECK(plan_batches(g).empty());
    CHECK(estimate_bytes(10000) == 10000ull * 12 * 1024);
    CHECK(estimate_seconds(10000) == 250 && estimate_seconds(0) == 0 && estimate_seconds(3) == 1);
    CHECK(can_pregenerate("1.21.4") && !can_pregenerate("1.19.3") && !can_pregenerate(""));

    CHECK(game_rules().size() >= 50);
    CHECK(game_rules_for("1.16.5").size() < game_rules_for("1.21.4").size());
    std::set<std::string> names;
    for (const GameRule &r : game_rules()) {
        CHECK(names.insert(r.name).second);
        CHECK(!r.help.empty());
        if (r.is_bool) CHECK(r.def == "true" || r.def == "false");
        else CHECK(std::stoll(r.def) >= r.min && std::stoll(r.def) <= r.max);
    }
    CHECK(structure_sets().size() == 20);
    for (const NamedId &b : overworld_biomes()) CHECK(block_id(b.id) == b.id);
    for (const NamedId &b : flat_blocks()) CHECK(block_id(b.id) == b.id);
    CHECK(block_id("Grass_Block") == "minecraft:grass_block");
    CHECK(block_id("mymod:special/block") == "mymod:special/block");
    CHECK(block_id("bad id").empty() && block_id("").empty() && block_id(":x").empty());
    CHECK(random_seed(1) == random_seed(1) && random_seed(1) != random_seed(2));
    CHECK(random_seed(42).find_first_not_of("-0123456789") == std::string::npos);
}

static void library()
{
    Library lib;
    ServerRecord s;
    s.id                     = lib.unique_id("Family survival!");
    s.name                   = "Family survival!";
    s.folder                 = "D:/Servers/family";
    s.launch.jar             = "server.jar";
    s.launch.memory_max_mb   = 6144;
    s.schedule.backup_on     = true;
    s.schedule.restart_hour  = 4;
    s.last_backup            = 1794100000;
    lib.servers.push_back(s);
    CHECK(s.id == "family-survival");
    CHECK(lib.unique_id("Family survival") == "family-survival-2");
    CHECK(lib.unique_id("***") == "server");
    for (int i = 0; i < 310; ++i) lib.record({std::time_t(i), s.id, "Start", "", "Done"});
    CHECK(lib.history.size() == 300 && lib.history.front().when == 10);
    lib.backup_folder = "E:/Backups";
    Library back;
    CHECK(from_json(to_json(lib), back));
    CHECK(back.servers.size() == 1 && back.servers[0].launch.memory_max_mb == 6144 && back.servers[0].schedule.backup_on);
    CHECK(back.servers[0].schedule.restart_hour == 4 && back.servers[0].last_backup == 1794100000);
    CHECK(back.history.size() == 300 && back.backup_folder == "E:/Backups");
    CHECK(back.find("family-survival") != nullptr && back.find("nope") == nullptr);
    CHECK(!from_json("[]", back));
    CHECK(from_json("{\"servers\":[{\"id\":\"x\"}]}", back) && back.servers.empty()); // no folder, not a server
    CHECK(unique_name("World", {"world", "World (1)"}) == "World (2)");
    CHECK(unique_name("Fresh", {"World"}) == "Fresh");
}

static void sha1()
{
    CHECK(sha1_hex("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    CHECK(sha1_hex("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK(sha1_hex("message digest") == "c12252ceda8be8994d5fa0290a47231c1d16aae3");
    CHECK(sha1_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") == "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    Sha1 chunked;
    const std::string million(1000000, 'a');
    for (std::size_t at = 0; at < million.size(); at += 4999) chunked.update(million.data() + at, std::min<std::size_t>(4999, million.size() - at));
    CHECK(chunked.hex() == "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
}

int main()
{
    versions();
    properties();
    settings();
    folder_files();
    console();
    mojang();
    launching();
    backups_and_schedules();
    generator();
    library();
    sha1();
    if (failures) {
        std::printf("game servers: %d of %d checks failed\n", failures, checks);
        return 1;
    }
    std::printf("game servers: all %d checks passed\n", checks);
    return 0;
}
