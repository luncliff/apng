//#define CATCH_CONFIG_MAIN
//#define CATCH_CONFIG_WINDOWS_CRTDBG
#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>
#include <catch2/catch_reporter_sonarqube.hpp>
#define SPDLOG_HEADER_ONLY
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <gsl/gsl>

#include <pplawait.h>
#include <ppltasks.h>
#include <winrt/Windows.Foundation.h> // namespace winrt::Windows::Foundation
#include <winrt/Windows.System.h>     // namespace winrt::Windows::System

namespace fs = std::filesystem;
using namespace std;
using winrt::com_ptr;

fs::path get_asset_dir() noexcept {
#if defined(ASSET_DIR)
    if (fs::exists(ASSET_DIR))
        return {ASSET_DIR};
#endif
    return fs::current_path();
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, ".65001");

    auto stream = spdlog::stdout_color_st("test");
    stream->set_pattern("[%^%l%$] %v");
    stream->set_level(spdlog::level::level_enum::trace);
    spdlog::set_default_logger(stream);

    winrt::init_apartment();
    auto on_exit = gsl::finally(&winrt::uninit_apartment);
    Catch::Session session{};
    return session.run(argc, argv);
}
