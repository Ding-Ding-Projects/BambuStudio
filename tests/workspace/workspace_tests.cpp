#include <catch_main.hpp>
#include "libslic3r/WorkspaceBundle.hpp"
#include "libslic3r/WorkspacePlanner.hpp"
#include "miniz/miniz.h"

#include <algorithm>
#include <filesystem>
#include <cstdio>
#include <fstream>

namespace fs = std::filesystem;
using namespace Slic3r::Workspace;

namespace {

struct TemporaryTree {
    fs::path root = fs::temp_directory_path() / ("workspace-test-" + new_id());
    TemporaryTree() { fs::create_directories(root); }
    ~TemporaryTree() { std::error_code ignored; fs::remove_all(root, ignored); }
};

void write_file(const fs::path &path, const std::string &content)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    REQUIRE(output.good());
    output << content;
    REQUIRE(output.good());
}

void write_3mf(const fs::path &path)
{
    FILE *file = nullptr;
#ifdef _WIN32
    REQUIRE(_wfopen_s(&file, path.c_str(), L"wb") == 0);
#else
    file = std::fopen(path.c_str(), "wb");
#endif
    REQUIRE(file != nullptr);
    mz_zip_archive archive{};
    REQUIRE(mz_zip_writer_init_cfile(&archive, file, 0));
    const std::string model = "<model unit=\"millimeter\"/>";
    REQUIRE(mz_zip_writer_add_mem(&archive, "3D/3dmodel.model", model.data(), model.size(), MZ_BEST_COMPRESSION));
    REQUIRE(mz_zip_writer_finalize_archive(&archive));
    REQUIRE(mz_zip_writer_end(&archive));
    REQUIRE(std::fclose(file) == 0);
}

void write_3mf_with_corrupt_history(const fs::path &path)
{
    FILE *file = nullptr;
#ifdef _WIN32
    REQUIRE(_wfopen_s(&file, path.c_str(), L"wb") == 0);
#else
    file = std::fopen(path.c_str(), "wb");
#endif
    REQUIRE(file != nullptr);
    mz_zip_archive archive{};
    REQUIRE(mz_zip_writer_init_cfile(&archive, file, 0));
    const std::string model = "<model unit=\"millimeter\"/>";
    REQUIRE(mz_zip_writer_add_mem(&archive, "3D/3dmodel.model", model.data(), model.size(), MZ_BEST_COMPRESSION));
    REQUIRE(mz_zip_writer_add_mem(&archive, "Metadata/bambu_project_history.json", "{}", 2, MZ_BEST_COMPRESSION));
    REQUIRE(mz_zip_writer_add_mem(&archive, "Metadata/bambu_project_history.pack", "bad", 3, MZ_NO_COMPRESSION));
    REQUIRE(mz_zip_writer_finalize_archive(&archive));
    REQUIRE(mz_zip_writer_end(&archive));
    REQUIRE(std::fclose(file) == 0);
}

Workspace example(const fs::path &root)
{
    Workspace workspace;
    workspace.id = new_id();
    workspace.title = "Bench fixtures";
    workspace.notes = "One archive for the entire project.";
    workspace.time_zone = "America/Toronto";
    Member member;
    member.id = new_id();
    member.name = "Bracket";
    member.project_path = root / "bracket.3mf";
    write_3mf(member.project_path);
    SourceFile source{"cad/bracket.step", root / "bracket.step"};
    write_file(source.local_path, "editable source bytes");
    member.editable_sources.push_back(source);
    workspace.members.push_back(member);
    ChecklistItem item;
    item.id = new_id();
    item.text = "Review, then print";
    item.due_date = "2026-03-08";
    item.due_utc_offset_minutes = -240;
    item.linked_member_id = member.id;
    workspace.checklist.push_back(item);
    CalendarSlot slot;
    slot.id = new_id();
    slot.title = "Print bracket";
    slot.printer_id = "printer-a";
    slot.member_id = member.id;
    slot.time_zone = workspace.time_zone;
    slot.start_utc = 1772992800;
    slot.end_utc = slot.start_utc + 3600;
    slot.utc_offset_minutes = -240;
    workspace.slots.push_back(slot);
    return workspace;
}

void rewrite_archive(const fs::path &source, const fs::path &target, const std::string &bad_entry)
{
    FILE *input = nullptr;
    FILE *output = nullptr;
#ifdef _WIN32
    REQUIRE(_wfopen_s(&input, source.c_str(), L"rb") == 0);
    REQUIRE(_wfopen_s(&output, target.c_str(), L"wb") == 0);
#else
    input = std::fopen(source.c_str(), "rb");
    output = std::fopen(target.c_str(), "wb");
#endif
    REQUIRE(input != nullptr);
    REQUIRE(output != nullptr);
    mz_zip_archive reader{}, writer{};
    REQUIRE(mz_zip_reader_init_cfile(&reader, input, 0, 0));
    REQUIRE(mz_zip_writer_init_cfile(&writer, output, 0));
    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&reader); ++i)
        REQUIRE(mz_zip_writer_add_from_zip_reader(&writer, &reader, i));
    REQUIRE(mz_zip_writer_add_mem(&writer, bad_entry.c_str(), "bad", 3, MZ_BEST_COMPRESSION));
    REQUIRE(mz_zip_writer_finalize_archive(&writer));
    REQUIRE(mz_zip_writer_end(&writer));
    REQUIRE(mz_zip_reader_end(&reader));
    REQUIRE(std::fclose(input) == 0);
    REQUIRE(std::fclose(output) == 0);
}

void corrupt_member(const fs::path &source, const fs::path &target)
{
    FILE *input = nullptr;
    FILE *output = nullptr;
#ifdef _WIN32
    REQUIRE(_wfopen_s(&input, source.c_str(), L"rb") == 0);
    REQUIRE(_wfopen_s(&output, target.c_str(), L"wb") == 0);
#else
    input = std::fopen(source.c_str(), "rb");
    output = std::fopen(target.c_str(), "wb");
#endif
    REQUIRE(input != nullptr);
    REQUIRE(output != nullptr);
    mz_zip_archive reader{}, writer{};
    REQUIRE(mz_zip_reader_init_cfile(&reader, input, 0, 0));
    REQUIRE(mz_zip_writer_init_cfile(&writer, output, 0));
    bool replaced = false;
    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&reader); ++i) {
        mz_zip_archive_file_stat stat{};
        REQUIRE(mz_zip_reader_file_stat(&reader, i, &stat));
        const std::string name = stat.m_filename;
        if (name.rfind("Members/", 0) == 0 && name.size() >= 4 && name.substr(name.size() - 4) == ".3mf") {
            REQUIRE(mz_zip_writer_add_mem(&writer, name.c_str(), "changed", 7, MZ_BEST_COMPRESSION));
            replaced = true;
        } else REQUIRE(mz_zip_writer_add_from_zip_reader(&writer, &reader, i));
    }
    REQUIRE(replaced);
    REQUIRE(mz_zip_writer_finalize_archive(&writer));
    REQUIRE(mz_zip_writer_end(&writer));
    REQUIRE(mz_zip_reader_end(&reader));
    REQUIRE(std::fclose(input) == 0);
    REQUIRE(std::fclose(output) == 0);
}

} // namespace

TEST_CASE("Workspace bundle retains owned files and stable identities", "[workspace]")
{
    TemporaryTree temporary;
    Workspace workspace = example(temporary.root);
    const fs::path target = temporary.root / "project.bambu-workspace";
    const auto saved = save_bundle(workspace, target);
    INFO(saved.error);
    REQUIRE(saved.ok());
    const auto inspected = inspect_bundle(target);
    INFO(inspected.error);
    REQUIRE(inspected.ok());
    REQUIRE(inspected.workspace.id == workspace.id);
    REQUIRE(inspected.workspace.members.front().id == workspace.members.front().id);
    REQUIRE(inspected.workspace.checklist.front().linked_member_id == workspace.members.front().id);
    const fs::path staging = temporary.root / "private-staging";
    fs::create_directory(staging);
    const auto loaded = load_bundle(target, staging);
    INFO(loaded.error);
    REQUIRE(loaded.ok());
    REQUIRE(fs::exists(loaded.workspace.members.front().project_path));
    REQUIRE(fs::exists(loaded.workspace.members.front().editable_sources.front().local_path));
    REQUIRE(loaded.staging_directory.string().find(staging.string()) == 0);
}

TEST_CASE("Corrupt optional member history cannot be published over a valid bundle", "[workspace]")
{
    TemporaryTree temporary;
    Workspace workspace = example(temporary.root);
    const fs::path target = temporary.root / "project.bambu-workspace";
    REQUIRE(save_bundle(workspace, target).ok());
    const fs::path corrupt = temporary.root / "corrupt-history.3mf";
    write_3mf_with_corrupt_history(corrupt);
    std::string validation_error;
    REQUIRE_FALSE(validate_member_3mf(corrupt, &validation_error));
    REQUIRE(validation_error.find("history") != std::string::npos);
    workspace.members.front().project_path = corrupt;
    REQUIRE_FALSE(save_bundle(workspace, target).ok());
    REQUIRE(inspect_bundle(target).ok());
    mz_zip_archive geometry{};
    REQUIRE(mz_zip_reader_init_file(&geometry, corrupt.u8string().c_str(), 0));
    REQUIRE(mz_zip_reader_locate_file(&geometry, "3D/3dmodel.model", nullptr, 0) >= 0);
    REQUIRE(mz_zip_reader_end(&geometry));
}

TEST_CASE("Named-zone deadlines wait for verified timezone rules", "[workspace]")
{
    TemporaryTree temporary;
    Workspace workspace = example(temporary.root);
    const auto &deadline = workspace.checklist.front();
    REQUIRE_FALSE(deadline_offset_verifiable(workspace, deadline));
    const auto warnings = validate_plan(workspace, {});
    REQUIRE(std::any_of(warnings.begin(), warnings.end(), [](const auto &warning) {
        return warning.code == "unverified_deadline_timezone";
    }));
    const auto reminders = due_reminders(workspace, 0, 2000000000);
    REQUIRE(std::none_of(reminders.begin(), reminders.end(), [](const auto &reminder) {
        return reminder.kind == "deadline";
    }));
    workspace.time_zone = "UTC";
    workspace.checklist.front().due_utc_offset_minutes = 0;
    REQUIRE(deadline_offset_verifiable(workspace, workspace.checklist.front()));
    const auto utc_reminders = due_reminders(workspace, 0, 2000000000);
    REQUIRE(std::any_of(utc_reminders.begin(), utc_reminders.end(), [](const auto &reminder) {
        return reminder.kind == "deadline";
    }));
}

TEST_CASE("Workspace ZIP rejects traversal and retains previous archive after rejected save", "[workspace]")
{
    TemporaryTree temporary;
    Workspace workspace = example(temporary.root);
    const fs::path target = temporary.root / "project.bambu-workspace";
    REQUIRE(save_bundle(workspace, target).ok());
    const auto original_size = fs::file_size(target);
    const fs::path hostile = temporary.root / "hostile.bambu-workspace";
    rewrite_archive(target, hostile, "../outside.txt");
    REQUIRE_FALSE(inspect_bundle(hostile).ok());
    const fs::path staging = temporary.root / "private-staging";
    fs::create_directory(staging);
    REQUIRE_FALSE(load_bundle(hostile, staging).ok());
    REQUIRE_FALSE(fs::exists(temporary.root / "outside.txt"));
    workspace.members.front().project_path = temporary.root / "missing.3mf";
    REQUIRE_FALSE(save_bundle(workspace, target).ok());
    REQUIRE(fs::file_size(target) == original_size);
    REQUIRE(inspect_bundle(target).ok());
    const fs::path changed = temporary.root / "changed.bambu-workspace";
    corrupt_member(target, changed);
    REQUIRE_FALSE(inspect_bundle(changed).ok());
    write_file(workspace.members.front().project_path, "not a 3MF");
    REQUIRE_FALSE(save_bundle(workspace, target).ok());
    REQUIRE(inspect_bundle(target).ok());
}

TEST_CASE("Planner warns about overlaps and missing printers without submitting prints", "[workspace]")
{
    TemporaryTree temporary;
    Workspace workspace = example(temporary.root);
    CalendarSlot second = workspace.slots.front();
    second.id = new_id();
    second.start_utc += 1800;
    second.end_utc += 1800;
    workspace.slots.push_back(second);
    const auto warnings = validate_plan(workspace, {});
    REQUIRE(std::count_if(warnings.begin(), warnings.end(), [](const auto &warning) { return warning.code == "missing_printer"; }) == 2);
    REQUIRE(std::count_if(warnings.begin(), warnings.end(), [](const auto &warning) { return warning.code == "printer_overlap"; }) == 1);
    REQUIRE(agenda(workspace, second.start_utc, second.end_utc).size() == 2);
}

TEST_CASE("Reminders catch up across DST and exports preserve UTC instants", "[workspace]")
{
    TemporaryTree temporary;
    Workspace workspace = example(temporary.root);
    const auto due = workspace.slots.front().start_utc - 900;
    const auto reminders = due_reminders(workspace, due - 3600, due + 3600);
    REQUIRE(std::any_of(reminders.begin(), reminders.end(), [&](const auto &reminder) {
        return reminder.kind == "planned_print" && reminder.due_utc == due;
    }));
    REQUIRE(snooze_slot(workspace, workspace.slots.front().id, due + 7200));
    REQUIRE(due_reminders(workspace, due - 60, due + 60).empty());
    REQUIRE(dismiss_slot_reminder(workspace, workspace.slots.front().id));
    REQUIRE(due_reminders(workspace, due + 7100, due + 7300).empty());
    REQUIRE(calendar_ics(workspace).find("DTSTART:20260308T") != std::string::npos);
    REQUIRE(calendar_ics(workspace).find("DTSTART;VALUE=DATE:20260308") != std::string::npos);
    REQUIRE(checklist_csv(workspace).find("due_date") != std::string::npos);
    REQUIRE(checklist_json(workspace).find("2026-03-08") != std::string::npos);
}
