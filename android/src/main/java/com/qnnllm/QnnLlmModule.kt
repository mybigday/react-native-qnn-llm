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
  fun create(config: String, promise: Promise) {
    Thread {
      try {
        val context = Context(config)
        val id = mContextId.incrementAndGet()
        mContexts[id] = context
        promise.resolve(id)
      } catch (e: Exception) {
        promise.reject(e)
      }
    }.start()
  }

  @ReactMethod
  fun unpack(bundlePath: String, unpackDir: String, promise: Promise) {
    Thread {
      try {
        Context.unpack(bundlePath, unpackDir)
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject(e)
      }
    }.start()
  }

  @ReactMethod
  fun free(id: Long, promise: Promise) {
    Thread {
      try {
        mContexts.remove(id)?.free()
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject(e)
      }
    }.start()
  }

  @ReactMethod
  fun process(id: Long, input: String, promise: Promise) {
    Thread {
      try {
        mContexts[id]?.process(input)
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject(e)
      }
    }.start()
  }

  @ReactMethod
  fun query(id: Long, input: String, promise: Promise) {
    Thread {
      try {
        val context = mContexts[id]
        if (context == null) {
          promise.reject(Exception("Context not found"))
        }
        val profile = context.query(input, object : Context.Callback() {
          override fun onResponse(response: String, sentenceCode: Int) {
            val data = Arguments.createMap()
            data.putString("response", response)
            data.putInt("code", sentenceCode)
            data.putInt("contextId", id)
            fireEvent("onResponse", data)
          }
        })
        promise.resolve(profile)
      } catch (e: Exception) {
        promise.reject(e)
      }
    }.start()
  }

  @ReactMethod
  fun setStopWords(id: Long, stopWords: String, promise: Promise) {
    Thread {
      try {
        mContexts[id]?.setStopWords(stopWords)
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject(e)
      }
    }.start()
  }

  @ReactMethod
  fun applySamplerConfig(id: Long, config: String, promise: Promise) {
    Thread {
      try {
        mContexts[id]?.applySamplerConfig(config)
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject(e)
      }
    }.start()
  }

  @ReactMethod
  fun saveSession(id: Long, filename: String, promise: Promise) {
    Thread {
      try {
        mContexts[id]?.saveSession(filename)
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject(e)
      }
    }.start()
  }

  @ReactMethod
  fun restoreSession(id: Long, filename: String, promise: Promise) {
    Thread {
      try {
        mContexts[id]?.restoreSession(filename)
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject(e)
      }
    }.start()
  }

  @ReactMethod
  fun abort(id: Long, promise: Promise) {
    Thread {
      try {
        mContexts[id]?.abort()
        promise.resolve(null)
      } catch (e: Exception) {
        promise.reject(e)
      }
    }.start()
  }

  @ReactMethod
  fun addListener(type: String?) {}

  @ReactMethod
  fun removeListeners(type: Int?) {}

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
