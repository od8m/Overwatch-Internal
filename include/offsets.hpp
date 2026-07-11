#pragma once
#include <cstdint>

namespace offset {
    constexpr uint64_t Address_entity_base      = 0x39BB858;
    constexpr uint64_t EntityList_SlotCount_RVA = 0x39BB840;
    constexpr uint64_t OW_COMPONENT_QWORD       = 0x3B19B70;
    constexpr uint64_t OW_COMPONENT_BYTE        = 0x38002A2;
    constexpr uint64_t OW_VIEWMATRIX_ENC        = 0x3960960;
    constexpr uint64_t GlobalAdmin_WorldBz_RVA    = 0x3B1F9F0;
    constexpr uint64_t ComponentXorQwordOffset    = 0x147;
    constexpr uint64_t VisibilityValueOffset      = 0x98;
    constexpr uint64_t InputMouseScaleX_RVA       = 0x3806B2C;
    constexpr uint64_t InputMouseScaleY_RVA       = 0x3806B44;
    constexpr uint64_t Fov_Changer_RVA              = 0x40C7698;
    constexpr uint64_t FreeCam_HookFunction_RVA     = 0x1FEB520;
    constexpr uint64_t TeamVision_HookFunction_RVA  = 0x20F00C0;

    constexpr uint64_t VM_P1                      = 0x20;
    constexpr uint64_t VM_P2                      = 0x48;
    constexpr uint64_t VM_ViewProjectionParent    = 0x6D8;
    constexpr uint64_t VM_ViewProjectionPtr       = 0x8;
    constexpr uint64_t VM_ViewProjectionMatrix    = 0xC0;
    constexpr uint64_t VM_ViewMatrix              = 0x140;
    constexpr uint64_t VM_ProjMatrix              = 0xB0;

    constexpr uint64_t ViewMatrix_Sub             = 0x1F81C7CF462C79B2ULL;
    constexpr uint64_t ViewMatrix_Xor             = 0x60548C3F0B3C2134ULL;
    constexpr uint64_t EntityList_PtrOffset       = 0x18;
}

enum eComponentType : int32_t {
    TYPE_TRANSFORM         = 0x01,
    TYPE_VELOCITY          = 0x04,
    TYPE_TEAM              = 0x21,
    TYPE_BONE              = 0x24,
    TYPE_LINK              = 0x34,
    TYPE_P_VISIBILITY      = 0x35,
    TYPE_SKILL             = 0x37,
    TYPE_HEALTH            = 0x3B,
    TYPE_ROTATION          = 0x2F,
    TYPE_PLAYERCONTROLLER  = 0x43,
    TYPE_P_HEROID          = 0x54,
    TYPE_OUTLINE           = 0x5B,
    TYPE_ABILITY           = 0x86,
};

static const int g_boneIds[18] = { 17, 16, 81, 82, 49, 54, 14, 51, 86, 96, 87, 97, 41, 71, 99, 89, 100, 90 };

static const int g_boneLines[17][2] = {
    {0,1}, {1,2}, {2,3},
    {1,4}, {4,6}, {6,12},
    {1,5}, {5,7}, {7,13},
    {3,8}, {8,10}, {10,15},
    {3,9}, {9,11}, {11,14},
    {15,17}, {14,16}
};
