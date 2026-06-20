#include "bsp_explorer.hpp"

#include <algorithm>

namespace idtech {

int32_t BSPExplorer::findLeaf(
    const BSPScene& scene,
    const float3& position) const
{
    if (scene.models.empty()) {
        return -1;
    }

    int32_t index = scene.models.front().headNodes[0];
    while (index >= 0) {
        if (static_cast<std::size_t>(index) >= scene.nodes.size()) {
            return -1;
        }

        const auto& node = scene.nodes[index];
        if (node.planenum < 0 ||
            static_cast<std::size_t>(node.planenum) >= scene.planes.size()) {
            return -1;
        }

        const auto& plane = scene.planes[node.planenum];
        const float distance =
            float3::dot(position, plane.normal) - plane.dist;
        index = node.children[distance >= 0.0f ? 0 : 1];
    }

    const int32_t leafIndex = ~index;
    return leafIndex >= 0 &&
            static_cast<std::size_t>(leafIndex) < scene.leafs.size()
        ? leafIndex
        : -1;
}

bool BSPExplorer::buildVisibleLeafMask(
    const BSPScene& scene,
    int32_t cameraLeaf,
    std::vector<uint8_t>& visibleLeaves) const
{
    visibleLeaves.assign(scene.leafs.size(), 0);
    if (scene.leafs.empty()) {
        return false;
    }

    const auto markAll = [&]() {
        std::fill(visibleLeaves.begin(), visibleLeaves.end(), uint8_t{1});
        if (!m_config.settings.includeSolidLeaf) {
            visibleLeaves[0] = 0;
        }
    };

    if (!m_config.settings.usePotentiallyVisibleSet) {
        markAll();
        return true;
    }
    if (cameraLeaf <= 0 ||
        static_cast<std::size_t>(cameraLeaf) >= scene.leafs.size()) {
        if (m_config.settings.allVisibleOnMissingPVS) {
            markAll();
            return true;
        }
        return false;
    }

    const int32_t visibilityOffset =
        scene.leafs[cameraLeaf].visibilityOffset;
    if (visibilityOffset < 0 ||
        static_cast<std::size_t>(visibilityOffset) >= scene.visibility.size()) {
        if (m_config.settings.allVisibleOnMissingPVS) {
            markAll();
            return true;
        }
        return false;
    }

    std::size_t cursor = static_cast<std::size_t>(visibilityOffset);
    std::size_t leaf = 1;
    while (leaf < scene.leafs.size()) {
        if (cursor >= scene.visibility.size()) {
            return false;
        }

        const uint8_t value = scene.visibility[cursor++];
        if (value == 0) {
            if (cursor >= scene.visibility.size()) {
                return false;
            }
            const uint8_t runLength = scene.visibility[cursor++];
            if (runLength == 0) {
                return false;
            }
            leaf = std::min(
                scene.leafs.size(),
                leaf + static_cast<std::size_t>(runLength) * 8);
            continue;
        }

        for (uint32_t bit = 0; bit < 8 && leaf < scene.leafs.size();
             ++bit, ++leaf) {
            if ((value & (1u << bit)) != 0) {
                visibleLeaves[leaf] = 1;
            }
        }
    }

    if (m_config.settings.includeSolidLeaf) {
        visibleLeaves[0] = 1;
    }
    return true;
}

bool BSPExplorer::explore(
    const BSPScene& scene,
    const float3& cameraPosition,
    Result& result) const
{
    result = {};
    result.cameraLeaf = findLeaf(scene, cameraPosition);

    std::vector<uint8_t> visibleLeafMask;
    if (!buildVisibleLeafMask(scene, result.cameraLeaf, visibleLeafMask)) {
        return false;
    }

    std::vector<uint8_t> visibleFaceMask(scene.faces.size(), 0);
    for (std::size_t leafIndex = 0;
         leafIndex < scene.leafs.size();
         ++leafIndex) {
        if (!visibleLeafMask[leafIndex]) {
            continue;
        }

        const auto& leaf = scene.leafs[leafIndex];
        if (m_config.callbacks.leafInFrustum &&
            !m_config.callbacks.leafInFrustum(leaf.bounds)) {
            continue;
        }

        result.visibleLeaves.push_back(static_cast<uint32_t>(leafIndex));
        const std::size_t first = leaf.firstMarkSurface;
        const std::size_t count = leaf.markSurfaceCount;
        if (first > scene.markSurfaces.size() ||
            count > scene.markSurfaces.size() - first) {
            return false;
        }

        for (std::size_t i = 0; i < count; ++i) {
            const uint32_t faceIndex = scene.markSurfaces[first + i];
            if (faceIndex >= visibleFaceMask.size()) {
                return false;
            }
            visibleFaceMask[faceIndex] = 1;
        }
    }

    for (std::size_t faceIndex = 0;
         faceIndex < visibleFaceMask.size();
         ++faceIndex) {
        if (visibleFaceMask[faceIndex]) {
            result.visibleFaces.push_back(static_cast<uint32_t>(faceIndex));
        }
    }
    return true;
}

} // namespace idtech
