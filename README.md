# champoc

Native chams for Roblox with an ImGui menu, plus the tooling to re-derive the
offsets theo's API doesn't serve.

The chams (D3D11 vtable hook + shader pack + geometry walk) is not mine. It was
taken from **thenwefuckin** — <https://discord.gg/thenwefuckin>. Original file:
`reference/thenwefuckin_chams.cpp`. Keep the credit.

## how it works

1. Find the renderer: `VisualEngine -> RenderView -> DeviceD3D11`.
2. Clone the `DeviceContextD3D11` wrapper's vtable into an allocated page, put a
   thunk in slot 28, and swap the object's vtable pointer to the clone. The thunks
   are marked as valid CFG targets so the virtual call is allowed.
3. Collect the `GeometryD3D11*` pointers for each character
   (players -> parts -> cluster node -> FastCluster entities -> material).
4. At draw time the thunk checks whether the draw's geometry pointer is a target.
   If so it binds a chams pixel shader, calls the original draw, then restores the
   previous shader/state.

All of this is done from outside the process by patching remote memory.

## layout

```
src/main.cpp              overlay (DirectComposition + ImGui) + menu + loop
src/chams_native.inc      chams body (from thenwefuckin)
src/chams_native.h        include wrapper
src/chams_native_shim.h   maps the body onto process.hpp/game.hpp
src/process.hpp           OpenProcess + RPM/WPM
src/game.hpp              instance walk, player list, world->screen
src/offsets.hpp           all offsets
tools/main.py             FastClusterEntity::VTableRva + field offsets
tools/rva_all.py          RTTI scan for the device/geometry/fastcluster vtables
tools/debug_walk.py       prints the DataModel -> Players -> character walk
reference/                original thenwefuckin source
vendor/imgui
```

## build

Open `champoc.sln`, build Release x64, run `bin\champoc.exe` (asks for admin).
Launch Roblox first. INSERT opens the menu, END quits.

Menu: enable, style (26), color, glow (color + strength), through walls.

## offsets

Roblox recompiles every patch, so these move.

**general** (`src/offsets.hpp`): from theo. Get the current version at
`https://offsets.imtheo.lol/roblox/version`, then copy what you need from
`https://offsets.imtheo.lol/<version>/offsets.hpp`.

**chams / d3d**: theo doesn't have these. With Roblox running:

```
py tools\main.py       # FastClusterEntity::VTableRva
py tools\rva_all.py    # GeometryD3D11, DeviceD3D11, FastCluster, FastClusterBinding
```

Paste the results into the `chams / d3d` group in `src/offsets.hpp`. The struct
field offsets (`D3D_DevicePtr`, `D3D_ContextObj`, `FastCluster::BindingSubobject`,
`FastClusterEntity::MaterialPtr`, ...) rarely change.

## notes

- through walls needs `FFlag::RenderFastClusterOcclusionCulling4` (a module byte,
  not in theo's dump). Left at `0` here, so that toggle does nothing right now.
- it's a render hook, so it is not stream-proof.
