#include "bsp_renderer.hpp"

#include <cstdint>
#include <vector>

std::vector<uint8_t> loadBSPFile(const char* filename);

int main() {
    idtech::BSPRenderer renderer;
    idtech::BSPRenderer::Config config;

    config.resourceCallbacks.createTexture =
        [](const idtech::TextureDesc&) {
            return idtech::TextureHandle{123};
        };
    config.resourceCallbacks.destroyTexture =
        [](idtech::TextureHandle) {};
    config.resourceCallbacks.createBuffer =
        [](const idtech::BufferDesc&) {
            return idtech::BufferHandle{456};
        };
    config.resourceCallbacks.destroyBuffer =
        [](idtech::BufferHandle) {};

    config.callbacks.drawPrimitive =
        [](const idtech::BSPDrawBatch& batch) {
            // Bind batch.vertexBuffer and batch.indexBuffer.
            // Bind batch.albedoTexture and batch.lightmapTexture.
            // Draw batch.indexCount indices starting at batch.firstIndex.
        };

    renderer.setConfig(config);

    const auto bspData = loadBSPFile("map.bsp");
    if (renderer.loadScene(bspData) && renderer.createResources()) {
        renderer.render();
    }

    return 0;
}
