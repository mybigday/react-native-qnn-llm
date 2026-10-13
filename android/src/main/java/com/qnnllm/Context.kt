package com.qnnllm

import android.os.Build
import android.util.Log
import com.facebook.react.bridge.ReadableMap
import java.util.concurrent.locks.ReentrantReadWriteLock
import kotlin.concurrent.read
import kotlin.concurrent.write
import android.content.Context as AndroidContext

// Fully resolved per-query params, read by the native side through its fields
class QueryParams(
  @JvmField val prompt: String,
  @JvmField val stopJson: String,
  @JvmField val maxTokens: Long,
  @JvmField val temperature: Float,
  @JvmField val topK: Int,
  @JvmField val topP: Float,
  @JvmField val seed: Int,
  @JvmField val grammarKind: String,
  @JvmField val grammarContent: String,
  @JvmField val grammarFile: String,
) {
  companion object {
    @JvmStatic
    fun fromMap(map: ReadableMap) = QueryParams(
      prompt = map.getString("prompt") ?: "",
      stopJson = map.getString("stopJson") ?: "{}",
      maxTokens = map.getDouble("maxTokens").toLong(),
      temperature = map.getDouble("temperature").toFloat(),
      topK = map.getInt("topK"),
      topP = map.getDouble("topP").toFloat(),
      seed = map.getDouble("seed").toInt(),
      grammarKind = map.getString("grammarKind") ?: "",
      grammarContent = map.getString("grammarContent") ?: "",
      grammarFile = map.getString("grammarFile") ?: "",
    )
  }
}

class QueryResult(
  val text: String,
  val profile: String,
  val interrupted: Boolean,
  val numGenerated: Long,
)

class Context constructor(
  context: AndroidContext,
  config: String,
  dialogType: String,
  defaults: QueryParams,
) {
  private val mLibPath: String = context.applicationInfo.nativeLibraryDir
  private val mContextPtr: Long
  // Calls hold the read lock so release() can't free the native context under them
  private val mLock = ReentrantReadWriteLock()
  private var mReleased = false

  abstract class Callback {
    abstract fun onResponse(response: String?, sentenceCode: Int)
  }

  external fun create(
    libPath: String,
    tmpDir: String,
    config: String,
    dialogType: String,
    defaults: QueryParams,
  ): Long
  external fun free(contextPtr: Long)
  external fun process(contextPtr: Long, input: String)
  external fun query(contextPtr: Long, params: QueryParams, callback: Callback): QueryResult
  external fun saveSession(contextPtr: Long, filename: String)
  external fun restoreSession(contextPtr: Long, filename: String)
  external fun abort(contextPtr: Long)
  external fun applyLora(contextPtr: Long, engine: String, loraAdapterName: String)
  external fun setLoraStrength(
    contextPtr: Long,
    engine: String,
    tensorNames: Array<String>,
    alphas: FloatArray,
  )
  external fun releaseLoraMemory(contextPtr: Long, engine: String, loraAdapterName: String)
  external fun getContextOccupancy(contextPtr: Long): Long
  external fun getAppliedLoraAdapter(contextPtr: Long): String

  init {
    this.mContextPtr = create(mLibPath, context.cacheDir.path, config, dialogType, defaults)
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
    fun create(
      context: AndroidContext,
      config: String,
      dialogType: String,
      defaults: QueryParams,
    ): Context {
      load()
      return Context(context, config, dialogType, defaults)
    }

    @JvmStatic
    fun unpack(bundlePath: String, unpackDir: String): String {
      load()
      return nativeUnpack(bundlePath, unpackDir)
    }
  }

  fun process(input: String) {
    withContext { process(mContextPtr, input) }
  }

  fun saveSession(filename: String) {
    withContext { saveSession(mContextPtr, filename) }
  }

  fun restoreSession(filename: String) {
    withContext { restoreSession(mContextPtr, filename) }
  }

  fun query(params: QueryParams, callback: Callback): QueryResult {
    return withContext { query(mContextPtr, params, callback) }
  }

  fun abort() {
    withContext { abort(mContextPtr) }
  }

  fun applyLora(engine: String, loraAdapterName: String) {
    withContext { applyLora(mContextPtr, engine, loraAdapterName) }
  }

  fun setLoraStrength(engine: String, strengths: Map<String, Float>) {
    withContext {
      setLoraStrength(
        mContextPtr,
        engine,
        strengths.keys.toTypedArray(),
        strengths.values.toFloatArray(),
      )
    }
  }

  fun releaseLoraMemory(engine: String, loraAdapterName: String) {
    withContext { releaseLoraMemory(mContextPtr, engine, loraAdapterName) }
  }

  fun getContextOccupancy(): Long {
    return withContext { getContextOccupancy(mContextPtr) }
  }

  fun getAppliedLoraAdapter(): String {
    return withContext { getAppliedLoraAdapter(mContextPtr) }
  }

  private fun <T> withContext(block: () -> T): T = mLock.read {
    if (mReleased) throw Exception("Context is released")
    block()
  }

  fun release() {
    if (!mLock.writeLock().tryLock()) {
      // a query may be running, abort it so the write lock is not held up
      try {
        abort()
      } catch (e: Exception) {
        Log.w("QnnLlm", "Failed to abort before release", e)
      }
      mLock.writeLock().lock()
    }
    try {
      if (mReleased) return
      mReleased = true
      free(mContextPtr)
    } finally {
      mLock.writeLock().unlock()
    }
  }
}
