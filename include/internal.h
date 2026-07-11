#pragma once
#include <cstdint>

struct InternalEntity {
    float pos[3];
    float bones[18][3];
    int   boneCount;
    bool  hasBones;
    bool  visible;
    bool  isLocal;
    float health;
    float healthMax;
    uint8_t team;
    uint16_t heroId;
};

struct InternalSettings {
    bool drawBoxes;
    bool drawSkeleton;
    bool drawLines;
    bool draw3dBox;
    bool teamCheck;
    bool visCheck;
    bool ultBars;
    bool ultPanel;
    bool drawUltBars;
    float boxCol[4];
    float skelCol[4];
    float invSkelCol[4];
};

struct InternalData {
    float viewMatrix[16];
    float cameraPos[3];
    float screenW;
    float screenH;
    int   entityCount;
    InternalEntity entities[64];
    InternalSettings settings;
    volatile int frameCount;
};
