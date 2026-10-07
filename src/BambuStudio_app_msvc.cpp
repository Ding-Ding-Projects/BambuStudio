// Why?
#define _WIN32_WINNT 0x0502
// The standard Windows includes.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include <wchar.h>
#ifdef SLIC3R_GUI
extern "C"
{
    // Let the NVIDIA and AMD know we want to use their graphics card
    // on a dual graphics card system.
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000000;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0;
}
#endif /* SLIC3R_GUI */
#include <stdlib.h>
#include <stdio.h>

// The launcher's printf diagnostics are invisible in CI (GUI subsystem, lost
// pipes). Mirror every decision to %TEMP%\bbs-launcher-trace.log so a dead
// early exit can be diagnosed from the runner. Best effort, silent on error.
// Several launcher processes append to the same file (an install event, the
// first start after an install, the application it hands over to), so every
// line starts with the local time and the process id.
static void launcher_trace(const wchar_t *fmt, ...)
{
    wchar_t path[MAX_PATH + 1] = {0};
    const DWORD n = ::GetEnvironmentVariableW(L"TEMP", path, MAX_PATH - 40);
    if (n == 0 || n >= MAX_PATH - 40)
        return;
    wcscat(path, L"\\bbs-launcher-trace.log");
    FILE *f = _wfopen(path, L"a, ccs=UTF-8");
    if (f == nullptr)
        return;
    SYSTEMTIME now;
    ::GetLocalTime(&now);
    fwprintf(f, L"%04u-%02u-%02uT%02u:%02u:%02u.%03u pid=%lu ", (unsigned) now.wYear, (unsigned) now.wMonth,
             (unsigned) now.wDay, (unsigned) now.wHour, (unsigned) now.wMinute, (unsigned) now.wSecond,
             (unsigned) now.wMilliseconds, (unsigned long) ::GetCurrentProcessId());
    va_list args;
    va_start(args, fmt);
    vfwprintf(f, fmt, args);
    va_end(args);
    fputwc(L'\n', f);
    fclose(f);
}
#ifdef SLIC3R_GUI
#include <GL/GL.h>
#endif /* SLIC3R_GUI */
#include <objbase.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shell32.lib")

// Squirrel.Windows install events.
//
// The version resource marks this executable as aware of Squirrel (SquirrelAwareVersion in the
// 040904B0 block, the one Squirrel reads). Squirrel then runs it with one of the arguments below
// instead of making a shortcut for, and starting, every executable in the package; that is how the
// regex helper ended up with the application's shortcut. Each event does its work and exits at
// once: nothing of the application is loaded, because Squirrel waits for the process.

// This executable's folder and file name, and the install root (the folder above app-<version>,
// where Squirrel keeps Update.exe). The folder and root end in a backslash.
static bool squirrel_paths(std::wstring &exe_name, std::wstring &root)
{
    wchar_t path[MAX_PATH + 1] = { 0 };
    const DWORD length = ::GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return false;
    std::wstring full(path, length);
    const size_t name_at = full.find_last_of(L'\\');
    if (name_at == std::wstring::npos || name_at == 0)
        return false;
    exe_name = full.substr(name_at + 1);
    const size_t root_at = full.find_last_of(L'\\', name_at - 1);
    if (root_at == std::wstring::npos)
        return false;
    root = full.substr(0, root_at + 1);
    return true;
}

// Runs Update.exe from the install root with the given arguments and waits up to ten seconds.
static bool run_squirrel_update(const std::wstring &root, const std::wstring &arguments)
{
    const std::wstring update = root + L"Update.exe";
    if (::GetFileAttributesW(update.c_str()) == INVALID_FILE_ATTRIBUTES)
        return false;
    std::wstring command = L"\"" + update + L"\" " + arguments;
    std::vector<wchar_t> buffer(command.begin(), command.end());
    buffer.push_back(L'\0');
    STARTUPINFOW startup = { 0 };
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = { 0 };
    if (!::CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, root.c_str(), &startup, &process))
        return false;
    ::WaitForSingleObject(process.hProcess, 10000);
    DWORD exit_code = 1;
    ::GetExitCodeProcess(process.hProcess, &exit_code);
    ::CloseHandle(process.hThread);
    ::CloseHandle(process.hProcess);
    return exit_code == 0;
}

// Packages made before the executable was marked aware got shortcuts named after the old version
// resource ("BambuStudio", in a Start Menu folder "Bambu Research"). Removes such a shortcut, but
// only when it points into this installation, so a shortcut of any other copy is left alone.
static void remove_legacy_squirrel_shortcut(int folder_id, const wchar_t *relative_path, const std::wstring &root)
{
    wchar_t folder[MAX_PATH + 1] = { 0 };
    if (FAILED(::SHGetFolderPathW(nullptr, folder_id, nullptr, SHGFP_TYPE_CURRENT, folder)))
        return;
    const std::wstring link = std::wstring(folder) + L"\\" + relative_path;
    if (::GetFileAttributesW(link.c_str()) == INVALID_FILE_ATTRIBUTES)
        return;
    bool ours = false;
    IShellLinkW *shell_link = nullptr;
    if (SUCCEEDED(::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void **>(&shell_link)))) {
        IPersistFile *file = nullptr;
        if (SUCCEEDED(shell_link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&file)))) {
            wchar_t target[MAX_PATH + 1] = { 0 };
            if (SUCCEEDED(file->Load(link.c_str(), STGM_READ)) && SUCCEEDED(shell_link->GetPath(target, MAX_PATH, nullptr, 0)))
                ours = _wcsnicmp(target, root.c_str(), root.size()) == 0;
            file->Release();
        }
        shell_link->Release();
    }
    if (!ours)
        return;
    ::DeleteFileW(link.c_str());
    // The Start Menu folder goes too once it is empty; RemoveDirectory refuses a folder that is not.
    const size_t folder_end = link.find_last_of(L'\\');
    if (wcschr(relative_path, L'\\') != nullptr && folder_end != std::wstring::npos)
        ::RemoveDirectoryW(link.substr(0, folder_end).c_str());
}

// True when <special folder>\relative_path exists.
static bool squirrel_link_exists(int folder_id, const wchar_t *relative_path)
{
    wchar_t folder[MAX_PATH + 1] = { 0 };
    if (FAILED(::SHGetFolderPathW(nullptr, folder_id, nullptr, SHGFP_TYPE_CURRENT, folder)))
        return false;
    const std::wstring link = std::wstring(folder) + L"\\" + relative_path;
    return ::GetFileAttributesW(link.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// The shortcuts this version makes: named after ProductName and filed under CompanyName (the version resource).
static const wchar_t *const kSquirrelDesktopLink = L"Bambu Studio MD3.lnk";
static const wchar_t *const kSquirrelStartLink   = L"codingmachineedge\\Bambu Studio MD3.lnk";
// The shortcuts packages made before the launcher was marked aware.
static const wchar_t *const kLegacyDesktopLink   = L"BambuStudio.lnk";
static const wchar_t *const kLegacyStartLink     = L"Bambu Research\\BambuStudio.lnk";

// The first start after an install. After an interactive install, Update.exe starts
// app-<version>\bambu-studio.exe with --squirrel-firstrun while the installer is still finishing,
// and does not wait for it. Started that way the application has been reported not to appear,
// while the install root's bambu-studio.exe (Squirrel's stub, the target of both shortcuts) starts
// the same version normally. So this process waits, at most a minute, for its parent to finish when
// that parent is Update.exe, starts the stub the way a shortcut does, without any Squirrel argument,
// and exits.
// Returns 0 once the stub has started. Returns -1 when there is no install root or no stub, or the
// stub could not be started; this process then starts the application itself, as it always did.
static int squirrel_first_run()
{
    // This process's parent. Toolhelp names each image without its folder.
    const DWORD self = ::GetCurrentProcessId();
    DWORD parent = 0;
    std::wstring parent_image = L"unknown";
    const HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry = { 0 };
        entry.dwSize = sizeof(entry);
        for (BOOL more = ::Process32FirstW(snapshot, &entry); more; more = ::Process32NextW(snapshot, &entry)) {
            if (entry.th32ProcessID == self) {
                parent = entry.th32ParentProcessID;
                break;
            }
        }
        entry.dwSize = sizeof(entry);
        for (BOOL more = parent != 0 ? ::Process32FirstW(snapshot, &entry) : FALSE; more; more = ::Process32NextW(snapshot, &entry)) {
            if (entry.th32ProcessID == parent) {
                parent_image = entry.szExeFile;
                break;
            }
        }
        ::CloseHandle(snapshot);
    }
    const bool from_update = _wcsicmp(parent_image.c_str(), L"Update.exe") == 0;

    // Update.exe does not wait for this process, so waiting for Update.exe cannot deadlock; the
    // minute only bounds the wait should the installer hang. A parent that has already exited
    // cannot be opened, and there is nothing left to wait for.
    const wchar_t *waited = L"skipped";
    DWORD wait_error = 0;
    if (from_update) {
        launcher_trace(L"squirrel event --squirrel-firstrun: waiting up to 60 s for Update.exe pid=%lu", (unsigned long) parent);
        const HANDLE update = ::OpenProcess(SYNCHRONIZE, FALSE, parent);
        if (update == nullptr) {
            wait_error = ::GetLastError();
            waited = L"not-opened";
        } else {
            const DWORD result = ::WaitForSingleObject(update, 60000);
            if (result == WAIT_OBJECT_0) {
                waited = L"exited";
            } else if (result == WAIT_TIMEOUT) {
                waited = L"timed-out";
            } else {
                wait_error = ::GetLastError();
                waited = L"failed";
            }
            ::CloseHandle(update);
        }
    }

    // One line with the whole outcome, whichever way this ends. foreground is -1 when no stub started.
    const auto outcome = [&](const std::wstring &stub, DWORD pid, DWORD error, int foreground, const wchar_t *next) {
        launcher_trace(L"squirrel event --squirrel-firstrun: parent=%lu %ls update=%d wait=%ls wait_error=%lu stub=%ls "
                       L"started=%d pid=%lu error=%lu foreground=%d; %ls",
                       (unsigned long) parent, parent_image.c_str(), (int) from_update, waited, (unsigned long) wait_error,
                       stub.c_str(), (int) (pid != 0), (unsigned long) pid, (unsigned long) error, foreground, next);
    };

    std::wstring exe_name, root;
    if (!squirrel_paths(exe_name, root)) {
        outcome(L"none", 0, 0, -1, L"no install root, starting here");
        return -1;
    }
    // Squirrel's stub has this executable's name and sits in the install root, beside Update.exe.
    const std::wstring stub = root + exe_name;
    if (::GetFileAttributesW(stub.c_str()) == INVALID_FILE_ATTRIBUTES) {
        const DWORD missing_error = ::GetLastError();
        outcome(stub, 0, missing_error, -1, L"no stub, starting here");
        return -1;
    }

    // Only the stub's own path: no Squirrel argument reaches it, so the application it starts makes
    // a normal start. Shown normally, as a shortcut starts it. Breaking away from any job the
    // installer's processes run in keeps the end of the installer from taking the application with
    // it; a job that forbids breaking away refuses the flag, so the second attempt goes without it.
    const std::wstring command = L"\"" + stub + L"\"";
    const DWORD attempts[] = { CREATE_BREAKAWAY_FROM_JOB, 0 };
    DWORD start_error = 0;
    STARTUPINFOW startup = { 0 };
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_SHOWNORMAL;
    PROCESS_INFORMATION process = { 0 };
    BOOL started = FALSE;
    for (int attempt = 0; attempt < 2 && !started; ++attempt) {
        std::vector<wchar_t> buffer(command.begin(), command.end());
        buffer.push_back(L'\0');
        started = ::CreateProcessW(stub.c_str(), buffer.data(), nullptr, nullptr, FALSE, attempts[attempt], nullptr, root.c_str(),
                                   &startup, &process);
        if (!started) {
            start_error = ::GetLastError();
            launcher_trace(L"squirrel event --squirrel-firstrun: CreateProcess flags=0x%08lx failed, error=%lu",
                           (unsigned long) attempts[attempt], (unsigned long) start_error);
        }
    }
    if (!started) {
        outcome(stub, 0, start_error, -1, L"start failed, starting here");
        return -1;
    }
    // The stub hands the right to take the foreground on to the application, as it does for a
    // shortcut; this works only while this process holds that right itself, so it is best effort.
    const BOOL foreground = ::AllowSetForegroundWindow(process.dwProcessId);
    const DWORD foreground_error = foreground ? 0 : ::GetLastError();
    ::CloseHandle(process.hThread);
    ::CloseHandle(process.hProcess);
    outcome(stub, process.dwProcessId, foreground_error, foreground ? 1 : 0, L"handed over, exiting");
    return 0;
}

// Returns -1 when the arguments are not a Squirrel event (a normal start), else the exit code.
static int handle_squirrel_event(int argc, wchar_t **argv)
{
    if (argc < 2 || wcsncmp(argv[1], L"--squirrel-", 11) != 0)
        return -1;
    const wchar_t *event = argv[1];
    // The first start after an install goes through the install root's stub (squirrel_first_run).
    // When that cannot be done it is a normal start, and the argument is dropped from the command line.
    if (wcscmp(event, L"--squirrel-firstrun") == 0)
        return squirrel_first_run();
    const bool installed = wcscmp(event, L"--squirrel-install") == 0;
    const bool updated   = wcscmp(event, L"--squirrel-updated") == 0;
    const bool uninstall = wcscmp(event, L"--squirrel-uninstall") == 0;
    std::wstring exe_name, root;
    if ((installed || updated || uninstall) && squirrel_paths(exe_name, root)) {
        // An install makes both shortcuts. An update keeps only the places that still hold this
        // application's shortcut, new or legacy, so a shortcut the person deleted stays deleted.
        std::wstring locations;
        const bool desktop = installed || uninstall || squirrel_link_exists(CSIDL_DESKTOPDIRECTORY, kSquirrelDesktopLink) ||
                             squirrel_link_exists(CSIDL_DESKTOPDIRECTORY, kLegacyDesktopLink);
        const bool start   = installed || uninstall || squirrel_link_exists(CSIDL_PROGRAMS, kSquirrelStartLink) ||
                             squirrel_link_exists(CSIDL_PROGRAMS, kLegacyStartLink);
        if (desktop)
            locations = L"Desktop";
        if (start)
            locations += locations.empty() ? L"StartMenu" : L",StartMenu";
        bool done = false;
        if (!locations.empty()) {
            const std::wstring arguments = std::wstring(uninstall ? L"--removeShortcut=" : L"--createShortcut=") + exe_name +
                                           L" --shortcut-locations=" + locations;
            done = run_squirrel_update(root, arguments);
            launcher_trace(L"squirrel event %ls: Update.exe %ls -> %d", event, arguments.c_str(), (int) done);
        } else {
            launcher_trace(L"squirrel event %ls: no shortcut of this application left to update", event);
        }
        // The legacy shortcuts go only once the new ones exist (or the application is being removed):
        // a legacy shortcut that still works is better than none.
        if (done || uninstall) {
            const HRESULT com = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            remove_legacy_squirrel_shortcut(CSIDL_DESKTOPDIRECTORY, kLegacyDesktopLink, root);
            remove_legacy_squirrel_shortcut(CSIDL_PROGRAMS, kLegacyStartLink, root);
            if (SUCCEEDED(com))
                ::CoUninitialize();
        }
        // Squirrel removes the shortcut but leaves the Start Menu folder it made; RemoveDirectory
        // refuses a folder that still holds anything, so another product's shortcuts are safe.
        if (uninstall) {
            wchar_t programs[MAX_PATH + 1] = { 0 };
            if (SUCCEEDED(::SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, SHGFP_TYPE_CURRENT, programs)))
                ::RemoveDirectoryW((std::wstring(programs) + L"\\codingmachineedge").c_str());
        }
    } else {
        // --squirrel-obsolete, and any event a later Squirrel adds: nothing to do.
        launcher_trace(L"squirrel event %ls: nothing to do", event);
    }
    return 0;
}
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <stdio.h>
#ifdef SLIC3R_GUI
class OpenGLVersionCheck
{
public:
    std::string version;
    std::string glsl_version;
    std::string vendor;
    std::string renderer;
    HINSTANCE   hOpenGL = nullptr;
    bool 		success = false;
    bool load_opengl_dll()
    {
        MSG      msg = { 0 };
        WNDCLASS wc = { 0 };
        wc.lpfnWndProc = OpenGLVersionCheck::supports_opengl2_wndproc;
        wc.hInstance = (HINSTANCE)GetModuleHandle(nullptr);
        wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
        wc.lpszClassName = L"BambuStudio_opengl_version_check";
        wc.style = CS_OWNDC;
        if (RegisterClass(&wc)) {
            HWND hwnd = CreateWindowW(wc.lpszClassName, L"BambuStudio_opengl_version_check", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, 0, 0, wc.hInstance, (LPVOID)this);
            if (hwnd) {
                message_pump_exit = false;
                while (GetMessage(&msg, NULL, 0, 0) > 0 && !message_pump_exit)
                    DispatchMessage(&msg);
            }
        }
        return this->success;
    }
    bool unload_opengl_dll()
    {
        if (this->hOpenGL != nullptr) {
            if (::FreeLibrary(this->hOpenGL) != FALSE) {
                if (::GetModuleHandle(L"opengl32.dll") == nullptr) {
                    printf("System OpenGL library successfully released\n");
                    this->hOpenGL = nullptr;
                    return true;
                }
                else
                    printf("System OpenGL library released but not removed\n");
            }
            else
                printf("System OpenGL library NOT released\n");
            return false;
        }
        return true;
    }
    bool is_version_greater_or_equal_to(unsigned int major, unsigned int minor) const
    {
        // printf("is_version_greater_or_equal_to, version: %s\n", version.c_str());
        std::vector<std::string> tokens;
        boost::split(tokens, version, boost::is_any_of(" "), boost::token_compress_on);
        if (tokens.empty())
            return false;
        std::vector<std::string> numbers;
        boost::split(numbers, tokens[0], boost::is_any_of("."), boost::token_compress_on);
        unsigned int gl_major = 0;
        unsigned int gl_minor = 0;
        if (numbers.size() > 0)
            gl_major = ::atoi(numbers[0].c_str());
        if (numbers.size() > 1)
            gl_minor = ::atoi(numbers[1].c_str());
        // printf("Major: %d, minor: %d\n", gl_major, gl_minor);
        if (gl_major < major)
            return false;
        else if (gl_major > major)
            return true;
        else
            return gl_minor >= minor;
    }
protected:
    static bool message_pump_exit;
    void check(HWND hWnd)
    {
        hOpenGL = LoadLibraryExW(L"opengl32.dll", nullptr, 0);
        if (hOpenGL == nullptr) {
            printf("Failed loading the system opengl32.dll\n");
            return;
        }
        typedef HGLRC(WINAPI* Func_wglCreateContext)(HDC);
        typedef BOOL(WINAPI* Func_wglMakeCurrent)(HDC, HGLRC);
        typedef BOOL(WINAPI* Func_wglDeleteContext)(HGLRC);
        typedef GLubyte* (WINAPI* Func_glGetString)(GLenum);
        Func_wglCreateContext 	wglCreateContext = (Func_wglCreateContext)GetProcAddress(hOpenGL, "wglCreateContext");
        Func_wglMakeCurrent 	wglMakeCurrent = (Func_wglMakeCurrent)GetProcAddress(hOpenGL, "wglMakeCurrent");
        Func_wglDeleteContext 	wglDeleteContext = (Func_wglDeleteContext)GetProcAddress(hOpenGL, "wglDeleteContext");
        Func_glGetString 		glGetString = (Func_glGetString)GetProcAddress(hOpenGL, "glGetString");
        if (wglCreateContext == nullptr || wglMakeCurrent == nullptr || wglDeleteContext == nullptr || glGetString == nullptr) {
            printf("Failed loading the system opengl32.dll: The library is invalid.\n");
            return;
        }
        PIXELFORMATDESCRIPTOR pfd =
        {
            sizeof(PIXELFORMATDESCRIPTOR),
            1,
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            PFD_TYPE_RGBA,            	// The kind of framebuffer. RGBA or palette.
            32,                        	// Color depth of the framebuffer.
            0, 0, 0, 0, 0, 0,
            0,
            0,
            0,
            0, 0, 0, 0,
            24,                        	// Number of bits for the depthbuffer
            8,                        	// Number of bits for the stencilbuffer
            0,                        	// Number of Aux buffers in the framebuffer.
            PFD_MAIN_PLANE,
            0,
            0, 0, 0
        };
        HDC ourWindowHandleToDeviceContext = ::GetDC(hWnd);
        // Gdi32.dll
        int letWindowsChooseThisPixelFormat = ::ChoosePixelFormat(ourWindowHandleToDeviceContext, &pfd);
        // Gdi32.dll
        SetPixelFormat(ourWindowHandleToDeviceContext, letWindowsChooseThisPixelFormat, &pfd);
        // Opengl32.dll
        HGLRC glcontext = wglCreateContext(ourWindowHandleToDeviceContext);
        wglMakeCurrent(ourWindowHandleToDeviceContext, glcontext);
        // Opengl32.dll
        const char* data = (const char*)glGetString(GL_VERSION);
        if (data != nullptr)
            this->version = data;
        // printf("check -version: %s\n", version.c_str());
        data = (const char*)glGetString(0x8B8C); // GL_SHADING_LANGUAGE_VERSION
        if (data != nullptr)
            this->glsl_version = data;
        data = (const char*)glGetString(GL_VENDOR);
        if (data != nullptr)
            this->vendor = data;
        data = (const char*)glGetString(GL_RENDERER);
        if (data != nullptr)
            this->renderer = data;
        // Opengl32.dll
        wglDeleteContext(glcontext);
        ::ReleaseDC(hWnd, ourWindowHandleToDeviceContext);
        this->success = true;
    }
    static LRESULT CALLBACK supports_opengl2_wndproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
        {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            OpenGLVersionCheck* ogl_data = reinterpret_cast<OpenGLVersionCheck*>(pCreate->lpCreateParams);
            ogl_data->check(hWnd);
            DestroyWindow(hWnd);
            return 0;
        }
        case WM_NCDESTROY:
            message_pump_exit = true;
            return 0;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
};
bool OpenGLVersionCheck::message_pump_exit = false;
#endif /* SLIC3R_GUI */
extern "C" {
    typedef int(__stdcall* Slic3rMainFunc)(int argc, wchar_t** argv);
    Slic3rMainFunc bambustu_main = nullptr;
}
extern "C" {
#ifdef SLIC3R_WRAPPER_NOCONSOLE
    int APIENTRY wWinMain(HINSTANCE /* hInstance */, HINSTANCE /* hPrevInstance */, PWSTR /* lpCmdLine */, int /* nCmdShow */)
    {
        // if started from console/CLI then reopen stdout/stderr pipes to it for printf/cout output,
        // which includes --help content and any option parsing errors.
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
          freopen("CONOUT$", "w", stdout);
          freopen("CONOUT$", "w", stderr);
        }

        int 	  argc;
        wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
#else
    int wmain(int argc, wchar_t** argv)
    {
#endif
        // Allow the asserts to open message box, such message box allows to ignore the assert and continue with the application.
        // Without this call, the seemingly same message box is being opened by the abort() function, but that is too late and
        // the application will be killed even if "Ignore" button is pressed.
        _set_error_mode(_OUT_TO_MSGBOX);
        launcher_trace(L"launcher start: %ls", ::GetCommandLineW());
        // An install, update or uninstall event from Squirrel is handled here and never starts the app.
        const int squirrel_exit = handle_squirrel_event(argc, argv);
        if (squirrel_exit >= 0)
            return squirrel_exit;
        std::vector<wchar_t*> argv_extended;
        argv_extended.emplace_back(argv[0]);
#ifdef SLIC3R_WRAPPER_GCODEVIEWER
        wchar_t gcodeviewer_param[] = L"--gcodeviewer";
        argv_extended.emplace_back(gcodeviewer_param);
#endif /* SLIC3R_WRAPPER_GCODEVIEWER */
#ifdef SLIC3R_GUI
        // Here one may push some additional parameters based on the wrapper type.
        bool force_mesa = false;
#endif /* SLIC3R_GUI */
        for (int i = 1; i < argc; ++i) {
            // Squirrel starts the app with this after the first install; it means nothing to the app.
            if (wcscmp(argv[i], L"--squirrel-firstrun") == 0)
                continue;
#ifdef SLIC3R_GUI
            if (wcscmp(argv[i], L"--sw-renderer") == 0)
                force_mesa = true;
            else if (wcscmp(argv[i], L"--no-sw-renderer") == 0)
                force_mesa = false;
#endif /* SLIC3R_GUI */
            argv_extended.emplace_back(argv[i]);
        }
        argv_extended.emplace_back(nullptr);
#ifdef SLIC3R_GUI
        OpenGLVersionCheck opengl_version_check;
        bool load_mesa =
            // Forced from the command line.
            force_mesa ||
            // Try to load the default OpenGL driver and test its context version.
            !opengl_version_check.load_opengl_dll() || !opengl_version_check.is_version_greater_or_equal_to(3, 2);
#endif /* SLIC3R_GUI */
        wchar_t path_to_exe[MAX_PATH + 1] = { 0 };
        ::GetModuleFileNameW(nullptr, path_to_exe, MAX_PATH);
        wchar_t drive[_MAX_DRIVE];
        wchar_t dir[_MAX_DIR];
        wchar_t fname[_MAX_FNAME];
        wchar_t ext[_MAX_EXT];
        _wsplitpath(path_to_exe, drive, dir, fname, ext);
        _wmakepath(path_to_exe, drive, dir, nullptr, nullptr);
#ifdef SLIC3R_GUI
        // https://wiki.qt.io/Cross_compiling_Mesa_for_Windows
        // http://download.qt.io/development_releases/prebuilt/llvmpipe/windows/
        launcher_trace(L"gl check: success=%d version=%hs load_mesa=%d",
                       (int) opengl_version_check.success, opengl_version_check.version.c_str(), (int) load_mesa);
        if (load_mesa) {
            bool res = opengl_version_check.unload_opengl_dll();
            launcher_trace(L"unload_opengl_dll -> %d", (int) res);
            if (!res) {
                launcher_trace(L"EXIT -1: could not unload system opengl32");
                MessageBox(nullptr, L"Error:BambuStudio was unable to automatically switch to MESA OpenGL library.",
                    L"BambuStudio Error", MB_OK);
                return -1;
            }
            else {
            // The staged Mesa build also carries the d3d12 gallium driver. On a host whose own
            // OpenGL is too old (usually no GPU driver at all) that driver can be chosen first and
            // fail during context creation, ending the process with a Direct3D HRESULT. Select
            // llvmpipe before the DLL loads, so its C runtime's copy of the environment already
            // holds the choice; a driver the caller set explicitly is kept. This mirrors
            // OpenGLManager::apply_bundled_softgl_environment() for Mesa beside the executable.
            wchar_t chosen_driver[16] = { 0 };
            if (::GetEnvironmentVariableW(L"GALLIUM_DRIVER", chosen_driver, 16) == 0) {
                const wchar_t *mesa_environment[][2] = {
                    { L"GALLIUM_DRIVER", L"llvmpipe" },
                    { L"LIBGL_ALWAYS_SOFTWARE", L"1" },
                    { L"MESA_GL_VERSION_OVERRIDE", L"3.3" },
                };
                for (const auto &variable : mesa_environment) {
                    ::SetEnvironmentVariableW(variable[0], variable[1]);
                    _wputenv_s(variable[0], variable[1]);
                }
                launcher_trace(L"mesa environment: GALLIUM_DRIVER=llvmpipe");
            } else {
                launcher_trace(L"mesa environment: keeping GALLIUM_DRIVER=%ls", chosen_driver);
            }
            wchar_t path_to_mesa[MAX_PATH + 1] = { 0 };
            wcscpy(path_to_mesa, path_to_exe);
            wcscat(path_to_mesa, L"mesa\\opengl32.dll");
            printf("Loading MESA OpenGL library: %S\n", path_to_mesa);
            // LOAD_WITH_ALTERED_SEARCH_PATH: Mesa's opengl32 pulls
            // libgallium_wgl.dll, which lives in mesa\ too — the default
            // search resolves dependencies from the application directory and
            // fails with ERROR_MOD_NOT_FOUND (126), so the staged folder never
            // actually loaded.
            HINSTANCE hInstance_OpenGL = LoadLibraryExW(path_to_mesa, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
                if (hInstance_OpenGL == nullptr) {
                printf("MESA OpenGL library was not loaded\n");
                    launcher_trace(L"mesa load FAILED: %ls error=%lu", path_to_mesa, ::GetLastError());
                } else {
                    printf("MESA OpenGL library was loaded sucessfully\n");
                    launcher_trace(L"mesa loaded: %ls", path_to_mesa);
                }
            }
        }
#endif /* SLIC3R_GUI */
        wchar_t path_to_slic3r[MAX_PATH + 1] = { 0 };
        wcscpy(path_to_slic3r, path_to_exe);
        wcscat(path_to_slic3r, L"BambuStudio.dll");
        //	printf("Loading Slic3r library: %S\n", path_to_slic3r);
        HINSTANCE hInstance_Slic3r = LoadLibraryExW(path_to_slic3r, nullptr, 0);
        if (hInstance_Slic3r == nullptr) {
            const DWORD load_error = GetLastError();
            printf("BambuStudio.dll was not loaded, error=%d\n", (int) load_error);
            launcher_trace(L"EXIT -1: BambuStudio.dll load failed, error=%lu", load_error);
            return -1;
        }
        launcher_trace(L"BambuStudio.dll loaded; resolving bambustu_main");
        // resolve function address here
        bambustu_main = (Slic3rMainFunc)GetProcAddress(hInstance_Slic3r,
#ifdef _WIN64
            // there is just a single calling conversion, therefore no mangling of the function name.
            "bambustu_main"
#else	// stdcall calling convention declaration
            "_bambustu_main@8"
#endif
        );
        if (bambustu_main == nullptr) {
            printf("could not locate the function bambustu_main in BambuStudio.dll\n");
            launcher_trace(L"EXIT -1: bambustu_main not exported");
            return -1;
        }
        // argc minus the trailing nullptr of the argv
        const int result = bambustu_main((int)argv_extended.size() - 1, argv_extended.data());
        // No such line means the process ended inside the application (an exit call or a crash).
        launcher_trace(L"bambustu_main returned %d", result);
        return result;
    }
    }
