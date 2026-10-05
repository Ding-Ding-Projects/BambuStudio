#include <windows.h>
#include <filesystem>
#include <fstream>
int wmain(int argc, wchar_t **argv)
{
    if (argc < 3) return 7;
    const auto marker = std::filesystem::path(argv[argc - 2]).parent_path() / "child.pid";
    { std::ofstream out(marker); out << GetCurrentProcessId(); }
    Sleep(60000);
    return 2;
}
