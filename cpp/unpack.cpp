#include "unpack.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <zstd.h>
#include <zlib.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
static constexpr size_t IO_BUFFER_SIZE = 1 << 20;  // 1 MiB

//------------------------------------------------------------------------------
// MemoryMap implementation
//------------------------------------------------------------------------------

MemoryMap::MemoryMap(const std::string &path) {
#ifdef _WIN32
    file_ = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_ == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open file");
    LARGE_INTEGER sz;
    if (!GetFileSizeEx((HANDLE)file_, &sz)) throw std::runtime_error("GetFileSizeEx failed");
    size_ = static_cast<size_t>(sz.QuadPart);
    mapping_ = CreateFileMappingA((HANDLE)file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping_) throw std::runtime_error("CreateFileMapping failed");
    data_ = static_cast<const uint8_t*>(MapViewOfFile((HANDLE)mapping_, FILE_MAP_READ, 0, 0, 0));
    if (!data_) throw std::runtime_error("MapViewOfFile failed");
#else
    fd_ = open(path.c_str(), O_RDONLY);
    if (fd_ < 0) throw std::runtime_error("Cannot open file");
    struct stat st; fstat(fd_, &st);
    size_ = static_cast<size_t>(st.st_size);
    void *ptr = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
    if (ptr == MAP_FAILED) throw std::runtime_error("mmap failed");
    data_ = static_cast<const uint8_t*>(ptr);
#endif
}

MemoryMap::~MemoryMap() {
#ifdef _WIN32
    if (data_) UnmapViewOfFile(data_);
    if (mapping_) CloseHandle((HANDLE)mapping_);
    if (file_) CloseHandle((HANDLE)file_);
#else
    if (data_) munmap(const_cast<uint8_t*>(data_), size_);
    if (fd_ >= 0) close(fd_);
#endif
}

const uint8_t* MemoryMap::data() const { return data_; }
size_t         MemoryMap::size() const { return size_; }

//------------------------------------------------------------------------------
// ThreadPool implementation (PImpl idiom)
//------------------------------------------------------------------------------

struct ThreadPool::Impl {
    std::vector<std::thread>          workers;
    std::queue<std::function<void()>> tasks;
    std::mutex                        mutex;
    std::condition_variable           cvTask;
    std::condition_variable           cvDone;
    bool                              stop = false;
    size_t                            busy = 0;
};

ThreadPool::ThreadPool(size_t threadCount)
    : impl_(new Impl()) {
    for (size_t i = 0; i < threadCount; ++i) {
        impl_->workers.emplace_back([this] {
            auto &I = *impl_;
            while (true) {
                std::function<void()> job;
                {
                    std::unique_lock<std::mutex> lock(I.mutex);
                    I.cvTask.wait(lock, [&] { return I.stop || !I.tasks.empty(); });
                    if (I.stop && I.tasks.empty()) return;
                    job = std::move(I.tasks.front());
                    I.tasks.pop();
                    ++I.busy;
                }
                job();
                {
                    std::unique_lock<std::mutex> lock(I.mutex);
                    --I.busy;
                    if (I.tasks.empty() && I.busy == 0) {
                        I.cvDone.notify_one();
                    }
                }
            }
        });
    }
}

void ThreadPool::enqueue(std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->tasks.push(std::move(job));
    }
    impl_->cvTask.notify_one();
}

void ThreadPool::wait() {
    std::unique_lock<std::mutex> lock(impl_->mutex);
    impl_->cvDone.wait(lock, [&] { return impl_->tasks.empty() && impl_->busy == 0; });
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->stop = true;
    }
    impl_->cvTask.notify_all();
    for (auto &worker : impl_->workers) {
        worker.join();
    }
    delete impl_;
}

//------------------------------------------------------------------------------
// Utility: read little-endian integers from memory
//------------------------------------------------------------------------------

template<typename T>
static T readLE(const uint8_t *ptr) {
    T val;
    std::memcpy(&val, ptr, sizeof(T));
    return val;
}

//------------------------------------------------------------------------------
// Compute global CRC32 over data[0..size-5] (exclude last 4-byte footer)
//------------------------------------------------------------------------------

static uint32_t computeGlobalCrc(const uint8_t *data, size_t size) {
    size_t validLen = (size > sizeof(uint32_t)) ? size - sizeof(uint32_t) : 0;
    uint32_t crc = crc32(0, nullptr, 0);
    size_t offset = 0;
    while (offset < validLen) {
        size_t chunk = std::min<size_t>(IO_BUFFER_SIZE, validLen - offset);
        crc = crc32(crc, data + offset, static_cast<uInt>(chunk));
        offset += chunk;
    }
    return crc;
}

//------------------------------------------------------------------------------
static void decompressSection(const uint8_t *base,
                              size_t offset,
                              size_t compSize,
                              const fs::path &outputPath) {
    const uint8_t *srcPtr = base + offset;
    ZSTD_DStream *dctx = ZSTD_createDStream();
    if (!dctx) throw std::runtime_error("Failed to create Zstd decompressor");
    if (ZSTD_isError(ZSTD_initDStream(dctx))) {
        throw std::runtime_error("Failed to initialize Zstd decompressor");
    }

    std::ofstream outFile(outputPath, std::ios::binary);
    ZSTD_inBuffer inBuf{srcPtr, compSize, 0};
    std::vector<char> outBuf(IO_BUFFER_SIZE);
    ZSTD_outBuffer outZ{outBuf.data(), outBuf.size(), 0};

    while (inBuf.pos < inBuf.size) {
        size_t ret = ZSTD_decompressStream(dctx, &outZ, &inBuf);
        if (ZSTD_isError(ret)) {
            ZSTD_freeDStream(dctx);
            throw std::runtime_error("Zstd decompression error");
        }
        outFile.write(outBuf.data(), outZ.pos);
        outZ.pos = 0;
    }

    ZSTD_freeDStream(dctx);
}

//------------------------------------------------------------------------------
// unpackModel implementation
//------------------------------------------------------------------------------

void unpackModel(const std::string &bundlePath,
                 const std::string &outDir) {
    MemoryMap mm(bundlePath);
    const uint8_t *base = mm.data();
    size_t totalSize   = mm.size();

    // Validate global CRC32
    uint32_t storedCrc = readLE<uint32_t>(base + totalSize - sizeof(uint32_t));
    if (storedCrc != computeGlobalCrc(base, totalSize)) {
        throw std::runtime_error("Global CRC mismatch");
    }

    // Parse header manually
    const uint8_t *p = base;
    p += 7; // magic
    p += 2; // version
    p += 4; // reserved
    uint64_t configOffset = readLE<uint64_t>(p); p += 8;
    uint64_t configLength = readLE<uint64_t>(p); p += 8;
    uint64_t tocOffset    = readLE<uint64_t>(p); p += 8;

    // Collect entries (config.json + TOC entries)
    std::vector<Entry> entries;
    entries.push_back({"config.json", configOffset, configLength, 0, 0});

    size_t ptr = tocOffset;
    while (ptr + sizeof(uint16_t) < totalSize - sizeof(uint32_t)) {
        uint16_t nameLen = readLE<uint16_t>(base + ptr); ptr += 2;
        std::string name(reinterpret_cast<const char*>(base + ptr), nameLen);
        ptr += nameLen;
        uint64_t offset = readLE<uint64_t>(base + ptr); ptr += 8;
        uint64_t clen   = readLE<uint64_t>(base + ptr); ptr += 8;
        uint64_t rlen   = readLE<uint64_t>(base + ptr); ptr += 8;
        uint32_t crc    = readLE<uint32_t>(base + ptr); ptr += 4;
        entries.push_back({name, offset, clen, rlen, crc});
    }

    fs::create_directories(outDir);
    ThreadPool pool(std::thread::hardware_concurrency());
    for (auto &e : entries) {
        // Skip if file exists and size matches expected raw length
        fs::path outPath = fs::path(outDir) / e.name;
        if (fs::exists(outPath) && fs::file_size(outPath) == e.raw_length) {
            continue; // skip already extracted section
        }
        pool.enqueue([=](){ decompressSection(base, e.offset, e.comp_length, fs::path(outDir)/e.name); });
    }
    pool.wait();
}
