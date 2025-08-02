package com.qnnllm

import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.ReactMethod
import com.facebook.react.bridge.Promise
import com.facebook.react.bridge.Arguments
import com.facebook.react.bridge.WritableMap
import com.facebook.react.modules.core.DeviceEventManagerModule

import java.util.concurrent.atomic.AtomicLong
import java.io.File

class QnnLlmModule internal constructor(context: ReactApplicationContext) :
  QnnLlmSpec(context) {

  override fun getName(): String {
    return NAME
  }

  private val mContexts = mutableMapOf<Long, Context>()
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

  @ReactMethod
  override fun create(config: String, promise: Promise) {
    Thread {
      try {
        val context = Context(reactApplicationContext, config)
        val id = mContextId.incrementAndGet()
        mContexts[id] = context
        promise.resolve(id)
      } catch (e: Exception) {
        promise.reject("E_CREATE_CONTEXT", e.message, e)
      }
    }.start()
  }

  @ReactMethod
  override fun unpack(bundlePath: String, unpackDir: String, promise: Promise) {
    Thread {
      try {
        Context.unpack(bundlePath, unpackDir)
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject("E_UNPACK", e.message, e)
      }
    }.start()
  }

  @ReactMethod
  override fun free(id: Double, promise: Promise) {
    Thread {
      try {
        mContexts.remove(id.toLong())?.release()
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject("E_FREE", e.message, e)
      }
    }.start()
  }

  @ReactMethod
  override fun process(id: Double, input: String, promise: Promise) {
    Thread {
      try {
        mContexts[id.toLong()]?.process(input)
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject("E_PROCESS", e.message, e)
      }
    }.start()
  }

  @ReactMethod
  override fun query(id: Double, input: String, promise: Promise) {
    Thread {
      try {
        val context = mContexts[id.toLong()]
        if (context == null) {
          promise.reject(Exception("Context not found"))
        }
        val profile = context?.query(input, object : Context.Callback() {
          override fun onResponse(response: String, sentenceCode: Int) {
            val data = Arguments.createMap()
            data.putString("response", response)
            data.putInt("code", sentenceCode)
            data.putInt("contextId", id.toInt())
            fireEvent("onResponse", data)
          }
        })
        promise.resolve(profile)
      } catch (e: Exception) {
        promise.reject("E_QUERY", e.message, e)
      }
    }.start()
  }

  @ReactMethod
  override fun setStopWords(id: Double, stopWords: String, promise: Promise) {
    Thread {
      try {
        mContexts[id.toLong()]?.setStopWords(stopWords)
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject("E_SET_STOP_WORDS", e.message, e)
      }
    }.start()
  }

  @ReactMethod
  override fun applySamplerConfig(id: Double, config: String, promise: Promise) {
    Thread {
      try {
        mContexts[id.toLong()]?.applySamplerConfig(config)
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject("E_APPLY_SAMPLER_CONFIG", e.message, e)
      }
    }.start()
  }

  @ReactMethod
  override fun saveSession(id: Double, filename: String, promise: Promise) {
    Thread {
      try {
        mContexts[id.toLong()]?.saveSession(filename)
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject("E_SAVE_SESSION", e.message, e)
      }
    }.start()
  }

  @ReactMethod
  override fun restoreSession(id: Double, filename: String, promise: Promise) {
    Thread {
      try {
        mContexts[id.toLong()]?.restoreSession(filename)
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject("E_RESTORE_SESSION", e.message, e)
      }
    }.start()
  }

  @ReactMethod
  override fun abort(id: Double, promise: Promise) {
    Thread {
      try {
        mContexts[id.toLong()]?.abort()
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject("E_ABORT", e.message, e)
      }
    }.start()
  }

  @ReactMethod
  fun addListener(type: String) {}

  @ReactMethod
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
