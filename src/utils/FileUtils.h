#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <fstream>
#include <stdexcept>

// Reads a SPIR-V binary File and returns Its contents as uint32_t words.
// Throws std::runtime_error if the file cannot be opened.

inline auto readSpirv(const std::string& file_path) -> std::vector<uint32_t>
{
    std::ifstream spir_v_file(file_path, std::ios::ate | std::ios::binary);
    if (!spir_v_file.is_open())
    {
        throw std::runtime_error("Failed to open file: " + file_path + "!\n");
    }

    /*
        .tellg returns the position at the end to get the size.
        we divide that size by the memory size fo uint32_t to have the correct length.
    */ 
    size_t file_size = static_cast<size_t>(spir_v_file.tellg());
    std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

    spir_v_file.seekg(0, std::ios::beg); // returns the read position to the begining
    
    /*
        dumping the reinterpreted data (char*) to the buffer data
    */
    spir_v_file.read(reinterpret_cast<char*>(buffer.data()), file_size);
    spir_v_file.close();

    return buffer;
}