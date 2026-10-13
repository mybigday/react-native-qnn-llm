#pragma once

#include <cstddef>

namespace qnnllm {

const char *genie_status_to_string(int status);

// Genie_AllocCallback_t for GenieProfile_getJsonData / GenieDialog_getValue,
// the caller frees the data
void alloc_json_data(size_t size, const char **data);

}  // namespace qnnllm
