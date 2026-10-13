#include "context.h"
#include "log.h"
#include "utils.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

namespace qnnllm {

namespace {
// Clears `busy` on every exit path (normal return or exception unwinding) so
// a concurrent release() waiting on this flag is never left spinning forever,
// and so `busy` stays true for the entire duration a query()/process() call
// keeps touching the dialog/profile handles.
class BusyGuard {
public:
  explicit BusyGuard(std::atomic<bool> &flag) : flag_(flag) { flag_ = true; }
  ~BusyGuard() { flag_ = false; }
  BusyGuard(const BusyGuard &) = delete;
  BusyGuard &operator=(const BusyGuard &) = delete;

private:
  std::atomic<bool> &flag_;
};
}  // namespace

void logStdoutCallback(GenieLog_Handle_t handle, const char* fmt, GenieLog_Level_t level, uint64_t timestamp, va_list argp) {
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), fmt, argp);
  switch (level) {
    case GENIE_LOG_LEVEL_ERROR: LOGE("%s", buffer); break;
    case GENIE_LOG_LEVEL_WARN: LOGW("%s", buffer); break;
    case GENIE_LOG_LEVEL_INFO: LOGI("%s", buffer); break;
    case GENIE_LOG_LEVEL_VERBOSE: LOGD("%s", buffer); break;
  }
}


Context::Context(const char *config_str, QueryParams defaults, std::string dialog_type,
                 std::string tmp_dir)
    : dialog_type(std::move(dialog_type)), tmp_dir(std::move(tmp_dir)) {
  applied_stop_json = defaults.stop_json;
  applied_max_tokens = defaults.max_tokens;
  applied_sampler = defaults.sampler;
  applied_grammar = defaults.grammar;
  busy = false;
  Genie_Status_t status;
  GenieLog_create((GenieLogConfig_Handle_t)NULL, logStdoutCallback, QNN_LOG_LEVEL, &logHandle);
  status = GenieProfile_create(NULL, &profileHandle);
  if (status == GENIE_STATUS_SUCCESS) {
    status = GenieDialogConfig_createFromJson(config_str, &configHandle);
  }
  if (status == GENIE_STATUS_SUCCESS) {
    status = GenieDialogConfig_bindProfiler(configHandle, profileHandle);
  }
  if (status == GENIE_STATUS_SUCCESS) {
    status = GenieDialog_create(configHandle, &handle);
  }
  if (status != GENIE_STATUS_SUCCESS) {
    // the destructor does not run when the constructor throws
    release();
    throw std::runtime_error(genie_status_to_string(status));
  }
}

Context::~Context() {
  try {
    release();
  } catch (const std::runtime_error &e) {
    LOGE("Failed to release context: %s", e.what());
  }
}

void Context::release() {
  if (busy) {
    Genie_Status_t status = GenieDialog_signal(handle, GENIE_DIALOG_ACTION_ABORT);
    if (status != GENIE_STATUS_SUCCESS) {
      throw std::runtime_error(genie_status_to_string(status));
    }
    // Wait for the in-flight query()/process() call to actually return from
    // GenieDialog_query before freeing the dialog out from under it. That call
    // clears `busy` itself (BusyGuard) once it no longer touches the handles.
    while (busy) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  if (handle != NULL) {
    Genie_Status_t status = GenieDialog_free(handle);
    if (status != GENIE_STATUS_SUCCESS) {
      throw std::runtime_error(genie_status_to_string(status));
    }
    handle = NULL;
  }
  if (configHandle != NULL) {
    if (GenieDialogConfig_free(configHandle) != GENIE_STATUS_SUCCESS) {
      LOGE("Failed to free GenieDialogConfig handle");
    }
    configHandle = NULL;
  }
  if (profileHandle != NULL) {
    if (GenieProfile_free(profileHandle) != GENIE_STATUS_SUCCESS) {
      LOGE("Failed to free GenieProfile handle");
    }
    profileHandle = NULL;
  }
  if (logHandle != NULL) {
    if (GenieLog_free(logHandle) != GENIE_STATUS_SUCCESS) {
      LOGE("Failed to free GenieLog handle");
    }
    logHandle = NULL;
  }
  callback = nullptr;
}

std::string Context::version() {
  auto major = Genie_getApiMajorVersion();
  auto minor = Genie_getApiMinorVersion();
  auto patch = Genie_getApiPatchVersion();
  char version[32];
  snprintf(version, sizeof(version), "%d.%d.%d", major, minor, patch);
  return std::string(version);
}

void Context::saveSession(const char *filename) {
  if (handle == NULL) {
    throw std::runtime_error("Context handle is NULL");
  }
  if (busy) {
    throw std::runtime_error("Context is busy");
  }
  Genie_Status_t status = GenieDialog_save(handle, filename);
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
}

void Context::restoreSession(const char *filename) {
  if (handle == NULL) {
    throw std::runtime_error("Context handle is NULL");
  }
  if (busy) {
    throw std::runtime_error("Context is busy");
  }
  Genie_Status_t status = GenieDialog_restore(handle, filename);
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
}

void Context::process(std::string prompt) {
  if (handle == NULL) {
    throw std::runtime_error("Context handle is NULL");
  }
  if (busy) {
    throw std::runtime_error("Context is busy");
  }
  BusyGuard guard(busy);
  std::string query = prompt;
  Genie_Status_t status;
  GenieDialog_SentenceCode_t sentenceCode = GENIE_DIALOG_SENTENCE_COMPLETE;
  if (!last_context_data.empty()) {
    sentenceCode = GENIE_DIALOG_SENTENCE_REWIND;
  }
  status = GenieDialog_query(handle, query.c_str(), sentenceCode, process_callback, this);
  if (status != GENIE_STATUS_SUCCESS && status != GENIE_STATUS_WARNING_ABORTED) {
    // retry normal query
    if (prompt.find(last_context_data) == 0) {
      query = prompt.substr(last_context_data.length());
    } else {
      status = GenieDialog_reset(handle);
      if (status != GENIE_STATUS_SUCCESS) {
        throw std::runtime_error(genie_status_to_string(status));
      }
    }
    status = GenieDialog_query(handle, query.c_str(), sentenceCode, process_callback, this);
  }
  if (status != GENIE_STATUS_SUCCESS && status != GENIE_STATUS_WARNING_ABORTED) {
    throw std::runtime_error(genie_status_to_string(status));
  }
  last_context_data = prompt;
}

void Context::process_callback(const char *response,
  const GenieDialog_SentenceCode_t sentenceCode,
  const void *userData) {
  Context *self = (Context *)userData;
  GenieDialog_signal(self->handle, GENIE_DIALOG_ACTION_ABORT);
}

QueryResult Context::query(const QueryParams &params, Callback callback) {
  if (handle == NULL) {
    throw std::runtime_error("Context handle is NULL");
  }
  if (busy) {
    throw std::runtime_error("Context is busy");
  }
  BusyGuard guard(busy);
  applyStopWords(params.stop_json);
  applyMaxTokens(params.max_tokens);
  applySampler(params.sampler);
  applyGrammar(params.grammar);
  const std::string &input = params.prompt;
  std::string query = input;
  Genie_Status_t status;
  GenieDialog_SentenceCode_t sentenceCode = GENIE_DIALOG_SENTENCE_COMPLETE;
  if (!last_context_data.empty()) {
    sentenceCode = GENIE_DIALOG_SENTENCE_REWIND;
  }
  this->callback = std::move(callback);
  this->text.clear();
  status = GenieDialog_query(handle, query.c_str(), sentenceCode, on_response, this);
  if (status != GENIE_STATUS_SUCCESS && status != GENIE_STATUS_WARNING_ABORTED) {
    // retry normal query
    if (input.find(last_context_data) == 0) {
      query = input.substr(last_context_data.length());
    } else {
      status = GenieDialog_reset(handle);
      if (status != GENIE_STATUS_SUCCESS) {
        this->callback = nullptr;
        throw std::runtime_error(genie_status_to_string(status));
      }
    }
    this->text.clear();
    status = GenieDialog_query(handle, query.c_str(), sentenceCode, on_response, this);
  }
  this->callback = nullptr;
  if (status != GENIE_STATUS_SUCCESS && status != GENIE_STATUS_WARNING_ABORTED) {
    throw std::runtime_error(genie_status_to_string(status));
  }
  last_context_data = input;

  QueryResult result;
  result.text = std::move(this->text);
  this->text.clear();
  result.interrupted = status == GENIE_STATUS_WARNING_ABORTED;
  Genie_DataType_t dataType;
  Genie_Value_t value;
  if (GenieDialog_getValue(handle, GENIE_DIALOG_PARAM_NUM_GENERATED, nullptr,
                           &dataType, &value) == GENIE_STATUS_SUCCESS) {
    result.num_generated = value.uint32Value;
  }
  const char *profile_json = nullptr;
  GenieProfile_getJsonData(profileHandle, alloc_json_data, &profile_json);
  if (profile_json) {
    result.profile_json = profile_json;
    free((char *)profile_json);
  }
  return result;
}

void Context::abort() {
  if (handle == NULL) {
    throw std::runtime_error("Context handle is NULL");
  }
  Genie_Status_t status = GenieDialog_signal(handle, GENIE_DIALOG_ACTION_ABORT);
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
}

void Context::on_response(const char *response, const GenieDialog_SentenceCode_t sentenceCode,
                          const void *userData) {
  auto self = (Context *)userData;
  if (self == nullptr) return;
  if (response) {
    self->last_context_data += response;
    self->text += response;
  }
  if (self->callback != nullptr) {
    self->callback(response, sentenceCode);
  }
}

void Context::applyStopWords(const std::string &stop_json) {
  if (stop_json == applied_stop_json) {
    return;
  }
  Genie_Status_t status = GenieDialog_setStopSequence(handle, stop_json.c_str());
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
  applied_stop_json = stop_json;
}

void Context::applyMaxTokens(uint32_t max_tokens) {
  if (max_tokens == applied_max_tokens) {
    return;
  }
  Genie_Status_t status = GenieDialog_setMaxNumTokens(handle, max_tokens);
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
  applied_max_tokens = max_tokens;
}

void Context::applySampler(const SamplerParams &sampler) {
  if (sampler == applied_sampler) {
    return;
  }
  GenieSampler_Handle_t samplerHandle = NULL;
  Genie_Status_t status = GenieDialog_getSampler(handle, &samplerHandle);
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
  std::ostringstream json;
  json.imbue(std::locale::classic());
  json << std::setprecision(9) << "{\"sampler\":{\"version\":1"
       << ",\"temp\":" << sampler.temperature << ",\"top-k\":" << sampler.top_k
       << ",\"top-p\":" << sampler.top_p << ",\"seed\":" << sampler.seed
       << "}}";
  GenieSamplerConfig_Handle_t samplerConfig = NULL;
  status = GenieSamplerConfig_createFromJson(json.str().c_str(), &samplerConfig);
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
  status = GenieSampler_applyConfig(samplerHandle, samplerConfig);
  GenieSamplerConfig_free(samplerConfig);
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
  applied_sampler = sampler;
}

void Context::applyGrammar(const GrammarParams &grammar) {
  if (grammar == applied_grammar) {
    return;
  }
  // Genie only reports GENIE_STATUS_ERROR_GENERAL (details go to the log), so
  // catch the known unsupported case up front with a readable message.
  if (!grammar.kind.empty() && dialog_type != "basic" && dialog_type != "eaglet") {
    throw std::runtime_error("Dialog type '" + dialog_type +
                             "' does not support grammar (supported: basic, eaglet)");
  }
  Genie_Status_t status;
  if (grammar.kind.empty()) {
    status = GenieDialog_setGrammar(handle, nullptr, nullptr);
  } else if (!grammar.file.empty()) {
    status = GenieDialog_setGrammar(handle, grammar.kind.c_str(), grammar.file.c_str());
  } else {
    // GenieDialog_setGrammar only accepts a file path; it reads and compiles
    // the file before returning, so the temp file can be removed right after.
    static std::atomic<uint64_t> counter{0};
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::string path = tmp_dir + "/qnn-llm-grammar-" + std::to_string(stamp) + "-" +
                       std::to_string(counter++);
    {
      std::ofstream file(path, std::ios::binary);
      file << grammar.content;
      if (!file) {
        std::remove(path.c_str());
        throw std::runtime_error("Failed to write grammar file");
      }
    }
    status = GenieDialog_setGrammar(handle, grammar.kind.c_str(), path.c_str());
    std::remove(path.c_str());
  }
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(std::string("Failed to set grammar (invalid ") +
                             grammar.kind + "?): " + genie_status_to_string(status));
  }
  applied_grammar = grammar;
}

void Context::applyLora(const std::string &engine, const std::string &lora_adapter_name) {
  if (busy) {
    throw std::runtime_error("Context is busy");
  }
  Genie_Status_t status = GenieDialog_applyLora(handle, engine.c_str(), lora_adapter_name.c_str());
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
}

void Context::setLoraStrength(const std::string &engine, const LoraStrengthMap &lora_strength_map) {
  if (busy) {
    throw std::runtime_error("Context is busy");
  }
  for (const auto &[tensor_name, alpha] : lora_strength_map) {
    Genie_Status_t status =
        GenieDialog_setLoraStrength(handle, engine.c_str(), tensor_name.c_str(), alpha);
    if (status != GENIE_STATUS_SUCCESS) {
      throw std::runtime_error(genie_status_to_string(status));
    }
  }
}

void Context::releaseLoraMemory(const std::string &engine, const std::string &lora_adapter_name) {
  if (busy) {
    throw std::runtime_error("Context is busy");
  }
  Genie_Status_t status =
      GenieDialog_releaseLoraMemory(handle, engine.c_str(), lora_adapter_name.c_str());
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
}

uint32_t Context::getContextOccupancy() {
  if (busy) {
    throw std::runtime_error("Context is busy");
  }
  Genie_DataType_t dataType;
  Genie_Value_t value;
  Genie_Status_t status = GenieDialog_getValue(
      handle, GENIE_DIALOG_PARAM_CONTEXT_OCCUPANCY, nullptr, &dataType, &value);
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
  return value.uint32Value;
}

std::string Context::getAppliedLoraAdapter() {
  if (busy) {
    throw std::runtime_error("Context is busy");
  }
  Genie_DataType_t dataType;
  Genie_Value_t value;
  value.stringValue = nullptr;
  Genie_Status_t status = GenieDialog_getValue(
      handle, GENIE_DIALOG_PARAM_APPLIED_LORA_ADAPTER, alloc_json_data, &dataType, &value);
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
  std::string result = value.stringValue ? value.stringValue : "";
  free((void *)value.stringValue);
  return result;
}

}  // namespace qnnllm
