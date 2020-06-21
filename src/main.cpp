#include <filesystem>

namespace fs = std::filesystem;

using namespace std;

#if defined(_WIN32)

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

auto read_all(FILE* stream, size_t& rsz) -> std::unique_ptr<std::byte[]> {
    struct _stat64 info {};
    if (_fstat64(_fileno(stream), &info) != 0)
        throw system_error{errno, system_category(), "_fstat64"};
    auto blob = make_unique<std::byte[]>(info.st_size);
    rsz = 0;
    while (!feof(stream)) {
        void* b = blob.get() + rsz;
        const auto sz = info.st_size - rsz;
        rsz += fread_s(b, sz, sizeof(std::byte), sz, stream);
        if (auto ec = ferror(stream))
            throw system_error{ec, system_category(), "fread_s"};
        if (rsz == static_cast<size_t>(info.st_size))
            break;
    }
    return blob;
}

#else
#include <sys/stat.h>

auto create(const fs::path& p) -> std::unique_ptr<FILE, int (*)(FILE*)> {
    auto fpath = p.generic_u8string();
    FILE* fp = fopen(fpath.c_str(), "w+b");
    if (fp == nullptr)
        throw system_error{errno, system_category(), "fopen"};
    return {fp, &fclose};
}

auto open(const fs::path& p) -> std::unique_ptr<FILE, int (*)(FILE*)> {
    auto fpath = p.generic_u8string();
    FILE* fp = fopen(fpath.c_str(), "rb");
    if (fp == nullptr)
        throw system_error{errno, system_category(), "fopen"};
    return {fp, &fclose};
}

auto read_all(FILE* stream, size_t& rsz) -> std::unique_ptr<std::byte[]> {
    struct stat info {};
    if (fstat(fileno(stream), &info) != 0)
        throw system_error{errno, system_category(), "fstat"};
    auto blob = make_unique<std::byte[]>(info.st_size);
    rsz = 0;
    while (!feof(stream)) {
        void* b = blob.get() + rsz;
        const auto sz = info.st_size - rsz;
        rsz += fread(b, sizeof(byte), sz, stream);
        if (auto ec = ferror(stream))
            throw system_error{ec, system_category(), "fread"};
        if (rsz == static_cast<size_t>(info.st_size))
            break;
    }
    return blob;
}
#endif

auto read_all(const fs::path& p, size_t& fsize)
    -> std::unique_ptr<std::byte[]> {
    auto fin = open(p);
    return read_all(fin.get(), fsize);
}
