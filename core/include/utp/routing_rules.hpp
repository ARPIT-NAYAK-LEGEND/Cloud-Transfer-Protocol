#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace utp {

enum class RuleConditionType {
    EXTENSION,      // e.g. ext=pdf,docx
    SIZE_GREATER,   // e.g. size>100M
    SIZE_LESS,      // e.g. size<10M
    PATH_PREFIX,    // e.g. path=backup/
    NAME_CONTAINS,  // e.g. name=archive
    DEFAULT         // fallback destination
};

struct RoutingRule {
    RuleConditionType type{RuleConditionType::DEFAULT};
    std::string raw_condition;
    std::vector<std::string> match_values; // extensions, prefixes, or keywords
    uint64_t size_threshold{0};            // bytes for size conditions
    std::string destination_uri;
    std::string label;
};

class RoutingRuleEngine {
public:
    RoutingRuleEngine() = default;

    void add_rule(const RoutingRule& rule);
    void set_default_destination(const std::string& destination_uri);

    // Parses a rule specifier string:
    // e.g. "ext=mp4,mkv:s3://media", "size>100M:b2://cold", "default:local:///archive"
    static RoutingRule parse_rule_string(const std::string& rule_str);

    // Evaluates a file against the rule chain and returns the matched destination URI
    std::string match_destination(const std::string& file_path, uint64_t file_size) const;

    // Returns the matched rule (or nullopt if none matched and no default)
    std::optional<RoutingRule> find_matching_rule(const std::string& file_path, uint64_t file_size) const;

    const std::vector<RoutingRule>& get_rules() const { return rules_; }
    const std::string& get_default_destination() const { return default_dest_; }

    static uint64_t parse_size_bytes(const std::string& size_str);

private:
    std::vector<RoutingRule> rules_;
    std::string default_dest_;
};

} // namespace utp
