#include "utp/routing_rules.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace utp {

static std::string to_lower_str(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

static std::vector<std::string> split_csv(const std::string& str) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        while (!item.empty() && (item.front() == ' ' || item.front() == '\t')) item.erase(0, 1);
        while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) item.pop_back();
        if (!item.empty()) {
            tokens.push_back(to_lower_str(item));
        }
    }
    return tokens;
}

uint64_t RoutingRuleEngine::parse_size_bytes(const std::string& size_str) {
    if (size_str.empty()) return 0;
    std::string s = to_lower_str(size_str);
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(0, 1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();

    uint64_t multiplier = 1;
    if (s.ends_with("kb") || s.ends_with("k")) {
        multiplier = 1024ULL;
        s = s.substr(0, s.find_last_not_of("kb ") + 1);
    } else if (s.ends_with("mb") || s.ends_with("m")) {
        multiplier = 1024ULL * 1024ULL;
        s = s.substr(0, s.find_last_not_of("mb ") + 1);
    } else if (s.ends_with("gb") || s.ends_with("g")) {
        multiplier = 1024ULL * 1024ULL * 1024ULL;
        s = s.substr(0, s.find_last_not_of("gb ") + 1);
    } else if (s.ends_with("tb") || s.ends_with("t")) {
        multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
        s = s.substr(0, s.find_last_not_of("tb ") + 1);
    } else if (s.ends_with("b")) {
        s = s.substr(0, s.length() - 1);
    }

    try {
        double val = std::stod(s);
        return static_cast<uint64_t>(val * static_cast<double>(multiplier));
    } catch (...) {
        throw std::invalid_argument("Invalid size specification: " + size_str);
    }
}

void RoutingRuleEngine::add_rule(const RoutingRule& rule) {
    if (rule.type == RuleConditionType::DEFAULT) {
        default_dest_ = rule.destination_uri;
    } else {
        rules_.push_back(rule);
    }
}

void RoutingRuleEngine::set_default_destination(const std::string& destination_uri) {
    default_dest_ = destination_uri;
}

RoutingRule RoutingRuleEngine::parse_rule_string(const std::string& rule_str) {
    // Expected formats:
    // "ext=mp4,mkv:s3://media"
    // "size>100M:b2://cold"
    // "size<10M:onedrive://small"
    // "path=research/:gdrive://research"
    // "name=backup:azure://archives"
    // "default:local:///fallback"

    size_t colon_pos = rule_str.find(':');
    if (colon_pos == std::string::npos || colon_pos == 0) {
        throw std::invalid_argument("Invalid rule syntax (missing ':' separator): " + rule_str);
    }

    // However, the destination itself has "://" (e.g. s3://bucket).
    // So the condition is before the FIRST colon that separates condition from destination.
    // e.g. "ext=mp4,mkv:s3://media" -> condition "ext=mp4,mkv", dest "s3://media"
    std::string condition = rule_str.substr(0, colon_pos);
    std::string destination = rule_str.substr(colon_pos + 1);

    RoutingRule rule;
    rule.raw_condition = condition;
    rule.destination_uri = destination;

    std::string cond_lower = to_lower_str(condition);

    if (cond_lower == "default" || cond_lower == "*") {
        rule.type = RuleConditionType::DEFAULT;
        rule.label = "Default Fallback";
        return rule;
    }

    if (cond_lower.rfind("ext=", 0) == 0) {
        rule.type = RuleConditionType::EXTENSION;
        rule.match_values = split_csv(condition.substr(4));
        rule.label = "File Extension (" + condition.substr(4) + ")";
    } else if (cond_lower.rfind("size>", 0) == 0) {
        rule.type = RuleConditionType::SIZE_GREATER;
        rule.size_threshold = parse_size_bytes(condition.substr(5));
        rule.label = "Size > " + condition.substr(5);
    } else if (cond_lower.rfind("size<", 0) == 0) {
        rule.type = RuleConditionType::SIZE_LESS;
        rule.size_threshold = parse_size_bytes(condition.substr(5));
        rule.label = "Size < " + condition.substr(5);
    } else if (cond_lower.rfind("path=", 0) == 0) {
        rule.type = RuleConditionType::PATH_PREFIX;
        rule.match_values = split_csv(condition.substr(5));
        rule.label = "Path Prefix (" + condition.substr(5) + ")";
    } else if (cond_lower.rfind("name=", 0) == 0) {
        rule.type = RuleConditionType::NAME_CONTAINS;
        rule.match_values = split_csv(condition.substr(5));
        rule.label = "Name Contains (" + condition.substr(5) + ")";
    } else {
        throw std::invalid_argument("Unsupported rule condition: " + condition +
                                   " (allowed: ext=..., size>..., size<..., path=..., name=..., default:...)");
    }

    return rule;
}

std::optional<RoutingRule> RoutingRuleEngine::find_matching_rule(const std::string& file_path, uint64_t file_size) const {
    std::string p_lower = to_lower_str(file_path);

    // Extract filename and extension
    size_t last_slash = p_lower.find_last_of("/\\");
    std::string filename = (last_slash == std::string::npos) ? p_lower : p_lower.substr(last_slash + 1);

    std::string ext = "";
    size_t last_dot = filename.find_last_of('.');
    if (last_dot != std::string::npos && last_dot + 1 < filename.length()) {
        ext = filename.substr(last_dot + 1);
    }

    for (const auto& rule : rules_) {
        switch (rule.type) {
            case RuleConditionType::EXTENSION: {
                for (const auto& target_ext : rule.match_values) {
                    std::string norm_ext = target_ext.starts_with('.') ? target_ext.substr(1) : target_ext;
                    if (ext == norm_ext) {
                        return rule;
                    }
                }
                break;
            }
            case RuleConditionType::SIZE_GREATER: {
                if (file_size > rule.size_threshold) {
                    return rule;
                }
                break;
            }
            case RuleConditionType::SIZE_LESS: {
                if (file_size < rule.size_threshold) {
                    return rule;
                }
                break;
            }
            case RuleConditionType::PATH_PREFIX: {
                for (const auto& prefix : rule.match_values) {
                    if (p_lower.starts_with(prefix)) {
                        return rule;
                    }
                }
                break;
            }
            case RuleConditionType::NAME_CONTAINS: {
                for (const auto& kw : rule.match_values) {
                    if (filename.find(kw) != std::string::npos) {
                        return rule;
                    }
                }
                break;
            }
            case RuleConditionType::DEFAULT:
                return rule;
        }
    }

    if (!default_dest_.empty()) {
        RoutingRule def;
        def.type = RuleConditionType::DEFAULT;
        def.destination_uri = default_dest_;
        def.label = "Default";
        return def;
    }

    return std::nullopt;
}

std::string RoutingRuleEngine::match_destination(const std::string& file_path, uint64_t file_size) const {
    auto match = find_matching_rule(file_path, file_size);
    if (match) {
        return match->destination_uri;
    }
    throw std::runtime_error("No routing rule matched file: " + file_path +
                             " (and no default destination was specified)");
}

} // namespace utp
