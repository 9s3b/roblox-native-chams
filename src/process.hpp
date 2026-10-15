#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <cstring>

namespace nc {

struct Process {
    HANDLE    handle = NULL;
    DWORD     pid_v = 0;
    uintptr_t base_v = 0;

    bool connected() const { return handle && handle != INVALID_HANDLE_VALUE; }

    bool alive() const {
        if (!connected()) return false;
        DWORD code = 0;
        return GetExitCodeProcess(handle, &code) && code == STILL_ACTIVE;
    }

    HANDLE   raw()  const { return handle; }
    uint32_t pid()  const { return pid_v; }
    uint64_t base() const { return base_v; }

    bool read_buf(uintptr_t addr, void* dst, size_t n) const {
        if (!connected() || !addr || !dst || !n) return false;
        SIZE_T got = 0;
        return ReadProcessMemory(handle, (LPCVOID)addr, dst, n, &got) && got == n;
    }
    bool write_buf(uintptr_t addr, const void* src, size_t n) const {
        if (!connected() || !addr || !src || !n) return false;
        return WriteProcessMemory(handle, (LPVOID)addr, src, n, nullptr) != FALSE;
    }

    template <typename T> T read(uintptr_t addr) const {
        T v{};
        read_buf(addr, &v, sizeof(T));
        return v;
    }
    template <typename T> bool write(uintptr_t addr, const T& v) const {
        return write_buf(addr, &v, sizeof(T));
    }

    bool Attach(const wchar_t* exe = L"RobloxPlayerBeta.exe") {
        pid_v = FindPid(exe);
        if (!pid_v) return false;

        handle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
                             PROCESS_QUERY_INFORMATION, FALSE, pid_v);
        if (!handle) handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid_v);
        if (!handle) handle = OpenProcess(PROCESS_VM_READ, FALSE, pid_v);
        if (!handle) return false;

        base_v = FindModule(pid_v, exe);
        return base_v != 0;
    }

    void Detach() {
        if (handle) { CloseHandle(handle); handle = NULL; }
        pid_v = 0; base_v = 0;
    }

    static DWORD FindPid(const wchar_t* exe) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        DWORD found = 0;
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, exe) == 0) { found = pe.th32ProcessID; break; }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        return found;
    }

    static uintptr_t FindModule(DWORD pid, const wchar_t* name) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap == INVALID_HANDLE_VALUE) return 0;
        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);
        uintptr_t b = 0;
        if (Module32FirstW(snap, &me)) {
            do {
                if (_wcsicmp(me.szModule, name) == 0) { b = (uintptr_t)me.modBaseAddr; break; }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
        return b;
    }
};

inline Process g_process;

} // namespace nc
