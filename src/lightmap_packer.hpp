#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <span>
#include <vector>

namespace idtech {

struct CpuTexture {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    std::vector<uint8_t> pixels;

    bool empty() const {
        return pixels.empty();
    }
};

struct LightmapImage {
    uint32_t id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    std::span<const uint8_t> pixels;
};

struct LightmapPlacement {
    uint32_t id = 0;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct LightmapAtlas {
    CpuTexture texture;
    std::vector<LightmapPlacement> placements;
};

class LightmapPacker {
public:
    struct Options {
        uint32_t initialSize = 128;
        uint32_t maximumSize = 4096;
        uint32_t padding = 1;
        uint8_t clearValue = 255;
    };

    LightmapPacker() = default;
    explicit LightmapPacker(Options options) : m_options(options) {}

    bool pack(std::span<const LightmapImage> images, LightmapAtlas& atlas) const {
        atlas = {};
        if (images.empty()) {
            return true;
        }

        const uint32_t channels = images.front().channels;
        if (channels == 0 || m_options.maximumSize == 0) {
            return false;
        }

        uint32_t largestWidth = 0;
        uint32_t largestHeight = 0;
        uint64_t totalArea = 0;
        for (const auto& image : images) {
            if (image.width == 0 || image.height == 0 ||
                image.channels != channels) {
                return false;
            }

            const uint64_t pixelCount =
                static_cast<uint64_t>(image.width) * image.height;
            if (pixelCount * channels != image.pixels.size()) {
                return false;
            }

            const uint64_t paddedWidth =
                static_cast<uint64_t>(image.width) + 2 * m_options.padding;
            const uint64_t paddedHeight =
                static_cast<uint64_t>(image.height) + 2 * m_options.padding;
            if (paddedWidth > m_options.maximumSize ||
                paddedHeight > m_options.maximumSize) {
                return false;
            }

            largestWidth =
                std::max(largestWidth, static_cast<uint32_t>(paddedWidth));
            largestHeight =
                std::max(largestHeight, static_cast<uint32_t>(paddedHeight));
            totalArea += paddedWidth * paddedHeight;
        }

        uint32_t width = nextPowerOfTwo(std::max(m_options.initialSize, largestWidth));
        uint32_t height = nextPowerOfTwo(std::max(m_options.initialSize, largestHeight));
        while (static_cast<uint64_t>(width) * height < totalArea) {
            if (!grow(width, height)) {
                return false;
            }
        }

        std::vector<Rect> packed;
        while (!tryPack(images, width, height, packed)) {
            if (!grow(width, height)) {
                return false;
            }
        }

        const uint64_t atlasBytes =
            static_cast<uint64_t>(width) * height * channels;
        if (atlasBytes > std::numeric_limits<std::size_t>::max()) {
            return false;
        }

        atlas.texture.width = width;
        atlas.texture.height = height;
        atlas.texture.channels = channels;
        atlas.texture.pixels.assign(
            static_cast<std::size_t>(atlasBytes),
            m_options.clearValue);
        atlas.placements.resize(images.size());

        for (std::size_t i = 0; i < images.size(); ++i) {
            const auto& image = images[i];
            const auto& rectangle = packed[i];
            const uint32_t contentX = rectangle.x + m_options.padding;
            const uint32_t contentY = rectangle.y + m_options.padding;

            atlas.placements[i] = {
                image.id,
                contentX,
                contentY,
                image.width,
                image.height
            };
            copyImage(image, contentX, contentY, atlas.texture);
            extrudePadding(image, contentX, contentY, atlas.texture);
        }

        return true;
    }

private:
    struct Rect {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    static uint32_t nextPowerOfTwo(uint32_t value) {
        if (value <= 1) {
            return 1;
        }
        --value;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        return value + 1;
    }

    bool grow(uint32_t& width, uint32_t& height) const {
        if (width <= height && width < m_options.maximumSize) {
            width = std::min(width * 2, m_options.maximumSize);
            return true;
        }
        if (height < m_options.maximumSize) {
            height = std::min(height * 2, m_options.maximumSize);
            return true;
        }
        if (width < m_options.maximumSize) {
            width = std::min(width * 2, m_options.maximumSize);
            return true;
        }
        return false;
    }

    bool tryPack(
        std::span<const LightmapImage> images,
        uint32_t atlasWidth,
        uint32_t atlasHeight,
        std::vector<Rect>& packed) const
    {
        std::vector<std::size_t> order(images.size());
        std::iota(order.begin(), order.end(), 0);
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            const uint64_t areaA =
                static_cast<uint64_t>(images[a].width + 2 * m_options.padding) *
                (images[a].height + 2 * m_options.padding);
            const uint64_t areaB =
                static_cast<uint64_t>(images[b].width + 2 * m_options.padding) *
                (images[b].height + 2 * m_options.padding);
            return areaA > areaB;
        });

        std::vector<uint32_t> skyline(atlasWidth, 0);
        packed.assign(images.size(), {});

        for (const std::size_t imageIndex : order) {
            const uint32_t width =
                images[imageIndex].width + 2 * m_options.padding;
            const uint32_t height =
                images[imageIndex].height + 2 * m_options.padding;

            uint32_t bestX = 0;
            uint32_t bestY = std::numeric_limits<uint32_t>::max();
            for (uint32_t x = 0; x + width <= atlasWidth; ++x) {
                uint32_t y = 0;
                for (uint32_t column = x; column < x + width; ++column) {
                    y = std::max(y, skyline[column]);
                    if (y >= bestY) {
                        break;
                    }
                }
                if (y + height <= atlasHeight && y < bestY) {
                    bestX = x;
                    bestY = y;
                    if (bestY == 0) {
                        break;
                    }
                }
            }

            if (bestY == std::numeric_limits<uint32_t>::max()) {
                return false;
            }

            packed[imageIndex] = {bestX, bestY, width, height};
            std::fill(
                skyline.begin() + bestX,
                skyline.begin() + bestX + width,
                bestY + height);
        }

        return true;
    }

    static void copyPixel(
        const CpuTexture& source,
        uint32_t sourceX,
        uint32_t sourceY,
        CpuTexture& destination,
        uint32_t destinationX,
        uint32_t destinationY)
    {
        const std::size_t sourceOffset =
            (static_cast<std::size_t>(sourceY) * source.width + sourceX) *
            source.channels;
        const std::size_t destinationOffset =
            (static_cast<std::size_t>(destinationY) * destination.width +
             destinationX) *
            destination.channels;
        std::copy_n(
            source.pixels.data() + sourceOffset,
            source.channels,
            destination.pixels.data() + destinationOffset);
    }

    static CpuTexture asTexture(const LightmapImage& image) {
        CpuTexture texture;
        texture.width = image.width;
        texture.height = image.height;
        texture.channels = image.channels;
        texture.pixels.assign(image.pixels.begin(), image.pixels.end());
        return texture;
    }

    static void copyImage(
        const LightmapImage& image,
        uint32_t destinationX,
        uint32_t destinationY,
        CpuTexture& destination)
    {
        const std::size_t rowBytes =
            static_cast<std::size_t>(image.width) * image.channels;
        for (uint32_t y = 0; y < image.height; ++y) {
            const auto* source =
                image.pixels.data() + static_cast<std::size_t>(y) * rowBytes;
            auto* target =
                destination.pixels.data() +
                (static_cast<std::size_t>(destinationY + y) * destination.width +
                 destinationX) *
                    destination.channels;
            std::copy_n(source, rowBytes, target);
        }
    }

    void extrudePadding(
        const LightmapImage& image,
        uint32_t x,
        uint32_t y,
        CpuTexture& destination) const
    {
        if (m_options.padding == 0) {
            return;
        }

        const CpuTexture source = asTexture(image);
        for (uint32_t padding = 1; padding <= m_options.padding; ++padding) {
            for (uint32_t column = 0; column < image.width; ++column) {
                copyPixel(source, column, 0, destination, x + column, y - padding);
                copyPixel(
                    source,
                    column,
                    image.height - 1,
                    destination,
                    x + column,
                    y + image.height - 1 + padding);
            }
            for (uint32_t row = 0; row < image.height; ++row) {
                copyPixel(source, 0, row, destination, x - padding, y + row);
                copyPixel(
                    source,
                    image.width - 1,
                    row,
                    destination,
                    x + image.width - 1 + padding,
                    y + row);
            }

            copyPixel(source, 0, 0, destination, x - padding, y - padding);
            copyPixel(
                source,
                image.width - 1,
                0,
                destination,
                x + image.width - 1 + padding,
                y - padding);
            copyPixel(
                source,
                0,
                image.height - 1,
                destination,
                x - padding,
                y + image.height - 1 + padding);
            copyPixel(
                source,
                image.width - 1,
                image.height - 1,
                destination,
                x + image.width - 1 + padding,
                y + image.height - 1 + padding);
        }
    }

    Options m_options;
};

} // namespace idtech
