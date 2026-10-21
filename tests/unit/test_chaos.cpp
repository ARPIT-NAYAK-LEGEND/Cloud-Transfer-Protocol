#include <gtest/gtest.h>
#include "utp/engine.hpp"
#include "utp/state_manager.hpp"
#include "../mock_provider.hpp"
#include <memory>

using namespace utp;

class ChaosTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize state manager table
        StateManager sm("utp_test.db");
    }
};

TEST_F(ChaosTest, ResilientTransferWithChaos) {
    auto src = std::make_unique<MockProvider>();
    auto dest = std::make_unique<MockProvider>();

    src->add_file("chaos_test.bin", 100 * 1024 * 1024); // 100MB file
    
    // 30% chance to fail ANY read or write operation with a transient error
    src->enable_chaos(30);
    dest->enable_chaos(30);

    Engine engine(std::move(src), std::move(dest), "utp_test.db");
    
    // The engine should retry automatically due to its exponential backoff logic.
    // If it succeeds without throwing a fatal error, the architecture works perfectly!
    EXPECT_NO_THROW({
        engine.transfer("mock", "chaos_test.bin", "mock", "dest_chaos.bin");
    });
}

// Bounded queue test case

// Large file integrity test case
