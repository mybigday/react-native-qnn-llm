import {
  configDefaults,
  preProcessConfig,
  resolveQueryParams,
  selectDialogType,
  toQueryResult,
} from '../utils';
import type { ContextConfig } from '../types';

const engine = (role?: string) => ({
  'version': 1,
  'n-threads': 4,
  ...(role ? { role } : {}),
  'backend': { type: 'QnnHtp' as const },
  'model': { version: 1, type: 'binary' as const },
});

const baseConfig = (dialog: Partial<ContextConfig['dialog']> = {}) =>
  ({
    dialog: {
      version: 1,
      type: 'basic',
      context: { version: 1 },
      sampler: { version: 1 },
      tokenizer: { version: 1, path: 'tokenizer.json' },
      engine: engine(),
      ...dialog,
    },
  }) as ContextConfig;

describe('selectDialogType', () => {
  it('drops settings for other dialog types', () => {
    const config = baseConfig({ type: 'lade', lade: { x: 1 }, spd: { y: 1 } });
    const { dialog } = selectDialogType(config, 'basic');
    expect(dialog.type).toBe('basic');
    expect(dialog.lade).toBeUndefined();
    expect(dialog.spd).toBeUndefined();
  });

  it('keeps settings for the chosen type', () => {
    const { dialog } = selectDialogType(baseConfig({ lade: { x: 1 } }), 'lade');
    expect(dialog.type).toBe('lade');
    expect(dialog.lade).toEqual({ x: 1 });
  });

  it('throws for an unprovided type', () => {
    expect(() => selectDialogType(baseConfig(), 'lade')).toThrow(
      "Config does not provide settings for dialog type 'lade'"
    );
  });

  it('picks the target engine for basic from a two-engine config', () => {
    const config = baseConfig({
      type: 'spd',
      spd: {},
      engine: [engine('draft'), engine('target')],
    });
    const { dialog } = selectDialogType(config, 'basic');
    expect(Array.isArray(dialog.engine)).toBe(false);
    expect((dialog.engine as any).role).toBeUndefined();
  });

  it('requires engine roles for multi-engine types', () => {
    const config = baseConfig({ type: 'spd', spd: {} });
    expect(() => selectDialogType(config)).toThrow('needs engines with roles');
  });
});

describe('query params', () => {
  const config = baseConfig({
    'stop-sequence': ['<|eot_id|>'],
    'max-num-tokens': 200,
    'sampler': { 'version': 1, 'temp': 0.5, 'top-k': 40 },
    'context': {
      version: 1,
      grammar: { backend: 'xgrammar', file: '/g.json' },
    },
  });
  const defaults = configDefaults(config);

  it('reads defaults from the config', () => {
    expect(defaults).toEqual({
      prompt: '',
      stopJson: '{"stop-sequence":["<|eot_id|>"]}',
      maxTokens: 200,
      temperature: 0.5,
      topK: 40,
      topP: 0.8,
      seed: -1,
      grammarKind: 'json-schema',
      grammarContent: '',
      grammarFile: '/g.json',
    });
  });

  it('falls back to defaults for omitted params', () => {
    expect(resolveQueryParams({ prompt: 'hi' }, defaults)).toEqual({
      ...defaults,
      prompt: 'hi',
    });
  });

  it('overrides per query', () => {
    const resolved = resolveQueryParams(
      {
        prompt: 'hi',
        stop: ['x'],
        n_predict: -5,
        temperature: 0,
        seed: 42,
        json_schema: { type: 'object' },
        regex: 'ignored',
      },
      defaults
    );
    expect(resolved).toMatchObject({
      stopJson: '{"stop-sequence":["x"]}',
      maxTokens: -1,
      temperature: 0,
      topK: 40,
      seed: 42,
      grammarKind: 'json-schema',
      grammarContent: '{"type":"object"}',
      grammarFile: '',
    });
  });

  it('gives grammar precedence over json_schema', () => {
    const resolved = resolveQueryParams(
      { prompt: 'hi', grammar: 'root ::= "a"', json_schema: '{}' },
      defaults
    );
    expect(resolved.grammarKind).toBe('ebnf');
    expect(resolved.grammarContent).toBe('root ::= "a"');
  });

  it('validates params', () => {
    expect(() => resolveQueryParams({} as any, defaults)).toThrow(
      'params.prompt must be a string'
    );
    expect(() =>
      resolveQueryParams({ prompt: 'hi', stop: 'x' as any }, defaults)
    ).toThrow('params.stop must be an array of strings');
  });
});

describe('toQueryResult', () => {
  it('computes timings from the profile', () => {
    const profile = {
      components: [
        {
          events: [
            { type: 'create' },
            {
              'num-prompt-tokens': { value: 10 },
              'time-to-first-token': { value: 200000 },
              'prompt-processing-rate': { value: 50 },
              'num-generated-tokens': { value: 20 },
              'token-generation-time': { value: 1000000 },
              'token-generation-rate': { value: 20 },
            },
          ],
        },
      ],
    };
    const result = toQueryResult({
      text: 'hello',
      profile: JSON.stringify(profile),
      interrupted: false,
      numGenerated: 21,
    });
    expect(result).toMatchObject({
      text: 'hello',
      tokens_evaluated: 10,
      tokens_predicted: 21,
      interrupted: false,
      timings: {
        prompt_n: 10,
        prompt_ms: 200,
        prompt_per_token_ms: 20,
        prompt_per_second: 50,
        predicted_n: 21,
        predicted_ms: 1000,
        predicted_per_second: 20,
      },
    });
  });

  it('handles a missing profile', () => {
    const result = toQueryResult({
      text: '',
      profile: '',
      interrupted: true,
      numGenerated: -1,
    });
    expect(result.tokens_predicted).toBe(0);
    expect(result.profile).toBeUndefined();
  });
});

describe('preProcessConfig', () => {
  const bundleConfig = (extensions?: string) =>
    baseConfig({
      engine: {
        ...engine(),
        backend: {
          type: 'QnnHtp',
          QnnHtp: { 'use-mmap': true },
          ...(extensions ? { extensions } : {}),
        },
        model: {
          version: 1,
          type: 'binary',
          binary: { 'version': 1, 'ctx-bins': ['a.bin', 'b.bin'] },
        },
      },
    });
  const options = (files: string[] = []) => ({
    htpConfigFilePath: '/builtin/htp_config.json',
    exists: async (path: string) => files.includes(path),
  });

  it('resolves paths against the unpack dir', async () => {
    const config = bundleConfig();
    await preProcessConfig(config, '/unpack', { ...options(), n_threads: 2 });
    const e = config.dialog.engine as any;
    expect(config.dialog.tokenizer.path).toBe('/unpack/tokenizer.json');
    expect(e.model.binary['ctx-bins']).toEqual([
      '/unpack/a.bin',
      '/unpack/b.bin',
    ]);
    expect(e['n-threads']).toBe(2);
    expect(e.backend.QnnHtp['use-mmap']).toBe(true);
  });

  it('prefers the extensions config shipped in the bundle', async () => {
    const config = bundleConfig('htp_ext.json');
    await preProcessConfig(
      config,
      '/unpack',
      options(['/unpack/htp_ext.json'])
    );
    expect((config.dialog.engine as any).backend.extensions).toBe(
      '/unpack/htp_ext.json'
    );
  });

  it('falls back to the built-in extensions config', async () => {
    for (const config of [bundleConfig('htp_ext.json'), bundleConfig()]) {
      await preProcessConfig(config, '/unpack', options());
      expect((config.dialog.engine as any).backend.extensions).toBe(
        '/builtin/htp_config.json'
      );
    }
  });

  it('disables mmap on Windows', async () => {
    const config = bundleConfig();
    await preProcessConfig(config, '/unpack', {
      ...options(),
      isWindows: true,
    });
    expect((config.dialog.engine as any).backend.QnnHtp['use-mmap']).toBe(
      false
    );
  });
});
