#include "context.h"
#include "log.h"

namespace qnnllm {

const char *genie_status_to_string(int status) {
  switch (status) {
    case GENIE_STATUS_SUCCESS:
      return "GENIE_STATUS_SUCCESS";
    case GENIE_STATUS_WARNING_ABORTED:
      return "GENIE_STATUS_WARNING_ABORTED";
    case GENIE_STATUS_ERROR_GENERAL:
      return "GENIE_STATUS_ERROR_GENERAL";
    case GENIE_STATUS_ERROR_INVALID_ARGUMENT:
      return "GENIE_STATUS_ERROR_INVALID_ARGUMENT";
    case GENIE_STATUS_ERROR_MEM_ALLOC:
      return "GENIE_STATUS_ERROR_MEM_ALLOC";
    case GENIE_STATUS_ERROR_INVALID_CONFIG:
      return "GENIE_STATUS_ERROR_INVALID_CONFIG";
    case GENIE_STATUS_ERROR_INVALID_HANDLE:
      return "GENIE_STATUS_ERROR_INVALID_HANDLE";
    case GENIE_STATUS_ERROR_QUERY_FAILED:
      return "GENIE_STATUS_ERROR_QUERY_FAILED";
    case GENIE_STATUS_ERROR_JSON_FORMAT:
      return "GENIE_STATUS_ERROR_JSON_FORMAT";
    case GENIE_STATUS_ERROR_JSON_SCHEMA:
      return "GENIE_STATUS_ERROR_JSON_SCHEMA";
    case GENIE_STATUS_ERROR_JSON_VALUE:
      return "GENIE_STATUS_ERROR_JSON_VALUE";
    case GENIE_STATUS_ERROR_GENERATE_FAILED:
      return "GENIE_STATUS_ERROR_GENERATE_FAILED";
    case GENIE_STATUS_ERROR_GET_HANDLE_FAILED:
      return "GENIE_STATUS_ERROR_GET_HANDLE_FAILED";
    case GENIE_STATUS_ERROR_APPLY_CONFIG_FAILED:
      return "GENIE_STATUS_ERROR_APPLY_CONFIG_FAILED";
    case GENIE_STATUS_ERROR_SET_PARAMS_FAILED:
      return "GENIE_STATUS_ERROR_SET_PARAMS_FAILED";
    case GENIE_STATUS_ERROR_BOUND_HANDLE:
      return "GENIE_STATUS_ERROR_BOUND_HANDLE";
    default:
      return "UNKNOWN_GENIE_STATUS";
  }
}
  
void alloc_json_data(size_t size, const char **data) { *data = (char *)malloc(size); }

Context::Context(const char *config_str) {
  Genie_Status_t status;
  status = GenieProfile_create(NULL, &profileHandle);
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
  status = GenieDialogConfig_createFromJson(config_str, &configHandle);
  if (status != GENIE_STATUS_SUCCESS) {
    GenieProfile_free(profileHandle);
    throw std::runtime_error(genie_status_to_string(status));
  }
  status = GenieDialogConfig_bindProfiler(configHandle, profileHandle);
  if (status != GENIE_STATUS_SUCCESS) {
    GenieDialogConfig_free(configHandle);
    GenieProfile_free(profileHandle);
    throw std::runtime_error(genie_status_to_string(status));
  }
  status = GenieDialog_create(configHandle, &handle);
  if (status != GENIE_STATUS_SUCCESS) {
    GenieDialogConfig_free(configHandle);
    GenieProfile_free(profileHandle);
    throw std::runtime_error(genie_status_to_string(status));
  }
  busy = false;
  last_context_data = "";
}

Context::~Context() {
  if (callback != nullptr) {
    callback = nullptr;
  }
  if (handle != NULL) {
    if (GenieDialog_free(handle) != GENIE_STATUS_SUCCESS) {
      LOGE("Failed to free GenieDialog handle");
    }
  }
  if (configHandle != NULL) {
    if (GenieDialogConfig_free(configHandle) != GENIE_STATUS_SUCCESS) {
      LOGE("Failed to free GenieDialogConfig handle");
    }
  }
  if (profileHandle != NULL) {
    if (GenieProfile_free(profileHandle) != GENIE_STATUS_SUCCESS) {
      LOGE("Failed to free GenieProfile handle");
    }
  }
}

std::string Context::version() {
  auto major = Genie_getApiMajorVersion();
  auto minor = Genie_getApiMinorVersion();
  auto patch = Genie_getApiPatchVersion();
  char version[8];
  snprintf(version, sizeof(version), "%d.%d.%d", major, minor, patch);
  return std::string(version);
}

void Context::setStopWords(const char *stop_words) {
  Genie_Status_t status = GenieDialog_setStopSequence(handle, stop_words);
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
}

void Context::applySamplerConfig(const char *config_str) {
  if (handle == NULL) {
    throw std::runtime_error("Context handle is NULL");
  }
  GenieSampler_Handle_t samplerHandle = NULL;
  Genie_Status_t status = GenieDialog_getSampler(handle, &samplerHandle);
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
  GenieSamplerConfig_Handle_t configHandle = NULL;
  status = GenieSamplerConfig_createFromJson(config_str, &configHandle);
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
  status = GenieSampler_applyConfig(samplerHandle, configHandle);
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
}
  
void Context::saveSession(const char *filename) {
  if (handle == NULL) {
    throw std::runtime_error("Context handle is NULL");
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
  Genie_Status_t status = GenieDialog_restore(handle, filename);
  if (status != GENIE_STATUS_SUCCESS) {
    throw std::runtime_error(genie_status_to_string(status));
  }
}

void Context::process(std::string prompt) {
  if (busy) {
    throw std::runtime_error("Context is busy");
  }
  std::string query = prompt;
  Genie_Status_t status;
  GenieDialog_SentenceCode_t sentenceCode = GENIE_DIALOG_SENTENCE_COMPLETE;
  if (!last_context_data.empty()) {
    sentenceCode = GENIE_DIALOG_SENTENCE_REWIND;
  }
  busy = true;
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
  busy = false;
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
  
std::string Context::query(std::string input, Callback callback) {
  if (busy) {
    throw std::runtime_error("Context is busy");
  }
  std::string query = input;
  Genie_Status_t status;
  GenieDialog_SentenceCode_t sentenceCode = GENIE_DIALOG_SENTENCE_COMPLETE;
  if (!last_context_data.empty()) {
    sentenceCode = GENIE_DIALOG_SENTENCE_REWIND;
  }
  busy = true;
  this->callback = std::move(callback);
  status = GenieDialog_query(handle, query.c_str(), sentenceCode, on_response, this);
  if (status != GENIE_STATUS_SUCCESS && status != GENIE_STATUS_WARNING_ABORTED) {
    // retry normal query
    if (input.find(last_context_data) == 0) {
      query = input.substr(last_context_data.length());
    } else {
      status = GenieDialog_reset(handle);
      if (status != GENIE_STATUS_SUCCESS) {
        throw std::runtime_error(genie_status_to_string(status));
      }
    }
    status = GenieDialog_query(handle, query.c_str(), sentenceCode, on_response, this);
  }
  busy = false;
  if (status != GENIE_STATUS_SUCCESS && status != GENIE_STATUS_WARNING_ABORTED) {
    throw std::runtime_error(genie_status_to_string(status));
  }
  last_context_data = input;
  const char* profile_json = nullptr;
  GenieProfile_getJsonData(profileHandle, alloc_json_data, &profile_json);
  std::string profile_json_str(profile_json);
  free((char*)profile_json);
  return profile_json_str;
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
  if (self == nullptr || self->callback == nullptr) return;
  if (response) {
    self->last_context_data += response;
  }
  self->callback(response, sentenceCode);
  if (
    sentenceCode == GENIE_DIALOG_SENTENCE_COMPLETE ||
    sentenceCode == GENIE_DIALOG_SENTENCE_END ||
    sentenceCode == GENIE_DIALOG_SENTENCE_ABORT
  ) {
    LOGI("Response complete");
    self->callback = nullptr;
  }
}

}  // namespace qnnllm
