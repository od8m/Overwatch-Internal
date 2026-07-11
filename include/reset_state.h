#pragma once
#include "sdk.hpp"
#include "memory.hpp"
#include "features.hpp"
#include "render.hpp"

// Clears cached entity list / local player / smoothing state and re-reads game data
// (same effect as reinjecting without closing the game).
void ResetReaderRuntime();
