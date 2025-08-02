# react-native-qnn-llm

Qualcomm lib Genie binding for React Native

## Installation

```sh
npm install react-native-qnn-llm
```

## Usage


```js
import { Context, SentenceCode } from 'react-native-qnn-llm';

const context = await Context.create(/* Genie config object */);
// Or load bundled
// const context = await Context.load({ bundle_path: 'path/to/bundle', unpack_dir: 'path/to/store/unpacked', n_thread?: Number })

await context.query('Hello, world!', (result, sentenceCode) => {
  console.log(result);
});

await context.save_session('path/to/session-directory');

await context.restore_session('path/to/session-directory');

await context.set_stop_words(['stop_word1', 'stop_word2']);

await context.apply_sampler_config({
  /* Genie sampler config */
});

await context.release();
```


## Contributing

See the [contributing guide](CONTRIBUTING.md) to learn how to contribute to the repository and the development workflow.

## License

MIT

---

Made with [create-react-native-library](https://github.com/callstack/react-native-builder-bob)
