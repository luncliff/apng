/**
 * @author Park DongHa (luncliff@gmail.com)
 * @see https://github.com/microsoft/angle/wiki
 * @see https://github.com/microsoft/DirectXTK/wiki/Getting-Started
 * @todo https://github.com/google/angle/blob/master/src/tests/egl_tests/EGLStreamTest.cpp
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
#include <concurrent_queue.h>
#include <pplcancellation_token.h>

// clang-format off
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <DirectXTK/DirectXHelpers.h>
#include <DirectXTK/WICTextureLoader.h>
#include <DirectXTK/ScreenGrab.h>

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

TEST_CASE("D3D_FEATURE_LEVEL_11_0(D3D_DRIVER_TYPE_REFERENCE)", "[directx][!mayfail]") {
    com_ptr<ID3D11Device> device{};
    com_ptr<ID3D11DeviceContext> device_context{};
    D3D_FEATURE_LEVEL levels[1]{D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL level{};
    REQUIRE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_REFERENCE, NULL,
                              D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 1, //
                              D3D11_SDK_VERSION, device.put(), &level, device_context.put()) == S_OK);
    REQUIRE(device);
    REQUIRE(device->GetFeatureLevel() == level);
}

TEST_CASE("D3D_FEATURE_LEVEL_11_0(D3D_DRIVER_TYPE_NULL)", "[directx][!mayfail]") {
    com_ptr<ID3D11Device> device{};
    com_ptr<ID3D11DeviceContext> device_context{};
    D3D_FEATURE_LEVEL levels[1]{D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL level{};
    REQUIRE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_NULL, NULL,
                              D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 1, //
                              D3D11_SDK_VERSION, device.put(), &level, device_context.put()) == S_OK);
    REQUIRE(device);
    REQUIRE(device->GetFeatureLevel() == level);
}

TEST_CASE("D3D_FEATURE_LEVEL_10_1(D3D_DRIVER_TYPE_HARDWARE)", "[directx][!mayfail]") {
    com_ptr<ID3D11Device> device{};
    com_ptr<ID3D11DeviceContext> device_context{};
    D3D_FEATURE_LEVEL levels[1]{D3D_FEATURE_LEVEL_10_1};
    D3D_FEATURE_LEVEL level{};
    REQUIRE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL,
                              D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 1, //
                              D3D11_SDK_VERSION, device.put(), &level, device_context.put()) == S_OK);
    REQUIRE(device);
    REQUIRE(device->GetFeatureLevel() == level);
}

TEST_CASE("D3D_FEATURE_LEVEL_11_1(D3D_DRIVER_TYPE_HARDWARE)", "[directx][!mayfail]") {
    com_ptr<ID3D11Device> device{};
    com_ptr<ID3D11DeviceContext> device_context{};
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
        EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
        EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE,
        EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE,
        11,
        EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE,
        EGL_DONT_CARE,
        EGL_NONE,
    };
    EGLDisplay display = eglGetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, attrs);
    REQUIRE(display != EGL_NO_DISPLAY);
}

TEST_CASE("Device(11.0) for ANGLE", "[egl][directx][!mayfail]") {
    com_ptr<ID3D11Device> device{};
    com_ptr<ID3D11DeviceContext> device_context{};
    D3D_FEATURE_LEVEL level{};
    REQUIRE(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL,
                              D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT |
                                  D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                              nullptr, 0, //
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
        const auto flags = handle->GetCreationFlags();
        REQUIRE(flags & D3D11_CREATE_DEVICE_SINGLETHREADED);
        REQUIRE(flags & D3D11_CREATE_DEVICE_BGRA_SUPPORT);
        REQUIRE(flags & D3D11_CREATE_DEVICE_VIDEO_SUPPORT);
    }

#if !defined(QT_OPENGL_LIB)
    // Qt 5.12+ makes a crash in DllMain resouce cleanup after this TC
    // Qt 5.9 seems like OK after the `eglReleaseDeviceANGLE`...
    SECTION("eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT)") {
        EGLDisplay es_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, es_device, nullptr);
        if (es_display == EGL_NO_DISPLAY)
            FAIL(eglGetError());
        EGLint versions[2]{};
        REQUIRE(eglInitialize(es_display, versions + 0, versions + 1));
        REQUIRE(eglTerminate(es_display));
    }
#endif

    /// @see https://github.com/google/angle/blob/master/extensions/EGL_ANGLE_platform_angle.txt
    SECTION("eglGetPlatformDisplayEXT(EGL_DEFAULT_DISPLAY)") {
        EGLDisplay es_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, nullptr);
        if (es_display == EGL_NO_DISPLAY)
            FAIL(eglGetError());
        EGLint version[2];
        REQUIRE(eglInitialize(es_display, version + 0, version + 1));
        REQUIRE(eglTerminate(es_display));
    }
    SECTION("eglGetPlatformDisplayEXT(EGL_D3D11_ONLY_DISPLAY_ANGLE)") {
        EGLDisplay es_display =
            eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_D3D11_ONLY_DISPLAY_ANGLE, nullptr);
        if (es_display == EGL_NO_DISPLAY)
            FAIL(eglGetError());
        EGLint version[2];
        REQUIRE(eglInitialize(es_display, version + 0, version + 1));
        REQUIRE(eglTerminate(es_display));
    }
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
        if (auto hr = make_device_and_context(device.put(), device_context.put(), level); FAILED(hr))
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

/// @see https://github.com/Microsoft/angle/wiki/Interop-with-other-DirectX-code
class ID3D11Texture2DTestCase2 {
  protected:
    com_ptr<ID3D11Device> device{};
    com_ptr<ID3D11DeviceContext> device_context{};
    com_ptr<ID3D11Texture2D> texture{};
    EGLDisplay es_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    egl_context_t es_context;

  public:
    ID3D11Texture2DTestCase2() noexcept(false) : es_context{es_display, EGL_NO_CONTEXT} {
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
        if (auto hr = make_device_and_context(device.put(), device_context.put(), level); FAILED(hr))
            FAIL(hr);
        D3D11_TEXTURE2D_DESC info{};
        info.Width = 640;
        info.Height = 480;
        info.MipLevels = info.ArraySize = 1;
        info.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        info.SampleDesc.Count = 1;
        info.SampleDesc.Quality = 0;
        info.Usage = D3D11_USAGE_DEFAULT;
        info.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        info.CPUAccessFlags = 0;
        info.MiscFlags = D3D11_RESOURCE_MISC_SHARED; // for IDXGIResource::GetSharedHandle
        REQUIRE(device->CreateTexture2D(&info, nullptr, texture.put()) == S_OK);
    }
    ~ID3D11Texture2DTestCase2() {
        if (auto ec = es_context.suspend(); ec != EGL_SUCCESS)
            spdlog::error("es_context failed to suspend: {}", ec);
        es_context.destroy();
    }
};

void setup_egl_config(EGLDisplay display, EGLConfig& config, EGLint& minor);

TEST_CASE_METHOD(ID3D11Texture2DTestCase2, "Texture2D to EGLSurface(eglCreatePbufferFromClientBuffer)",
                 "[directx][!mayfail][egl]") {
    {
        EGLConfig es_config{};
        EGLSurface es_surface = EGL_NO_SURFACE;
        EGLint count = 1;
        REQUIRE(es_context.get_configs(&es_config, count, nullptr) == 0);
        if (auto ec = make_egl_client_surface(es_display, es_config, texture.get(), es_surface))
            FAIL(ec);
        // transfer the ownership to es_context
        REQUIRE(es_context.resume(es_surface, es_config) == EGL_SUCCESS);
    }
    REQUIRE(glGetString(GL_VERSION));

    SECTION("GL_FRAMEBUFFER_COMPLETE") {
        REQUIRE(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        REQUIRE(eglMakeCurrent(es_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    }
    SECTION("GL_DRAW_FRAMEBUFFER_BINDING") {
        GLint fbo = UINT32_MAX;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
        CAPTURE(fbo);
        REQUIRE(glGetError() == GL_NO_ERROR);
    }
    SECTION("glGetFramebufferAttachmentParameteriv") {
        GLint value = 0;
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &value);
        if (auto ec = glGetError())
            FAIL("GL_COLOR_ATTACHMENT0/GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE");
        // ...
    }
#if defined(GL_FRAMEBUFFER_DEFAULT_WIDTH) && defined(GL_FRAMEBUFFER_DEFAULT_HEIGHT)
    SECTION("glGetFramebufferParameteriv") {
        GLint value = 0;
        glGetFramebufferParameteriv(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, &value);
        if (auto ec = glGetError())
            FAIL("GL_FRAMEBUFFER_DEFAULT_WIDTH");
        glGetFramebufferParameteriv(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, &value);
        if (auto ec = glGetError())
            FAIL("GL_FRAMEBUFFER_DEFAULT_HEIGHT");
    }
#endif
}

/// @see https://github.com/google/angle/blob/master/util/EGLWindow.cpp
/// @see https://github.com/google/angle/blob/master/src/tests/egl_tests/EGLPresentPathD3D11Test.cpp
TEST_CASE_METHOD(ID3D11Texture2DTestCase2, "Texture2D to EGLImage(eglBindTexImage)", "[directx][!mayfail][egl]") {
    EGLConfig es_config{};
    {
        EGLSurface es_surface = EGL_NO_SURFACE;
        EGLint count = 1;
        REQUIRE(es_context.get_configs(&es_config, count, nullptr) == 0);
        EGLint attrs[]{EGL_WIDTH, 128, EGL_HEIGHT, 128, EGL_NONE};
        es_surface = eglCreatePbufferSurface(es_display, es_config, attrs);
        if (es_surface == EGL_NO_SURFACE)
            REQUIRE(eglGetError() == EGL_SUCCESS);
        // transfer the ownership to es_context
        REQUIRE(es_context.resume(es_surface, es_config) == EGL_SUCCESS);
    }
    REQUIRE(glGetString(GL_VERSION));

    EGLSurface es_surface = EGL_NO_SURFACE;
    if (auto ec = make_egl_client_surface(es_display, es_config, texture.get(), es_surface))
        FAIL(ec);
    auto on_return_1 = gsl::finally([display = es_display, es_surface]() { eglDestroySurface(display, es_surface); });

    SECTION("bind to GL_TEXTURE_2D") {
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
    SECTION("bind to GL_FRAMEBUFFER") {
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

class mapped_tex2d_t final {
    EGLDisplay display;
    EGLSurface surface;
    com_ptr<ID3D11Texture2D> texture;
    GLuint tex{};

  public:
    mapped_tex2d_t(EGLDisplay _display, EGLConfig _config, com_ptr<ID3D11Texture2D> _texture)
        : display{_display}, surface{EGL_NO_SURFACE}, texture{_texture} {
        if (auto ec = make_egl_client_surface(_display, _config, texture.get(), surface))
            FAIL(ec);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        REQUIRE(eglBindTexImage(display, surface, EGL_BACK_BUFFER));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
        REQUIRE(glGetError() == GL_NO_ERROR);
    }
    ~mapped_tex2d_t() noexcept {
        glDeleteTextures(1, &tex);
        eglDestroySurface(display, surface);
    }
    GLuint handle() const noexcept {
        return tex;
    }
};

class fbo_owner_t final {
    GLuint fbo{};

  public:
    fbo_owner_t(GLuint tex) noexcept {
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        REQUIRE(glGetError() == GL_NO_ERROR);
    }
    ~fbo_owner_t() noexcept {
        glDeleteFramebuffers(1, &fbo);
    }

    GLint handle() const noexcept {
        return fbo;
    }
};

TEST_CASE_METHOD(ID3D11Texture2DTestCase2, "Save Texture2D to file", "[directx]") {
    EGLConfig es_config{};
    {
        EGLSurface es_surface = EGL_NO_SURFACE;
        EGLint count = 1;
        REQUIRE(es_context.get_configs(&es_config, count, nullptr) == 0);
        EGLint attrs[]{EGL_WIDTH, 128, EGL_HEIGHT, 128, EGL_NONE};
        es_surface = eglCreatePbufferSurface(es_display, es_config, attrs);
        if (es_surface == EGL_NO_SURFACE)
            REQUIRE(eglGetError() == EGL_SUCCESS);
        // transfer the ownership to es_context
        REQUIRE(es_context.resume(es_surface, es_config) == EGL_SUCCESS);
    }
    REQUIRE(glGetString(GL_VERSION));

    mapped_tex2d_t tex{es_display, es_config, texture};
    fbo_owner_t framebuffer{tex.handle()};

    REQUIRE(framebuffer.handle() != 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer.handle());
    auto on_return = gsl::finally([]() { glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); });

    REQUIRE(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glClearColor(1, 1, 0, 1); // the files should have red + green
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();
    REQUIRE(glGetError() == GL_NO_ERROR);

    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    REQUIRE(desc.CPUAccessFlags == 0);
    REQUIRE(desc.Usage == D3D11_USAGE_DEFAULT);

    com_ptr<ID3D11Resource> resource{};
    REQUIRE(texture->QueryInterface(resource.put()) == S_OK);

    SECTION("GUID_ContainerFormatPng") {
        REQUIRE(DirectX::SaveWICTextureToFile(device_context.get(), resource.get(), GUID_ContainerFormatPng,
                                              L"texture_B8G8R8A8_UNORM.png") == S_OK);
    }
    SECTION("GUID_ContainerFormatJpeg") {
        REQUIRE(DirectX::SaveWICTextureToFile(device_context.get(), resource.get(), GUID_ContainerFormatJpeg,
                                              L"texture_B8G8R8A8_UNORM.jpeg") == S_OK);
    }
    SECTION("GUID_ContainerFormatHeif") {
        REQUIRE(DirectX::SaveWICTextureToFile(device_context.get(), resource.get(), GUID_ContainerFormatHeif,
                                              L"texture_B8G8R8A8_UNORM.heif") == S_OK);
    }
    SECTION("GUID_ContainerFormatWebp") {
        auto hr = DirectX::SaveWICTextureToFile(device_context.get(), resource.get(), GUID_ContainerFormatWebp,
                                                L"texture_B8G8R8A8_UNORM.webp");
        REQUIRE(hr == WINCODEC_ERR_COMPONENTNOTFOUND);
    }
}

/// @see https://docs.microsoft.com/en-us/cpp/parallel/concrt/reference/concurrency-namespace?view=msvc-160
TEST_CASE("concurrent_queue", "[windows]") {
    concurrency::concurrent_queue<winrt::com_ptr<ID3D11Texture2D>> queue{};
    REQUIRE(queue.empty());
    {
        queue.push(nullptr);
        winrt::com_ptr<ID3D11Texture2D> texture{};
        REQUIRE(queue.try_pop(texture));
        REQUIRE_FALSE(texture);
    }

    concurrency::cancellation_token_source token_source{};
    concurrency::cancellation_token token = token_source.get_token();
    REQUIRE_FALSE(token.is_canceled());
    REQUIRE(token.is_cancelable());
    REQUIRE_NOTHROW(token_source.cancel());
    REQUIRE(token.is_canceled());
};