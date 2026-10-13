import type { TurboModule } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

// Fully resolved per-query params; omitted query params are filled from the config.
export type NativeQueryParams = {
  prompt: string;
  stopJson: string;
  // < 0 means no limit
  maxTokens: number;
  temperature: number;
  topK: number;
  topP: number;
  seed: number;
  // '' means no grammar
  grammarKind: string;
  grammarContent: string;
  grammarFile: string;
};

export type NativeQueryResult = {
  text: string;
  profile: string;
  interrupted: boolean;
  numGenerated: number;
};

export interface Spec extends TurboModule {
  createContext(
    config: string,
    dialogType: string,
    defaults: NativeQueryParams
  ): Promise<number>;
  unpack(bundlePath: string, unpackDir: string): Promise<string>;
  exists(path: string): Promise<boolean>;
  freeContext(context: number): Promise<void>;
  process(context: number, input: string): Promise<void>;
  query(context: number, params: NativeQueryParams): Promise<NativeQueryResult>;
  saveSession(context: number, filename: string): Promise<void>;
  restoreSession(context: number, filename: string): Promise<void>;
  abort(context: number): Promise<void>;
  applyLora(
    context: number,
    engine: string,
    loraAdapterName: string
  ): Promise<void>;
  setLoraStrength(
    context: number,
    engine: string,
    // { [tensorName]: alpha }
    strengths: Object
  ): Promise<void>;
  releaseLoraMemory(
    context: number,
    engine: string,
    loraAdapterName: string
  ): Promise<void>;
  getContextOccupancy(context: number): Promise<number>;
  getAppliedLoraAdapter(context: number): Promise<string>;
}

export default TurboModuleRegistry.getEnforcing<Spec>('QnnLlm');
