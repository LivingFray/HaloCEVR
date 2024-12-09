#pragma once
#include "../Maths/Matrices.h"

struct Transform
{
	float scale;
	float rotation[9];
	Vector3 translation;
};

struct TransformQuat
{
	Vector4 rotation; // Quaternion
	Vector3 translation;
	float scale;
};

namespace Helpers
{
	void MakeTransformFromXZ(const Vector3* facingVector, const Vector3* upVector, Transform* outTransform);
	void MakeTransformFromQuat(const Vector4* quaternion, Transform* outTransform);
	void CombineTransforms(const Transform* transformA, const Transform* transformB, Transform* outTransform);
	Vector3 Lerp(const Vector3& a, const Vector3& b, float t);
}