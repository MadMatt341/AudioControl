#include "App.h"
#include "WinrtOperation.h"
audio::MediaGate mediaGate;
KernelHandle mediaThread;
std::atomic<bool> mediaStop = false;
struct MediaContext {
    HWND target;
    std::uintptr_t generation;
} mediaContext;
struct MediaBackend {
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession session{nullptr};
    std::uint64_t deadline = GetTickCount64() + 3000;
    template <class Async> auto Wait(Async operation) {
        WinrtOperation<Async> pending{operation};
        return audio::AwaitMedia(pending, [] { return GetTickCount64(); }, [] { return mediaStop.load(); }, deadline);
    }
    int Discover() {
        using namespace winrt::Windows::Media::Control;
        auto manager = Wait(GlobalSystemMediaTransportControlsSessionManager::RequestAsync());
        session = manager.GetCurrentSession();
        if (!session)
            return 0;
        auto status = session.GetPlaybackInfo().PlaybackStatus();
        using Status = GlobalSystemMediaTransportControlsSessionPlaybackStatus;
        return status == Status::Playing ? 1 : (status == Status::Paused || status == Status::Stopped ? 2 : 0);
    }
    bool Command(bool pause) {
        if (mediaStop.load())
            throw audio::MediaResult::Cancelled;
        if (GetTickCount64() >= deadline)
            throw audio::MediaResult::TimedOut;
        return pause ? Wait(session.TryPauseAsync()) : Wait(session.TryPlayAsync());
    }
};
// Request state only when the shortcut is pressed; no resident polling thread.
DWORD WINAPI MediaWorker(void *context) {
    const auto request = *static_cast<MediaContext *>(context);
    auto result = audio::MediaResult::Fallback;
    bool initialized = false;
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        initialized = true;
        MediaBackend backend;
        result = audio::ControlMedia(backend);
    } catch (...) { /* Report failure or use fallback when discovery failed. */
    }
    if (initialized)
        winrt::uninit_apartment();
    if (!mediaStop.load())
        PostMessage(request.target, MediaResultMessage, static_cast<WPARAM>(result),
                    static_cast<LPARAM>(request.generation));
    return 0;
}

bool BeginMedia(HWND hwnd) {
    auto generation = mediaGate.Begin();
    if (!generation)
        return 0;
    if (mediaThread) {
        WaitForSingleObject(mediaThread, INFINITE);
        mediaThread.reset();
    }
    mediaContext = {hwnd, generation};
    mediaThread = CreateThread(nullptr, 0, MediaWorker, &mediaContext, 0, nullptr);
    if (!mediaThread) {
        mediaGate.Complete(generation);
        Show(L"Media control unavailable", L"MEDIA CONTROL");
    }
    return true;
}
bool CompleteMedia(LPARAM generation) {
    return mediaGate.Complete(static_cast<std::uintptr_t>(generation));
}
void StopMedia() {
    mediaGate.Stop();
    mediaStop = true;
    if (mediaThread) {
        // Async waits observe stop within 50 ms. Bound shutdown if an external
        // COM server blocks inside a synchronous call or cancellation itself.
        WaitForSingleObject(mediaThread, 1000);
        mediaThread.reset();
    }
}
