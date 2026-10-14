#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include "CrashDiagnostics.h"

namespace CrashDiagnostics {
namespace {
HANDLE output = INVALID_HANDLE_VALUE;
PVOID handler = nullptr;
volatile LONG entered = 0;
volatile LONG reports = 0;
void Write(const char* text, DWORD size) {
    if (output == INVALID_HANDLE_VALUE || size == 0) return;
    DWORD written = 0;
    WriteFile(output,text,size,&written,nullptr);
    FlushFileBuffers(output);
}
LONG WINAPI OnException(EXCEPTION_POINTERS* info) {
    if (!info || !info->ExceptionRecord || !info->ContextRecord) return EXCEPTION_CONTINUE_SEARCH;
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != EXCEPTION_IN_PAGE_ERROR && code != EXCEPTION_INT_DIVIDE_BY_ZERO)
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedCompareExchange(&entered,1,0) != 0) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&reports) <= 8) {
        // Avoid Logger/iostream locks, heap allocation, stack unwinding, and
        // symbol loading from an exception callback. A pre-opened file receives
        // raw registers, module-relative instruction address and stack words.
        const auto& e = *info->ExceptionRecord;
        const auto& c = *info->ContextRecord;
        MEMORY_BASIC_INFORMATION region{};
        VirtualQuery(e.ExceptionAddress,&region,sizeof(region));
        const auto address = reinterpret_cast<std::uintptr_t>(e.ExceptionAddress);
        const auto base = reinterpret_cast<std::uintptr_t>(region.AllocationBase);
        char module[MAX_PATH]{};
        if (region.Type == MEM_IMAGE)
            GetModuleFileNameA(reinterpret_cast<HMODULE>(region.AllocationBase),module,MAX_PATH);
        char buffer[2048]{};
#if defined(_M_IX86)
        const int size = _snprintf_s(buffer,sizeof(buffer),_TRUNCATE,
            "First-chance exception (may subsequently be handled by Halo/Chimera)\r\n"
            "code=%08lX address=%08lX module_base=%08lX rva=%08lX thread=%lu\r\n"
            "module=%s\r\nEIP=%08lX ESP=%08lX EBP=%08lX EFLAGS=%08lX\r\n"
            "EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX ESI=%08lX EDI=%08lX\r\n"
            "access_kind=%lu target=%08lX\r\n",
            code,static_cast<DWORD>(address),static_cast<DWORD>(base),static_cast<DWORD>(address-base),
            GetCurrentThreadId(),module,c.Eip,c.Esp,c.Ebp,c.EFlags,c.Eax,c.Ebx,c.Ecx,c.Edx,c.Esi,c.Edi,
            e.NumberParameters >= 1 ? static_cast<DWORD>(e.ExceptionInformation[0]) : 0,
            e.NumberParameters >= 2 ? static_cast<DWORD>(e.ExceptionInformation[1]) : 0);
        if (size > 0) Write(buffer,static_cast<DWORD>(size));
        DWORD stack[16]{}; SIZE_T bytes = 0;
        if (ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(c.Esp),stack,sizeof(stack),&bytes)) {
            for (unsigned i=0; i<bytes/sizeof(DWORD); ++i) {
                const int n = _snprintf_s(buffer,sizeof(buffer),_TRUNCATE,
                    "stack[%02u]=%08lX\r\n",i,stack[i]);
                if (n > 0) Write(buffer,static_cast<DWORD>(n));
            }
        }
#else
        const int size = _snprintf_s(buffer,sizeof(buffer),_TRUNCATE,"code=%08lX module=%s\r\n",code,module);
        if (size > 0) Write(buffer,static_cast<DWORD>(size));
#endif
    }
    InterlockedExchange(&entered,0);
    return EXCEPTION_CONTINUE_SEARCH;
}
}
bool Start() {
    if (handler) return true;
    output = CreateFileA("VR\\StylishCrash.log",FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,
        nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
    if (output == INVALID_HANDLE_VALUE) return false;
    const char marker[] = "Stylish Restart R1.7 crash diagnostics armed; no exception recorded yet.\r\n";
    Write(marker,sizeof(marker)-1);
    entered = reports = 0;
    handler = AddVectoredExceptionHandler(1,OnException);
    if (!handler) { CloseHandle(output); output = INVALID_HANDLE_VALUE; return false; }
    return true;
}
void Stop() {
    if (handler) { RemoveVectoredExceptionHandler(handler); handler = nullptr; }
    if (output != INVALID_HANDLE_VALUE) { CloseHandle(output); output = INVALID_HANDLE_VALUE; }
}
}
