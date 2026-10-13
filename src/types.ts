export type DialogType =
  | 'basic'
  | 'ssd-q1'
  | 'lade'
  | 'spd'
  | 'multistream'
  | 'eaglet'
  | 'kv-share';

export interface EngineConfig {
  'version': number;
  'n-threads': number;
  'role'?: string;
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
  [key: string]: any;
}

/**
 * Context config.
 * @see https://docs.qualcomm.com/bundle/publicresource/topics/80-63442-100/json.html
 */
export interface ContextConfig {
  dialog: {
    'version': number;
    'type': DialogType;
    'stop-sequence'?: string[];
    'max-num-tokens'?: number;
    'context': {
      version: number;
      grammar?: {
        type?: string;
        backend?: string;
        file?: string;
        [key: string]: any;
      };
      [key: string]: any;
    };
    'sampler': {
      'version': number;
      'temp'?: number;
      'top-k'?: number;
      'top-p'?: number;
      'seed'?: number;
      [key: string]: any;
    };
    'tokenizer': {
      version: number;
      path: string;
    };
    'engine': EngineConfig | EngineConfig[];
    [key: string]: any;
  };
}

export interface CreateOptions {
  /**
   * Dialog type to use, overrides `dialog.type`. Settings for the other
   * dialog types are dropped; picking one the config has no settings for throws.
   * `basic` is always available.
   */
  dialog_type?: DialogType;
}

export interface QueryParams {
  prompt: string;
  /** Stop sequences. Default: config `dialog.stop-sequence` (or none) */
  stop?: string[];
  /** Max tokens to generate, < 0 means no limit. Default: config `dialog.max-num-tokens` (or no limit) */
  n_predict?: number;
  /** Default: config `dialog.sampler.temp` */
  temperature?: number;
  /** Default: config `dialog.sampler.top-k` */
  top_k?: number;
  /** Default: config `dialog.sampler.top-p` */
  top_p?: number;
  /** Default: config `dialog.sampler.seed` */
  seed?: number;
  /** EBNF grammar, takes precedence over `json_schema` and `regex` */
  grammar?: string;
  /** JSON schema, takes precedence over `regex` */
  json_schema?: string | object;
  regex?: string;
}

export interface QueryTimings {
  prompt_n: number;
  prompt_ms: number;
  prompt_per_token_ms: number;
  prompt_per_second: number;
  predicted_n: number;
  predicted_ms: number;
  predicted_per_token_ms: number;
  predicted_per_second: number;
}

export interface QueryResult {
  text: string;
  tokens_evaluated: number;
  tokens_predicted: number;
  interrupted: boolean;
  timings: QueryTimings;
  /** Raw Genie profile JSON */
  profile: any;
}
