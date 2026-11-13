import type { GlobalParams, PadParams } from '../types';

/** Messages from the UI thread to the audio worklet. */
export type ToEngineMessage =
  | { type: 'sample'; left: Float32Array; right: Float32Array; sampleRate: number }
  | { type: 'params'; pads: PadParams[]; global: GlobalParams }
  | { type: 'trigger'; pad: number; velocity: number; delayFrames?: number }
  | { type: 'audition'; pad: PadParams; velocity: number }
  | { type: 'allOff'; fast?: boolean };

/** Messages from the audio worklet back to the UI. */
export type FromEngineMessage = {
  type: 'meter';
  buses: number[];
  voices: number;
};
