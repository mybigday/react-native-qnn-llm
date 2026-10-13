package com.qnnllm

import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.module.annotations.ReactModule
import com.facebook.react.bridge.Promise
import com.facebook.react.bridge.Arguments
import com.facebook.react.bridge.ReadableMap
import com.facebook.react.bridge.WritableMap
import com.facebook.react.modules.core.DeviceEventManagerModule

import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicLong
import java.io.File

@ReactModule(name = QnnLlmModule.NAME)
class QnnLlmModule(reactContext: ReactApplicationContext) :
  NativeQnnLlmSpec(reactContext) {

  override fun getName(): String {
    return NAME
  }

  private val mContexts = ConcurrentHashMap<Long, Context>()
  private val mContextId = AtomicLong(0)

  val mHtpConfigFilePath: String

  init {
    // generate htp config file
    val configFile = File.createTempFile("htp_config", ".json").also {
      it.writeText(Constants.HTP_CONFIG)
    }
    mHtpConfigFilePath = configFile.path
    configFile.deleteOnExit()
  }

  override fun createContext(
    config: String,
    dialogType: String,
    defaults: ReadableMap,
    promise: Promise
  ) {
    Thread {
      try {
        val context = Context.create(
          reactApplicationContext,
          config,
          dialogType,
          QueryParams.fromMap(defaults)
        )
        val id = mContextId.incrementAndGet()
        mContexts[id] = context
        promise.resolve(id.toDouble())
      } catch (e: Exception) {
        promise.reject("E_CREATE_CONTEXT", e.message, e)
      }
    }.start()
  }

  override fun unpack(bundlePath: String, unpackDir: String, promise: Promise) {
    Thread {
      try {
        promise.resolve(Context.unpack(bundlePath, unpackDir))
      } catch (e: Exception) {
        promise.reject("E_UNPACK", e.message, e)
      }
    }.start()
  }

  override fun exists(path: String, promise: Promise) {
    promise.resolve(File(path).exists())
  }

  override fun freeContext(id: Double, promise: Promise) {
    Thread {
      try {
        mContexts.remove(id.toLong())?.release()
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject("E_FREE", e.message, e)
      }
    }.start()
  }

  // Runs `block` on a new thread with the context, rejecting if it's not found
  private fun withContext(id: Double, code: String, promise: Promise, block: (Context) -> Any?) {
    Thread {
      try {
        val context = mContexts[id.toLong()]
        if (context == null) {
          promise.reject(code, "Context not found")
          return@Thread
        }
        promise.resolve(block(context))
      } catch (e: Exception) {
        promise.reject(code, e.message, e)
      }
    }.start()
  }

  override fun process(id: Double, input: String, promise: Promise) {
    withContext(id, "E_PROCESS", promise) { it.process(input); null }
  }

  override fun query(id: Double, params: ReadableMap, promise: Promise) {
    withContext(id, "E_QUERY", promise) { context ->
      val result = context.query(QueryParams.fromMap(params), object : Context.Callback() {
        override fun onResponse(response: String?, sentenceCode: Int) {
          val data = Arguments.createMap()
          data.putString("response", response ?: "")
          data.putInt("sentenceCode", sentenceCode)
          data.putInt("contextId", id.toInt())
          fireEvent("onResponse", data)
        }
      })
      Arguments.createMap().apply {
        putString("text", result.text)
        putString("profile", result.profile)
        putBoolean("interrupted", result.interrupted)
        putDouble("numGenerated", result.numGenerated.toDouble())
      }
    }
  }

  override fun saveSession(id: Double, filename: String, promise: Promise) {
    withContext(id, "E_SAVE_SESSION", promise) { it.saveSession(filename); null }
  }

  override fun restoreSession(id: Double, filename: String, promise: Promise) {
    withContext(id, "E_RESTORE_SESSION", promise) { it.restoreSession(filename); null }
  }

  override fun abort(id: Double, promise: Promise) {
    withContext(id, "E_ABORT", promise) { it.abort(); null }
  }

  override fun applyLora(id: Double, engine: String, loraAdapterName: String, promise: Promise) {
    withContext(id, "E_APPLY_LORA", promise) { it.applyLora(engine, loraAdapterName); null }
  }

  override fun setLoraStrength(id: Double, engine: String, strengths: ReadableMap, promise: Promise) {
    withContext(id, "E_SET_LORA_STRENGTH", promise) { context ->
      val map = strengths.toHashMap().mapValues { (_, v) -> (v as Number).toFloat() }
      context.setLoraStrength(engine, map)
      null
    }
  }

  override fun releaseLoraMemory(id: Double, engine: String, loraAdapterName: String, promise: Promise) {
    withContext(id, "E_RELEASE_LORA_MEMORY", promise) {
      it.releaseLoraMemory(engine, loraAdapterName)
      null
    }
  }

  override fun getContextOccupancy(id: Double, promise: Promise) {
    withContext(id, "E_GET_CONTEXT_OCCUPANCY", promise) { it.getContextOccupancy().toDouble() }
  }

  override fun getAppliedLoraAdapter(id: Double, promise: Promise) {
    withContext(id, "E_GET_APPLIED_LORA_ADAPTER", promise) { it.getAppliedLoraAdapter() }
  }

  fun addListener(type: String) {}

  fun removeListeners(count: Int) {}

  private fun fireEvent(eventName: String, data: WritableMap) {
    reactApplicationContext.getJSModule(DeviceEventManagerModule.RCTDeviceEventEmitter::class.java)
      .emit(eventName, data)
  }

  override fun getConstants(): MutableMap<String, Any> =
    hashMapOf("HTP_CONFIG_FILE_PATH" to mHtpConfigFilePath)

  companion object {
    const val NAME = "QnnLlm"
  }
}
