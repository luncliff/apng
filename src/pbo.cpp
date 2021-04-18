#include <graphics.h>
#include <spdlog/spdlog.h>

pbo_reader_t::pbo_reader_t(GLuint length) noexcept : pbos{}, length{length}, offset{}, ec{GL_NO_ERROR} {
    spdlog::trace(__FUNCTION__);
    glGenBuffers(capacity, pbos);
    if (ec = glGetError())
        return;
    for (auto i = 0u; i < capacity; ++i) {
        spdlog::debug("- pbo:");
        spdlog::debug("  id: {}", pbos[i]);
        spdlog::debug("  length: {}", length);
        spdlog::debug("  usage: GL_STREAM_READ");
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER, length, nullptr, GL_STREAM_READ);
    }
    ec = glGetError();
}

GLenum pbo_reader_t::is_valid() const noexcept {
    return ec;
}

pbo_reader_t::~pbo_reader_t() noexcept {
    spdlog::trace(__FUNCTION__);
    for (auto i = 0u; i < capacity; ++i)
        spdlog::debug("- pbo: {}", pbos[i]);
    // delete and report if error generated
    glDeleteBuffers(capacity, pbos);
    if (auto ec = glGetError())
        spdlog::error("{} {}", __FUNCTION__, get_opengl_category().message(ec));
}

GLenum pbo_reader_t::pack(uint16_t idx, GLuint fbo, const GLint frame[4], GLenum format, GLenum type) noexcept {
    spdlog::trace(__FUNCTION__);
    if (idx >= capacity)
        return GL_INVALID_VALUE;
    //if (length < (frame[2] - frame[0]) * (frame[3] - frame[1]) * 4)
    //    return GL_OUT_OF_MEMORY;
    spdlog::debug("- pack:");
    spdlog::debug("  pbo: {}", pbos[idx]);
    spdlog::debug("  format: {:#x}", format);
    spdlog::debug("  type: {:#x}", type);
    spdlog::debug("  frame: '{} {} {} {}'", frame[0], frame[1], frame[2], frame[3]);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[idx]);
    glReadPixels(frame[0], frame[1], frame[2], frame[3], format, type, reinterpret_cast<void*>(offset));
    if (auto ec = glGetError())
        return ec; // probably GL_OUT_OF_MEMORY?
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    return glGetError();
}

GLenum pbo_reader_t::map_and_invoke(uint16_t idx, reader_callback_t callback, void* user_data) noexcept {
    spdlog::trace(__FUNCTION__);
    if (idx >= capacity)
        return GL_INVALID_VALUE;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[idx]);
    if (const void* ptr = glMapBufferRange(GL_PIXEL_PACK_BUFFER, offset, length, GL_MAP_READ_BIT)) {
        spdlog::debug("- mapping:");
        spdlog::debug("  pbo: {}", pbos[idx]);
        spdlog::debug("  offset: {}", offset);
        callback(user_data, ptr, length);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }
    return glGetError();
}

pbo_writer_t::pbo_writer_t(GLuint length) noexcept : pbos{}, length{length}, ec{GL_NO_ERROR} {
    spdlog::trace(__FUNCTION__);
    glGenBuffers(capacity, pbos);
    if (ec = glGetError())
        return;
    for (auto i = 0u; i < capacity; ++i) {
        spdlog::debug("- pbo:");
        spdlog::debug("  id: {}", pbos[i]);
        spdlog::debug("  length: {}", length);
        spdlog::debug("  usage: GL_STREAM_DRAW");
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, length, nullptr, GL_STREAM_DRAW);
    }
    ec = glGetError();
}

GLenum pbo_writer_t::is_valid() const noexcept {
    return ec;
}

pbo_writer_t::~pbo_writer_t() noexcept {
    spdlog::trace(__FUNCTION__);
    for (auto i = 0u; i < capacity; ++i)
        spdlog::debug("- pbo: {}", pbos[i]);
    // delete and report if error generated
    glDeleteBuffers(capacity, pbos);
    if (auto ec = glGetError())
        spdlog::error("{} {}", __FUNCTION__, get_opengl_category().message(ec));
}

/// @todo GL_MAP_UNSYNCHRONIZED_BIT?
GLenum pbo_writer_t::map_and_invoke(uint16_t idx, writer_callback_t callback, void* user_data) noexcept {
    spdlog::trace(__FUNCTION__);
    if (idx >= capacity)
        return GL_INVALID_VALUE;
    // 1 is for write (upload)
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[idx]);
    if (void* mapping = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, length, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT))
        callback(user_data, mapping, length);
    if (glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER) == false)
        spdlog::warn("unmap buffer failed: {}", pbos[idx]);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    return glGetError();
}

GLenum pbo_writer_t::unpack(uint16_t idx, GLuint tex2d, const GLint frame[4], //
                            GLenum format, GLenum type) noexcept {
    spdlog::trace(__FUNCTION__);
    if (idx >= capacity)
        return GL_INVALID_VALUE;
    GLenum ec = GL_NO_ERROR;
    glBindTexture(GL_TEXTURE_2D, tex2d);
    if (ec = glGetError())
        return ec;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[idx]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, frame[0], frame[1], frame[2], frame[3], format, type, 0);
    if (ec = glGetError())
        spdlog::warn("tex sub image failed: {}", pbos[idx]);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    return ec ? ec : glGetError();
}
