#pragma once
#include "sdk.hpp"
#include "utils.hpp"
#include "offsets.hpp"
#include "memory.hpp"
#include <cmath>

inline bool StealthKeyDown(int vk)
{
    static BYTE s_keys[256] = {};
    if (!GetKeyboardState(s_keys)) return false;
    return (s_keys[vk] & 0x80) != 0;
}

inline void ViewMatrixToAngles(const Matrix& m, float& yaw, float& pitch)
{
	float fx = -m.data[2];
	float fy = -m.data[6];
	float fz = -m.data[10];
	float len = sqrtf(fx * fx + fy * fy + fz * fz);
	if (len > 0.0001f) { fx /= len; fy /= len; fz /= len; }
	pitch = asinf(fminf(1.f, fmaxf(-1.f, fy)));
	yaw = atan2f(fx, fz);
}

inline void BuildViewMatrix(const float3& pos, float yaw, float pitch, Matrix& out)
{
	float cp = cosf(pitch), sp = sinf(pitch);
	float cy = cosf(yaw), sy = sinf(yaw);

	float fx = sy * cp;
	float fy = sp;
	float fz = cy * cp;
	float rx = cy;
	float ry = 0.f;
	float rz = -sy;
	float ux = -sy * sp;
	float uy = cp;
	float uz = -cy * sp;

	out = {};
	out.data[0]  = rx;  out.data[4]  = ry;  out.data[8]  = rz;
	out.data[1]  = ux;  out.data[5]  = uy;  out.data[9]  = uz;
	out.data[2]  = -fx; out.data[6]  = -fy; out.data[10] = -fz;
	out.data[15] = 1.f;
	out.data[12] = -(rx * pos.x + ry * pos.y + rz * pos.z);
	out.data[13] = -(ux * pos.x + uy * pos.y + uz * pos.z);
	out.data[14] =  (fx * pos.x + fy * pos.y + fz * pos.z);
}

inline void ApplyFreecam()
{
	static bool   s_active = false;
	static float3 s_pos = {};
	static float  s_yaw = 0.f, s_pitch = 0.f;
	static Matrix s_savedView = {};
	static POINT  s_lastMouse = {};
	static uint64_t s_lastTick = 0;

	if (!g_freecam) {
		s_active = false;
		return;
	}

	if (!SDK || !g_debug.vm_ok) return;

	uint64_t now = GetTickCount64();
	float dt = s_lastTick ? (float)(now - s_lastTick) * 0.001f : 0.007f;
	if (dt > 0.05f) dt = 0.05f;
	s_lastTick = now;

	if (!s_active) {
		if (g_vmCameraPtr)
			s_savedView = SDK->RPM<Matrix>(g_vmCameraPtr + offset::VM_ViewMatrix);
		s_pos = g_cameraPos;
		ViewMatrixToAngles(s_savedView, s_yaw, s_pitch);
		s_active = true;
		GetCursorPos(&s_lastMouse);
	}

	float speed = g_freecamSpeed * dt;
	if (speed < 0.01f) speed = 0.01f;

	float cp = cosf(s_pitch), sp = sinf(s_pitch);
	float cy = cosf(s_yaw), sy = sinf(s_yaw);
	float3 fwd = { sy * cp, sp, cy * cp };
	float3 right = { cy, 0.f, -sy };

	if (StealthKeyDown('W')) { s_pos.x += fwd.x * speed; s_pos.y += fwd.y * speed; s_pos.z += fwd.z * speed; }
	if (StealthKeyDown('S')) { s_pos.x -= fwd.x * speed; s_pos.y -= fwd.y * speed; s_pos.z -= fwd.z * speed; }
	if (StealthKeyDown('A')) { s_pos.x -= right.x * speed; s_pos.z -= right.z * speed; }
	if (StealthKeyDown('D')) { s_pos.x += right.x * speed; s_pos.z += right.z * speed; }
	if (StealthKeyDown(VK_SPACE)) s_pos.y += speed;
	if (StealthKeyDown(VK_LCONTROL)) s_pos.y -= speed;

	if (StealthKeyDown(VK_RBUTTON)) {
		POINT cur;
		GetCursorPos(&cur);
		s_yaw   += (float)(cur.x - s_lastMouse.x) * 0.003f;
		s_pitch -= (float)(cur.y - s_lastMouse.y) * 0.003f;
		if (s_pitch >  1.45f) s_pitch =  1.45f;
		if (s_pitch < -1.45f) s_pitch = -1.45f;
		s_lastMouse = cur;
	} else {
		GetCursorPos(&s_lastMouse);
	}

	Matrix viewMtx;
	BuildViewMatrix(s_pos, s_yaw, s_pitch, viewMtx);

	Matrix projMtx = {};
	if (g_vmCameraPtr)
		projMtx = SDK->RPM<Matrix>(g_vmCameraPtr + offset::VM_ProjMatrix);
	projMtx.data[3] = 0.f;

	g_viewMatrix = MulMatrix(viewMtx, projMtx);
	g_cameraPos = s_pos;
	g_debug.vm_w = g_viewMatrix.data[15];
	g_debug.vm_ok = fabsf(g_viewMatrix.data[15]) > 0.0001f;
}
