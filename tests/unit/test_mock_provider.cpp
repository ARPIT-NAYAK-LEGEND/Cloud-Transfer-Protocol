#include <gtest/gtest.h>
#include "../mock_provider.hpp"

using namespace utp;

TEST(MockProviderTest, Capabilities) {
    MockProvider provider;
    auto caps = provider.capabilities();
    EXPECT_TRUE(caps.ranged_read);
    EXPECT_EQ(caps.max_chunk_size, 8388608);
}

TEST(MockProviderTest, Stat) {
    MockProvider provider;
    provider.add_file("test.txt", 1024);
    
    auto info = provider.stat("test.txt");
    EXPECT_EQ(info.size, 1024);
    EXPECT_EQ(info.path, "test.txt");

    EXPECT_THROW(provider.stat("nonexistent.txt"), std::runtime_error);
}

TEST(MockProviderTest, OpenSessions) {
    MockProvider provider;
    auto read_stream = provider.openRead("test.txt");
    EXPECT_EQ(read_stream.id, "read_test.txt");

    auto write_stream = provider.openWrite("test.txt", 1024);
    EXPECT_EQ(write_stream.id, "write_test.txt");

    // Commit and abort should not throw
    EXPECT_NO_THROW(provider.commit(write_stream));
    EXPECT_NO_THROW(provider.abort(write_stream));
}
