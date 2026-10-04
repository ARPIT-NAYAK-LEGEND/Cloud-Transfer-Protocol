#include <gtest/gtest.h>
#include "utp/uri_parser.hpp"

using namespace utp;

TEST(UriParserTest, BasicUri) {
    auto res = parse_uri("gdrive://drive/path/to/file.txt");
    EXPECT_EQ(res.scheme, "gdrive");
    EXPECT_EQ(res.host_or_bucket, "drive");
    EXPECT_EQ(res.path, "path/to/file.txt");
}

TEST(UriParserTest, LocalUri) {
    auto res = parse_uri("local:///tmp/a.bin");
    EXPECT_EQ(res.scheme, "local");
    EXPECT_EQ(res.host_or_bucket, "");
    EXPECT_EQ(res.path, "tmp/a.bin");
}

TEST(UriParserTest, EmptyPath) {
    auto res = parse_uri("s3://my-bucket");
    EXPECT_EQ(res.scheme, "s3");
    EXPECT_EQ(res.host_or_bucket, "my-bucket");
    EXPECT_EQ(res.path, "");
}

TEST(UriParserTest, MissingScheme) {
    EXPECT_THROW(parse_uri("/path/to/file"), std::invalid_argument);
    EXPECT_THROW(parse_uri("://empty_scheme"), std::invalid_argument);
}

TEST(UriParserTest, UnusualCharacters) {
    auto res = parse_uri("s3://bucket_name-123/path%20with%20spaces/a.bin");
    EXPECT_EQ(res.scheme, "s3");
    EXPECT_EQ(res.host_or_bucket, "bucket_name-123");
    EXPECT_EQ(res.path, "path%20with%20spaces/a.bin");
}

TEST(UriParserTest, MalformedInput) {
    EXPECT_THROW(parse_uri("invalid_scheme!://bucket/path"), std::invalid_argument);
}

TEST(UriParserTest, TrailingSlash) {
    auto res = parse_uri("gdrive://drive/path/");
    EXPECT_EQ(res.scheme, "gdrive");
    EXPECT_EQ(res.host_or_bucket, "drive");
    EXPECT_EQ(res.path, "path/");
}
