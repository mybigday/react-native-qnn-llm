import { NativeModules, Platform, NativeEventEmitter } from 'react-native';

const LINKING_ERROR =
  `The package 'react-native-qnn-llm' doesn't seem to be linked. Make sure: \n\n` +
  Platform.select({ ios: "- You have run 'pod install'\n", default: '' }) +
  '- You rebuilt the app after installing the package\n' +
  '- You are not using Expo Go\n';

// @ts-expect-error
const isTurboModuleEnabled = global.__turboModuleProxy != null;

const QnnLlmModule = isTurboModuleEnabled
  ? require('./NativeQnnLlm').default
  : NativeModules.QnnLlm;

const QnnLlm = QnnLlmModule
  ? QnnLlmModule
  : new Proxy(
      {},
      {
        get() {
          throw new Error(LINKING_ERROR);
        },
      }
    );

const eventEmitter = QnnLlmModule ? new NativeEventEmitter(QnnLlmModule) : null;

export enum SentenceCode {
  Complete = 0,
  Begin = 1,
  Continue = 2,
  End = 3,
  Abort = 4,
}

interface ResponseEvent {
  response: string;
  sentenceCode: SentenceCode;
  contextId: number;
}

const join = (...paths: string[]) => paths.join('/');

export const getHtpConfigFilePath = () => QnnLlm.HTP_CONFIG_FILE_PATH;

export interface SamplerConfig {
  'version': number;
  'seed': number;
  'temp': number;
  'top-k': number;
  'top-p': number;
  'greedy': boolean;
}

/**
 * Context config.
 * @see https://docs.qualcomm.com/bundle/publicresource/topics/80-63442-100/json.html
 */
export interface ContextConfig {
  dialog: {
    version: number;
    type: 'basic';
    context: {
      version: number;
      [key: string]: any;
    };
    sampler: {
      version: number;
      [key: string]: any;
    };
    tokenizer: {
      version: number;
      path: string;
    };
    engine: {
      'version': number;
      'n-threads': number;
      'backend': {
        type: 'QnnHtp' | 'QnnGenAiTransformer';
        QnnHtp?: {
          'use-mmap': boolean;
          [key: string]: any;
        };
        QnnGenAiTransformer?: Record<string, any>;
        extensions?: string;
      };
      'model': {
        version: number;
        type: 'binary' | 'library';
        binary?: {
          'version': number;
          'ctx-bins': string[];
        };
        library?: {
          'version': number;
          'model-bin': string;
        };
        [key: string]: any;
      };
    };
  };
}

export class Context {
  private _id: number;

  private constructor(context: number) {
    this._id = context;
  }

  /**
   * Create a context from a config.
   * @param config - The config to create the context.
   * @returns The context.
   */
  static async create(config: ContextConfig): Promise<Context> {
    const context = await QnnLlm.create(JSON.stringify(config));
    return new Context(context);
  }

  /**
   * Load a context from a bundle.
   * @param bundle_path - The path to the bundled model.
   * @param unpack_dir - The path to store the unpacked model.
   * @param n_threads - The number of threads to use.
   * @returns The context.
   */
  static async load({
    bundle_path,
    unpack_dir,
    n_threads,
  }: {
    bundle_path: string;
    unpack_dir: string;
    n_threads?: number;
  }): Promise<Context> {
    const config = JSON.parse(await QnnLlm.unpack(bundle_path, unpack_dir));
    if (config.dialog.engine.backend.type === 'QnnHtp') {
      config.dialog.engine.backend.extensions = getHtpConfigFilePath();
      config.dialog.engine.backend.QnnHtp['use-mmap'] =
        Platform.OS !== 'windows';
    }
    config.dialog.tokenizer.path = join(
      unpack_dir,
      config.dialog.tokenizer.path
    );
    if (config.dialog.engine.model.type === 'binary') {
      const bins = config.dialog.engine.model.binary['ctx-bins'];
      config.dialog.engine.model.binary['ctx-bins'] = bins.map((bin: string) =>
        join(unpack_dir, bin)
      );
    } else {
      const bin = config.dialog.engine.library['model-bin'];
      config.dialog.engine.library['model-bin'] = join(unpack_dir, bin);
    }
    if (n_threads && n_threads > 0)
      config.dialog.engine['n-threads'] = n_threads;
    return Context.create(config);
  }

  /**
   * Process the prompt.
   * @param input - The prompt to process.
   */
  process(input: string): Promise<void> {
    return QnnLlm.process(this._id, input);
  }

  /**
   * Make a completion request.
   * @param input - The input to query.
   * @param callback - The callback to call when the response is received.
   * @returns Performance profile.
   */
  async query(
    input: string,
    callback: (response: string, sentenceCode: SentenceCode) => void
  ): Promise<object> {
    const listener = eventEmitter!.addListener(
      'response',
      ({ response, sentenceCode, contextId }: ResponseEvent) => {
        if (contextId !== this._id) {
          return;
        }
        callback(response, sentenceCode);
      }
    );
    try {
      return JSON.parse(await QnnLlm.query(this._id, input));
    } catch (error) {
      throw error;
    } finally {
      listener.remove();
    }
  }

  /**
   * Set the stop words.
   * @param stopWords - The stop words to set.
   */
  set_stop_words(stopWords?: string[]): Promise<void> {
    return QnnLlm.setStopWords(
      this._id,
      stopWords ? JSON.stringify({ 'stop-sequence': stopWords }) : '{}'
    );
  }

  /**
   * Apply the sampler config.
   * @param config - The sampler config to apply.
   */
  apply_sampler_config(config: Partial<SamplerConfig>): Promise<void> {
    return QnnLlm.applySamplerConfig(this._id, JSON.stringify(config));
  }

  /**
   * Save the session.
   * @param filename - The filename to save the session to.
   */
  save_session(filename: string): Promise<void> {
    return QnnLlm.saveSession(this._id, filename);
  }

  /**
   * Restore the session.
   * @param filename - The filename to restore the session from.
   */
  restore_session(filename: string): Promise<void> {
    return QnnLlm.restoreSession(this._id, filename);
  }

  /**
   * Abort the completion.
   */
  abort(): Promise<void> {
    return QnnLlm.abort(this._id);
  }

  /**
   * Release the context.
   */
  release(): Promise<void> {
    return QnnLlm.free(this._id);
  }
}
