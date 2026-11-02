#pragma once

/**
 * @file Config.hpp
 * @brief Typed, persistent configuration for Ore ESP.
 *
 * Field names are JSON keys. Schema strings live in static storage so
 * pl::config::FieldSchema std::string_view members never dangle.
 */

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <pl/Config.hpp>

namespace oreesp {

struct ModConfig {
    static constexpr int kCurrentVersion = 1;

    int version = kCurrentVersion;

    bool enabled = true;

    int scanRadius = 24;        // 8..48 blocks
    int scanIntervalTicks = 10; // 4..40 ticks between scans
    int maxVeins = 80;          // 16..200 veins rendered
    double lineWidth = 1.8;     // 1.0..4.0 stroke scale

    bool mergeVeins = false;    // false = exact per-block wireframe, true = vein AABB
    bool showLabels = true;     // show vein name, block count, and distance
    bool showTracers = false;   // draw tracer lines from crosshair to veins
    bool hudOutlines = false;   // draw outlines via 2D HUD overlay instead of 3D GPU mesh

    // Per-ore toggles (apply on the very next render frame)
    bool showDiamond = true;
    bool showAncientDebris = true;
    bool showEmerald = true;
    bool showGold = true;
    bool showIron = true;
    bool showLapis = true;
    bool showRedstone = true;
    bool showQuartz = true;
    bool showCopper = false;
    bool showCoal = false;
};

namespace detail {

struct FieldDef {
    std::string name;
    std::string title;
    std::string description;
    std::optional<double> minimum;
    std::optional<double> maximum;
    bool readOnly = false;
};

inline const std::vector<FieldDef> &fieldDefs() {
    static const std::vector<FieldDef> defs = {
        {"version", "Config version", "", std::nullopt, std::nullopt, true},
        {"enabled", "Enabled", "Master switch for through-wall Ore ESP.", {}, {}, false},
        {"scanRadius", "Scan radius", "Block search radius around your player.", 8.0, 48.0, false},
        {"scanIntervalTicks", "Scan interval (ticks)",
         "Ticks between background rescan passes (20 ticks = 1 second).", 4.0, 40.0, false},
        {"maxVeins", "Max rendered veins",
         "Maximum ore veins drawn at once (prioritizes rarer and closer veins).", 16.0, 200.0, false},
        {"lineWidth", "Outline thickness", "3D wireframe stroke thickness.", 1.0, 4.0, false},
        {"mergeVeins", "Merge connected veins",
         "On: draw one bounding box per vein. Off: outline every individual ore block.", {}, {}, false},
        {"showLabels", "Show ore labels",
         "Display ore name, vein size, and distance in meters.", {}, {}, false},
        {"showTracers", "Show tracer lines",
         "Draw lines from screen center to highlighted ore veins.", {}, {}, false},
        {"hudOutlines", "Use 2D HUD outline renderer",
         "Off: zero-latency in-world GPU 3D through-wall outlines (works even with Hide HUD). On: 2D HUD overlay lines.",
         {}, {}, false},
        {"showDiamond", "Diamond Ore", "Highlight Diamond & Deepslate Diamond Ore.", {}, {}, false},
        {"showAncientDebris", "Ancient Debris", "Highlight Ancient Debris in the Nether.", {}, {}, false},
        {"showEmerald", "Emerald Ore", "Highlight Emerald & Deepslate Emerald Ore.", {}, {}, false},
        {"showGold", "Gold Ore",
         "Highlight Gold, Deepslate Gold, Nether Gold, Gilded Blackstone & Raw Gold.", {}, {}, false},
        {"showIron", "Iron Ore", "Highlight Iron, Deepslate Iron & Raw Iron blocks.", {}, {}, false},
        {"showLapis", "Lapis Lazuli Ore", "Highlight Lapis & Deepslate Lapis Ore.", {}, {}, false},
        {"showRedstone", "Redstone Ore", "Highlight Redstone & Deepslate Redstone Ore.", {}, {}, false},
        {"showQuartz", "Nether Quartz Ore", "Highlight Nether Quartz Ore.", {}, {}, false},
        {"showCopper", "Copper Ore", "Highlight Copper, Deepslate Copper & Raw Copper blocks.", {}, {}, false},
        {"showCoal", "Coal Ore", "Highlight Coal & Deepslate Coal Ore.", {}, {}, false},
    };
    return defs;
}

} // namespace detail
} // namespace oreesp

template <> struct pl::config::Schema<oreesp::ModConfig> {
    static constexpr std::string_view title = "Ore ESP";
    static constexpr std::string_view description =
        "Through-wall 3D wireframe ESP for Overworld, Deepslate, and Nether ores.";

    static pl::config::FieldSchema field(std::string_view name) {
        for (const auto &d : oreesp::detail::fieldDefs()) {
            if (d.name == name) {
                return pl::config::FieldSchema{.title = d.title,
                                               .description = d.description,
                                               .minimum = d.minimum,
                                               .maximum = d.maximum,
                                               .readOnly = d.readOnly};
            }
        }
        return {};
    }
};
