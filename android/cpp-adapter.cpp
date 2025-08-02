#include "context.h"
#include "unpack.h"
#include <jni.h>
#include <fstream>

// package com.qnnllm

// Context::getVersion(): String
extern "C" JNIEXPORT jstring JNICALL Java_com_qnnllm_Context_getVersion(JNIEnv *env,
                                                                              jclass jthiz) {
  return env->NewStringUTF(qnnllm::Context::version().c_str());
}

// Context::create(config: String): Context*
extern "C" JNIEXPORT jlong JNICALL Java_com_qnnllm_Context_create(JNIEnv *env, jclass jthiz,
                                                                        jstring lib_path,
                                                                        jstring jconfig) {
  const char *lib_path_str = env->GetStringUTFChars(lib_path, nullptr);
  char ld_library_path[1024];
  snprintf(ld_library_path, sizeof(ld_library_path), "%s:/vendor/dsp/cdsp:/vendor/lib64",
           lib_path_str);
  setenv("LD_LIBRARY_PATH", ld_library_path, 1);
  char adsp_library_path[1024];
  snprintf(adsp_library_path, sizeof(adsp_library_path),
           "%s;/system/lib/rfsa/adsp;/system/vendor/lib/rfsa/adsp;/dsp", lib_path_str);
  setenv("ADSP_LIBRARY_PATH", adsp_library_path, 1);
  env->ReleaseStringUTFChars(lib_path, lib_path_str);
  const char *config_str = env->GetStringUTFChars(jconfig, nullptr);
  Context *ctx = NULL;
  try {
    ctx = new Context(config_str);
    env->ReleaseStringUTFChars(jconfig, config_str);
    return (jlong)ctx;
  } catch (const std::runtime_error &e) {
    env->ReleaseStringUTFChars(jconfig, config_str);
    env->ThrowNew(env->FindClass("java/lang/Exception"), e.what());
    return 0;
  }
}

// Context::unpack(bundlePath: String, unpackDir: String): String
extern "C" JNIEXPORT jstring JNICALL Java_com_qnnllm_Context_unpack(JNIEnv *env, jclass jthiz,
                                                                     jstring jbundle_path,
                                                                     jstring junpack_dir) {
  const char *bundle_path_str = env->GetStringUTFChars(jbundle_path, nullptr);
  const char *unpack_dir_str = env->GetStringUTFChars(junpack_dir, nullptr);
  try {
    unpackModel(bundle_path_str, unpack_dir_str);
    std::ifstream config_file(std::string(unpack_dir_str) + "/config.json");
    std::string config_str((std::istreambuf_iterator<char>(config_file)),
                           std::istreambuf_iterator<char>());
    env->ReleaseStringUTFChars(jbundle_path, bundle_path_str);
    env->ReleaseStringUTFChars(junpack_dir, unpack_dir_str);
    return env->NewStringUTF(config_str.c_str());
  } catch (const std::runtime_error &e) {
    env->ReleaseStringUTFChars(jbundle_path, bundle_path_str);
    env->ReleaseStringUTFChars(junpack_dir, unpack_dir_str);
    env->ThrowNew(env->FindClass("java/lang/Exception"), e.what());
    return NULL;
  }
}

// Context::free(ctx: Context*): void
extern "C" JNIEXPORT void JNICALL Java_com_qnnllm_Context_free(JNIEnv *env, jclass jthiz,
                                                                     jlong jcontext) {
  try {
    delete (Context *)jcontext;
  } catch (const std::runtime_error &e) {
    env->ThrowNew(env->FindClass("java/lang/Exception"), e.what());
  }
}

// Context::process(ctx: Context*, input: String): void
extern "C" JNIEXPORT void JNICALL Java_com_qnnllm_Context_process(JNIEnv *env, jclass jthiz,
                                                                     jlong jcontext,
                                                                     jstring jinput) {
  const char *input_str = env->GetStringUTFChars(jinput, nullptr);
  try {
    ((Context *)jcontext)->process(input_str);
  } catch (const std::runtime_error &e) {
    env->ReleaseStringUTFChars(jinput, input_str);
    env->ThrowNew(env->FindClass("java/lang/Exception"), e.what());
  }
  env->ReleaseStringUTFChars(jinput, input_str);
}

// Context::query(ctx: Context*, input_str: String, java_callback: Object): String
extern "C" JNIEXPORT jstring JNICALL Java_com_qnnllm_Context_query(JNIEnv *env, jclass jthiz,
                                                                         jlong jcontext,
                                                                         jstring jinput,
                                                                         jobject jcallback) {
  const char *input = env->GetStringUTFChars(jinput, nullptr);
  std::string input_str = input;
  env->ReleaseStringUTFChars(jinput, input);
  jweak weak_callback = env->NewWeakGlobalRef(jcallback);
  try {
    auto profile = ((Context *)jcontext)->query(input_str, [env, weak_callback](
      const char *response, const GenieDialog_SentenceCode_t sentenceCode) {
      jclass callback_class = env->GetObjectClass(weak_callback);
      jmethodID on_response_method = env->GetMethodID(callback_class, "onResponse", "(Ljava/lang/String;I)V");
      env->CallVoidMethod(weak_callback, on_response_method, env->NewStringUTF(response), (jint)sentenceCode);
    });
    env->DeleteWeakGlobalRef(weak_callback);
    return env->NewStringUTF(profile.c_str());
  } catch (const std::runtime_error &e) {
    env->DeleteWeakGlobalRef(weak_callback);
    env->ThrowNew(env->FindClass("java/lang/Exception"), e.what());
    return NULL;
  }
}

// Context::setStopWords(ctx: Context*, stop_words: String): void
extern "C" JNIEXPORT void JNICALL Java_com_qnnllm_Context_setStopWords(JNIEnv *env,
                                                                             jclass jthiz,
                                                                             jlong jcontext,
                                                                             jstring jstop_words) {
  const char *stop_words_str = env->GetStringUTFChars(jstop_words, nullptr);
  try {
    ((Context *)jcontext)->setStopWords(stop_words_str);
    env->ReleaseStringUTFChars(jstop_words, stop_words_str);
  } catch (const std::runtime_error &e) {
    env->ReleaseStringUTFChars(jstop_words, stop_words_str);
    env->ThrowNew(env->FindClass("java/lang/Exception"), e.what());
  }
}

// Context::applySamplerConfig(ctx: Context*, config: String): void
extern "C" JNIEXPORT void JNICALL Java_com_qnnllm_Context_applySamplerConfig(
    JNIEnv *env, jclass jthiz, jlong jcontext, jstring jconfig) {
  const char *config_str = env->GetStringUTFChars(jconfig, nullptr);
  try {
    ((Context *)jcontext)->applySamplerConfig(config_str);
    env->ReleaseStringUTFChars(jconfig, config_str);
  } catch (const std::runtime_error &e) {
    env->ReleaseStringUTFChars(jconfig, config_str);
    env->ThrowNew(env->FindClass("java/lang/Exception"), e.what());
  }
}

// Context::saveSession(ctx: Context*, filename: String): void
extern "C" JNIEXPORT void JNICALL Java_com_qnnllm_Context_saveSession(JNIEnv *env,
                                                                            jclass jthiz,
                                                                            jlong jcontext,
                                                                            jstring jfilename) {
  const char *filename_str = env->GetStringUTFChars(jfilename, nullptr);
  try {
    ((Context *)jcontext)->saveSession(filename_str);
    env->ReleaseStringUTFChars(jfilename, filename_str);
  } catch (const std::runtime_error &e) {
    env->ReleaseStringUTFChars(jfilename, filename_str);
    env->ThrowNew(env->FindClass("java/lang/Exception"), e.what());
  }
}

// Context::restoreSession(ctx: Context*, filename: String): void
extern "C" JNIEXPORT void JNICALL Java_com_qnnllm_Context_restoreSession(JNIEnv *env,
                                                                               jclass jthiz,
                                                                               jlong jcontext,
                                                                               jstring jfilename) {
  const char *filename_str = env->GetStringUTFChars(jfilename, nullptr);
  try {
    ((Context *)jcontext)->restoreSession(filename_str);
    env->ReleaseStringUTFChars(jfilename, filename_str);
  } catch (const std::runtime_error &e) {
    env->ReleaseStringUTFChars(jfilename, filename_str);
    env->ThrowNew(env->FindClass("java/lang/Exception"), e.what());
  }
}

// Context::abort(ctx: Context*): void
extern "C" JNIEXPORT void JNICALL Java_com_qnnllm_Context_abort(JNIEnv *env, jclass jthiz,
                                                                      jlong jcontext) {
  try {
    ((Context *)jcontext)->abort();
  } catch (const std::runtime_error &e) {
    env->ThrowNew(env->FindClass("java/lang/Exception"), e.what());
  }
}
