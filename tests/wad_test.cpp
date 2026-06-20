#include "wad.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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

} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path wadPath =
        argc > 1
            ? std::filesystem::path(argv[1])
            : std::filesystem::path(IDTECH_TEST_ASSET_DIR) / "skins.wad";

    const auto bytes = readFile(wadPath);
    if (bytes.empty()) {
        std::cerr << "Unable to read WAD: " << wadPath << '\n';
        return 1;
    }

    idtech::WadArchive wad;
    if (!wad.load(bytes)) {
        std::cerr << "Unable to parse WAD: " << wadPath << '\n';
        return 2;
    }

    std::size_t textureCount = 0;
    std::size_t textureBytes = 0;
    std::cout
        << "Loaded: " << wadPath << '\n'
        << "File size: " << bytes.size() << " bytes\n"
        << "Format: " << wad.magic() << '\n'
        << "Entries: " << wad.entries().size() << '\n';

    for (std::size_t i = 0; i < wad.entries().size(); ++i) {
        const auto texture = wad.readMipTexture(i);
        if (!texture) {
            continue;
        }

        ++textureCount;
        for (const auto& mip : texture->mipPixels) {
            textureBytes += mip.size();
        }
        if (textureCount <= 8) {
            std::cout
                << "  " << texture->name << ": "
                << texture->width << 'x' << texture->height << '\n';
        }
    }

    std::cout
        << "Valid mip textures: " << textureCount << '\n'
        << "Decoded indexed pixels: " << textureBytes << " bytes\n";
    if (textureCount == 0) {
        std::cerr << "WAD contains no valid mip textures\n";
        return 3;
    }

    const auto& firstName = wad.entries().front().name;
    if (wad.find(firstName) == nullptr) {
        std::cerr << "Case-insensitive entry lookup failed\n";
        return 4;
    }

    return 0;
}
