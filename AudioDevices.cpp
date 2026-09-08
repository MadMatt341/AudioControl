#include "App.h"
#include "PolicyConfig.h"
void Check(HRESULT hr) {
    if (FAILED(hr))
        throw hr;
}
ComPtr<IMMDeviceEnumerator> Enumerator() {
    ComPtr<IMMDeviceEnumerator> e;
    Check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&e)));
    return e;
}
std::wstring Id(IMMDevice *d) {
    LPWSTR raw = nullptr;
    Check(d->GetId(&raw));
    TaskString owned(raw);
    return std::wstring(owned.get());
}
std::wstring Current(IMMDeviceEnumerator *e, ERole role) {
    ComPtr<IMMDevice> d;
    HRESULT hr = e->GetDefaultAudioEndpoint(eRender, role, &d);
    if (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
        return {};
    Check(hr);
    return Id(d.Get());
}
std::vector<Device> Devices(IMMDeviceEnumerator *e, unsigned *skipped) {
    ComPtr<IMMDeviceCollection> list;
    Check(e->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &list));
    UINT count;
    Check(list->GetCount(&count));
    unsigned unavailable = 0;
    auto devices = audio::CollectAvailable<Device, HRESULT>(
        count,
        [&](unsigned i) {
            ComPtr<IMMDevice> d;
            Check(list->Item(i, &d));
            auto id = Id(d.Get());
            ComPtr<IPropertyStore> props;
            VariantOwner property;
            HRESULT hr = d->OpenPropertyStore(STGM_READ, &props);
            if (SUCCEEDED(hr))
                hr = props->GetValue(PKEY_Device_FriendlyName, &property.value);
            const auto &v = property.value;
            std::wstring name =
                SUCCEEDED(hr) && v.vt == VT_LPWSTR && v.pwszVal ? v.pwszVal : L"Audio output (name unavailable)";
            return Device{id, name};
        },
        unavailable);
    if (skipped)
        *skipped = unavailable;
    std::sort(devices.begin(), devices.end(),
              [](const Device &a, const Device &b) { return a.name == b.name ? a.id < b.id : a.name < b.name; });
    return devices;
}
bool Included(const Device &d) {
    return GetPrivateProfileInt(L"Devices", d.id.c_str(), 1, config.c_str()) != 0;
}
size_t Next(const std::vector<Device> &ds, const std::wstring &current) {
    for (size_t i = 0; i < ds.size(); ++i)
        if (ds[i].id == current)
            return (i + 1) % ds.size();
    return 0;
}
void Cycle() {
    try {
        auto e = Enumerator();
        unsigned skipped = 0;
        auto ds = Devices(e.Get(), &skipped);
        ds.erase(std::remove_if(ds.begin(), ds.end(), [](const Device &d) { return !Included(d); }), ds.end());
        if (ds.empty()) {
            Show(skipped ? L"Connected outputs became unavailable; try again" : L"No enabled outputs connected");
            return;
        }
        auto current = Current(e.Get(), eMultimedia);
        const auto &target = ds[Next(ds, current)];
        ComPtr<Policy> policy;
        Check(CoCreateInstance(PolicyClass, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&policy)));
        struct Backend {
            IMMDeviceEnumerator *enumerator;
            Policy *policy;
            std::wstring Read(int role) {
                return Current(enumerator, role == 0 ? eConsole : eMultimedia);
            }
            void Write(int role, const std::wstring &id) {
                Check(policy->SetDefaultEndpoint(id.c_str(), role == 0 ? eConsole : eMultimedia));
            }
        } backend{e.Get(), policy.Get()};
        auto result = audio::SwitchOutput(backend, target.id);
        if (result == audio::SwitchResult::Restored) {
            Show(L"Switch failed; previous outputs restored");
            return;
        }
        if (result == audio::SwitchResult::Partial) {
            Show(L"Switch incomplete; could not restore both outputs. Check Windows sound settings.");
            return;
        }
        ShowDevice(target.name);
    } catch (HRESULT hr) {
        wchar_t msg[100];
        swprintf_s(msg, L"Could not switch output (0x%08X)", static_cast<unsigned>(hr));
        Show(msg);
    }
}
