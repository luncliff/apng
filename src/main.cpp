#include <graphics.h>

namespace fs = std::filesystem;

using namespace std;

auto create(const fs::path& p) -> std::unique_ptr<FILE, int (*)(FILE*)> {
    auto fpath = p.generic_wstring();
    FILE* fp{};
    if (auto ec = _wfopen_s(&fp, fpath.c_str(), L"w+b"))
        throw system_error{static_cast<int>(ec), system_category(), "fopen"};
    return {fp, &fclose};
}

auto open(const fs::path& p) -> std::unique_ptr<FILE, int (*)(FILE*)> {
    auto fpath = p.generic_wstring();
    FILE* fp{};
    if (auto ec = _wfopen_s(&fp, fpath.c_str(), L"rb"))
        throw system_error{static_cast<int>(ec), system_category(), "fopen"};
    return {fp, &fclose};
}

uint32_t get_size(FILE* stream, size_t& sz) noexcept {
    struct _stat64 info {};
    if (_fstat64(_fileno(stream), &info) != 0)
        return errno; // throw system_error{errno, system_category(), "_fstat64"};
    sz = gsl::narrow_cast<size_t>(info.st_size);
    return 0;
}

uint32_t fill(FILE* stream, size_t& rsz, std::byte* buf, size_t buflen) noexcept {
    rsz = 0;
    while (!feof(stream)) {
        void* b = buf + rsz;
        const auto sz = buflen - rsz;
        rsz += fread_s(b, sz, sizeof(std::byte), sz, stream);
        if (auto ec = ferror(stream))
            return ec; // throw system_error{ec, system_category(), "fread_s"};
        if (rsz == buflen)
            break;
    }
    return 0;
}
