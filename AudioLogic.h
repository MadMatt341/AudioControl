#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace audio {
template <class T, class Error, class Read>
std::vector<T> CollectAvailable(unsigned count, Read read, unsigned &skipped) {
    std::vector<T> result;
    for (unsigned i = 0; i < count; ++i) {
        try {
            result.push_back(read(i));
        } catch (Error) {
            ++skipped;
        }
    }
    return result;
}
enum class SwitchResult { Success, Restored, Partial };
// Backend exposes Read(role) and Write(role, id); failures may throw.
// Snapshot both roles before mutating either. Never touch communications.
template <class Backend> SwitchResult SwitchOutput(Backend &backend, const std::wstring &target) {
    std::array<std::wstring, 2> previous{backend.Read(0), backend.Read(1)};
    try {
        for (int role = 0; role < 2; ++role)
            if (previous[role] != target)
                backend.Write(role, target);
        bool confirmed = true;
        for (int role = 0; role < 2; ++role)
            if (backend.Read(role) != target)
                confirmed = false;
        if (confirmed)
            return SwitchResult::Success;
    } catch (...) {
    }
    // A failed write may still have applied. Re-read both roles and attempt
    // each restoration independently, including after verification failure.
    for (int role = 0; role < 2; ++role) {
        try {
            if (backend.Read(role) == previous[role])
                continue;
        } catch (...) {
        }
        if (!previous[role].empty()) {
            try {
                backend.Write(role, previous[role]);
            } catch (...) {
            }
        }
    }
    bool restored = true;
    for (int role = 0; role < 2; ++role) {
        try {
            if (backend.Read(role) != previous[role])
                restored = false;
        } catch (...) {
            restored = false;
        }
    }
    return restored ? SwitchResult::Restored : SwitchResult::Partial;
}

enum class MediaResult : unsigned { Fallback, Paused, Playing, Rejected, TimedOut, Cancelled };
// Shared by real WinRT operations and deterministic fake operations in tests.
// Poll returns true only when complete; deadline uses a monotonic clock.
template <class Operation, class Clock, class Stopped>
auto AwaitMedia(Operation &operation, Clock now, Stopped stopped, std::uint64_t deadline) {
    while (true) {
        if (stopped() || now() >= deadline) {
            const auto reason = stopped() ? MediaResult::Cancelled : MediaResult::TimedOut;
            try {
                operation.Cancel();
            } catch (...) {
            }
            throw reason;
        }
        if (operation.Poll())
            return operation.Result();
    }
}
template <class Backend> MediaResult ControlMedia(Backend &backend) {
    auto failure = MediaResult::Fallback;
    try {
        const int state = backend.Discover(); // 1 playing, 2 paused/stopped, 0 unknown
        if (state == 0)
            return failure;
        failure = MediaResult::Rejected; // never fall back after dispatching a command
        if (!backend.Command(state == 1))
            return failure;
        return state == 1 ? MediaResult::Paused : MediaResult::Playing;
    } catch (MediaResult reason) {
        return reason;
    } catch (...) {
        return failure;
    }
}
struct MediaGate {
    std::uintptr_t generation = 0;
    bool busy = false;
    std::uintptr_t Begin() {
        if (busy)
            return 0;
        busy = true;
        return ++generation;
    }
    bool Complete(std::uintptr_t id) {
        if (!busy || id != generation)
            return false;
        busy = false;
        return true;
    }
    void Stop() {
        busy = false;
        ++generation;
    }
};
} // namespace audio
