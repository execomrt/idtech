#pragma once
#include <vector>
#include "bsp_types.hpp"

namespace idtech {

// Main BSP loader
class BSPLoader {
public:
    BSPLoader();
    ~BSPLoader();
    
    // Load BSP from memory
    bool load(const std::vector<uint8_t>& data);
    
    // Get the built scene
    const BSPScene& getScene() const { return m_scene; }
    BSPScene& getScene() { return m_scene; }
    
    // Control face visibility
    void setFaceVisible(int faceIndex, bool visible);
    void resetVisibility();
    
private:
    BSPScene m_scene;
    std::vector<uint8_t> m_data;
    
    // Internal builders
    bool loadQuake1(const std::vector<uint8_t>& data);
    bool loadQuake2(const std::vector<uint8_t>& data);
};

} // namespace idtech
