#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "lightmap_packer.hpp"
#include "math_types.hpp"

namespace idtech {

// Common runtime structures shared by the supported BSP formats.
struct Lump {
    int32_t fileofs;
    int32_t filelen;
};

struct Plane {
    float3 normal;
    float dist;
    int32_t type;
};

struct Node {
    int32_t planenum;
    int16_t children[2];
    int16_t mins[3];
    int16_t maxs[3];
    uint16_t firstface;
    uint16_t numfaces;
};

struct Bounds {
    float3 minimum;
    float3 maximum;
};

struct BSPLeaf {
    int32_t contents = 0;
    int32_t visibilityOffset = -1;
    Bounds bounds;
    uint32_t firstMarkSurface = 0;
    uint32_t markSurfaceCount = 0;
};

struct BSPModel {
    Bounds bounds;
    float3 origin;
    int32_t headNodes[4]{};
    int32_t visibleLeafCount = 0;
    uint32_t firstFace = 0;
    uint32_t faceCount = 0;
};

struct Edge {
    uint16_t v[2];
};

struct Vertex {
    float3 position;
};

// For rendering
struct VertexP3N3T2T2 {
    float3 position;
    float3 normal;
    float2 texCoord0;
    float2 texCoord1;
};

struct FaceInfo {
    uint32_t firstVertex;
    uint32_t vertexCount;
    uint32_t firstIndex;
    uint32_t indexCount;
    uint32_t materialId;
    int32_t lightmapIndex;
    bool visible;
};

struct Material {
    std::string name;
    std::string externalName;
    int textureIndex = -1;
    bool translucent = false;
    bool masked = false;
    bool renderable = true;
    float alpha = 1.0f;
    CpuTexture albedo;
};

struct BSPScene {
    int bspVersion = 0;
    std::string entities;

    std::vector<VertexP3N3T2T2> vertices;
    std::vector<uint32_t> indices;
    std::vector<FaceInfo> faces;
    std::vector<Material> materials;

    std::vector<Node> nodes;
    std::vector<Plane> planes;
    std::vector<BSPLeaf> leafs;
    std::vector<BSPModel> models;
    std::vector<uint32_t> markSurfaces;

    std::vector<uint8_t> visibility;
    std::vector<uint8_t> lightmapData;
    LightmapAtlas lightmapAtlas;
};

} // namespace idtech
