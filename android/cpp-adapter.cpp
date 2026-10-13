#include "context.h"
#include "unpack.h"
#include "jni-utils.h"
#include "log.h"
#include <jni.h>
#include <fstream>
#include <cstring>

// package com.qnnllm

// Context::create(libPath: String, tmpDir: String, config: String, dialogType: String, defaults: QueryParams): Context*
extern "C" JNIEXPORT jlong JNICALL Java_com_qnnllm_Context_create(JNIEnv *env, jclass jthiz,
                                                                        jstring lib_path,
                                                                        jstring jtmp_dir,
                                                                        jstring jconfig,
                                                                        jstring jdialog_type,
                                                                        jobject jdefaults) {
  std::string lib_path_str = getString(env, lib_path);
  char ld_library_path[1024];
  snprintf(ld_library_path, sizeof(ld_library_path), "%s:/vendor/dsp/cdsp:/vendor/lib64",
           lib_path_str.c_str());
  setenv("LD_LIBRARY_PATH", ld_library_path, 1);
  char adsp_library_path[1024];
  snprintf(adsp_library_path, sizeof(adsp_library_path),
           "%s;/system/lib/rfsa/adsp;/system/vendor/lib/rfsa/adsp;/dsp", lib_path_str.c_str());
  setenv("ADSP_LIBRARY_PATH", adsp_library_path, 1);
  LOGI("QNN libGenie version: %s", qnnllm::Context::version().c_str());
  std::string config_str = getString(env, jconfig);
  try {
    auto ctx = new qnnllm::Context(config_str.c_str(), toQueryParams(env, jdefaults),
                                   getString(env, jdialog_type), getString(env, jtmp_dir));
    return (jlong)ctx;
  } catch (const std::runtime_error &e) {
    throwException(env, e.what());
    return 0;
  }
}

// Context::nativeUnpack(bundlePath: String, unpackDir: String): String
extern "C" JNIEXPORT jstring JNICALL Java_com_qnnllm_Context_nativeUnpack(JNIEnv *env, jclass cls,
                                                                     jstring jbundle_path,
                                                                     jstring junpack_dir) {
  std::string bundle_path_str = getString(env, jbundle_path);
  std::string unpack_dir_str = getString(env, junpack_dir);
  try {
    unpackModel(bundle_path_str.c_str(), unpack_dir_str.c_str());
    std::ifstream config_file(unpack_dir_str + "/config.json");
    std::string config_str((std::istreambuf_iterator<char>(config_file)),
                           std::istreambuf_iterator<char>());
    return toJString(env, config_str);
  } catch (const std::runtime_error &e) {
    throwException(env, e.what());
    return NULL;
  }
}

// Context::free(ctx: Context*): void
extern "C" JNIEXPORT void JNICALL Java_com_qnnllm_Context_free(JNIEnv *env, jclass jthiz,
                                                                     jlong jcontext) {
  auto ctx = (qnnllm::Context *)jcontext;
  try {
    // aborts and waits for an in-flight query()/process() before freeing
    ctx->release();
  } catch (const std::runtime_error &e) {
    throwException(env, e.what());
  }
  delete ctx;
}

// Context::process(ctx: Context*, input: String): void
extern "C" JNIEXPORT void JNICALL Java_com_qnnllm_Context_process(JNIEnv *env, jclass jthiz,
                                                                     jlong jcontext,
                                                                     jstring jinput) {
  try {
    ((qnnllm::Context *)jcontext)->process(getString(env, jinput));
  } catch (const std::runtime_error &e) {
    throwException(env, e.what());
  }
}

// Context::query(ctx: Context*, params: QueryParams, callback: Callback): QueryResult
extern "C" JNIEXPORT jobject JNICALL Java_com_qnnllm_Context_query(JNIEnv *env, jclass jthiz,
                                                                         jlong jcontext,
                                                                         jobject jparams,
                                                                         jobject jcallback) {
  qnnllm::QueryParams params = toQueryParams(env, jparams);
  // Genie calls back on this thread while GenieDialog_query is running
  jclass callback_class = env->GetObjectClass(jcallback);
  jmethodID on_response_method =
      env->GetMethodID(callback_class, "onResponse", "(Ljava/lang/String;I)V");
  env->DeleteLocalRef(callback_class);
  try {
    auto result = ((qnnllm::Context *)jcontext)->query(params, [env, jcallback, on_response_method](
      const char *response, const GenieDialog_SentenceCode_t sentenceCode) {
      // free per-token local refs, a long generation would overflow the local ref table
      jstring jresponse = response ? toJString(env, response, strlen(response)) : nullptr;
      env->CallVoidMethod(jcallback, on_response_method, jresponse, (jint)sentenceCode);
      if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
      }
      if (jresponse) env->DeleteLocalRef(jresponse);
    });
    jclass result_class = env->FindClass("com/qnnllm/QueryResult");
    jmethodID ctor =
        env->GetMethodID(result_class, "<init>", "(Ljava/lang/String;Ljava/lang/String;ZJ)V");
    jstring jtext = toJString(env, result.text);
    jstring jprofile = toJString(env, result.profile_json);
    jobject jresult = env->NewObject(result_class, ctor, jtext, jprofile,
                                     (jboolean)result.interrupted, (jlong)result.num_generated);
    env->DeleteLocalRef(jprofile);
    env->DeleteLocalRef(jtext);
    env->DeleteLocalRef(result_class);
    return jresult;
  } catch (const std::runtime_error &e) {
    throwException(env, e.what());
    return NULL;
  }
}

// Context::saveSession(ctx: Context*, filename: String): void
extern "C" JNIEXPORT void JNICALL Java_com_qnnllm_Context_saveSession(JNIEnv *env, jclass jthiz,
                                                                  jlong jcontext,
                                                                  jstring jfilename) {
  try {
    ((qnnllm::Context *)jcontext)->saveSession(getString(env, jfilename).c_str());
  } catch (const std::runtime_error &e) {
    throwException(env, e.what());
  }
}

// Context::restoreSession(ctx: Context*, filename: String): void
extern "C" JNIEXPORT void JNICALL Java_com_qnnllm_Context_restoreSession(JNIEnv *env, jclass jthiz,
                                                                  jlong jcontext,
                                                                  jstring jfilename) {
  try {
    ((qnnllm::Context *)jcontext)->restoreSession(getString(env, jfilename).c_str());
  } catch (const std::runtime_error &e) {
    throwException(env, e.what());
  }
}

// Context::abort(ctx: Context*): void
extern "C" JNIEXPORT void JNICALL Java_com_qnnllm_Context_abort(JNIEnv *env, jclass jthiz,
                                                                      jlong jcontext) {
  try {
    ((qnnllm::Context *)jcontext)->abort();
  } catch (const std::runtime_error &e) {
    throwException(env, e.what());
  }
}

// Context::applyLora(ctx: Context*, engine: String, loraAdapterName: String): void
extern "C" JNIEXPORT void JNICALL Java_com_qnnllm_Context_applyLora(JNIEnv *env, jclass jthiz,
                                                                          jlong jcontext,
                                                                          jstring jengine,
                                                                          jstring jname) {
  try {
    ((qnnllm::Context *)jcontext)->applyLora(getString(env, jengine), getString(env, jname));
  } catch (const std::runtime_error &e) {
    throwException(env, e.what());
  }
}

// Context::setLoraStrength(ctx: Context*, engine: String, tensorNames: Array<String>, alphas: FloatArray): void
extern "C" JNIEXPORT void JNICALL Java_com_qnnllm_Context_setLoraStrength(JNIEnv *env,
                                                                                jclass jthiz,
                                                                                jlong jcontext,
                                                                                jstring jengine,
                                                                                jobjectArray jnames,
                                                                                jfloatArray jalphas) {
  qnnllm::LoraStrengthMap lora_strength_map;
  jsize length = env->GetArrayLength(jnames);
  jfloat *alphas = env->GetFloatArrayElements(jalphas, nullptr);
  for (jsize i = 0; i < length; i++) {
    jstring jname = (jstring)env->GetObjectArrayElement(jnames, i);
    lora_strength_map[getString(env, jname)] = alphas[i];
    env->DeleteLocalRef(jname);
  }
  env->ReleaseFloatArrayElements(jalphas, alphas, JNI_ABORT);
  try {
    ((qnnllm::Context *)jcontext)->setLoraStrength(getString(env, jengine), lora_strength_map);
  } catch (const std::runtime_error &e) {
    throwException(env, e.what());
  }
}

// Context::releaseLoraMemory(ctx: Context*, engine: String, loraAdapterName: String): void
extern "C" JNIEXPORT void JNICALL Java_com_qnnllm_Context_releaseLoraMemory(JNIEnv *env,
                                                                                  jclass jthiz,
                                                                                  jlong jcontext,
                                                                                  jstring jengine,
                                                                                  jstring jname) {
  try {
    ((qnnllm::Context *)jcontext)->releaseLoraMemory(getString(env, jengine), getString(env, jname));
  } catch (const std::runtime_error &e) {
    throwException(env, e.what());
  }
}

// Context::getContextOccupancy(ctx: Context*): Long
extern "C" JNIEXPORT jlong JNICALL Java_com_qnnllm_Context_getContextOccupancy(JNIEnv *env,
                                                                                     jclass jthiz,
                                                                                     jlong jcontext) {
  try {
    return (jlong)((qnnllm::Context *)jcontext)->getContextOccupancy();
  } catch (const std::runtime_error &e) {
    throwException(env, e.what());
    return 0;
  }
}

// Context::getAppliedLoraAdapter(ctx: Context*): String
extern "C" JNIEXPORT jstring JNICALL Java_com_qnnllm_Context_getAppliedLoraAdapter(JNIEnv *env,
                                                                                         jclass jthiz,
                                                                                         jlong jcontext) {
  try {
    return toJString(env, ((qnnllm::Context *)jcontext)->getAppliedLoraAdapter());
  } catch (const std::runtime_error &e) {
    throwException(env, e.what());
    return NULL;
  }
}
