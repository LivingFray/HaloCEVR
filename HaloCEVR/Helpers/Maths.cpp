#include "Maths.h"

void Helpers::MakeTransformFromXZ(const Vector3* facingVector, const Vector3* upVector, Transform* outTransform)
{
	outTransform->scale = 1.0;
	outTransform->rotation[0] = upVector->x;
	outTransform->rotation[1] = upVector->y;
	outTransform->rotation[2] = upVector->z;
	outTransform->rotation[3] = facingVector->y * upVector->z - facingVector->z * upVector->y;
	outTransform->rotation[4] = facingVector->z * upVector->x - upVector->z * facingVector->x;
	outTransform->rotation[5] = facingVector->x * upVector->y - facingVector->y * upVector->x;
	outTransform->rotation[6] = facingVector->x;
	outTransform->rotation[7] = facingVector->y;
	outTransform->rotation[8] = facingVector->z;
	(outTransform->translation).x = 0.0;
	(outTransform->translation).y = 0.0;
	(outTransform->translation).z = 0.0;
}

void Helpers::MakeTransformFromQuat(const Vector4* quaternion, Transform* outTransform)
{
	float twoZZ;
	float twoXW;
	float twoYW;
	float twoZW;
	float twoXX;
	float twoXY;
	float twoXZ;
	float twoYY;
	float twoYZ;
	float twoOverLength;

	float len = quaternion->w * quaternion->w + quaternion->z * quaternion->z + quaternion->y * quaternion->y + quaternion->x * quaternion->x;
	if (len == 0.0f) {
		twoOverLength = 0.0f;
	}
	else {
		twoOverLength = 2.0f / len;
	}
	float twoX = twoOverLength * quaternion->x;
	float twoY = twoOverLength * quaternion->y;
	float twoZ = twoOverLength * quaternion->z;
	twoXW = twoX * quaternion->w;
	twoYW = twoY * quaternion->w;
	twoZW = twoZ * quaternion->w;
	twoXX = twoX * quaternion->x;
	twoXY = twoY * quaternion->x;
	twoXZ = twoZ * quaternion->x;
	twoYY = twoY * quaternion->y;
	twoYZ = twoZ * quaternion->y;
	twoZZ = twoZ * quaternion->z;
	outTransform->scale = 1.0f;
	(outTransform->translation).x = 0.0f;
	(outTransform->translation).y = 0.0f;
	(outTransform->translation).z = 0.0f;
	outTransform->rotation[0] = 1.0f - (twoYY + twoZZ);
	outTransform->rotation[1] = twoXY - twoZW;
	outTransform->rotation[2] = twoXZ + twoYW;
	outTransform->rotation[3] = twoXY + twoZW;
	outTransform->rotation[4] = 1.0f - (twoZZ + twoXX);
	outTransform->rotation[5] = twoYZ - twoXW;
	outTransform->rotation[6] = twoXZ - twoYW;
	outTransform->rotation[7] = twoYZ + twoXW;
	outTransform->rotation[8] = 1.0f - (twoYY + twoXX);
}

void Helpers::CombineTransforms(const Transform* transformA, const Transform* transformB, Transform* outTransform)
{
	Transform tempTransform;

	if (transformA == outTransform) {
		tempTransform = *transformA;
		transformA = &tempTransform;
	}
	if (transformB == outTransform) {
		tempTransform = *transformB;
		transformB = &tempTransform;
	}
	outTransform->rotation[0] =
		transformB->rotation[2] * transformA->rotation[6] +
		transformA->rotation[0] * transformB->rotation[0] + transformA->rotation[3] * transformB->rotation[1];
	outTransform->rotation[1] =
		transformA->rotation[4] * transformB->rotation[1] +
		transformA->rotation[1] * transformB->rotation[0] + transformA->rotation[7] * transformB->rotation[2];
	outTransform->rotation[2] =
		transformA->rotation[5] * transformB->rotation[1] +
		transformA->rotation[2] * transformB->rotation[0] + transformA->rotation[8] * transformB->rotation[2];
	outTransform->rotation[3] =
		transformA->rotation[3] * transformB->rotation[4] +
		transformB->rotation[5] * transformA->rotation[6] + transformA->rotation[0] * transformB->rotation[3];
	outTransform->rotation[4] =
		transformA->rotation[1] * transformB->rotation[3] +
		transformA->rotation[4] * transformB->rotation[4] + transformA->rotation[7] * transformB->rotation[5];
	outTransform->rotation[5] =
		transformA->rotation[2] * transformB->rotation[3] +
		transformA->rotation[5] * transformB->rotation[4] + transformA->rotation[8] * transformB->rotation[5];
	outTransform->rotation[6] =
		transformA->rotation[3] * transformB->rotation[7] +
		transformA->rotation[0] * transformB->rotation[6] + transformB->rotation[8] * transformA->rotation[6];
	outTransform->rotation[7] =
		transformB->rotation[7] * transformA->rotation[4] +
		transformB->rotation[8] * transformA->rotation[7] + transformA->rotation[1] * transformB->rotation[6];
	outTransform->rotation[8] =
		transformB->rotation[7] * transformA->rotation[5] +
		transformB->rotation[8] * transformA->rotation[8] + transformA->rotation[2] * transformB->rotation[6];
	(outTransform->translation).x =
		((transformB->translation).z * transformA->rotation[6] +
			transformA->rotation[3] * (transformB->translation).y + transformA->rotation[0] * (transformB->translation).x) * transformA->scale +
		(transformA->translation).x;
	(outTransform->translation).y =
		((transformB->translation).x * transformA->rotation[1] +
			(transformB->translation).y * transformA->rotation[4] + transformA->rotation[7] * (transformB->translation).z) * transformA->scale +
		(transformA->translation).y;
	(outTransform->translation).z =
		((transformB->translation).x * transformA->rotation[2] +
			(transformB->translation).y * transformA->rotation[5] + transformA->rotation[8] * (transformB->translation).z) * transformA->scale +
		(transformA->translation).z;
	outTransform->scale = transformA->scale * transformB->scale;
}


Vector3 Helpers::Lerp(const Vector3& a, const Vector3& b, float t)
{
	return a + t * (b - a);
}
