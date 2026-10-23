#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <filesystem>

#include "utp/types.hpp"
#include "utp/uri_parser.hpp"
#include "utp/chunk_planner.hpp"
#include "utp/state_manager.hpp"
#include "utp/engine.hpp"
#include "mock_provider.hpp"

namespace fs = std::filesystem;

// ANSI Colors for test reporter
#define T_RESET   "\033[0m"
#define T_BOLD    "\033[1m"
#define T_GREEN   "\033[38;2;46;204;113m"
#define T_RED     "\033[38;2;231;76;60m"
#define T_CYAN    "\033[38;2;0;229;255m"
#define T_YELLOW  "\033[38;2;241;196;15m"
#define T_MUTED   "\033[38;2;120;120;140m"

static int g_tests_passed = 0;
static int g_tests_failed = 0;

void run_test(const std::string& suite_name, const std::string& test_name, std::function<void()> fn) {
    std::cout << "  " T_CYAN "◇ " T_RESET << suite_name << " :: " << test_name << " ... ";
    std::cout.flush();
    auto start = std::chrono::high_resolution_clock::now();
    try {
        fn();
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << T_GREEN "PASSED" T_RESET T_MUTED " (" << ms << " ms)" T_RESET "\n";
        g_tests_passed++;
    } catch (const std::exception& e) {
        std::cout << T_RED "FAILED: " << e.what() << T_RESET "\n";
        g_tests_failed++;
    } catch (...) {
        std::cout << T_RED "FAILED (unknown exception)" T_RESET "\n";
        g_tests_failed++;
    }
}

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        throw std::runtime_error("Assertion failed: " #a " == " #b " (at line " + std::to_string(__LINE__) + ")"); \
    } \
} while(0)

#define ASSERT_TRUE(c) do { \
    if (!(c)) { \
        throw std::runtime_error("Assertion failed: " #c " is true (at line " + std::to_string(__LINE__) + ")"); \
    } \
} while(0)

#define ASSERT_THROWS(expr, ex_type) do { \
    bool threw = false; \
    try { expr; } catch (const ex_type&) { threw = true; } catch (...) {} \
    if (!threw) { \
        throw std::runtime_error("Expected exception " #ex_type " not thrown for " #expr); \
    } \
} while(0)

// ═══════════════════════════════════════════════════════════════════════════
//  TEST SUITES
// ═══════════════════════════════════════════════════════════════════════════

void test_core_types() {
    utp::Transfer transfer;
    transfer.id = "utp_test_id";
    transfer.state = utp::TransferState::CREATED;
    transfer.size = 1048576;
    transfer.bytes_transferred = 0;

    ASSERT_EQ(transfer.id, "utp_test_id");
    ASSERT_EQ(transfer.state, utp::TransferState::CREATED);
    ASSERT_EQ(transfer.size, 1048576ULL);

    utp::TransferError err(utp::ErrorCategory::RATE_LIMIT, "429 Too Many Requests");
    ASSERT_EQ(err.category, utp::ErrorCategory::RATE_LIMIT);
    ASSERT_TRUE(std::string(err.what()).find("429") != std::string::npos);
}

void test_uri_parser_suite() {
    // Basic URI
    auto res1 = utp::parse_uri("gdrive://drive/path/to/file.txt");
    ASSERT_EQ(res1.scheme, "gdrive");
    ASSERT_EQ(res1.host_or_bucket, "drive");
    ASSERT_EQ(res1.path, "path/to/file.txt");

    // Local URI
    auto res2 = utp::parse_uri("local:///tmp/a.bin");
    ASSERT_EQ(res2.scheme, "local");
    ASSERT_EQ(res2.host_or_bucket, "");
    ASSERT_EQ(res2.path, "tmp/a.bin");

    // S3 Bucket and Key
    auto res3 = utp::parse_uri("s3://my-cloud-bucket/data/dataset.parquet");
    ASSERT_EQ(res3.scheme, "s3");
    ASSERT_EQ(res3.host_or_bucket, "my-cloud-bucket");
    ASSERT_EQ(res3.path, "data/dataset.parquet");

    // Account in Scheme
    auto res4 = utp::parse_uri("gdrive:my_work://folder/doc.pdf");
    ASSERT_EQ(res4.scheme, "gdrive");
    ASSERT_EQ(res4.account, "my_work");
    ASSERT_EQ(res4.host_or_bucket, "folder");
    ASSERT_EQ(res4.path, "doc.pdf");

    // Account with userinfo @
    auto res5 = utp::parse_uri("onedrive://personal@documents/file.docx");
    ASSERT_EQ(res5.scheme, "onedrive");
    ASSERT_EQ(res5.account, "personal");
    ASSERT_EQ(res5.host_or_bucket, "documents");
    ASSERT_EQ(res5.path, "file.docx");

    // Escaped spaces
    auto res6 = utp::parse_uri("s3://bucket_name-123/path%20with%20spaces/a.bin");
    ASSERT_EQ(res6.scheme, "s3");
    ASSERT_EQ(res6.host_or_bucket, "bucket_name-123");
    ASSERT_EQ(res6.path, "path%20with%20spaces/a.bin");

    // Malformed URIs
    ASSERT_THROWS(utp::parse_uri("/just/a/raw/path"), std::invalid_argument);
    ASSERT_THROWS(utp::parse_uri("://missing_scheme"), std::invalid_argument);
    ASSERT_THROWS(utp::parse_uri("invalid$scheme://bucket/file"), std::invalid_argument);
}

void test_mock_provider_suite() {
    utp::MockProvider provider;
    auto caps = provider.capabilities();
    ASSERT_TRUE(caps.ranged_read);
    ASSERT_EQ(caps.max_chunk_size, 8388608ULL);

    provider.add_file("archive.tar.gz", 2048576);
    auto info = provider.stat("archive.tar.gz");
    ASSERT_EQ(info.path, "archive.tar.gz");
    ASSERT_EQ(info.size, 2048576ULL);

    ASSERT_THROWS(provider.stat("nonexistent_file.bin"), std::runtime_error);

    auto r_stream = provider.openRead("archive.tar.gz");
    ASSERT_EQ(r_stream.id, "read_archive.tar.gz");

    auto w_session = provider.openWrite("output.bin", 2048576);
    ASSERT_EQ(w_session.id, "write_output.bin");

    uint8_t buffer[64];
    size_t n = provider.read(r_stream, 0, buffer, sizeof(buffer));
    ASSERT_EQ(n, sizeof(buffer));

    provider.write(w_session, 0, buffer, sizeof(buffer));
    provider.commit(w_session);
    provider.abort(w_session);
}

void test_chunk_planner_suite() {
    utp::ProviderCapabilities caps1{true, true, true, true, true, true, true, 8388608, 4};
    utp::ProviderCapabilities caps2{true, true, true, true, true, true, true, 8388608, 4};

    // Small file: 15 MB (< 20 MB -> 2.5 MB chunks)
    uint64_t small_size = 15ULL * 1024ULL * 1024ULL;
    auto plan_small = utp::ChunkPlanner::plan(small_size, caps1, caps2);
    ASSERT_TRUE(!plan_small.empty());
    uint64_t total_calc = 0;
    for (const auto& c : plan_small) {
        total_calc += c.size;
    }
    ASSERT_EQ(total_calc, small_size);

    // Large file: 100 MB (10.24 MB chunks)
    uint64_t large_size = 100ULL * 1024ULL * 1024ULL;
    auto plan_large = utp::ChunkPlanner::plan(large_size, caps1, caps2);
    total_calc = 0;
    for (const auto& c : plan_large) {
        total_calc += c.size;
    }
    ASSERT_EQ(total_calc, large_size);

    // Provider max_chunk_size limit enforcement
    utp::ProviderCapabilities limited_caps{true, true, true, true, true, true, true, 2621440, 4}; // 2.5MB cap
    auto plan_limited = utp::ChunkPlanner::plan(large_size, limited_caps, caps2);
    for (const auto& c : plan_limited) {
        ASSERT_TRUE(c.size <= 2621440);
    }
}

void test_state_manager_suite() {
    std::string test_db = "scratch_test_state.db";
    if (fs::exists(test_db)) fs::remove(test_db);

    {
        utp::StateManager sm(test_db);

        utp::Transfer t;
        t.id = "utp_eval_001";
        t.source = {"gdrive", "source_file_123"};
        t.destination = {"onedrive", "backup/file.zip"};
        t.size = 10485760;
        t.chunk_size = 2621440;
        t.bytes_transferred = 0;
        t.state = utp::TransferState::CREATED;

        sm.create_transfer(t);

        auto opt = sm.get_transfer("utp_eval_001");
        ASSERT_TRUE(opt.has_value());
        ASSERT_EQ(opt->id, "utp_eval_001");
        ASSERT_EQ(opt->size, 10485760ULL);
        ASSERT_EQ(opt->state, utp::TransferState::CREATED);

        sm.update_transfer_state("utp_eval_001", utp::TransferState::TRANSFERRING);
        sm.update_transfer_progress("utp_eval_001", 5242880);

        auto opt2 = sm.get_transfer("utp_eval_001");
        ASSERT_EQ(opt2->state, utp::TransferState::TRANSFERRING);
        ASSERT_EQ(opt2->bytes_transferred, 5242880ULL);

        std::vector<utp::ChunkInfo> plan = {
            {0, 2621440, utp::ChunkState::PENDING},
            {2621440, 2621440, utp::ChunkState::PENDING}
        };
        sm.save_plan("utp_eval_001", plan);
        auto retrieved_plan = sm.get_plan("utp_eval_001");
        ASSERT_EQ(retrieved_plan.size(), 2ULL);
        ASSERT_EQ(retrieved_plan[0].size, 2621440ULL);

        sm.update_chunk_state("utp_eval_001", 0, utp::ChunkState::COMPLETE);
        auto updated_plan = sm.get_plan("utp_eval_001");
        ASSERT_EQ(updated_plan[0].state, utp::ChunkState::COMPLETE);
        ASSERT_EQ(updated_plan[1].state, utp::ChunkState::PENDING);
    }

    std::error_code ec;
    fs::remove(test_db, ec);
}

void test_chaos_and_resilience_suite() {
    std::string test_db = "scratch_chaos_test.db";
    if (fs::exists(test_db)) { std::error_code ec; fs::remove(test_db, ec); }

    auto src = std::make_shared<utp::MockProvider>();
    auto dest = std::make_shared<utp::MockProvider>();

    // 20 MB payload
    uint64_t file_size = 20ULL * 1024ULL * 1024ULL;
    src->add_file("resilient_test_payload.bin", file_size);

    // Inject 25% transient failures (429 Rate Limits and Connection Timeouts)
    src->enable_chaos(25);
    dest->enable_chaos(25);

    uint64_t last_progress = 0;
    {
        utp::Engine engine(src, dest, test_db);

        std::string transfer_id = engine.transfer(
            "mock_source", "resilient_test_payload.bin",
            "mock_dest", "dest_resilient_payload.bin",
            [&last_progress](uint64_t bytes, uint64_t total) {
                last_progress = bytes;
                (void)total;
            }
        );

        ASSERT_TRUE(!transfer_id.empty());
        ASSERT_EQ(last_progress, file_size);
    }

    std::error_code ec;
    fs::remove(test_db, ec);
}

void test_performance_pipeline_suite() {
    std::string test_db = "scratch_perf_test.db";
    if (fs::exists(test_db)) { std::error_code ec; fs::remove(test_db, ec); }

    auto src = std::make_shared<utp::MockProvider>();
    auto dest = std::make_shared<utp::MockProvider>();

    // 100 MB benchmark payload
    uint64_t file_size = 100ULL * 1024ULL * 1024ULL;
    src->add_file("bench_100mb.dat", file_size);

    uint64_t streamed_bytes = 0;
    double mb_per_sec = 0;

    {
        utp::Engine engine(src, dest, test_db);

        auto t0 = std::chrono::high_resolution_clock::now();

        engine.transfer(
            "mock_src", "bench_100mb.dat",
            "mock_dst", "bench_100mb_out.dat",
            [&streamed_bytes](uint64_t b, uint64_t /*tot*/) {
                streamed_bytes = b;
            }
        );

        auto t1 = std::chrono::high_resolution_clock::now();
        double sec = std::chrono::duration<double>(t1 - t0).count();
        mb_per_sec = (file_size / (1024.0 * 1024.0)) / (sec > 0.0001 ? sec : 0.0001);
    }

    ASSERT_EQ(streamed_bytes, file_size);
    std::cout << T_MUTED "    [Throughput: " T_BOLD T_GREEN << static_cast<int>(mb_per_sec) << " MB/s" T_RESET T_MUTED " in-memory streaming speed]" T_RESET << " ";

    std::error_code ec;
    fs::remove(test_db, ec);
}

int main() {
    std::cout << "\n" T_BOLD T_CYAN "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║       Universal Cloud Transfer Protocol (UTP) Test Suite       ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝" T_RESET "\n\n";

    run_test("Core Types", "Enums & Structs Validation", test_core_types);
    run_test("URI Parser", "Standard, Cloud & Multi-Account URIs", test_uri_parser_suite);
    run_test("Mock Provider", "Capabilities, Stat & Sessions", test_mock_provider_suite);
    run_test("Chunk Planner", "Dynamic Adaptive Chunk Sizing & Limits", test_chunk_planner_suite);
    run_test("State Manager", "SQLite ACID Persistence & Chunk Progress", test_state_manager_suite);
    run_test("Chaos & Resilience", "25% Packet/Rate-Limit Drop Auto-Recovery", test_chaos_and_resilience_suite);
    run_test("Pipeline Performance", "100MB Zero-Disk In-Memory Streaming Throughput", test_performance_pipeline_suite);

    std::cout << "\n" T_BOLD "─── Evaluation Summary ───" T_RESET "\n";
    std::cout << "  Passed: " T_GREEN << g_tests_passed << T_RESET "\n";
    if (g_tests_failed > 0) {
        std::cout << "  Failed: " T_RED << g_tests_failed << T_RESET "\n\n";
        return 1;
    } else {
        std::cout << "  Failed: " T_MUTED "0" T_RESET "\n";
        std::cout << "  Result: " T_BOLD T_GREEN "ALL TESTS PASSED! Release Ready." T_RESET "\n\n";
        return 0;
    }
}
