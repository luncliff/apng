/**
 * @author Park DongHa (luncliff@gmail.com)
 */
#include <catch2/catch.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>

#include <nlohmann/json.hpp>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define TINYGLTF_NO_INCLUDE_JSON
#include <tiny_gltf.h>

namespace fs = std::filesystem;

fs::path get_asset_dir() noexcept;

TEST_CASE("Load GLB", "[gltf]") {
    auto fpath = get_asset_dir() / "Igloo.glb";
    REQUIRE(fs::exists(fpath));
    tinygltf::TinyGLTF loader{};
    tinygltf::Model model{};
    if (std::string e, w; loader.LoadBinaryFromFile(&model, &e, &w, fpath.generic_u8string()) == false) {
        spdlog::warn(w);
        FAIL(e);
    }
    REQUIRE(model.extensionsRequired.size() == 1);
}
