#include "bsp_renderer.hpp"

#include <limits>
#include <string>

namespace idtech {

BSPRenderer::~BSPRenderer() {
    destroyResources();
}

void BSPRenderer::setConfig(const Config& config) {
    destroyResources();
    m_config = config;

    BSPExplorer::Config explorerConfig;
    explorerConfig.settings.usePotentiallyVisibleSet =
        config.settings.usePotentiallyVisibleSet;
    explorerConfig.settings.allVisibleOnMissingPVS =
        config.settings.allVisibleOnMissingPVS;
    explorerConfig.settings.includeSolidLeaf =
        config.settings.includeSolidLeaf;
    explorerConfig.callbacks.leafInFrustum =
        config.callbacks.leafInFrustum;
    m_explorer.setConfig(explorerConfig);
}

bool BSPRenderer::loadScene(const std::vector<uint8_t>& bspData) {
    destroyResources();
    return m_loader.load(bspData);
}

bool BSPRenderer::createResources() {
    destroyResources();

    const auto& scene = m_loader.getScene();
    if (scene.vertices.empty() || scene.indices.empty()) {
        return false;
    }
    if (!createTextures()) {
        destroyResources();
        return false;
    }

    buildDrawBatches();
    if (!createBuffers()) {
        destroyResources();
        return false;
    }

    assignResourceHandles(m_drawBatches);
    return true;
}

void BSPRenderer::assignResourceHandles(
    std::vector<BSPDrawBatch>& batches)
{
    for (auto& batch : batches) {
        batch.vertexBuffer = m_vertexBuffer;
        batch.indexBuffer = m_indexBuffer;
        batch.lightmapTexture = m_lightmapTexture;
        if (batch.materialIndex < m_albedoTextures.size()) {
            batch.albedoTexture = m_albedoTextures[batch.materialIndex];
        }
    }
}

bool BSPRenderer::createTextures() {
    const auto& callback = m_config.resourceCallbacks.createTexture;
    if (!callback) {
        return false;
    }

    const auto& scene = m_loader.getScene();
    if (!scene.lightmapAtlas.texture.empty()) {
        const auto& texture = scene.lightmapAtlas.texture;
        const TextureDesc desc{
            texture.width,
            texture.height,
            1,
            texture.channels,
            texture.pixels.data(),
            "idtech_lightmap_atlas"
        };
        m_lightmapTexture = callback(desc);
        if (!m_lightmapTexture.valid()) {
            return false;
        }
    }

    m_albedoTextures.assign(scene.materials.size(), {});
    for (std::size_t i = 0; i < scene.materials.size(); ++i) {
        const auto& material = scene.materials[i];
        if (material.albedo.empty()) {
            continue;
        }

        const std::string label = "idtech_albedo_" + material.name;
        const TextureDesc desc{
            material.albedo.width,
            material.albedo.height,
            1,
            material.albedo.channels,
            material.albedo.pixels.data(),
            label.c_str()
        };
        m_albedoTextures[i] = callback(desc);
        if (!m_albedoTextures[i].valid()) {
            return false;
        }
    }

    return true;
}

void BSPRenderer::buildDrawBatches() {
    const auto& scene = m_loader.getScene();
    m_renderIndices.clear();
    m_faceFirstIndices.assign(
        scene.faces.size(),
        std::numeric_limits<uint32_t>::max());
    m_drawBatches.clear();

    for (std::size_t materialIndex = 0;
         materialIndex < scene.materials.size();
         ++materialIndex) {
        BSPDrawBatch batch;
        batch.firstIndex = static_cast<uint32_t>(m_renderIndices.size());
        batch.materialIndex = static_cast<uint32_t>(materialIndex);
        batch.translucent = scene.materials[materialIndex].translucent;

        for (std::size_t faceIndex = 0;
             faceIndex < scene.faces.size();
             ++faceIndex) {
            const auto& face = scene.faces[faceIndex];
            if (face.materialId != materialIndex) {
                continue;
            }

            m_faceFirstIndices[faceIndex] =
                static_cast<uint32_t>(m_renderIndices.size());
            m_renderIndices.insert(
                m_renderIndices.end(),
                scene.indices.begin() + face.firstIndex,
                scene.indices.begin() + face.firstIndex + face.indexCount);
            batch.indexCount += face.indexCount;
        }

        if (batch.indexCount != 0) {
            m_drawBatches.push_back(batch);
        }
    }
}

bool BSPRenderer::explore(const float3& cameraPosition) {
    if (!m_explorer.explore(
            m_loader.getScene(),
            cameraPosition,
            m_explorationResult)) {
        m_visibleDrawBatches.clear();
        return false;
    }

    buildVisibleDrawBatches();
    assignResourceHandles(m_visibleDrawBatches);
    return true;
}

void BSPRenderer::buildVisibleDrawBatches() {
    const auto& scene = m_loader.getScene();
    std::vector<uint8_t> visibleFaces(scene.faces.size(), 0);
    for (const uint32_t faceIndex : m_explorationResult.visibleFaces) {
        if (faceIndex < visibleFaces.size()) {
            visibleFaces[faceIndex] = 1;
        }
    }

    m_visibleDrawBatches.clear();
    for (std::size_t materialIndex = 0;
         materialIndex < scene.materials.size();
         ++materialIndex) {
        BSPDrawBatch current;
        bool hasCurrent = false;

        for (std::size_t faceIndex = 0;
             faceIndex < scene.faces.size();
             ++faceIndex) {
            const auto& face = scene.faces[faceIndex];
            if (!visibleFaces[faceIndex] ||
                face.materialId != materialIndex ||
                m_faceFirstIndices[faceIndex] ==
                    std::numeric_limits<uint32_t>::max()) {
                continue;
            }

            const uint32_t firstIndex = m_faceFirstIndices[faceIndex];
            if (hasCurrent &&
                current.firstIndex + current.indexCount == firstIndex) {
                current.indexCount += face.indexCount;
                continue;
            }

            if (hasCurrent) {
                m_visibleDrawBatches.push_back(current);
            }
            current = {};
            current.firstIndex = firstIndex;
            current.indexCount = face.indexCount;
            current.materialIndex = static_cast<uint32_t>(materialIndex);
            current.translucent = scene.materials[materialIndex].translucent;
            hasCurrent = true;
        }

        if (hasCurrent) {
            m_visibleDrawBatches.push_back(current);
        }
    }
}

bool BSPRenderer::createBuffers() {
    const auto& callback = m_config.resourceCallbacks.createBuffer;
    if (!callback || m_renderIndices.empty()) {
        return false;
    }

    const auto& vertices = m_loader.getScene().vertices;
    m_vertexBuffer = callback({
        vertices.size() * sizeof(VertexP3N3T2T2),
        BufferUsage::Vertex,
        vertices.data(),
        "idtech_vertices"
    });
    if (!m_vertexBuffer.valid()) {
        return false;
    }

    m_indexBuffer = callback({
        m_renderIndices.size() * sizeof(uint32_t),
        BufferUsage::Index,
        m_renderIndices.data(),
        "idtech_indices"
    });
    return m_indexBuffer.valid();
}

void BSPRenderer::destroyResources() {
    const auto& resources = m_config.resourceCallbacks;

    if (resources.destroyBuffer) {
        if (m_vertexBuffer.valid()) {
            resources.destroyBuffer(m_vertexBuffer);
        }
        if (m_indexBuffer.valid()) {
            resources.destroyBuffer(m_indexBuffer);
        }
    }
    m_vertexBuffer.reset();
    m_indexBuffer.reset();

    if (resources.destroyTexture) {
        for (const auto texture : m_albedoTextures) {
            if (texture.valid()) {
                resources.destroyTexture(texture);
            }
        }
        if (m_lightmapTexture.valid()) {
            resources.destroyTexture(m_lightmapTexture);
        }
    }
    m_albedoTextures.clear();
    m_lightmapTexture.reset();
    m_renderIndices.clear();
    m_faceFirstIndices.clear();
    m_drawBatches.clear();
    m_visibleDrawBatches.clear();
    m_explorationResult = {};
}

void BSPRenderer::render() const {
    renderBatches(m_drawBatches);
}

bool BSPRenderer::renderVisible(const float3& cameraPosition) {
    if (!explore(cameraPosition)) {
        return false;
    }
    renderBatches(m_visibleDrawBatches);
    return true;
}

void BSPRenderer::renderBatches(
    const std::vector<BSPDrawBatch>& batches) const
{
    const auto& draw = m_config.callbacks.drawPrimitive;
    if (!draw) {
        return;
    }

    for (const auto& batch : batches) {
        if (!m_config.settings.drawTranslucent && batch.translucent) {
            continue;
        }
        draw(batch);
    }
}

} // namespace idtech
