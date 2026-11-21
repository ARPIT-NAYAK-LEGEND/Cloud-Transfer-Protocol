# Contributing to Cloud Transfer Protocol (CTP / UTP)

First off, thank you for considering contributing to the **Universal Cloud Transfer Protocol (UTP)**! Open-source contributions are what make developer tools resilient, secure, and universally adaptable.

Please review this document to understand the codebase architecture, development setup, coding standards, and pull request workflow.

---

## 📜 Code of Conduct

This project and everyone participating in it is governed by our [Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code. Please report unacceptable behavior to `security@arpitnayak.tech`.

---

## 🏗️ Architectural Invariants (Must Follow)

Every contribution must preserve UTP's core design principles:

1. **Zero-Disk Invariant**:
   - Data streamed between clouds **must never touch local physical storage** (HDD/SSD/NVMe).
   - Use volatile in-memory circular ring buffers (`RingBuffer` in `core/include/utp/engine.hpp`).
   - Temporary scratch files or payload disk caches are strictly prohibited.
2. **End-to-End Cryptographic Security**:
   - All network connections must enforce TLS 1.3 encryption.
   - Dual in-flight checksums (SHA-256 + MD5) must be computed in memory as chunks flow through the buffer.
   - Credentials, OAuth tokens, and refresh tokens must remain strictly local on the user's filesystem (`~/.utp_*`). **Zero telemetry or remote logging.**
3. **Crash Resiliency (ACID Ledger)**:
   - Every 8 MB / 16 MB chunk commitment boundary must be recorded in the local SQLite WAL ledger (`utp.db`).
   - If interrupted (`kill -9`, network disconnection, power failure), `utp resume <id>` must resume deterministically from the last uncommitted chunk.

---

## 🛠️ Development Setup & Prerequisites

### Required Toolchain
- **C++20 Compliant Compiler**:
  - **Linux**: GCC 12+ or Clang 15+
  - **Windows**: MinGW-w64 (GCC 13+) or MSVC 2022+
  - **macOS**: Apple Clang 15+ or Homebrew GCC 13+
- **System Libraries**:
  - `libcurl` (with OpenSSL / TLS 1.3 support)
  - `OpenSSL` (libssl & libcrypto)
  - `SQLite3` (libsqlite3)
- **Node.js**: Node 18+ (for `npm/` CLI wrapper and `web/` landing page)

### Installing Dependencies

**Ubuntu / Debian:**
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libcurl4-openssl-dev libssl-dev libsqlite3-dev sqlite3
```

**macOS (Homebrew):**
```bash
brew install cmake curl openssl@3 sqlite
```

**Windows (MSYS2 / MinGW-w64):**
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-curl mingw-w64-x86_64-openssl mingw-w64-x86_64-sqlite3
```

---

## 🚀 Building & Testing

### 1. Build the Standalone CLI Binary
```bash
# On Linux / macOS:
chmod +x build.sh
./build.sh

# On Windows:
.\build_cli.bat
```
This produces an optimized standalone executable:
- `./utp` (Linux / macOS) or `utp.exe` (Windows)

### 2. Run the Full Test Battery
Run the 23-suite automated test suite (including mock cloud providers, URI parsers, SQLite WAL state recovery, and multi-cloud fan-out):
```bash
# On Linux / macOS:
g++ -std=c++20 -O3 -I core/include -I . \
  tests/test_runner.cpp \
  core/src/chunk_planner.cpp core/src/engine.cpp core/src/state_manager.cpp core/src/uri_parser.cpp \
  core/src/sync_engine.cpp core/src/multi_engine.cpp core/src/routing_rules.cpp \
  providers/common/http_client.cpp providers/local/local_provider.cpp \
  -o test_runner -lcurl -lssl -lcrypto -lpthread -lsqlite3
./test_runner

# On Windows:
.\build_tests.bat
.\test_runner.exe
```

### 3. Run the Definition-of-Done (DoD) Crash-Resume Test
Verifies that interrupting a transfer (`kill -9`) resumes seamlessly without corrupting data:
```bash
./tests/e2e_resume_dod.sh        # Linux / macOS
powershell -ExecutionPolicy Bypass -File .\tests\e2e_resume_dod.ps1  # Windows
```

---

## 🔌 Adding a New Storage Provider

To add support for a new cloud provider (e.g. Box, MEGA, SFTP):

1. **Implement the Provider Interface:**
   Create a new class inheriting from `StorageProvider` in [`providers/interface/storage_provider.hpp`](providers/interface/storage_provider.hpp):
   ```cpp
   class MyCloudProvider : public StorageProvider {
   public:
       ResourceMetadata get_metadata(const Resource& resource) override;
       std::vector<ResourceMetadata> list_directory(const Resource& resource) override;
       StreamSession open_read_stream(const Resource& resource, uint64_t offset, uint64_t length) override;
       StreamSession open_write_stream(const Resource& resource, uint64_t total_size) override;
       bool commit_chunk(StreamSession& session, const uint8_t* data, size_t size, uint64_t offset) override;
       bool finalize_stream(StreamSession& session) override;
   };
   ```
2. **Register URI Scheme:**
   Add the provider URI scheme to [`core/src/uri_parser.cpp`](core/src/uri_parser.cpp) and the factory in [`cli/main.cpp`](cli/main.cpp).
3. **Add Tests:**
   Add unit tests in `tests/unit/` verifying range reads, stream commits, and error handling.

---

## 📬 Pull Request Workflow

1. **Fork the repo** and create a descriptive branch:
   ```bash
   git checkout -b feat/add-box-provider
   ```
2. **Adhere to Code Style:**
   - C++20 standard
   - RAII for all resource management (smart pointers, no raw `malloc`/`free`)
   - Descriptive variable names and no hardcoded personal credentials or secrets
3. **Verify Tests Pass:**
   Ensure all 23 suites in `test_runner` pass before opening your PR.
4. **Submit Pull Request:**
   - Fill out the [Pull Request Template](.github/PULL_REQUEST_TEMPLATE.md).
   - Link any related GitHub issues.
   - CI will automatically run compiler checks, secret hygiene scans, and crash-resume gates.

Thank you for helping build the future of zero-disk cloud data pipelines!
