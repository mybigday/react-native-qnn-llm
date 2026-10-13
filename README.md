# react-native-qnn-llm

Qualcomm lib Genie binding for React Native

## Installation

```sh
npm install react-native-qnn-llm
```

### Android Setup

#### Download and setup env

1. Download SDK from [here](https://www.qualcomm.com/developer/software/qualcomm-ai-engine-direct-sdk)
2. Setup env `source path/to/QNN/SDK/bin/envsetup.sh`

#### QNN libs

Add QNN libs dependency into `android/app/build.gradle`

```diff
dependencies {
+   implementation "com.qualcomm.qti:qnn-runtime:latest.integration@aar"
```

Or set `useQnnSdkLib=true` in `android/gradle.properties`

Notes: If you use dependency, the version should match QNN SDK version (currently built with QAIRT `2.51.0.260929`)

#### Proguard

```diff
+ -keep class com.qnnllm.** { *; }
```

## Usage

```js
import { Context, SentenceCode } from 'react-native-qnn-llm';

const context = await Context.create(/* Genie config object */);
// Or load bundled
// const context = await Context.load({ bundle_path: 'path/to/bundle', unpack_dir: 'path/to/store/unpacked', n_threads?: Number, dialog_type?: string })

// The config may hold settings for several dialog types (`lade`, `ssd-q1`, `spd`, ...);
// `dialog_type` overrides `dialog.type` and the unused blocks are dropped before loading.
// Picking a type the config has no settings for throws. `basic` is always available.
// const context = await Context.create(config, { dialog_type: 'basic' });

// Only `prompt` is required; the callback is optional too.
const result = await context.query({ prompt: 'Hello, world!' }, (text, sentenceCode) => {
  console.log(text);
});

// All other params are optional and per query: when omitted they fall back to
// the value from the Genie config, not to what a previous query used.
await context.query({
  prompt: 'Describe a person as JSON.',
  stop: ['<|eot_id|>'],
  n_predict: 128,
  temperature: 0.7,
  json_schema: { type: 'object', properties: { name: { type: 'string' } }, required: ['name'] },
});
// {
//   text, tokens_evaluated, tokens_predicted, interrupted,
//   timings: { prompt_n, prompt_ms, prompt_per_token_ms, prompt_per_second,
//              predicted_n, predicted_ms, predicted_per_token_ms, predicted_per_second },
//   profile, // raw Genie profile JSON
// }

await context.save_session('path/to/session-directory');

await context.restore_session('path/to/session-directory');

// how full the context window is (unitless, driver-reported)
const occupancy = await context.get_context_occupancy();

await context.apply_lora('primary', 'my-lora-adapter');

await context.set_lora_strength('primary', { 'lora_alpha_tensor_name': 0.8 });

// name of the LoRA adapter currently applied, or '' if none
const loraAdapter = await context.get_applied_lora_adapter();

await context.release_lora_memory('primary', 'my-lora-adapter');

// aborts a running query first
await context.release();
```

### Query params

| Param | Type | When omitted |
|---|---|---|
| `prompt` | `string` | **required** |
| `stop` | `string[]` | config `dialog.stop-sequence` (or none) |
| `n_predict` | `number`, `< 0` = no limit | config `dialog.max-num-tokens` (or no limit) |
| `temperature`, `top_k`, `top_p`, `seed` | `number`, each independent | config `dialog.sampler` value (or Genie default) |
| `grammar` (EBNF) / `json_schema` (`string \| object`) / `regex` | pick one, precedence `grammar` > `json_schema` > `regex` | config `dialog.context.grammar` (or none) |

- Grammar needs dialog type `basic` or `eaglet`. With a grammar, Genie emits the EOS token text (e.g. `<|eot_id|>`) at the end; add it to `stop` to drop it.
- `lade` dialogs in QAIRT 2.51 ignore `n_predict`, `stop` and `abort()`: generation only ends at EOS or a full context, and `release()` during a query waits for that. Use `basic` if you need them.

### Migrating from 0.2

- `query(prompt, callback)` is now `query({ prompt, ...params }, callback?)` and resolves with a result object instead of the raw profile (still available as `result.profile`).
- `set_stop_words()` and `apply_sampler_config()` are removed, pass `stop`, `temperature`, `top_k`, `top_p`, `seed` per query instead.

## Bundled File

To easier to deploy model, we announced packed file struct.

- Constant entry config path.
- Auto resolve file path.
- Patch config on load.

You can quickly pack your model files use [pack.py](https://github.com/mybigday/node-qnn-llm/blob/main/pack.py).

Usage: `pack.py path/to/config.json`

## Contributing

See the [contributing guide](CONTRIBUTING.md) to learn how to contribute to the repository and the development workflow.

## License

MIT

---

Made with [create-react-native-library](https://github.com/callstack/react-native-builder-bob)

---

<p align="center">
  <a href="https://bricks.tools">
    <img width="90px" src="https://avatars.githubusercontent.com/u/17320237?s=200&v=4">
  </a>
  <p align="center">
    Built and maintained by <a href="https://bricks.tools">BRICKS</a>.
  </p>
</p>