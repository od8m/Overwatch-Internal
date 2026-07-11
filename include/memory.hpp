#pragma once
#include "sdk.hpp"
#include "utils.hpp"
#include "offsets.hpp"

void ReadEntities();
void ReadViewMatrix();
void ResetEntityListResolver();
void ResetMemoryAuxCaches();
bool WorldToScreen(float3 world, float& sx, float& sy);
inline bool IsValidPtr(uint64_t p) {
    return p >= 0x10000ULL && p < 0x00007FFFFFFFFFFFULL;
}