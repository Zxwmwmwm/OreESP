#pragma once

/**
 * @file GameHook.hpp
 * @brief Native Bedrock tick scanner + in-engine GPU 3D & HudOverlay ESP renderer.
 */

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "mod/Config.hpp"

namespace pl::memory {
class HookHandle;
}

namespace oreesp {

enum class OreKind : std::uint8_t {
    None = 0,
    AncientDebris = 1,
    Diamond = 2,
    Emerald = 3,
    Gold = 4,
    Lapis = 5,
    Redstone = 6,
    Iron = 7,
    Quartz = 8,
    Copper = 9,
    Coal = 10,
    Count = 11,
};

struct Vec3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct BlockPos {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

struct GridEdge {
    std::int32_t x0 = 0;
    std::int32_t y0 = 0;
    std::int32_t z0 = 0;
    std::int32_t x1 = 0;
    std::int32_t y1 = 0;
    std::int32_t z1 = 0;
};

struct OreVein {
    OreKind kind = OreKind::None;
    std::uint16_t blockCount = 0;
    std::int32_t minX = 0;
    std::int32_t minY = 0;
    std::int32_t minZ = 0;
    std::int32_t maxX = 0;
    std::int32_t maxY = 0;
    std::int32_t maxZ = 0;
    float centerX = 0.0f;
    float centerY = 0.0f;
    float centerZ = 0.0f;
    std::uint32_t edgeOffset = 0;
    std::uint32_t edgeCount = 0;
};

struct ScanSnapshot {
    std::vector<OreVein> veins;
    std::vector<GridEdge> edges;
    Vec3f scanOrigin{};
    int scanRadius = 24;
};

class GameHook {
public:
    static GameHook &get();

    bool start();
    void stop();

    void applyConfig(const ModConfig &cfg);

private:
    GameHook() = default;

    struct RawOreBlock {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
        OreKind kind = OreKind::None;
    };

    struct CacheSlot {
        const void *blockPtr = nullptr;
        OreKind kind = OreKind::None;
        bool occupied = false;
    };

    static void normalTickDetour(void *localPlayer);
    static void renderLevelDetour(void *levelRenderer, void *screenContext,
                                  void *frameRenderObj);
    static void *clientUpdateDetour(void *clientInstance, bool arg);

    void onNormalTick(void *localPlayer);
    void onRenderLevel(void *levelRenderer, void *screenContext);
    void onClientUpdate();

    void watchLoop();
    void installNow();
    void clearOverlay();
    void resetScanState(bool resetCache);

    OreKind classifyBlock(const void *block);
    void finalizeScanPass(const std::vector<RawOreBlock> &sourceBlocks,
                          const BlockPos &center, int radius);
    void pruneBrokenNearbyOres(void *blockSource, const Vec3f &feet);

    [[nodiscard]] std::shared_ptr<const ScanSnapshot> loadSnapshot() const;
    void storeSnapshot(std::shared_ptr<const ScanSnapshot> snap);

    // Live atomic settings for lock-free render-thread reads
    std::atomic<bool> mEnabled{true};
    std::atomic<int> mScanRadius{24};
    std::atomic<int> mScanIntervalTicks{10};
    std::atomic<int> mMaxVeins{80};
    std::atomic<float> mLineWidth{1.8f};
    std::atomic<bool> mMergeVeins{false};
    std::atomic<bool> mShowLabels{true};
    std::atomic<bool> mShowTracers{false};
    std::atomic<bool> mHudOutlines{false};
    std::atomic<std::uint32_t> mOreMask{0x01FEu};
    std::atomic<std::uint64_t> mRescanEpoch{1};

    // Lifecycle & hook handles
    std::atomic<bool> mInstalled{false};
    std::atomic<bool> mStopRequested{false};
    std::atomic<bool> mStarting{false};
    std::thread mWatcher;

    void *mGetBlockFn = nullptr;
    void *mTessBeginFn = nullptr;
    void *mTessColorFn = nullptr;
    void *mTessVertexFn = nullptr;
    void *mRenderMeshFn = nullptr;
    std::uintptr_t mRenderMaterialGroup = 0;

    void *mOrigNormalTick = nullptr;
    void *mOrigRenderLevel = nullptr;
    void *mOrigClientUpdate = nullptr;
    pl::memory::HookHandle *mTickHook = nullptr;
    pl::memory::HookHandle *mRenderHook = nullptr;
    pl::memory::HookHandle *mUpdateHook = nullptr;

    // Tick-thread scanner state
    static constexpr std::size_t kCacheSize = 4096;
    std::array<CacheSlot, kCacheSize> mBlockCache{};
    const void *mLastPlayer = nullptr;
    const void *mLastDimension = nullptr;
    const void *mLastBlockSource = nullptr;
    std::uint64_t mObservedEpoch = 0;
    int mCooldownTicks = 0;
    bool mScanActive = false;
    BlockPos mScanCenter{};
    int mActiveRadius = 24;
    int mActiveMinY = -64;
    int mActiveMaxY = 320;
    int mCursorDx = 0;
    int mCursorDz = 0;
    Vec3f mLastCompletedPos{};
    bool mHasCompletedScan = false;
    BlockPos mCompletedCenter{};
    int mCompletedRadius = 24;
    std::vector<RawOreBlock> mStagingBlocks;
    std::vector<RawOreBlock> mKnownBlocks;

    // Cross-thread snapshot + diagnostics + overlay liveness tracking
    mutable std::mutex mSnapshotMutex;
    std::shared_ptr<const ScanSnapshot> mSnapshot;
    std::atomic<std::int64_t> mLastWorldRenderMs{0};
    std::atomic<bool> mOverlayDirty{false};
    std::atomic<bool> mLoggedFirstScan{false};
    std::atomic<bool> mLoggedFirstRender{false};
};

} // namespace oreesp
