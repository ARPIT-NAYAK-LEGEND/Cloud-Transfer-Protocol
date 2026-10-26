#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace utp {

// §7 event stream — every surface (CLI, API, Web) consumes these instead of
// polling ad-hoc state. Persisted in the `events` ledger table and optionally
// forwarded live to an in-process subscriber.
enum class EventType {
    TRANSFER_CREATED,
    TRANSFER_STARTED,
    TRANSFER_PAUSED,
    TRANSFER_RESUMED,
    TRANSFER_CANCELLED,
    TRANSFER_VERIFIED,
    TRANSFER_COMPLETED,
    TRANSFER_FAILED,
    OBJECT_STARTED,
    OBJECT_COMPLETED,
    CHUNK_COMPLETED,
};

struct Event {
    std::string transfer_id;
    EventType type;
    std::string payload;   // JSON details: chunk index, byte counters, error text
    int64_t timestamp{0};   // epoch nanoseconds
};

inline const char* event_type_to_string(EventType type) {
    switch (type) {
        case EventType::TRANSFER_CREATED:   return "TRANSFER_CREATED";
        case EventType::TRANSFER_STARTED:    return "TRANSFER_STARTED";
        case EventType::TRANSFER_PAUSED:     return "TRANSFER_PAUSED";
        case EventType::TRANSFER_RESUMED:    return "TRANSFER_RESUMED";
        case EventType::TRANSFER_CANCELLED:  return "TRANSFER_CANCELLED";
        case EventType::TRANSFER_VERIFIED:   return "TRANSFER_VERIFIED";
        case EventType::TRANSFER_COMPLETED:  return "TRANSFER_COMPLETED";
        case EventType::TRANSFER_FAILED:    return "TRANSFER_FAILED";
        case EventType::OBJECT_STARTED:     return "OBJECT_STARTED";
        case EventType::OBJECT_COMPLETED:   return "OBJECT_COMPLETED";
        case EventType::CHUNK_COMPLETED:     return "CHUNK_COMPLETED";
    }
    return "UNKNOWN";
}

inline EventType event_type_from_string(const std::string& str) {
    if (str == "TRANSFER_CREATED")  return EventType::TRANSFER_CREATED;
    if (str == "TRANSFER_STARTED")  return EventType::TRANSFER_STARTED;
    if (str == "TRANSFER_PAUSED")   return EventType::TRANSFER_PAUSED;
    if (str == "TRANSFER_RESUMED")  return EventType::TRANSFER_RESUMED;
    if (str == "TRANSFER_CANCELLED") return EventType::TRANSFER_CANCELLED;
    if (str == "TRANSFER_VERIFIED") return EventType::TRANSFER_VERIFIED;
    if (str == "TRANSFER_COMPLETED") return EventType::TRANSFER_COMPLETED;
    if (str == "TRANSFER_FAILED")   return EventType::TRANSFER_FAILED;
    if (str == "OBJECT_STARTED")    return EventType::OBJECT_STARTED;
    if (str == "OBJECT_COMPLETED")  return EventType::OBJECT_COMPLETED;
    if (str == "CHUNK_COMPLETED")   return EventType::CHUNK_COMPLETED;
    return EventType::TRANSFER_FAILED; // unknown strings must never read as success
}

using EventCallback = std::function<void(const Event&)>;

} // namespace utp
