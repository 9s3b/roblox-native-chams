#pragma once
// Glue between the ported chams body (chams_native.inc) and this project's
// process/game helpers. Provides the names the body expects: g_proc, node,
// pcache, g_ctx, settings, math, projection, Offsets, LOGF.

#include <Windows.h>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>

#include "process.hpp"
#include "game.hpp"
#include "offsets.hpp"

using namespace ::offsets;

struct ChamsSettings {
    bool  enabled = false;
    int   shader = 0;
    float color[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    bool  glow = false;
    float glowColor[4] = { 0.0f, 1.0f, 1.0f, 1.0f };
    float glowStrength = 1.0f;
    bool  walls = false;
};
inline ChamsSettings g_chams;

namespace nc {
    inline void Logf(const char* fmt, ...) {
        char buf[1024];
        va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
        FILE* f = nullptr;
        if (fopen_s(&f, "nativechams.log", "a") == 0 && f) { fprintf(f, "%s\n", buf); fclose(f); }
    }
}
#define LOGF(tag, fmt, ...) nc::Logf(fmt, __VA_ARGS__)

namespace thenwefuckin {

    inline nc::Process* g_proc = &nc::g_process;

    namespace Offsets {
        namespace VisualEngine {
            inline std::uintptr_t& Pointer    = VisualEnginePointer;
            inline std::uintptr_t& RenderView = VisualEngineRenderView;
            inline std::uintptr_t& ViewMatrix = VisualEngineViewMatrix;
            inline std::uintptr_t& Dimensions = VisualEngineDimensions;
        }
        namespace RenderView { inline constexpr std::uintptr_t DeviceD3D11 = 0x8; }
        namespace DeviceD3D11 {
            inline std::uintptr_t& DevicePtr    = D3D_DevicePtr;
            inline std::uintptr_t& ContextPtr   = D3D_ContextPtr;
            inline std::uintptr_t& ContextObj   = D3D_ContextObj;
            inline std::uintptr_t& SwapChainPtr = D3D_SwapChainPtr;
            inline std::uintptr_t& VTableRva    = D3D_VTableRva;
        }
        namespace GeometryD3D11 { inline std::uintptr_t& VTableRva = GeometryD3D11VTableRva; }
        namespace FastCluster {
            inline constexpr std::uintptr_t BindingSubobject = 0x90;
            inline constexpr std::uintptr_t EntityBegin      = 0x48;
            inline constexpr std::uintptr_t EntityEnd        = 0x50;
            inline std::uintptr_t& VTableRva    = FastClusterVTableRva;
            inline std::uintptr_t& VTableRvaSub = FastClusterVTableRvaSub;
        }
        namespace FastClusterBinding {
            inline constexpr std::uintptr_t Owner     = 0x60;
            inline constexpr std::uintptr_t Primitive = 0x8;
            inline std::uintptr_t& VTableRva = FastClusterBindingVTableRva;
        }
        namespace FastClusterEntity {
            inline constexpr std::uintptr_t MaterialPtr      = 0x20;
            inline constexpr std::uintptr_t DecalMaterialPtr = 0x48;
            inline std::uintptr_t& VTableRva = FastClusterEntityVTableRva;
        }
        namespace Primitive { inline std::uintptr_t& Position = PrimitivePosition; }
        namespace BasePart {
            inline std::uintptr_t& Primitive   = BasePartPrimitive;
            inline std::uintptr_t& ClusterNode = BasePartClusterNode;
        }
        namespace FFlag { inline std::uintptr_t& RenderFastClusterOcclusionCulling4 = RenderFastClusterOcclusionCulling4; }
    }

    namespace math {
        struct vec2 { float x = 0.f, y = 0.f; vec2() = default; vec2(float a, float b) : x(a), y(b) {} };
        struct vec3 { float x = 0.f, y = 0.f, z = 0.f; vec3() = default; vec3(float a, float b, float c) : x(a), y(b), z(c) {} };
        struct vec4 { float x = 0.f, y = 0.f, z = 0.f, w = 0.f; };
        struct view_matrix {
            float m[16]{};
            vec4 transform(const vec3& p) const {
                vec4 r;
                r.x = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
                r.y = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
                r.z = m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14];
                r.w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
                return r;
            }
        };
    }

    struct node {
        std::uintptr_t addr = 0;
        node() = default;
        explicit node(std::uintptr_t a) : addr(a) {}
        explicit operator bool() const { return addr != 0; }
        std::string class_name() const { return nc::GetClassName(addr); }
        std::vector<node> children() const {
            std::vector<node> o;
            for (std::uintptr_t c : nc::GetChildren(addr)) o.emplace_back(c);
            return o;
        }
    };

    struct context {
        node local_player;
        math::view_matrix view{};
        node get_character(const node& p) const {
            if (!p.addr) return node{};
            return node{ nc::g_process.read<std::uintptr_t>(p.addr + ModelInstance) };
        }
    };
    inline context g_ctx;

    namespace pcache {
        struct Entry {
            std::uintptr_t player = 0, model = 0, humanoid = 0, team = 0;
            float hp = 0.f, maxHp = 100.f;
            std::string name, teamStr, tool;
            bool valid = false, isR6 = true;
        };
        inline std::vector<Entry> entries;
        inline std::mutex mtx;
        inline void update() {
            std::vector<Entry> v;
            v.reserve(nc::g_players.size());
            for (const auto& p : nc::g_players) {
                Entry e{};
                e.player = p.player;
                e.model  = p.model;
                e.valid  = (p.model != 0);
                // health not read here; maxHp=0 means "unknown" so the body's
                // (hp<=0 && maxHp>0) dead check never filters anyone out.
                e.hp = 1.f;
                e.maxHp = 0.f;
                v.push_back(std::move(e));
            }
            { std::lock_guard<std::mutex> lk(mtx); entries = std::move(v); }
            g_ctx.local_player = node{ nc::g_localPlayer };
            std::memcpy(g_ctx.view.m, nc::g_viewMatrix, sizeof g_ctx.view.m);
        }
    }

    namespace settings {
        inline bool&  chams_enabled       = g_chams.enabled;
        inline int&   chams_shader        = g_chams.shader;
        inline bool&  chams_glow          = g_chams.glow;
        inline float& chams_glow_strength = g_chams.glowStrength;
        inline bool&  chams_walls         = g_chams.walls;
        inline float (&chams_color)[4]      = g_chams.color;
        inline float (&chams_glow_color)[4] = g_chams.glowColor;
    }

    namespace projection {
        inline bool to_screen(const math::vec3& world, const math::view_matrix&, float, float, math::vec2& out) {
            float p[3] = { world.x, world.y, world.z };
            float o[2];
            if (!nc::WorldToScreen(p, o)) return false;
            out.x = o[0]; out.y = o[1];
            return true;
        }
        inline math::vec2 screen_size() { return math::vec2((float)nc::g_screenW, (float)nc::g_screenH); }
    }

} // namespace thenwefuckin
