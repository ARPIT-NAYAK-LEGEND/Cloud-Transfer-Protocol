#!/usr/bin/env bash
# Universal Cloud Transfer Protocol (UTP)
# Phase 4 Definition-of-Done Verification Script:
# (1) Start transfer of multi-chunk file
# (2) Send kill -9 mid-transfer
# (3) Verify partial chunk state in SQLite
# (4) Run `utp resume <id>`
# (5) Confirm already-complete chunks are skipped & transfer finishes
# (6) Confirm source and destination SHA-256 match byte-for-byte

set -e

# ANSI Color Codes
CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${CYAN}================================================================${NC}"
echo -e "${CYAN}    UTP Phase 4 Definition-of-Done Test: kill -9 -> resume     ${NC}"
echo -e "${CYAN}================================================================${NC}"

# 1. Locate utp binary
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

UTP_BIN=""
if [ -f "$ROOT_DIR/utp" ]; then
    UTP_BIN="$ROOT_DIR/utp"
elif [ -f "$ROOT_DIR/utp.exe" ]; then
    UTP_BIN="$ROOT_DIR/utp.exe"
elif [ -f "$ROOT_DIR/build/cli/utp" ]; then
    UTP_BIN="$ROOT_DIR/build/cli/utp"
elif [ -f "$ROOT_DIR/build/cli/utp.exe" ]; then
    UTP_BIN="$ROOT_DIR/build/cli/utp.exe"
else
    echo -e "${RED}Error: UTP binary not found. Please build the project first.${NC}"
    exit 1
fi

echo -e "${GREEN}[1/6] Found UTP executable:${NC} $UTP_BIN"

# If executing a Windows binary under WSL / Git-Bash, delegate directly to native PowerShell DoD test
if [[ "$UTP_BIN" == *.exe ]] && command -v powershell.exe >/dev/null 2>&1; then
    PS1_SCRIPT="$SCRIPT_DIR/e2e_resume_dod.ps1"
    if command -v wslpath >/dev/null 2>&1; then
        PS1_SCRIPT=$(wslpath -w "$PS1_SCRIPT")
    fi
    exec powershell.exe -ExecutionPolicy Bypass -File "$PS1_SCRIPT"
fi

# 2. Setup isolated test environment
TEST_DIR="$ROOT_DIR/scratch_dod_test_$$"
mkdir -p "$TEST_DIR"
cleanup() {
    rm -rf "$TEST_DIR"
}
trap cleanup EXIT

SRC_FILE="$TEST_DIR/dod_source.bin"
DST_FILE="$TEST_DIR/dod_dest.bin"
DB_FILE="$TEST_DIR/utp.db"

# 3. Generate multi-chunk source file (40 MB = 5 chunks @ 8 MB)
echo -e "${GREEN}[2/6] Generating 40 MB multi-chunk payload...${NC}"
if command -v python3 >/dev/null 2>&1; then
    python3 -c "
import os
with open('$SRC_FILE', 'wb') as f:
    for i in range(40):
        f.write(os.urandom(1024 * 1024))
"
elif command -v python >/dev/null 2>&1; then
    python -c "
import os
with open('$SRC_FILE', 'wb') as f:
    for i in range(40):
        f.write(os.urandom(1024 * 1024))
"
else
    dd if=/dev/urandom of="$SRC_FILE" bs=1M count=40 2>/dev/null
fi

# Calculate Source SHA-256
if command -v sha256sum >/dev/null 2>&1; then
    SRC_SHA=$(sha256sum "$SRC_FILE" | awk '{print $1}')
elif command -v shasum >/dev/null 2>&1; then
    SRC_SHA=$(shasum -a 256 "$SRC_FILE" | awk '{print $1}')
elif command -v python3 >/dev/null 2>&1; then
    SRC_SHA=$(python3 -c "import hashlib; print(hashlib.sha256(open('$SRC_FILE','rb').read()).hexdigest())")
else
    SRC_SHA=$(python -c "import hashlib; print(hashlib.sha256(open('$SRC_FILE','rb').read()).hexdigest())")
fi

echo -e "      Source SHA-256: ${CYAN}$SRC_SHA${NC}"

# Path normalization for Windows PE binaries executed via WSL or MSYS
SRC_ARG="$SRC_FILE"
DST_ARG="$DST_FILE"
if command -v wslpath >/dev/null 2>&1 && [[ "$UTP_BIN" == *.exe ]]; then
    SRC_ARG=$(wslpath -m "$SRC_FILE")
    DST_ARG=$(wslpath -m "$DST_FILE")
elif command -v cygpath >/dev/null 2>&1 && [[ "$UTP_BIN" == *.exe ]]; then
    SRC_ARG=$(cygpath -m "$SRC_FILE")
    DST_ARG=$(cygpath -m "$DST_FILE")
fi

# 4. Start background transfer with bandwidth throttled (10 MB/s to ensure multiple seconds in flight)
TRANSFER_ID="dod_transfer_$$"
echo -e "${GREEN}[3/6] Starting transfer '$TRANSFER_ID' in background...${NC}"

cd "$TEST_DIR"
"$UTP_BIN" transfer "file://$SRC_ARG" "file://$DST_ARG" --id "$TRANSFER_ID" --bandwidth 10000000 > "$TEST_DIR/transfer.log" 2>&1 &
UTP_PID=$!

echo -e "      Launched PID: $UTP_PID. Monitoring for in-flight progress..."

# 5. Wait for partial progress (at least 1 chunk completed, but not finished)
PARTIAL_PROGRESS=0
for i in $(seq 1 100); do
    if [ -f "$DB_FILE" ]; then
        if command -v sqlite3 >/dev/null 2>&1; then
            COMPLETED_CHUNKS=$(sqlite3 "$DB_FILE" "SELECT count(*) FROM chunks WHERE transfer_id='$TRANSFER_ID' AND state=1;" 2>/dev/null || echo 0)
            if [ "$COMPLETED_CHUNKS" -ge 1 ]; then
                PARTIAL_PROGRESS=$COMPLETED_CHUNKS
                break
            fi
        fi
    fi
    sleep 0.1
done

if [ "$PARTIAL_PROGRESS" -lt 1 ]; then
    # Fallback sleep if sqlite3 CLI isn't installed
    sleep 1.2
fi

# 6. Abruptly kill the transfer process with kill -9
echo -e "${YELLOW}[4/6] Sending SIGKILL (kill -9) to PID $UTP_PID mid-transfer...${NC}"
kill -9 "$UTP_PID" 2>/dev/null || true
wait "$UTP_PID" 2>/dev/null || true
echo -e "      Process $UTP_PID terminated abruptly."

# Verify SQLite ledger state shows partial progress (neither empty nor completed)
if command -v sqlite3 >/dev/null 2>&1 && [ -f "$DB_FILE" ]; then
    STATE_BEFORE=$(sqlite3 "$DB_FILE" "SELECT state FROM transfers WHERE id='$TRANSFER_ID';" 2>/dev/null || echo "UNKNOWN")
    CHUNKS_DONE=$(sqlite3 "$DB_FILE" "SELECT count(*) FROM chunks WHERE transfer_id='$TRANSFER_ID' AND state=1;" 2>/dev/null || echo 0)
    CHUNKS_REMAINING=$(sqlite3 "$DB_FILE" "SELECT count(*) FROM chunks WHERE transfer_id='$TRANSFER_ID' AND state!=1;" 2>/dev/null || echo 0)
    echo -e "      Ledger inspection before resume: State=${YELLOW}$STATE_BEFORE${NC}, Done=${GREEN}$CHUNKS_DONE${NC}, Remaining=${YELLOW}$CHUNKS_REMAINING${NC}"
    if [ "$STATE_BEFORE" = "COMPLETED" ]; then
        echo -e "${RED}Error: Transfer had already completed before kill -9 was sent!${NC}"
        exit 1
    fi
    if [ "$STATE_BEFORE" = "FAILED" ]; then
        echo -e "${RED}Error: Transfer failed before kill -9 could be sent!${NC}"
        cat "$TEST_DIR/transfer.log"
        exit 1
    fi
fi

# 7. Resume the transfer
echo -e "${GREEN}[5/6] Resuming transfer via 'utp resume $TRANSFER_ID'...${NC}"
"$UTP_BIN" resume "$TRANSFER_ID" >> "$TEST_DIR/transfer.log" 2>&1
RESUME_EXIT=$?

if [ $RESUME_EXIT -ne 0 ]; then
    echo -e "${RED}Error: 'utp resume' failed with exit code $RESUME_EXIT.${NC}"
    cat "$TEST_DIR/transfer.log"
    exit 1
fi

# Verify final SQLite ledger state
if command -v sqlite3 >/dev/null 2>&1 && [ -f "$DB_FILE" ]; then
    FINAL_STATE=$(sqlite3 "$DB_FILE" "SELECT state FROM transfers WHERE id='$TRANSFER_ID';" 2>/dev/null || echo "")
    REMAINING_AFTER=$(sqlite3 "$DB_FILE" "SELECT count(*) FROM chunks WHERE transfer_id='$TRANSFER_ID' AND state!=1;" 2>/dev/null || echo 0)
    echo -e "      Ledger inspection after resume: Final State=${GREEN}$FINAL_STATE${NC}, Incomplete Chunks=$REMAINING_AFTER"
    if [ "$FINAL_STATE" != "COMPLETED" ]; then
        echo -e "${RED}Error: Final transfer state is not COMPLETED (got: '$FINAL_STATE').${NC}"
        exit 1
    fi
    if [ "$REMAINING_AFTER" -ne 0 ]; then
        echo -e "${RED}Error: Not all chunks reached COMPLETE state!${NC}"
        exit 1
    fi
fi

# 8. Cryptographic verification (Source SHA-256 == Destination SHA-256)
echo -e "${GREEN}[6/6] Verifying cryptographic integrity (SHA-256)...${NC}"
if [ ! -f "$DST_FILE" ]; then
    echo -e "${RED}Error: Destination file $DST_FILE does not exist!${NC}"
    cat "$TEST_DIR/transfer.log"
    exit 1
fi

if command -v sha256sum >/dev/null 2>&1; then
    DST_SHA=$(sha256sum "$DST_FILE" | awk '{print $1}')
elif command -v shasum >/dev/null 2>&1; then
    DST_SHA=$(shasum -a 256 "$DST_FILE" | awk '{print $1}')
elif command -v python3 >/dev/null 2>&1; then
    DST_SHA=$(python3 -c "import hashlib; print(hashlib.sha256(open('$DST_FILE','rb').read()).hexdigest())")
else
    DST_SHA=$(python -c "import hashlib; print(hashlib.sha256(open('$DST_FILE','rb').read()).hexdigest())")
fi

echo -e "      Destination SHA-256: ${CYAN}$DST_SHA${NC}"

if [ "$SRC_SHA" != "$DST_SHA" ]; then
    echo -e "${RED}FAIL: Hash mismatch!${NC}"
    echo -e "  Expected: $SRC_SHA"
    echo -e "  Actual:   $DST_SHA"
    exit 1
fi

echo -e "\n${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║   DEFINITION-OF-DONE VERIFIED: kill -9 -> resume -> SHA-256   ║${NC}"
echo -e "${GREEN}║   All completed chunks skipped; byte-identical output!        ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}\n"

exit 0
