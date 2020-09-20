/**
 * @see https://github.com/microsoft/angle/wiki
 * @see https://github.com/microsoft/DirectXTK/wiki/Getting-Started
 */
//#define CATCH_CONFIG_WINDOWS_CRTDBG
#include <catch2/catch.hpp>
#include <spdlog/spdlog.h>
// clang-format off
#include <opengl_1.h>
#include <EGL/eglext_angle.h>

#include <comdef.h>
#include <wrl/client.h>
#pragma comment(lib, "comctl32")

#include <wincodecsdk.h>
#include <DirectXTK/WICTextureLoader.h>

#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")

#include <d3d11.h>
#include <d3d11_1.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")

//#include <d3dcompiler.h>
//#pragma comment(lib, "d3dcompiler.lib")

// clang-format on

using namespace std;
using namespace Microsoft::WRL;

auto get_current_stream() noexcept -> std::shared_ptr<spdlog::logger>;
fs::path get_asset_dir() noexcept;

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
        REQUIRE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, nullptr, 0, //
                                  D3D11_SDK_VERSION, device.GetAddressOf(), &level, context.GetAddressOf()) == S_OK);
        REQUIRE(device);
        REQUIRE(device->GetFeatureLevel() == level);
    }
    SECTION("D3D_FEATURE_LEVEL_10_1") {
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_10_1;
        REQUIRE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, nullptr, 0, //
                                  D3D11_SDK_VERSION, device.GetAddressOf(), &level, context.GetAddressOf()) == S_OK);
        REQUIRE(device);
        REQUIRE(device->GetFeatureLevel() == level);
    }
    SECTION("D3D_FEATURE_LEVEL_11_1") {
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_1;
        REQUIRE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, nullptr, 0, //
                                  D3D11_SDK_VERSION, device.GetAddressOf(), &level, context.GetAddressOf()) == S_OK);
        REQUIRE(device);
        REQUIRE(device->GetFeatureLevel() == level);
    }
}

TEST_CASE("eglGetPlatformDisplay(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)", "[egl][directx]") {
    EGLAttrib attrs[]{
        EGL_PLATFORM_ANGLE_TYPE_ANGLE,
        EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
        EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE,
        11,
        EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE,
        0,
        EGL_NONE,
    };
    EGLDisplay display = eglGetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, attrs);
    REQUIRE(display != EGL_NO_DISPLAY);
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

    ComPtr<ID3D11Device> handle{};
    ComPtr<ID3D11DeviceContext> context{};
    GIVEN("D3D_FEATURE_LEVEL_9_3") {
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_9_3;
        D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_9_3};
        if (auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 1, //
                                        D3D11_SDK_VERSION, handle.GetAddressOf(), &level, context.GetAddressOf());
            FAILED(hr))
            throw _com_error{hr};
        EGLDeviceEXT device = createDevice(EGL_D3D11_DEVICE_ANGLE, handle.Get(), nullptr);
        REQUIRE(device != EGL_NO_DEVICE_EXT);
        REQUIRE(eglGetError() == EGL_SUCCESS);
        auto on_return = gsl::finally([releaseDevice, device]() { //
            releaseDevice(device);
        });
        test_egl_display(device);
        WHEN("eglQueryDeviceAttribEXT(EGL_D3D9_DEVICE_ANGLE)") {
            // Created with Dx11 API. D3D9 query muet fail
            EGLAttrib attr{};
            REQUIRE_FALSE(eglQueryDeviceAttribEXT(device, EGL_D3D9_DEVICE_ANGLE, &attr));
            REQUIRE(eglGetError() == EGL_BAD_ATTRIBUTE);
        }
    }
    GIVEN("D3D_FEATURE_LEVEL_11_1") {
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_1;
        D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        if (auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 2, //
                                        D3D11_SDK_VERSION, handle.GetAddressOf(), &level, context.GetAddressOf());
            FAILED(hr))
            throw _com_error{hr};

        EGLDeviceEXT device = createDevice(EGL_D3D11_DEVICE_ANGLE, handle.Get(), nullptr);
        REQUIRE(device != EGL_NO_DEVICE_EXT);
        REQUIRE(eglGetError() == EGL_SUCCESS);
        auto on_return = gsl::finally([releaseDevice, device]() { //
            releaseDevice(device);
        });
        test_egl_display(device);
        WHEN("eglQueryDeviceAttribEXT(EGL_D3D11_DEVICE_ANGLE)") {
            test_egl_attribute_d3d11(device, level);
        }
    }
}

// see https://docs.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_usage#resource-usage-restrictions
// see https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createtexture2d#remarks
TEST_CASE("ID3D11Texture2D(D3D11_USAGE_DYNAMIC)", "[directx][texture]") {
    ComPtr<ID3D11Device> device{};
    ComPtr<ID3D11DeviceContext> context{};
    D3D_FEATURE_LEVEL level{};
    D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_1};
    if (auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 1, //
                                    D3D11_SDK_VERSION, device.GetAddressOf(), &level, context.GetAddressOf());
        FAILED(hr))
        throw _com_error{hr};

    ComPtr<ID3D11Texture2D> texture{};
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = desc.Height = 500;
    desc.MipLevels = desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    desc.Usage = D3D11_USAGE_DYNAMIC; // write by CPU, read by GPU
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    SECTION("DXGI_FORMAT_R8G8B8A8_UNORM") {
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        if (auto hr = device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf()))
            throw _com_error{hr};
    }
    SECTION("DXGI_FORMAT_NV12") {
        desc.Format = DXGI_FORMAT_NV12;
        if (auto hr = device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf()))
            throw _com_error{hr};
    }
    SECTION("DXGI_FORMAT_YUY2") {
        desc.Format = DXGI_FORMAT_YUY2;
        if (auto hr = device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf()))
            throw _com_error{hr};
    }
    REQUIRE(texture);

    // see https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map
    D3D11_MAPPED_SUBRESOURCE mapping{};
    if (auto hr = context->Map(texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapping))
        throw _com_error{hr};
    CAPTURE(mapping.DepthPitch);
    REQUIRE(mapping.pData);
    switch (desc.Format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        REQUIRE(mapping.RowPitch == 512 * 4); // aligned in 16 bytes
        break;
    case DXGI_FORMAT_NV12:
        REQUIRE(mapping.RowPitch == 512);
        break;
    case DXGI_FORMAT_YUY2:
        REQUIRE(mapping.RowPitch == 512 * 2);
        break;
    }
    context->Unmap(texture.Get(), 0);
}

SCENARIO("IWICImagingFactor", "[windows]") {
    ComPtr<IWICImagingFactory> factory{};
    REQUIRE(CoCreateInstance(CLSID_WICImagingFactory, NULL, //
                             CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf())) == S_OK);

    GIVEN("image/png") {
        const auto fpath = get_asset_dir() / "image_1080_608.png";
        REQUIRE(fs::exists(fpath));

        WHEN("IWICBitmapDecoder") {
            ComPtr<IWICBitmapDecoder> decoder = nullptr;
            REQUIRE(factory->CreateDecoderFromFilename(fpath.c_str(), nullptr, GENERIC_READ, //
                                                       WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf()) == S_OK);
            ComPtr<IWICBitmapFrameDecode> source = nullptr;
            REQUIRE(decoder->GetFrame(0, source.GetAddressOf()) == S_OK);
            WICRect rect{};
            REQUIRE(source->GetSize(reinterpret_cast<UINT*>(&rect.Width), reinterpret_cast<UINT*>(&rect.Height)) ==
                    S_OK);
            REQUIRE(rect.Width == 1080);
            REQUIRE(rect.Height == 608);
            // @todo source->CopyPixels(...);
        }
        WHEN("CreateWICTextureFromFile") {
            ComPtr<ID3D11Device> device{};
            ComPtr<ID3D11DeviceContext> context{};
            D3D_FEATURE_LEVEL level{};
            D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_1};
            if (auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 1, //
                                            D3D11_SDK_VERSION, device.GetAddressOf(), &level, context.GetAddressOf());
                FAILED(hr))
                throw _com_error{hr};
            ComPtr<ID3D11Resource> resource{};
            ComPtr<ID3D11Texture2D> texture{};
            ComPtr<ID3D11ShaderResourceView> texture_view{};
            REQUIRE(DirectX::CreateWICTextureFromFile(device.Get(), context.Get(), //
                                                      fpath.c_str(),               //
                                                      resource.GetAddressOf(), texture_view.GetAddressOf()) == S_OK);
            REQUIRE(resource);
            REQUIRE(texture_view);
            resource.As(&texture);
            REQUIRE(texture);

            D3D11_TEXTURE2D_DESC desc{};
            texture->GetDesc(&desc);
            REQUIRE(desc.Width == 1080);
            REQUIRE(desc.Height == 608);
        }
    }
}

// Create an EGLDisplay using the EGLDevice
// see https://github.com/google/angle/blob/master/src/tests/egl_tests/EGLPresentPathD3D11Test.cpp
EGLDisplay make_egl_display(EGLDeviceEXT device) {
    EGLDisplay display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, device, nullptr);
    if (display == EGL_NO_DISPLAY)
        FAIL(eglGetError());

    EGLint major = 0, minor = 0;
    if (eglInitialize(display, &major, &minor) == false)
        FAIL(eglGetError());
    REQUIRE(major == 1); // expect 1.4+
    REQUIRE(minor >= 4);

    REQUIRE(eglBindAPI(EGL_OPENGL_ES_API));

    const EGLint attrs[] = {EGL_NONE};
    EGLint count = 0;
    if (eglChooseConfig(display, attrs, nullptr, 0, &count) == false)
        FAIL(eglGetError());
    REQUIRE(count > 0);
    return display;
}

/// @see https://docs.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-layers
void make_device_and_context(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext>& context,
                             D3D_FEATURE_LEVEL& level) {
    ComPtr<IDXGIFactory> factory = nullptr;
    if (auto hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)factory.GetAddressOf()))
        throw _com_error{hr};

    IDXGIAdapter* adapter = nullptr;
    for (auto i = 0u; factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC info{};
        if (auto hr = adapter->GetDesc(&info); FAILED(hr))
            throw _com_error{hr};
        wstring_view name{info.Description};
        if (name.find(L"NVIDIA") != std::string::npos)
            break;
    }
    if (auto hr = D3D11CreateDevice(adapter, adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,          //
                                    NULL, D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, //
                                    D3D11_SDK_VERSION, device.GetAddressOf(), &level, context.GetAddressOf()))
        throw _com_error{hr};
}

/// @see https://github.com/google/angle/blob/master/src/tests/egl_tests/EGLPresentPathD3D11Test.cpp
SCENARIO("ID3D11Texture2D to EGLImage", "[egl]") {
    auto stream = get_current_stream();

    ComPtr<ID3D11Device> device{};
    ComPtr<ID3D11DeviceContext> context{};
    D3D_FEATURE_LEVEL level{};
    make_device_and_context(device, context, level);
    REQUIRE(device);
    REQUIRE(level >= D3D_FEATURE_LEVEL_9_3);

    ComPtr<ID3D11Texture2D> texture{};
    {
        D3D11_TEXTURE2D_DESC info{};
        info.Width = 960;
        info.Height = 720;
        info.MipLevels = info.ArraySize = 1;
        info.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        info.SampleDesc.Count = 1;
        info.SampleDesc.Quality = 0;
        info.Usage = D3D11_USAGE_DEFAULT;
        info.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        info.CPUAccessFlags = 0;
        info.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
        REQUIRE(device->CreateTexture2D(&info, nullptr, texture.GetAddressOf()) == S_OK);
    }
    REQUIRE(texture);

    auto create_device = reinterpret_cast<PFNEGLCREATEDEVICEANGLEPROC>(eglGetProcAddress("eglCreateDeviceANGLE"));
    auto release_device = reinterpret_cast<PFNEGLRELEASEDEVICEANGLEPROC>(eglGetProcAddress("eglReleaseDeviceANGLE"));
    REQUIRE(create_device);
    REQUIRE(release_device);

    GIVEN("EGLDisplay/EGLDevice") {
        EGLDeviceEXT es_device = create_device(EGL_D3D11_DEVICE_ANGLE, device.Get(), nullptr);
        REQUIRE(eglGetError() == EGL_SUCCESS);
        auto on_return_1 = gsl::finally([release_device, es_device]() { release_device(es_device); });

        EGLDisplay es_display = make_egl_display(es_device);
        auto on_return_2 = gsl::finally([es_display]() { eglTerminate(es_display); });

        EGLint count = 0;
        EGLConfig es_configs[10]{};
        {
            EGLint attrs[]{EGL_RED_SIZE,
                           8,
                           EGL_GREEN_SIZE,
                           8,
                           EGL_BLUE_SIZE,
                           8,
                           EGL_ALPHA_SIZE,
                           8,
                           EGL_RENDERABLE_TYPE,
                           EGL_OPENGL_ES3_BIT, // EGL_OPENGL_ES2_BIT
                           EGL_SURFACE_TYPE,
                           EGL_PBUFFER_BIT | EGL_WINDOW_BIT,
                           EGL_NONE};
            eglChooseConfig(es_display, attrs, es_configs, 10, &count);
            REQUIRE(eglGetError() == EGL_SUCCESS);
            REQUIRE(count);
        }

        const EGLint attrs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE};
        const EGLContext es_context = eglCreateContext(es_display, es_configs[0], EGL_NO_CONTEXT, attrs);
        REQUIRE(eglGetError() == EGL_SUCCESS);
        eglMakeCurrent(es_display, EGL_NO_SURFACE, EGL_NO_SURFACE, es_context);
        REQUIRE(eglGetError() == EGL_SUCCESS);
        auto on_return_3 = gsl::finally([es_display]() {
            eglMakeCurrent(es_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            REQUIRE(eglGetError() == EGL_SUCCESS);
        });
        stream->debug("{:s}", glGetString(GL_VERSION));

        WHEN("create EGLSurface from shared resource") {
            ComPtr<IDXGIResource> resource{};
            REQUIRE(texture.As(&resource) == S_OK);
            HANDLE handle{};
            REQUIRE(resource->GetSharedHandle(&handle) == S_OK);
            D3D11_TEXTURE2D_DESC desc{};
            texture->GetDesc(&desc);

            EGLint attrs[]{EGL_WIDTH,
                           desc.Width,
                           EGL_HEIGHT,
                           desc.Height, //
                           EGL_TEXTURE_TARGET,
                           EGL_TEXTURE_2D,
                           EGL_TEXTURE_FORMAT,
                           EGL_TEXTURE_RGBA, //
                           EGL_NONE};
            EGLSurface es_surface = eglCreatePbufferFromClientBuffer(es_display, EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE,
                                                                     handle, es_configs[0], attrs);
            if (es_surface == EGL_NO_SURFACE)
                FAIL(eglGetError());
            auto on_return_4 = gsl::finally([es_display, es_surface]() {
                eglDestroySurface(es_display, es_surface);
                REQUIRE(eglGetError() == EGL_SUCCESS);
            });

            THEN("eglMakeCurrent") {
                switch (auto status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)) {
                case GL_FRAMEBUFFER_UNDEFINED: // default framebuffer doesn't exist
                    break;
                default:
                    FAIL(status);
                }
                eglMakeCurrent(es_display, es_surface, es_surface, es_context);
                REQUIRE(eglGetError() == EGL_SUCCESS);
                REQUIRE(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
            }

            THEN("bind to GL_TEXTURE_2D") {
                const GLenum target = GL_TEXTURE_2D;
                GLuint tex{};
                glGenTextures(1, &tex);
                glBindTexture(target, tex);
                auto on_return_5 = gsl::finally([tex]() {
                    glDeleteTextures(1, &tex);
                    REQUIRE(glGetError() == GL_NO_ERROR);
                });

                // see https://www.khronos.org/registry/EGL/sdk/docs/man/html/eglBindTexImage.xhtml
                // After eglBindTexImage is called, the specified surface is no longer available for reading or writing.
                eglBindTexImage(es_display, es_surface, EGL_BACK_BUFFER);
                REQUIRE(eglGetError() == EGL_SUCCESS);

                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                REQUIRE(glGetError() == GL_NO_ERROR);
            }

            THEN("bind to GL_FRAMEBUFFER") {
                GLuint tex{};
                glGenTextures(1, &tex);
                glBindTexture(GL_TEXTURE_2D, tex);
                auto on_return_5 = gsl::finally([tex]() {
                    glDeleteTextures(1, &tex);
                    REQUIRE(glGetError() == GL_NO_ERROR);
                });
                eglBindTexImage(es_display, es_surface, EGL_BACK_BUFFER);
                REQUIRE(eglGetError() == EGL_SUCCESS);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                REQUIRE(glGetError() == GL_NO_ERROR);

                GLuint fbo{};
                glGenFramebuffers(1, &fbo);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
                auto on_return_6 = gsl::finally([fbo]() {
                    glDeleteFramebuffers(1, &fbo);
                    REQUIRE(glGetError() == GL_NO_ERROR);
                });
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

                switch (auto status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)) {
                case GL_FRAMEBUFFER_COMPLETE: // color buffer is ready
                    glClear(GL_COLOR_BUFFER_BIT);
                    glFlush();
                    REQUIRE(glGetError() == GL_NO_ERROR);
                    break;
                default:
                    CAPTURE(glGetError());
                    FAIL(status);
                }
                REQUIRE(glGetError() == GL_NO_ERROR);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            }
        }
    }
}
