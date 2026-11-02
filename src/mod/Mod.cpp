#include "mod/Mod.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <string>

#include "mod/GameHook.hpp"

namespace oreesp {
namespace {

int parseInt(std::string_view text, int fallback) {
    try {
        return std::stoi(std::string(text));
    } catch (...) {
        return fallback;
    }
}

double parseDouble(std::string_view text, double fallback) {
    try {
        return std::stod(std::string(text));
    } catch (...) {
        return fallback;
    }
}

bool parseBool(std::string_view text, bool fallback) {
    if (text == "true" || text == "1")
        return true;
    if (text == "false" || text == "0")
        return false;
    return fallback;
}

std::string formatFloat(double v) {
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    return std::string(buf);
}

void clampConfig(ModConfig &cfg) {
    cfg.scanRadius = std::clamp(cfg.scanRadius, 8, 48);
    cfg.scanIntervalTicks = std::clamp(cfg.scanIntervalTicks, 4, 40);
    cfg.maxVeins = std::clamp(cfg.maxVeins, 16, 200);
    cfg.lineWidth = std::clamp(cfg.lineWidth, 1.0, 4.0);
}

void pushSettings() {
    const auto cfg = Mod::instance().configSnapshot();
    GameHook::get().applyConfig(cfg);
}

} // namespace

Mod &Mod::instance() {
    static Mod mod;
    return mod;
}

Mod::Mod() : mSelf(ll::mod::NativeMod::current()) {}

ll::mod::NativeMod *Mod::self() const {
    if (auto *cached = mSelf.load(std::memory_order_acquire))
        return cached;
    auto *now = ll::mod::NativeMod::current();
    if (now)
        mSelf.store(now, std::memory_order_release);
    return now;
}

pl::log::Logger &Mod::log() const {
    static pl::log::Logger &instance = pl::log::Logger::getOrCreate(kLogTag);
    return instance;
}

bool Mod::load() {
    auto &log = Mod::log();
    try {
        std::lock_guard lock(mConfigMutex);
        mConfigFile.emplace();
        if (!mConfigFile->load())
            log.warn("config missing or unreadable, writing defaults");

        auto &cfg = mConfigFile->value();
        clampConfig(cfg);
        cfg.version = ModConfig::kCurrentVersion;
        mConfigFile->save();

        log.info("loaded (config {})", mConfigFile->configPath().string());
        return true;
    } catch (const std::exception &ex) {
        log.error("load() failed: {}", ex.what());
        return false;
    }
}

bool Mod::enable() {
    auto &log = Mod::log();
    try {
        const auto cfg = configSnapshot();

        pl::modmenu::ModuleBuilder builder(kModuleId, "Ore ESP");
        if (auto *self = Mod::self())
            builder.modId(self->getId());

        const bool moduleOk =
            builder
                .description("Highlights Overworld, Deepslate, and Nether ores through walls.")
                .defaultEnabled(cfg.enabled)
                .hideInHudEditor(true)
                .config("enabled", "Enabled", pl::modmenu::ConfigType::Toggle,
                        cfg.enabled ? "true" : "false")
                .config("scanRadius", "Scan radius (blocks)",
                        pl::modmenu::ConfigType::SliderInt,
                        std::to_string(cfg.scanRadius), "8", "48")
                .config("scanIntervalTicks", "Scan interval (ticks)",
                        pl::modmenu::ConfigType::SliderInt,
                        std::to_string(cfg.scanIntervalTicks), "4", "40")
                .config("maxVeins", "Max rendered veins",
                        pl::modmenu::ConfigType::SliderInt,
                        std::to_string(cfg.maxVeins), "16", "200")
                .config("lineWidth", "Outline thickness",
                        pl::modmenu::ConfigType::SliderFloat,
                        formatFloat(cfg.lineWidth), "1.0", "4.0")
                .config("mergeVeins", "Merge connected veins",
                        pl::modmenu::ConfigType::Toggle,
                        cfg.mergeVeins ? "true" : "false")
                .config("showLabels", "Show ore labels & distance",
                        pl::modmenu::ConfigType::Toggle,
                        cfg.showLabels ? "true" : "false")
                .config("showTracers", "Show tracer lines",
                        pl::modmenu::ConfigType::Toggle,
                        cfg.showTracers ? "true" : "false")
                .config("hudOutlines", "Use 2D HUD outline renderer",
                        pl::modmenu::ConfigType::Toggle,
                        cfg.hudOutlines ? "true" : "false")
                .config("showDiamond", "Diamond Ore",
                        pl::modmenu::ConfigType::Toggle,
                        cfg.showDiamond ? "true" : "false")
                .config("showAncientDebris", "Ancient Debris",
                        pl::modmenu::ConfigType::Toggle,
                        cfg.showAncientDebris ? "true" : "false")
                .config("showEmerald", "Emerald Ore",
                        pl::modmenu::ConfigType::Toggle,
                        cfg.showEmerald ? "true" : "false")
                .config("showGold", "Gold Ore",
                        pl::modmenu::ConfigType::Toggle,
                        cfg.showGold ? "true" : "false")
                .config("showIron", "Iron Ore",
                        pl::modmenu::ConfigType::Toggle,
                        cfg.showIron ? "true" : "false")
                .config("showLapis", "Lapis Lazuli Ore",
                        pl::modmenu::ConfigType::Toggle,
                        cfg.showLapis ? "true" : "false")
                .config("showRedstone", "Redstone Ore",
                        pl::modmenu::ConfigType::Toggle,
                        cfg.showRedstone ? "true" : "false")
                .config("showQuartz", "Nether Quartz Ore",
                        pl::modmenu::ConfigType::Toggle,
                        cfg.showQuartz ? "true" : "false")
                .config("showCopper", "Copper Ore",
                        pl::modmenu::ConfigType::Toggle,
                        cfg.showCopper ? "true" : "false")
                .config("showCoal", "Coal Ore",
                        pl::modmenu::ConfigType::Toggle,
                        cfg.showCoal ? "true" : "false")
                .onToggle(&Mod::handleToggle)
                .onConfigChanged(&Mod::handleConfigChanged)
                .registerModule();

        mRegistered.store(moduleOk, std::memory_order_release);
        pushSettings();
        const bool hookOk = GameHook::get().start();
        mRunning.store(true, std::memory_order_release);

        log.info("enable(): module={} hook={} radius={}", moduleOk, hookOk,
                 cfg.scanRadius);
        return moduleOk;
    } catch (const std::exception &ex) {
        log.error("enable() failed: {}", ex.what());
        return false;
    }
}

bool Mod::disable() {
    try {
        ModConfig offCfg = configSnapshot();
        offCfg.enabled = false;
        GameHook::get().applyConfig(offCfg);
        GameHook::get().stop();
        if (mRegistered.exchange(false, std::memory_order_acq_rel)) {
            pl::modmenu::unregisterModule(kModuleId);
        }
        mRunning.store(false, std::memory_order_release);
        log().info("disabled cleanly");
        return true;
    } catch (const std::exception &ex) {
        log().error("disable() failed: {}", ex.what());
        return false;
    }
}

bool Mod::unload() {
    try {
        ModConfig offCfg{};
        offCfg.enabled = false;
        GameHook::get().applyConfig(offCfg);
        GameHook::get().stop();
        if (mRegistered.exchange(false, std::memory_order_acq_rel)) {
            pl::modmenu::unregisterModule(kModuleId);
        }
        std::lock_guard lock(mConfigMutex);
        mConfigFile.reset();
        log().info("unloaded");
        return true;
    } catch (const std::exception &ex) {
        log().error("unload() failed: {}", ex.what());
        return false;
    }
}

void Mod::handleToggle(std::string_view moduleId, bool enabled) {
    if (moduleId != kModuleId)
        return;

    // IMPORTANT: Never call pl::modmenu::* inside onToggle/onConfigChanged!
    // libpreloader invokes these callbacks while holding its non-recursive
    // ModMenu mutex on Android's UI thread; re-entering pl::modmenu deadlocks UI.
    auto &mod = Mod::instance();
    auto cfg = mod.configSnapshot();
    cfg.enabled = enabled;
    mod.writeConfig(cfg);
    pushSettings();
}

void Mod::handleConfigChanged(std::string_view moduleId, std::string_view key,
                              std::string_view value) {
    if (moduleId != kModuleId)
        return;

    auto &mod = Mod::instance();
    auto cfg = mod.configSnapshot();
    const std::string k(key);
    bool changed = true;

    if (k == "enabled") {
        cfg.enabled = parseBool(value, cfg.enabled);
    } else if (k == "scanRadius") {
        cfg.scanRadius = parseInt(value, cfg.scanRadius);
    } else if (k == "scanIntervalTicks") {
        cfg.scanIntervalTicks = parseInt(value, cfg.scanIntervalTicks);
    } else if (k == "maxVeins") {
        cfg.maxVeins = parseInt(value, cfg.maxVeins);
    } else if (k == "lineWidth") {
        cfg.lineWidth = parseDouble(value, cfg.lineWidth);
    } else if (k == "mergeVeins") {
        cfg.mergeVeins = parseBool(value, cfg.mergeVeins);
    } else if (k == "showLabels") {
        cfg.showLabels = parseBool(value, cfg.showLabels);
    } else if (k == "showTracers") {
        cfg.showTracers = parseBool(value, cfg.showTracers);
    } else if (k == "hudOutlines") {
        cfg.hudOutlines = parseBool(value, cfg.hudOutlines);
    } else if (k == "showDiamond") {
        cfg.showDiamond = parseBool(value, cfg.showDiamond);
    } else if (k == "showAncientDebris") {
        cfg.showAncientDebris = parseBool(value, cfg.showAncientDebris);
    } else if (k == "showEmerald") {
        cfg.showEmerald = parseBool(value, cfg.showEmerald);
    } else if (k == "showGold") {
        cfg.showGold = parseBool(value, cfg.showGold);
    } else if (k == "showIron") {
        cfg.showIron = parseBool(value, cfg.showIron);
    } else if (k == "showLapis") {
        cfg.showLapis = parseBool(value, cfg.showLapis);
    } else if (k == "showRedstone") {
        cfg.showRedstone = parseBool(value, cfg.showRedstone);
    } else if (k == "showQuartz") {
        cfg.showQuartz = parseBool(value, cfg.showQuartz);
    } else if (k == "showCopper") {
        cfg.showCopper = parseBool(value, cfg.showCopper);
    } else if (k == "showCoal") {
        cfg.showCoal = parseBool(value, cfg.showCoal);
    } else {
        changed = false;
    }

    if (!changed)
        return;

    clampConfig(cfg);
    mod.writeConfig(cfg);
    pushSettings();
}

ModConfig Mod::configSnapshot() const {
    std::lock_guard lock(mConfigMutex);
    if (!mConfigFile)
        return ModConfig{};
    return mConfigFile->value();
}

void Mod::writeConfig(const ModConfig &value) {
    std::lock_guard lock(mConfigMutex);
    if (!mConfigFile)
        return;
    mConfigFile->value() = value;
    mConfigFile->save();
}

} // namespace oreesp
