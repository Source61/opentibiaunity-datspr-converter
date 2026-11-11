#pragma once

#include "binary_stream.h"

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace otu {

// Version-independent attribute ids. Each client version numbers its .dat flags
// differently; thing_type.cpp maps between the two.
enum class Attr : uint8_t {
    Ground = 0, GroundBorder, Bottom, Top, Container, Stackable, ForceUse, MultiUse, Writable,
    WritableOnce, FluidContainer, Splash, Unpassable, Unmoveable, Unsight, BlockPath, Pickupable,
    Hangable, HookSouth, HookEast, Rotateable, Light, DontHide, Translucent, Offset, Elevation,
    LyingCorpse, AnimateAlways, MinimapColor, LensHelp, FullGround, Look, Cloth, Market,
    DefaultAction, Use, Wrapable, Unwrapable, TopEffect,
    FloorChange = 251, NoMoveAnimation = 252, Charges = 253,
};

enum class ThingCategory : uint8_t { Item = 0, Creature, Effect, Missile };
constexpr int kThingCategoryCount = 4;

enum class FrameGroupType : uint8_t { Idle = 0, Moving = 1 };

struct LightInfo {
    uint16_t intensity = 0;
    uint16_t color = 0;
};

struct OffsetInfo {
    uint16_t x = 0;
    uint16_t y = 0;
};

struct MarketInfo {
    std::string name;
    uint16_t category = 0;
    uint16_t restrictLevel = 0;
    uint16_t restrictProfession = 0;
    uint16_t showAs = 0;
    uint16_t tradeAs = 0;
};

// std::monostate marks a plain presence flag.
using AttrValue = std::variant<std::monostate, uint16_t, LightInfo, OffsetInfo, MarketInfo>;

struct FrameDuration {
    int32_t minimum = 0;
    int32_t maximum = 0;
};

// Per-phase animation data, stored in the .dat from client 10.50 on.
struct FrameGroupAnimator {
    bool async = false;
    int32_t loopCount = 0;
    int8_t startPhase = -1;
    std::vector<FrameDuration> durations;

    void unserialize(uint8_t phases, BinaryReader& in);
    void serialize(BinaryWriter& out, int firstPhase, int phaseCount) const;
};

class ThingType;

struct FrameGroup {
    uint8_t width = 0;  // in 32px tiles
    uint8_t height = 0; // in 32px tiles
    uint8_t exactSize = 0;
    uint8_t layers = 0;
    uint8_t patternWidth = 0;
    uint8_t patternHeight = 0;
    uint8_t patternDepth = 0;
    uint8_t phases = 0;
    std::optional<FrameGroupAnimator> animator;
    std::vector<uint32_t> sprites;

    void unserialize(BinaryReader& in, int clientVersion);
    void serialize(const ThingType& thing, BinaryWriter& out, int fromVersion, int newVersion,
                   int firstPhase, int phaseCount) const;
};

class ThingType {
public:
    ThingCategory category = ThingCategory::Item;
    uint16_t id = 0;

    // Both lists keep insertion order (like the C# Dictionary they replace), so the .dat
    // writer emits attributes in the order they were read.
    std::vector<std::pair<Attr, AttrValue>> attributes;
    std::vector<std::pair<FrameGroupType, FrameGroup>> frameGroups;

    bool has(Attr a) const { return find(a) != nullptr; }
    const AttrValue* find(Attr a) const;
    template <class T>
    const T& get(Attr a) const {
        return std::get<T>(*find(a));
    }
    const FrameGroup& frameGroup(FrameGroupType type) const;

    void unserialize(BinaryReader& in, int clientVersion);
    void serialize(BinaryWriter& out, int fromVersion, int newVersion) const;

private:
    void set(Attr a, AttrValue value);
    void serializeAttributes(BinaryWriter& out, int newVersion) const;
};

} // namespace otu
