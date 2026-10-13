#pragma once

#include "GenieDialog.h"
#include "log.h"
#include <string>
#include <atomic>
#include <stdexcept>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <unordered_map>

#ifndef QNN_LOG_LEVEL
#define QNN_LOG_LEVEL GENIE_LOG_LEVEL_INFO
#endif

namespace qnnllm {

typedef std::unordered_map<std::string, float> LoraStrengthMap;

// Defaults mirror qualla's basic sampler when the config leaves a key unset.
struct SamplerParams {
  float temperature = 0.1f;
  int32_t top_k = 0;
  float top_p = 0.8f;
  int32_t seed = -1;
  bool operator==(const SamplerParams &o) const {
    return temperature == o.temperature && top_k == o.top_k &&
           top_p == o.top_p && seed == o.seed;
  }
};

// Empty kind means no grammar. Either `content` (written to a temp file) or
// `file` (an existing path, e.g. from the dialog config) is used.
struct GrammarParams {
  std::string kind;
  std::string content;
  std::string file;
  bool operator==(const GrammarParams &o) const {
    return kind == o.kind && content == o.content && file == o.file;
  }
};

struct QueryParams {
  std::string prompt;
  std::string stop_json = "{}";
  uint32_t max_tokens = UINT32_MAX;
  SamplerParams sampler;
  GrammarParams grammar;
};

struct QueryResult {
  std::string text;
  std::string profile_json;
  bool interrupted = false;
  int64_t num_generated = -1;
};

class Context {
public:
  typedef std::function<void(const char *response, const GenieDialog_SentenceCode_t sentenceCode)> Callback;

  // `defaults` holds the per-query values the dialog config was created with,
  // `tmp_dir` is where grammar content is written for GenieDialog_setGrammar.
  Context(const char *config_str, QueryParams defaults, std::string dialog_type,
          std::string tmp_dir);
  ~Context();

  void release();

  void saveSession(const char *filename);

  void restoreSession(const char *filename);

  void process(std::string prompt);

  QueryResult query(const QueryParams &params, Callback callback);

  void abort();

  void applyLora(const std::string &engine, const std::string &lora_adapter_name);

  void setLoraStrength(const std::string &engine, const LoraStrengthMap &lora_strength_map);

  void releaseLoraMemory(const std::string &engine, const std::string &lora_adapter_name);

  uint32_t getContextOccupancy();

  std::string getAppliedLoraAdapter();

  static std::string version();

protected:
  static void on_response(const char *response, const GenieDialog_SentenceCode_t sentenceCode,
                          const void *userData);

  static void process_callback(const char *response, const GenieDialog_SentenceCode_t sentenceCode,
                               const void *userData);

private:
  void applyStopWords(const std::string &stop_json);
  void applyMaxTokens(uint32_t max_tokens);
  void applySampler(const SamplerParams &sampler);
  void applyGrammar(const GrammarParams &grammar);

  GenieDialog_Handle_t handle = NULL;
  GenieDialogConfig_Handle_t configHandle = NULL;
  GenieProfile_Handle_t profileHandle = NULL;
  GenieLog_Handle_t logHandle = NULL;
  std::string last_context_data;
  std::string dialog_type;
  std::string tmp_dir;
  std::string applied_stop_json;
  uint32_t applied_max_tokens;
  SamplerParams applied_sampler;
  GrammarParams applied_grammar;
  std::atomic<bool> busy;
  Callback callback;
  std::string text;
};

}  // namespace qnnllm
