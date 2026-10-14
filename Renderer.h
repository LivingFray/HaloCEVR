#pragma once
#include <cstdint>
#include <d3d9.h>
#include "../Maths/Vectors.h"
#include "Maths.h"

struct Viewport
{
	float left;
	float right;
	float top;
	float bottom;
};
static_assert(sizeof(Viewport) == 0x10);

struct sRect
{
	short top;
	short left;
	short bottom;
	short right;
};
static_assert(sizeof(sRect) == 0x8);

struct CameraFrustum
{
	Vector3 position;
	Vector3 facingDirection;
	Vector3 upDirection;
	// Native render visibility permits drawing the local player body.
	bool drawPlayer;
	std::uint8_t unk0[3];
	float fov;
	sRect WindowViewport;
	sRect InnerViewport;
	// zNear == 0 selects the explicit world clip plane below in the native builder.
	float zNear;
	float zFar;
	Vector4 unk1; // Explicit/oblique near plane when zNear == 0.
};
static_assert(sizeof(CameraFrustum) == 0x54);

struct Renderer
{
	short playerId;
	bool unk1;
	// Possibly just padding
	std::uint8_t unk2;
	// Native render_scene uses the first for visibility/culling, the second for
	// rasterization. Normally they match; cockpit culling adds a bounded margin.
	CameraFrustum frustum;
	CameraFrustum frustum2;
};
static_assert(sizeof(Renderer) == 0xac);

struct CameraRenderMatrices
{
	Viewport viewport;
	Transform viewMatrix;
	Transform matrix;
	Vector4 quaternions[6];
	float zNear;
	float zFar;
	Vector3 frustumCorners[4];
	Vector3 cameraPosition;
	Vector3 frustumCentre;
	float floats[6];
	uint32_t unk0;
	D3DMATRIX projectionMatrix;
	float unk1;
	float unk2;
};

namespace Helpers
{
	sRect* GetWindowRect();
	sRect* GetCurrentRect();
	CameraRenderMatrices* GetActiveCameraMatrices();
}