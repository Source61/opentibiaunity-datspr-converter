// Tibia.dat/Tibia.spr (clients < 11) -> OpenTibiaUnity appearances.otud + assets.otus.

#include "content_data.h"
#include "content_sprites.h"
#include "converters.h"
#include "image.h"
#include "work_queue.h"

#include "appearances.pb.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

namespace otu {
namespace {

namespace fs = std::filesystem;
namespace pb = OpenTibiaUnity::Protobuf::Appearances;
namespace pbs = OpenTibiaUnity::Protobuf::Shared;
using AppearanceList = google::protobuf::RepeatedPtrField<pb::Appearance>;

// Frame groups bucketed by size in tiles: 1x1, 1x2 (32x64), 2x1 (64x32), 2x2.
constexpr int kSizeClasses = 4;

struct TileSize {
    int width;
    int height;
};

class LegacyConverter {
public:
    LegacyConverter(int clientVersion, bool useAlpha) : m_clientVersion(clientVersion), m_useAlpha(useAlpha) {}

    bool run();

private:
    void generateFlags(const ThingType& thing, pb::Appearance& appearance);
    void generateAppearance(const ThingType& thing, pb::Appearance& appearance);

    void saveSprites(AppearanceList& appearances, int& start, const ContentSprites& sprites);
    void deploySprites(AppearanceList& appearances, std::array<std::vector<uint32_t>, kSizeClasses>& sprites);
    void changeSpriteIds(pb::FrameGroup& group, int parts);
    void saveStaticBitmaps(const std::vector<uint32_t>& sprites, int& start, const ContentSprites& sprParser,
                           int width, int height);

    int m_clientVersion;
    bool m_useAlpha;
    uint32_t m_referencedSpriteId = 0;
    uint32_t m_referenceFrameGroupId = 0;
    std::unordered_map<const pb::FrameGroup*, TileSize> m_frameGroupSizes;
    std::vector<std::unique_ptr<SheetEntry>> m_sheets;
    WorkQueue m_encoder;
};

void LegacyConverter::generateFlags(const ThingType& thing, pb::Appearance& appearance) {
    if (thing.attributes.empty())
        return;

    pb::AppearanceFlags& flags = *appearance.mutable_flags();
    auto u16 = [&](Attr a) { return thing.get<uint16_t>(a); };

    if (thing.has(Attr::Ground)) flags.mutable_ground()->set_speed(u16(Attr::Ground));
    if (thing.has(Attr::Writable)) flags.mutable_writable()->set_max_text_length(u16(Attr::Writable));
    if (thing.has(Attr::WritableOnce)) flags.mutable_writable_once()->set_max_text_length_once(u16(Attr::WritableOnce));
    if (thing.has(Attr::MinimapColor)) flags.mutable_automap()->set_color(u16(Attr::MinimapColor));
    if (thing.has(Attr::Elevation)) flags.mutable_height()->set_elevation(u16(Attr::Elevation));
    if (thing.has(Attr::LensHelp)) flags.mutable_lens_help()->set_id(u16(Attr::LensHelp));
    if (thing.has(Attr::Cloth)) flags.mutable_clothes()->set_slot(u16(Attr::Cloth));

    if (thing.has(Attr::DefaultAction)) {
        const uint16_t action = u16(Attr::DefaultAction);
        if (action > 4)
            std::cout << "Invalid default action: " << action << " for item id: " << thing.id << '\n';
        flags.mutable_default_action()->set_action(static_cast<pbs::PlayerAction>(action));
    }

    if (thing.has(Attr::GroundBorder)) flags.set_ground_border(true);
    if (thing.has(Attr::Bottom)) flags.set_bottom(true);
    if (thing.has(Attr::Top)) flags.set_top(true);
    if (thing.has(Attr::Container)) flags.set_container(true);
    if (thing.has(Attr::Stackable)) flags.set_stackable(true);
    if (thing.has(Attr::Use)) flags.set_use(true);
    if (thing.has(Attr::ForceUse)) flags.set_force_use(true);
    if (thing.has(Attr::MultiUse)) flags.set_multi_use(true);
    if (thing.has(Attr::FluidContainer)) flags.set_fluid_container(true);
    if (thing.has(Attr::Splash)) flags.set_splash(true);
    if (thing.has(Attr::Unpassable)) flags.set_unpassable(true);
    if (thing.has(Attr::Unmoveable)) flags.set_unmoveable(true);
    if (thing.has(Attr::Unsight)) flags.set_unsight(true);
    if (thing.has(Attr::BlockPath)) flags.set_block_path(true);
    if (thing.has(Attr::NoMoveAnimation)) flags.set_no_move_animation(true);
    if (thing.has(Attr::Pickupable)) flags.set_pickupable(true);
    if (thing.has(Attr::Hangable)) flags.set_hangable(true);

    // only one hook is possible
    if (thing.has(Attr::HookSouth))
        flags.mutable_hook()->set_type(pbs::HookTypeSouth);
    else if (thing.has(Attr::HookEast))
        flags.mutable_hook()->set_type(pbs::HookTypeEast);

    if (thing.has(Attr::Rotateable)) flags.set_rotateable(true);
    if (thing.has(Attr::DontHide)) flags.set_dont_hide(true);
    if (thing.has(Attr::Translucent)) flags.set_translucent(true);
    if (thing.has(Attr::LyingCorpse)) flags.set_lying_corpse(true);
    if (thing.has(Attr::AnimateAlways)) flags.set_animate_always(true);
    if (thing.has(Attr::FullGround)) flags.set_full_ground(true);
    if (thing.has(Attr::Look)) flags.set_ignore_look(true);
    if (thing.has(Attr::Wrapable)) flags.set_wrapable(true);
    // The C# converter mapped Unwrapable onto GroundBorder; kept for identical output.
    if (thing.has(Attr::Unwrapable)) flags.set_ground_border(true);
    if (thing.has(Attr::TopEffect)) flags.set_top_effect(true);

    if (thing.has(Attr::Light)) {
        const auto& light = thing.get<LightInfo>(Attr::Light);
        flags.mutable_light()->set_intensity(light.intensity);
        flags.mutable_light()->set_color(light.color);
    }

    if (thing.has(Attr::Offset)) {
        const auto& offset = thing.get<OffsetInfo>(Attr::Offset);
        flags.mutable_offset()->set_x(offset.x);
        flags.mutable_offset()->set_y(offset.y);
    }

    if (thing.has(Attr::Market)) {
        const auto& market = thing.get<MarketInfo>(Attr::Market);
        pb::AppearanceFlagMarket& out = *flags.mutable_market();
        out.set_category(static_cast<pbs::ItemCategory>(market.category));
        out.set_trade_as_object_id(market.tradeAs);
        out.set_show_as_object_id(market.showAs);
        out.set_minimum_level(market.restrictLevel);
        out.add_restrict_to_profession(static_cast<pbs::PlayerProfession>(market.restrictProfession));
        appearance.set_name(market.name);
    }
}

void LegacyConverter::generateAppearance(const ThingType& thing, pb::Appearance& appearance) {
    appearance.set_id(thing.id);
    generateFlags(thing, appearance);

    for (const auto& [type, legacy] : thing.frameGroups) {
        pb::FrameGroup& group = *appearance.add_frame_groups();
        group.set_type(type == FrameGroupType::Idle ? pbs::FrameGroupTypeOutfitIdle : pbs::FrameGroupTypeOutfitWalking);
        group.set_id(m_referenceFrameGroupId++);

        pb::SpriteInfo& info = *group.mutable_sprite_info();
        info.set_pattern_width(legacy.patternWidth);
        info.set_pattern_height(legacy.patternHeight);
        info.set_pattern_depth(legacy.patternDepth);
        info.set_layers(legacy.layers);
        info.set_phases(legacy.phases);
        info.set_bounding_square(legacy.exactSize);

        if (legacy.animator) {
            const FrameGroupAnimator& animator = *legacy.animator;
            pb::SpriteAnimation& animation = *info.mutable_animation();
            animation.set_default_start_phase(static_cast<uint32_t>(static_cast<int32_t>(animator.startPhase)));
            animation.set_synchronized(!animator.async);

            if (animator.loopCount < 0) {
                animation.set_loop_type(pbs::AnimationLoopTypePingPong);
            } else if (animator.loopCount == 0) {
                animation.set_loop_type(pbs::AnimationLoopTypeInfinite);
            } else {
                animation.set_loop_type(pbs::AnimationLoopTypeCounted);
                animation.set_loop_count(static_cast<uint32_t>(animator.loopCount));
            }

            for (const FrameDuration& duration : animator.durations) {
                pb::SpritePhase& phase = *animation.add_sprite_phases();
                phase.set_durationmin(static_cast<uint32_t>(duration.minimum));
                phase.set_durationmax(static_cast<uint32_t>(duration.maximum));
            }
        }

        for (uint32_t sprite : legacy.sprites)
            info.add_sprite_ids(sprite);

        m_frameGroupSizes[&group] = {legacy.width, legacy.height};
    }
}

// Collects the legacy 32x32 sprite ids of every frame group by size class, then renumbers the
// frame groups so that each id refers to one whole (up to 64x64) sprite in the new atlases.
void LegacyConverter::deploySprites(AppearanceList& appearances,
                                    std::array<std::vector<uint32_t>, kSizeClasses>& sprites) {
    std::array<std::vector<pb::FrameGroup*>, kSizeClasses> groups;
    for (pb::Appearance& appearance : appearances) {
        for (pb::FrameGroup& group : *appearance.mutable_frame_groups()) {
            auto it = m_frameGroupSizes.find(&group);
            if (it == m_frameGroupSizes.end())
                continue;

            const auto [width, height] = it->second;
            int sizeClass = -1;
            if (width == 1 && height == 1)
                sizeClass = 0;
            else if (width == 1 && height == 2)
                sizeClass = 1;
            else if (width == 2 && height == 1)
                sizeClass = 2;
            else if (width == 2 && height == 2)
                sizeClass = 3;

            if (sizeClass < 0) {
                std::cout << "Invalid width or height, currently there is maximum support for 64x64 sprites ("
                          << width << ", " << height << ")\n";
                continue;
            }

            const auto& ids = group.sprite_info().sprite_ids();
            sprites[sizeClass].insert(sprites[sizeClass].end(), ids.begin(), ids.end());
            groups[sizeClass].push_back(&group);
        }
    }

    constexpr int kParts[kSizeClasses] = {1, 2, 2, 4}; // 32x32 tiles per sprite
    for (int i = 0; i < kSizeClasses; i++)
        for (pb::FrameGroup* group : groups[i])
            changeSpriteIds(*group, kParts[i]);
}

void LegacyConverter::changeSpriteIds(pb::FrameGroup& group, int parts) {
    auto& ids = *group.mutable_sprite_info()->mutable_sprite_ids();
    const int total = (ids.size() + parts - 1) / parts;
    ids.Clear();
    for (int i = 0; i < total; i++)
        ids.Add(m_referencedSpriteId++);
}

void LegacyConverter::saveSprites(AppearanceList& appearances, int& start, const ContentSprites& sprites) {
    std::array<std::vector<uint32_t>, kSizeClasses> bySize;
    deploySprites(appearances, bySize);

    saveStaticBitmaps(bySize[0], start, sprites, 32, 32);
    saveStaticBitmaps(bySize[1], start, sprites, 32, 64);
    saveStaticBitmaps(bySize[2], start, sprites, 64, 32);
    saveStaticBitmaps(bySize[3], start, sprites, 64, 64);
}

// Packs sprites of one size into 512x512 sheets, left to right and top to bottom. Composing
// happens here; PNG encoding runs on the worker pool.
void LegacyConverter::saveStaticBitmaps(const std::vector<uint32_t>& sprites, int& start,
                                        const ContentSprites& sprParser, int width, int height) {
    if (sprites.empty())
        return;

    uint16_t spriteType;
    if (width == 32 && height == 32)
        spriteType = 1;
    else if (width == 32 && height == 64)
        spriteType = 2;
    else if (width == 64 && height == 32)
        spriteType = 3;
    else
        spriteType = 4;

    const int tilesWide = width / kSpriteSize;
    const int tilesHigh = height / kSpriteSize;
    const size_t parts = size_t(tilesWide) * tilesHigh;
    const int perSheet = (kSheetDimension * kSheetDimension) / (width * height);

    int localStart = start;
    start += static_cast<int>(sprites.size() / parts);

    auto sheet = std::make_shared<Image>(kSheetDimension, kSheetDimension);
    auto flush = [&](int first, int last) {
        auto& entry = m_sheets.emplace_back(std::make_unique<SheetEntry>());
        entry->spriteType = spriteType;
        entry->firstSpriteId = static_cast<uint32_t>(first);
        entry->lastSpriteId = static_cast<uint32_t>(last);
        m_encoder.push([target = entry.get(), image = std::move(sheet)] { target->png = encodePng(*image); });
    };

    std::array<uint8_t, kSpriteBytes> tile;
    int x = 0, y = 0, count = 0;
    for (size_t i = 0; i < sprites.size(); i = std::min(i + parts, sprites.size())) {
        if (y >= kSheetDimension) {
            flush(localStart, localStart + perSheet - 1);
            localStart += perSheet;
            sheet = std::make_shared<Image>(kSheetDimension, kSheetDimension);
            x = y = count = 0;
        }

        // Tile m of a multi-tile sprite sits at column m % tilesWide, row m / tilesWide,
        // counted from the bottom-right corner.
        for (size_t m = 0; m < parts && i + m < sprites.size(); m++) {
            if (!sprParser.getSprite(sprites[i + m], tile.data()))
                continue;
            const int dx = (tilesWide - 1 - int(m % tilesWide)) * kSpriteSize;
            const int dy = (tilesHigh - 1 - int(m / tilesWide)) * kSpriteSize;
            blitSprite(*sheet, tile.data(), x + dx, y + dy);
        }

        x += width;
        if (x >= kSheetDimension) {
            y += height;
            x = 0;
        }
        count++;
    }

    flush(localStart, localStart + count - 1);
}

bool LegacyConverter::run() {
    const fs::path base = std::to_string(m_clientVersion);
    const fs::path datFile = base / "Tibia.dat";
    const fs::path sprFile = base / "Tibia.spr";
    if (!fs::exists(datFile) || !fs::exists(sprFile)) {
        std::cerr << "Tibia.dat or Tibia.spr doesn't exist\n";
        return false;
    }

    std::cout << "Processing Appearances..." << std::flush;
    pb::Appearances appearances;
    {
        const ContentData contentData(readFile(datFile), m_clientVersion);
        AppearanceList* lists[kThingCategoryCount] = {appearances.mutable_objects(), appearances.mutable_outfits(),
                                                      appearances.mutable_effects(), appearances.mutable_missles()};
        for (int category = 0; category < kThingCategoryCount; category++)
            for (const ThingType& thing : contentData.things[category])
                generateAppearance(thing, *lists[category]->Add());
    }
    std::cout << "\rProcessing Appearances: Done!\n";

    const ContentSprites contentSprites(readFile(sprFile), m_clientVersion, m_useAlpha);
    const fs::path resultPath = base / "result";
    fs::create_directories(resultPath);

    std::cout << "Processing Spritesheets..." << std::flush;
    int start = 0;
    saveSprites(*appearances.mutable_outfits(), start, contentSprites);
    saveSprites(*appearances.mutable_effects(), start, contentSprites);
    saveSprites(*appearances.mutable_missles(), start, contentSprites);
    saveSprites(*appearances.mutable_objects(), start, contentSprites);
    m_encoder.wait();
    std::cout << "\rProcessing Spritesheets: Done!\n";

    // Written after the sprites: saveSprites() renumbers the frame groups' sprite ids.
    writeMessage(resultPath / "appearances.otud", appearances);

    std::stable_sort(m_sheets.begin(), m_sheets.end(),
                     [](const auto& a, const auto& b) { return a->firstSpriteId < b->firstSpriteId; });
    std::vector<const SheetEntry*> sheets;
    uint32_t atlasId = 0;
    for (auto& sheet : m_sheets) {
        sheet->atlasId = atlasId++;
        sheets.push_back(sheet.get());
    }
    writeAssets(resultPath / "assets.otus", sheets);
    return true;
}

} // namespace

bool convertLegacy(int clientVersion, bool useAlpha) {
    return LegacyConverter(clientVersion, useAlpha).run();
}

} // namespace otu
