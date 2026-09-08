#pragma once
#include "App.h"
template <class Async> struct WinrtOperation {
    Async operation;
    std::shared_ptr<winrt::handle> completed;
    explicit WinrtOperation(Async value)
        : operation(value), completed(std::make_shared<winrt::handle>(CreateEvent(nullptr, TRUE, FALSE, nullptr))) {
        if (!*completed)
            winrt::throw_last_error();
        // WinRT permits assigning Completed only once. Keep the event alive
        // in the callback if completion arrives after cancellation/timeout.
        operation.Completed([signal = completed](auto const &, auto) noexcept { SetEvent(signal->get()); });
    }
    bool Poll() {
        auto result = WaitForSingleObject(completed->get(), 50);
        if (result == WAIT_FAILED)
            winrt::throw_last_error();
        return result == WAIT_OBJECT_0;
    }
    auto Result() {
        return operation.GetResults();
    }
    void Cancel() {
        operation.Cancel();
    }
};
