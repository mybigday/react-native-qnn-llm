#include "jni-utils.h"
#include <cstdint>

// GetStringUTFChars returns modified UTF-8 (surrogate pairs for emoji etc.),
// so encode through String.getBytes("UTF-8") to get standard UTF-8.
std::string getString(JNIEnv *env, jstring jstr) {
  if (jstr == nullptr) return "";
  jclass string_class = env->GetObjectClass(jstr);
  jmethodID get_bytes = env->GetMethodID(string_class, "getBytes", "(Ljava/lang/String;)[B");
  jstring charset = env->NewStringUTF("UTF-8");
  jbyteArray bytes = (jbyteArray)env->CallObjectMethod(jstr, get_bytes, charset);
  jsize length = env->GetArrayLength(bytes);
  std::string str(length, '\0');
  env->GetByteArrayRegion(bytes, 0, length, (jbyte *)str.data());
  env->DeleteLocalRef(bytes);
  env->DeleteLocalRef(charset);
  env->DeleteLocalRef(string_class);
  return str;
}

std::string getStringField(JNIEnv *env, jobject obj, jclass cls, const char *name) {
  jstring jstr = (jstring)env->GetObjectField(obj, env->GetFieldID(cls, name, "Ljava/lang/String;"));
  std::string str = getString(env, jstr);
  env->DeleteLocalRef(jstr);
  return str;
}

// NewStringUTF expects modified UTF-8 and rejects 4-byte sequences (e.g. emoji),
// so decode standard UTF-8 through String(byte[], "UTF-8") instead.
jstring toJString(JNIEnv *env, const char *str, size_t len) {
  jbyteArray bytes = env->NewByteArray((jsize)len);
  env->SetByteArrayRegion(bytes, 0, (jsize)len, (const jbyte *)str);
  jclass string_class = env->FindClass("java/lang/String");
  jmethodID ctor = env->GetMethodID(string_class, "<init>", "([BLjava/lang/String;)V");
  jstring charset = env->NewStringUTF("UTF-8");
  jstring result = (jstring)env->NewObject(string_class, ctor, bytes, charset);
  env->DeleteLocalRef(charset);
  env->DeleteLocalRef(string_class);
  env->DeleteLocalRef(bytes);
  return result;
}

jstring toJString(JNIEnv *env, const std::string &str) {
  return toJString(env, str.data(), str.size());
}

// Reads a com.qnnllm.QueryParams object
qnnllm::QueryParams toQueryParams(JNIEnv *env, jobject jparams) {
  qnnllm::QueryParams params;
  jclass cls = env->GetObjectClass(jparams);
  params.prompt = getStringField(env, jparams, cls, "prompt");
  params.stop_json = getStringField(env, jparams, cls, "stopJson");
  // Like llama.rn: a negative n_predict means no limit.
  jlong max_tokens = env->GetLongField(jparams, env->GetFieldID(cls, "maxTokens", "J"));
  params.max_tokens = max_tokens < 0 || max_tokens >= UINT32_MAX ? UINT32_MAX : (uint32_t)max_tokens;
  params.sampler.temperature = env->GetFloatField(jparams, env->GetFieldID(cls, "temperature", "F"));
  params.sampler.top_k = env->GetIntField(jparams, env->GetFieldID(cls, "topK", "I"));
  params.sampler.top_p = env->GetFloatField(jparams, env->GetFieldID(cls, "topP", "F"));
  params.sampler.seed = env->GetIntField(jparams, env->GetFieldID(cls, "seed", "I"));
  params.grammar.kind = getStringField(env, jparams, cls, "grammarKind");
  params.grammar.content = getStringField(env, jparams, cls, "grammarContent");
  params.grammar.file = getStringField(env, jparams, cls, "grammarFile");
  env->DeleteLocalRef(cls);
  return params;
}

void throwException(JNIEnv *env, const char *message) {
  jclass exception_class = env->FindClass("java/lang/Exception");
  env->ThrowNew(exception_class, message);
  env->DeleteLocalRef(exception_class);
}
