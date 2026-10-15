#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "offsets.hpp"
#include "process.hpp"

// Roblox object-model helpers: instance traversal, player list, world->screen.
namespace nc {

inline float     g_viewMatrix[16] = {};
inline int       g_screenW = 0;
inline int       g_screenH = 0;
inline uintptr_t g_base = 0;
inline DWORD     g_pid = 0;

inline uintptr_t g_workspace = 0;
inline uintptr_t g_playersService = 0;
inline uintptr_t g_localPlayer = 0;

struct PlayerEntry { uintptr_t player = 0; uintptr_t model = 0; };
inline std::vector<PlayerEntry> g_players;

inline std::unordered_map<uintptr_t, std::string> s_classCache;

inline std::string ReadCString(uintptr_t p, size_t maxn = 120) {
    if (!p) return "";
    char buf[128] = {};
    if (maxn > sizeof(buf) - 1) maxn = sizeof(buf) - 1;
    g_process.read_buf(p, buf, maxn);
    buf[sizeof(buf) - 1] = 0;
    return std::string(buf);
}

inline std::string ReadName(uintptr_t inst) {
    uintptr_t np = g_process.read<uintptr_t>(inst + offsets::Name);
    return np ? ReadCString(np + 8) : "";
}

inline std::string GetClassName(uintptr_t inst) {
    if (!inst) return "";
    uintptr_t cd = g_process.read<uintptr_t>(inst + offsets::ClassDescriptor);
    if (!cd) return "";
    auto it = s_classCache.find(cd);
    if (it != s_classCache.end()) return it->second;

    std::string name;
    uintptr_t sp = g_process.read<uintptr_t>(cd + offsets::ClassDescriptorToClassName);
    if (sp) {
        uint32_t len = g_process.read<uint32_t>(sp + offsets::StringLength);
        if (len > 0 && len <= 128) {
            if (len <= 15) name = ReadCString(sp, len);
            else { uintptr_t s = g_process.read<uintptr_t>(sp); if (s) name = ReadCString(s, len); }
        }
    }
    s_classCache[cd] = name;
    return name;
}

inline std::vector<uintptr_t> GetChildren(uintptr_t parent) {
    std::vector<uintptr_t> out;
    if (!parent) return out;
    uintptr_t vec = g_process.read<uintptr_t>(parent + offsets::Children);
    if (!vec) return out;
    uintptr_t begin = g_process.read<uintptr_t>(vec);
    uintptr_t end   = g_process.read<uintptr_t>(vec + 0x8);
    if (!begin || !end || end <= begin) return out;
    size_t count = (end - begin) / 16;
    if (count > 10000) count = 10000;
    std::vector<unsigned char> buf(count * 16);
    if (!g_process.read_buf(begin, buf.data(), buf.size())) return out;
    out.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        uintptr_t c = *reinterpret_cast<uintptr_t*>(&buf[i * 16]);
        if (c) out.push_back(c);
    }
    return out;
}

inline bool GetPartPosition(uintptr_t part, float out[3]) {
    if (!part) return false;
    uintptr_t prim = g_process.read<uintptr_t>(part + offsets::BasePartPrimitive);
    if (!prim) return false;
    return g_process.read_buf(prim + offsets::PrimitivePosition, out, sizeof(float) * 3);
}

inline bool WorldToScreen(const float pos[3], float out[2]) {
    const float* vm = g_viewMatrix;
    float x = pos[0] * vm[0] + pos[1] * vm[1] + pos[2] * vm[2] + vm[3];
    float y = pos[0] * vm[4] + pos[1] * vm[5] + pos[2] * vm[6] + vm[7];
    float w = pos[0] * vm[12] + pos[1] * vm[13] + pos[2] * vm[14] + vm[15];
    if (w < 0.1f) return false;
    float invW = 1.0f / w;
    out[0] = (g_screenW * 0.5f) + (x * invW * g_screenW * 0.5f);
    out[1] = (g_screenH * 0.5f) - (y * invW * g_screenH * 0.5f);
    return true;
}

// Call on a timer (~15ms); the chams target rebuild is throttled separately.
inline void RefreshPlayers() {
    uintptr_t fakeDM = g_process.read<uintptr_t>(g_base + offsets::FakeDataModelPointer);
    uintptr_t dm = fakeDM ? g_process.read<uintptr_t>(fakeDM + offsets::FakeDataModelToDataModel) : 0;
    if (!dm) { g_players.clear(); g_playersService = 0; g_localPlayer = 0; return; }

    uintptr_t ve = g_process.read<uintptr_t>(g_base + offsets::VisualEnginePointer);
    if (ve) {
        g_process.read_buf(ve + offsets::VisualEngineViewMatrix, g_viewMatrix, sizeof(g_viewMatrix));
        float dims[2] = {};
        if (g_process.read_buf(ve + offsets::VisualEngineDimensions, dims, sizeof(dims))) {
            if (dims[0] >= 1.0f && dims[1] >= 1.0f) {
                g_screenW = (int)dims[0];
                g_screenH = (int)dims[1];
            }
        }
    }

    g_workspace = g_process.read<uintptr_t>(dm + offsets::DataModelWorkspace);

    if (!g_playersService || GetClassName(g_playersService) != "Players") {
        g_playersService = 0;
        for (uintptr_t c : GetChildren(dm)) {
            if (GetClassName(c) == "Players") { g_playersService = c; break; }
        }
    }
    if (!g_playersService) { g_players.clear(); return; }

    g_localPlayer = g_process.read<uintptr_t>(g_playersService + offsets::LocalPlayer);

    g_players.clear();
    for (uintptr_t c : GetChildren(g_playersService)) {
        if (GetClassName(c) != "Player") continue;
        uintptr_t model = g_process.read<uintptr_t>(c + offsets::ModelInstance);
        g_players.push_back({ c, model });
    }
}

} // namespace nc
