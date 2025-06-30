#pragma once
#include <fstream>
#include <vector>
#include <filesystem>
#include <stdexcept>
#include <cstdint>
#include <span>

namespace dk::helper {
/*
 * 写入和读取 POD（Plain Old Data）类型和 Blob（字节数组）到文件。
 */

template <typename POD>
void write_pod(const std::filesystem::path& f, const POD& v)
{
    std::ofstream os(f, std::ios::binary);
    if (!os) throw std::runtime_error("open fail: " + f.string());
    os.write(reinterpret_cast<const char*>(&v), sizeof(POD));
}

template <typename T>
void write_blob(const std::filesystem::path& f, const std::vector<T>& buf)
{
    std::ofstream os(f, std::ios::binary);
    os.write(reinterpret_cast<const char*>(buf.data()),
             buf.size() * sizeof(T));
}

template <typename T>
void append_blob(const std::filesystem::path& f, std::span<const T> buf)
{
    std::ofstream os(f, std::ios::binary | std::ios::app);
    if (!os) throw std::runtime_error("append open fail: " + f.string());
    os.write(reinterpret_cast<const char*>(buf.data()), buf.size_bytes());
}

template <typename POD>
POD read_pod(const std::filesystem::path& f)
{
    std::ifstream is(f, std::ios::binary);
    POD           v{};
    is.read(reinterpret_cast<char*>(&v), sizeof(v));
    return v;
}

template <typename T>
std::vector<T> read_blob(const std::filesystem::path& f)
{
    std::ifstream is(f, std::ios::binary | std::ios::ate);
    const size_t  sz = is.tellg();
    is.seekg(0);
    std::vector<T> buf(sz / sizeof(T));
    is.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(sz));
    return buf;
}

inline uint64_t hash_buffer(const std::span<const uint8_t> s)
{
    uint64_t h = 14695981039346656037ULL;
    for (uint8_t b : s) h = (h ^ b) * 1099511628211ULL;
    return h;
}
} // namespace helpers
