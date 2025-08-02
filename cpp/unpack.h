#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

// -----------------------------------------------------------------------------
// Container format constants
// -----------------------------------------------------------------------------

static constexpr char CONTAINER_MAGIC[7] = {'Q','G','E','N','I','E','1'};
static constexpr uint16_t CONTAINER_VERSION = 1;

// -----------------------------------------------------------------------------
// Metadata for each section inside the bundle
// -----------------------------------------------------------------------------
struct Entry {
    std::string name;        // Filename (e.g., "config.json", "model_part_1.bin")
    uint64_t    offset;      // Byte offset where this section's compressed data begins
    uint64_t    comp_length; // Length in bytes of the compressed data
    uint64_t    raw_length;  // Expected size after decompression
    uint32_t    crc32;       // CRC32 checksum of the compressed data
};

// -----------------------------------------------------------------------------
// Cross-platform memory-map helper (read-only)
// -----------------------------------------------------------------------------
class MemoryMap {
public:
    MemoryMap(const std::string &path);
    ~MemoryMap();

    const uint8_t *data() const;
    size_t         size() const;

private:
    const uint8_t *data_;
    size_t         size_;
#ifdef _WIN32
    void          *file_;
    void          *mapping_;
#else
    int            fd_;
#endif
};

// -----------------------------------------------------------------------------
// Simple thread pool for executing tasks in parallel
// -----------------------------------------------------------------------------
class ThreadPool {
public:
    explicit ThreadPool(size_t threads);
    ~ThreadPool();

    void enqueue(std::function<void()> job);
    void wait();

private:
    struct Impl;
    Impl *impl_;
};

// -----------------------------------------------------------------------------
// Public API: unpack function only
// -----------------------------------------------------------------------------

/**
 * unpackModel
 *
 * Extracts all sections from a bundled file into the specified output directory.
 * Performs CRC validation and uses a thread pool to decompress sections in parallel.
 *
 * @param bundlePath Path to the input bundle file
 * @param outDir     Directory where extracted files will be written
 */
void unpackModel(const std::string &bundlePath,
                 const std::string &outDir);
