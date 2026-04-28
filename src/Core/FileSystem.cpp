#include "GameCore/Core/FileSystem.h"

#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace GameCore::Core
{
    bool FileSystem::exists(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        return file.good();
    }

    std::string FileSystem::readTextFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file)
        {
            throw std::runtime_error("Could not open text file: " + path);
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::vector<std::uint8_t> FileSystem::readBinaryFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Could not open binary file: " + path);
        }

        return std::vector<std::uint8_t>(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>());
    }
}
