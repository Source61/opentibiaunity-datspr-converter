#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace otu {

// Little-endian reader over an in-memory buffer. Unlike the C# MemoryStream it replaces,
// reading past the end throws instead of silently yielding 0xFF/zero bytes.
class BinaryReader {
public:
    BinaryReader(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}
    explicit BinaryReader(const std::vector<uint8_t>& data) : BinaryReader(data.data(), data.size()) {}

    size_t position() const { return m_pos; }
    size_t size() const { return m_size; }
    size_t remaining() const { return m_size - m_pos; }
    const uint8_t* current() const { return m_data + m_pos; }

    void seek(size_t pos) {
        if (pos > m_size)
            throw std::runtime_error("seek past end of data");
        m_pos = pos;
    }
    void skip(size_t n) { seek(m_pos + n); }

    uint8_t u8() {
        need(1);
        return m_data[m_pos++];
    }
    int8_t i8() { return static_cast<int8_t>(u8()); }
    uint16_t u16() { return static_cast<uint16_t>(le(2)); }
    int16_t i16() { return static_cast<int16_t>(u16()); }
    uint32_t u32() { return static_cast<uint32_t>(le(4)); }
    int32_t i32() { return static_cast<int32_t>(u32()); }
    uint64_t u64() { return le(8); }

    // u16 length prefix followed by single-byte characters. Bytes above 0x7F become '?',
    // which is what .NET's Encoding.ASCII produced (and keeps protobuf strings valid UTF-8).
    std::string asciiString() {
        uint16_t length = u16();
        need(length);
        std::string s(reinterpret_cast<const char*>(m_data + m_pos), length);
        for (char& c : s)
            if (static_cast<unsigned char>(c) > 0x7F)
                c = '?';
        m_pos += length;
        return s;
    }

private:
    void need(size_t n) const {
        if (m_size - m_pos < n)
            throw std::runtime_error("unexpected end of data");
    }
    uint64_t le(int bytes) {
        need(bytes);
        uint64_t v = 0;
        for (int i = 0; i < bytes; i++)
            v |= static_cast<uint64_t>(m_data[m_pos + i]) << (8 * i);
        m_pos += bytes;
        return v;
    }

    const uint8_t* m_data;
    size_t m_size;
    size_t m_pos = 0;
};

class BinaryWriter {
public:
    void u8(uint8_t v) { m_data.push_back(v); }
    void i8(int8_t v) { u8(static_cast<uint8_t>(v)); }
    void u16(uint16_t v) { le(v, 2); }
    void u32(uint32_t v) { le(v, 4); }
    void i32(int32_t v) { le(static_cast<uint32_t>(v), 4); }
    void bytes(const uint8_t* data, size_t size) { m_data.insert(m_data.end(), data, data + size); }
    void asciiString(const std::string& s) {
        u16(static_cast<uint16_t>(s.size()));
        bytes(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }

    std::vector<uint8_t>& data() { return m_data; }

private:
    void le(uint64_t v, int bytes) {
        for (int i = 0; i < bytes; i++)
            m_data.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }

    std::vector<uint8_t> m_data;
};

} // namespace otu
