#include "thing_type.h"

#include <cstdio>
#include <span>
#include <stdexcept>

namespace otu {
namespace {

enum class Payload : uint8_t {
    None,        // presence flag only
    U16,         // one uint16 value
    Light,       // intensity, color
    Offset,      // x, y
    OffsetFixed, // 7.30-7.50: presence flag; the displacement is always (8, 8)
    Market,      // category, trade-as, show-as, name, profession, level
};

struct FlagDef {
    uint8_t code;
    Attr attr;
    Payload payload;
    int minWriteVersion = 0; // first client version whose .dat can hold this flag
};

constexpr uint8_t kLastFlag = 0xFF;

// Flag numbering per .dat format, generated from the C# Metaflags/Attributes*.cs constants
// and the per-version Unserialize* switch statements it used to parse them.
constexpr FlagDef k730[] = {
    {0, Attr::Ground, Payload::U16}, {1, Attr::Bottom, Payload::None}, {2, Attr::Top, Payload::None},
    {3, Attr::Container, Payload::None}, {4, Attr::Stackable, Payload::None},
    {5, Attr::MultiUse, Payload::None}, {6, Attr::ForceUse, Payload::None}, {7, Attr::Writable, Payload::U16},
    {8, Attr::WritableOnce, Payload::U16}, {9, Attr::FluidContainer, Payload::None},
    {10, Attr::Splash, Payload::None}, {11, Attr::Unpassable, Payload::None},
    {12, Attr::Unmoveable, Payload::None}, {13, Attr::Unsight, Payload::None},
    {14, Attr::BlockPath, Payload::None}, {15, Attr::Pickupable, Payload::None},
    {16, Attr::Light, Payload::Light}, {17, Attr::FloorChange, Payload::None},
    {18, Attr::FullGround, Payload::None}, {19, Attr::Elevation, Payload::U16},
    {20, Attr::Offset, Payload::OffsetFixed}, {22, Attr::MinimapColor, Payload::U16},
    {23, Attr::Rotateable, Payload::None}, {24, Attr::LyingCorpse, Payload::None},
    {25, Attr::AnimateAlways, Payload::None}, {26, Attr::LensHelp, Payload::U16},
};
constexpr FlagDef k750[] = {
    {0, Attr::Ground, Payload::U16}, {1, Attr::Bottom, Payload::None}, {2, Attr::Top, Payload::None},
    {3, Attr::Container, Payload::None}, {4, Attr::Stackable, Payload::None},
    {5, Attr::MultiUse, Payload::None}, {6, Attr::ForceUse, Payload::None}, {7, Attr::Writable, Payload::U16},
    {8, Attr::WritableOnce, Payload::U16}, {9, Attr::FluidContainer, Payload::None},
    {10, Attr::Splash, Payload::None}, {11, Attr::Unpassable, Payload::None},
    {12, Attr::Unmoveable, Payload::None}, {13, Attr::Unsight, Payload::None},
    {14, Attr::BlockPath, Payload::None}, {15, Attr::Pickupable, Payload::None},
    {16, Attr::Light, Payload::Light}, {17, Attr::FloorChange, Payload::None},
    {18, Attr::FullGround, Payload::None}, {19, Attr::Elevation, Payload::U16},
    {20, Attr::Offset, Payload::OffsetFixed}, {22, Attr::MinimapColor, Payload::U16},
    {23, Attr::Rotateable, Payload::None}, {24, Attr::LyingCorpse, Payload::None},
    {25, Attr::Hangable, Payload::None}, {26, Attr::HookSouth, Payload::None},
    {27, Attr::HookEast, Payload::None}, {28, Attr::AnimateAlways, Payload::None},
    {29, Attr::LensHelp, Payload::U16},
};
constexpr FlagDef k772[] = {
    {0, Attr::Ground, Payload::U16}, {1, Attr::GroundBorder, Payload::None}, {2, Attr::Bottom, Payload::None},
    {3, Attr::Top, Payload::None}, {4, Attr::Container, Payload::None}, {5, Attr::Stackable, Payload::None},
    {6, Attr::MultiUse, Payload::None}, {7, Attr::ForceUse, Payload::None}, {8, Attr::Writable, Payload::U16},
    {9, Attr::WritableOnce, Payload::U16}, {10, Attr::FluidContainer, Payload::None},
    {11, Attr::Splash, Payload::None}, {12, Attr::Unpassable, Payload::None},
    {13, Attr::Unmoveable, Payload::None}, {14, Attr::Unsight, Payload::None},
    {15, Attr::BlockPath, Payload::None}, {16, Attr::Pickupable, Payload::None},
    {17, Attr::Hangable, Payload::None}, {18, Attr::HookSouth, Payload::None},
    {19, Attr::HookEast, Payload::None}, {20, Attr::Rotateable, Payload::None},
    {21, Attr::Light, Payload::Light}, {23, Attr::FloorChange, Payload::None},
    {24, Attr::Offset, Payload::Offset}, {25, Attr::Elevation, Payload::U16},
    {26, Attr::LyingCorpse, Payload::None}, {27, Attr::AnimateAlways, Payload::None},
    {28, Attr::MinimapColor, Payload::U16}, {29, Attr::LensHelp, Payload::U16},
    {30, Attr::FullGround, Payload::None},
};
constexpr FlagDef k854[] = {
    {0, Attr::Ground, Payload::U16}, {1, Attr::GroundBorder, Payload::None}, {2, Attr::Bottom, Payload::None},
    {3, Attr::Top, Payload::None}, {4, Attr::Container, Payload::None}, {5, Attr::Stackable, Payload::None},
    {6, Attr::ForceUse, Payload::None}, {7, Attr::MultiUse, Payload::None}, {8, Attr::Charges, Payload::None},
    {9, Attr::Writable, Payload::U16}, {10, Attr::WritableOnce, Payload::U16},
    {11, Attr::FluidContainer, Payload::None}, {12, Attr::Splash, Payload::None},
    {13, Attr::Unpassable, Payload::None}, {14, Attr::Unmoveable, Payload::None},
    {15, Attr::Unsight, Payload::None}, {16, Attr::BlockPath, Payload::None},
    {17, Attr::Pickupable, Payload::None}, {18, Attr::Hangable, Payload::None},
    {19, Attr::HookSouth, Payload::None}, {20, Attr::HookEast, Payload::None},
    {21, Attr::Rotateable, Payload::None}, {22, Attr::Light, Payload::Light},
    {23, Attr::DontHide, Payload::None}, {24, Attr::FloorChange, Payload::None},
    {25, Attr::Offset, Payload::Offset}, {26, Attr::Elevation, Payload::U16},
    {27, Attr::LyingCorpse, Payload::None}, {28, Attr::AnimateAlways, Payload::None},
    {29, Attr::MinimapColor, Payload::U16}, {30, Attr::LensHelp, Payload::U16},
    {31, Attr::FullGround, Payload::None},
};
constexpr FlagDef k986[] = {
    {0, Attr::Ground, Payload::U16}, {1, Attr::GroundBorder, Payload::None}, {2, Attr::Bottom, Payload::None},
    {3, Attr::Top, Payload::None}, {4, Attr::Container, Payload::None}, {5, Attr::Stackable, Payload::None},
    {6, Attr::ForceUse, Payload::None}, {7, Attr::MultiUse, Payload::None}, {8, Attr::Writable, Payload::U16},
    {9, Attr::WritableOnce, Payload::U16}, {10, Attr::FluidContainer, Payload::None},
    {11, Attr::Splash, Payload::None}, {12, Attr::Unpassable, Payload::None},
    {13, Attr::Unmoveable, Payload::None}, {14, Attr::Unsight, Payload::None},
    {15, Attr::BlockPath, Payload::None}, {16, Attr::Pickupable, Payload::None},
    {17, Attr::Hangable, Payload::None}, {18, Attr::HookSouth, Payload::None},
    {19, Attr::HookEast, Payload::None}, {20, Attr::Rotateable, Payload::None},
    {21, Attr::Light, Payload::Light}, {22, Attr::DontHide, Payload::None},
    {23, Attr::Translucent, Payload::None}, {24, Attr::Offset, Payload::Offset},
    {25, Attr::Elevation, Payload::U16}, {26, Attr::LyingCorpse, Payload::None},
    {27, Attr::AnimateAlways, Payload::None}, {28, Attr::MinimapColor, Payload::U16},
    {29, Attr::LensHelp, Payload::U16}, {30, Attr::FullGround, Payload::None},
    {31, Attr::Look, Payload::None}, {32, Attr::Cloth, Payload::U16}, {33, Attr::Market, Payload::Market},
};
constexpr FlagDef k1010[] = {
    {0, Attr::Ground, Payload::U16}, {1, Attr::GroundBorder, Payload::None}, {2, Attr::Bottom, Payload::None},
    {3, Attr::Top, Payload::None}, {4, Attr::Container, Payload::None}, {5, Attr::Stackable, Payload::None},
    {6, Attr::ForceUse, Payload::None}, {7, Attr::MultiUse, Payload::None}, {8, Attr::Writable, Payload::U16},
    {9, Attr::WritableOnce, Payload::U16}, {10, Attr::FluidContainer, Payload::None},
    {11, Attr::Splash, Payload::None}, {12, Attr::Unpassable, Payload::None},
    {13, Attr::Unmoveable, Payload::None}, {14, Attr::Unsight, Payload::None},
    {15, Attr::BlockPath, Payload::None}, {16, Attr::NoMoveAnimation, Payload::None},
    {17, Attr::Pickupable, Payload::None}, {18, Attr::Hangable, Payload::None},
    {19, Attr::HookSouth, Payload::None}, {20, Attr::HookEast, Payload::None},
    {21, Attr::Rotateable, Payload::None}, {22, Attr::Light, Payload::Light},
    {23, Attr::DontHide, Payload::None}, {24, Attr::Translucent, Payload::None},
    {25, Attr::Offset, Payload::Offset}, {26, Attr::Elevation, Payload::U16},
    {27, Attr::LyingCorpse, Payload::None}, {28, Attr::AnimateAlways, Payload::None},
    {29, Attr::MinimapColor, Payload::U16}, {30, Attr::LensHelp, Payload::U16},
    {31, Attr::FullGround, Payload::None}, {32, Attr::Look, Payload::None}, {33, Attr::Cloth, Payload::U16},
    {34, Attr::Market, Payload::Market}, {35, Attr::DefaultAction, Payload::U16},
    {254, Attr::Use, Payload::None}, {36, Attr::Wrapable, Payload::None, 1092},
    {37, Attr::Unwrapable, Payload::None, 1092}, {38, Attr::TopEffect, Payload::None, 1093},
};

std::span<const FlagDef> flagsFor(int version) {
    if (version <= 730) return k730;
    if (version <= 750) return k750;
    if (version <= 772) return k772;
    if (version <= 854) return k854;
    if (version <= 986) return k986;
    return k1010; // Tibia 10.x
}

const FlagDef* byCode(std::span<const FlagDef> flags, int code) {
    for (const FlagDef& def : flags)
        if (def.code == code)
            return &def;
    return nullptr;
}

const FlagDef* byAttr(std::span<const FlagDef> flags, Attr attr) {
    for (const FlagDef& def : flags)
        if (def.attr == attr)
            return &def;
    return nullptr;
}

std::string hex2(int v) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "0x%02X", v);
    return buf;
}

} // namespace

const AttrValue* ThingType::find(Attr a) const {
    for (const auto& [attr, value] : attributes)
        if (attr == a)
            return &value;
    return nullptr;
}

void ThingType::set(Attr a, AttrValue value) {
    for (auto& [attr, existing] : attributes) {
        if (attr == a) {
            existing = std::move(value);
            return;
        }
    }
    attributes.emplace_back(a, std::move(value));
}

const FrameGroup& ThingType::frameGroup(FrameGroupType type) const {
    for (const auto& [groupType, group] : frameGroups)
        if (groupType == type)
            return group;
    throw std::runtime_error("thing " + std::to_string(id) + " has no frame group " +
                             std::to_string(static_cast<int>(type)));
}

void ThingType::unserialize(BinaryReader& in, int clientVersion) {
    const auto flags = flagsFor(clientVersion);
    int attr = 0, previousAttr = 0, lastAttr = 0;
    try {
        for (attr = in.u8(); attr < kLastFlag; attr = in.u8()) {
            const FlagDef* def = byCode(flags, attr);
            if (!def)
                throw std::runtime_error("Unknown flag (ID = " + std::to_string(id) + ", Category = " +
                                         std::to_string(static_cast<int>(category)) + "): " + std::to_string(attr));

            // The C# 9.86 and 10.x readers stored the Translucent flag as DontHide. Kept, so the
            // generated appearances match what the original converter produced.
            const Attr target = def->attr == Attr::Translucent ? Attr::DontHide : def->attr;
            switch (def->payload) {
            case Payload::None:
                set(target, std::monostate{});
                break;
            case Payload::U16:
                set(target, in.u16());
                break;
            case Payload::Light: {
                LightInfo light;
                light.intensity = in.u16();
                light.color = in.u16();
                set(target, light);
                break;
            }
            case Payload::Offset: {
                OffsetInfo offset;
                offset.x = in.u16();
                offset.y = in.u16();
                set(target, offset);
                break;
            }
            case Payload::OffsetFixed:
                set(target, OffsetInfo{8, 8});
                break;
            case Payload::Market: {
                MarketInfo market;
                market.category = in.u16();
                market.tradeAs = in.u16();
                market.showAs = in.u16();
                market.name = in.asciiString();
                market.restrictProfession = in.u16();
                market.restrictLevel = in.u16();
                set(target, std::move(market));
                break;
            }
            }

            lastAttr = previousAttr;
            previousAttr = attr;
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Parsing Failed (") + e.what() + "). (attr: " + hex2(attr) +
                                 ", previous: " + hex2(previousAttr) + ", last: " + hex2(lastAttr) + ")");
    }

    const bool hasFrameGroups = category == ThingCategory::Creature && clientVersion >= 1057;
    const int groupCount = hasFrameGroups ? in.u8() : 1;
    for (int i = 0; i < groupCount; i++) {
        auto type = hasFrameGroups ? static_cast<FrameGroupType>(in.u8()) : FrameGroupType::Idle;
        FrameGroup group;
        group.unserialize(in, clientVersion);

        bool replaced = false;
        for (auto& [existingType, existing] : frameGroups) {
            if (existingType == type) {
                existing = std::move(group);
                replaced = true;
                break;
            }
        }
        if (!replaced)
            frameGroups.emplace_back(type, std::move(group));
    }
}

void ThingType::serializeAttributes(BinaryWriter& out, int newVersion) const {
    const auto flags = flagsFor(newVersion);
    for (const auto& [attr, value] : attributes) {
        const FlagDef* def = byAttr(flags, attr);
        if (!def || newVersion < def->minWriteVersion)
            continue; // the target format has no such flag

        out.u8(def->code);
        switch (def->payload) {
        case Payload::None:
        case Payload::OffsetFixed:
            break;
        case Payload::U16:
            out.u16(std::get<uint16_t>(value));
            break;
        case Payload::Light: {
            const auto& light = std::get<LightInfo>(value);
            out.u16(light.intensity);
            out.u16(light.color);
            break;
        }
        case Payload::Offset: {
            const auto& offset = std::get<OffsetInfo>(value);
            out.u16(offset.x);
            out.u16(offset.y);
            break;
        }
        case Payload::Market: {
            const auto& market = std::get<MarketInfo>(value);
            out.u16(market.category);
            out.u16(market.tradeAs);
            out.u16(market.showAs);
            out.asciiString(market.name);
            out.u16(market.restrictProfession);
            out.u16(market.restrictLevel);
            break;
        }
        }
    }
    out.u8(kLastFlag);
}

void ThingType::serialize(BinaryWriter& out, int fromVersion, int newVersion) const {
    serializeAttributes(out, newVersion);

    if (category != ThingCategory::Creature) {
        const FrameGroup& group = frameGroup(FrameGroupType::Idle);
        group.serialize(*this, out, fromVersion, newVersion, 0, group.phases);
        return;
    }

    // Before 10.57 a creature's walking animation is just more phases of its single frame
    // group; from 10.57 on idle and moving are separate frame groups.
    if (fromVersion < 1057) {
        const FrameGroup& group = frameGroup(FrameGroupType::Idle);
        if (newVersion < 1057) {
            group.serialize(*this, out, fromVersion, newVersion, 0, group.phases);
        } else if (group.phases == 1 || has(Attr::AnimateAlways)) {
            out.u8(1);
            out.u8(static_cast<uint8_t>(FrameGroupType::Idle));
            group.serialize(*this, out, fromVersion, newVersion, 0, group.phases);
        } else {
            out.u8(2);
            out.u8(static_cast<uint8_t>(FrameGroupType::Idle));
            group.serialize(*this, out, fromVersion, newVersion, 0, 1);
            out.u8(static_cast<uint8_t>(FrameGroupType::Moving));
            group.serialize(*this, out, fromVersion, newVersion, 1, group.phases - 1);
        }
        return;
    }

    if (newVersion < 1057)
        throw std::runtime_error("It's not possible to convert a client >= 1057 to a client < 1057");

    out.u8(static_cast<uint8_t>(frameGroups.size()));
    for (const auto& [type, group] : frameGroups) {
        out.u8(static_cast<uint8_t>(type));
        group.serialize(*this, out, fromVersion, newVersion, 0, group.phases);
    }
}

void FrameGroupAnimator::unserialize(uint8_t phases, BinaryReader& in) {
    async = in.u8() == 0;
    loopCount = in.i32();
    startPhase = in.i8();
    durations.resize(phases);
    for (FrameDuration& d : durations) {
        d.minimum = in.i32();
        d.maximum = in.i32();
    }
}

void FrameGroupAnimator::serialize(BinaryWriter& out, int firstPhase, int phaseCount) const {
    out.u8(async ? 0 : 1);
    out.i32(loopCount);

    const int maxPhase = firstPhase + phaseCount;
    if (startPhase > 0 && (startPhase < firstPhase || startPhase > maxPhase))
        out.i8(static_cast<int8_t>(firstPhase));
    else
        out.i8(startPhase);

    for (int i = 0; i < phaseCount; i++) {
        const FrameDuration& d = durations.at(firstPhase + i);
        out.i32(d.minimum);
        out.i32(d.maximum);
    }
}

// Animation block for clients >= 10.50, synthesized for frame groups read from older clients.
static void serializeLegacyAnimation(const ThingType& thing, BinaryWriter& out, int phaseCount) {
    out.u8(1); // synchronous
    out.i32(thing.has(Attr::AnimateAlways) || thing.category == ThingCategory::Item ? 0 : 1); // loop count
    out.i8(0); // start phase

    int duration;
    if (thing.category == ThingCategory::Effect)
        duration = 75;
    else
        duration = phaseCount > 0 ? 1000 / phaseCount : 40;

    for (int i = 0; i < phaseCount; i++) {
        out.i32(duration);
        out.i32(duration);
    }
}

void FrameGroup::unserialize(BinaryReader& in, int clientVersion) {
    width = in.u8();
    height = in.u8();
    exactSize = (width > 1 || height > 1) ? in.u8() : 32;
    layers = in.u8();
    patternWidth = in.u8();
    patternHeight = in.u8();
    patternDepth = clientVersion >= 755 ? in.u8() : 1;
    phases = in.u8();

    if (phases > 1 && clientVersion >= 1050) {
        animator.emplace();
        animator->unserialize(phases, in);
    }

    const size_t total = size_t(width) * height * layers * patternWidth * patternHeight * patternDepth * phases;
    sprites.resize(total);
    for (uint32_t& sprite : sprites)
        sprite = clientVersion >= 960 ? in.u32() : in.u16();
}

void FrameGroup::serialize(const ThingType& thing, BinaryWriter& out, int fromVersion, int newVersion,
                           int firstPhase, int phaseCount) const {
    out.u8(width);
    out.u8(height);
    if (width > 1 || height > 1)
        out.u8(exactSize);

    out.u8(layers);
    out.u8(patternWidth);
    out.u8(patternHeight);
    if (newVersion >= 755)
        out.u8(patternDepth);
    out.u8(static_cast<uint8_t>(phaseCount));

    if (phaseCount > 1 && newVersion >= 1050) {
        if (fromVersion < 1050)
            serializeLegacyAnimation(thing, out, phaseCount);
        else if (animator)
            animator->serialize(out, firstPhase, phaseCount);
        else
            throw std::runtime_error("thing " + std::to_string(thing.id) + " has phases but no animation data");
    }

    const size_t spritesPerPhase = size_t(width) * height * layers * patternWidth * patternHeight * patternDepth;
    const size_t offset = firstPhase * spritesPerPhase;
    for (size_t j = 0; j < phaseCount * spritesPerPhase; j++) {
        const uint32_t sprite = sprites.at(offset + j);
        if (newVersion >= 960)
            out.u32(sprite);
        else
            out.u16(static_cast<uint16_t>(sprite));
    }
}

} // namespace otu
