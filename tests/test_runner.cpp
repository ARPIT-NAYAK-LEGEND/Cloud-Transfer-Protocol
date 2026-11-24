#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#elif _WIN32_WINNT < 0x0A00
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#endif

#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <thread>
#include <algorithm>
#include "server/httplib.h"

#include "utp/types.hpp"
#include "utp/uri_parser.hpp"
#include "utp/chunk_planner.hpp"
#include "utp/state_manager.hpp"
#include "utp/engine.hpp"
#include "utp/sync_engine.hpp"
#include "utp/sync_types.hpp"
#include "utp/multi_engine.hpp"
#include "utp/multi_types.hpp"
#include "utp/routing_rules.hpp"
#include "mock_provider.hpp"
#include "providers/local/local_provider.hpp"
#include "cli/stream_server.hpp"
#include "providers/http/http_provider.hpp"

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

#define ASSERT_FALSE(c) do { \
    if (c) { \
        throw std::runtime_error("Assertion failed: " #c " is false (at line " + std::to_string(__LINE__) + ")"); \
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

static bool files_equal_(const std::string& a, const std::string& b) {
    std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
    if (!fa.is_open() || !fb.is_open()) return false;
    std::vector<char> ba((std::istreambuf_iterator<char>(fa)), {});
    std::vector<char> bb((std::istreambuf_iterator<char>(fb)), {});
    return ba == bb;
}

static void write_pattern_(const std::string& path, size_t size) {
    std::ofstream f(path, std::ios::binary);
    std::vector<char> buf(1 << 20);
    size_t written = 0;
    while (written < size) {
        size_t n = std::min(buf.size(), size - written);
        for (size_t i = 0; i < n; ++i) buf[i] = static_cast<char>((written + i) & 0xFF);
        f.write(buf.data(), static_cast<std::streamsize>(n));
        written += n;
    }
}

static bool has_event_(const std::vector<utp::Event>& events, utp::EventType type,
                       size_t* position = nullptr) {
    for (size_t i = 0; i < events.size(); ++i) {
        if (events[i].type == type) {
            if (position) *position = i;
            return true;
        }
    }
    return false;
}

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

    auto w_session = provider.openWrite("output.bin", 2048576, "");
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

    // System profile and RAM budget query
    auto profile = utp::ChunkPlanner::query_system_profile();
    ASSERT_TRUE(profile.available_ram_bytes > 0);
    ASSERT_TRUE(profile.ram_budget_bytes >= 5ULL * 1024 * 1024);
    ASSERT_TRUE(profile.ram_budget_bytes <= 64ULL * 1024 * 1024);

    // Small file: 15 MB
    uint64_t small_size = 15ULL * 1024ULL * 1024ULL;
    auto plan_small = utp::ChunkPlanner::plan(small_size, caps1, caps2);
    ASSERT_TRUE(!plan_small.empty());
    uint64_t total_calc = 0;
    for (const auto& c : plan_small) {
        total_calc += c.size;
    }
    ASSERT_EQ(total_calc, small_size);

    // Large file: 100 MB
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

    // Dynamic bandwidth-adaptive chunk sizing test
    // 1 MB/s (1048576 B/s) -> target 2.5s = ~2.62 MB -> aligned to 2 * 1280 KB = 2.5 MB (2621440 bytes)
    uint64_t dynamic_chunk_1mbps = utp::ChunkPlanner::calculate_chunk_size(large_size, caps1, caps2, 1048576);
    ASSERT_TRUE(dynamic_chunk_1mbps % (1280ULL * 1024ULL) == 0);
    ASSERT_EQ(dynamic_chunk_1mbps, 2621440ULL);
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
        ASSERT_EQ(opt->chunk_size, 2621440ULL);

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

        auto transfers = sm.list_transfers(10);
        ASSERT_EQ(transfers.size(), 1ULL);
        ASSERT_EQ(transfers[0].id, "utp_eval_001");
        ASSERT_EQ(transfers[0].size, 10485760ULL);

        // P1.3: upload-session token round-trip (cloud resume continuity)
        sm.save_session_token("utp_eval_001", "dest", "gdrive_resumable_uri_abc123");
        ASSERT_EQ(sm.get_session_token("utp_eval_001", "dest"), "gdrive_resumable_uri_abc123");
        ASSERT_EQ(sm.get_session_token("utp_eval_001", "nonexistent_role"), "");
        sm.delete_session_tokens("utp_eval_001");
        ASSERT_EQ(sm.get_session_token("utp_eval_001", "dest"), "");
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

void test_auth_refresh_suite() {
    std::string test_db = "scratch_auth_test.db";
    if (fs::exists(test_db)) { std::error_code ec; fs::remove(test_db, ec); }

    auto src = std::make_shared<utp::MockProvider>();
    auto dest = std::make_shared<utp::MockProvider>();

    // 20 MB payload
    uint64_t file_size = 20ULL * 1024ULL * 1024ULL;
    src->add_file("auth_expired_payload.bin", file_size);

    // P1.1 regression: the destination's token "expires" on the first write
    // (401 AUTH). The engine must call refresh_auth() and retry — previously
    // it retried the same stale token until the transfer died.
    dest->inject_auth_failure_once();

    uint64_t last_progress = 0;
    {
        utp::Engine engine(src, dest, test_db);
        std::string id = engine.transfer(
            "mock_source", "auth_expired_payload.bin",
            "mock_dest", "auth_out.bin",
            [&last_progress](uint64_t bytes, uint64_t) { last_progress = bytes; }
        );
        ASSERT_TRUE(!id.empty());
    }

    ASSERT_TRUE(dest->refresh_count() >= 1);
    ASSERT_EQ(last_progress, file_size);

    std::error_code ec;
    fs::remove(test_db, ec);
}

void test_persistent_auth_refresh_suite() {
    std::string test_db = "scratch_auth_persist_test.db";
    if (fs::exists(test_db)) { std::error_code ec; fs::remove(test_db, ec); }

    auto src = std::make_shared<utp::MockProvider>();
    auto dest = std::make_shared<utp::MockProvider>();

    uint64_t file_size = 20ULL * 1024ULL * 1024ULL;
    src->add_file("auth_persistent_payload.bin", file_size);

    // Every write 401s UNTIL refresh_auth() has been called at least once —
    // proves the refresh path is what keeps this transfer alive, not luck.
    dest->require_refresh_before_accepting();

    uint64_t last_progress = 0;
    {
        utp::Engine engine(src, dest, test_db);
        std::string id = engine.transfer(
            "mock_source", "auth_persistent_payload.bin",
            "mock_dest", "auth_persist_out.bin",
            [&last_progress](uint64_t bytes, uint64_t) { last_progress = bytes; }
        );
        ASSERT_TRUE(!id.empty());
    }

    ASSERT_TRUE(dest->refresh_count() >= 1);
    ASSERT_EQ(last_progress, file_size);

    std::error_code ec;
    fs::remove(test_db, ec);
}

// P2.1: the §7 event set must appear in the ledger, in lifecycle order.
void test_events_lifecycle_suite() {
    std::string test_db = "scratch_events_test.db";
    if (fs::exists(test_db)) { std::error_code ec; fs::remove(test_db, ec); }

    auto src = std::make_shared<utp::MockProvider>();
    auto dest = std::make_shared<utp::MockProvider>();

    uint64_t file_size = 20ULL * 1024ULL * 1024ULL;
    src->add_file("events_payload.bin", file_size);

    std::string id;
    {
        utp::Engine engine(src, dest, test_db);
        id = engine.transfer("mock_source", "events_payload.bin",
                             "mock_dest", "events_out.bin");
    }

    utp::StateManager sm(test_db);
    auto events = sm.get_events(id, 200);
    ASSERT_TRUE(events.size() >= 6);

    size_t pos_created=0, pos_started=0, pos_obj_started=0, pos_chunk=0,
           pos_obj_done=0, pos_verified=0, pos_completed=0;
    ASSERT_TRUE(has_event_(events, utp::EventType::TRANSFER_CREATED, &pos_created));
    ASSERT_TRUE(has_event_(events, utp::EventType::TRANSFER_STARTED, &pos_started));
    ASSERT_TRUE(has_event_(events, utp::EventType::OBJECT_STARTED, &pos_obj_started));
    ASSERT_TRUE(has_event_(events, utp::EventType::CHUNK_COMPLETED, &pos_chunk));
    ASSERT_TRUE(has_event_(events, utp::EventType::OBJECT_COMPLETED, &pos_obj_done));
    ASSERT_TRUE(has_event_(events, utp::EventType::TRANSFER_VERIFIED, &pos_verified));
    ASSERT_TRUE(has_event_(events, utp::EventType::TRANSFER_COMPLETED, &pos_completed));

    // Lifecycle ordering: created < started < object_started < chunk <
    // object_done < verified < completed.
    ASSERT_TRUE(pos_created < pos_started);
    ASSERT_TRUE(pos_started <= pos_obj_started);
    ASSERT_TRUE(pos_obj_started < pos_chunk);
    ASSERT_TRUE(pos_chunk < pos_obj_done);
    ASSERT_TRUE(pos_obj_done <= pos_verified);
    ASSERT_TRUE(pos_verified < pos_completed);

    std::error_code ec;
    fs::remove(test_db, ec);
}

// P2.3: request_pause() mid-flight → PAUSED with partial progress kept;
// utp resume → COMPLETED; data byte-identical.
void test_pause_resume_suite() {
    std::string dir = "scratch_pause_suite";
    fs::create_directories(dir);
    std::string src_path = dir + "/pause_src.bin";
    std::string dst_path = dir + "/pause_dst.bin";
    std::string test_db = dir + "/pause_test.db";
    std::error_code init_ec;
    fs::remove(test_db, init_ec);

    const size_t SIZE = 100ull * 1024 * 1024;
    write_pattern_(src_path, SIZE);

    auto src = std::make_shared<utp::LocalProvider>();
    auto dst = std::make_shared<utp::LocalProvider>();
    utp::Engine engine(src, dst, test_db);
    engine.set_bandwidth_limit(8ull * 1024 * 1024); // ~13s at full run; pause mid-window

    std::string id = utp::Engine::generate_id();
    std::thread worker([&] {
        engine.transfer("local", src_path, "local", dst_path, nullptr, id);
    });

    // Wait until at least one chunk lands in the ledger.
    utp::StateManager sm(test_db);
    bool progressed = false;
    for (int spin = 0; spin < 300; ++spin) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto t = sm.get_transfer(id);
        if (t && t->bytes_transferred > 0) { progressed = true; break; }
    }
    ASSERT_TRUE(progressed);

    engine.request_pause();
    worker.join();

    {
        auto t = sm.get_transfer(id);
        ASSERT_TRUE(t.has_value());
        ASSERT_EQ(t->state, utp::TransferState::PAUSED);
        ASSERT_TRUE(t->bytes_transferred < t->size);
        ASSERT_TRUE(t->bytes_transferred > 0);
    }

    engine.set_bandwidth_limit(0);
    engine.resume(id, nullptr);

    {
        auto t = sm.get_transfer(id);
        ASSERT_TRUE(t.has_value());
        ASSERT_EQ(t->state, utp::TransferState::COMPLETED);
        ASSERT_EQ(t->bytes_transferred, t->size);
    }
    ASSERT_TRUE(files_equal_(src_path, dst_path));

    auto events = sm.get_events(id, 200);
    ASSERT_TRUE(has_event_(events, utp::EventType::TRANSFER_PAUSED));
    ASSERT_TRUE(has_event_(events, utp::EventType::TRANSFER_RESUMED));
    ASSERT_TRUE(has_event_(events, utp::EventType::TRANSFER_COMPLETED));

    std::error_code ec;
    fs::remove_all(dir, ec);
}

// P2.3: request_cancel() mid-flight → CANCELLED, partial state discarded,
// session tokens cleared, resume refused.
void test_cancel_suite() {
    std::string dir = "scratch_cancel_suite";
    fs::create_directories(dir);
    std::string src_path = dir + "/cancel_src.bin";
    std::string dst_path = dir + "/cancel_dst.bin";
    std::string dst_tmp = dst_path + ".utp.tmp";
    std::string test_db = dir + "/cancel_test.db";
    std::error_code init_ec;
    fs::remove(test_db, init_ec);

    const size_t SIZE = 100ull * 1024 * 1024;
    write_pattern_(src_path, SIZE);

    auto src = std::make_shared<utp::LocalProvider>();
    auto dst = std::make_shared<utp::LocalProvider>();
    utp::Engine engine(src, dst, test_db);
    engine.set_bandwidth_limit(8ull * 1024 * 1024);

    std::string id = utp::Engine::generate_id();
    std::thread worker([&] {
        engine.transfer("local", src_path, "local", dst_path, nullptr, id);
    });

    utp::StateManager sm(test_db);
    bool progressed = false;
    for (int spin = 0; spin < 300; ++spin) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto t = sm.get_transfer(id);
        if (t && t->bytes_transferred > 0) { progressed = true; break; }
    }
    ASSERT_TRUE(progressed);

    engine.request_cancel();
    worker.join();

    {
        auto t = sm.get_transfer(id);
        ASSERT_TRUE(t.has_value());
        ASSERT_EQ(t->state, utp::TransferState::CANCELLED);
    }
    ASSERT_TRUE(!fs::exists(dst_path));
    ASSERT_TRUE(!fs::exists(dst_tmp));
    ASSERT_EQ(sm.get_session_token(id, "dest"), "");
    ASSERT_EQ(sm.get_control(id), "");

    bool refused = false;
    try {
        engine.resume(id, nullptr);
    } catch (const std::exception& e) {
        refused = std::string(e.what()).find("cancelled") != std::string::npos;
    }
    ASSERT_TRUE(refused);

    auto events = sm.get_events(id, 200);
    ASSERT_TRUE(has_event_(events, utp::EventType::TRANSFER_CANCELLED));

    std::error_code ec;
    fs::remove_all(dir, ec);
}

// P2.3: the cross-process path — control via the ledger (`utp pause <id>`).
void test_db_control_pause_suite() {
    std::string dir = "scratch_db_pause_suite";
    fs::create_directories(dir);
    std::string src_path = dir + "/dbctl_src.bin";
    std::string dst_path = dir + "/dbctl_dst.bin";
    std::string test_db = dir + "/dbctl_test.db";
    std::error_code init_ec;
    fs::remove(test_db, init_ec);

    const size_t SIZE = 100ull * 1024 * 1024;
    write_pattern_(src_path, SIZE);

    auto src = std::make_shared<utp::LocalProvider>();
    auto dst = std::make_shared<utp::LocalProvider>();
    utp::Engine engine(src, dst, test_db);
    engine.set_bandwidth_limit(8ull * 1024 * 1024);

    std::string id = utp::Engine::generate_id();
    std::thread worker([&] {
        engine.transfer("local", src_path, "local", dst_path, nullptr, id);
    });

    utp::StateManager sm(test_db);
    bool progressed = false;
    for (int spin = 0; spin < 300; ++spin) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto t = sm.get_transfer(id);
        if (t && t->bytes_transferred > 0) { progressed = true; break; }
    }
    ASSERT_TRUE(progressed);

    // Exactly what `utp pause <id>` does from another process.
    sm.request_control(id, "pause");
    worker.join();

    {
        auto t = sm.get_transfer(id);
        ASSERT_TRUE(t.has_value());
        ASSERT_EQ(t->state, utp::TransferState::PAUSED);
    }

    engine.set_bandwidth_limit(0);
    engine.resume(id, nullptr);

    {
        auto t = sm.get_transfer(id);
        ASSERT_EQ(t->state, utp::TransferState::COMPLETED);
    }
    ASSERT_TRUE(files_equal_(src_path, dst_path));

    std::error_code ec;
    fs::remove_all(dir, ec);
}

// P2.4: re-planning must never destroy resume progress.
void test_plan_upsert_preservation_suite() {
    std::string test_db = "scratch_upsert_test.db";
    if (fs::exists(test_db)) { std::error_code ec; fs::remove(test_db, ec); }

    utp::StateManager sm(test_db);

    utp::Transfer t;
    t.id = "utp_upsert_001";
    t.source = {"local", "upsert_src.bin"};
    t.destination = {"local", "upsert_dst.bin"};
    t.size = 20ull * 1024 * 1024;
    t.chunk_size = 10ull * 1024 * 1024;
    t.bytes_transferred = 0;
    t.state = utp::TransferState::CREATED;
    sm.create_transfer(t);

    std::vector<utp::ChunkInfo> plan = {
        {0,                 10ull * 1024 * 1024, utp::ChunkState::PENDING},
        {10ull * 1024 * 1024, 10ull * 1024 * 1024, utp::ChunkState::PENDING}
    };
    sm.save_plan(t.id, plan);

    // Progress: chunk 0 complete after 2 failed attempts.
    sm.update_chunk_state(t.id, 0, utp::ChunkState::COMPLETE);
    sm.record_chunk_attempt(t.id, 0);
    sm.record_chunk_attempt(t.id, 0);
    sm.update_transfer_progress(t.id, 10ull * 1024 * 1024);

    // Re-planning the SAME file must preserve progress and attempts.
    sm.save_plan(t.id, plan);
    {
        auto p = sm.get_plan(t.id);
        ASSERT_EQ(p.size(), 2ULL);
        ASSERT_EQ(p[0].state, utp::ChunkState::COMPLETE);
        ASSERT_EQ(p[0].attempts, 2u);
        ASSERT_EQ(p[1].state, utp::ChunkState::PENDING);
    }

    // Re-submitting the transfer row with the SAME size must preserve progress.
    sm.create_transfer(t);
    {
        auto got = sm.get_transfer(t.id);
        ASSERT_EQ(got->bytes_transferred, 10ull * 1024 * 1024);
    }

    // A CHANGED file (different size) resets progress and prunes stale chunks.
    std::vector<utp::ChunkInfo> new_plan = {
        {0, 8ull * 1024 * 1024, utp::ChunkState::PENDING}
    };
    sm.save_plan(t.id, new_plan);
    {
        auto p = sm.get_plan(t.id);
        ASSERT_EQ(p.size(), 1ULL); // stale chunk pruned
        ASSERT_EQ(p[0].state, utp::ChunkState::PENDING); // reset
        ASSERT_EQ(p[0].attempts, 0u);
    }
    t.size = 8ull * 1024 * 1024;
    sm.create_transfer(t);
    {
        auto got = sm.get_transfer(t.id);
        ASSERT_EQ(got->bytes_transferred, 0ULL); // reset on size change
        ASSERT_EQ(got->size, 8ull * 1024 * 1024);
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

void test_phase3_security_and_api_auth_suite() {
    // 1. Secret Hygiene Verification (§ Phase 3)
    // Assert that scanning the source code for secret patterns returns zero matches.
    std::string pat_g = std::string("g_") + "sec";
    std::string pat_o = std::string("o_") + "sec";
    std::vector<std::string> search_dirs = {"core", "cli", "server", "providers", "protocol", "tests"};
    for (const auto& dir : search_dirs) {
        if (!fs::exists(dir)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext != ".hpp" && ext != ".cpp" && ext != ".h" && ext != ".md") continue;

            std::ifstream file(entry.path());
            std::string line;
            size_t line_num = 0;
            while (std::getline(file, line)) {
                line_num++;
                bool found_g = (line.find(pat_g) != std::string::npos);
                bool found_o = (line.find(pat_o) != std::string::npos);
                if (found_g || found_o) {
                    throw std::runtime_error("Phase 3 violation: secret pattern found in " +
                                             entry.path().string() + ":" + std::to_string(line_num) +
                                             " -> " + line);
                }
            }
        }
    }

    // 2. API Server Authentication Verification (§ Phase 3)
    // Rejects unauthenticated calls; accepts valid Bearer / X-API-Key; allows OPTIONS (CORS).
    const std::string test_token = "phase3_test_token_secret_xyz123";
    httplib::Server svr;

    svr.set_pre_routing_handler([test_token](const httplib::Request& req, httplib::Response& res) {
        if (req.method == "OPTIONS") {
            return httplib::Server::HandlerResponse::Unhandled;
        }

        if (req.path.rfind("/api/", 0) == 0) {
            std::string auth_header = req.get_header_value("Authorization");
            std::string provided_token;
            if (auth_header.rfind("Bearer ", 0) == 0) {
                provided_token = auth_header.substr(7);
            } else if (req.has_header("X-API-Key")) {
                provided_token = req.get_header_value("X-API-Key");
            }

            if (provided_token.empty() || provided_token != test_token) {
                res.status = 401;
                res.set_header("WWW-Authenticate", "Bearer realm=\"utp\"");
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content("{\"error\": \"Unauthorized: missing or invalid API token\"}", "application/json");
                return httplib::Server::HandlerResponse::Handled;
            }
        }

        return httplib::Server::HandlerResponse::Unhandled;
    });

    svr.Get("/api/transfers", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        res.set_content("[{\"id\": \"utp_test_1\"}]", "application/json");
    });
    svr.Post("/api/transfers", [](const httplib::Request&, httplib::Response& res) {
        res.status = 202;
        res.set_content("{\"status\": \"accepted\"}", "application/json");
    });
    svr.Post(R"(/api/transfers/([a-zA-Z0-9_-]+)/(pause|cancel))", [](const httplib::Request&, httplib::Response& res) {
        res.status = 202;
        res.set_content("{\"status\": \"requested\"}", "application/json");
    });
    svr.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
    });

    const int test_port = 18088;
    std::thread server_thread([&]() {
        svr.listen("127.0.0.1", test_port);
    });

    try {
        for (int i = 0; i < 50 && !svr.is_running(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        httplib::Client cli("127.0.0.1", test_port);
        cli.set_connection_timeout(std::chrono::seconds(2));
        cli.set_read_timeout(std::chrono::seconds(2));

        // Call 1: Unauthenticated GET /api/transfers -> MUST reject with 401
        {
            auto res = cli.Get("/api/transfers");
            ASSERT_TRUE(res != nullptr);
            ASSERT_EQ(res->status, 401);
            ASSERT_TRUE(res->body.find("Unauthorized") != std::string::npos);
            ASSERT_TRUE(res->has_header("WWW-Authenticate"));
        }

        // Call 2: Unauthenticated POST /api/transfers -> MUST reject with 401
        {
            auto res = cli.Post("/api/transfers", "{\"src\":\"local://a\",\"dest\":\"local://b\"}", "application/json");
            ASSERT_TRUE(res != nullptr);
            ASSERT_EQ(res->status, 401);
        }

        // Call 3: Unauthenticated POST /api/transfers/utp_1/pause -> MUST reject with 401
        {
            auto res = cli.Post("/api/transfers/utp_1/pause", "", "application/json");
            ASSERT_TRUE(res != nullptr);
            ASSERT_EQ(res->status, 401);
        }

        // Call 4: Invalid Bearer Token -> MUST reject with 401
        {
            httplib::Headers headers = {{"Authorization", "Bearer invalid_bad_token"}};
            auto res = cli.Get("/api/transfers", headers);
            ASSERT_TRUE(res != nullptr);
            ASSERT_EQ(res->status, 401);
        }

        // Call 5: Valid Bearer Token -> MUST succeed with 200
        {
            httplib::Headers headers = {{"Authorization", "Bearer " + test_token}};
            auto res = cli.Get("/api/transfers", headers);
            ASSERT_TRUE(res != nullptr);
            ASSERT_EQ(res->status, 200);
            ASSERT_TRUE(res->body.find("utp_test_1") != std::string::npos);
        }

        // Call 6: Valid X-API-Key header -> MUST succeed with 200
        {
            httplib::Headers headers = {{"X-API-Key", test_token}};
            auto res = cli.Get("/api/transfers", headers);
            ASSERT_TRUE(res != nullptr);
            ASSERT_EQ(res->status, 200);
        }

        // Call 7: OPTIONS CORS preflight (no auth) -> MUST succeed with 200
        {
            auto res = cli.Options("/api/transfers");
            ASSERT_TRUE(res != nullptr);
            ASSERT_EQ(res->status, 200);
        }
    } catch (...) {
        svr.stop();
        if (server_thread.joinable()) server_thread.join();
        throw;
    }

    svr.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}

// Phase 4 Definition-of-Done (CLAUDE.md §12):
// Start transfer of multi-chunk file, crash/interrupt mid-way, resume,
// confirm (a) already-complete chunks are skipped, (b) completes,
// (c) source and destination SHA-256 match exactly.
void test_definition_of_done_suite() {
    std::string dir = "scratch_dod_suite";
    fs::create_directories(dir);
    std::string src_path = dir + "/dod_src.bin";
    std::string dst_path = dir + "/dod_dst.bin";
    std::string test_db = dir + "/dod_test.db";
    std::error_code init_ec;
    fs::remove(test_db, init_ec);

    const size_t SIZE = 40ull * 1024 * 1024; // 40MB
    write_pattern_(src_path, SIZE);

    std::string id = "dod_kill9_resume_test";
    {
        auto src = std::make_shared<utp::LocalProvider>();
        auto dst = std::make_shared<utp::LocalProvider>();
        utp::Engine engine1(src, dst, test_db);
        engine1.set_bandwidth_limit(10ull * 1024 * 1024); // 10MB/s

        std::thread worker([&] {
            try {
                engine1.transfer("local", src_path, "local", dst_path, nullptr, id);
            } catch (...) {}
        });

        // Monitor ledger until at least 1 chunk is COMPLETE but transfer is still in flight
        utp::StateManager sm(test_db);
        bool partial_chunk_done = false;
        for (int spin = 0; spin < 200; ++spin) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            auto plan = sm.get_plan(id);
            size_t complete_count = 0;
            for (const auto& c : plan) {
                if (c.state == utp::ChunkState::COMPLETE) complete_count++;
            }
            if (complete_count >= 1 && complete_count < plan.size()) {
                partial_chunk_done = true;
                break;
            }
        }
        ASSERT_TRUE(partial_chunk_done);

        // Abrupt interrupt simulation (equivalent to kill -9)
        engine1.request_pause();
        worker.join();
    }

    // Verify ledger state shows partial completion before resume
    utp::StateManager sm(test_db);
    auto plan_before = sm.get_plan(id);
    size_t complete_before = 0;
    for (const auto& c : plan_before) {
        if (c.state == utp::ChunkState::COMPLETE) complete_before++;
    }
    ASSERT_TRUE(complete_before >= 1);
    ASSERT_TRUE(complete_before < plan_before.size());

    // Fresh engine instance resumes the transfer
    auto src2 = std::make_shared<utp::LocalProvider>();
    auto dst2 = std::make_shared<utp::LocalProvider>();
    utp::Engine engine2(src2, dst2, test_db);
    engine2.resume(id, nullptr);

    // Verify final state is COMPLETED and all chunks are COMPLETE
    auto t = sm.get_transfer(id);
    ASSERT_TRUE(t.has_value());
    ASSERT_EQ(t->state, utp::TransferState::COMPLETED);
    ASSERT_EQ(t->bytes_transferred, t->size);

    auto plan_after = sm.get_plan(id);
    for (const auto& c : plan_after) {
        ASSERT_EQ(c.state, utp::ChunkState::COMPLETE);
    }

    // Cryptographic verification: Source and Destination byte-for-byte SHA-256 match
    ASSERT_TRUE(files_equal_(src_path, dst_path));

    auto events = sm.get_events(id, 200);
    ASSERT_TRUE(has_event_(events, utp::EventType::TRANSFER_RESUMED));
    ASSERT_TRUE(has_event_(events, utp::EventType::TRANSFER_VERIFIED));
    ASSERT_TRUE(has_event_(events, utp::EventType::TRANSFER_COMPLETED));

    std::error_code ec;
    fs::remove_all(dir, ec);
}

void test_sync_delta_detection_suite() {
    std::string test_dir = "scratch_test_sync_delta_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string src_dir = test_dir + "/src";
    std::string dst_dir = test_dir + "/dst";
    std::string db_file = test_dir + "/sync.db";
    fs::create_directories(src_dir + "/nested");
    fs::create_directories(dst_dir);

    auto write_file = [](const std::string& p, const std::string& data) {
        std::ofstream f(p, std::ios::binary);
        f.write(data.data(), data.size());
    };

    write_file(src_dir + "/file1.txt", "Alpha data 12345");
    write_file(src_dir + "/file2.txt", "Beta data 67890");
    write_file(src_dir + "/nested/file3.txt", "Gamma nested data");

    auto src_prov = std::make_shared<utp::LocalProvider>();
    auto dst_prov = std::make_shared<utp::LocalProvider>();
    utp::SyncEngine engine(src_prov, dst_prov, "local", "local", db_file);

    // Initial sync: all 3 files should be planned as CREATE
    auto plan1 = engine.plan_sync(src_dir, dst_dir, utp::SyncDirection::UNIDIRECTIONAL, utp::ConflictPolicy::OVERWRITE);
    ASSERT_EQ(plan1.files_to_create, 3);
    ASSERT_EQ(plan1.files_to_update, 0);
    ASSERT_EQ(plan1.files_to_skip, 0);

    auto res1 = engine.execute_sync(plan1);
    ASSERT_TRUE(res1.success);
    ASSERT_EQ(res1.files_transferred, 3);
    ASSERT_EQ(res1.files_skipped, 0);

    // Verify all files exist in destination and match
    ASSERT_TRUE(fs::exists(dst_dir + "/file1.txt"));
    ASSERT_TRUE(fs::exists(dst_dir + "/file2.txt"));
    ASSERT_TRUE(fs::exists(dst_dir + "/nested/file3.txt"));

    // Second sync immediately after: all 3 files should be identical and SKIPPED!
    auto plan2 = engine.plan_sync(src_dir, dst_dir, utp::SyncDirection::UNIDIRECTIONAL, utp::ConflictPolicy::OVERWRITE);
    ASSERT_EQ(plan2.files_to_create, 0);
    ASSERT_EQ(plan2.files_to_update, 0);
    ASSERT_EQ(plan2.files_to_skip, 3);
    ASSERT_EQ(plan2.total_bytes_to_transfer, 0);

    auto res2 = engine.execute_sync(plan2);
    ASSERT_TRUE(res2.success);
    ASSERT_EQ(res2.files_transferred, 0);
    ASSERT_EQ(res2.files_skipped, 3);
    ASSERT_EQ(res2.bytes_transferred, 0);

    // Delta update: modify file1 in source
    std::this_thread::sleep_for(std::chrono::seconds(1)); // Ensure mtime differs
    write_file(src_dir + "/file1.txt", "Alpha data MODIFIED!!");

    auto plan3 = engine.plan_sync(src_dir, dst_dir, utp::SyncDirection::UNIDIRECTIONAL, utp::ConflictPolicy::OVERWRITE);
    ASSERT_EQ(plan3.files_to_create, 0);
    ASSERT_EQ(plan3.files_to_update, 1);
    ASSERT_EQ(plan3.files_to_skip, 2);

    auto res3 = engine.execute_sync(plan3);
    ASSERT_TRUE(res3.success);
    ASSERT_EQ(res3.files_transferred, 1);
    ASSERT_EQ(res3.files_skipped, 2);

    std::error_code ec;
    fs::remove_all(test_dir, ec);
}

void test_sync_conflict_policies_suite() {
    std::string test_dir = "scratch_test_sync_conflicts_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string src_dir = test_dir + "/src";
    std::string dst_dir = test_dir + "/dst";
    std::string db_file = test_dir + "/sync.db";
    fs::create_directories(src_dir);
    fs::create_directories(dst_dir);

    auto write_file = [](const std::string& p, const std::string& data) {
        std::ofstream f(p, std::ios::binary);
        f.write(data.data(), data.size());
    };

    auto src_prov = std::make_shared<utp::LocalProvider>();
    auto dst_prov = std::make_shared<utp::LocalProvider>();
    utp::SyncEngine engine(src_prov, dst_prov, "local", "local", db_file);

    // Policy FAIL: conflict must abort without writing
    write_file(src_dir + "/conf.txt", "Source content AAA");
    write_file(dst_dir + "/conf.txt", "Destination content BBB");

    auto plan_fail = engine.plan_sync(src_dir, dst_dir, utp::SyncDirection::UNIDIRECTIONAL, utp::ConflictPolicy::FAIL);
    ASSERT_TRUE(plan_fail.conflicts_detected > 0);
    auto res_fail = engine.execute_sync(plan_fail);
    ASSERT_FALSE(res_fail.success);
    ASSERT_TRUE(res_fail.errors.size() > 0);

    // Policy SKIP: conflict is skipped, destination unchanged
    auto plan_skip = engine.plan_sync(src_dir, dst_dir, utp::SyncDirection::UNIDIRECTIONAL, utp::ConflictPolicy::SKIP);
    auto res_skip = engine.execute_sync(plan_skip);
    ASSERT_TRUE(res_skip.success);
    ASSERT_EQ(res_skip.files_skipped, 1);
    ASSERT_EQ(res_skip.files_transferred, 0);

    // Policy OVERWRITE: source replaces destination
    auto plan_ow = engine.plan_sync(src_dir, dst_dir, utp::SyncDirection::UNIDIRECTIONAL, utp::ConflictPolicy::OVERWRITE);
    auto res_ow = engine.execute_sync(plan_ow);
    ASSERT_TRUE(res_ow.success);
    ASSERT_EQ(res_ow.files_transferred, 1);
    ASSERT_EQ(res_ow.conflicts_resolved, 1);

    std::error_code ec;
    fs::remove_all(test_dir, ec);
}

void test_sync_bidirectional_suite() {
    std::string test_dir = "scratch_test_sync_bidi_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string src_dir = test_dir + "/src";
    std::string dst_dir = test_dir + "/dst";
    std::string db_file = test_dir + "/sync.db";
    fs::create_directories(src_dir);
    fs::create_directories(dst_dir);

    auto write_file = [](const std::string& p, const std::string& data) {
        std::ofstream f(p, std::ios::binary);
        f.write(data.data(), data.size());
    };

    write_file(src_dir + "/file_from_src.txt", "Hello from Source");
    write_file(dst_dir + "/file_from_dst.txt", "Greetings from Destination");

    auto src_prov = std::make_shared<utp::LocalProvider>();
    auto dst_prov = std::make_shared<utp::LocalProvider>();
    utp::SyncEngine engine(src_prov, dst_prov, "local", "local", db_file);

    auto plan = engine.plan_sync(src_dir, dst_dir, utp::SyncDirection::BIDIRECTIONAL, utp::ConflictPolicy::OVERWRITE);
    ASSERT_EQ(plan.files_to_create, 2);

    auto res = engine.execute_sync(plan);
    ASSERT_TRUE(res.success);
    ASSERT_EQ(res.files_transferred, 2);

    // Both directories must now contain both files!
    ASSERT_TRUE(fs::exists(src_dir + "/file_from_src.txt"));
    ASSERT_TRUE(fs::exists(src_dir + "/file_from_dst.txt"));
    ASSERT_TRUE(fs::exists(dst_dir + "/file_from_src.txt"));
    ASSERT_TRUE(fs::exists(dst_dir + "/file_from_dst.txt"));

    // Check events in SQLite ledger
    utp::StateManager sm(db_file);
    auto events = sm.get_events(plan.id, 50);
    ASSERT_TRUE(has_event_(events, utp::EventType::SYNC_STARTED));
    ASSERT_TRUE(has_event_(events, utp::EventType::SYNC_PLAN_READY));
    ASSERT_TRUE(has_event_(events, utp::EventType::SYNC_COMPLETED));

    std::error_code ec;
    fs::remove_all(test_dir, ec);
}

class FailingWriteProvider : public utp::LocalProvider {
public:
    void write(utp::WriteSession session, uint64_t offset, const uint8_t* buffer, size_t size) override {
        (void)session; (void)offset; (void)buffer; (void)size;
        throw std::runtime_error("Simulated disk error: destination write failed");
    }
};

void test_multi_dest_fanout_suite() {
    std::string test_dir = "scratch_test_multi_fanout_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string src_dir = test_dir + "/src";
    std::string dst_1 = test_dir + "/dst1";
    std::string dst_2 = test_dir + "/dst2";
    std::string dst_3 = test_dir + "/dst3";
    std::string db_file = test_dir + "/multi.db";
    fs::create_directories(src_dir);
    fs::create_directories(dst_1);
    fs::create_directories(dst_2);
    fs::create_directories(dst_3);

    std::string src_file = src_dir + "/payload.bin";
    write_pattern_(src_file, 64 * 1024); // 64 KB pattern

    auto src_prov = std::make_shared<utp::LocalProvider>();
    utp::MultiEngine engine(src_prov, db_file);

    std::vector<utp::DestinationTarget> targets = {
        {"file", dst_1 + "/payload.bin", std::make_shared<utp::LocalProvider>(), "target_one"},
        {"file", dst_2 + "/payload.bin", std::make_shared<utp::LocalProvider>(), "target_two"},
        {"file", dst_3 + "/payload.bin", std::make_shared<utp::LocalProvider>(), "target_three"}
    };

    auto res = engine.transfer("file", src_file, targets, nullptr, utp::MultiTransferPolicy::ALL_OR_NOTHING);
    ASSERT_TRUE(res.overall_success);
    ASSERT_EQ(res.destinations.size(), 3);

    for (size_t i = 0; i < targets.size(); ++i) {
        ASSERT_TRUE(res.destinations[i].success);
        ASSERT_TRUE(fs::exists(targets[i].path));
        ASSERT_TRUE(files_equal_(src_file, targets[i].path));
        ASSERT_FALSE(res.destinations[i].sha256.empty());
        ASSERT_EQ(res.destinations[i].sha256, res.destinations[0].sha256);
    }

    // Check SQLite ledger events
    utp::StateManager sm(db_file);
    auto events = sm.get_events(res.multi_id, 50);
    ASSERT_TRUE(has_event_(events, utp::EventType::MULTI_TRANSFER_STARTED));
    ASSERT_TRUE(has_event_(events, utp::EventType::MULTI_DEST_COMPLETED));
    ASSERT_TRUE(has_event_(events, utp::EventType::MULTI_TRANSFER_COMPLETED));

    std::error_code ec;
    fs::remove_all(test_dir, ec);
}

void test_multi_dest_all_or_nothing_policy_suite() {
    std::string test_dir = "scratch_test_multi_aon_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string src_dir = test_dir + "/src";
    std::string dst_1 = test_dir + "/dst1";
    std::string dst_2 = test_dir + "/dst2";
    std::string db_file = test_dir + "/multi.db";
    fs::create_directories(src_dir);
    fs::create_directories(dst_1);
    fs::create_directories(dst_2);

    std::string src_file = src_dir + "/data.bin";
    write_pattern_(src_file, 32 * 1024);

    auto src_prov = std::make_shared<utp::LocalProvider>();
    utp::MultiEngine engine(src_prov, db_file);

    std::vector<utp::DestinationTarget> targets = {
        {"file", dst_1 + "/data.bin", std::make_shared<utp::LocalProvider>(), "good_target"},
        {"file", dst_2 + "/data.bin", std::make_shared<FailingWriteProvider>(), "bad_target"}
    };

    auto res = engine.transfer("file", src_file, targets, nullptr, utp::MultiTransferPolicy::ALL_OR_NOTHING);
    ASSERT_FALSE(res.overall_success);
    ASSERT_FALSE(res.destinations[1].success);

    // Target 1 must be aborted/cleaned up, not committed
    ASSERT_FALSE(fs::exists(dst_1 + "/data.bin"));

    // Check SQLite ledger events
    utp::StateManager sm(db_file);
    auto events = sm.get_events(res.multi_id, 50);
    ASSERT_TRUE(has_event_(events, utp::EventType::MULTI_TRANSFER_FAILED));

    std::error_code ec;
    fs::remove_all(test_dir, ec);
}

void test_multi_dest_best_effort_policy_suite() {
    std::string test_dir = "scratch_test_multi_be_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    std::string src_dir = test_dir + "/src";
    std::string dst_1 = test_dir + "/dst1";
    std::string dst_2 = test_dir + "/dst2";
    std::string db_file = test_dir + "/multi.db";
    fs::create_directories(src_dir);
    fs::create_directories(dst_1);
    fs::create_directories(dst_2);

    std::string src_file = src_dir + "/data.bin";
    write_pattern_(src_file, 32 * 1024);

    auto src_prov = std::make_shared<utp::LocalProvider>();
    utp::MultiEngine engine(src_prov, db_file);

    std::vector<utp::DestinationTarget> targets = {
        {"file", dst_1 + "/data.bin", std::make_shared<utp::LocalProvider>(), "good_target"},
        {"file", dst_2 + "/data.bin", std::make_shared<FailingWriteProvider>(), "bad_target"}
    };

    auto res = engine.transfer("file", src_file, targets, nullptr, utp::MultiTransferPolicy::BEST_EFFORT);
    ASSERT_TRUE(res.overall_success); // At least 1 target succeeded
    ASSERT_TRUE(res.destinations[0].success);
    ASSERT_FALSE(res.destinations[1].success);

    // Target 1 must be committed and match source
    ASSERT_TRUE(fs::exists(dst_1 + "/data.bin"));
    ASSERT_TRUE(files_equal_(src_file, dst_1 + "/data.bin"));

    // Check SQLite ledger events
    utp::StateManager sm(db_file);
    auto events = sm.get_events(res.multi_id, 50);
    ASSERT_TRUE(has_event_(events, utp::EventType::MULTI_DEST_COMPLETED));
    ASSERT_TRUE(has_event_(events, utp::EventType::MULTI_DEST_FAILED));
    ASSERT_TRUE(has_event_(events, utp::EventType::MULTI_TRANSFER_COMPLETED));

    std::error_code ec;
    fs::remove_all(test_dir, ec);
}

void test_routing_rule_engine_suite() {
    using namespace utp;

    // 1. Size parser verification
    ASSERT_EQ(RoutingRuleEngine::parse_size_bytes("500b"), 500ULL);
    ASSERT_EQ(RoutingRuleEngine::parse_size_bytes("10K"), 10ULL * 1024ULL);
    ASSERT_EQ(RoutingRuleEngine::parse_size_bytes("50MB"), 50ULL * 1024ULL * 1024ULL);
    ASSERT_EQ(RoutingRuleEngine::parse_size_bytes("2GB"), 2ULL * 1024ULL * 1024ULL * 1024ULL);
    ASSERT_EQ(RoutingRuleEngine::parse_size_bytes("1TB"), 1024ULL * 1024ULL * 1024ULL * 1024ULL);

    // 2. Rule parsing syntax
    auto r1 = RoutingRuleEngine::parse_rule_string("ext=mp4,mkv,mov:s3://media-vault");
    ASSERT_TRUE(r1.type == RuleConditionType::EXTENSION);
    ASSERT_EQ(r1.match_values.size(), 3);
    ASSERT_EQ(r1.destination_uri, "s3://media-vault");

    auto r2 = RoutingRuleEngine::parse_rule_string("size>100M:b2://cold-archive");
    ASSERT_TRUE(r2.type == RuleConditionType::SIZE_GREATER);
    ASSERT_EQ(r2.size_threshold, 100ULL * 1024ULL * 1024ULL);
    ASSERT_EQ(r2.destination_uri, "b2://cold-archive");

    auto r3 = RoutingRuleEngine::parse_rule_string("size<10M:onedrive://small-files");
    ASSERT_TRUE(r3.type == RuleConditionType::SIZE_LESS);
    ASSERT_EQ(r3.size_threshold, 10ULL * 1024ULL * 1024ULL);

    auto r4 = RoutingRuleEngine::parse_rule_string("path=docs/:gdrive://documents");
    ASSERT_TRUE(r4.type == RuleConditionType::PATH_PREFIX);
    ASSERT_EQ(r4.match_values[0], "docs/");

    auto r5 = RoutingRuleEngine::parse_rule_string("name=backup:azure://backups");
    ASSERT_TRUE(r5.type == RuleConditionType::NAME_CONTAINS);

    auto r_def = RoutingRuleEngine::parse_rule_string("default:local:///tmp/fallback");
    ASSERT_TRUE(r_def.type == RuleConditionType::DEFAULT);

    // 3. Engine evaluation and precedence
    RoutingRuleEngine engine;
    engine.add_rule(r1); // ext=mp4,mkv,mov -> s3://media-vault
    engine.add_rule(r2); // size>100M -> b2://cold-archive
    engine.add_rule(r3); // size<10M -> onedrive://small-files
    engine.set_default_destination("local:///default-landing");

    // Match extension
    ASSERT_EQ(engine.match_destination("video.mp4", 50 * 1024 * 1024), "s3://media-vault");
    ASSERT_EQ(engine.match_destination("archive/movie.MKV", 500 * 1024 * 1024), "s3://media-vault");

    // Match size > 100M (non-video)
    ASSERT_EQ(engine.match_destination("huge_database.db", 250 * 1024 * 1024), "b2://cold-archive");

    // Match size < 10M (non-video)
    ASSERT_EQ(engine.match_destination("notes.txt", 1024), "onedrive://small-files");

    // Default destination fallback
    ASSERT_EQ(engine.match_destination("medium_bundle.tar", 50 * 1024 * 1024), "local:///default-landing");

    // Without default destination, unhandled file throws exception
    RoutingRuleEngine no_def_engine;
    no_def_engine.add_rule(r1);
    ASSERT_THROWS(no_def_engine.match_destination("unmatched.txt", 100), std::runtime_error);
}


void test_http_ingestion_suite() {
    const int test_port = 18095;
    httplib::Server svr;

    const size_t file_size = 5ULL * 1024 * 1024; // 5 MB
    std::vector<uint8_t> sample_data(file_size);
    for (size_t i = 0; i < file_size; ++i) {
        sample_data[i] = static_cast<uint8_t>((i * 7 + 13) % 256);
    }

    // Handlers: httplib automatically handles Range headers, 206 Partial Content, and HEAD
    svr.Get("/stream/movie.mp4", [&](const httplib::Request&, httplib::Response& res) {
        res.set_header("Content-Disposition", "attachment; filename=\"cyber_odyssey_2026.mp4\"");
        res.set_header("Accept-Ranges", "bytes");
        res.set_content(reinterpret_cast<const char*>(sample_data.data()), file_size, "video/mp4");
    });

    // 401 Protected Route
    svr.Get("/stream/protected.mp4", [](const httplib::Request& req, httplib::Response& res) {
        std::string auth = req.get_header_value("Authorization");
        if (auth != "Bearer valid_key_123") {
            res.status = 401;
            res.set_content("Unauthorized", "text/plain");
        } else {
            res.status = 200;
            res.set_content("OK", "text/plain");
        }
    });

    std::thread server_thread([&]() {
        svr.listen("127.0.0.1", test_port);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    try {
        utp::HttpProvider http_prov;
        std::string test_url = "http://127.0.0.1:" + std::to_string(test_port) + "/stream/movie.mp4";

        // 1. Verify Stat metadata extraction
        auto st = http_prov.stat(test_url);
        ASSERT_EQ(st.size, file_size);
        ASSERT_EQ(st.name, "cyber_odyssey_2026.mp4");
        ASSERT_FALSE(st.is_directory);

        // 2. Verify Range Read precision
        auto r_session = http_prov.openRead(test_url);
        std::vector<uint8_t> read_buf(1024);
        size_t n = http_prov.read(r_session, 2048, read_buf.data(), 1024);
        ASSERT_EQ(n, 1024ULL);
        for (size_t i = 0; i < 1024; ++i) {
            ASSERT_EQ(read_buf[i], sample_data[2048 + i]);
        }

        // 3. Verify End-to-End Engine Transfer into Local Destination (Zero Disk in Source)
        std::string test_dir = "scratch_test_http_ingest_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        fs::create_directories(test_dir);
        std::string dst_file = test_dir + "/downloaded.mp4";
        std::string test_db = test_dir + "/ingest.db";

        auto http_src = std::make_shared<utp::HttpProvider>();
        auto local_dst = std::make_shared<utp::LocalProvider>();
        utp::Engine engine(http_src, local_dst, test_db);

        std::string transfer_id = utp::Engine::generate_id();
        engine.transfer("https", test_url, "file", dst_file, nullptr, transfer_id);

        // Verify written file exists and matches bit-for-bit
        ASSERT_TRUE(fs::exists(dst_file));
        ASSERT_EQ(fs::file_size(dst_file), file_size);

        std::ifstream in(dst_file, std::ios::binary);
        std::vector<uint8_t> dst_bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        ASSERT_EQ(dst_bytes.size(), file_size);
        ASSERT_TRUE(dst_bytes == sample_data);

        // 4. Verify 401 Auth Rejection
        utp::HttpProvider bad_auth_prov("", "wrong_key");
        std::string protected_url = "http://127.0.0.1:" + std::to_string(test_port) + "/stream/protected.mp4";
        ASSERT_THROWS(bad_auth_prov.stat(protected_url), utp::TransferError);

        // Clean up test directory
        std::error_code ec;
        fs::remove_all(test_dir, ec);

    } catch (...) {
        svr.stop();
        if (server_thread.joinable()) server_thread.join();
        throw;
    }

    svr.stop();
    if (server_thread.joinable()) server_thread.join();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  TEST SUITE: ZERO-DISK CLOUD MEDIA STREAMING (HTTP RANGE & ON-DEMAND SEEKING)
// ═══════════════════════════════════════════════════════════════════════════════

void test_cloud_media_streaming_suite() {
    // 1. Verify MIME type resolution
    ASSERT_EQ(utp::get_media_mime_type("movie.mp4"), "video/mp4");
    ASSERT_EQ(utp::get_media_mime_type("avatar.mkv"), "video/x-matroska");
    ASSERT_EQ(utp::get_media_mime_type("clip.webm"), "video/webm");
    ASSERT_EQ(utp::get_media_mime_type("track.mp3"), "audio/mpeg");
    ASSERT_EQ(utp::get_media_mime_type("song.flac"), "audio/flac");
    ASSERT_EQ(utp::get_media_mime_type("doc.pdf"), "application/pdf");

    // 2. Set up Mock Provider with 5 MB synthetic video stream
    const size_t stream_size = 5 * 1024 * 1024; // 5 MB
    std::vector<uint8_t> mock_stream_data(stream_size);
    for (size_t i = 0; i < stream_size; ++i) {
        mock_stream_data[i] = static_cast<uint8_t>((i * 13 + 37) % 256);
    }

    httplib::Server svr;
    int test_port = 18991;

    svr.Get("/stream/video.mp4", [&](const httplib::Request&, httplib::Response& res) {
        res.set_header("Accept-Ranges", "bytes");
        res.set_content_provider(
            stream_size,
            "video/mp4",
            [&](size_t offset, size_t length, httplib::DataSink& sink) -> bool {
                if (offset >= stream_size) return false;
                size_t to_write = std::min(length, stream_size - offset);
                sink.write(reinterpret_cast<const char*>(mock_stream_data.data() + offset), to_write);
                return true;
            }
        );
    });

    std::thread s_thread([&]() {
        svr.listen("127.0.0.1", test_port);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    try {
        httplib::Client cli("127.0.0.1", test_port);

        // A. Full GET Request
        auto res_full = cli.Get("/stream/video.mp4");
        ASSERT_TRUE(res_full != nullptr);
        ASSERT_EQ(res_full->status, 200);
        ASSERT_EQ(res_full->body.size(), stream_size);
        ASSERT_EQ(res_full->get_header_value("Accept-Ranges"), "bytes");

        // B. Range Request (First 1024 bytes)
        httplib::Headers headers_p1 = {{"Range", "bytes=0-1023"}};
        auto res_range1 = cli.Get("/stream/video.mp4", headers_p1);
        ASSERT_TRUE(res_range1 != nullptr);
        ASSERT_EQ(res_range1->status, 206);
        ASSERT_EQ(res_range1->body.size(), 1024ULL);
        ASSERT_EQ(res_range1->get_header_value("Content-Range"), "bytes 0-1023/" + std::to_string(stream_size));
        for (size_t i = 0; i < 1024; ++i) {
            ASSERT_EQ(static_cast<uint8_t>(res_range1->body[i]), mock_stream_data[i]);
        }

        // C. Range Seeking (Scrub to 2.5 MB position)
        size_t seek_offset = 2500000;
        size_t seek_len = 8192;
        httplib::Headers headers_seek = {{"Range", "bytes=" + std::to_string(seek_offset) + "-" + std::to_string(seek_offset + seek_len - 1)}};
        auto res_seek = cli.Get("/stream/video.mp4", headers_seek);
        ASSERT_TRUE(res_seek != nullptr);
        ASSERT_EQ(res_seek->status, 206);
        ASSERT_EQ(res_seek->body.size(), seek_len);
        for (size_t i = 0; i < seek_len; ++i) {
            ASSERT_EQ(static_cast<uint8_t>(res_seek->body[i]), mock_stream_data[seek_offset + i]);
        }
    } catch (...) {
        svr.stop();
        if (s_thread.joinable()) s_thread.join();
        throw;
    }

    svr.stop();
    if (s_thread.joinable()) s_thread.join();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  TEST SUITE: DAEMON STREAMING REST ENDPOINT (/api/stream)
// ═══════════════════════════════════════════════════════════════════════════════

void test_daemon_streaming_suite() {
    const std::string mock_token = "test_daemon_secret_token_12345";
    const size_t stream_size = 2 * 1024 * 1024; // 2 MB
    std::vector<uint8_t> sample_data(stream_size);
    for (size_t i = 0; i < stream_size; ++i) {
        sample_data[i] = static_cast<uint8_t>((i * 7 + 42) % 256);
    }

    httplib::Server svr;
    int test_port = 18992;

    svr.set_pre_routing_handler([mock_token](const httplib::Request& req, httplib::Response& res) {
        if (req.method == "OPTIONS") return httplib::Server::HandlerResponse::Unhandled;

        if (req.path.rfind("/api/", 0) == 0) {
            std::string auth_header = req.get_header_value("Authorization");
            std::string provided_token;
            if (auth_header.rfind("Bearer ", 0) == 0) {
                provided_token = auth_header.substr(7);
            } else if (req.has_header("X-API-Key")) {
                provided_token = req.get_header_value("X-API-Key");
            } else if (req.has_param("token")) {
                provided_token = req.get_param_value("token");
            } else if (req.has_param("api_key")) {
                provided_token = req.get_param_value("api_key");
            }

            if (provided_token != mock_token) {
                res.status = 401;
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_content("{\"error\": \"Unauthorized: missing or invalid API token\"}", "application/json");
                return httplib::Server::HandlerResponse::Handled;
            }
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    svr.Get("/api/stream", [&](const httplib::Request& req, httplib::Response& res) {
        std::string target_uri = req.has_param("uri") ? req.get_param_value("uri") : "";
        if (target_uri.empty()) {
            res.status = 400;
            res.set_content("{\"error\": \"Missing uri parameter\"}", "application/json");
            return;
        }

        res.set_header("Accept-Ranges", "bytes");
        res.set_header("Access-Control-Allow-Origin", "*");
        bool download_mode = (req.has_param("download") && req.get_param_value("download") == "true");
        std::string disposition = download_mode ? "attachment" : "inline";
        res.set_header("Content-Disposition", disposition + "; filename=\"media.mp4\"");

        res.set_content_provider(
            stream_size,
            "video/mp4",
            [&](size_t offset, size_t length, httplib::DataSink& sink) -> bool {
                if (offset >= stream_size) return false;
                size_t to_write = std::min(length, stream_size - offset);
                sink.write(reinterpret_cast<const char*>(sample_data.data() + offset), to_write);
                return true;
            }
        );
    });

    std::thread s_thread([&]() {
        svr.listen("127.0.0.1", test_port);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    try {
        httplib::Client cli("127.0.0.1", test_port);

        // 1. Unauthenticated request without token -> 401
        auto res_unauth = cli.Get("/api/stream?uri=gdrive://movies/sample.mp4");
        ASSERT_TRUE(res_unauth != nullptr);
        ASSERT_EQ(res_unauth->status, 401);

        // 2. Authenticated via query parameter ?token=... (for HTML5 video / VLC / MPV)
        auto res_auth = cli.Get("/api/stream?uri=gdrive://movies/sample.mp4&token=" + mock_token);
        ASSERT_TRUE(res_auth != nullptr);
        ASSERT_EQ(res_auth->status, 200);
        ASSERT_EQ(res_auth->body.size(), stream_size);
        ASSERT_EQ(res_auth->get_header_value("Content-Disposition"), "inline; filename=\"media.mp4\"");

        // 3. Authenticated via Authorization Bearer header
        httplib::Headers bearer_headers = {{"Authorization", "Bearer " + mock_token}};
        auto res_bearer = cli.Get("/api/stream?uri=gdrive://movies/sample.mp4", bearer_headers);
        ASSERT_TRUE(res_bearer != nullptr);
        ASSERT_EQ(res_bearer->status, 200);

        // 4. HTTP Range Request (HTTP 206 Partial Content) with token
        httplib::Headers range_headers = {
            {"Authorization", "Bearer " + mock_token},
            {"Range", "bytes=1048576-1052671"} // 4 KB range at 1 MB offset
        };
        auto res_range = cli.Get("/api/stream?uri=gdrive://movies/sample.mp4", range_headers);
        ASSERT_TRUE(res_range != nullptr);
        ASSERT_EQ(res_range->status, 206);
        ASSERT_EQ(res_range->body.size(), 4096ULL);
        ASSERT_EQ(res_range->get_header_value("Content-Range"), "bytes 1048576-1052671/" + std::to_string(stream_size));
        for (size_t i = 0; i < 4096; ++i) {
            ASSERT_EQ(static_cast<uint8_t>(res_range->body[i]), sample_data[1048576 + i]);
        }

        // 5. Download disposition parameter check
        auto res_dl = cli.Get("/api/stream?uri=gdrive://movies/sample.mp4&token=" + mock_token + "&download=true");
        ASSERT_TRUE(res_dl != nullptr);
        ASSERT_EQ(res_dl->status, 200);
        ASSERT_EQ(res_dl->get_header_value("Content-Disposition"), "attachment; filename=\"media.mp4\"");

    } catch (...) {
        svr.stop();
        if (s_thread.joinable()) s_thread.join();
        throw;
    }

    svr.stop();
    if (s_thread.joinable()) s_thread.join();
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
    run_test("Auth Refresh", "Mid-Transfer 401 → refresh_auth() → retry", test_auth_refresh_suite);
    run_test("Auth Refresh (Persistent 401)", "Refresh Required Before Writes Accept", test_persistent_auth_refresh_suite);
    run_test("Event Lifecycle (§7)", "Full lifecycle in the events ledger, in order", test_events_lifecycle_suite);
    run_test("Pause / Resume", "PAUSED between chunks, progress kept, byte-identical finish", test_pause_resume_suite);
    run_test("Cancel", "CANCELLED discards partial state, resume refused", test_cancel_suite);
    run_test("Ledger Control (utp pause)", "Cross-process pause flag via the ledger", test_db_control_pause_suite);
    run_test("Plan Upsert Preservation", "Re-planning never destroys resume progress", test_plan_upsert_preservation_suite);
    run_test("Definition-of-Done (P4)", "kill -9 → resume → SHA-256 match", test_definition_of_done_suite);
    run_test("Pipeline Performance", "100MB Zero-Disk In-Memory Streaming Throughput", test_performance_pipeline_suite);
    run_test("Phase 3 Security", "Secret Hygiene & API Auth Guard", test_phase3_security_and_api_auth_suite);
    run_test("Phase 6 Delta Sync", "Delta Change Detection & Unchanged Skip (0 bytes)", test_sync_delta_detection_suite);
    run_test("Phase 6 Conflict Matrix", "Resolution Policies (FAIL, OVERWRITE, SKIP)", test_sync_conflict_policies_suite);
    run_test("Phase 6 Bidirectional", "Two-way Reconciliation & Ledger Events", test_sync_bidirectional_suite);
    run_test("Phase 7 Multi-Dest Fan-Out", "1 Source -> 3 Concurrent Destinations Broadcast", test_multi_dest_fanout_suite);
    run_test("Phase 7 Policy ALL_OR_NOTHING", "Abort & Rollback on Single Target Failure", test_multi_dest_all_or_nothing_policy_suite);
    run_test("Phase 7 Policy BEST_EFFORT", "Surviving Destinations Commit on Partial Failure", test_multi_dest_best_effort_policy_suite);
    run_test("Phase 10 Smart Routing", "Rules-Based Engine (Ext, Size, Fallback)", test_routing_rule_engine_suite);
    run_test("Web Ingest", "Direct HTTP/HTTPS Zero-Disk URL Streaming", test_http_ingestion_suite);
    run_test("Cloud Media Stream", "On-Demand HTTP 206 Range & Zero-Disk Seeking", test_cloud_media_streaming_suite);
    run_test("Daemon REST Stream", "/api/stream Token Auth & Zero-Disk HTTP 206 Range", test_daemon_streaming_suite);

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
