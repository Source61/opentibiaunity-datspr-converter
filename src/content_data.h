#pragma once

#include "thing_type.h"

#include <array>
#include <cstdint>
#include <vector>

namespace otu {

// Tibia.dat of a legacy (< 11) client.
class ContentData {
public:
    ContentData(const std::vector<uint8_t>& buffer, int clientVersion);

    // Things per category, ordered by id. Item ids start at 100, all others at 1.
    std::array<std::vector<ThingType>, kThingCategoryCount> things;
    uint32_t datSignature = 0;

    // Re-encodes this .dat in the format of another client version.
    std::vector<uint8_t> convertTo(int newVersion) const;

    static uint32_t signatureFor(int version);

private:
    int m_clientVersion;
};

} // namespace otu
