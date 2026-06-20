#include "lightmap_packer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>

int main() {
    const std::array<uint8_t, 4> red{255, 0, 0, 255};
    const std::array<uint8_t, 6> green{0, 255, 0, 0, 255, 0};
    const std::array<idtech::LightmapImage, 2> images{{
        {10, 2, 2, 1, red},
        {20, 3, 2, 1, green}
    }};

    idtech::LightmapAtlas atlas;
    const idtech::LightmapPacker packer({
        .initialSize = 4,
        .maximumSize = 16,
        .padding = 1,
        .clearValue = 0
    });

    if (!packer.pack(images, atlas)) {
        std::cerr << "Unable to pack test lightmaps\n";
        return 1;
    }
    if (atlas.placements.size() != images.size() ||
        atlas.texture.width > 16 || atlas.texture.height > 16 ||
        atlas.texture.channels != 1) {
        std::cerr << "Invalid atlas result\n";
        return 2;
    }

    const auto redPlacement = std::find_if(
        atlas.placements.begin(),
        atlas.placements.end(),
        [](const idtech::LightmapPlacement& placement) {
            return placement.id == 10;
        });
    if (redPlacement == atlas.placements.end()) {
        std::cerr << "Missing packed lightmap\n";
        return 3;
    }

    const auto pixel = [&](uint32_t x, uint32_t y) {
        return atlas.texture.pixels[
            static_cast<std::size_t>(y) * atlas.texture.width + x];
    };
    if (pixel(redPlacement->x, redPlacement->y) != 255 ||
        pixel(redPlacement->x - 1, redPlacement->y) != 255 ||
        pixel(redPlacement->x, redPlacement->y - 1) != 255) {
        std::cerr << "Pixel copy or padding extrusion failed\n";
        return 4;
    }

    std::cout
        << "Packed " << atlas.placements.size() << " lightmaps into "
        << atlas.texture.width << 'x' << atlas.texture.height << '\n';
    return 0;
}
