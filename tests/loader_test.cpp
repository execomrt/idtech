#include "bsp_explorer.hpp"
#include "bsp_loader.hpp"
#include "bsp_renderer.hpp"
#include "wad.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }

    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    };
}

std::string findPlayerStart(const std::string& entities) {
    const auto classPosition = entities.find("\"classname\" \"info_player_start\"");
    if (classPosition == std::string::npos) {
        return {};
    }

    const auto entityBegin = entities.rfind('{', classPosition);
    const auto entityEnd = entities.find('}', classPosition);
    if (entityBegin == std::string::npos || entityEnd == std::string::npos) {
        return {};
    }

    const auto originKey = entities.find("\"origin\"", entityBegin);
    if (originKey == std::string::npos || originKey >= entityEnd) {
        return {};
    }

    const auto valueBegin = entities.find('"', originKey + 8);
    if (valueBegin == std::string::npos || valueBegin >= entityEnd) {
        return {};
    }
    const auto valueEnd = entities.find('"', valueBegin + 1);
    if (valueEnd == std::string::npos || valueEnd >= entityEnd) {
        return {};
    }

    return entities.substr(valueBegin + 1, valueEnd - valueBegin - 1);
}

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path bspPath =
        argc > 1
            ? std::filesystem::path(argv[1])
            : std::filesystem::path(IDTECH_TEST_ASSET_DIR) / "start.bsp";

    const auto bytes = readFile(bspPath);
    if (bytes.empty()) {
        std::cerr << "Unable to read BSP: " << bspPath << '\n';
        return 1;
    }

    idtech::BSPLoader loader;
    if (!loader.load(bytes)) {
        std::cerr << "Unable to parse BSP: " << bspPath << '\n';
        return 2;
    }

    const auto& scene = loader.getScene();
    std::cout
        << "Loaded: " << bspPath << '\n'
        << "File size: " << bytes.size() << " bytes\n"
        << "BSP version: " << scene.bspVersion << '\n'
        << "Faces: " << scene.faces.size() << '\n'
        << "Render vertices: " << scene.vertices.size() << '\n'
        << "Triangle indices: " << scene.indices.size() << '\n'
        << "Triangles: " << scene.indices.size() / 3 << '\n'
        << "Materials: " << scene.materials.size() << '\n'
        << "Planes: " << scene.planes.size() << '\n'
        << "Nodes: " << scene.nodes.size() << '\n'
        << "Leaves: " << scene.leafs.size() << '\n'
        << "Visibility data: " << scene.visibility.size() << " bytes\n"
        << "Lightmap data: " << scene.lightmapData.size() << " bytes\n"
        << "Packed lightmaps: " << scene.lightmapAtlas.placements.size() << '\n'
        << "Lightmap atlas: "
        << scene.lightmapAtlas.texture.width << 'x'
        << scene.lightmapAtlas.texture.height << " x "
        << scene.lightmapAtlas.texture.channels << " channels ("
        << scene.lightmapAtlas.texture.pixels.size() << " bytes)\n"
        << "Entity text: " << scene.entities.size() << " bytes\n";

    std::size_t windingMatchesNormal = 0;
    std::size_t windingOpposesNormal = 0;
    for (const auto& face : scene.faces) {
        if (face.vertexCount < 3) {
            continue;
        }
        const auto& a = scene.vertices[face.firstVertex + 0];
        const auto& b = scene.vertices[face.firstVertex + 1];
        const auto& c = scene.vertices[face.firstVertex + 2];
        const auto geometricNormal = idtech::float3::cross(
            b.position - a.position,
            c.position - a.position);
        if (idtech::float3::dot(geometricNormal, a.normal) >= 0.0f) {
            ++windingMatchesNormal;
        } else {
            ++windingOpposesNormal;
        }
    }
    std::cout
        << "Face winding vs normal: "
        << windingMatchesNormal << " matching, "
        << windingOpposesNormal << " opposing\n";

    for (std::size_t materialIndex = 0;
         materialIndex < scene.materials.size();
         ++materialIndex) {
        if (scene.materials[materialIndex].renderable) {
            continue;
        }
        std::size_t faceCount = 0;
        for (const auto& face : scene.faces) {
            faceCount += face.materialId == materialIndex ? 1 : 0;
        }
        std::cout
            << "Tool material skipped: "
            << scene.materials[materialIndex].name
            << " (" << faceCount << " faces)\n";
    }

    const auto playerStart = findPlayerStart(scene.entities);
    if (!playerStart.empty()) {
        std::cout << "info_player_start origin: " << playerStart << '\n';
    } else {
        std::cout << "info_player_start origin: not found\n";
    }

    idtech::float3 cameraPosition{};
    if (!playerStart.empty()) {
        std::istringstream input(playerStart);
        input >> cameraPosition.x >> cameraPosition.y >> cameraPosition.z;
    }

    idtech::BSPExplorer explorer;
    idtech::BSPExplorer::Config explorerConfig;
    explorerConfig.callbacks.leafInFrustum =
        [](const idtech::Bounds&) {
            return true;
        };
    explorer.setConfig(explorerConfig);

    idtech::BSPExplorer::Result exploration;
    if (!explorer.explore(scene, cameraPosition, exploration)) {
        std::cerr << "Unable to explore BSP visibility\n";
        return 3;
    }

    std::cout
        << "Camera leaf: " << exploration.cameraLeaf << '\n'
        << "PVS/frustum leaves: " << exploration.visibleLeaves.size() << '\n'
        << "PVS/frustum faces: " << exploration.visibleFaces.size() << '\n';

    explorerConfig.callbacks.leafInFrustum =
        [](const idtech::Bounds&) {
            return false;
        };
    explorer.setConfig(explorerConfig);
    idtech::BSPExplorer::Result rejectedExploration;
    if (!explorer.explore(scene, cameraPosition, rejectedExploration) ||
        !rejectedExploration.visibleFaces.empty()) {
        std::cerr << "Leaf frustum callback validation failed\n";
        return 4;
    }

    const std::filesystem::path wadPath =
        argc > 2
            ? std::filesystem::path(argv[2])
            : std::filesystem::path(IDTECH_TEST_ASSET_DIR) / "skins.wad";
    const auto wadBytes = readFile(wadPath);
    if (!wadBytes.empty()) {
        idtech::WadArchive wad;
        if (!wad.load(wadBytes)) {
            std::cerr << "Unable to parse WAD: " << wadPath << '\n';
            return 5;
        }

        std::size_t resolvedMaterials = 0;
        for (std::size_t materialIndex = 0;
             materialIndex < scene.materials.size();
             ++materialIndex) {
            const auto& material = scene.materials[materialIndex];
            const auto lookupName = material.externalName.empty()
                ? material.name
                : material.externalName;
            if (!lookupName.empty() && wad.find(lookupName) != nullptr) {
                ++resolvedMaterials;
            } else {
                std::size_t faceCount = 0;
                for (const auto& face : scene.faces) {
                    faceCount += face.materialId == materialIndex ? 1 : 0;
                }
                std::cout
                    << "  unresolved material[" << materialIndex << "]: "
                    << material.name;
                if (!material.externalName.empty()) {
                    std::cout << " -> " << material.externalName;
                }
                std::cout << " (" << faceCount << " faces)\n";
            }
        }

        std::cout
            << "WAD: " << wadPath << '\n'
            << "WAD entries: " << wad.entries().size() << '\n'
            << "BSP materials found in WAD: " << resolvedMaterials
            << '/' << scene.materials.size() << '\n';
    }

    uint64_t nextResourceId = 1;
    std::size_t createdTextures = 0;
    std::size_t destroyedTextures = 0;
    std::size_t createdBuffers = 0;
    std::size_t destroyedBuffers = 0;
    std::size_t drawCalls = 0;
    uint64_t drawnIndices = 0;

    idtech::BSPRenderer renderer;
    idtech::BSPRenderer::Config renderConfig;
    renderConfig.resourceCallbacks.createTexture =
        [&](const idtech::TextureDesc&) {
            ++createdTextures;
            return idtech::TextureHandle{nextResourceId++};
        };
    renderConfig.resourceCallbacks.destroyTexture =
        [&](idtech::TextureHandle) {
            ++destroyedTextures;
        };
    renderConfig.resourceCallbacks.createBuffer =
        [&](const idtech::BufferDesc&) {
            ++createdBuffers;
            return idtech::BufferHandle{nextResourceId++};
        };
    renderConfig.resourceCallbacks.destroyBuffer =
        [&](idtech::BufferHandle) {
            ++destroyedBuffers;
        };
    renderConfig.callbacks.drawPrimitive =
        [&](const idtech::BSPDrawBatch& batch) {
            if (!batch.vertexBuffer.valid() || !batch.indexBuffer.valid() ||
                !batch.lightmapTexture.valid()) {
                return;
            }
            ++drawCalls;
            drawnIndices += batch.indexCount;
        };

    renderer.setConfig(renderConfig);
    if (!renderer.loadScene(bytes) || !renderer.createResources()) {
        std::cerr << "Unable to create renderer resources\n";
        return 6;
    }
    renderer.render();

    std::cout
        << "Render batches: " << renderer.getDrawBatches().size() << '\n'
        << "Draw callbacks: " << drawCalls << '\n'
        << "Drawn indices: " << drawnIndices << '\n'
        << "Created textures: " << createdTextures << '\n'
        << "Created buffers: " << createdBuffers << '\n';

    uint64_t expectedRenderableIndices = 0;
    for (const auto& face : scene.faces) {
        if (face.materialId < scene.materials.size() &&
            scene.materials[face.materialId].renderable) {
            expectedRenderableIndices += face.indexCount;
        }
    }
    if (drawCalls != renderer.getDrawBatches().size() ||
        drawnIndices != expectedRenderableIndices) {
        std::cerr << "Renderer batch callback validation failed\n";
        return 7;
    }

    drawCalls = 0;
    drawnIndices = 0;
    uint64_t expectedVisibleIndices = 0;
    for (const uint32_t faceIndex : exploration.visibleFaces) {
        const auto& face = scene.faces[faceIndex];
        if (face.materialId < scene.materials.size() &&
            scene.materials[face.materialId].renderable) {
            expectedVisibleIndices += face.indexCount;
        }
    }
    if (!renderer.renderVisible(cameraPosition) ||
        drawCalls != renderer.getVisibleDrawBatches().size() ||
        drawnIndices != expectedVisibleIndices) {
        std::cerr << "Visible renderer batch validation failed\n";
        return 8;
    }

    std::cout
        << "Visible render batches: "
        << renderer.getVisibleDrawBatches().size() << '\n'
        << "Visible drawn indices: " << drawnIndices << '\n';

    renderer.destroyResources();
    if (destroyedTextures != createdTextures ||
        destroyedBuffers != createdBuffers) {
        std::cerr << "Renderer resource destruction validation failed\n";
        return 9;
    }

    return 0;
}
