#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace GameCore::Core
{
    class FileSystem
    {
    public:
        [[nodiscard]] static bool exists(const std::string& path);
        [[nodiscard]] static std::string readTextFile(const std::string& path);
        [[nodiscard]] static std::vector<std::uint8_t> readBinaryFile(const std::string& path);
    };
}
