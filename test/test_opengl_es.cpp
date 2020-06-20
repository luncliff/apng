#include <catch2/catch.hpp>

#include <gsl/gsl>
#include <iostream>

#include <EGL/egl.h>
#include <EGL/eglext.h>

TEST_CASE("eglQueryString", "[egl]") {
    // eglQueryString returns static, zero-terminated string
    SECTION("EGL_EXTENSIONS") {
        const auto txt = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
        REQUIRE(txt);
        const auto txtlen = strlen(txt);

        auto offset = 0;
        for (auto i = 0u; i < txtlen; ++i) {
            if (isspace(txt[i]) == false)
                continue;
            const auto extname = std::string_view{txt + offset, i - offset};
            std::cout << extname << std::endl;
            offset = ++i;
        }
    }
}
