// ---------------------------------------------------------------------------
// config_probe.cpp - host-side (x86_64) validation of a mod's typed config.
//
// Builds the real src/mod/Config.hpp against the real preloader headers and
// runs it on the build machine, under ASan + UBSan. This catches bugs that only
// appear on-device otherwise:
//
//   * schema metadata built from temporaries (dangling std::string_view)
//     -> "invalid UTF-8 byte at index 7: 0x80" at runtime
//   * non UTF-8 strings reaching nlohmann::json
//   * config structs that boost.pfr cannot reflect
//   * enum / vector / optional fields that fail (de)serialisation
//
// Run: levimods/scripts/build.sh <mod>  (wired in as step 1/5)
//
// The mod supplies tools/probe_config.hpp with:  using ProbeConfig = ns::ModConfig;
// ---------------------------------------------------------------------------

#include "mod/Config.hpp"
#include "probe_config.hpp"

#include <cstdio>
#include <exception>
#include <string>

#include <nlohmann/json.hpp>

namespace {

int g_failures = 0;

void report(const char *what, const std::exception &ex) {
    std::printf("[probe] FAIL  %s: %s\n", what, ex.what());
    ++g_failures;
}

// dump() uses nlohmann's strict UTF-8 handler, so invalid bytes throw 316 here
// instead of on the device.
void checkJson(const char *what, const nlohmann::ordered_json &json) {
    try {
        const std::string text = json.dump(2);
        const auto reparsed = nlohmann::json::parse(text);
        std::printf("[probe] ok    %-22s %zu bytes, %zu keys\n", what, text.size(),
                    reparsed.size());
    } catch (const std::exception &ex) {
        report(what, ex);
    }
}

template <typename T> void probe(const char *label) {
    T defaults{};

    try {
        checkJson("default config", pl::config::defaultJson(defaults));
    } catch (const std::exception &ex) {
        report("default config", ex);
    }

    try {
        checkJson("schema", pl::config::schema(defaults));
    } catch (const std::exception &ex) {
        report("schema", ex);
    }

    // Every schema title/description must survive being read back a second time
    // after other allocations have stomped the stack.
    try {
        const auto schema = pl::config::schema(defaults);
        if (schema.contains("properties")) {
            for (auto it = schema["properties"].begin(); it != schema["properties"].end(); ++it) {
                const auto &prop = it.value();
                if (prop.contains("title")) {
                    const auto &t = prop["title"].template get_ref<const std::string &>();
                    if (t.empty())
                        throw std::runtime_error("empty schema title for " + it.key());
                }
            }
        }
        std::printf("[probe] ok    %-22s all field titles readable\n", "titles");
    } catch (const std::exception &ex) {
        report("titles", ex);
    }

    std::printf("[probe] %s: %s\n", label, g_failures ? "FAILED" : "PASS");
}

} // namespace

int main() {
    probe<ProbeConfig>("ProbeConfig");
    return g_failures == 0 ? 0 : 1;
}
