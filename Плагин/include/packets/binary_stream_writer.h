#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace zombie {

class BinaryStreamWriter {
public:
    BinaryStreamWriter() = default;

    void writeByte(uint8_t value)
    {
        buffer_.push_back(static_cast<char>(value));
    }

    void writeBool(bool value)
    {
        writeByte(value ? 1 : 0);
    }

    void writeBytes(const void *data, size_t size)
    {
        const auto *bytes = static_cast<const char *>(data);
        buffer_.insert(buffer_.end(), bytes, bytes + size);
    }

    void writeRawString(std::string_view str)
    {
        buffer_.insert(buffer_.end(), str.begin(), str.end());
    }

    void writeVarUInt32(uint32_t value)
    {
        while (value >= 0x80) {
            buffer_.push_back(static_cast<char>((value & 0x7F) | 0x80));
            value >>= 7;
        }
        buffer_.push_back(static_cast<char>(value & 0x7F));
    }

    void writeVarInt32(int32_t value)
    {
        // Zig-zag encoding
        auto uval = static_cast<uint32_t>((value << 1) ^ (value >> 31));
        writeVarUInt32(uval);
    }

    void writeVarUInt64(uint64_t value)
    {
        while (value >= 0x80) {
            buffer_.push_back(static_cast<char>((value & 0x7F) | 0x80));
            value >>= 7;
        }
        buffer_.push_back(static_cast<char>(value & 0x7F));
    }

    void writeVarInt64(int64_t value)
    {
        // Zig-zag encoding
        auto uval = static_cast<uint64_t>((value << 1) ^ (value >> 63));
        writeVarUInt64(uval);
    }

    void writeString(std::string_view str)
    {
        writeVarUInt32(static_cast<uint32_t>(str.size()));
        writeRawString(str);
    }

    void writeLInt32(int32_t value)
    {
        uint32_t uval = static_cast<uint32_t>(value);
        buffer_.push_back(static_cast<char>(uval & 0xFF));
        buffer_.push_back(static_cast<char>((uval >> 8) & 0xFF));
        buffer_.push_back(static_cast<char>((uval >> 16) & 0xFF));
        buffer_.push_back(static_cast<char>((uval >> 24) & 0xFF));
    }

    void writeLUInt32(uint32_t uval)
    {
        buffer_.push_back(static_cast<char>(uval & 0xFF));
        buffer_.push_back(static_cast<char>((uval >> 8) & 0xFF));
        buffer_.push_back(static_cast<char>((uval >> 16) & 0xFF));
        buffer_.push_back(static_cast<char>((uval >> 24) & 0xFF));
    }

    [[nodiscard]] std::string_view getView() const
    {
        return {buffer_.data(), buffer_.size()};
    }

    [[nodiscard]] const std::string &getString() const
    {
        return buffer_;
    }

private:
    std::string buffer_;
};

}  // namespace zombie
