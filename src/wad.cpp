#include "wad.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <limits>

namespace idtech {
namespace {

#pragma pack(push, 1)
struct WadHeader {
    char magic[4];
    int32_t entryCount;
    int32_t directoryOffset;
};

struct WadDiskEntry {
    int32_t offset;
    int32_t size;
    int32_t expandedSize;
    uint8_t type;
    uint8_t compression;
    uint8_t padding[2];
    char name[16];
};

struct WadDiskMipTexture {
    char name[16];
    uint32_t width;
    uint32_t height;
    uint32_t mipOffsets[4];
};
#pragma pack(pop)

static_assert(sizeof(WadHeader) == 12);
static_assert(sizeof(WadDiskEntry) == 32);
static_assert(sizeof(WadDiskMipTexture) == 40);

std::string fixedString(const char* value, std::size_t capacity) {
    const auto end = std::find(value, value + capacity, '\0');
    return {value, end};
}

bool equalIgnoreCase(std::string_view left, std::string_view right) {
    return left.size() == right.size() &&
        std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                std::tolower(static_cast<unsigned char>(b));
        });
}

} // namespace

bool WadArchive::load(const std::vector<uint8_t>& data) {
    m_magic.clear();
    m_entries.clear();
    m_data.clear();

    if (data.size() < sizeof(WadHeader)) {
        return false;
    }

    WadHeader header{};
    std::memcpy(&header, data.data(), sizeof(header));
    if (std::memcmp(header.magic, "WAD2", 4) != 0 &&
        std::memcmp(header.magic, "WAD3", 4) != 0) {
        return false;
    }
    if (header.entryCount < 0 || header.directoryOffset < 0) {
        return false;
    }

    const auto count = static_cast<std::size_t>(header.entryCount);
    const auto directoryOffset = static_cast<std::size_t>(header.directoryOffset);
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(WadDiskEntry)) {
        return false;
    }
    const auto directorySize = count * sizeof(WadDiskEntry);
    if (directoryOffset > data.size() ||
        directorySize > data.size() - directoryOffset) {
        return false;
    }

    m_entries.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        WadDiskEntry diskEntry{};
        std::memcpy(
            &diskEntry,
            data.data() + directoryOffset + i * sizeof(WadDiskEntry),
            sizeof(diskEntry));

        if (diskEntry.offset < 0 || diskEntry.size < 0 ||
            diskEntry.expandedSize < 0) {
            return false;
        }
        if (diskEntry.compression != 0) {
            return false;
        }

        const auto offset = static_cast<std::size_t>(diskEntry.offset);
        const auto size = static_cast<std::size_t>(diskEntry.size);
        if (offset > data.size() || size > data.size() - offset) {
            return false;
        }

        m_entries.push_back({
            fixedString(diskEntry.name, sizeof(diskEntry.name)),
            static_cast<uint32_t>(diskEntry.offset),
            static_cast<uint32_t>(diskEntry.size),
            static_cast<uint32_t>(diskEntry.expandedSize),
            diskEntry.type,
            diskEntry.compression
        });
    }

    m_magic.assign(header.magic, sizeof(header.magic));
    m_data = data;
    return true;
}

const WadEntry* WadArchive::find(std::string_view name) const {
    const auto entry = std::find_if(
        m_entries.begin(),
        m_entries.end(),
        [name](const WadEntry& candidate) {
            return equalIgnoreCase(candidate.name, name);
        });
    return entry == m_entries.end() ? nullptr : &*entry;
}

std::optional<WadMipTexture> WadArchive::readMipTexture(std::size_t index) const {
    if (index >= m_entries.size()) {
        return std::nullopt;
    }

    const auto& entry = m_entries[index];
    if (entry.size < sizeof(WadDiskMipTexture) ||
        entry.offset > m_data.size() ||
        entry.size > m_data.size() - entry.offset) {
        return std::nullopt;
    }

    WadDiskMipTexture diskTexture{};
    std::memcpy(&diskTexture, m_data.data() + entry.offset, sizeof(diskTexture));
    if (diskTexture.width == 0 || diskTexture.height == 0) {
        return std::nullopt;
    }

    WadMipTexture texture;
    texture.name = fixedString(diskTexture.name, sizeof(diskTexture.name));
    texture.width = diskTexture.width;
    texture.height = diskTexture.height;
    std::copy_n(diskTexture.mipOffsets, 4, texture.mipOffsets);

    for (std::size_t level = 0; level < 4; ++level) {
        const uint64_t width = std::max<uint32_t>(1, texture.width >> level);
        const uint64_t height = std::max<uint32_t>(1, texture.height >> level);
        const uint64_t offset = texture.mipOffsets[level];
        const uint64_t byteCount = width * height;
        if (offset > entry.size || byteCount > entry.size - offset) {
            return std::nullopt;
        }

        const auto begin = m_data.begin() + entry.offset + offset;
        texture.mipPixels[level].assign(begin, begin + byteCount);
    }

    return texture;
}

} // namespace idtech
