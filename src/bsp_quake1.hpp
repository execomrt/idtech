#pragma once
#include <cstdint>
#include <vector>
#include "bsp_types.hpp"

namespace idtech::quake1 {

inline constexpr int kMaxMipLevels = 4;
inline constexpr int kMaxLightmaps = 4;
inline constexpr int kNumAmbients = 4;
inline constexpr int kMaxMapHulls = 4;

// Quake 1 lump indices
enum LumpIndex {
    kEntities = 0,
    kPlanes = 1,
    kTextures = 2,
    kVertices = 3,
    kVisibility = 4,
    kNodes = 5,
    kTexinfo = 6,
    kFaces = 7,
    kLightmaps = 8,
    kClipNodes = 9,
    kLeafs = 10,
    kMarkSurfaces = 11,
    kEdges = 12,
    kSurfedges = 13,
    kModels = 14,
    kCount = 15
};

struct Header {
    int32_t version;
    idtech::Lump lumps[kCount];
};

struct DiskPlane {
    float normal[3];
    float dist;
    int32_t type;
};

struct DiskNode {
    int32_t planenum;
    int16_t children[2];
    int16_t mins[3];
    int16_t maxs[3];
    uint16_t firstface;
    uint16_t numfaces;
};

struct DiskEdge {
    uint16_t vertices[2];
};

struct DiskVertex {
    float point[3];
};

struct DiskTexinfo {
    float vecs[2][4];
    int32_t miptex;
    int32_t flags;
};

struct DiskFace {
    uint16_t planenum;
    int16_t side;
    int32_t firstedge;
    int16_t numedges;
    int16_t texinfo;
    uint8_t styles[kMaxLightmaps];
    int32_t lightofs;
};

struct DiskLeaf {
    int32_t contents;
    int32_t visofs;
    int16_t mins[3];
    int16_t maxs[3];
    uint16_t firstmarksurface;
    uint16_t nummarksurfaces;
    uint8_t ambient_level[kNumAmbients];
};

struct DiskModel {
    float mins[3];
    float maxs[3];
    float origin[3];
    int32_t headnode[kMaxMapHulls];
    int32_t visleafs;
    int32_t firstface;
    int32_t numfaces;
};

struct DiskClipnode {
    int32_t planenum;
    int16_t children[2];
};

struct DiskMipTexture {
    char name[16];
    uint32_t width;
    uint32_t height;
    uint32_t offsets[kMaxMipLevels];
};

bool load(const std::vector<uint8_t>& data, BSPScene& scene);

} // namespace idtech::quake1
