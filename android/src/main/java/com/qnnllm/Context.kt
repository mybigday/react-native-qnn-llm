package com.qnnllm

import android.os.Build
import android.util.Log

class Context constructor(libPath: String, config: String) {
  private val mContextPtr: Long

  abstract class Callback {
    abstract fun onResponse(response: String, sentenceCode: Int)
  }

  external fun getVersion(): String
  external fun create(libPath: String, config: String): Long
  external fun unpack(bundlePath: String, unpackDir: String): String
  external fun free(contextPtr: Long)
  external fun process(contextPtr: Long, input: String)
  external fun query(contextPtr: Long, input: String, callback: Callback): String
  external fun setStopWords(contextPtr: Long, stopWords: String)
  external fun applySamplerConfig(contextPtr: Long, config: String)
  external fun saveSession(contextPtr: Long, filename: String)
  external fun restoreSession(contextPtr: Long, filename: String)
  external fun abort(contextPtr: Long)

  init {
    if (!Build.SUPPORTED_ABIS.contains("arm64-v8a")) {
      Log.e("QnnLlm", "Not supported for ${Build.SUPPORTED_ABIS}")
      throw RuntimeException("QNN LLM is not supported on this device")
    } else {
      System.loadLibrary("qnn-llm")
    }
    Log.i("QnnLlm", "API Version: v${getVersion()}")
    this.mContextPtr = create(libPath, config)
  }

  fun process(input: String) {
    process(mContextPtr, input)
  }

  fun setStopWords(stopWords: String) {
    setStopWords(mContextPtr, stopWords)
  }

  fun applySamplerConfig(config: String) {
    applySamplerConfig(mContextPtr, config)
  }

  fun saveSession(filename: String) {
    saveSession(mContextPtr, filename)
  }

  fun restoreSession(filename: String) {
    restoreSession(mContextPtr, filename)
  }

  fun query(input: String, callback: Callback): String {
    return query(mContextPtr, input, callback)
  }

  fun abort() {
    abort(mContextPtr)
  }

  fun release() {
    free(mContextPtr)
  }
}
