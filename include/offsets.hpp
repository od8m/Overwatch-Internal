#pragma once
#include <cstdint>

// Overwatch 2.24.1.0.153480 — dumped 2026-09-19 
namespace offset {

    constexpr uint64_t ENTITY_LIST           = 0x3AB5AC8;
    constexpr uint64_t ENTITY_REGISTRY       = 0x3AB5AB4;  // capacity @+0, count @+4, slots ptr @+0x14
    constexpr uint64_t CAMERA_ENC            = 0x3A55C48;
    constexpr uint64_t CLIENT_GAME           = 0x3C17AA8;
    constexpr uint64_t GLOBAL_ADMIN          = 0x3C1D810;
    constexpr uint64_t GKEY_PTR              = 0x3C17988;
    constexpr uint64_t BYTE_KEY              = 0x38F0A5B;
    constexpr uint64_t SM_XOR_BYTE           = 0x38F011A;
    constexpr uint64_t COMP_KEY_OFF          = 0x32;
    constexpr uint64_t SM_GKEY_OFF           = 0x55;
    constexpr uint64_t FOV_RVA               = 0x41D3E68;
    constexpr uint64_t VERSION_STRING        = 0x4239B28;
    constexpr uint64_t LAT_OVERRIDE_RVA      = 0x391ADE0;

    constexpr uint64_t ENT_COMP_BASE         = 0x80;
    constexpr uint64_t ENT_BITMAP            = 0x110;
    constexpr uint64_t ENT_INDEX             = 0x130;
    constexpr uint64_t ENT_UID               = 0x138;

    // (decrypt_camera)
    constexpr uint64_t VM_P1                 = 0x20;
    constexpr uint64_t VM_P2                 = 0x48;
    constexpr uint64_t VM_ViewMatrix         = 0x140;
    constexpr uint64_t VM_ProjMatrix         = 0xB0;

    // globaladmin decrypt chain → singleton_table @ +0x50 after decrypt
    constexpr uint64_t GlobalAdmin_EncOff    = 0x160;
    constexpr uint64_t SingletonTableOff     = 0x50;

    constexpr uint64_t VisibilityValueOffset = 0x98;

    constexpr uint64_t Address_entity_base      = ENTITY_LIST;
    constexpr uint64_t EntityList_SlotCount_RVA = ENTITY_REGISTRY;
    constexpr uint64_t EntityList_PtrOffset     = 0x14;
    constexpr uint64_t OW_COMPONENT_QWORD       = GKEY_PTR;
    constexpr uint64_t OW_COMPONENT_BYTE        = BYTE_KEY;
    constexpr uint64_t ComponentXorQwordOffset  = COMP_KEY_OFF;
    constexpr uint64_t OW_VIEWMATRIX_ENC        = CAMERA_ENC;
    constexpr uint64_t GlobalAdmin_WorldBz_RVA  = GLOBAL_ADMIN;
    constexpr uint64_t Fov_Changer_RVA          = FOV_RVA;
    constexpr uint64_t Ent_CompBase             = ENT_COMP_BASE;
    constexpr uint64_t Ent_Bitmap               = ENT_BITMAP;
    constexpr uint64_t Ent_Index                = ENT_INDEX;
    constexpr uint64_t Ent_Uid                  = ENT_UID;
}

enum eComponentType : int32_t {
    TYPE_TRANSFORM         = 0x01,
    TYPE_VELOCITY          = 0x04,
    TYPE_TEAM              = 0x20,
    TYPE_BONE              = 0x24,
    TYPE_ROTATION          = 0x2F,
    TYPE_LINK              = 0x33,
    TYPE_P_VISIBILITY      = 0x34,   // check_visible: decrypt_component(ent, 0x34)
    TYPE_SKILL             = 0x37,
    TYPE_HEALTH            = 0x3A,
    TYPE_PLAYERCONTROLLER  = 0x42,
    TYPE_P_HEROID          = 0x82,
    TYPE_OUTLINE           = 0x5A,
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
