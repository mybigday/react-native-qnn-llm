package com.qnnllm

import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.ReactContextBaseJavaModule
import com.facebook.react.bridge.Promise

abstract class QnnLlmSpec internal constructor(context: ReactApplicationContext) :
  ReactContextBaseJavaModule(context) {

  abstract fun create(config: String, promise: Promise)
  abstract fun unpack(bundlePath: String, unpackDir: String, promise: Promise)
  abstract fun free(id: Double, promise: Promise)
  abstract fun process(id: Double, input: String, promise: Promise)
  abstract fun query(id: Double, input: String, promise: Promise)
  abstract fun setStopWords(id: Double, stopWords: String, promise: Promise)
  abstract fun applySamplerConfig(id: Double, config: String, promise: Promise)
  abstract fun saveSession(id: Double, filename: String, promise: Promise)
  abstract fun restoreSession(id: Double, filename: String, promise: Promise)
  abstract fun abort(id: Double, promise: Promise)
}
