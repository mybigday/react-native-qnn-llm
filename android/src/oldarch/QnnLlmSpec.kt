package com.qnnllm

import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.ReactContextBaseJavaModule
import com.facebook.react.bridge.Promise

abstract class QnnLlmSpec internal constructor(context: ReactApplicationContext) :
  ReactContextBaseJavaModule(context) {

  abstract fun create(config: String, promise: Promise)
  abstract fun unpack(bundlePath: String, unpackDir: String, promise: Promise)
  abstract fun free(id: Long, promise: Promise)
  abstract fun process(id: Long, input: String, promise: Promise)
  abstract fun query(id: Long, input: String, promise: Promise)
  abstract fun setStopWords(id: Long, stopWords: String, promise: Promise)
  abstract fun applySamplerConfig(id: Long, config: String, promise: Promise)
  abstract fun saveSession(id: Long, filename: String, promise: Promise)
  abstract fun restoreSession(id: Long, filename: String, promise: Promise)
  abstract fun abort(id: Long, promise: Promise)

  abstract fun addListener(type: String?)
  abstract fun removeListeners(type: Int?)
}
