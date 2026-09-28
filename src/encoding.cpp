#include "dbms/encoding.h"

namespace dbms {

void encode_value_order_preserving(Key& out, const Value& val) {
    if (val.type == ValueType::INT64) {
        uint8_t buf[8];
        encode_i64_be(buf, val.i64);
        out.insert(out.end(), buf, buf + 8);
    } else {
        for (char c : val.str) {
            uint8_t uc = static_cast<uint8_t>(c);
            if (uc == 0x00) {
                out.push_back(0x00);
                out.push_back(0xFF);
            } else {
                out.push_back(uc);
            }
        }
        out.push_back(0x00);
        out.push_back(0x00);
    }
}

size_t decode_value_order_preserving(const uint8_t* ptr, size_t len, ValueType type, Value& out_val) {
    if (type == ValueType::INT64) {
        if (len < 8) return 0;
        out_val = Value(decode_i64_be(ptr));
        return 8;
    } else {
        std::string s;
        size_t i = 0;
        while (i < len) {
            if (ptr[i] == 0x00) {
                if (i + 1 < len && ptr[i + 1] == 0xFF) {
                    s.push_back('\0');
                    i += 2;
                } else if (i + 1 < len && ptr[i + 1] == 0x00) {
                    i += 2;
                    out_val = Value(s);
                    return i;
                } else {
                    i++;
                }
            } else {
                s.push_back(static_cast<char>(ptr[i]));
                i++;
            }
        }
        out_val = Value(s);
        return i;
    }
}

Key encode_key(uint32_t prefix_id, const std::vector<Value>& values) {
    Key key;
    uint8_t pbuf[4];
    encode_u32_be(pbuf, prefix_id);
    key.insert(key.end(), pbuf, pbuf + 4);

    for (const auto& v : values) {
        encode_value_order_preserving(key, v);
    }
    return key;
}

Key encode_key_partial(uint32_t prefix_id, const std::vector<Value>& values) {
    return encode_key(prefix_id, values);
}

ValueData encode_row_payload(const TableDef& tdef, const Record& rec) {
    ValueData payload;

    for (size_t i = tdef.pkeys; i < tdef.cols.size(); ++i) {
        const std::string& col_name = tdef.cols[i];
        ValueType col_type = tdef.types[i];
        const Value* v = rec.get(col_name);

        if (col_type == ValueType::INT64) {
            uint8_t buf[8];
            int64_t val = v ? v->i64 : 0;
            encode_u64(buf, static_cast<uint64_t>(val));
            payload.insert(payload.end(), buf, buf + 8);
        } else {
            std::string s = v ? v->str : "";
            uint8_t buf[4];
            encode_u32(buf, static_cast<uint32_t>(s.size()));
            payload.insert(payload.end(), buf, buf + 4);
            payload.insert(payload.end(), s.begin(), s.end());
        }
    }

    return payload;
}

Status decode_row_payload(const ValueData& payload, const TableDef& tdef, Record& rec_out) {
    const uint8_t* ptr = payload.data();
    size_t len = payload.size();
    size_t offset = 0;

    for (size_t i = tdef.pkeys; i < tdef.cols.size(); ++i) {
        const std::string& col_name = tdef.cols[i];
        ValueType col_type = tdef.types[i];

        if (col_type == ValueType::INT64) {
            if (offset + 8 > len) return Status::Corrupted("Payload truncated decoding INT64");
            int64_t val = static_cast<int64_t>(decode_u64(ptr + offset));
            rec_out.set(col_name, Value(val));
            offset += 8;
        } else {
            if (offset + 4 > len) return Status::Corrupted("Payload truncated decoding STR length");
            uint32_t slen = decode_u32(ptr + offset);
            offset += 4;
            if (offset + slen > len) return Status::Corrupted("Payload truncated decoding STR bytes");
            std::string s(reinterpret_cast<const char*>(ptr + offset), slen);
            rec_out.set(col_name, Value(s));
            offset += slen;
        }
    }

    return Status::OK();
}

} // namespace dbms
