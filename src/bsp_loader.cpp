#include "bsp_loader.hpp"
#include "bsp_quake1.hpp"

namespace idtech {

BSPLoader::BSPLoader() {}
BSPLoader::~BSPLoader() {}

bool BSPLoader::load(const std::vector<uint8_t>& data) {
    m_data = data;
    m_scene = {};
    
    if (data.size() < 4) return false;
    
    // Try each format
    if (loadQuake1(data)) return true;
    if (loadQuake2(data)) return true;
    return false;
}

bool BSPLoader::loadQuake1(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(quake1::Header)) return false;
    
    const quake1::Header* header = reinterpret_cast<const quake1::Header*>(data.data());
    if (header->version != 29 && header->version != 30) return false;
    
    return quake1::load(data, m_scene);
}

bool BSPLoader::loadQuake2(const std::vector<uint8_t>& data) {
    // Similar to Quake 1 but different structure
    // ... implementation
    return false;
}

void BSPLoader::setFaceVisible(int faceIndex, bool visible) {
    if (faceIndex >= 0 && faceIndex < static_cast<int>(m_scene.faces.size())) {
        m_scene.faces[faceIndex].visible = visible;
    }
}

void BSPLoader::resetVisibility() {
    for (auto& face : m_scene.faces) {
        face.visible = true;
    }
}

} // namespace idtech
