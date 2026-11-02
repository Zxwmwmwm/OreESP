#pragma once

/**
 * @file Guard.hpp
 * @brief Thread-local fault-guarded memory reads for native Bedrock structures.
 *
 * IMPORTANT (Android 11+ ARM64 Scudo TBI):
 * Heap pointers on modern Android carry a non-zero top byte (e.g. 0xB400007...)
 * under ARM64 Top-Byte-Ignore (TBI). Masking off bits 56..63 before checking the
 * canonical 48-bit user VA range ensures Scudo heap pointers are accepted while
 * still rejecting null, small integers, and all-ones sentinel values.
 */

#include <csetjmp>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace oreesp::guard {

namespace detail {

inline thread_local sigjmp_buf tlJump;
inline thread_local volatile sig_atomic_t tlArmed = 0;

inline struct sigaction gOldSegv {};
inline struct sigaction gOldBus {};
inline bool gInstalled = false;
inline std::mutex gInstallMutex;

inline void handler(int sig, siginfo_t *info, void *ctx) {
    if (tlArmed) {
        tlArmed = 0;
        siglongjmp(tlJump, 1);
    }

    const struct sigaction *old = (sig == SIGSEGV) ? &gOldSegv : &gOldBus;
    if ((old->sa_flags & SA_SIGINFO) != 0 && old->sa_sigaction != nullptr) {
        old->sa_sigaction(sig, info, ctx);
        return;
    }
    if (old->sa_handler != nullptr && old->sa_handler != SIG_DFL &&
        old->sa_handler != SIG_IGN) {
        old->sa_handler(sig);
        return;
    }
    ::signal(sig, SIG_DFL);
    ::raise(sig);
}

inline void install() {
    std::lock_guard lock(gInstallMutex);
    if (gInstalled)
        return;
    struct sigaction sa {};
    sa.sa_sigaction = &handler;
    sa.sa_flags = SA_SIGINFO;
    ::sigemptyset(&sa.sa_mask);
    ::sigaction(SIGSEGV, &sa, &gOldSegv);
    ::sigaction(SIGBUS, &sa, &gOldBus);
    gInstalled = true;
}

} // namespace detail

template <typename Fn> bool run(Fn &&fn) {
    detail::install();

    if (sigsetjmp(detail::tlJump, 1) != 0) {
        detail::tlArmed = 0;
        return false;
    }
    detail::tlArmed = 1;
    fn();
    detail::tlArmed = 0;
    return true;
}

inline bool isPointer(const void *p) {
    const auto untagged =
        reinterpret_cast<std::uintptr_t>(p) & 0x0000FFFFFFFFFFFFull;
    const auto topByte = reinterpret_cast<std::uintptr_t>(p) >> 56u;
    if (topByte == 0xFFu)
        return false;
    return untagged >= 0x10000u;
}

inline void *readPtr(const void *base, std::size_t offset) {
    void *value = nullptr;
    std::memcpy(&value, static_cast<const char *>(base) + offset, sizeof(value));
    return isPointer(value) ? value : nullptr;
}

inline bool printable(const char *text, std::size_t size) {
    if (size == 0 || size > 96)
        return false;
    bool letter = false;
    for (std::size_t i = 0; i < size; ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c < 0x20 || c > 0x7E)
            return false;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
            letter = true;
    }
    return letter;
}

} // namespace oreesp::guard
