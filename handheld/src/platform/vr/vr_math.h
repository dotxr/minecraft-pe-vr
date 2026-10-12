#ifndef PLATFORM_VR_VR_MATH_H__
#define PLATFORM_VR_VR_MATH_H__

// Small vector and quaternion helpers for the VR code. Header-only and written
// in C++98 style so the game sources (which build as gnu++98) can use them too.
#include "vr_state.h"
#include <cmath>

namespace vr {

const float kVrPi = 3.14159265358979323846f;
const float kRadToDeg = 180.0f / kVrPi;
const float kDegToRad = kVrPi / 180.0f;

inline Vec3 makeVec(float x, float y, float z) {
    Vec3 v;
    v.x = x; v.y = y; v.z = z;
    return v;
}

inline Vec3 add(const Vec3& a, const Vec3& b) { return makeVec(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vec3 sub(const Vec3& a, const Vec3& b) { return makeVec(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vec3 scale(const Vec3& a, float s) { return makeVec(a.x * s, a.y * s, a.z * s); }
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Quat makeQuat(float x, float y, float z, float w) {
    Quat q;
    q.x = x; q.y = y; q.z = z; q.w = w;
    return q;
}

inline Quat conjugate(const Quat& q) { return makeQuat(-q.x, -q.y, -q.z, q.w); }

// Rotates v by the unit quaternion q.
inline Vec3 rotate(const Quat& q, const Vec3& v) {
    Vec3 u = makeVec(q.x, q.y, q.z);
    Vec3 t = scale(makeVec(u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x), 2.0f);
    Vec3 c = makeVec(u.y * t.z - u.z * t.y, u.z * t.x - u.x * t.z, u.x * t.y - u.y * t.x);
    return add(add(v, scale(t, q.w)), c);
}

// Direction the controller or head points along (-Z in OpenXR).
inline Vec3 forward(const Quat& q) { return rotate(q, makeVec(0, 0, -1)); }

// Rotation about +Y by degrees, applied to a vector (same sense as Minecraft's yaw).
inline Vec3 rotateYaw(const Vec3& v, float degrees) {
    float b = degrees * kDegToRad;
    float c = std::cos(b), s = std::sin(b);
    return makeVec(v.x * c - v.z * s, v.y, v.z * c + v.x * s);
}

// Minecraft's yaw (yRot) that faces the given horizontal direction.
// Forward in game space is (-sin(yRot), *, cos(yRot)).
inline float yawOf(const Vec3& f) { return std::atan2(-f.x, f.z) * kRadToDeg; }

// Minecraft's pitch (xRot) that faces the given direction; positive looks down.
inline float pitchOf(const Vec3& f) {
    float len = std::sqrt(f.x * f.x + f.y * f.y + f.z * f.z);
    if (len <= 0.0f) return 0.0f;
    float s = -f.y / len;
    if (s > 1.0f) s = 1.0f;
    if (s < -1.0f) s = -1.0f;
    return std::asin(s) * kRadToDeg;
}

}  // namespace vr

#endif /*PLATFORM_VR_VR_MATH_H__*/
