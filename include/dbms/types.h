#ifndef DBMS_TYPES_H
#define DBMS_TYPES_H

#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <stdexcept>
#include <cstring>
#include <cassert>
#include <iostream>
#include <optional>
#include <variant>

namespace dbms {

using PageID = uint64_t;
using Key = std::vector<uint8_t>;
using ValueData = std::vector<uint8_t>;

constexpr size_t BTREE_PAGE_SIZE = 4096;
constexpr size_t BTREE_MAX_KEY_SIZE = 1000;
constexpr size_t BTREE_MAX_VAL_SIZE = 3000;

constexpr uint16_t BNODE_NODE = 1; // Internal node
constexpr uint16_t BNODE_LEAF = 2; // Leaf node

constexpr uint64_t INVALID_PAGE_ID = 0;

// Status codes for operation outcomes
enum class StatusCode {
    OK = 0,
    NOT_FOUND,
    KEY_EXISTS,
    CORRUPTED,
    IO_ERROR,
    INVALID_ARGUMENT,
    TABLE_EXISTS,
    TABLE_NOT_FOUND,
    TYPE_MISMATCH,
    PARSE_ERROR,
    TX_ABORTED
};

class Status {
public:
    StatusCode code;
    std::string message;

    Status() : code(StatusCode::OK) {}
    Status(StatusCode c, std::string msg = "") : code(c), message(std::move(msg)) {}

    static Status OK() { return Status(StatusCode::OK); }
    static Status NotFound(std::string msg = "Not found") { return Status(StatusCode::NOT_FOUND, std::move(msg)); }
    static Status KeyExists(std::string msg = "Key exists") { return Status(StatusCode::KEY_EXISTS, std::move(msg)); }
    static Status Corrupted(std::string msg = "Corrupted data") { return Status(StatusCode::CORRUPTED, std::move(msg)); }
    static Status IOError(std::string msg = "I/O error") { return Status(StatusCode::IO_ERROR, std::move(msg)); }
    static Status InvalidArgument(std::string msg = "Invalid argument") { return Status(StatusCode::INVALID_ARGUMENT, std::move(msg)); }
    static Status TableExists(std::string msg = "Table exists") { return Status(StatusCode::TABLE_EXISTS, std::move(msg)); }
    static Status TableNotFound(std::string msg = "Table not found") { return Status(StatusCode::TABLE_NOT_FOUND, std::move(msg)); }
    static Status TypeMismatch(std::string msg = "Type mismatch") { return Status(StatusCode::TYPE_MISMATCH, std::move(msg)); }
    static Status ParseError(std::string msg = "Parse error") { return Status(StatusCode::PARSE_ERROR, std::move(msg)); }
    static Status TxAborted(std::string msg = "Transaction aborted") { return Status(StatusCode::TX_ABORTED, std::move(msg)); }

    bool ok() const { return code == StatusCode::OK; }
    bool is_not_found() const { return code == StatusCode::NOT_FOUND; }
};

// --- Little-Endian Serialization Helpers ---

inline uint16_t decode_u16(const uint8_t* ptr) {
    return static_cast<uint16_t>(ptr[0]) |
           (static_cast<uint16_t>(ptr[1]) << 8);
}

inline void encode_u16(uint8_t* ptr, uint16_t val) {
    ptr[0] = static_cast<uint8_t>(val & 0xFF);
    ptr[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
}

inline uint32_t decode_u32(const uint8_t* ptr) {
    return static_cast<uint32_t>(ptr[0]) |
           (static_cast<uint32_t>(ptr[1]) << 8) |
           (static_cast<uint32_t>(ptr[2]) << 16) |
           (static_cast<uint32_t>(ptr[3]) << 24);
}

inline void encode_u32(uint8_t* ptr, uint32_t val) {
    ptr[0] = static_cast<uint8_t>(val & 0xFF);
    ptr[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    ptr[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
    ptr[3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}

inline uint64_t decode_u64(const uint8_t* ptr) {
    uint64_t val = 0;
    for (int i = 0; i < 8; ++i) {
        val |= (static_cast<uint64_t>(ptr[i]) << (i * 8));
    }
    return val;
}

inline void encode_u64(uint8_t* ptr, uint64_t val) {
    for (int i = 0; i < 8; ++i) {
        ptr[i] = static_cast<uint8_t>((val >> (i * 8)) & 0xFF);
    }
}

// --- Big-Endian Order-Preserving Serialization Helpers ---

inline void encode_u32_be(uint8_t* ptr, uint32_t val) {
    ptr[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
    ptr[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
    ptr[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
    ptr[3] = static_cast<uint8_t>(val & 0xFF);
}

inline uint32_t decode_u32_be(const uint8_t* ptr) {
    return (static_cast<uint32_t>(ptr[0]) << 24) |
           (static_cast<uint32_t>(ptr[1]) << 16) |
           (static_cast<uint32_t>(ptr[2]) << 8) |
           static_cast<uint32_t>(ptr[3]);
}

inline void encode_i64_be(uint8_t* ptr, int64_t val) {
    uint64_t uval = static_cast<uint64_t>(val) ^ (1ULL << 63);
    for (int i = 0; i < 8; ++i) {
        ptr[i] = static_cast<uint8_t>((uval >> ((7 - i) * 8)) & 0xFF);
    }
}

inline int64_t decode_i64_be(const uint8_t* ptr) {
    uint64_t uval = 0;
    for (int i = 0; i < 8; ++i) {
        uval = (uval << 8) | static_cast<uint64_t>(ptr[i]);
    }
    uval ^= (1ULL << 63);
    return static_cast<int64_t>(uval);
}

} // namespace dbms

#endif // DBMS_TYPES_H
