#pragma once

#include <algorithm>
#include <cmath>

namespace rt {

struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    Vec3() = default;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
    Vec3 operator-(const Vec3& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
    Vec3 operator*(const Vec3& rhs) const { return {x * rhs.x, y * rhs.y, z * rhs.z}; }

    Vec3& operator+=(const Vec3& rhs) {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    Vec3& operator*=(double s) {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }

    double length() const { return std::sqrt(x * x + y * y + z * z); }
    double lengthSquared() const { return x * x + y * y + z * z; }

    Vec3 normalized() const {
        const double len = length();
        if (len == 0.0) {
            return {0.0, 0.0, 0.0};
        }
        return *this / len;
    }

    static double dot(const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static Vec3 cross(const Vec3& a, const Vec3& b) {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x,
        };
    }

    static Vec3 clamp(const Vec3& v, double lo, double hi) {
        return {
            std::clamp(v.x, lo, hi),
            std::clamp(v.y, lo, hi),
            std::clamp(v.z, lo, hi),
        };
    }
};

inline Vec3 operator*(double s, const Vec3& v) {
    return v * s;
}

inline Vec3 reflect(const Vec3& incident, const Vec3& normal) {
    return incident - normal * (2.0 * Vec3::dot(incident, normal));
}

} // namespace rt
