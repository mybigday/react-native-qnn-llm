import { NativeModules, Platform, NativeEventEmitter } from 'react-native';
import type { NativeQueryParams } from './NativeQnnLlm';
import type {
  ContextConfig,
  CreateOptions,
  DialogType,
  QueryParams,
  QueryResult,
} from './types';
import {
  configDefaults,
  preProcessConfig,
  resolveQueryParams,
  selectDialogType,
  toQueryResult,
} from './utils';

export * from './types';

const LINKING_ERROR =
  `The package 'react-native-qnn-llm' doesn't seem to be linked. Make sure: \n\n` +
  Platform.select({ ios: "- You have run 'pod install'\n", default: '' }) +
  '- You rebuilt the app after installing the package\n' +
  '- You are not using Expo Go\n';

// @ts-expect-error
const isTurboModuleEnabled = global.__turboModuleProxy != null;

const QnnLlmModule =
  isTurboModuleEnabled && Platform.OS === 'android'
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

export const getHtpConfigFilePath = () => QnnLlm.HTP_CONFIG_FILE_PATH;

export class Context {
  private _id: number;
  private _defaults: NativeQueryParams;

  private constructor(context: number, defaults: NativeQueryParams) {
    this._id = context;
    this._defaults = defaults;
  }

  /**
   * Create a context from a config.
   * @param config - The config to create the context.
   * @param options - Create options.
   * @returns The context.
   */
  static async create(
    config: ContextConfig,
    { dialog_type }: CreateOptions = {}
  ): Promise<Context> {
    const selected = selectDialogType(config, dialog_type);
    const defaults = configDefaults(selected);
    const context = await QnnLlm.createContext(
      JSON.stringify(selected),
      selected.dialog.type,
      defaults
    );
    return new Context(context, defaults);
  }

  /**
   * Load a context from a bundle.
   * @param bundle_path - The path to the bundled model.
   * @param unpack_dir - The path to store the unpacked model.
   * @param n_threads - The number of threads to use.
   * @param dialog_type - Dialog type to use, see `CreateOptions`.
   * @returns The context.
   */
  static async load({
    bundle_path,
    unpack_dir,
    n_threads,
    dialog_type,
  }: {
    bundle_path: string;
    unpack_dir: string;
    n_threads?: number;
    dialog_type?: DialogType;
  }): Promise<Context> {
    const config = JSON.parse(await QnnLlm.unpack(bundle_path, unpack_dir));
    if (!config.dialog) throw new Error('Config is not a LLM dialog config');
    await preProcessConfig(config, unpack_dir, {
      n_threads,
      htpConfigFilePath: getHtpConfigFilePath(),
      exists: (path) => QnnLlm.exists(path),
      isWindows: Platform.OS === 'windows',
    });
    return Context.create(config, { dialog_type });
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
   * Params other than `prompt` are per query: when omitted they fall back to
   * the value from the Genie config, not to what a previous query used.
   * @param params - The query params.
   * @param callback - Called with each piece of the response.
   * @returns The result with generation stats.
   */
  async query(
    params: QueryParams,
    callback?: (response: string, sentenceCode: SentenceCode) => void
  ): Promise<QueryResult> {
    const resolved = resolveQueryParams(params, this._defaults);
    const listener = callback
      ? eventEmitter!.addListener('onResponse', (event) => {
          const { response, sentenceCode, contextId } = event as ResponseEvent;
          if (contextId !== this._id) {
            return;
          }
          callback(response, sentenceCode);
        })
      : null;
    try {
      return toQueryResult(await QnnLlm.query(this._id, resolved));
    } finally {
      listener?.remove();
    }
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
   * How full the context window is (unitless, driver-reported).
   */
  get_context_occupancy(): Promise<number> {
    return QnnLlm.getContextOccupancy(this._id);
  }

  /**
   * Apply a LoRA adapter defined in the config.
   * @param engine - The engine role, e.g. `primary`.
   * @param lora_adapter_name - The adapter name.
   */
  apply_lora(engine: string, lora_adapter_name: string): Promise<void> {
    return QnnLlm.applyLora(this._id, engine, lora_adapter_name);
  }

  /**
   * Set LoRA strength.
   * @param engine - The engine role, e.g. `primary`.
   * @param strengths - Map of alpha tensor name to strength.
   */
  set_lora_strength(
    engine: string,
    strengths: Record<string, number>
  ): Promise<void> {
    return QnnLlm.setLoraStrength(this._id, engine, strengths);
  }

  /**
   * Name of the LoRA adapter currently applied, or '' if none.
   */
  get_applied_lora_adapter(): Promise<string> {
    return QnnLlm.getAppliedLoraAdapter(this._id);
  }

  /**
   * Release the memory of a LoRA adapter.
   * @param engine - The engine role, e.g. `primary`.
   * @param lora_adapter_name - The adapter name.
   */
  release_lora_memory(
    engine: string,
    lora_adapter_name: string
  ): Promise<void> {
    return QnnLlm.releaseLoraMemory(this._id, engine, lora_adapter_name);
  }

  /**
   * Release the context. A running query is aborted first.
   */
  release(): Promise<void> {
    return QnnLlm.freeContext(this._id);
  }
}
