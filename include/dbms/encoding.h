#ifndef DBMS_ENCODING_H
#define DBMS_ENCODING_H

#include "schema.h"

namespace dbms {

// Order-preserving key encoding
Key encode_key(uint32_t prefix_id, const std::vector<Value>& values);
Key encode_key_partial(uint32_t prefix_id, const std::vector<Value>& values);

// Row payload binary encoding & decoding
ValueData encode_row_payload(const TableDef& tdef, const Record& rec);
Status decode_row_payload(const ValueData& payload, const TableDef& tdef, Record& rec_out);

// Individual value encoders
void encode_value_order_preserving(Key& out, const Value& val);
size_t decode_value_order_preserving(const uint8_t* ptr, size_t len, ValueType type, Value& out_val);

} // namespace dbms

#endif // DBMS_ENCODING_H
