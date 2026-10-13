import type { NativeQueryParams, NativeQueryResult } from './NativeQnnLlm';
import type {
  ContextConfig,
  DialogType,
  EngineConfig,
  QueryParams,
  QueryResult,
} from './types';

export const join = (...paths: string[]) => paths.join('/');

const DIALOG_TYPES: DialogType[] = [
  'basic',
  'ssd-q1',
  'lade',
  'spd',
  'multistream',
  'eaglet',
  'kv-share',
];

// Dialog types that run two engines, and the roles Genie requires for them
const MULTI_ENGINE_ROLES: Partial<Record<DialogType, string[]>> = {
  'spd': ['draft', 'target'],
  'eaglet': ['draft', 'target'],
  'kv-share': ['primary', 'secondary'],
};

// Genie rejects a config carrying settings for any dialog type other than
// `dialog.type`, so a config may hold several and we keep only the chosen one.
export const selectDialogType = (
  config: ContextConfig,
  dialogType?: DialogType
): ContextConfig => {
  const dialog = config?.dialog;
  if (!dialog) throw new Error('Config is not a LLM dialog config');
  const type = dialogType ?? dialog.type ?? 'basic';
  if (!DIALOG_TYPES.includes(type)) {
    throw new Error(
      `Unknown dialog type '${type}' (supported: ${DIALOG_TYPES.join(', ')})`
    );
  }
  if (type !== 'basic' && type !== dialog.type && !dialog[type]) {
    throw new Error(
      `Config does not provide settings for dialog type '${type}'`
    );
  }
  let { engine } = dialog;
  const roles = MULTI_ENGINE_ROLES[type];
  if (roles) {
    if (
      !Array.isArray(engine) ||
      !roles.every((role) => engine.some((e: EngineConfig) => e.role === role))
    ) {
      throw new Error(
        `Dialog type '${type}' needs engines with roles: ${roles.join(', ')}`
      );
    }
  } else if (Array.isArray(engine)) {
    const main = engine.find(
      (e) => e.role === 'target' || e.role === 'primary'
    );
    if (!main) {
      throw new Error(
        `Config has no target or primary engine for dialog type '${type}'`
      );
    }
    // a single-engine dialog must not carry `role`
    engine = { ...main };
    delete engine.role;
  }
  const selected: ContextConfig['dialog'] = { ...dialog, type, engine };
  DIALOG_TYPES.filter((t) => t !== type).forEach((t) => delete selected[t]);
  return { ...config, dialog: selected };
};

const stopJson = (stop: string[]) => JSON.stringify({ 'stop-sequence': stop });

// Per-query values the dialog starts with, so a query that omits a param
// falls back to the config instead of whatever the previous query set.
// Sampler defaults mirror qualla's basic sampler when the config leaves a key unset.
export const configDefaults = (config: ContextConfig): NativeQueryParams => {
  const { dialog } = config;
  const defaults: NativeQueryParams = {
    prompt: '',
    stopJson: '{}',
    maxTokens: -1,
    temperature: 0.1,
    topK: 0,
    topP: 0.8,
    seed: -1,
    grammarKind: '',
    grammarContent: '',
    grammarFile: '',
  };
  if (Array.isArray(dialog['stop-sequence'])) {
    defaults.stopJson = stopJson(dialog['stop-sequence']);
  }
  if (typeof dialog['max-num-tokens'] === 'number') {
    defaults.maxTokens = dialog['max-num-tokens'];
  }
  const sampler = dialog.sampler ?? {};
  if (typeof sampler.temp === 'number') defaults.temperature = sampler.temp;
  if (typeof sampler['top-k'] === 'number') defaults.topK = sampler['top-k'];
  if (typeof sampler['top-p'] === 'number') defaults.topP = sampler['top-p'];
  if (typeof sampler.seed === 'number') defaults.seed = sampler.seed;
  const grammar = dialog.context?.grammar;
  // qualla only builds the init-time grammar when a backend is set
  if (
    typeof grammar?.backend === 'string' &&
    typeof grammar.file === 'string'
  ) {
    defaults.grammarKind =
      typeof grammar.type === 'string' ? grammar.type : 'json-schema';
    defaults.grammarFile = grammar.file;
  }
  return defaults;
};

export const resolveQueryParams = (
  params: QueryParams,
  defaults: NativeQueryParams
): NativeQueryParams => {
  if (typeof params?.prompt !== 'string') {
    throw new Error('params.prompt must be a string');
  }
  const resolved: NativeQueryParams = { ...defaults, prompt: params.prompt };
  if (Array.isArray(params.stop)) {
    resolved.stopJson = stopJson(params.stop);
  } else if (params.stop != null) {
    throw new Error('params.stop must be an array of strings');
  }
  // Like llama.rn: n_predict < 0 means no limit.
  if (typeof params.n_predict === 'number') {
    resolved.maxTokens =
      params.n_predict < 0 ? -1 : Math.floor(params.n_predict);
  } else if (params.n_predict != null) {
    throw new Error('params.n_predict must be a number');
  }
  if (typeof params.temperature === 'number')
    resolved.temperature = params.temperature;
  if (typeof params.top_k === 'number') resolved.topK = params.top_k;
  if (typeof params.top_p === 'number') resolved.topP = params.top_p;
  if (typeof params.seed === 'number') resolved.seed = params.seed;
  // Same precedence as llama.rn: grammar > json_schema; regex is Genie-only.
  const grammar = (kind: string, content: string) => {
    resolved.grammarKind = kind;
    resolved.grammarContent = content;
    resolved.grammarFile = '';
  };
  if (typeof params.grammar === 'string') {
    grammar('ebnf', params.grammar);
  } else if (typeof params.json_schema === 'string') {
    grammar('json-schema', params.json_schema);
  } else if (params.json_schema && typeof params.json_schema === 'object') {
    grammar('json-schema', JSON.stringify(params.json_schema));
  } else if (typeof params.regex === 'string') {
    grammar('regex', params.regex);
  }
  return resolved;
};

// Finds the most recent dialog query stat in Genie's profile JSON:
// { components: [{ events: [{ "num-prompt-tokens": { value }, ... }] }] }
const findQueryEvent = (profile: any): Record<string, any> | undefined => {
  let found: Record<string, any> | undefined;
  const components = profile?.components;
  if (!Array.isArray(components)) return found;
  for (const component of components) {
    if (!Array.isArray(component?.events)) continue;
    for (const event of component.events) {
      if (
        event &&
        typeof event === 'object' &&
        'num-generated-tokens' in event
      ) {
        found = event;
      }
    }
  }
  return found;
};

const eventValue = (event: Record<string, any> | undefined, name: string) => {
  const value = event?.[name]?.value;
  return typeof value === 'number' ? value : 0;
};

export const toQueryResult = (result: NativeQueryResult): QueryResult => {
  let profile: any;
  if (result.profile) {
    try {
      profile = JSON.parse(result.profile);
    } catch {
      profile = undefined;
    }
  }
  const event = findQueryEvent(profile);
  const prompt_n = eventValue(event, 'num-prompt-tokens');
  // profile times are in microseconds
  const prompt_ms = eventValue(event, 'time-to-first-token') / 1000;
  const predicted_n =
    result.numGenerated >= 0
      ? result.numGenerated
      : eventValue(event, 'num-generated-tokens');
  const predicted_ms = eventValue(event, 'token-generation-time') / 1000;
  return {
    text: result.text,
    tokens_evaluated: prompt_n,
    tokens_predicted: predicted_n,
    interrupted: result.interrupted,
    timings: {
      prompt_n,
      prompt_ms,
      prompt_per_token_ms: prompt_n > 0 ? prompt_ms / prompt_n : 0,
      prompt_per_second: eventValue(event, 'prompt-processing-rate'),
      predicted_n,
      predicted_ms,
      predicted_per_token_ms: predicted_n > 0 ? predicted_ms / predicted_n : 0,
      predicted_per_second: eventValue(event, 'token-generation-rate'),
    },
    profile,
  };
};

export interface PreProcessOptions {
  n_threads?: number;
  /** Built-in HTP backend extensions config, used when the bundle has none */
  htpConfigFilePath: string;
  /** Whether `path` exists */
  exists: (path: string) => Promise<boolean>;
  /** Whether running on Windows, `use-mmap` is not supported there */
  isWindows?: boolean;
}

const preProcessEngine = async (
  engine: EngineConfig,
  dir: string,
  { n_threads, htpConfigFilePath, exists, isWindows }: PreProcessOptions
) => {
  if (engine.backend.type === 'QnnHtp') {
    const extPath =
      typeof engine.backend.extensions === 'string'
        ? join(dir, engine.backend.extensions)
        : null;
    engine.backend.extensions =
      extPath && (await exists(extPath)) ? extPath : htpConfigFilePath;
    if (isWindows && engine.backend.QnnHtp) {
      engine.backend.QnnHtp['use-mmap'] = false;
    }
  }
  if (engine.model.type === 'binary' && engine.model.binary) {
    const bins = engine.model.binary['ctx-bins'];
    engine.model.binary['ctx-bins'] = bins.map((bin: string) => join(dir, bin));
  } else if (engine.model.library) {
    const bin = engine.model.library['model-bin'];
    engine.model.library['model-bin'] = join(dir, bin);
  }
  if (n_threads && n_threads > 0) engine['n-threads'] = n_threads;
};

// Resolves the paths of an unpacked bundle config against its directory
export const preProcessConfig = async (
  config: ContextConfig,
  dir: string,
  options: PreProcessOptions
) => {
  const model = config.dialog;
  if (model.embedding?.type === 'lut') {
    model.embedding['lut-path'] = join(dir, model.embedding['lut-path']);
  }
  if (model.lut) {
    model.lut['lut-path'] = join(dir, model.lut['lut-path']);
  }
  if (model.tokenizer) {
    model.tokenizer.path = join(dir, model.tokenizer.path);
  }
  const engines = Array.isArray(model.engine) ? model.engine : [model.engine];
  for (const engine of engines) {
    if (engine) await preProcessEngine(engine, dir, options);
  }
};
