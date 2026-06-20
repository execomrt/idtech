#pragma once

#include "bsp_types.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace idtech {

class BSPExplorer {
public:
    struct Settings {
        bool usePotentiallyVisibleSet = true;
        bool allVisibleOnMissingPVS = true;
        bool includeSolidLeaf = false;
    };

    struct Callbacks {
        std::function<bool(const Bounds&)> leafInFrustum;
    };

    struct Config {
        Settings settings;
        Callbacks callbacks;
    };

    struct Result {
        int32_t cameraLeaf = -1;
        std::vector<uint32_t> visibleLeaves;
        std::vector<uint32_t> visibleFaces;
    };

    void setConfig(const Config& config) { m_config = config; }

    int32_t findLeaf(const BSPScene& scene, const float3& position) const;
    bool explore(
        const BSPScene& scene,
        const float3& cameraPosition,
        Result& result) const;

private:
    bool buildVisibleLeafMask(
        const BSPScene& scene,
        int32_t cameraLeaf,
        std::vector<uint8_t>& visibleLeaves) const;

    Config m_config;
};

} // namespace idtech
