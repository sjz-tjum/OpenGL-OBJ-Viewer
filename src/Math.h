#pragma once

#include <cmath>

namespace og {

constexpr float kPi = 3.14159265358979323846f;

inline float radians(float degrees) { return degrees * (kPi / 180.0f); }
inline float degrees(float radians) { return radians * (180.0f / kPi); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
};

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(const Vec3& v) {
    const float len = length(v);
    return len > 1e-8f ? v / len : Vec3(0.0f, 0.0f, 0.0f);
}

struct Mat4 {
    float m[16] = {};

    static Mat4 identity() {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    float& at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.at(row, k) * b.at(k, c);
            }
            r.at(row, c) = sum;
        }
    }
    return r;
}

inline Vec3 transformPoint(const Mat4& m, const Vec3& p) {
    const float x = m.at(0, 0) * p.x + m.at(0, 1) * p.y + m.at(0, 2) * p.z + m.at(0, 3);
    const float y = m.at(1, 0) * p.x + m.at(1, 1) * p.y + m.at(1, 2) * p.z + m.at(1, 3);
    const float z = m.at(2, 0) * p.x + m.at(2, 1) * p.y + m.at(2, 2) * p.z + m.at(2, 3);
    return {x, y, z};
}

inline Mat4 translation(const Vec3& t) {
    Mat4 r = Mat4::identity();
    r.at(0, 3) = t.x;
    r.at(1, 3) = t.y;
    r.at(2, 3) = t.z;
    return r;
}

inline Mat4 scaling(const Vec3& s) {
    Mat4 r = Mat4::identity();
    r.at(0, 0) = s.x;
    r.at(1, 1) = s.y;
    r.at(2, 2) = s.z;
    return r;
}

inline Mat4 rotationX(float a) {
    Mat4 r = Mat4::identity();
    const float c = std::cos(a);
    const float s = std::sin(a);
    r.at(1, 1) = c; r.at(1, 2) = -s;
    r.at(2, 1) = s; r.at(2, 2) = c;
    return r;
}

inline Mat4 rotationY(float a) {
    Mat4 r = Mat4::identity();
    const float c = std::cos(a);
    const float s = std::sin(a);
    r.at(0, 0) = c;  r.at(0, 2) = s;
    r.at(2, 0) = -s; r.at(2, 2) = c;
    return r;
}

inline Mat4 rotationZ(float a) {
    Mat4 r = Mat4::identity();
    const float c = std::cos(a);
    const float s = std::sin(a);
    r.at(0, 0) = c; r.at(0, 1) = -s;
    r.at(1, 0) = s; r.at(1, 1) = c;
    return r;
}

inline Mat4 perspective(float fovyRadians, float aspect, float zNear, float zFar) {
    Mat4 r;
    const float f = 1.0f / std::tan(fovyRadians * 0.5f);
    r.at(0, 0) = f / aspect;
    r.at(1, 1) = f;
    r.at(2, 2) = (zFar + zNear) / (zNear - zFar);
    r.at(2, 3) = (2.0f * zFar * zNear) / (zNear - zFar);
    r.at(3, 2) = -1.0f;
    return r;
}

inline Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    const Vec3 f = normalize(target - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);
    Mat4 r = Mat4::identity();
    r.at(0, 0) = s.x;  r.at(0, 1) = s.y;  r.at(0, 2) = s.z;  r.at(0, 3) = -dot(s, eye);
    r.at(1, 0) = u.x;  r.at(1, 1) = u.y;  r.at(1, 2) = u.z;  r.at(1, 3) = -dot(u, eye);
    r.at(2, 0) = -f.x; r.at(2, 1) = -f.y; r.at(2, 2) = -f.z; r.at(2, 3) = dot(f, eye);
    return r;
}

inline void normalMatrix(const Mat4& model, float out[9]) {
    const float a00 = model.at(0, 0), a01 = model.at(0, 1), a02 = model.at(0, 2);
    const float a10 = model.at(1, 0), a11 = model.at(1, 1), a12 = model.at(1, 2);
    const float a20 = model.at(2, 0), a21 = model.at(2, 1), a22 = model.at(2, 2);
    const float det = a00 * (a11 * a22 - a12 * a21)
                    - a01 * (a10 * a22 - a12 * a20)
                    + a02 * (a10 * a21 - a11 * a20);
    if (std::fabs(det) < 1e-8f) {
        out[0] = 1.0f; out[1] = 0.0f; out[2] = 0.0f;
        out[3] = 0.0f; out[4] = 1.0f; out[5] = 0.0f;
        out[6] = 0.0f; out[7] = 0.0f; out[8] = 1.0f;
        return;
    }
    const float invDet = 1.0f / det;
    const float b00 = (a11 * a22 - a12 * a21) * invDet;
    const float b01 = (a02 * a21 - a01 * a22) * invDet;
    const float b02 = (a01 * a12 - a02 * a11) * invDet;
    const float b10 = (a12 * a20 - a10 * a22) * invDet;
    const float b11 = (a00 * a22 - a02 * a20) * invDet;
    const float b12 = (a02 * a10 - a00 * a12) * invDet;
    const float b20 = (a10 * a21 - a11 * a20) * invDet;
    const float b21 = (a01 * a20 - a00 * a21) * invDet;
    const float b22 = (a00 * a11 - a01 * a10) * invDet;
    out[0] = b00; out[1] = b01; out[2] = b02;
    out[3] = b10; out[4] = b11; out[5] = b12;
    out[6] = b20; out[7] = b21; out[8] = b22;
}

} // namespace og
