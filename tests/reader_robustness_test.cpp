// Robustness of the .prof reader against truncated and corrupted files.
// A corrupted capture must be rejected (0 blocks + a log message), never crash or read out of bounds.
// Best run under AddressSanitizer (-DEASY_PROFILER_TESTS_ASAN=ON) to catch silent overreads.

#include <easy/profiler.h>
#include <easy/arbitrary_value.h>
#include <easy/reader.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>
#include <sstream>
#include <string>
#include <thread>

namespace {

struct ReadResult
{
    profiler::block_index_t blocks = 0;
    std::string log;
};

ReadResult readCapture(const std::string& bytes)
{
    std::istringstream stream(bytes, std::ios::in | std::ios::binary);
    std::atomic<int> progress{0};
    profiler::BeginEndTime beginEnd{};
    profiler::SerializedData serializedBlocks, serializedDescriptors;
    profiler::descriptors_list_t descriptors;
    profiler::blocks_t blocks;
    profiler::thread_blocks_tree_t trees;
    profiler::bookmarks_t bookmarks;
    uint32_t descriptorsCount = 0, version = 0;
    profiler::processid_t pid = 0;
    std::ostringstream log;

    ReadResult result;
    result.blocks = ::fillTreesFromStream(progress, stream, beginEnd, serializedBlocks, serializedDescriptors,
                                                  descriptors, blocks, trees, bookmarks, descriptorsCount, version, pid,
                                                  true, log);
    result.log = log.str();
    return result;
}

void instrumentedWork(int depth)
{
    EASY_FUNCTION(profiler::colors::Blue);
    EASY_VALUE("depth", depth);
    if (depth > 0)
    {
        EASY_BLOCK("nested", profiler::colors::Green);
        instrumentedWork(depth - 1);
    }
    EASY_EVENT("leaf", profiler::colors::Red);
}

/// Builds a small but representative capture once: blocks, nested blocks, run-time names,
/// events, scalar values, arrays, text and two named threads.
const std::string& referenceCapture()
{
    static const std::string bytes = [] {
        EASY_PROFILER_ENABLE;
        EASY_MAIN_THREAD;

        std::thread worker([] {
            EASY_THREAD_SCOPE("Worker");
            for (int i = 0; i < 50; ++i)
                instrumentedWork(3);
        });

        for (int i = 0; i < 50; ++i)
        {
            const std::string runtimeName = "job_" + std::to_string(i % 5);
            EASY_BLOCK(runtimeName);
            const float samples[4] = {1.f, 2.f, 3.f, static_cast<float>(i)};
            EASY_VALUE("samples", samples);
            EASY_TEXT("state", "running");
            instrumentedWork(2);
        }
        worker.join();

        const std::string path = testing::TempDir() + "easy_profiler_reader_test.prof";
        EXPECT_GT(profiler::dumpBlocksToFile(path.c_str()), 0U);

        std::ifstream file(path, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        std::remove(path.c_str());
        return content;
    }();
    return bytes;
}

constexpr size_t kHeaderSize = 72;          // v2.1 header (see EasyFileHeader in reader.cpp)
constexpr size_t kMemorySizeOffset = 40;    // uint64_t memory_size
constexpr size_t kDescMemorySizeOffset = 48;// uint64_t descriptors_memory_size

void patchU64(std::string& bytes, size_t offset, uint64_t value)
{
    std::memcpy(&bytes[offset], &value, sizeof(value));
}

} // namespace

TEST(ReaderRobustness, ValidCaptureIsRead)
{
    const auto& bytes = referenceCapture();
    ASSERT_GT(bytes.size(), kHeaderSize);

    const auto result = readCapture(bytes);
    EXPECT_GT(result.blocks, 0U) << result.log;
}

TEST(ReaderRobustness, TruncatedFilesAreRejectedWithoutCrash)
{
    const auto& bytes = referenceCapture();

    // The file ends with optional bookmarks and an end marker: a cut there loses no block
    // and may legitimately be accepted. Any earlier cut must be rejected.
    constexpr size_t kTailSize = 64;

    // Every cut in the header and the descriptors, then every 7 bytes through the blocks
    for (size_t cut = 0; cut < bytes.size(); cut += (cut < 1024 ? 1 : 7))
    {
        const auto result = readCapture(bytes.substr(0, cut));
        if (cut + kTailSize < bytes.size())
            EXPECT_EQ(result.blocks, 0U) << "cut at " << cut << " of " << bytes.size();
    }
}

// Regression: header sizes were trusted for allocations (std::bad_alloc on a corrupted header)
TEST(ReaderRobustness, HugeSizesInHeaderAreRejected)
{
    auto bytes = referenceCapture();
    patchU64(bytes, kMemorySizeOffset, 1ULL << 60);
    auto result = readCapture(bytes);
    EXPECT_EQ(result.blocks, 0U);
    EXPECT_NE(result.log.find("exceed"), std::string::npos) << result.log;

    bytes = referenceCapture();
    patchU64(bytes, kDescMemorySizeOffset, 1ULL << 60);
    result = readCapture(bytes);
    EXPECT_EQ(result.blocks, 0U);
}

// Regression: the descriptor size check was commented out (heap overflow on a corrupted file)
TEST(ReaderRobustness, OversizedDescriptorRecordIsRejected)
{
    auto bytes = referenceCapture();
    const uint16_t hugeRecord = 0xFFFF;
    std::memcpy(&bytes[kHeaderSize], &hugeRecord, sizeof(hugeRecord)); // size of the first descriptor
    const auto result = readCapture(bytes);
    EXPECT_EQ(result.blocks, 0U);
}

TEST(ReaderRobustness, RandomCorruptionNeverCrashes)
{
    const auto& reference = referenceCapture();
    std::mt19937 rng(20261122u); // deterministic
    std::uniform_int_distribution<size_t> position(0, reference.size() - 1);
    std::uniform_int_distribution<int> byteValue(0, 255);

    for (int iteration = 0; iteration < 300; ++iteration)
    {
        auto bytes = reference;
        for (int flips = 0; flips < 4; ++flips)
            bytes[position(rng)] = static_cast<char>(byteValue(rng));

        // Only "no crash / no out-of-bounds access" is asserted: a flip in a timestamp is
        // legitimately undetectable and the file may still be read.
        (void)readCapture(bytes);
    }
    SUCCEED();
}
