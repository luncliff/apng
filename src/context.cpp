/**
 * @author Park DongHa (luncliff@gmail.com)
 */
#include <graphics.h>
#include <spdlog/spdlog.h>
#if __has_include(<EGL/eglext_angle.h>)
#include <EGL/eglext_angle.h>
#endif
#include <winrt/base.h>

#define report_error_code(fname, ec) spdlog::error("{}: {:#x}", fname, ec), ec;

class opengl_error_category_t final : public std::error_category {
    const char* name() const noexcept override {
        return "OpenGL";
    }
    std::string message(int ec) const override {
        constexpr auto bufsz = 40;
        char buf[bufsz]{};
        const auto len = snprintf(buf, bufsz, "error %5d(%4x)", ec, ec);
        return {buf, static_cast<size_t>(len)};
    }
};

std::error_category& get_opengl_category() noexcept {
    static opengl_error_category_t single{};
    return single;
};

EGLint get_configs(EGLDisplay display, EGLConfig* configs, EGLint& count, const EGLint* attrs) noexcept {
    constexpr auto color_size = 8;
    constexpr auto depth_size = 16;
    EGLint backup_attrs[]{EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                          EGL_BLUE_SIZE,       color_size,         EGL_GREEN_SIZE,   color_size,
                          EGL_RED_SIZE,        color_size,         EGL_ALPHA_SIZE,   color_size,
                          EGL_DEPTH_SIZE,      depth_size,         EGL_NONE};
    if (attrs == nullptr)
        attrs = backup_attrs;
    if (eglChooseConfig(display, attrs, configs, count, &count) == EGL_FALSE)
        return eglGetError();
    return 0;
}

egl_surface_owner_t::egl_surface_owner_t(EGLDisplay display, EGLConfig config, EGLSurface surface) noexcept
    : display{display}, config{config}, surface{surface} {
}

egl_surface_owner_t::~egl_surface_owner_t() noexcept {
    if (eglDestroySurface(display, surface) == EGL_TRUE)
        return;
    auto ec = eglGetError();
    spdlog::error("{}: {:#x}", "eglDestroySurface", ec);
}

EGLint egl_surface_owner_t::get_size(EGLint& width, EGLint& height) noexcept {
    eglQuerySurface(display, surface, EGL_WIDTH, &width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &height);
    if (auto ec = eglGetError(); ec != EGL_SUCCESS) {
        spdlog::error("{}: {:#x}", "eglQuerySurface", ec);
        return ec;
    }
    return 0;
}

EGLSurface egl_surface_owner_t::handle() const noexcept {
    return surface;
}

egl_context_t::egl_context_t(EGLDisplay display, EGLContext share_context) noexcept : display{display} {
    spdlog::debug(__FUNCTION__);
    if (eglInitialize(display, versions + 0, versions + 1) == false) {
        report_error_code("eglInitialize", eglGetError());
        return;
    }
    spdlog::debug("EGLDisplay {} {}.{}", display, versions[0], versions[1]);

    // acquire EGLConfigs
    EGLint num_config = 1;
    if (auto ec = get_configs(display, configs, num_config, nullptr)) {
        spdlog::error("{}: {:#x}", "eglChooseConfig", ec);
        return;
    }

    // create context for OpenGL ES 3.0+
    EGLint attrs[]{EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
    if (context = eglCreateContext(display, configs[0], share_context, attrs); context != EGL_NO_CONTEXT)
        spdlog::debug("EGL create: context {} {}", context, share_context);
}

egl_context_t::~egl_context_t() noexcept {
    spdlog::debug(__FUNCTION__);
    destroy();
}

EGLint egl_context_t::resume(EGLSurface es_surface, EGLConfig) noexcept {
    if (context == EGL_NO_CONTEXT)
        return EGL_NOT_INITIALIZED;
    if (es_surface == EGL_NO_SURFACE)
        return GL_INVALID_VALUE;
    surface = es_surface;
    spdlog::debug("EGL current: {}/{} {}", surface, surface, context);
    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        auto ec = eglGetError();
        spdlog::error("{}: {:#x}", "eglMakeCurrent", ec);
        return ec;
    }
    return 0;
}

EGLint egl_context_t::resume(gsl::not_null<EGLNativeWindowType> window) noexcept {
    return ENOTSUP;
    // create surface with the window
    EGLint* attrs = nullptr;
    if (surface = eglCreateWindowSurface(display, configs[0], window, attrs); surface == EGL_NO_SURFACE)
        return eglGetError(); /// @todo the value can be EGL_SUCCESS. Check the available cases

    // bind surface and context
    spdlog::debug("EGL current: {}/{} {}", surface, surface, context);
    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE)
        return report_error_code("eglMakeCurrent", eglGetError());
    return 0;
}

EGLint egl_context_t::suspend() noexcept {
    spdlog::trace(__FUNCTION__);
    if (context == EGL_NO_CONTEXT)
        return EGL_NOT_INITIALIZED;

    // unbind surface. OpenGL ES 3.1 will return true
    spdlog::debug("EGL current: EGL_NO_SURFACE/EGL_NO_SURFACE {}", context);
    if (eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context) == EGL_FALSE) {
        // OpenGL ES 3.0 will report error. consume it
        // then unbind both surface and context.
        report_error_code("eglMakeCurrent", eglGetError());
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    surface = EGL_NO_SURFACE;
    return 0;
}

void egl_context_t::destroy() noexcept {
    spdlog::trace(__FUNCTION__);
    if (display == EGL_NO_DISPLAY) // already terminated
        return;

    // unbind surface and context
    spdlog::debug("EGL current: EGL_NO_SURFACE/EGL_NO_SURFACE EGL_NO_CONTEXT");
    if (eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE) {
        report_error_code("eglMakeCurrent", eglGetError());
        return;
    }
    // destroy known context
    if (context != EGL_NO_CONTEXT) {
        spdlog::warn("EGL destroy: context {}", context);
        if (eglDestroyContext(display, context) == EGL_FALSE)
            report_error_code("eglDestroyContext", eglGetError());
        context = EGL_NO_CONTEXT;
    }
    // destroy known surface
    if (surface != EGL_NO_SURFACE) {
        spdlog::warn("EGL destroy: surface {}", surface);
        if (eglDestroySurface(display, surface) == EGL_FALSE)
            report_error_code("eglDestroySurface", eglGetError());
        surface = EGL_NO_SURFACE;
    }
    display = EGL_NO_DISPLAY;
}

EGLint egl_context_t::swap() noexcept {
    if (eglSwapBuffers(display, surface))
        return 0;
    switch (const auto ec = eglGetError()) {
    case EGL_BAD_CONTEXT:
    case EGL_CONTEXT_LOST:
        destroy();
        [[fallthrough]];
    default:
        return ec; // EGL_BAD_SURFACE and the others ...
    }
}

EGLContext egl_context_t::handle() const noexcept {
    return context;
}

EGLConfig egl_context_t::config() const noexcept {
    return configs[0];
}

bool for_each_extension(EGLDisplay display, bool (*handler)(std::string_view, void* ptr), void* ptr) noexcept {
    if (const auto txt = eglQueryString(display, EGL_EXTENSIONS)) {
        const auto txtlen = strlen(txt);
        auto offset = 0;
        for (auto i = 0u; i < txtlen; ++i) {
            if (isspace(txt[i]) == false)
                continue;
            if (handler({txt + offset, i - offset}, ptr))
                return true;
            offset = ++i;
        }
    }
    return false;
}

void get_extensions(EGLDisplay display, std::vector<std::string_view>& names) noexcept {
    for_each_extension(
        display,
        [](std::string_view name, void* ptr) {
            auto& ref = *reinterpret_cast<std::vector<std::string_view>*>(ptr);
            ref.emplace_back(name);
            return false; // continue loop
        },
        &names);
}

bool has_extension(EGLDisplay display, std::string_view name) noexcept {
    return for_each_extension(
        display,
        [](std::string_view name, void* ptr) {
            const auto& ref = *reinterpret_cast<std::string_view*>(ptr);
            return ref == name; // if same name, then end the loop.
        },
        &name);
}

uint32_t make_egl_attributes(gsl::not_null<ID3D11Texture2D*> texture, std::vector<EGLint>& attrs) noexcept {
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    attrs.emplace_back(EGL_TEXTURE_TARGET);
    attrs.emplace_back(EGL_TEXTURE_2D);
    switch (desc.Format) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        attrs.emplace_back(EGL_TEXTURE_FORMAT);
        attrs.emplace_back(EGL_TEXTURE_RGBA);
        break;
    default:
        return ENOTSUP;
    }
    attrs.emplace_back(EGL_WIDTH);
    attrs.emplace_back(gsl::narrow_cast<EGLint>(desc.Width));
    attrs.emplace_back(EGL_HEIGHT);
    attrs.emplace_back(gsl::narrow_cast<EGLint>(desc.Height));
    attrs.emplace_back(EGL_NONE);
    return 0;
}

uint32_t make_egl_client_surface(EGLDisplay display, EGLConfig config, ID3D11Texture2D* texture,
                                 EGLSurface& surface) noexcept {
    if (texture == nullptr)
        return EINVAL;
    if (has_extension(display, "EGL_ANGLE_surface_d3d_texture_2d_share_handle") == false)
        return ENOTSUP;
    std::vector<EGLint> attrs{};
    if (auto ec = make_egl_attributes(texture, attrs))
        return ec;

    winrt::com_ptr<IDXGIResource> resource{};
    if (auto hr = texture->QueryInterface(resource.put()); FAILED(hr))
        return hr;
    HANDLE handle{};
    if (auto hr = resource->GetSharedHandle(&handle); FAILED(hr))
        return hr;

    surface = eglCreatePbufferFromClientBuffer(display, EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE, handle, //
                                               config, attrs.data());
    if (surface == EGL_NO_SURFACE)
        return eglGetError();
    return EXIT_SUCCESS;
}
