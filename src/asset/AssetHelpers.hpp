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
    if (!os)
    {
        throw std::runtime_error("open fail: " + f.string());
    }
    os.write(reinterpret_cast<const char*>(&v), sizeof(POD));
}

template <typename T>
void write_blob(const std::filesystem::path& f, const std::span<T>& buf)
{
    std::ofstream os(f, std::ios::binary);
    os.write(reinterpret_cast<const char*>(buf.data()),
             buf.size() * sizeof(T));
}

template <typename T>
void append_blob(const std::filesystem::path& f, const std::span<T>& buf)
{
    std::ofstream os(f, std::ios::binary | std::ios::app);
    if (!os) throw std::runtime_error("append open fail: " + f.string());
    os.write(reinterpret_cast<const char*>(buf.data()), buf.size_bytes());
}

template <typename POD>
POD read_pod(const std::filesystem::path& f, const std::streamoff off = 0)
{
    std::ifstream is(f, std::ios::binary);
    if (!is) throw std::runtime_error("open fail: " + f.string());
    is.seekg(off);
    POD v{};
    is.read(reinterpret_cast<char*>(&v), sizeof(POD));
    if (!is) throw std::runtime_error("read_pod fail: " + f.string());
    return v;
}

/* ---------- 读定长 Blob ----------
 * 假设你已知元素个数 elemCount */
template <typename T>
std::vector<T> read_blob(const std::filesystem::path& f,
                         const std::streamoff         off,
                         const size_t                 elem_count)
{
    std::ifstream is(f, std::ios::binary);
    if (!is) throw std::runtime_error("open fail: " + f.string());
    is.seekg(off);

    std::vector<T> buf(elem_count);
    is.read(reinterpret_cast<char*>(buf.data()), elem_count * sizeof(T));
    if (!is) throw std::runtime_error("read_blob(len) fail: " + f.string());
    return buf;
}

/* ---------- 读到文件末尾的 Blob ----------
 * 只给 offset，让函数自动算元素个数 */
template <typename T>
std::vector<T> read_blob(const std::filesystem::path& f,
                         const std::streamoff         off = 0)
{
    std::ifstream is(f, std::ios::binary | std::ios::ate);
    if (!is) throw std::runtime_error("open fail: " + f.string());

    std::streamsize fileSize = is.tellg();
    if (fileSize < off) throw std::runtime_error("offset beyond EOF: " + f.string());

    size_t bytes = static_cast<size_t>(fileSize - off);
    if (bytes % sizeof(T) != 0)
        throw std::runtime_error("file size not aligned to T: " + f.string());

    size_t         elemCount = bytes / sizeof(T);
    std::vector<T> buf(elemCount);

    is.seekg(off);
    is.read(reinterpret_cast<char*>(buf.data()), bytes);

    if (!is) throw std::runtime_error("read_blob(eof) fail: " + f.string());
    return buf;
}

// 对任意内存块做 FNV-1a
inline uint64_t fnv1a_hash(const void* data, const std::size_t len)
{
    static constexpr uint64_t FNV_OFFSET_BASIS = 14652710'57926649ULL;
    static constexpr uint64_t FNV_PRIME        = 1099511628211ULL;
    auto                      ptr              = static_cast<const uint8_t*>(data);
    uint64_t                  hash             = FNV_OFFSET_BASIS;
    for (std::size_t i = 0; i < len; ++i)
    {
        hash ^= ptr[i];
        hash *= FNV_PRIME;
    }
    return hash;
}


inline uint64_t hash_combine(const uint64_t seed, const uint64_t v)
{
    // 来自 boost::hash_combine 的经典写法
    return seed ^ (v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

template <typename T>
uint64_t hash_array_elements(const T* arr, const std::size_t count)
{
    uint64_t seed = 0xcbf29ce484222325ULL; // 任意非 0 初始值
    for (std::size_t i = 0; i < count; ++i)
    {
        // 先对单个元素用 std::hash<T>
        const uint64_t h = std::hash<T>{}(arr[i]);
        seed             = hash_combine(seed, h);
    }
    return seed;
}

template <typename T>
uint64_t hash_buffer(const T* arr, const std::size_t count)
{
    if constexpr (std::is_trivially_copyable_v<T>)
    {
        return fnv1a_hash(arr, sizeof(T) * count);
    }
    else
    {
        uint64_t seed = 0xcbf29ce484222325ULL;
        for (std::size_t i = 0; i < count; ++i)
        {
            seed = hash_combine(seed, static_cast<uint64_t>(std::hash<T>{}(arr[i])));
        }
        return seed;
    }
}

template <typename T>
uint64_t hash_buffer(const std::vector<T>& vec)
{
    return hash_buffer(vec.data(), vec.size());
}

template <typename T, std::size_t N>
uint64_t hash_buffer(const std::array<T, N>& arr)
{
    return hash_buffer(arr.data(), arr.size());
}

template <typename T>
uint64_t hash_buffer(std::span<const T> s)
{
    return hash_buffer(s.data(), s.size());
}
} // namespace helpers
