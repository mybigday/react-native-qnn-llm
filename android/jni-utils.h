#pragma once

#include "context.h"
#include <jni.h>
#include <string>

// Java String -> standard UTF-8
std::string getString(JNIEnv *env, jstring jstr);

std::string getStringField(JNIEnv *env, jobject obj, jclass cls, const char *name);

// standard UTF-8 -> Java String
jstring toJString(JNIEnv *env, const char *str, size_t len);

jstring toJString(JNIEnv *env, const std::string &str);

// Reads a com.qnnllm.QueryParams object
qnnllm::QueryParams toQueryParams(JNIEnv *env, jobject jparams);

void throwException(JNIEnv *env, const char *message);
