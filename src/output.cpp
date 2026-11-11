#include "binary_stream.h"
#include "converters.h"

#include <fstream>
#include <google/protobuf/message_lite.h>
#include <stdexcept>
#include <string>

namespace otu {

std::vector<uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        throw std::runtime_error("cannot open " + path.string());
    std::vector<uint8_t> data(static_cast<size_t>(in.tellg()));
    in.seekg(0);
    if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size())))
        throw std::runtime_error("cannot read " + path.string());
    return data;
}

static void writeOrThrow(std::ofstream& out, const std::filesystem::path& path, const void* data, size_t size) {
    if (!out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size)))
        throw std::runtime_error("cannot write " + path.string());
}

void writeFile(const std::filesystem::path& path, const void* data, size_t size) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        throw std::runtime_error("cannot create " + path.string());
    writeOrThrow(out, path, data, size);
}

void writeMessage(const std::filesystem::path& path, const google::protobuf::MessageLite& message) {
    std::string bytes;
    if (!message.SerializeToString(&bytes))
        throw std::runtime_error("cannot serialize " + path.string());
    writeFile(path, bytes.data(), bytes.size());
}

void writeAssets(const std::filesystem::path& path, const std::vector<const SheetEntry*>& sheets) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        throw std::runtime_error("cannot create " + path.string());

    BinaryWriter header;
    header.u32(static_cast<uint32_t>(sheets.size()));
    writeOrThrow(out, path, header.data().data(), header.data().size());

    for (const SheetEntry* sheet : sheets) {
        BinaryWriter record;
        record.u32(sheet->atlasId);
        record.u16(sheet->spriteType);
        record.u32(sheet->firstSpriteId);
        record.u32(sheet->lastSpriteId);
        record.u32(static_cast<uint32_t>(sheet->png.size()));
        writeOrThrow(out, path, record.data().data(), record.data().size());
        writeOrThrow(out, path, sheet->png.data(), sheet->png.size());
    }
}

} // namespace otu
