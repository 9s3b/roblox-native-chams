#pragma once
#include <cstdint>

// Offsets for the running Roblox build.
//
// "general" comes from theo's dump (https://offsets.imtheo.lol) and changes every
// update. The chams/D3D RVAs are not in that dump - re-derive them with
// tools/main.py and tools/rva_all.py (see README).
//
// current build: version-11f155f3450f4c8e
namespace offsets {

    // general
    inline uintptr_t VisualEnginePointer        = 0x884c2e0;
    inline uintptr_t VisualEngineViewMatrix     = 0x1b0;
    inline uintptr_t VisualEngineRenderView     = 0xc38;
    inline uintptr_t VisualEngineDimensions     = 0xb10;
    inline uintptr_t FakeDataModelPointer       = 0x8dc8000;
    inline uintptr_t FakeDataModelToDataModel   = 0x1f8;
    inline uintptr_t DataModelWorkspace         = 0x150;
    inline uintptr_t ClassDescriptor            = 0x18;
    inline uintptr_t ClassDescriptorToClassName = 0x8;
    inline uintptr_t Children                   = 0x78;
    inline uintptr_t Name                       = 0x70;
    inline uintptr_t StringLength               = 0x10;
    inline uintptr_t LocalPlayer                = 0x120;
    inline uintptr_t ModelInstance              = 0x298;
    inline uintptr_t BasePartPrimitive          = 0x180;
    inline uintptr_t BasePartClusterNode        = 0x188;
    inline uintptr_t PrimitivePosition          = 0xd4;

    // chams / d3d (re-derive every update)
    inline uintptr_t D3D_DevicePtr               = 0x178;
    inline uintptr_t D3D_ContextPtr              = 0x180;
    inline uintptr_t D3D_ContextObj              = 0x1b0;
    inline uintptr_t D3D_SwapChainPtr            = 0x80;
    inline uintptr_t D3D_VTableRva               = 0x6EBFEF8;
    inline uintptr_t GeometryD3D11VTableRva      = 0x6EC0678;
    inline uintptr_t FastClusterVTableRva        = 0x6F2AD80;
    inline uintptr_t FastClusterVTableRvaSub     = 0x6F2ADA0;
    inline uintptr_t FastClusterBindingVTableRva = 0x6F2C080;
    inline uintptr_t FastClusterEntityVTableRva  = 0x6F2C158;

    // through-walls flag; 0 = disabled (not derived yet)
    inline uintptr_t RenderFastClusterOcclusionCulling4 = 0x0;
}
