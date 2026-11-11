// Tibia 11+ assets (catalog-content.json, appearances protobuf, LZMA-packed BMP sprite sheets)
// -> OpenTibiaUnity appearances.otud + assets.otus.

#include "cip_lzma.h"
#include "converters.h"
#include "image.h"
#include "work_queue.h"

#include "appearances.pb.h"
#include "tibia/appearances.pb.h"

#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace otu {
namespace {

namespace fs = std::filesystem;
namespace pb = OpenTibiaUnity::Protobuf::Appearances;
namespace pbs = OpenTibiaUnity::Protobuf::Shared;
namespace tp = tibia::protobuf::appearances;
namespace tps = tibia::protobuf::shared;

struct CatalogSheet {
    std::string file;
    SheetEntry entry;
};

// Newtonsoft's JToken string conversion: strings as-is, other values as their JSON text.
std::string jsonText(const nlohmann::json& value) {
    return value.is_string() ? value.get<std::string>() : value.dump();
}

int64_t jsonInt(const nlohmann::json& value) {
    if (value.is_number_integer())
        return value.get<int64_t>();
    if (value.is_string())
        return std::stoll(value.get<std::string>());
    throw std::runtime_error("expected a number in catalog-content.json, got " + value.dump());
}

void parseCatalog(const fs::path& path, std::string& appearancesFile, std::vector<CatalogSheet>& sheets) {
    const auto bytes = readFile(path);
    const auto catalog = nlohmann::json::parse(bytes.begin(), bytes.end());
    if (!catalog.is_array())
        throw std::runtime_error("Invalid catalog-content JSON");

    uint32_t index = 0;
    for (const auto& object : catalog) {
        if (!object.is_object())
            continue;
        const auto type = object.find("type");
        if (type == object.end())
            continue;

        if (jsonText(*type) == "appearances") {
            const auto file = object.find("file");
            if (file == object.end())
                continue;
            appearancesFile = jsonText(*file);
        }

        const auto file = object.find("file");
        const auto spriteType = object.find("spritetype");
        const auto first = object.find("firstspriteid");
        const auto last = object.find("lastspriteid");
        if (file == object.end() || spriteType == object.end() || first == object.end() || last == object.end())
            continue;

        CatalogSheet& sheet = sheets.emplace_back();
        sheet.file = jsonText(*file);
        sheet.entry.spriteType = static_cast<uint16_t>(jsonInt(*spriteType) + 1);
        sheet.entry.firstSpriteId = static_cast<uint32_t>(jsonInt(*first));
        sheet.entry.lastSpriteId = static_cast<uint32_t>(jsonInt(*last));
        sheet.entry.atlasId = index++; // catalog order, not the sorted order
    }

    std::stable_sort(sheets.begin(), sheets.end(), [](const CatalogSheet& a, const CatalogSheet& b) {
        return a.entry.firstSpriteId < b.entry.firstSpriteId;
    });
}

void convertFlags(const tp::AppearanceFlags& in, pb::AppearanceFlags& flags) {
    if (in.has_bank())
        flags.mutable_ground()->set_speed(in.bank().waypoints());

    flags.set_ground_border(in.clip());
    flags.set_bottom(in.bottom());
    flags.set_top(in.top());
    flags.set_container(in.container());
    flags.set_stackable(in.cumulative());
    flags.set_force_use(in.forceuse());
    flags.set_multi_use(in.multiuse());

    if (in.has_write())
        flags.mutable_writable()->set_max_text_length(in.write().max_text_length());
    if (in.has_write_once())
        flags.mutable_writable_once()->set_max_text_length_once(in.write_once().max_text_length_once());

    flags.set_fluid_container(in.liquidcontainer());
    flags.set_splash(in.liquidpool());
    flags.set_unpassable(in.unpass());
    flags.set_unmoveable(in.unmove());
    flags.set_unsight(in.unsight());
    flags.set_block_path(in.avoid());
    flags.set_hangable(in.hang());

    if (in.has_hook()) {
        pb::AppearanceFlagHook& hook = *flags.mutable_hook();
        if (in.hook().direction() == tps::HOOK_TYPE_SOUTH)
            hook.set_type(pbs::HookTypeSouth);
        else if (in.hook().direction() == tps::HOOK_TYPE_EAST)
            hook.set_type(pbs::HookTypeEast);
    }

    flags.set_rotateable(in.rotate());

    if (in.has_light()) {
        flags.mutable_light()->set_intensity(in.light().brightness());
        flags.mutable_light()->set_color(in.light().color());
    }

    flags.set_dont_hide(in.dont_hide());
    flags.set_translucent(in.translucent());

    if (in.has_shift()) {
        flags.mutable_offset()->set_x(in.shift().x());
        flags.mutable_offset()->set_y(in.shift().y());
    }

    if (in.has_height())
        flags.mutable_height()->set_elevation(in.height().elevation());

    flags.set_lying_corpse(in.lying_object());
    flags.set_animate_always(in.animate_always());

    if (in.has_automap())
        flags.mutable_automap()->set_color(in.automap().color());
    if (in.has_lenshelp())
        flags.mutable_lens_help()->set_id(in.lenshelp().id());

    flags.set_full_ground(in.fullbank());
    flags.set_ignore_look(in.ignore_look());

    if (in.has_clothes())
        flags.mutable_clothes()->set_slot(in.clothes().slot());

    if (in.has_market()) {
        const tp::AppearanceFlagMarket& market = in.market();
        pb::AppearanceFlagMarket& out = *flags.mutable_market();
        out.set_category(static_cast<pbs::ItemCategory>(market.category()));
        out.set_trade_as_object_id(market.trade_as_object_id());
        out.set_show_as_object_id(market.show_as_object_id());
        out.set_minimum_level(market.minimum_level());
        for (int profession : market.restrict_to_profession()) {
            // Tibia encodes "any profession" as -1.
            out.add_restrict_to_profession(profession == -1 ? pbs::PlayerProfessionAny
                                                            : static_cast<pbs::PlayerProfession>(profession));
        }
    }

    if (in.has_default_action())
        flags.mutable_default_action()->set_action(static_cast<pbs::PlayerAction>(in.default_action().action()));

    flags.set_use(in.usable());
    flags.set_wrapable(in.wrap());
    flags.set_un_wrapable(in.unwrap());
    flags.set_top_effect(in.topeffect());

    for (const tp::AppearanceFlagNPC& npc : in.npcsaledata()) {
        pb::AppearanceFlagNPC& out = *flags.add_npc_sale_data();
        out.set_name(npc.name());
        out.set_location(npc.location());
        out.set_sale_price(npc.sale_price());
        out.set_buy_price(npc.buy_price());
    }

    if (in.has_changedtoexpire())
        flags.mutable_changed_to_expire()->set_former_object_type_id(in.changedtoexpire().former_object_typeid());

    flags.set_corpse(in.corpse());
    flags.set_playercorpse(in.player_corpse());

    if (in.has_cyclopediaitem())
        flags.mutable_cyclopedia_item()->set_cyclopedia_type(in.cyclopediaitem().cyclopedia_type());
}

void convertAppearance(const tp::Appearance& in, pb::Appearance& appearance) {
    appearance.set_id(in.id());
    appearance.set_name(in.name());

    for (const tp::FrameGroup& tibiaGroup : in.frame_group()) {
        pb::FrameGroup& group = *appearance.add_frame_groups();
        group.set_type(static_cast<pbs::FrameGroupType>(tibiaGroup.fixed_frame_group()));
        group.set_id(tibiaGroup.id());

        if (!tibiaGroup.has_sprite_info())
            continue;

        const tp::SpriteInfo& tibiaInfo = tibiaGroup.sprite_info();
        pb::SpriteInfo& info = *group.mutable_sprite_info();
        info.set_pattern_width(tibiaInfo.pattern_width());
        info.set_pattern_height(tibiaInfo.pattern_height());
        info.set_pattern_depth(tibiaInfo.pattern_depth());
        info.set_layers(tibiaInfo.layers());
        for (uint32_t sprite : tibiaInfo.sprite_id())
            info.add_sprite_ids(sprite);
        info.set_bounding_square(tibiaInfo.bounding_square());
        info.set_is_opaque(tibiaInfo.is_opaque());
        for (const tp::Box& tibiaBox : tibiaInfo.bounding_box_per_direction()) {
            pb::Box& box = *info.add_bounding_boxes_per_direction();
            box.set_x(tibiaBox.x());
            box.set_y(tibiaBox.y());
            box.set_width(tibiaBox.width());
            box.set_height(tibiaBox.height());
        }

        if (tibiaInfo.has_animation()) {
            const tp::SpriteAnimation& tibiaAnimation = tibiaInfo.animation();
            pb::SpriteAnimation& animation = *info.mutable_animation();
            animation.set_default_start_phase(tibiaAnimation.default_start_phase());
            animation.set_synchronized(tibiaAnimation.synchronized());
            animation.set_random_start_phase(tibiaAnimation.random_start_phase());
            animation.set_loop_count(tibiaAnimation.loop_count());

            // Tibia encodes ping-pong looping as -1.
            const int loopType = tibiaAnimation.loop_type();
            animation.set_loop_type(loopType == -1 ? pbs::AnimationLoopTypePingPong
                                                   : static_cast<pbs::AnimationLoopType>(loopType));

            for (const tp::SpritePhase& tibiaPhase : tibiaAnimation.sprite_phase()) {
                pb::SpritePhase& phase = *animation.add_sprite_phases();
                phase.set_durationmin(tibiaPhase.duration_min());
                phase.set_durationmax(tibiaPhase.duration_max());
            }

            info.set_phases(static_cast<uint32_t>(tibiaAnimation.sprite_phase_size()));
        } else {
            info.set_phases(1);
        }
    }

    if (in.has_flags())
        convertFlags(in.flags(), *appearance.mutable_flags());
}

} // namespace

bool convertProtobuf(int clientVersion, int buildVersion) {
    const fs::path base = fs::path(std::to_string(clientVersion)) / std::to_string(buildVersion);
    const fs::path loadPath = base / "assets";
    const fs::path resultPath = base / "result";

    const fs::path catalogFile = loadPath / "catalog-content.json";
    if (!fs::exists(catalogFile)) {
        std::cerr << "catalog-content.json not found at " << catalogFile.string() << '\n';
        return false;
    }

    std::string appearancesFile;
    std::vector<CatalogSheet> sheets;
    parseCatalog(catalogFile, appearancesFile, sheets);
    if (appearancesFile.empty()) {
        std::cerr << catalogFile.string() << " lists no appearances file\n";
        return false;
    }

    const fs::path datFile = loadPath / appearancesFile;
    if (!fs::exists(datFile)) {
        std::cerr << "appearances.dat not found at " << datFile.string() << '\n';
        return false;
    }

    tp::Appearances tibia;
    {
        const auto bytes = readFile(datFile);
        if (!tibia.ParseFromArray(bytes.data(), static_cast<int>(bytes.size()))) {
            std::cerr << "cannot parse " << datFile.string() << '\n';
            return false;
        }
    }

    pb::Appearances appearances;
    for (const tp::Appearance& appearance : tibia.object())
        convertAppearance(appearance, *appearances.add_objects());
    for (const tp::Appearance& appearance : tibia.outfit())
        convertAppearance(appearance, *appearances.add_outfits());
    for (const tp::Appearance& appearance : tibia.effect())
        convertAppearance(appearance, *appearances.add_effects());
    for (const tp::Appearance& appearance : tibia.missile())
        convertAppearance(appearance, *appearances.add_missles());

    if (tibia.has_special_meaning_appearance_ids()) {
        const tp::SpecialMeaningAppearanceIds& in = tibia.special_meaning_appearance_ids();
        pb::SpecialMeaningAppearanceIds& out = *appearances.mutable_special_meaning_appearance_ids();
        out.set_gold_coin_id(in.gold_coin_id());
        out.set_platinum_coin_id(in.platinum_coin_id());
        out.set_crystal_coin_id(in.crystal_coin_id());
        out.set_tibia_coin_id(in.tibia_coin_id());
        out.set_stamped_letter_id(in.stamped_letter_id());
        out.set_supply_stash_id(in.supply_stash_id());
    }

    fs::create_directories(resultPath);
    writeMessage(resultPath / "appearances.otud", appearances);

    std::cout << "Processing Spritesheets..." << std::flush;
    std::vector<const SheetEntry*> present;
    std::vector<std::string> missing;
    {
        WorkQueue workers;
        for (CatalogSheet& sheet : sheets) {
            const fs::path file = loadPath / sheet.file;
            if (!fs::exists(file)) {
                missing.push_back(file.string());
                continue;
            }
            present.push_back(&sheet.entry);
            workers.push([file, target = &sheet.entry] {
                try {
                    const auto bmp = decompressCipLzma(readFile(file));
                    target->png = encodePng(decodeBmp(bmp.data(), bmp.size()));
                } catch (const std::exception& e) {
                    throw std::runtime_error(file.string() + ": " + e.what());
                }
            });
        }
        workers.wait();
    }
    writeAssets(resultPath / "assets.otus", present);
    std::cout << "\rProcessing Spritesheets: Done!\n";

    for (const std::string& file : missing)
        std::cerr << "warning: sprite sheet listed in the catalog but missing, skipped: " << file << '\n';
    return true;
}

} // namespace otu
