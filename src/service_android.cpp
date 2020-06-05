#include "service.hpp"

#include <android/asset_manager_jni.h>
#include <android/log.h>

#include <jni.h>

void execute(JNIEnv* env, jobject executor, jobject runnable) noexcept {
    jclass const clazz = env->GetObjectClass(executor);
    jmethodID const method = env->GetMethodID(clazz, "execute", //
                                              "(Ljava/lang/Runnable;)V");
    jvalue args{.l = runnable};
    env->CallVoidMethodA(executor, method, &args);
    env->DeleteLocalRef(clazz);
}
