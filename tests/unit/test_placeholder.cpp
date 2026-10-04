#include <gtest/gtest.h>
#include "utp/types.hpp"

TEST(UTPCoreTypes, EnumsAndStructsCompile) {
    utp::Transfer transfer;
    transfer.id = "utp_123";
    transfer.state = utp::TransferState::CREATED;
    transfer.size = 1024;
    transfer.bytes_transferred = 0;
    
    EXPECT_EQ(transfer.id, "utp_123");
    EXPECT_EQ(transfer.state, utp::TransferState::CREATED);
    EXPECT_EQ(transfer.size, 1024);
}
