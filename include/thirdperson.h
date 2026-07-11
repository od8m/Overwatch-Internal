#pragma once
#include "freecam.h"

inline void ApplyThirdPerson()
{
	if (!g_thirdPerson || g_freecam || !SDK || !g_debug.vm_ok) return;

	float3 origin = {};
	float yaw = 0.f;
	bool got = false;

	for (auto& e : g_entities) {
		if (!e.isLocal) continue;
		origin = e.hasBones ? e.bones[0] : e.pos;
		yaw = e.rot_y;
		got = true;
		break;
	}
	if (!got) return;

	float dist = g_thirdPersonDist;
	if (dist < 1.f) dist = 1.f;
	if (dist > 12.f) dist = 12.f;

	float pitch = 0.25f;
	float cp = cosf(pitch), sp = sinf(pitch);
	float cy = cosf(yaw), sy = sinf(yaw);
	float3 fwd = { sy * cp, sp, cy * cp };

	float3 cam = {
		origin.x - fwd.x * dist,
		origin.y - fwd.y * dist + 0.6f,
		origin.z - fwd.z * dist
	};

	Matrix viewMtx;
	BuildViewMatrix(cam, yaw, pitch, viewMtx);

	Matrix projMtx = {};
	if (g_vmCameraPtr)
		projMtx = SDK->RPM<Matrix>(g_vmCameraPtr + offset::VM_ProjMatrix);
	projMtx.data[3] = 0.f;

	g_viewMatrix = MulMatrix(viewMtx, projMtx);
	g_cameraPos = cam;
	g_debug.vm_w = g_viewMatrix.data[15];
}
