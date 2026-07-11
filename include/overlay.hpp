#pragma once
#include "sdk.hpp"

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
HWND MakeOverlay(HWND gameParent);
void SyncOverlayToGame(HWND overlay, HWND game, bool menuOpen);
void DestroyOverlay();
bool InitD3D(HWND hWnd);
void CreateRT();
void CleanupD3D();