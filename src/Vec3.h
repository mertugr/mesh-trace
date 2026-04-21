#pragma once

#include <cmath>

struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    explicit Vec3(float v) : x(v), y(v), z(v) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    Vec3 operator-(const Vec3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator*(const Vec3& v) const { return {x * v.x, y * v.y, z * v.z}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }

    Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vec3& operator-=(const Vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    float operator[](int i) const { return (&x)[i]; }
    float& operator[](int i) { return (&x)[i]; }

    float dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
    Vec3 cross(const Vec3& v) const {
        return {y * v.z - z * v.y,
                z * v.x - x * v.z,
                x * v.y - y * v.x};
    }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    float lengthSquared() const { return x * x + y * y + z * z; }
    Vec3 normalized() const {
        float l = length();
        return l > 0.0f ? *this / l : *this;
    }
    Vec3 cwiseMin(const Vec3& v) const { return {std::fmin(x, v.x), std::fmin(y, v.y), std::fmin(z, v.z)}; }
    Vec3 cwiseMax(const Vec3& v) const { return {std::fmax(x, v.x), std::fmax(y, v.y), std::fmax(z, v.z)}; }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }
