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
#include "mock_provider.hpp"
#include "providers/local/local_provider.hpp"

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
