//#define CATCH_CONFIG_MAIN
//#define CATCH_CONFIG_WINDOWS_CRTDBG
#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>
#define SPDLOG_HEADER_ONLY
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <filesystem>
#include <gsl/gsl>

#include <pplawait.h>
#include <ppltasks.h>
#include <winrt/Windows.Foundation.h> // namespace winrt::Windows::Foundation
#include <winrt/Windows.System.h>     // namespace winrt::Windows::System

namespace fs = std::filesystem;
using namespace std;
using winrt::com_ptr;

fs::path get_asset_dir() noexcept;
auto get_current_stream() noexcept -> std::shared_ptr<spdlog::logger>;

TEST_CASE("GLFW Required Extensions", "[glfw][extension]") {
    if (glfwInit() == GLFW_FALSE) {
        const char* message = nullptr;
        CAPTURE(glfwGetError(&message)); // get the recent error code
        FAIL(message);
    }
    auto on_return = gsl::finally(&glfwTerminate);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    uint32_t count = 0;
    const char** names = glfwGetRequiredInstanceExtensions(&count);
    REQUIRE(count > 0);

    auto stream = get_current_stream();
    stream->info("instance_required:");
    for (auto i = 0u; i < count; ++i)
        stream->info(" - {}", names[i]);
}

TEST_CASE("Vulkan Instance Layers", "[extension]") {
    uint32_t count = 0;
    REQUIRE(vkEnumerateInstanceLayerProperties(&count, nullptr) == VK_SUCCESS);
    auto layers = make_unique<VkLayerProperties[]>(count);
    REQUIRE(vkEnumerateInstanceLayerProperties(&count, layers.get()) == VK_SUCCESS);

    auto stream = get_current_stream();
    stream->info("instance_layers:");
    for (auto i = 0u; i < count; ++i) {
        string_view name{layers[i].layerName};
        const auto spec = layers[i].specVersion;
        stream->info(" - {}: {:x}", name, spec);
    }
}

TEST_CASE("Vulkan Instance Extenstions", "[extension]") {
    const char* layer = nullptr;
    uint32_t count = 0;
    REQUIRE(vkEnumerateInstanceExtensionProperties( //
                layer, &count, nullptr) == VK_SUCCESS);
    REQUIRE(count > 0);
    auto extensions = make_unique<VkExtensionProperties[]>(count);
    REQUIRE(vkEnumerateInstanceExtensionProperties( //
                layer, &count, extensions.get()) == VK_SUCCESS);

    auto stream = get_current_stream();
    stream->info("instance_extensions:");
    for (auto i = 0u; i < count; ++i) {
        string_view name{extensions[i].extensionName};
        const auto spec = extensions[i].specVersion;
        stream->info(" - {}: {:x}", name, spec);
    }
}

fs::path get_asset_dir() noexcept {
#if defined(ASSET_DIR)
    if (fs::exists(ASSET_DIR))
        return {ASSET_DIR};
#endif
    return fs::current_path();
}

auto stream = spdlog::stdout_color_st("test");

auto get_current_stream() noexcept -> std::shared_ptr<spdlog::logger> {
    using namespace spdlog::level;
    stream->set_pattern("%v");
    stream->set_level(level_enum::trace);
    return stream;
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, ".65001");
    winrt::init_apartment();
    auto on_exit = gsl::finally(&winrt::uninit_apartment);
    return Catch::Session().run(argc, argv);
}
