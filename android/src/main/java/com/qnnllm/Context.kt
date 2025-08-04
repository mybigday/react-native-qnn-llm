package com.qnnllm

import android.os.Build
import android.util.Log
import android.content.Context as AndroidContext

class Context constructor(context: AndroidContext, config: String) {
  private val mLibPath: String = context.applicationInfo.nativeLibraryDir
  private val mContextPtr: Long

  abstract class Callback {
    abstract fun onResponse(response: String, sentenceCode: Int)
  }

  external fun create(libPath: String, config: String): Long
  external fun free(contextPtr: Long)
  external fun process(contextPtr: Long, input: String)
  external fun query(contextPtr: Long, input: String, callback: Callback): String
  external fun setStopWords(contextPtr: Long, stopWords: String)
  external fun applySamplerConfig(contextPtr: Long, config: String)
  external fun saveSession(contextPtr: Long, filename: String)
  external fun restoreSession(contextPtr: Long, filename: String)
  external fun abort(contextPtr: Long)

  init {
    this.mContextPtr = create(mLibPath, config)
  }

  companion object {
    @JvmStatic
    external fun nativeUnpack(bundlePath: String, unpackDir: String): String

    @JvmStatic
    fun load() {
      if (!Build.SUPPORTED_ABIS.contains("arm64-v8a")) {
        Log.e("QnnLlm", "Not supported for ${Build.SUPPORTED_ABIS}")
        throw RuntimeException("QNN LLM is not supported on this device")
      } else {
        System.loadLibrary("qnn-llm")
      }
    }

    @JvmStatic
    fun create(context: AndroidContext, config: String): Context {
      load()
      return Context(context, config)
    }

    @JvmStatic
    fun unpack(bundlePath: String, unpackDir: String): String {
      load()
      return nativeUnpack(bundlePath, unpackDir)
    }
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
