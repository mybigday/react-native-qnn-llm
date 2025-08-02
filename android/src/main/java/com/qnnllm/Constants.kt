package com.qnnllm

object Constants {
  const val HTP_CONFIG =
    """
    {
      "devices": [
        {
          "cores":[{
            "perf_profile": "burst",
            "rpc_control_latency": 100
          }]
        }
      ]
    }
    """
}
