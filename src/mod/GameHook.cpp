#include "mod/GameHook.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

#include <dlfcn.h>

#include <pl/Logger.hpp>
#include <pl/ModMenu.hpp>
#include <pl/memory/Hook.hpp>
#include <pl/memory/Signature.hpp>

#include "mod/Guard.hpp"
#include "mod/Mod.hpp"

namespace oreesp {
namespace {

constexpr const char *kGameModule = "libminecraftpe.so";

// ---------------------------------------------------------------------------
// Verified UNIQUE signatures in libminecraftpe.so 1.26.60.04:
//   NormalTick                         -> 0xaf5e460
//   BlockSourceGetBlock                -> 0x10371248
//   RenderLevel                        -> 0xb7a35d8
//   ClientInstanceUpdate               -> 0x9b7b278
//   TessellatorBegin                   -> 0xac2a408
//   TessellatorColor                   -> 0xac2a9bc
//   TessellatorVertex                  -> 0xac2ac58
//   MeshHelpersRenderMeshImmediately2  -> 0xb769d24
//   MeshHelpersRenderMeshImmediately   -> 0xb76a204
//   RenderMaterialGroupCommon          -> 0x9cf8f98
// ---------------------------------------------------------------------------
constexpr const char *kSigNormalTick =
    "? ? ? FC ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 ? ? ? D1 54 D0 "
    "3B D5 F3 03 00 AA ? ? ? F9 ? ? ? F8 ? ? ? 39";

constexpr const char *kSigBlockSourceGetBlock =
    "? ? ? D1 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 56 D0 3B D5 ? ? ? F9 ? ? ? F9 ? ? ? B9 ? ? "
    "? 79 1F 01 09 6B ? ? ? 54 ? ? ? 79 F3 03 00 AA 1F 01 09 6B ? ? ? 54 E0 03 00 91";

constexpr const char *kSigRenderLevel =
    "? ? ? D1 ? ? ? FD ? ? ? 6D ? ? ? 6D ? ? ? 6D ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? "
    "A9 ? ? ? 91 57 D0 3B D5";

constexpr const char *kSigClientInstanceUpdate =
    "? ? ? FC ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 ? ? ? D1 59 D0 "
    "3B D5 ? ? ? 52 F5 03 02 2A";

constexpr const char *kSigTessellatorBegin =
    "? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 FD 03 00 91 ? ? ? 39 ? ? ? 39 08 01 09 2A";

constexpr const char *kSigTessellatorColor =
    "? ? ? 52 ? ? ? 39 04 01 27 1E 00 08 24 1E 21 08 24 1E 42 08 24 1E 63 08 24 1E 08 00 38 1E";

constexpr const char *kSigTessellatorVertex =
    "? ? ? D1 ? ? ? FD ? ? ? 6D ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? "
    "91 5B D0 3B D5 ? ? ? F9 ? ? ? F9";

constexpr const char *kSigRenderMeshImmediately2 =
    "? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 FD 03 00 91 ? ? ? D1 57 D0 3B D5 F6 03 00 AA E0 03 "
    "01 AA ? ? ? F9 F4 03 03 AA";

constexpr const char *kSigRenderMeshImmediately =
    "? ? ? A9 ? ? ? F9 ? ? ? A9 ? ? ? A9 ? ? ? A9 FD 03 00 91 ? ? ? D1 58 D0 3B D5 F7 03 00 "
    "AA E0 03 01 AA ? ? ? F9 F4 03 04 AA";

constexpr const char *kSigRenderMaterialGroupCommon =
    "? ? ? ? ? ? ? 91 ? ? ? D1 ? ? ? D1 ? ? ? 91 E3 03 14 AA ? ? ? 52 ? ? ? ? ? ? ? F9 ? ? ? "
    "A9 ? ? ? B4 ? ? ? 91 ? ? ? 92 ? ? ? ? ? ? ? B5 ? ? ? F9 E0 03 14 AA ? ? ? F9 00 01 3F D6 "
    "E0 03 14 AA ? ? ? ? ? ? ? F9 ? ? ? F9 ? ? ? B4 ? ? ? 91 ? ? ? 92 ? ? ? ? ? ? ? B5 ? ? ? "
    "F9 E0 03 14 AA ? ? ? F9 00 01 3F D6 E0 03 14 AA ? ? ? ? ? ? ? 38 ? ? ? 37 ? ? ? 39 ? ? ? "
    "37 ? ? ? F8 ? ? ? A9 ? ? ? B4 ? ? ? 91 ? ? ? 92 ? ? ? ? ? ? ? B5 ? ? ? F9 E0 03 14 AA ? "
    "? ? F9 00 01 3F D6 E0 03 14 AA ? ? ? ? ? ? ? F8 ? ? ? F8 ? ? ? B4 ? ? ? 91 ? ? ? 92 ? ? "
    "? ? ? ? ? B5 ? ? ? F9 E0 03 14 AA ? ? ? F9 00 01 3F D6 E0 03 14 AA ? ? ? ? ? ? ? ? ? ? ? "
    "F9 ? ? ? 38 ? ? ? 36 ? ? ? F8 ? ? ? ? ? ? ? 39 ? ? ? 36 ? ? ? F9 ? ? ? ? ? ? ? F8 ? ? ? "
    "A9 ? ? ? B5 ? ? ? F8 ? ? ? F9 ? ? ? A9 ? ? ? F9 ? ? ? F9 ? ? ? B4 ? ? ? A9 ? ? ? B4 ? ? "
    "? 91 ? ? ? 52 ? ? ? ? ? ? ? F9 ? ? ? F9 ? ? ? B4 ? ? ? F9 ? ? ? 91 E1 03 13 AA ? ? ? 91 "
    "? ? ? ? ? ? ? 39 ? ? ? A9 ? ? ? 72 ? ? ? D3 29 15 94 9A 08 01 8A 9A ? ? ? A9 ? ? ? D1 ? "
    "? ? 91 ? ? ? ? ? ? ? F9 ? ? ? F9 ? ? ? F9 E8 03 00 91";

// ---------------------------------------------------------------------------
// Verified 1.26.60.04 structure offsets
// ---------------------------------------------------------------------------
constexpr std::size_t kActorDimension = 0x1E0;
constexpr std::size_t kActorStateVector = 0x228;
constexpr std::size_t kDimensionBlockSource = 0xD0; // 208
constexpr std::size_t kBlockSourceMaxY = 0x40;      // int16_t
constexpr std::size_t kBlockSourceMinY = 0x42;      // int16_t
constexpr std::size_t kBlockBlockType = 0x68;
constexpr std::size_t kBlockTypeFullName = 0xD0;    // std::string

constexpr std::size_t kLevelRendererPlayer = 0x430;
constexpr std::size_t kLrpCamPos = 0x614;           // Vec3f
constexpr std::size_t kLrpCamTargetPos = 0x620;     // Vec3f

constexpr std::size_t kScreenContextCamera = 0x18;      // mce::Camera*
constexpr std::size_t kScreenContextColorHolder = 0x28; // float[4]*
constexpr std::size_t kScreenContextTessellator = 0xB0; // Tessellator*

constexpr std::size_t kCameraViewStack = 0x00;      // MatrixStack (0x48 bytes)
constexpr std::size_t kCameraWorldStack = 0x48;     // MatrixStack (0x48 bytes)
constexpr std::size_t kCameraProjStack = 0x90;      // MatrixStack (0x48 bytes)
constexpr std::size_t kDequeBlocks = 0x08;          // mat4**
constexpr std::size_t kDequeStart = 0x20;           // uint64_t
constexpr std::size_t kDequeSize = 0x28;            // uint64_t

constexpr std::size_t kRenderMaterialGroupOffset = 0x20;
constexpr std::size_t kRenderMaterialGroupGetMatVslot = 2; // +0x10

// Scanner & renderer budgets
constexpr int kSettleMs = 2000;
constexpr int kBlocksBudgetPerTick = 32768;
constexpr std::size_t kMaxRawOreBlocks = 8192;
constexpr std::size_t kMaxDrawCommands = 1600;
constexpr std::size_t kMaxGpuQuads = 4096; // 16384 vertices max per draw call
constexpr float kClipNearW = 0.08f;
constexpr float kRibbonNearDist = 0.12f;

using NormalTickFn = void (*)(void *);
using RenderLevelFn = void (*)(void *, void *, void *);
using ClientUpdateFn = void *(*)(void *, bool);
using GetBlockFn = const void *(*)(void *, const BlockPos *);
using TessBeginFn = void (*)(void *, void *, int, int, int);
using TessColorFn = void (*)(void *, float, float, float, float);
using TessVertexFn = void (*)(void *, float, float, float);
using RenderMeshFn = void (*)(void *, void *, void *, char *);

class HashedString {
public:
    std::uint64_t hash = 0;
    std::string value;
    mutable const HashedString *lastMatch = nullptr;

    explicit HashedString(const char *text)
        : hash(computeHash(text ? std::string_view(text) : std::string_view())),
          value(text ? text : ""),
          lastMatch(nullptr) {}

private:
    static std::uint64_t computeHash(std::string_view text) {
        if (text.empty())
            return 0;
        constexpr std::uint64_t kOffset = 0xCBF29CE484222325ULL;
        constexpr std::uint64_t kPrime = 0x100000001B3ULL;
        std::uint64_t h = kOffset;
        for (char ch : text) {
            h = static_cast<std::uint64_t>(static_cast<unsigned char>(ch)) ^
                (kPrime * h);
        }
        return h;
    }
};

// Non-trivial move/dtor ensures AArch64 Itanium C++ ABI indirect return in x8
struct MaterialPtr {
    void *sharedPtrData[2]{nullptr, nullptr};

    MaterialPtr() = default;
    MaterialPtr(const MaterialPtr &) = delete;
    MaterialPtr &operator=(const MaterialPtr &) = delete;

    MaterialPtr(MaterialPtr &&other) noexcept
        : sharedPtrData{other.sharedPtrData[0], other.sharedPtrData[1]} {
        other.sharedPtrData[0] = nullptr;
        other.sharedPtrData[1] = nullptr;
    }

    MaterialPtr &operator=(MaterialPtr &&other) noexcept {
        if (this != &other) {
            sharedPtrData[0] = other.sharedPtrData[0];
            sharedPtrData[1] = other.sharedPtrData[1];
            other.sharedPtrData[0] = nullptr;
            other.sharedPtrData[1] = nullptr;
        }
        return *this;
    }

    ~MaterialPtr() {}

    explicit operator bool() const { return sharedPtrData[0] != nullptr; }
};

MaterialPtr gUiFillMaterial;

std::uintptr_t resolveAdrpAdd(const std::uint32_t *insns, std::size_t count,
                              std::uint32_t targetReg) {
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t insn = insns[i];
        if ((insn & 0x1Fu) != targetReg)
            continue;
        if ((insn & 0x9F000000u) == 0x90000000u) {
            const auto page =
                (reinterpret_cast<std::uintptr_t>(&insns[i]) & ~0xFFFULL) +
                static_cast<std::uintptr_t>(
                    (static_cast<std::int64_t>(
                         static_cast<std::uint64_t>(
                             ((insn >> 3u) & 0x1FFFFCu) | ((insn >> 29u) & 3u))
                         << 43u) >>
                     31u));
            for (std::size_t j = i + 1; j < count; ++j) {
                const std::uint32_t add = insns[j];
                if ((add & 0xFF000000u) == 0x91000000u &&
                    ((add >> 5u) & 0x1Fu) == targetReg &&
                    (add & 0x1Fu) == targetReg) {
                    std::uint32_t imm12 = (add >> 10u) & 0xFFFu;
                    if ((add & 0x400000u) != 0u)
                        imm12 <<= 12u;
                    return page + imm12;
                }
                if ((add & 0x1Fu) == targetReg)
                    break;
            }
        }
    }
    return 0;
}

MaterialPtr lookupMaterial(std::uintptr_t groupAddr, const char *name) {
    if (groupAddr == 0)
        return {};
    MaterialPtr out;
    guard::run([&] {
        const auto *groupPtr = reinterpret_cast<void *>(groupAddr);
        if (!guard::isPointer(groupPtr))
            return;
        void **vtable = *reinterpret_cast<void ***>(groupAddr);
        if (!guard::isPointer(vtable))
            return;
        void *fn = vtable[kRenderMaterialGroupGetMatVslot];
        if (!guard::isPointer(fn))
            return;
        HashedString hs(name);
        using GetMatFn = MaterialPtr (*)(void *, const HashedString *);
        out = reinterpret_cast<GetMatFn>(fn)(
            reinterpret_cast<void *>(groupAddr), &hs);
    });
    return out;
}

pl::log::Logger &logger() {
    static pl::log::Logger &instance = pl::log::Logger::getOrCreate(kLogTag);
    return instance;
}

std::int64_t steadyNowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool libraryMapped() {
    void *handle = ::dlopen(kGameModule, RTLD_NOW | RTLD_NOLOAD);
    if (handle == nullptr)
        return false;
    ::dlclose(handle);
    return true;
}

bool sanePos(const Vec3f &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
           std::fabs(v.x) < 3.0e7f && std::fabs(v.z) < 3.0e7f &&
           std::fabs(v.y) < 1.0e5f;
}

std::uint32_t bitForOre(OreKind k) {
    return 1u << static_cast<std::uint32_t>(k);
}

const char *oreDisplayName(OreKind k) {
    switch (k) {
    case OreKind::AncientDebris:
        return "Ancient Debris";
    case OreKind::Diamond:
        return "Diamond";
    case OreKind::Emerald:
        return "Emerald";
    case OreKind::Gold:
        return "Gold";
    case OreKind::Lapis:
        return "Lapis";
    case OreKind::Redstone:
        return "Redstone";
    case OreKind::Iron:
        return "Iron";
    case OreKind::Quartz:
        return "Quartz";
    case OreKind::Copper:
        return "Copper";
    case OreKind::Coal:
        return "Coal";
    default:
        return "Ore";
    }
}

std::uint32_t oreBaseRgb(OreKind k) {
    switch (k) {
    case OreKind::AncientDebris:
        return 0xFF8C42u;
    case OreKind::Diamond:
        return 0x24F0FFu;
    case OreKind::Emerald:
        return 0x20FF70u;
    case OreKind::Gold:
        return 0xFFD83Bu;
    case OreKind::Lapis:
        return 0x4A8CFFu;
    case OreKind::Redstone:
        return 0xFF3B4Eu;
    case OreKind::Iron:
        return 0xF2CBB0u;
    case OreKind::Quartz:
        return 0xF5F2EAu;
    case OreKind::Copper:
        return 0xEA8248u;
    case OreKind::Coal:
        return 0x9BA6B4u;
    default:
        return 0xFFFFFFu;
    }
}

float oreRarityBonusMeters(OreKind k) {
    switch (k) {
    case OreKind::AncientDebris:
    case OreKind::Diamond:
    case OreKind::Emerald:
        return 36.0f;
    case OreKind::Gold:
    case OreKind::Lapis:
    case OreKind::Redstone:
        return 14.0f;
    case OreKind::Iron:
    case OreKind::Quartz:
        return 4.0f;
    default:
        return 0.0f;
    }
}

std::uint32_t withAlpha(std::uint32_t rgb, std::uint8_t alpha) {
    return (static_cast<std::uint32_t>(alpha) << 24u) | (rgb & 0x00FFFFFFu);
}

OreKind classifyName(std::string_view name) {
    if (name.starts_with("minecraft:"))
        name.remove_prefix(10);

    if (name == "diamond_ore" || name == "deepslate_diamond_ore")
        return OreKind::Diamond;
    if (name == "ancient_debris")
        return OreKind::AncientDebris;
    if (name == "emerald_ore" || name == "deepslate_emerald_ore")
        return OreKind::Emerald;
    if (name == "gold_ore" || name == "deepslate_gold_ore" ||
        name == "nether_gold_ore" || name == "gilded_blackstone" ||
        name == "raw_gold_block")
        return OreKind::Gold;
    if (name == "lapis_ore" || name == "deepslate_lapis_ore")
        return OreKind::Lapis;
    if (name == "redstone_ore" || name == "lit_redstone_ore" ||
        name == "deepslate_redstone_ore" || name == "lit_deepslate_redstone_ore")
        return OreKind::Redstone;
    if (name == "iron_ore" || name == "deepslate_iron_ore" ||
        name == "raw_iron_block")
        return OreKind::Iron;
    if (name == "quartz_ore")
        return OreKind::Quartz;
    if (name == "copper_ore" || name == "deepslate_copper_ore" ||
        name == "raw_copper_block")
        return OreKind::Copper;
    if (name == "coal_ore" || name == "deepslate_coal_ore")
        return OreKind::Coal;

    return OreKind::None;
}

bool copyBlockTypeName(const void *blockType, char *out, std::size_t capacity) {
    if (!guard::isPointer(blockType) || capacity < 8)
        return false;

    const char *base = static_cast<const char *>(blockType) + kBlockTypeFullName;
    const auto tag = static_cast<unsigned char>(base[0]);

    if ((tag & 1u) != 0u) {
        std::size_t size = 0;
        std::memcpy(&size, base + 0x08, sizeof(size));
        const void *data = guard::readPtr(base, 0x10);
        if (data != nullptr && size > 0 && size < capacity) {
            std::memcpy(out, data, size);
            out[size] = '\0';
            if (guard::printable(out, size))
                return true;
        }
    } else {
        const std::size_t shortSize = static_cast<std::size_t>(tag >> 1u);
        if (shortSize > 0 && shortSize < 23 && shortSize < capacity) {
            std::memcpy(out, base + 1, shortSize);
            out[shortSize] = '\0';
            if (guard::printable(out, shortSize))
                return true;
        }
    }

    const void *altData = guard::readPtr(base, 0x00);
    std::size_t altSize = 0;
    std::memcpy(&altSize, base + 0x08, sizeof(altSize));
    if (altData != nullptr && altSize > 0 && altSize < capacity) {
        std::memcpy(out, altData, altSize);
        out[altSize] = '\0';
        if (guard::printable(out, altSize))
            return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// 3D ribbon geometry + 4x4 projection math
// ---------------------------------------------------------------------------
struct GpuQuad {
    Vec3f v0, v1, v2, v3;
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
};

Vec3f cross3(const Vec3f &a, const Vec3f &b) {
    return Vec3f{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float dot3(const Vec3f &a, const Vec3f &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float length3(const Vec3f &v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

void pushRibbonSegment3D(Vec3f p0, Vec3f p1, const Vec3f &camFwd,
                         float widthScale, float r, float g, float b, float a,
                         std::vector<GpuQuad> &out) {
    if (out.size() + 2u > kMaxGpuQuads)
        return;

    const float d0 = dot3(p0, camFwd);
    const float d1 = dot3(p1, camFwd);
    if (d0 < kRibbonNearDist && d1 < kRibbonNearDist)
        return;

    if (d0 < kRibbonNearDist) {
        const float denom = d1 - d0;
        if (std::fabs(denom) < 1.0e-6f)
            return;
        const float t = (kRibbonNearDist - d0) / denom;
        p0.x += (p1.x - p0.x) * t;
        p0.y += (p1.y - p0.y) * t;
        p0.z += (p1.z - p0.z) * t;
    } else if (d1 < kRibbonNearDist) {
        const float denom = d1 - d0;
        if (std::fabs(denom) < 1.0e-6f)
            return;
        const float t = (kRibbonNearDist - d0) / denom;
        p1.x = p0.x + (p1.x - p0.x) * t;
        p1.y = p0.y + (p1.y - p0.y) * t;
        p1.z = p0.z + (p1.z - p0.z) * t;
    }

    const Vec3f dir{p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
    if (dot3(dir, dir) < 1.0e-8f)
        return;

    const Vec3f mid{0.5f * (p0.x + p1.x), 0.5f * (p0.y + p1.y),
                    0.5f * (p0.z + p1.z)};
    const float dist = std::max(length3(mid), 0.25f);

    Vec3f side = cross3(dir, mid);
    float sideLen = length3(side);
    if (sideLen < 1.0e-5f) {
        side = cross3(dir, Vec3f{0.0f, 1.0f, 0.0f});
        sideLen = length3(side);
        if (sideLen < 1.0e-5f) {
            side = cross3(dir, Vec3f{1.0f, 0.0f, 0.0f});
            sideLen = length3(side);
            if (sideLen < 1.0e-5f)
                return;
        }
    }

    const float halfW =
        std::clamp(0.00155f * widthScale * dist, 0.0024f, 0.11f);
    const float s = halfW / sideLen;
    side.x *= s;
    side.y *= s;
    side.z *= s;

    const Vec3f v0{p0.x + side.x, p0.y + side.y, p0.z + side.z};
    const Vec3f v1{p0.x - side.x, p0.y - side.y, p0.z - side.z};
    const Vec3f v2{p1.x - side.x, p1.y - side.y, p1.z - side.z};
    const Vec3f v3{p1.x + side.x, p1.y + side.y, p1.z + side.z};

    // Emit both windings so neither CW nor CCW backface culling can hide edges
    out.push_back(GpuQuad{v0, v1, v2, v3, r, g, b, a});
    out.push_back(GpuQuad{v0, v3, v2, v1, r, g, b, a});
}

struct ClipVec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

void mulMat4(const float a[16], const float b[16], float out[16]) {
    for (int col = 0; col < 4; ++col) {
        const float b0 = b[col * 4 + 0];
        const float b1 = b[col * 4 + 1];
        const float b2 = b[col * 4 + 2];
        const float b3 = b[col * 4 + 3];
        for (int row = 0; row < 4; ++row) {
            out[col * 4 + row] = a[0 * 4 + row] * b0 +
                                 a[1 * 4 + row] * b1 +
                                 a[2 * 4 + row] * b2 +
                                 a[3 * 4 + row] * b3;
        }
    }
}

ClipVec4 transformPoint(const float m[16], float x, float y, float z) {
    return ClipVec4{
        m[0] * x + m[4] * y + m[8] * z + m[12],
        m[1] * x + m[5] * y + m[9] * z + m[13],
        m[2] * x + m[6] * y + m[10] * z + m[14],
        m[3] * x + m[7] * y + m[11] * z + m[15],
    };
}

bool readMatrixStackTop(const void *camera, std::size_t stackOffset, float out[16]) {
    const char *stack = static_cast<const char *>(camera) + stackOffset;
    std::uint64_t start = 0;
    std::uint64_t size = 0;
    std::memcpy(&start, stack + kDequeStart, sizeof(start));
    std::memcpy(&size, stack + kDequeSize, sizeof(size));
    if (size == 0 || size > 4096)
        return false;

    const void *blocks = guard::readPtr(stack, kDequeBlocks);
    if (blocks == nullptr)
        return false;

    const std::uint64_t idx = start + size - 1u;
    const void *chunk = guard::readPtr(
        blocks, static_cast<std::size_t>((idx >> 6u) * sizeof(void *)));
    if (chunk == nullptr)
        return false;

    const char *elem = static_cast<const char *>(chunk) +
                       static_cast<std::size_t>((idx & 0x3Fu) * 64u);
    std::memcpy(out, elem, 64u);
    return std::isfinite(out[0]) && std::isfinite(out[5]) &&
           std::isfinite(out[10]) && std::isfinite(out[15]);
}

bool clipSegment2D(float &x0, float &y0, float &x1, float &y1,
                   float minX, float minY, float maxX, float maxY) {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    float t0 = 0.0f;
    float t1 = 1.0f;

    auto clipEdge = [&](float p, float q) -> bool {
        if (std::fabs(p) < 1.0e-6f)
            return q >= 0.0f;
        const float r = q / p;
        if (p < 0.0f) {
            if (r > t1)
                return false;
            if (r > t0)
                t0 = r;
        } else {
            if (r < t0)
                return false;
            if (r < t1)
                t1 = r;
        }
        return true;
    };

    if (!clipEdge(-dx, x0 - minX))
        return false;
    if (!clipEdge(dx, maxX - x0))
        return false;
    if (!clipEdge(-dy, y0 - minY))
        return false;
    if (!clipEdge(dy, maxY - y0))
        return false;

    const float nx0 = x0 + t0 * dx;
    const float ny0 = y0 + t0 * dy;
    const float nx1 = x0 + t1 * dx;
    const float ny1 = y0 + t1 * dy;
    x0 = nx0;
    y0 = ny0;
    x1 = nx1;
    y1 = ny1;
    return true;
}

bool emitProjectedSegment(const ClipVec4 &a, const ClipVec4 &b,
                          float surfaceW, float surfaceH, float signY,
                          std::uint32_t color, float strokeWidth,
                          std::vector<pl::modmenu::DrawCommand> &out) {
    ClipVec4 c0 = a;
    ClipVec4 c1 = b;

    if (c0.w < kClipNearW && c1.w < kClipNearW)
        return false;

    if (c0.w < kClipNearW) {
        const float denom = c1.w - c0.w;
        if (std::fabs(denom) < 1.0e-6f)
            return false;
        const float t = (kClipNearW - c0.w) / denom;
        c0.x += (c1.x - c0.x) * t;
        c0.y += (c1.y - c0.y) * t;
        c0.z += (c1.z - c0.z) * t;
        c0.w = kClipNearW;
    } else if (c1.w < kClipNearW) {
        const float denom = c1.w - c0.w;
        if (std::fabs(denom) < 1.0e-6f)
            return false;
        const float t = (kClipNearW - c0.w) / denom;
        c1.x = c0.x + (c1.x - c0.x) * t;
        c1.y = c0.y + (c1.y - c0.y) * t;
        c1.z = c0.z + (c1.z - c0.z) * t;
        c1.w = kClipNearW;
    }

    const float invW0 = 1.0f / c0.w;
    const float invW1 = 1.0f / c1.w;
    float sx0 = (0.5f + 0.5f * (c0.x * invW0)) * surfaceW;
    float sy0 = (0.5f + 0.5f * signY * (c0.y * invW0)) * surfaceH;
    float sx1 = (0.5f + 0.5f * (c1.x * invW1)) * surfaceW;
    float sy1 = (0.5f + 0.5f * signY * (c1.y * invW1)) * surfaceH;

    if (!std::isfinite(sx0) || !std::isfinite(sy0) ||
        !std::isfinite(sx1) || !std::isfinite(sy1))
        return false;

    constexpr float kPad = 128.0f;
    if (!clipSegment2D(sx0, sy0, sx1, sy1, -kPad, -kPad,
                       surfaceW + kPad, surfaceH + kPad))
        return false;

    pl::modmenu::DrawCommand cmd{};
    cmd.type = pl::modmenu::DrawCommandType::Line;
    cmd.x = sx0;
    cmd.y = sy0;
    cmd.w = sx1 - sx0;
    cmd.h = sy1 - sy0;
    cmd.color = color;
    cmd.size = strokeWidth;
    out.push_back(std::move(cmd));
    return true;
}

constexpr int kBoxEdges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7},
};

bool boxOutsideFrustum(const ClipVec4 corners[8]) {
    int behind = 0, left = 0, right = 0, bottom = 0, top = 0;
    for (int i = 0; i < 8; ++i) {
        const auto &c = corners[i];
        if (c.w < kClipNearW)
            ++behind;
        if (c.x < -c.w)
            ++left;
        if (c.x > c.w)
            ++right;
        if (c.y < -c.w)
            ++bottom;
        if (c.y > c.w)
            ++top;
    }
    return behind == 8 || left == 8 || right == 8 || bottom == 8 || top == 8;
}

GridEdge makeCanonicalEdge(int x0, int y0, int z0, int x1, int y1, int z1) {
    if (x1 < x0 || (x1 == x0 && (y1 < y0 || (y1 == y0 && z1 < z0)))) {
        return GridEdge{x1, y1, z1, x0, y0, z0};
    }
    return GridEdge{x0, y0, z0, x1, y1, z1};
}

bool edgeLess(const GridEdge &a, const GridEdge &b) {
    if (a.x0 != b.x0) return a.x0 < b.x0;
    if (a.y0 != b.y0) return a.y0 < b.y0;
    if (a.z0 != b.z0) return a.z0 < b.z0;
    if (a.x1 != b.x1) return a.x1 < b.x1;
    if (a.y1 != b.y1) return a.y1 < b.y1;
    return a.z1 < b.z1;
}

bool edgeEqual(const GridEdge &a, const GridEdge &b) {
    return a.x0 == b.x0 && a.y0 == b.y0 && a.z0 == b.z0 &&
           a.x1 == b.x1 && a.y1 == b.y1 && a.z1 == b.z1;
}

} // namespace

GameHook &GameHook::get() {
    static GameHook instance;
    return instance;
}

bool GameHook::start() {
    if (mInstalled.load(std::memory_order_acquire))
        return true;

    if (mStarting.exchange(true, std::memory_order_acq_rel))
        return true;

    mStopRequested.store(false, std::memory_order_release);
    if (mWatcher.joinable())
        mWatcher.join();
    mWatcher = std::thread([this] { watchLoop(); });

    logger().info("waiting for {} before installing Ore ESP hooks", kGameModule);
    return true;
}

void GameHook::stop() {
    mStopRequested.store(true, std::memory_order_release);
    if (mWatcher.joinable() && mWatcher.get_id() != std::this_thread::get_id())
        mWatcher.join();
    mStarting.store(false, std::memory_order_release);

    mEnabled.store(false, std::memory_order_release);
    storeSnapshot(nullptr);
}

void GameHook::applyConfig(const ModConfig &cfg) {
    const int oldRadius = mScanRadius.exchange(cfg.scanRadius, std::memory_order_acq_rel);
    mScanIntervalTicks.store(cfg.scanIntervalTicks, std::memory_order_release);
    mMaxVeins.store(cfg.maxVeins, std::memory_order_release);
    mLineWidth.store(static_cast<float>(cfg.lineWidth), std::memory_order_release);
    mMergeVeins.store(cfg.mergeVeins, std::memory_order_release);
    mShowLabels.store(cfg.showLabels, std::memory_order_release);
    mShowTracers.store(cfg.showTracers, std::memory_order_release);
    mHudOutlines.store(cfg.hudOutlines, std::memory_order_release);

    std::uint32_t mask = 0u;
    if (cfg.showAncientDebris) mask |= bitForOre(OreKind::AncientDebris);
    if (cfg.showDiamond)       mask |= bitForOre(OreKind::Diamond);
    if (cfg.showEmerald)       mask |= bitForOre(OreKind::Emerald);
    if (cfg.showGold)          mask |= bitForOre(OreKind::Gold);
    if (cfg.showLapis)         mask |= bitForOre(OreKind::Lapis);
    if (cfg.showRedstone)      mask |= bitForOre(OreKind::Redstone);
    if (cfg.showIron)          mask |= bitForOre(OreKind::Iron);
    if (cfg.showQuartz)        mask |= bitForOre(OreKind::Quartz);
    if (cfg.showCopper)        mask |= bitForOre(OreKind::Copper);
    if (cfg.showCoal)          mask |= bitForOre(OreKind::Coal);
    mOreMask.store(mask, std::memory_order_release);

    // Pure lock-free state update: never call pl::modmenu::submitDrawCommands
    // here because applyConfig() runs inside ModMenu's mutex-locked UI callbacks.
    // onRenderLevel/onClientUpdate clear HudOverlay safely on the next game frame.
    const bool wasEnabled = mEnabled.exchange(cfg.enabled, std::memory_order_acq_rel);
    if (!cfg.enabled) {
        return;
    }

    if (!wasEnabled || oldRadius != cfg.scanRadius) {
        mRescanEpoch.fetch_add(1u, std::memory_order_acq_rel);
    }
}

void GameHook::watchLoop() {
    bool mapped = false;
    for (int i = 0; i < 12000 && !mStopRequested.load(std::memory_order_acquire); ++i) {
        if (libraryMapped()) {
            mapped = true;
            break;
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(i < 6000 ? 1 : 50));
    }
    if (!mapped) {
        logger().error("{} never appeared - Ore ESP inactive", kGameModule);
        mStarting.store(false, std::memory_order_release);
        return;
    }

    logger().info("{} mapped, settling before signature scan", kGameModule);
    for (int waited = 0;
         waited < kSettleMs && !mStopRequested.load(std::memory_order_acquire);
         waited += 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (mStopRequested.load(std::memory_order_acquire)) {
        mStarting.store(false, std::memory_order_release);
        return;
    }

    installNow();
    mStarting.store(false, std::memory_order_release);
}

void GameHook::installNow() {
    if (mInstalled.load(std::memory_order_acquire))
        return;

    const auto tickAddr =
        pl::memory::resolveSignature(kSigNormalTick, kGameModule);
    const auto blockAddr =
        pl::memory::resolveSignature(kSigBlockSourceGetBlock, kGameModule);
    const auto renderAddr =
        pl::memory::resolveSignature(kSigRenderLevel, kGameModule);
    const auto updateAddr =
        pl::memory::resolveSignature(kSigClientInstanceUpdate, kGameModule);

    const auto tessBeginAddr =
        pl::memory::resolveSignature(kSigTessellatorBegin, kGameModule);
    const auto tessColorAddr =
        pl::memory::resolveSignature(kSigTessellatorColor, kGameModule);
    const auto tessVertexAddr =
        pl::memory::resolveSignature(kSigTessellatorVertex, kGameModule);
    auto renderMeshAddr =
        pl::memory::resolveSignature(kSigRenderMeshImmediately2, kGameModule);
    if (renderMeshAddr == 0) {
        renderMeshAddr =
            pl::memory::resolveSignature(kSigRenderMeshImmediately, kGameModule);
    }
    const auto rmgSigAddr =
        pl::memory::resolveSignature(kSigRenderMaterialGroupCommon, kGameModule);

    if (rmgSigAddr != 0) {
        const auto groupBase = resolveAdrpAdd(
            reinterpret_cast<const std::uint32_t *>(rmgSigAddr), 8, 0);
        if (groupBase != 0) {
            mRenderMaterialGroup = groupBase + kRenderMaterialGroupOffset;
        }
    }

    logger().info(
        "resolved tick=0x{:x} getBlock=0x{:x} render=0x{:x} tess=0x{:x}/0x{:x}/0x{:x} mesh=0x{:x} rmg=0x{:x}",
        tickAddr, blockAddr, renderAddr, tessBeginAddr, tessColorAddr,
        tessVertexAddr, renderMeshAddr, mRenderMaterialGroup);

    if (tickAddr == 0 || blockAddr == 0 || renderAddr == 0) {
        logger().error("required Ore ESP signatures missing - mod inactive");
        return;
    }

    mGetBlockFn = reinterpret_cast<void *>(blockAddr);
    mTessBeginFn = reinterpret_cast<void *>(tessBeginAddr);
    mTessColorFn = reinterpret_cast<void *>(tessColorAddr);
    mTessVertexFn = reinterpret_cast<void *>(tessVertexAddr);
    mRenderMeshFn = reinterpret_cast<void *>(renderMeshAddr);

    mTickHook = new pl::memory::HookHandle(
        reinterpret_cast<void *>(tickAddr),
        reinterpret_cast<void *>(&GameHook::normalTickDetour),
        &mOrigNormalTick, pl::memory::HookPriority::Normal);

    mRenderHook = new pl::memory::HookHandle(
        reinterpret_cast<void *>(renderAddr),
        reinterpret_cast<void *>(&GameHook::renderLevelDetour),
        &mOrigRenderLevel, pl::memory::HookPriority::Normal);

    if (updateAddr != 0) {
        mUpdateHook = new pl::memory::HookHandle(
            reinterpret_cast<void *>(updateAddr),
            reinterpret_cast<void *>(&GameHook::clientUpdateDetour),
            &mOrigClientUpdate, pl::memory::HookPriority::Normal);
    }

    if (!mTickHook->installed() || !mRenderHook->installed()) {
        logger().error("hook installation failed (tick={} render={})",
                       mTickHook->installed(), mRenderHook->installed());
        delete mTickHook;
        delete mRenderHook;
        delete mUpdateHook;
        mTickHook = nullptr;
        mRenderHook = nullptr;
        mUpdateHook = nullptr;
        return;
    }

    mInstalled.store(true, std::memory_order_release);
    logger().info("Ore ESP 1.0.0hf1 hooks active");
}

void GameHook::clearOverlay() {
    if (mOverlayDirty.exchange(false, std::memory_order_acq_rel)) {
        pl::modmenu::submitDrawCommands(kModuleId, {});
    }
}

void GameHook::resetScanState(bool resetCache) {
    mScanActive = false;
    mStagingBlocks.clear();
    mKnownBlocks.clear();
    mCooldownTicks = 0;
    mHasCompletedScan = false;
    if (resetCache) {
        mBlockCache.fill(CacheSlot{});
    }
}

std::shared_ptr<const ScanSnapshot> GameHook::loadSnapshot() const {
    std::lock_guard lock(mSnapshotMutex);
    return mSnapshot;
}

void GameHook::storeSnapshot(std::shared_ptr<const ScanSnapshot> snap) {
    std::lock_guard lock(mSnapshotMutex);
    mSnapshot = std::move(snap);
}

// ---------------------------------------------------------------------------
// Hook detours
// ---------------------------------------------------------------------------
void GameHook::normalTickDetour(void *localPlayer) {
    auto &self = GameHook::get();
    if (self.mOrigNormalTick != nullptr) {
        reinterpret_cast<NormalTickFn>(self.mOrigNormalTick)(localPlayer);
    }
    self.onNormalTick(localPlayer);
}

void GameHook::renderLevelDetour(void *levelRenderer, void *screenContext,
                                 void *frameRenderObj) {
    auto &self = GameHook::get();
    if (self.mOrigRenderLevel != nullptr) {
        reinterpret_cast<RenderLevelFn>(self.mOrigRenderLevel)(
            levelRenderer, screenContext, frameRenderObj);
    }
    self.onRenderLevel(levelRenderer, screenContext);
}

void *GameHook::clientUpdateDetour(void *clientInstance, bool arg) {
    auto &self = GameHook::get();
    void *res = nullptr;
    if (self.mOrigClientUpdate != nullptr) {
        res = reinterpret_cast<ClientUpdateFn>(self.mOrigClientUpdate)(
            clientInstance, arg);
    }
    self.onClientUpdate();
    return res;
}

// ---------------------------------------------------------------------------
// Fast Block* -> OreKind classifier (4096-entry open-addressed L1 cache)
// ---------------------------------------------------------------------------
OreKind GameHook::classifyBlock(const void *block) {
    if (!guard::isPointer(block))
        return OreKind::None;

    std::uintptr_t h =
        (reinterpret_cast<std::uintptr_t>(block) & 0x0000FFFFFFFFFFFFull) >> 4u;
    h ^= (h >> 13u);
    h *= 0x9E3779B97F4A7C15ull;
    const std::size_t baseIdx = static_cast<std::size_t>(h) & (kCacheSize - 1u);

    std::size_t insertIdx = baseIdx;
    bool foundEmpty = false;
    for (std::size_t step = 0; step < 8; ++step) {
        const std::size_t idx = (baseIdx + step) & (kCacheSize - 1u);
        const auto &slot = mBlockCache[idx];
        if (slot.occupied) {
            if (slot.blockPtr == block)
                return slot.kind;
        } else if (!foundEmpty) {
            insertIdx = idx;
            foundEmpty = true;
        }
    }

    char nameBuf[80]{};
    bool readOk = false;
    const bool noFault = guard::run([&] {
        const void *blockType = guard::readPtr(block, kBlockBlockType);
        if (blockType != nullptr) {
            readOk = copyBlockTypeName(blockType, nameBuf, sizeof(nameBuf));
        }
    });

    const OreKind kind =
        (noFault && readOk) ? classifyName(nameBuf) : OreKind::None;

    mBlockCache[insertIdx] = CacheSlot{
        .blockPtr = block,
        .kind = kind,
        .occupied = true,
    };
    return kind;
}

// ---------------------------------------------------------------------------
// Time-sliced tick-thread ore scanner + 6-connected vein builder
// ---------------------------------------------------------------------------
void GameHook::onNormalTick(void *localPlayer) {
    if (!mEnabled.load(std::memory_order_relaxed) ||
        !guard::isPointer(localPlayer) || mGetBlockFn == nullptr) {
        return;
    }

    Vec3f feet{};
    void *dimension = nullptr;
    void *blockSource = nullptr;
    std::int16_t minY = 0;
    std::int16_t maxY = 0;
    bool validWorld = false;

    const bool ok = guard::run([&] {
        const void *svc = guard::readPtr(localPlayer, kActorStateVector);
        if (svc == nullptr)
            return;
        std::memcpy(&feet, svc, sizeof(feet));
        if (!sanePos(feet))
            return;

        dimension = guard::readPtr(localPlayer, kActorDimension);
        if (dimension == nullptr)
            return;

        blockSource = guard::readPtr(dimension, kDimensionBlockSource);
        if (blockSource == nullptr)
            return;

        const void *vtable = guard::readPtr(blockSource, 0);
        if (vtable == nullptr)
            return;

        const char *bsBytes = static_cast<const char *>(blockSource);
        std::memcpy(&minY, bsBytes + kBlockSourceMinY, sizeof(minY));
        std::memcpy(&maxY, bsBytes + kBlockSourceMaxY, sizeof(maxY));
        if (minY >= maxY || minY < -2048 || maxY > 4096)
            return;

        validWorld = true;
    });

    if (!ok || !validWorld)
        return;

    if (localPlayer != mLastPlayer || dimension != mLastDimension ||
        blockSource != mLastBlockSource) {
        const bool dimChanged =
            (localPlayer != mLastPlayer || dimension != mLastDimension);
        mLastPlayer = localPlayer;
        mLastDimension = dimension;
        mLastBlockSource = blockSource;
        resetScanState(dimChanged);
        if (dimChanged) {
            storeSnapshot(nullptr);
        }
    }

    if (mHasCompletedScan && !mKnownBlocks.empty()) {
        pruneBrokenNearbyOres(blockSource, feet);
    }

    const std::uint64_t epoch = mRescanEpoch.load(std::memory_order_acquire);
    const bool forceRescan = (epoch != mObservedEpoch);
    if (forceRescan) {
        mObservedEpoch = epoch;
        mScanActive = false;
        mCooldownTicks = 0;
    }

    if (!mScanActive) {
        const float dxMove = feet.x - mLastCompletedPos.x;
        const float dyMove = feet.y - mLastCompletedPos.y;
        const float dzMove = feet.z - mLastCompletedPos.z;
        const float movedSq = dxMove * dxMove + dyMove * dyMove + dzMove * dzMove;

        if (!forceRescan && mHasCompletedScan && movedSq < 9.0f &&
            mCooldownTicks > 0) {
            --mCooldownTicks;
            return;
        }

        mActiveRadius = std::clamp(mScanRadius.load(std::memory_order_relaxed), 8, 48);
        mScanCenter = BlockPos{
            static_cast<std::int32_t>(std::floor(feet.x)),
            static_cast<std::int32_t>(std::floor(feet.y)),
            static_cast<std::int32_t>(std::floor(feet.z)),
        };
        mActiveMinY = std::max<int>(minY, mScanCenter.y - mActiveRadius);
        mActiveMaxY = std::min<int>(static_cast<int>(maxY) - 1,
                                    mScanCenter.y + mActiveRadius);
        if (mActiveMinY > mActiveMaxY)
            return;

        mCursorDx = -mActiveRadius;
        mCursorDz = -mActiveRadius;
        mStagingBlocks.clear();
        mStagingBlocks.reserve(512);
        mScanActive = true;
    }

    const auto getBlock = reinterpret_cast<GetBlockFn>(mGetBlockFn);
    int scannedBlocks = 0;

    while (mCursorDx <= mActiveRadius && scannedBlocks < kBlocksBudgetPerTick) {
        const std::int32_t wx = mScanCenter.x + mCursorDx;
        const std::int32_t wz = mScanCenter.z + mCursorDz;

        for (int wy = mActiveMinY; wy <= mActiveMaxY; ++wy) {
            const BlockPos pos{wx, wy, wz};
            const void *block = getBlock(blockSource, &pos);
            if (block != nullptr) {
                const OreKind kind = classifyBlock(block);
                if (kind != OreKind::None &&
                    mStagingBlocks.size() < kMaxRawOreBlocks) {
                    mStagingBlocks.push_back(RawOreBlock{wx, wy, wz, kind});
                }
            }
        }

        scannedBlocks += (mActiveMaxY - mActiveMinY + 1);

        ++mCursorDz;
        if (mCursorDz > mActiveRadius) {
            mCursorDz = -mActiveRadius;
            ++mCursorDx;
        }
    }

    if (mCursorDx > mActiveRadius) {
        mScanActive = false;
        mLastCompletedPos = feet;
        mHasCompletedScan = true;
        mCompletedCenter = mScanCenter;
        mCompletedRadius = mActiveRadius;
        mCooldownTicks =
            std::clamp(mScanIntervalTicks.load(std::memory_order_relaxed), 4, 40);
        mKnownBlocks = mStagingBlocks;
        finalizeScanPass(mKnownBlocks, mCompletedCenter, mCompletedRadius);
    }
}

void GameHook::pruneBrokenNearbyOres(void *blockSource, const Vec3f &feet) {
    const auto getBlock = reinterpret_cast<GetBlockFn>(mGetBlockFn);
    constexpr float kVerifyDistSq = 9.5f * 9.5f;
    bool removedAny = false;

    std::size_t writeIdx = 0;
    for (std::size_t i = 0; i < mKnownBlocks.size(); ++i) {
        const auto &blk = mKnownBlocks[i];
        const float dx = (static_cast<float>(blk.x) + 0.5f) - feet.x;
        const float dy = (static_cast<float>(blk.y) + 0.5f) - feet.y;
        const float dz = (static_cast<float>(blk.z) + 0.5f) - feet.z;
        if (dx * dx + dy * dy + dz * dz <= kVerifyDistSq) {
            const BlockPos pos{blk.x, blk.y, blk.z};
            const void *liveBlock = getBlock(blockSource, &pos);
            if (liveBlock == nullptr || classifyBlock(liveBlock) != blk.kind) {
                removedAny = true;
                continue;
            }
        }
        if (writeIdx != i) {
            mKnownBlocks[writeIdx] = blk;
        }
        ++writeIdx;
    }

    if (removedAny) {
        mKnownBlocks.resize(writeIdx);
        finalizeScanPass(mKnownBlocks, mCompletedCenter, mCompletedRadius);
    }
}

void GameHook::finalizeScanPass(const std::vector<RawOreBlock> &sourceBlocks,
                                const BlockPos &center, int radius) {
    auto snap = std::make_shared<ScanSnapshot>();
    snap->scanOrigin = Vec3f{
        static_cast<float>(center.x) + 0.5f,
        static_cast<float>(center.y) + 0.5f,
        static_cast<float>(center.z) + 0.5f,
    };
    snap->scanRadius = radius;

    if (!sourceBlocks.empty()) {
        std::vector<RawOreBlock> sorted = sourceBlocks;
        auto blockLess = [](const RawOreBlock &a, const RawOreBlock &b) {
            if (a.kind != b.kind) return a.kind < b.kind;
            if (a.x != b.x) return a.x < b.x;
            if (a.y != b.y) return a.y < b.y;
            return a.z < b.z;
        };
        std::sort(sorted.begin(), sorted.end(), blockLess);

        const std::size_t n = sorted.size();
        std::vector<std::uint8_t> visited(n, 0u);
        std::vector<std::size_t> queue;
        std::vector<GridEdge> veinEdges;
        queue.reserve(32);
        veinEdges.reserve(128);

        constexpr int kNeighbors[6][3] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
            {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
        };

        for (std::size_t i = 0; i < n; ++i) {
            if (visited[i])
                continue;

            visited[i] = 1u;
            queue.clear();
            queue.push_back(i);
            veinEdges.clear();

            const OreKind kind = sorted[i].kind;
            std::int32_t minX = sorted[i].x;
            std::int32_t minY = sorted[i].y;
            std::int32_t minZ = sorted[i].z;
            std::int32_t maxX = minX;
            std::int32_t maxY = minY;
            std::int32_t maxZ = minZ;
            double sumX = 0.0;
            double sumY = 0.0;
            double sumZ = 0.0;

            for (std::size_t q = 0; q < queue.size(); ++q) {
                const auto &blk = sorted[queue[q]];
                minX = std::min(minX, blk.x);
                minY = std::min(minY, blk.y);
                minZ = std::min(minZ, blk.z);
                maxX = std::max(maxX, blk.x);
                maxY = std::max(maxY, blk.y);
                maxZ = std::max(maxZ, blk.z);
                sumX += static_cast<double>(blk.x) + 0.5;
                sumY += static_cast<double>(blk.y) + 0.5;
                sumZ += static_cast<double>(blk.z) + 0.5;

                const int x0 = blk.x, y0 = blk.y, z0 = blk.z;
                const int x1 = x0 + 1, y1 = y0 + 1, z1 = z0 + 1;
                const int corners[8][3] = {
                    {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1},
                    {x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1},
                };
                for (const auto &e : kBoxEdges) {
                    const auto *p0 = corners[e[0]];
                    const auto *p1 = corners[e[1]];
                    veinEdges.push_back(makeCanonicalEdge(
                        p0[0], p0[1], p0[2], p1[0], p1[1], p1[2]));
                }

                for (const auto &nb : kNeighbors) {
                    const RawOreBlock key{blk.x + nb[0], blk.y + nb[1],
                                          blk.z + nb[2], kind};
                    auto it = std::lower_bound(
                        sorted.begin(), sorted.end(), key, blockLess);
                    if (it != sorted.end() && it->kind == key.kind &&
                        it->x == key.x && it->y == key.y && it->z == key.z) {
                        const auto idx =
                            static_cast<std::size_t>(it - sorted.begin());
                        if (!visited[idx]) {
                            visited[idx] = 1u;
                            queue.push_back(idx);
                        }
                    }
                }
            }

            std::sort(veinEdges.begin(), veinEdges.end(), edgeLess);
            veinEdges.erase(
                std::unique(veinEdges.begin(), veinEdges.end(), edgeEqual),
                veinEdges.end());

            const double invCount = 1.0 / static_cast<double>(queue.size());
            const auto edgeOffset = static_cast<std::uint32_t>(snap->edges.size());
            snap->edges.insert(snap->edges.end(), veinEdges.begin(), veinEdges.end());

            snap->veins.push_back(OreVein{
                .kind = kind,
                .blockCount = static_cast<std::uint16_t>(
                    std::min<std::size_t>(queue.size(), 65535u)),
                .minX = minX,
                .minY = minY,
                .minZ = minZ,
                .maxX = maxX,
                .maxY = maxY,
                .maxZ = maxZ,
                .centerX = static_cast<float>(sumX * invCount),
                .centerY = static_cast<float>(sumY * invCount),
                .centerZ = static_cast<float>(sumZ * invCount),
                .edgeOffset = edgeOffset,
                .edgeCount = static_cast<std::uint32_t>(veinEdges.size()),
            });
        }
    }

    if (!mLoggedFirstScan.exchange(true, std::memory_order_acq_rel)) {
        logger().info("first scan complete at ({}, {}, {}) r={}: {} ore blocks in {} veins",
                      center.x, center.y, center.z, radius,
                      sourceBlocks.size(), snap->veins.size());
    }

    storeSnapshot(std::move(snap));
}

// ---------------------------------------------------------------------------
// RenderLevel: In-engine 3D GPU through-wall ribbons + HudOverlay labels/fallback
// ---------------------------------------------------------------------------
void GameHook::onRenderLevel(void *levelRenderer, void *screenContext) {
    if (!mEnabled.load(std::memory_order_relaxed)) {
        clearOverlay();
        return;
    }

    if (!guard::isPointer(levelRenderer) || !guard::isPointer(screenContext))
        return;

    const auto snap = loadSnapshot();
    const std::uint32_t oreMask = mOreMask.load(std::memory_order_relaxed);
    mLastWorldRenderMs.store(steadyNowMs(), std::memory_order_relaxed);

    if (!snap || snap->veins.empty() || oreMask == 0u) {
        clearOverlay();
        return;
    }

    Vec3f camPos{};
    Vec3f camTargetPos{};
    void *tessellator = nullptr;
    float *colorHolder = nullptr;
    float viewMat[16]{};
    float worldMat[16]{};
    float projMat[16]{};
    bool camOk = false;
    bool matricesOk = false;

    const bool noFault = guard::run([&] {
        const void *lrp = guard::readPtr(levelRenderer, kLevelRendererPlayer);
        if (lrp == nullptr)
            return;
        std::memcpy(&camPos, static_cast<const char *>(lrp) + kLrpCamPos,
                    sizeof(camPos));
        std::memcpy(&camTargetPos,
                    static_cast<const char *>(lrp) + kLrpCamTargetPos,
                    sizeof(camTargetPos));
        if (!sanePos(camPos))
            return;
        camOk = true;

        tessellator = guard::readPtr(screenContext, kScreenContextTessellator);
        colorHolder = static_cast<float *>(
            guard::readPtr(screenContext, kScreenContextColorHolder));

        const void *camera = guard::readPtr(screenContext, kScreenContextCamera);
        if (camera != nullptr &&
            readMatrixStackTop(camera, kCameraViewStack, viewMat) &&
            readMatrixStackTop(camera, kCameraWorldStack, worldMat) &&
            readMatrixStackTop(camera, kCameraProjStack, projMat)) {
            matricesOk = true;
        }
    });

    if (!noFault || !camOk)
        return;

    Vec3f camFwd{camTargetPos.x - camPos.x,
                 camTargetPos.y - camPos.y,
                 camTargetPos.z - camPos.z};
    const float fwdLen = length3(camFwd);
    if (fwdLen > 1.0e-4f) {
        camFwd.x /= fwdLen;
        camFwd.y /= fwdLen;
        camFwd.z /= fwdLen;
    } else if (matricesOk) {
        // Fallback forward from view matrix row 2 (-Z camera axis)
        camFwd = Vec3f{-viewMat[2], -viewMat[6], -viewMat[10]};
        const float l2 = length3(camFwd);
        if (l2 > 1.0e-4f) {
            camFwd.x /= l2;
            camFwd.y /= l2;
            camFwd.z /= l2;
        } else {
            camFwd = Vec3f{0.0f, 0.0f, 1.0f};
        }
    } else {
        camFwd = Vec3f{0.0f, 0.0f, 1.0f};
    }

    if (!gUiFillMaterial && mRenderMaterialGroup != 0) {
        gUiFillMaterial = lookupMaterial(mRenderMaterialGroup, "ui_fill_color");
    }

    const bool canDrawGpu3D =
        static_cast<bool>(gUiFillMaterial) && tessellator != nullptr &&
        mTessBeginFn != nullptr && mTessColorFn != nullptr &&
        mTessVertexFn != nullptr && mRenderMeshFn != nullptr;

    const bool useGpuOutlines =
        canDrawGpu3D && !mHudOutlines.load(std::memory_order_relaxed);

    const int maxVeins = std::clamp(mMaxVeins.load(std::memory_order_relaxed), 16, 200);
    const bool mergeVeins = mMergeVeins.load(std::memory_order_relaxed);
    const bool showLabels = mShowLabels.load(std::memory_order_relaxed);
    const bool showTracers = mShowTracers.load(std::memory_order_relaxed);
    const float lineWidth =
        std::clamp(mLineWidth.load(std::memory_order_relaxed), 1.0f, 4.0f);
    const float maxDist = static_cast<float>(std::max(snap->scanRadius, 8)) + 8.0f;
    const float maxDistSq = maxDist * maxDist;

    struct Candidate {
        const OreVein *vein = nullptr;
        float dist = 0.0f;
        float score = 0.0f;
    };

    thread_local std::vector<Candidate> candidates;
    candidates.clear();
    candidates.reserve(snap->veins.size());

    for (const auto &v : snap->veins) {
        if ((oreMask & bitForOre(v.kind)) == 0u)
            continue;

        const float dx = v.centerX - camPos.x;
        const float dy = v.centerY - camPos.y;
        const float dz = v.centerZ - camPos.z;
        const float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq > maxDistSq)
            continue;

        const float dist = std::sqrt(distSq);
        const float score = dist - oreRarityBonusMeters(v.kind);
        candidates.push_back(Candidate{&v, dist, score});
    }

    if (candidates.empty()) {
        clearOverlay();
        return;
    }

    if (static_cast<int>(candidates.size()) > maxVeins) {
        std::nth_element(candidates.begin(),
                         candidates.begin() + maxVeins,
                         candidates.end(),
                         [](const Candidate &a, const Candidate &b) {
                             return a.score < b.score;
                         });
        candidates.resize(static_cast<std::size_t>(maxVeins));
    }

    // Painter's order: draw farther veins first so closer veins render cleanly on top
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &a, const Candidate &b) {
                  return a.dist > b.dist;
              });

    float mvpMat[16]{};
    float signY = -1.0f;
    if (matricesOk) {
        float pvMat[16]{};
        mulMat4(projMat, viewMat, pvMat);
        mulMat4(pvMat, worldMat, mvpMat);
        signY = (projMat[5] < 0.0f) ? 1.0f : -1.0f;
    }

    // 1. In-engine GPU 3D through-wall ribbon mesh (zero lag, works with Hide HUD)
    if (useGpuOutlines) {
        thread_local std::vector<GpuQuad> gpuQuads;
        gpuQuads.clear();
        gpuQuads.reserve(std::min<std::size_t>(candidates.size() * 24u, kMaxGpuQuads));

        const Vec3f tracerOrigin{camFwd.x * 0.55f, camFwd.y * 0.55f,
                                 camFwd.z * 0.55f};

        for (const auto &cand : candidates) {
            if (gpuQuads.size() + 24u > kMaxGpuQuads)
                break;

            const OreVein &v = *cand.vein;
            const float bx0 = static_cast<float>(v.minX) - camPos.x;
            const float by0 = static_cast<float>(v.minY) - camPos.y;
            const float bz0 = static_cast<float>(v.minZ) - camPos.z;
            const float bx1 = static_cast<float>(v.maxX + 1) - camPos.x;
            const float by1 = static_cast<float>(v.maxY + 1) - camPos.y;
            const float bz1 = static_cast<float>(v.maxZ + 1) - camPos.z;

            if (matricesOk) {
                const ClipVec4 bboxCorners[8] = {
                    transformPoint(mvpMat, bx0, by0, bz0),
                    transformPoint(mvpMat, bx1, by0, bz0),
                    transformPoint(mvpMat, bx1, by0, bz1),
                    transformPoint(mvpMat, bx0, by0, bz1),
                    transformPoint(mvpMat, bx0, by1, bz0),
                    transformPoint(mvpMat, bx1, by1, bz0),
                    transformPoint(mvpMat, bx1, by1, bz1),
                    transformPoint(mvpMat, bx0, by1, bz1),
                };
                if (boxOutsideFrustum(bboxCorners))
                    continue;
            }

            const std::uint32_t rgb = oreBaseRgb(v.kind);
            const float cr = static_cast<float>((rgb >> 16u) & 0xFFu) / 255.0f;
            const float cg = static_cast<float>((rgb >> 8u) & 0xFFu) / 255.0f;
            const float cb = static_cast<float>(rgb & 0xFFu) / 255.0f;
            const float fade = std::clamp(
                1.15f - (cand.dist / std::max(maxDist, 1.0f)) * 0.50f,
                0.48f, 0.92f);

            if (mergeVeins || v.blockCount <= 1u) {
                const Vec3f corners3D[8] = {
                    {bx0, by0, bz0}, {bx1, by0, bz0},
                    {bx1, by0, bz1}, {bx0, by0, bz1},
                    {bx0, by1, bz0}, {bx1, by1, bz0},
                    {bx1, by1, bz1}, {bx0, by1, bz1},
                };
                for (const auto &edge : kBoxEdges) {
                    pushRibbonSegment3D(corners3D[edge[0]], corners3D[edge[1]],
                                        camFwd, lineWidth, cr, cg, cb, fade,
                                        gpuQuads);
                }
            } else {
                const std::uint32_t endEdge = v.edgeOffset + v.edgeCount;
                for (std::uint32_t ei = v.edgeOffset;
                     ei < endEdge && ei < snap->edges.size(); ++ei) {
                    if (gpuQuads.size() + 2u > kMaxGpuQuads)
                        break;
                    const GridEdge &ge = snap->edges[ei];
                    pushRibbonSegment3D(
                        Vec3f{static_cast<float>(ge.x0) - camPos.x,
                              static_cast<float>(ge.y0) - camPos.y,
                              static_cast<float>(ge.z0) - camPos.z},
                        Vec3f{static_cast<float>(ge.x1) - camPos.x,
                              static_cast<float>(ge.y1) - camPos.y,
                              static_cast<float>(ge.z1) - camPos.z},
                        camFwd, lineWidth, cr, cg, cb, fade, gpuQuads);
                }
            }

            if (showTracers && gpuQuads.size() + 2u <= kMaxGpuQuads) {
                pushRibbonSegment3D(
                    tracerOrigin,
                    Vec3f{v.centerX - camPos.x,
                          v.centerY - camPos.y,
                          v.centerZ - camPos.z},
                    camFwd, lineWidth * 0.65f, cr, cg, cb, 0.55f, gpuQuads);
            }
        }

        if (!gpuQuads.empty()) {
            const auto tessBegin = reinterpret_cast<TessBeginFn>(mTessBeginFn);
            const auto tessColor = reinterpret_cast<TessColorFn>(mTessColorFn);
            const auto tessVertex = reinterpret_cast<TessVertexFn>(mTessVertexFn);
            const auto renderMesh = reinterpret_cast<RenderMeshFn>(mRenderMeshFn);

            float savedColor[4]{1.0f, 1.0f, 1.0f, 1.0f};
            if (colorHolder != nullptr) {
                std::memcpy(savedColor, colorHolder, sizeof(savedColor));
                colorHolder[0] = 1.0f;
                colorHolder[1] = 1.0f;
                colorHolder[2] = 1.0f;
                colorHolder[3] = 1.0f;
            }

            const int vertexCount = static_cast<int>(gpuQuads.size() * 4u);
            tessBegin(tessellator, nullptr, 1 /* QuadList */, vertexCount, 0);
            for (const auto &q : gpuQuads) {
                tessColor(tessellator, q.r, q.g, q.b, q.a);
                tessVertex(tessellator, q.v0.x, q.v0.y, q.v0.z);
                tessVertex(tessellator, q.v1.x, q.v1.y, q.v1.z);
                tessVertex(tessellator, q.v2.x, q.v2.y, q.v2.z);
                tessVertex(tessellator, q.v3.x, q.v3.y, q.v3.z);
            }

            char renderPad[0x58]{};
            renderMesh(screenContext, tessellator, &gUiFillMaterial, renderPad);

            if (colorHolder != nullptr) {
                std::memcpy(colorHolder, savedColor, sizeof(savedColor));
            }

            if (!mLoggedFirstRender.exchange(true, std::memory_order_acq_rel)) {
                logger().info("first GPU ESP frame rendered: veins={} quads={}",
                              candidates.size(), gpuQuads.size());
            }
        }
    }

    // 2. HudOverlay pass: floating ore labels (+ wireframe lines when hudOutlines=true)
    const bool needHudPass = showLabels || !useGpuOutlines;
    if (!needHudPass || !matricesOk) {
        clearOverlay();
        return;
    }

    const auto surface = pl::modmenu::getHudSurfaceSize();
    if (surface.width <= 1.0f || surface.height <= 1.0f)
        return;

    const float uiScale = std::clamp(surface.height / 1080.0f, 0.65f, 1.8f);
    const float strokePx = lineWidth * uiScale * 1.15f;
    const float labelFontPx = std::clamp(21.0f * uiScale, 15.0f, 30.0f);
    const float crossX = surface.width * 0.5f;
    const float crossY = surface.height * 0.5f;
    const float labelBgBits = std::bit_cast<float>(std::uint32_t{0x8C080C14u});

    thread_local std::vector<pl::modmenu::DrawCommand> commands;
    commands.clear();
    commands.reserve(std::min<std::size_t>(
        candidates.size() * (useGpuOutlines ? 2u : 16u), kMaxDrawCommands));

    for (const auto &cand : candidates) {
        if (commands.size() + 14u > kMaxDrawCommands)
            break;

        const OreVein &v = *cand.vein;
        const float bx0 = static_cast<float>(v.minX) - camPos.x;
        const float by0 = static_cast<float>(v.minY) - camPos.y;
        const float bz0 = static_cast<float>(v.minZ) - camPos.z;
        const float bx1 = static_cast<float>(v.maxX + 1) - camPos.x;
        const float by1 = static_cast<float>(v.maxY + 1) - camPos.y;
        const float bz1 = static_cast<float>(v.maxZ + 1) - camPos.z;

        const ClipVec4 bboxCorners[8] = {
            transformPoint(mvpMat, bx0, by0, bz0),
            transformPoint(mvpMat, bx1, by0, bz0),
            transformPoint(mvpMat, bx1, by0, bz1),
            transformPoint(mvpMat, bx0, by0, bz1),
            transformPoint(mvpMat, bx0, by1, bz0),
            transformPoint(mvpMat, bx1, by1, bz0),
            transformPoint(mvpMat, bx1, by1, bz1),
            transformPoint(mvpMat, bx0, by1, bz1),
        };

        if (boxOutsideFrustum(bboxCorners))
            continue;

        const std::uint32_t rgb = oreBaseRgb(v.kind);

        if (!useGpuOutlines) {
            const float fade = std::clamp(
                1.15f - (cand.dist / std::max(maxDist, 1.0f)) * 0.55f,
                0.45f, 1.0f);
            const auto wireAlpha =
                static_cast<std::uint8_t>(std::lround(235.0f * fade));
            const std::uint32_t wireColor = withAlpha(rgb, wireAlpha);

            if (mergeVeins || v.blockCount <= 1u) {
                for (const auto &edge : kBoxEdges) {
                    if (commands.size() >= kMaxDrawCommands)
                        break;
                    emitProjectedSegment(
                        bboxCorners[edge[0]], bboxCorners[edge[1]],
                        surface.width, surface.height, signY,
                        wireColor, strokePx, commands);
                }
            } else {
                const std::uint32_t endEdge = v.edgeOffset + v.edgeCount;
                for (std::uint32_t ei = v.edgeOffset;
                     ei < endEdge && ei < snap->edges.size(); ++ei) {
                    if (commands.size() >= kMaxDrawCommands)
                        break;
                    const GridEdge &ge = snap->edges[ei];
                    const ClipVec4 p0 = transformPoint(
                        mvpMat,
                        static_cast<float>(ge.x0) - camPos.x,
                        static_cast<float>(ge.y0) - camPos.y,
                        static_cast<float>(ge.z0) - camPos.z);
                    const ClipVec4 p1 = transformPoint(
                        mvpMat,
                        static_cast<float>(ge.x1) - camPos.x,
                        static_cast<float>(ge.y1) - camPos.y,
                        static_cast<float>(ge.z1) - camPos.z);
                    emitProjectedSegment(
                        p0, p1, surface.width, surface.height, signY,
                        wireColor, strokePx, commands);
                }
            }

            if (showTracers && commands.size() < kMaxDrawCommands) {
                const ClipVec4 centerClip = transformPoint(
                    mvpMat,
                    v.centerX - camPos.x,
                    v.centerY - camPos.y,
                    v.centerZ - camPos.z);
                if (centerClip.w >= 0.15f) {
                    float tx =
                        (0.5f + 0.5f * (centerClip.x / centerClip.w)) * surface.width;
                    float ty =
                        (0.5f + 0.5f * signY * (centerClip.y / centerClip.w)) * surface.height;
                    float x0 = crossX;
                    float y0 = crossY;
                    if (std::isfinite(tx) && std::isfinite(ty) &&
                        clipSegment2D(x0, y0, tx, ty, -64.0f, -64.0f,
                                      surface.width + 64.0f,
                                      surface.height + 64.0f)) {
                        pl::modmenu::DrawCommand tracer{};
                        tracer.type = pl::modmenu::DrawCommandType::Line;
                        tracer.x = x0;
                        tracer.y = y0;
                        tracer.w = tx - x0;
                        tracer.h = ty - y0;
                        tracer.color = withAlpha(rgb, 135u);
                        tracer.size = std::max(1.2f, strokePx * 0.72f);
                        commands.push_back(std::move(tracer));
                    }
                }
            }
        }

        if (showLabels && commands.size() < kMaxDrawCommands) {
            const ClipVec4 labelClip = transformPoint(
                mvpMat,
                v.centerX - camPos.x,
                static_cast<float>(v.maxY + 1) + 0.15f - camPos.y,
                v.centerZ - camPos.z);
            if (labelClip.w >= 0.25f) {
                const float lx =
                    (0.5f + 0.5f * (labelClip.x / labelClip.w)) * surface.width;
                const float ly =
                    (0.5f + 0.5f * signY * (labelClip.y / labelClip.w)) * surface.height;
                if (lx >= 16.0f && lx <= surface.width - 16.0f &&
                    ly >= 20.0f && ly <= surface.height - 12.0f) {
                    char textBuf[64]{};
                    const int meters = static_cast<int>(std::lround(cand.dist));
                    if (v.blockCount > 1u) {
                        std::snprintf(textBuf, sizeof(textBuf), "%s x%u [%dm]",
                                      oreDisplayName(v.kind),
                                      static_cast<unsigned>(v.blockCount),
                                      meters);
                    } else {
                        std::snprintf(textBuf, sizeof(textBuf), "%s [%dm]",
                                      oreDisplayName(v.kind), meters);
                    }

                    pl::modmenu::DrawCommand label{};
                    label.type = pl::modmenu::DrawCommandType::Text;
                    label.x = lx;
                    label.y = ly;
                    label.w = -2.0f;
                    label.h = 0.0f;
                    label.x3 = labelBgBits;
                    label.color = withAlpha(rgb, 245u);
                    label.size = labelFontPx;
                    label.text = textBuf;
                    commands.push_back(std::move(label));
                }
            }
        }
    }

    if (commands.empty()) {
        clearOverlay();
        return;
    }

    pl::modmenu::submitDrawCommands(kModuleId, commands);
    mOverlayDirty.store(true, std::memory_order_release);
}

void GameHook::onClientUpdate() {
    if (!mEnabled.load(std::memory_order_relaxed)) {
        clearOverlay();
        return;
    }

    if (!mOverlayDirty.load(std::memory_order_relaxed))
        return;

    const std::int64_t lastRender = mLastWorldRenderMs.load(std::memory_order_relaxed);
    if (lastRender > 0 && (steadyNowMs() - lastRender) > 300) {
        clearOverlay();
        storeSnapshot(nullptr);
    }
}

} // namespace oreesp
