import type { TurboModule } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

export interface Spec extends TurboModule {
  createContext(config: string): Promise<number>;
  unpack(bundlePath: string, unpackDir: string): Promise<string>;
  freeContext(context: number): Promise<void>;
  process(context: number, input: string): Promise<void>;
  query(context: number, input: string): Promise<string>;
  setStopWords(context: number, stopWords: string): Promise<void>;
  applySamplerConfig(context: number, config: string): Promise<void>;
  saveSession(context: number, filename: string): Promise<void>;
  restoreSession(context: number, filename: string): Promise<void>;
  abort(context: number): Promise<void>;
}

export default TurboModuleRegistry.getEnforcing<Spec>('QnnLlm');
