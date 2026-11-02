#pragma once

/**
 * @file Mod.hpp
 * @brief Ore ESP lifecycle and Mod Menu integration.
 */

#include <atomic>
#include <mutex>
#include <optional>
#include <string_view>

#include <pl/Logger.hpp>
#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>

#include "mod/Config.hpp"

namespace oreesp {

inline constexpr const char *kModuleId = "oreesp.main";
inline constexpr const char *kLogTag = "Ore ESP";

class Mod {
public:
    static Mod &instance();

    Mod();

    [[nodiscard]] ll::mod::NativeMod *self() const;
    [[nodiscard]] pl::log::Logger &log() const;

    bool load();
    bool enable();
    bool disable();
    bool unload();

    [[nodiscard]] ModConfig configSnapshot() const;
    void writeConfig(const ModConfig &value);

private:
    mutable std::atomic<ll::mod::NativeMod *> mSelf{nullptr};
    mutable std::mutex mConfigMutex;
    std::optional<pl::config::ConfigFile<ModConfig>> mConfigFile;
    std::atomic<bool> mRunning{false};
    std::atomic<bool> mRegistered{false};

    static void handleToggle(std::string_view moduleId, bool enabled);
    static void handleConfigChanged(std::string_view moduleId,
                                    std::string_view key,
                                    std::string_view value);
};

} // namespace oreesp
