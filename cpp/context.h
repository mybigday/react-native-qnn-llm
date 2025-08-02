#pragma once

#include "GenieDialog.h"
#include "log.h"
#include <string>
#include <atomic>
#include <stdexcept>
#include <cstdlib>
#include <functional>

namespace qnnllm {

const char *genie_status_to_string(int status);

class Context {
public:
  typedef std::function<void(const char *response, const GenieDialog_SentenceCode_t sentenceCode)> Callback;

  Context(const char *config_str);
  ~Context();

  void setStopWords(const char *stop_words);

  void applySamplerConfig(const char *config_str);

  void saveSession(const char *filename);

  void restoreSession(const char *filename);

  void process(std::string prompt);

  std::string query(std::string input, Callback callback);

  void abort();

  static std::string version();

protected:
  static void on_response(const char *response, const GenieDialog_SentenceCode_t sentenceCode,
                          const void *userData);

  static void process_callback(const char *response, const GenieDialog_SentenceCode_t sentenceCode,
                               const void *userData);

private:
  GenieDialog_Handle_t handle = NULL;
  GenieDialogConfig_Handle_t configHandle = NULL;
  GenieProfile_Handle_t profileHandle = NULL;
  std::string last_context_data;
  std::atomic<bool> busy;
  Callback callback;
};

}  // namespace qnnllm
