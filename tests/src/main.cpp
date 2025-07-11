#include "mc_MeshCodec.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>
#include <filesystem>

#define MAX_FILEPATH 0x1000

const std::string ParseInput(int argc, char** argv, int index) {
    static std::string null_string = "";

    if (argc < 2 + index) {
        return null_string;
    }

    size_t size = strnlen(argv[1 + index], MAX_FILEPATH);
    std::string value{argv[1 + index], argv[1 + index] + size};
    return value;
}

bool ReadFile(const std::string path, std::vector<mc::u8>& data) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open())
        return false;
    
    data.resize(file.tellg());
    file.seekg(0);

    file.read(reinterpret_cast<char*>(data.data()), data.size());

    file.close();

    return true;
}

void WriteFile(const std::string path, const std::span<const mc::u8>& data) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();
}

bool Decompress(const std::filesystem::path compressedPath, void* workMem, const std::filesystem::path outputPath) {
    std::vector<mc::u8> data;
    if (!ReadFile(compressedPath.string(), data)) {
        std::cout << "Failed to read file: " << compressedPath.string() << "\n";
        return false;
    }

    auto header = reinterpret_cast<const mc::ResMeshCodecPackageHeader*>(data.data());
    size_t decompressedSize = header->GetDecompressedSize();
    std::vector<mc::u8> outputBuffer(decompressedSize);

    if (mc::DecompressMC(outputBuffer.data(), decompressedSize, data.data(), data.size(), workMem, 0x10000000)) {
        std::cout << compressedPath.filename() << "\n";
        std::filesystem::create_directories(outputPath);
        WriteFile((outputPath / compressedPath.stem()).string(), outputBuffer);
        return true;
    }

    return false;
}

bool DecompressCave(const std::filesystem::path compressedPath, void* workMem, const std::filesystem::path outputPath) {
    std::vector<mc::u8> data;
    if (!ReadFile(compressedPath.string(), data)) {
        std::cout << "Failed to read file: " << compressedPath.string() << "\n";
        return false;
    }

    auto header = reinterpret_cast<const mc::ResChunkHeader*>(data.data());
    size_t decompressedSize = header->decompressedSize;
    std::vector<mc::u8> outputBuffer(decompressedSize);

    if (mc::DecompressChunk(outputBuffer.data(), decompressedSize, data.data(), data.size(), workMem, 0x10000000)) {
        std::cout << compressedPath.filename() << "\n";
        std::filesystem::create_directories(outputPath);
        WriteFile((outputPath / compressedPath.stem()).string(), outputBuffer);
        return true;
    }

    return false;
}

int main(int argc, char** argv) {

    // temporarily repurposing this as a simple cli program bc I'm lazy

    // 0x12ac10 is needed for StackAllocator initialization in game
    // you can get away with around 0x120000 here since ZSTD_DCtx is smaller than in game
    // allocate at least as much specified in the header - this includes the amount needed for StackAllocator + any additional buffers used for decompression
    // I'm just allocating an absurdly high amount here to be safe
    void* workMem = malloc(0x10000000);

    const std::filesystem::path dirPath = ParseInput(argc, argv, 0);
    const std::filesystem::path outputPath = ParseInput(argc, argv, 1);

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
        if (entry.path().extension() == ".mc") {
            if (!Decompress(entry.path().string(), workMem, outputPath / std::filesystem::relative(entry.path().parent_path(), dirPath)))
                std::cout << "Failed to decompress " << entry.path().string() << "\n";
        } else if (entry.path().extension() == ".chunk") {
            if (!DecompressCave(entry.path().string(), workMem, outputPath / std::filesystem::relative(entry.path().parent_path(), dirPath)))
                std::cout << "Failed to decompress " << entry.path().string() << "\n";
        }
    }

    free(workMem);

    return 0;
}