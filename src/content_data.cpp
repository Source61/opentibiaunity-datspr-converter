#include "content_data.h"

#include <stdexcept>
#include <string>

namespace otu {

static int firstId(int category) {
    return category == static_cast<int>(ThingCategory::Item) ? 100 : 1;
}

ContentData::ContentData(const std::vector<uint8_t>& buffer, int clientVersion) : m_clientVersion(clientVersion) {
    BinaryReader in(buffer);
    datSignature = in.u32();

    // Each category stores its highest id.
    std::array<int, kThingCategoryCount> counts{};
    for (int& count : counts)
        count = in.u16() + 1;

    for (int category = 0; category < kThingCategoryCount; category++) {
        for (int id = firstId(category); id < counts[category]; id++) {
            ThingType& thing = things[category].emplace_back();
            thing.id = static_cast<uint16_t>(id);
            thing.category = static_cast<ThingCategory>(category);
            try {
                thing.unserialize(in, clientVersion);
            } catch (const std::exception& e) {
                static constexpr const char* kNames[] = {"item", "outfit", "effect", "missile"};
                throw std::runtime_error(std::string("Tibia.dat, ") + kNames[category] + " " + std::to_string(id) +
                                         ": " + e.what());
            }
        }
    }
}

uint32_t ContentData::signatureFor(int version) {
    switch (version) {
    case 770: return 0x439D5A33;
    case 1098: return 0x42A3;
    default: return 0;
    }
}

std::vector<uint8_t> ContentData::convertTo(int newVersion) const {
    BinaryWriter out;
    out.u32(signatureFor(newVersion));
    for (int category = 0; category < kThingCategoryCount; category++)
        out.u16(static_cast<uint16_t>(things[category].size() + firstId(category) - 1));

    for (const auto& list : things)
        for (const ThingType& thing : list)
            thing.serialize(out, m_clientVersion, newVersion);

    return std::move(out.data());
}

} // namespace otu
