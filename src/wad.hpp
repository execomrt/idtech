#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace idtech {

struct WadEntry {
    std::string name;
    uint32_t offset = 0;
    uint32_t size = 0;
    uint32_t expandedSize = 0;
    uint8_t type = 0;
    uint8_t compression = 0;
};

struct WadMipTexture {
    std::string name;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipOffsets[4]{};
    std::array<std::vector<uint8_t>, 4> mipPixels;
};

class WadArchive {
public:
    bool load(const std::vector<uint8_t>& data);

    std::string_view magic() const { return m_magic; }
    const std::vector<WadEntry>& entries() const { return m_entries; }
    const WadEntry* find(std::string_view name) const;
    std::optional<WadMipTexture> readMipTexture(std::size_t index) const;

private:
    std::string m_magic;
    std::vector<WadEntry> m_entries;
    std::vector<uint8_t> m_data;
};

} // namespace idtech
