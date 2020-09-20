#include <catch2/catch.hpp>
#include <spdlog/spdlog.h>

// clang-format off
#include <opengl_1.h>
#include <EGL/eglext_angle.h>

#include <comdef.h>
#include <wrl/client.h>
#pragma comment(lib, "comctl32")

#include <d3d9.h>
#include <d3d11.h>
#include <d3d11_1.h>
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")

//#include <d3dcompiler.h>
//#pragma comment(lib, "d3dcompiler.lib")

// clang-format on

using namespace std;
using namespace Microsoft::WRL;

auto get_current_stream() noexcept -> std::shared_ptr<spdlog::logger>;

TEST_CASE("D3D9 Device", "[directx]") {
    ComPtr<IDirect3D9Ex> dxd9{};
    REQUIRE(Direct3DCreate9Ex(D3D_SDK_VERSION, dxd9.GetAddressOf()) == S_OK);
    D3DPRESENT_PARAMETERS params{};
    params.BackBufferWidth = 1920;
    params.BackBufferHeight = 1080;
    params.BackBufferFormat = D3DFMT_A8R8G8B8;
    params.BackBufferCount = 1;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.Windowed = true;
    params.hDeviceWindow = NULL; // HWND
    params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    ComPtr<IDirect3DDevice9> device{};
    REQUIRE(dxd9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &params,
                               device.GetAddressOf()) == S_OK);
    REQUIRE(device);
}

TEST_CASE("ID3D11Device with feature level", "[directx]") {
    ComPtr<ID3D11Device> device{};
    ComPtr<ID3D11DeviceContext> context{};
    SECTION("D3D_FEATURE_LEVEL_9_3") {
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_9_3;
        REQUIRE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0, nullptr, 0, //
                                  D3D11_SDK_VERSION, device.GetAddressOf(), &level, context.GetAddressOf()) == S_OK);
        REQUIRE(device);
        REQUIRE(device->GetFeatureLevel() == level);
    }
    SECTION("D3D_FEATURE_LEVEL_10_1") {
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_10_1;
        REQUIRE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0, nullptr, 0, //
                                  D3D11_SDK_VERSION, device.GetAddressOf(), &level, context.GetAddressOf()) == S_OK);
        REQUIRE(device);
        REQUIRE(device->GetFeatureLevel() == level);
    }
    SECTION("D3D_FEATURE_LEVEL_11_1") {
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_1;
        REQUIRE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0, nullptr, 0, //
                                  D3D11_SDK_VERSION, device.GetAddressOf(), &level, context.GetAddressOf()) == S_OK);
        REQUIRE(device);
        REQUIRE(device->GetFeatureLevel() == level);
    }
}

string w2mb(wstring_view in) noexcept(false) {
    string out{};
    out.reserve(MB_CUR_MAX * in.length());
    mbstate_t state{};
    for (wchar_t wc : in) {
        char mb[8]{}; // ensure null-terminated for UTF-8 (maximum 4 byte)
        const auto len = wcrtomb(mb, wc, &state);
        out += string_view{mb, len};
    }
    return out;
}

HRESULT check_dxgi_adapter(IDXGIAdapter* adapter) noexcept(false) {
    DXGI_ADAPTER_DESC info{};
    if (auto hr = adapter->GetDesc(&info); FAILED(hr))
        return hr;
    auto stream = get_current_stream();
    stream->info("{}:", w2mb(info.Description));
    stream->info(" - device_id: {:x}", info.DeviceId);
    stream->info(" - vendor_id: {:x}", info.VendorId);
    return S_OK;
}

struct physical_device_dx11_t final {
    ComPtr<ID3D11Device> handle;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level;

  public:
    explicit physical_device_dx11_t(ComPtr<IDXGIFactory> _factory, D3D_FEATURE_LEVEL _level) noexcept(false)
        : handle{}, context{}, level{} {
        IDXGIAdapter* adapter = nullptr;
        if (_factory) {
            for (auto i = 0u; _factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
                DXGI_ADAPTER_DESC info{};
                if (auto hr = adapter->GetDesc(&info); FAILED(hr))
                    throw _com_error{hr};
                //check_dxgi_adapter(adapter);
            }
        }
        adapter = nullptr; // will select the default device
        if (auto hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_HARDWARE, 0, 0, &_level, 1, //
                                        D3D11_SDK_VERSION, handle.GetAddressOf(), &level, context.GetAddressOf());
            FAILED(hr))
            throw _com_error{hr};
    }
};

// Create an EGLDisplay using the EGLDevice
void test_egl_display(EGLDeviceEXT device) {
    EGLDisplay display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, device, nullptr);
    if (display == EGL_NO_DISPLAY)
        FAIL(eglGetError());

    EGLint major = 0, minor = 0;
    if (eglInitialize(display, &major, &minor) == false)
        FAIL(eglGetError());
    auto on_return = gsl::finally([display]() { REQUIRE(eglTerminate(display)); });
    REQUIRE(major == 1); // expect 1.4+
    REQUIRE(minor >= 4);

    REQUIRE(eglBindAPI(EGL_OPENGL_ES_API));

    const EGLint attrs[] = {EGL_NONE};
    EGLint count = 0;
    if (eglChooseConfig(display, attrs, nullptr, 0, &count) == false)
        FAIL(eglGetError());
    REQUIRE(count > 0);
}

/// @see https://github.com/google/angle/blob/master/src/tests/egl_tests/EGLDeviceTest.cpp
void test_egl_attribute_d3d11(EGLDeviceEXT device, D3D_FEATURE_LEVEL level) {
    EGLAttrib attr{};
    REQUIRE(eglQueryDeviceAttribEXT(device, EGL_D3D11_DEVICE_ANGLE, &attr));
    REQUIRE(eglGetError() == EGL_SUCCESS);
    ID3D11Device* handle = reinterpret_cast<ID3D11Device*>(attr);
    REQUIRE(handle);
    REQUIRE(level == handle->GetFeatureLevel());
}

SCENARIO("Dx11 Device for ANGLE", "[egl]") {
    PFNEGLCREATEDEVICEANGLEPROC createDevice =
        reinterpret_cast<PFNEGLCREATEDEVICEANGLEPROC>(eglGetProcAddress("eglCreateDeviceANGLE"));
    REQUIRE(createDevice);
    PFNEGLRELEASEDEVICEANGLEPROC releaseDevice =
        reinterpret_cast<PFNEGLRELEASEDEVICEANGLEPROC>(eglGetProcAddress("eglReleaseDeviceANGLE"));
    REQUIRE(releaseDevice);

    ComPtr<IDXGIFactory> factory = nullptr;
    if (auto hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)factory.GetAddressOf()); FAILED(hr))
        throw _com_error{hr};
    REQUIRE(factory);

    GIVEN("D3D_FEATURE_LEVEL_9_3") {
        physical_device_dx11_t physical_device{factory, D3D_FEATURE_LEVEL_9_3};
        EGLDeviceEXT device = createDevice(EGL_D3D11_DEVICE_ANGLE, physical_device.handle.Get(), nullptr);
        REQUIRE(device != EGL_NO_DEVICE_EXT);
        REQUIRE(eglGetError() == EGL_SUCCESS);
        auto on_return = gsl::finally([releaseDevice, device]() { //
            releaseDevice(device);
        });
        WHEN("eglQueryDeviceAttribEXT(EGL_D3D9_DEVICE_ANGLE)") {
            // Created with Dx11 API. D3D9 query muet fail
            EGLAttrib attr{};
            REQUIRE_FALSE(eglQueryDeviceAttribEXT(device, EGL_D3D9_DEVICE_ANGLE, &attr));
            REQUIRE(eglGetError() == EGL_BAD_ATTRIBUTE);
        }
        WHEN("EGLDisplay") {
            test_egl_display(device);
        }
    }
    GIVEN("D3D_FEATURE_LEVEL_11_1") {
        physical_device_dx11_t physical_device{factory, D3D_FEATURE_LEVEL_11_1};
        EGLDeviceEXT device = createDevice(EGL_D3D11_DEVICE_ANGLE, physical_device.handle.Get(), nullptr);
        REQUIRE(device != EGL_NO_DEVICE_EXT);
        REQUIRE(eglGetError() == EGL_SUCCESS);
        auto on_return = gsl::finally([releaseDevice, device]() { //
            releaseDevice(device);
        });
        WHEN("eglQueryDeviceAttribEXT(EGL_D3D11_DEVICE_ANGLE)") {
            test_egl_attribute_d3d11(device, physical_device.level);
        }
        WHEN("EGLDisplay") {
            test_egl_display(device);
        }
    }
}
