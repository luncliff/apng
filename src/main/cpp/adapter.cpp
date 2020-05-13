/**
 * @author luncliff@gmail.com
 */
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <experimental/coroutine>

#include "dev_luncliff_Native.h"

extern "C" jint JNI_OnLoad(JavaVM* vm, void*) {
    constexpr auto version = JNI_VERSION_1_8;
    JNIEnv* env{};
    jint result = -1;
    if (vm->GetEnv((void**)&env, version) != JNI_OK) {
        return result;
    }
    return version;
}

jstring Java_dev_luncliff_Native_GetSystemMessage(JNIEnv* env, jclass, jint code) {
    const char* message = strerror(code);
    return env->NewStringUTF(message);
}
