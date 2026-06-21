#include "bsp_quake1.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <string_view>
#include <type_traits>

namespace idtech::quake1 {
namespace {

template <typename T>
bool readLump(
    const std::vector<uint8_t>& data,
    const Header& header,
    LumpIndex index,
    const T*& items,
    std::size_t& count)
{
    static_assert(std::is_trivially_copyable_v<T>);

    const auto& lump = header.lumps[index];
    if (lump.fileofs < 0 || lump.filelen < 0) {
        return false;
    }

    const auto offset = static_cast<std::size_t>(lump.fileofs);
    const auto length = static_cast<std::size_t>(lump.filelen);
    if (offset > data.size() || length > data.size() - offset) {
        return false;
    }
    if (length % sizeof(T) != 0) {
        return false;
    }

    items = reinterpret_cast<const T*>(data.data() + offset);
    count = length / sizeof(T);
    return true;
}

bool copyByteLump(
    const std::vector<uint8_t>& data,
    const Header& header,
    LumpIndex index,
    std::vector<uint8_t>& output)
{
    const auto& lump = header.lumps[index];
    if (lump.fileofs < 0 || lump.filelen < 0) {
        return false;
    }

    const auto offset = static_cast<std::size_t>(lump.fileofs);
    const auto length = static_cast<std::size_t>(lump.filelen);
    if (offset > data.size() || length > data.size() - offset) {
        return false;
    }

    output.assign(data.begin() + offset, data.begin() + offset + length);
    return true;
}

bool parseMaterials(
    const std::vector<uint8_t>& textureData,
    std::vector<Material>& materials)
{
    const auto isToolMaterial = [](std::string_view name) {
        std::string lower(name);
        std::transform(
            lower.begin(),
            lower.end(),
            lower.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
        return lower == "aaatrigger" ||
            lower == "clip" ||
            lower == "skip" ||
            lower == "hint" ||
            lower == "origin" ||
            lower == "null" ||
            lower == "bevel";
    };

    materials.clear();
    if (textureData.empty()) {
        return true;
    }
    if (textureData.size() < sizeof(int32_t)) {
        return false;
    }

    int32_t textureCount = 0;
    std::memcpy(&textureCount, textureData.data(), sizeof(textureCount));
    if (textureCount < 0) {
        return false;
    }

    const auto count = static_cast<std::size_t>(textureCount);
    if (count > (textureData.size() - sizeof(int32_t)) / sizeof(int32_t)) {
        return false;
    }

    materials.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        int32_t textureOffset = -1;
        std::memcpy(
            &textureOffset,
            textureData.data() + sizeof(int32_t) + i * sizeof(int32_t),
            sizeof(textureOffset));

        auto& material = materials[i];
        material.textureIndex = static_cast<int>(i);
        if (textureOffset < 0) {
            material.name = "<external>";
            continue;
        }

        const auto offset = static_cast<std::size_t>(textureOffset);
        if (offset > textureData.size() ||
            sizeof(DiskMipTexture) > textureData.size() - offset) {
            return false;
        }

        DiskMipTexture texture{};
        std::memcpy(&texture, textureData.data() + offset, sizeof(texture));
        const auto nameLength =
            std::find(std::begin(texture.name), std::end(texture.name), '\0') -
            std::begin(texture.name);
        material.name.assign(texture.name, nameLength);
        if (texture.offsets[0] != 0 &&
            texture.offsets[0] < textureData.size() - offset) {
            const auto* externalName = reinterpret_cast<const char*>(
                textureData.data() + offset + texture.offsets[0]);
            const std::size_t capacity =
                textureData.size() - offset - texture.offsets[0];
            const auto* externalEnd = std::find(
                externalName,
                externalName + capacity,
                '\0');
            material.externalName.assign(externalName, externalEnd);
        }
        material.translucent =
            !material.name.empty() && material.name.front() == '*';
        material.renderable = !isToolMaterial(material.name);
        material.alpha = material.translucent ? 0.75f : 1.0f;
    }

    return true;
}

float3 faceNormal(
    const DiskFace& face,
    const DiskPlane* planes,
    std::size_t planeCount)
{
    if (face.planenum >= planeCount) {
        return {};
    }

    const auto& plane = planes[face.planenum];
    const float sign = face.side == 0 ? 1.0f : -1.0f;
    return {
        plane.normal[0] * sign,
        plane.normal[1] * sign,
        plane.normal[2] * sign
    };
}

} // namespace

bool load(const std::vector<uint8_t>& data, BSPScene& scene) {
    if (data.size() < sizeof(Header)) {
        return false;
    }

    Header header{};
    std::memcpy(&header, data.data(), sizeof(header));
    if (header.version != 29 && header.version != 30) {
        return false;
    }

    const DiskVertex* diskVertices = nullptr;
    const DiskPlane* diskPlanes = nullptr;
    const DiskNode* diskNodes = nullptr;
    const DiskFace* diskFaces = nullptr;
    const DiskEdge* diskEdges = nullptr;
    const int32_t* surfEdges = nullptr;
    const DiskTexinfo* texinfo = nullptr;
    const DiskLeaf* diskLeafs = nullptr;
    const DiskModel* diskModels = nullptr;
    const uint16_t* diskMarkSurfaces = nullptr;

    std::size_t vertexCount = 0;
    std::size_t planeCount = 0;
    std::size_t nodeCount = 0;
    std::size_t faceCount = 0;
    std::size_t edgeCount = 0;
    std::size_t surfEdgeCount = 0;
    std::size_t texinfoCount = 0;
    std::size_t leafCount = 0;
    std::size_t modelCount = 0;
    std::size_t markSurfaceCount = 0;

    if (!readLump(data, header, kVertices, diskVertices, vertexCount) ||
        !readLump(data, header, kPlanes, diskPlanes, planeCount) ||
        !readLump(data, header, kNodes, diskNodes, nodeCount) ||
        !readLump(data, header, kFaces, diskFaces, faceCount) ||
        !readLump(data, header, kEdges, diskEdges, edgeCount) ||
        !readLump(data, header, kSurfedges, surfEdges, surfEdgeCount) ||
        !readLump(data, header, kTexinfo, texinfo, texinfoCount) ||
        !readLump(data, header, kLeafs, diskLeafs, leafCount) ||
        !readLump(data, header, kModels, diskModels, modelCount) ||
        !readLump(
            data,
            header,
            kMarkSurfaces,
            diskMarkSurfaces,
            markSurfaceCount)) {
        return false;
    }

    std::vector<uint8_t> entities;
    std::vector<uint8_t> textureData;
    if (!copyByteLump(data, header, kEntities, entities) ||
        !copyByteLump(data, header, kTextures, textureData) ||
        !copyByteLump(data, header, kVisibility, scene.visibility) ||
        !copyByteLump(data, header, kLightmaps, scene.lightmapData)) {
        return false;
    }
    if (!parseMaterials(textureData, scene.materials)) {
        return false;
    }

    scene.bspVersion = header.version;
    scene.entities.assign(
        reinterpret_cast<const char*>(entities.data()),
        entities.size());
    while (!scene.entities.empty() && scene.entities.back() == '\0') {
        scene.entities.pop_back();
    }

    scene.planes.resize(planeCount);
    for (std::size_t i = 0; i < planeCount; ++i) {
        scene.planes[i] = {
            {diskPlanes[i].normal[0], diskPlanes[i].normal[1], diskPlanes[i].normal[2]},
            diskPlanes[i].dist,
            diskPlanes[i].type
        };
    }

    scene.nodes.resize(nodeCount);
    for (std::size_t i = 0; i < nodeCount; ++i) {
        const auto& source = diskNodes[i];
        auto& destination = scene.nodes[i];
        destination.planenum = source.planenum;
        std::copy_n(source.children, 2, destination.children);
        std::copy_n(source.mins, 3, destination.mins);
        std::copy_n(source.maxs, 3, destination.maxs);
        destination.firstface = source.firstface;
        destination.numfaces = source.numfaces;
    }

    scene.leafs.resize(leafCount);
    for (std::size_t i = 0; i < leafCount; ++i) {
        const auto& source = diskLeafs[i];
        if (source.firstmarksurface > markSurfaceCount ||
            source.nummarksurfaces >
                markSurfaceCount - source.firstmarksurface) {
            return false;
        }

        scene.leafs[i] = {
            source.contents,
            source.visofs,
            {
                {
                    static_cast<float>(source.mins[0]),
                    static_cast<float>(source.mins[1]),
                    static_cast<float>(source.mins[2])
                },
                {
                    static_cast<float>(source.maxs[0]),
                    static_cast<float>(source.maxs[1]),
                    static_cast<float>(source.maxs[2])
                }
            },
            source.firstmarksurface,
            source.nummarksurfaces
        };
    }

    scene.models.resize(modelCount);
    for (std::size_t i = 0; i < modelCount; ++i) {
        const auto& source = diskModels[i];
        auto& destination = scene.models[i];
        destination.bounds = {
            {source.mins[0], source.mins[1], source.mins[2]},
            {source.maxs[0], source.maxs[1], source.maxs[2]}
        };
        destination.origin = {
            source.origin[0],
            source.origin[1],
            source.origin[2]
        };
        std::copy_n(source.headnode, kMaxMapHulls, destination.headNodes);
        destination.visibleLeafCount = source.visleafs;
        if (source.firstface < 0 || source.numfaces < 0) {
            return false;
        }
        destination.firstFace = static_cast<uint32_t>(source.firstface);
        destination.faceCount = static_cast<uint32_t>(source.numfaces);
    }

    scene.markSurfaces.assign(
        diskMarkSurfaces,
        diskMarkSurfaces + markSurfaceCount);
    for (const uint32_t faceIndex : scene.markSurfaces) {
        if (faceIndex >= faceCount) {
            return false;
        }
    }

    scene.vertices.clear();
    scene.indices.clear();
    scene.faces.clear();
    scene.faces.reserve(faceCount);

    struct FaceLightmap {
        int32_t lightmapIndex = -1;
        float minimumS = 0.0f;
        float minimumT = 0.0f;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    std::vector<FaceLightmap> faceLightmaps;
    faceLightmaps.reserve(faceCount);
    std::vector<std::vector<uint8_t>> lightmapPixels;
    const uint32_t lightmapChannels = header.version >= 30 ? 3u : 1u;

    for (std::size_t faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
        const auto& face = diskFaces[faceIndex];
        if (face.numedges < 3 || face.texinfo < 0 ||
            static_cast<std::size_t>(face.texinfo) >= texinfoCount ||
            face.firstedge < 0) {
            return false;
        }

        const auto firstEdge = static_cast<std::size_t>(face.firstedge);
        const auto faceEdgeCount = static_cast<std::size_t>(face.numedges);
        if (firstEdge > surfEdgeCount || faceEdgeCount > surfEdgeCount - firstEdge) {
            return false;
        }

        if (scene.vertices.size() >
            std::numeric_limits<uint32_t>::max() - faceEdgeCount) {
            return false;
        }

        const uint32_t firstVertex = static_cast<uint32_t>(scene.vertices.size());
        const auto& texture = texinfo[face.texinfo];
        const float3 normal = faceNormal(face, diskPlanes, planeCount);
        float minimumU = std::numeric_limits<float>::max();
        float minimumV = std::numeric_limits<float>::max();
        float maximumU = std::numeric_limits<float>::lowest();
        float maximumV = std::numeric_limits<float>::lowest();

        for (std::size_t edgeOffset = 0; edgeOffset < faceEdgeCount; ++edgeOffset) {
            const int32_t signedEdge = surfEdges[firstEdge + edgeOffset];
            const int64_t absoluteEdge =
                signedEdge < 0 ? -static_cast<int64_t>(signedEdge) : signedEdge;
            if (absoluteEdge < 0 || static_cast<std::size_t>(absoluteEdge) >= edgeCount) {
                return false;
            }

            const auto& edge = diskEdges[absoluteEdge];
            const uint16_t vertexIndex =
                signedEdge < 0 ? edge.vertices[1] : edge.vertices[0];
            if (vertexIndex >= vertexCount) {
                return false;
            }

            const auto& diskVertex = diskVertices[vertexIndex];
            const float3 position{
                diskVertex.point[0],
                diskVertex.point[1],
                diskVertex.point[2]
            };
            const float u =
                position.x * texture.vecs[0][0] +
                position.y * texture.vecs[0][1] +
                position.z * texture.vecs[0][2] +
                texture.vecs[0][3];
            const float v =
                position.x * texture.vecs[1][0] +
                position.y * texture.vecs[1][1] +
                position.z * texture.vecs[1][2] +
                texture.vecs[1][3];
            minimumU = std::min(minimumU, u);
            minimumV = std::min(minimumV, v);
            maximumU = std::max(maximumU, u);
            maximumV = std::max(maximumV, v);

            scene.vertices.push_back({
                position,
                normal,
                {u, v},
                {0.0f, 0.0f}
            });
        }

        const uint32_t firstIndex = static_cast<uint32_t>(scene.indices.size());
        for (uint32_t i = 1; i + 1 < faceEdgeCount; ++i) {
            scene.indices.push_back(firstVertex);
            scene.indices.push_back(firstVertex + i);
            scene.indices.push_back(firstVertex + i + 1);
        }

        FaceLightmap faceLightmap;
        if (face.lightofs >= 0 && face.styles[0] != 0xff) {
            const float minimumS = std::floor(minimumU / 16.0f);
            const float minimumT = std::floor(minimumV / 16.0f);
            const float maximumS = std::ceil(maximumU / 16.0f);
            const float maximumT = std::ceil(maximumV / 16.0f);
            const uint32_t width =
                static_cast<uint32_t>(maximumS - minimumS) + 1;
            const uint32_t height =
                static_cast<uint32_t>(maximumT - minimumT) + 1;
            const uint64_t byteCount =
                static_cast<uint64_t>(width) * height * lightmapChannels;
            const auto lightOffset = static_cast<std::size_t>(face.lightofs);
            if (byteCount > std::numeric_limits<std::size_t>::max() ||
                lightOffset > scene.lightmapData.size() ||
                byteCount > scene.lightmapData.size() - lightOffset) {
                return false;
            }

            faceLightmap.lightmapIndex =
                static_cast<int32_t>(lightmapPixels.size());
            faceLightmap.minimumS = minimumS;
            faceLightmap.minimumT = minimumT;
            faceLightmap.width = width;
            faceLightmap.height = height;
            lightmapPixels.emplace_back(
                scene.lightmapData.begin() + lightOffset,
                scene.lightmapData.begin() + lightOffset + byteCount);
        }

        scene.faces.push_back({
            firstVertex,
            static_cast<uint32_t>(faceEdgeCount),
            firstIndex,
            static_cast<uint32_t>((faceEdgeCount - 2) * 3),
            static_cast<uint32_t>(texture.miptex < 0 ? 0 : texture.miptex),
            faceLightmap.lightmapIndex,
            true
        });
        faceLightmaps.push_back(faceLightmap);
    }

    std::vector<LightmapImage> lightmapImages;
    lightmapImages.reserve(lightmapPixels.size());
    for (std::size_t faceIndex = 0; faceIndex < scene.faces.size(); ++faceIndex) {
        const int32_t lightmapIndex = scene.faces[faceIndex].lightmapIndex;
        if (lightmapIndex < 0) {
            continue;
        }

        const auto& pixels = lightmapPixels[lightmapIndex];
        const auto& faceLightmap = faceLightmaps[faceIndex];
        lightmapImages.push_back({
            static_cast<uint32_t>(faceIndex),
            faceLightmap.width,
            faceLightmap.height,
            lightmapChannels,
            pixels
        });
    }

    LightmapPacker packer;
    if (!packer.pack(lightmapImages, scene.lightmapAtlas)) {
        return false;
    }

    for (const auto& placement : scene.lightmapAtlas.placements) {
        if (placement.id >= scene.faces.size()) {
            return false;
        }

        const auto& face = scene.faces[placement.id];
        const auto& faceLightmap = faceLightmaps[placement.id];
        for (uint32_t i = 0; i < face.vertexCount; ++i) {
            auto& vertex = scene.vertices[face.firstVertex + i];
            const float localS =
                vertex.texCoord0.x / 16.0f - faceLightmap.minimumS;
            const float localT =
                vertex.texCoord0.y / 16.0f - faceLightmap.minimumT;
            vertex.texCoord1 = {
                (placement.x + localS + 0.5f) /
                    scene.lightmapAtlas.texture.width,
                (placement.y + localT + 0.5f) /
                    scene.lightmapAtlas.texture.height
            };
        }
    }

    return true;
}

} // namespace idtech::quake1
