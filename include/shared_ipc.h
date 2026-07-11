#pragma once
#include "sdk.hpp"
#include <cstdint>

#define RS_SHM_NAME     L"Local\\RsExternal_Shm_v1"
#define RS_SHM_MAGIC    0x52534578u
#define RS_SHM_VERSION  1u
#define RS_SHM_MAX_ENT  64u

#pragma pack(push, 8)

struct ShmEntity {
    uint64_t addr;
    uint64_t common;
    uint64_t link;
    uint16_t heroId;
    uint16_t _pad0;
    uint32_t uid;
    float health, health_max;
    float hp, hp_max, armor, armor_max, barrier, barrier_max;
    float3 pos;
    float rot_y;
    float3 bones[18];
    int boneCount;
    int team;
    uint32_t teamRaw;
    uint64_t pBoneData;
    uint8_t hasBones;
    uint8_t visible;
    uint8_t hasPC;
    uint8_t _pad1;
    char playerName[48];
};

struct RsSharedBlock {
    volatile uint64_t seq;
    uint64_t publishTick;
    uint32_t magic;
    uint32_t version;
    uint32_t publisherPid;
    uint32_t entityCount;
    uint32_t linkCount;
    uint32_t finalEntities;
    uint32_t _pad;
    bool     vmOk;
    float    vmW;
    Matrix   viewMatrix;
    float3   cameraPos;
    uint64_t vmCameraPtr;
    ShmEntity entities[RS_SHM_MAX_ENT];
};

#pragma pack(pop)

extern bool g_internalReaderActive;

void SharedIpc_InitConsumer();
void SharedIpc_ShutdownConsumer();
bool SharedIpc_TryConsume();

#ifdef RS_INTERNAL_READER
void SharedIpc_InitPublisher();
void SharedIpc_ShutdownPublisher();
void SharedIpc_Publish();
#endif
