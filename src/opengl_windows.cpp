#include "opengl_es.h"

auto make_offscreen_context(uint32_t& ec, std::string& message) noexcept
    -> std::shared_ptr<void> {
    message.clear();
    try {
        auto bundle = new egl_bundle_t{};
        return std::shared_ptr<void>{bundle,
                                     [bundle](void*) { delete bundle; }};
    } catch (const std::runtime_error& ex) {
        message = ex.what();
        return nullptr;
    }
}
