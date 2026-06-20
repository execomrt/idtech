#pragma once
#include <cstdint>
#include <cmath>
#include <array>

namespace idtech {

struct float2 {
    float x, y;
    float2() : x(0), y(0) {}
    float2(float _x, float _y) : x(_x), y(_y) {}
};

struct float3 {
    float x, y, z;
    float3() : x(0), y(0), z(0) {}
    float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    
    float3 operator+(const float3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    float3 operator-(const float3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    float3 operator*(float s) const { return {x*s, y*s, z*s}; }
    float3& operator+=(const float3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
    float3& operator-=(const float3& o) { x-=o.x; y-=o.y; z-=o.z; return *this; }
    float3& operator*=(float s) { x*=s; y*=s; z*=s; return *this; }
    
    float length() const { return std::sqrt(x*x + y*y + z*z); }
    float3 normalized() const { float l = length(); return l > 0 ? *this * (1.0f/l) : *this; }
    static float dot(const float3& a, const float3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
    static float3 cross(const float3& a, const float3& b) {
        return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
    }
};

struct float4 {
    float x, y, z, w;
    float4() : x(0), y(0), z(0), w(0) {}
    float4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
};

struct float4x4 {
    float m[16]; // row-major
    
    float4x4() { for(int i=0;i<16;i++) m[i]=0; }
    
    static float4x4 identity() {
        float4x4 r;
        r.m[0]=r.m[5]=r.m[10]=r.m[15]=1.0f;
        return r;
    }
    
    float4x4 transpose() const {
        float4x4 r;
        for(int i=0;i<4;i++)
            for(int j=0;j<4;j++)
                r.m[i*4+j] = m[j*4+i];
        return r;
    }
    
    float4 operator*(const float4& v) const {
        float4 r;
        r.x = m[0]*v.x + m[1]*v.y + m[2]*v.z + m[3]*v.w;
        r.y = m[4]*v.x + m[5]*v.y + m[6]*v.z + m[7]*v.w;
        r.z = m[8]*v.x + m[9]*v.y + m[10]*v.z + m[11]*v.w;
        r.w = m[12]*v.x + m[13]*v.y + m[14]*v.z + m[15]*v.w;
        return r;
    }
};

} // namespace idtech
