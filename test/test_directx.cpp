/**
 * @author Park DongHa (luncliff@gmail.com)
 * @see https://github.com/microsoft/angle/wiki
 * @see https://github.com/microsoft/DirectXTK/wiki/Getting-Started
 */
//#define CATCH_CONFIG_WINDOWS_CRTDBG
#include <catch2/catch.hpp>
#include <spdlog/spdlog.h>

// clang-format off
#include <graphics.h>
#include <EGL/eglext_angle.h>
// clang-format on

#include <pplawait.h>
#include <ppltasks.h>
#include <winrt/Windows.Foundation.h> // namespace winrt::Windows::Foundation
#include <winrt/Windows.System.h>     // namespace winrt::Windows::System

// clang-format off
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <DirectXTK/DirectXHelpers.h>
#include <DirectXTK/WICTextureLoader.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#include <wincodec.h>
#include <wincodecsdk.h>
// clang-format on

namespace fs = std::filesystem;
using winrt::com_ptr;

#define LOCAL_EGL_PROC(fn, name)                                                                                       \
    auto fn = reinterpret_cast<decltype(&name)>(eglGetProcAddress(#name));                                             \
    REQUIRE(fn);

class ID3D11DeviceTestCase1 {
  protected:
    com_ptr<ID3D11Device> device{};
    com_ptr<ID3D11DeviceContext> device_context{};
};

TEST_CASE_METHOD(ID3D11DeviceTestCase1, "D3D_FEATURE_LEVEL_11_0(D3D_DRIVER_TYPE_REFERENCE)", "[directx][!mayfail]") {
    D3D_FEATURE_LEVEL levels[1]{D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL level{};
    REQUIRE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_REFERENCE, NULL,
                              D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 1, //
                              D3D11_SDK_VERSION, device.put(), &level, device_context.put()) == S_OK);
    REQUIRE(device);
    REQUIRE(device->GetFeatureLevel() == level);
}

TEST_CASE_METHOD(ID3D11DeviceTestCase1, "D3D_FEATURE_LEVEL_11_0(D3D_DRIVER_TYPE_NULL)", "[directx][!mayfail]") {
    D3D_FEATURE_LEVEL levels[1]{D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL level{};
    REQUIRE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_NULL, NULL,
                              D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 1, //
                              D3D11_SDK_VERSION, device.put(), &level, device_context.put()) == S_OK);
    REQUIRE(device);
    REQUIRE(device->GetFeatureLevel() == level);
}

TEST_CASE_METHOD(ID3D11DeviceTestCase1, "D3D_FEATURE_LEVEL_10_1(D3D_DRIVER_TYPE_HARDWARE)", "[directx][!mayfail]") {
    D3D_FEATURE_LEVEL levels[1]{D3D_FEATURE_LEVEL_10_1};
    D3D_FEATURE_LEVEL level{};
    REQUIRE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL,
                              D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 1, //
                              D3D11_SDK_VERSION, device.put(), &level, device_context.put()) == S_OK);
    REQUIRE(device);
    REQUIRE(device->GetFeatureLevel() == level);
}

TEST_CASE_METHOD(ID3D11DeviceTestCase1, "D3D_FEATURE_LEVEL_11_1(D3D_DRIVER_TYPE_HARDWARE)", "[directx][!mayfail]") {
    D3D_FEATURE_LEVEL levels[2]{D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL level{};
    REQUIRE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL,
                              D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 2, //
                              D3D11_SDK_VERSION, device.put(), &level, device_context.put()) == S_OK);
    REQUIRE(device);
    REQUIRE(device->GetFeatureLevel() == level);
}

TEST_CASE("eglGetPlatformDisplay(EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)", "[egl][directx][!mayfail]") {
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

TEST_CASE_METHOD(ID3D11DeviceTestCase1, "Device(11.0) for ANGLE", "[egl][directx][!mayfail]") {
    D3D_FEATURE_LEVEL level{};
    REQUIRE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL,
                              D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, //
                              D3D11_SDK_VERSION, device.put(), &level, device_context.put()) == S_OK);
    REQUIRE(device);

    /// @see https://github.com/google/angle/blob/master/extensions/EGL_ANGLE_device_creation.txt
    /// @see https://github.com/google/angle/blob/master/extensions/EGL_ANGLE_device_creation_d3d11.txt
    LOCAL_EGL_PROC(create_device, eglCreateDeviceANGLE);
    LOCAL_EGL_PROC(release_device, eglReleaseDeviceANGLE);
    auto es_device_owner = std::unique_ptr<std::remove_pointer_t<EGLDeviceEXT>, PFNEGLRELEASEDEVICEANGLEPROC>{
        create_device(EGL_D3D11_DEVICE_ANGLE, device.get(), nullptr), release_device};
    EGLDeviceEXT es_device = es_device_owner.get();
    REQUIRE(es_device != EGL_NO_DEVICE_EXT);
    REQUIRE(eglGetError() == EGL_SUCCESS);

    /// @see https://github.com/google/angle/blob/master/extensions/EGL_EXT_device_query.txt
    /// @see https://github.com/google/angle/blob/master/src/tests/egl_tests/EGLDeviceTest.cpp
    SECTION("eglQueryDeviceAttribEXT(EGL_D3D9_DEVICE_ANGLE)") {
        // Created with Dx11 API. D3D9 query muet fail
        EGLAttrib attr{};
        REQUIRE_FALSE(eglQueryDeviceAttribEXT(es_device, EGL_D3D9_DEVICE_ANGLE, &attr));
        REQUIRE(eglGetError() == EGL_BAD_ATTRIBUTE);
    }
    SECTION("eglQueryDeviceAttribEXT(EGL_D3D11_DEVICE_ANGLE)") {
        EGLAttrib attr{};
        REQUIRE(eglQueryDeviceAttribEXT(es_device, EGL_D3D11_DEVICE_ANGLE, &attr));
        REQUIRE(eglGetError() == EGL_SUCCESS);
        ID3D11Device* handle = reinterpret_cast<ID3D11Device*>(attr);
        REQUIRE(handle);
        REQUIRE(level == handle->GetFeatureLevel());
    }

// #if !defined(QT_OPENGL_LIB)
    // Qt 5.12+ makes a crash in DllMain resouce cleanup after this TC
    // Qt 5.9 seems like OK after the `eglReleaseDeviceANGLE`...
    SECTION("eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT)") {
        EGLDisplay es_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, es_device, nullptr);
        if (es_display == EGL_NO_DISPLAY)
            FAIL(eglGetError());
        REQUIRE(eglTerminate(es_display));
    }
// #endif

    /// @see https://github.com/google/angle/blob/master/extensions/EGL_ANGLE_platform_angle.txt
    SECTION("eglGetPlatformDisplayEXT(EGL_DEFAULT_DISPLAY)") {
        EGLDisplay es_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, nullptr);
        if (es_display == EGL_NO_DISPLAY)
            FAIL(eglGetError());
        EGLint version[2];
        REQUIRE(eglInitialize(es_display, version + 0, version + 1));
        //REQUIRE(eglTerminate(es_display));
    }
    SECTION("eglGetPlatformDisplayEXT(EGL_D3D11_ONLY_DISPLAY_ANGLE)") {
        EGLDisplay es_display =
            eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_D3D11_ONLY_DISPLAY_ANGLE, nullptr);
        if (es_display == EGL_NO_DISPLAY)
            FAIL(eglGetError());
        EGLint version[2];
        REQUIRE(eglInitialize(es_display, version + 0, version + 1));
        //REQUIRE(eglTerminate(es_display));
    }
}

void make_egl_config(EGLDisplay display, EGLConfig& config) {
    EGLint count = 0;
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
    if (eglChooseConfig(display, attrs, &config, 1, &count) == false)
        FAIL(eglGetError());
    REQUIRE(count);
}

/// @brief Create an EGLDisplay using the EGLDevice
/// @see https://github.com/google/angle/blob/master/src/tests/egl_tests/EGLDeviceTest.cpp
/// @see https://github.com/google/angle/blob/master/src/tests/egl_tests/EGLPresentPathD3D11Test.cpp
[[deprecated("the function makes crash on Qt5.12+")]] void make_egl_display(EGLDeviceEXT device, EGLDisplay& display) {
    display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, device, nullptr);
    if (display == EGL_NO_DISPLAY)
        FAIL(eglGetError());
    EGLint major = 0, minor = 0;
    if (eglInitialize(display, &major, &minor) == false)
        FAIL(eglGetError());
    REQUIRE(major == 1); // expect 1.4+
    REQUIRE(minor >= 4);
    REQUIRE(eglBindAPI(EGL_OPENGL_ES_API));
}
/*

void make_client_egl_surface(EGLDisplay display, EGLConfig config, ID3D11Texture2D* texture, EGLSurface& surface) {
    com_ptr<IDXGIResource> resource{};
    REQUIRE(texture->QueryInterface(resource.put()) == S_OK);
    HANDLE handle{};
    REQUIRE(resource->GetSharedHandle(&handle) == S_OK);
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    EGLint attrs[]{EGL_WIDTH,
                   gsl::narrow_cast<EGLint>(desc.Width),
                   EGL_HEIGHT,
                   gsl::narrow_cast<EGLint>(desc.Height),
                   EGL_TEXTURE_TARGET,
                   EGL_TEXTURE_2D,
                   EGL_TEXTURE_FORMAT,
                   EGL_TEXTURE_RGBA,
                   EGL_NONE};
    surface = eglCreatePbufferFromClientBuffer(display, EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE, handle, config, attrs);
    if (surface == EGL_NO_SURFACE)
        FAIL(eglGetError());
}

/// @see https://docs.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-layers
HRESULT make_device_and_context(ID3D11Device** device, ID3D11DeviceContext** context, D3D_FEATURE_LEVEL& level) {
    com_ptr<IDXGIFactory> dxgi = nullptr;
    REQUIRE(CreateDXGIFactory(IID_PPV_ARGS(dxgi.put())) == S_OK);
    com_ptr<IDXGIAdapter> adapter = nullptr;
    for (auto i = 0u; dxgi->EnumAdapters(i, adapter.put()) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC info{};
        REQUIRE(adapter->GetDesc(&info) == S_OK);
        break; // take the first one
    }
    return D3D11CreateDevice(adapter.get(), adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,             //
                             NULL, D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, //
                             D3D11_SDK_VERSION, device, &level, context);
}

class ID3D11Texture2DTestCase1 {
  protected:
    com_ptr<IWICImagingFactory> factory{};
    com_ptr<ID3D11Device> device{};
    com_ptr<ID3D11DeviceContext> device_context{};
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;

  public:
    ID3D11Texture2DTestCase1() noexcept(false) {
        REQUIRE(CoCreateInstance(CLSID_WICImagingFactory, NULL, //
                                 CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())) == S_OK);
        if (auto hr = make_device_and_context(device.put(), device_context.put(), level))
            FAIL(hr);
    }
};

fs::path get_asset_dir() noexcept;

// see https://docs.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_usage#resource-usage-restrictions
// see https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createtexture2d#remarks
TEST_CASE_METHOD(ID3D11Texture2DTestCase1, "ID3D11Texture2D(D3D11_USAGE_DYNAMIC)", "[directx][!mayfail][texture]") {
    com_ptr<ID3D11Texture2D> texture{};
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = desc.Height = 500;
    desc.MipLevels = desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    desc.Usage = D3D11_USAGE_DYNAMIC; // write by CPU, read by GPU
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    SECTION("DXGI_FORMAT_R8G8B8A8_UNORM") {
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        REQUIRE(device->CreateTexture2D(&desc, nullptr, texture.put()) == S_OK);
    }
    SECTION("DXGI_FORMAT_NV12") {
        desc.Format = DXGI_FORMAT_NV12;
        REQUIRE(device->CreateTexture2D(&desc, nullptr, texture.put()) == S_OK);
    }
    SECTION("DXGI_FORMAT_YUY2") {
        desc.Format = DXGI_FORMAT_YUY2;
        REQUIRE(device->CreateTexture2D(&desc, nullptr, texture.put()) == S_OK);
    }
    REQUIRE(texture);

    /// @see https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map
    DirectX::MapGuard mapping{device_context.get(), texture.get(), 0, D3D11_MAP_WRITE_DISCARD, 0};
    REQUIRE(mapping.pData);
    CAPTURE(mapping.RowPitch, mapping.DepthPitch);
    /// @note the pitch may different in runtime. It is expected to be aligned in 16 bytes
    switch (desc.Format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        REQUIRE(mapping.RowPitch >= 500 * 4); // or 512 * 4
        break;
    case DXGI_FORMAT_NV12:
        REQUIRE(mapping.RowPitch >= 500); // or 512
        break;
    case DXGI_FORMAT_YUY2:
        REQUIRE(mapping.RowPitch >= 500 * 2); // or 512 * 2
        break;
    }
}

SCENARIO_METHOD(ID3D11Texture2DTestCase1, "IWICBitmapDecoder", "[directx][!mayfail][file]") {
    GIVEN("image/png") {
        const auto fpath = get_asset_dir() / "image_1080_608.png";
        REQUIRE(fs::exists(fpath));

        com_ptr<IWICBitmapDecoder> decoder = nullptr;
        REQUIRE(factory->CreateDecoderFromFilename(fpath.c_str(), nullptr, GENERIC_READ, //
                                                   WICDecodeMetadataCacheOnDemand, decoder.put()) == S_OK);
        com_ptr<IWICBitmapFrameDecode> frame = nullptr;
        REQUIRE(decoder->GetFrame(0, frame.put()) == S_OK);

        UINT width{}, height{};
        REQUIRE(frame->GetSize(&width, &height) == S_OK);
        REQUIRE(width == 1080);
        REQUIRE(height == 608);
    }
    GIVEN("image/jpg") {
        const auto fpath = get_asset_dir() / "image_400_337.jpg";
        REQUIRE(fs::exists(fpath));

        com_ptr<IWICBitmapDecoder> decoder = nullptr;
        REQUIRE(factory->CreateDecoderFromFilename(fpath.c_str(), nullptr, GENERIC_READ, //
                                                   WICDecodeMetadataCacheOnDemand, decoder.put()) == S_OK);
        com_ptr<IWICBitmapFrameDecode> frame = nullptr;
        REQUIRE(decoder->GetFrame(0, frame.put()) == S_OK);

        UINT width{}, height{};
        REQUIRE(frame->GetSize(&width, &height) == S_OK);
        REQUIRE(width == 400);
        REQUIRE(height == 337);
    }
}

SCENARIO_METHOD(ID3D11Texture2DTestCase1, "CreateWICTextureFromFile", "[directx][!mayfail][file]") {
    GIVEN("image/png") {
        const auto fpath = get_asset_dir() / "image_1080_608.png";
        REQUIRE(fs::exists(fpath));

        com_ptr<ID3D11Resource> resource{};
        com_ptr<ID3D11ShaderResourceView> texture_view{};
        REQUIRE(DirectX::CreateWICTextureFromFile(device.get(), device_context.get(), //
                                                  fpath.c_str(),                      //
                                                  resource.put(), texture_view.put()) == S_OK);
        REQUIRE(resource);
        REQUIRE(texture_view);
        com_ptr<ID3D11Texture2D> texture{};
        REQUIRE(resource->QueryInterface(texture.put()) == S_OK);
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        REQUIRE(desc.Width == 1080);
        REQUIRE(desc.Height == 608);
    }
    GIVEN("image/jpg") {
        const auto fpath = get_asset_dir() / "image_400_337.jpg";
        REQUIRE(fs::exists(fpath));

        com_ptr<ID3D11Resource> resource{};
        com_ptr<ID3D11ShaderResourceView> texture_view{};
        REQUIRE(DirectX::CreateWICTextureFromFile(device.get(), device_context.get(), //
                                                  fpath.c_str(),                      //
                                                  resource.put(), texture_view.put()) == S_OK);
        REQUIRE(resource);
        REQUIRE(texture_view);
        com_ptr<ID3D11Texture2D> texture{};
        REQUIRE(resource->QueryInterface(texture.put()) == S_OK);
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        REQUIRE(desc.Width == 400);
        REQUIRE(desc.Height == 337);
    }
}

HRESULT make_test_texture2D(ID3D11Device* device, D3D11_TEXTURE2D_DESC& info, ID3D11Texture2D** texture) {
    info.Width = 640;
    info.Height = 480;
    info.MipLevels = info.ArraySize = 1;
    info.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    info.SampleDesc.Count = 1;
    info.SampleDesc.Quality = 0;
    info.Usage = D3D11_USAGE_DEFAULT;
    info.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    info.CPUAccessFlags = 0;
    info.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    return device->CreateTexture2D(&info, nullptr, texture);
}

/// @see https://github.com/google/angle/blob/master/util/EGLWindow.cpp
/// @see https://github.com/google/angle/blob/master/src/tests/egl_tests/EGLPresentPathD3D11Test.cpp
TEST_CASE_METHOD(ID3D11Texture2DTestCase1, "ID3D11Texture2D to EGLImage", "[directx][!mayfail][egl][!mayfail]") {
    D3D11_TEXTURE2D_DESC info{};
    com_ptr<ID3D11Texture2D> texture{};
    REQUIRE(make_test_texture2D(device.get(), info, texture.put()) == S_OK);
    REQUIRE(texture);

    EGLDisplay es_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    //auto on_return_2 = gsl::finally([es_display]() { REQUIRE(eglTerminate(es_display)); });

    EGLConfig es_config{};
    make_egl_config(es_display, es_config);

    EGLSurface es_surface = EGL_NO_SURFACE;
    make_client_egl_surface(es_display, es_config, texture.get(), es_surface);
    auto on_return_3 = gsl::finally([es_display, es_surface]() { REQUIRE(eglDestroySurface(es_display, es_surface)); });

    // at least 3.0
    EGLint attrs[]{EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
    EGLContext es_context = eglCreateContext(es_display, es_config, EGL_NO_CONTEXT, attrs);
    if (es_context == EGL_NO_CONTEXT)
        FAIL(eglGetError());

    THEN("Framebuffer status") {
        // when EGL_NO_SURFACE, default framebuffer is undefined...?
        //REQUIRE(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_UNDEFINED);
        if (eglMakeCurrent(es_display, es_surface, es_surface, es_context) == false)
            FAIL(eglGetError());
        REQUIRE(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    }

    if (eglMakeCurrent(es_display, es_surface, es_surface, es_context) == false)
        FAIL(eglGetError());
    REQUIRE(glGetString(GL_VERSION));

    THEN("bind to GL_TEXTURE_2D") {
        constexpr GLenum target = GL_TEXTURE_2D;
        GLuint tex{};
        glGenTextures(1, &tex);
        glBindTexture(target, tex);
        auto on_return_4 = gsl::finally([tex]() {
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
        auto on_return_4 = gsl::finally([tex]() {
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
        auto on_return_5 = gsl::finally([fbo]() {
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


/// @todo https://github.com/google/angle/blob/master/src/tests/egl_tests/EGLStreamTest.cpp
*/