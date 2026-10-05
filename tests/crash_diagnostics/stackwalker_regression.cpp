#include "StackWalker.h"
#include <strsafe.h>
#include <cstdarg>
#include <cstdio>
#include <string>

class RecordingWalker : public CStackWalker {
public:
    RecordingWalker() : CStackWalker(GetCurrentProcess(), GetCurrentProcessId(), _T(".")) {}
    std::basic_string<TCHAR> output;
    void OutputString(LPCTSTR format, ...) override {
        TCHAR buffer[4096] = {};
        va_list args;
        va_start(args, format);
        StringCchVPrintf(buffer, _countof(buffer), format, args);
        va_end(args);
        output += buffer;
    }
};

__declspec(noinline) __declspec(dllexport) int exercise_walk() {
    RecordingWalker walker;
    CONTEXT context = {};
    RtlCaptureContext(&context);
    auto frames = walker.StackWalker(GetCurrentThread(), &context);
    if (!frames) return 1;
    bool retainedWithoutLines = false;
    bool retainedSymbolWithoutLines = false;
    unsigned count = 0;
    for (auto frame = frames; frame; frame = frame->pNext) {
        ++count;
        retainedWithoutLines |= frame->uFileNum == 0;
        retainedSymbolWithoutLines |= frame->uFileNum == 0 && frame->szFncName[0] != 0;
        TCHAR address[32] = {};
        StringCchPrintf(address, _countof(address), _T("%016llx:"), frame->szFncAddr);
        if (walker.output.find(address) == std::basic_string<TCHAR>::npos) return 2;
    }
    walker.FreeStackInformations(frames);
    if (!retainedWithoutLines || !retainedSymbolWithoutLines) return 3;
    if (walker.output.find(_T("module=")) == std::basic_string<TCHAR>::npos) return 4;
    std::printf("PASS: %u frames retained; symbols without lines and full-width addresses preserved\n", count);
    return 0;
}
DWORD WINAPI wait_for_release(void* event) {
    return WaitForSingleObject(static_cast<HANDLE>(event), 3000) == WAIT_OBJECT_0 ? 0 : 1;
}
int main() {
    const int result = exercise_walk();
    if (result != 0) return result;
    HANDLE release = CreateEvent(NULL, TRUE, FALSE, NULL);
    HANDLE thread = CreateThread(NULL, 0, wait_for_release, release, 0, NULL);
    if (!release || !thread) return 5;
    {
        RecordingWalker walker;
        auto frames = walker.StackWalker(thread);
        walker.FreeStackInformations(frames);
    }
    SetEvent(release);
    const DWORD waitResult = WaitForSingleObject(thread, 2000);
    CloseHandle(thread);
    CloseHandle(release);
    if (waitResult != WAIT_OBJECT_0) return 6;
    std::puts("PASS: inspected thread resumes after stack walking");
    return 0;
}
