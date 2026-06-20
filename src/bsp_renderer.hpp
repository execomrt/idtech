#pragma once

#include "bsp_explorer.hpp"
#include "bsp_loader.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace idtech {

struct TextureHandle {
    uint64_t id = 0;

    bool valid() const { return id != 0; }
    void reset() { id = 0; }
};

struct BufferHandle {
    uint64_t id = 0;

    bool valid() const { return id != 0; }
    void reset() { id = 0; }
};

enum class BufferUsage : uint8_t {
    Vertex,
    Index
};

struct TextureDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipCount = 1;
    uint32_t channels = 0;
    const void* data = nullptr;
    const char* label = nullptr;
};

struct BufferDesc {
    std::size_t size = 0;
    BufferUsage usage = BufferUsage::Vertex;
    const void* data = nullptr;
    const char* label = nullptr;
};

struct BSPDrawBatch {
    TextureHandle albedoTexture;
    TextureHandle lightmapTexture;
    BufferHandle vertexBuffer;
    BufferHandle indexBuffer;
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
    bool translucent = false;
};

class BSPRenderer {
public:
    struct Settings {
        bool drawTranslucent = true;
        bool usePotentiallyVisibleSet = true;
        bool allVisibleOnMissingPVS = true;
        bool includeSolidLeaf = false;
    };

    struct ResourceCallbacks {
        std::function<TextureHandle(const TextureDesc&)> createTexture;
        std::function<void(TextureHandle)> destroyTexture;
        std::function<BufferHandle(const BufferDesc&)> createBuffer;
        std::function<void(BufferHandle)> destroyBuffer;
    };

    struct Callbacks {
        std::function<void(const BSPDrawBatch&)> drawPrimitive;
        std::function<bool(const Bounds&)> leafInFrustum;
    };

    struct Config {
        Settings settings;
        ResourceCallbacks resourceCallbacks;
        Callbacks callbacks;
    };

    BSPRenderer() = default;
    ~BSPRenderer();

    BSPRenderer(const BSPRenderer&) = delete;
    BSPRenderer& operator=(const BSPRenderer&) = delete;

    void setConfig(const Config& config);
    bool loadScene(const std::vector<uint8_t>& bspData);

    bool createResources();
    void destroyResources();
    void render() const;
    bool explore(const float3& cameraPosition);
    bool renderVisible(const float3& cameraPosition);

    const BSPScene& getScene() const { return m_loader.getScene(); }
    BSPLoader& getLoader() { return m_loader; }
    const std::vector<BSPDrawBatch>& getDrawBatches() const {
        return m_drawBatches;
    }
    const std::vector<BSPDrawBatch>& getVisibleDrawBatches() const {
        return m_visibleDrawBatches;
    }
    const BSPExplorer::Result& getExplorationResult() const {
        return m_explorationResult;
    }

private:
    bool createBuffers();
    bool createTextures();
    void buildDrawBatches();
    void buildVisibleDrawBatches();
    void assignResourceHandles(std::vector<BSPDrawBatch>& batches);
    void renderBatches(const std::vector<BSPDrawBatch>& batches) const;

    BSPLoader m_loader;
    Config m_config;

    BufferHandle m_vertexBuffer;
    BufferHandle m_indexBuffer;
    TextureHandle m_lightmapTexture;
    std::vector<TextureHandle> m_albedoTextures;
    std::vector<uint32_t> m_renderIndices;
    std::vector<uint32_t> m_faceFirstIndices;
    std::vector<BSPDrawBatch> m_drawBatches;
    std::vector<BSPDrawBatch> m_visibleDrawBatches;

    BSPExplorer m_explorer;
    BSPExplorer::Result m_explorationResult;
};

} // namespace idtech
