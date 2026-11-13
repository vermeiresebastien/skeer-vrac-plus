/* AudioWorklet entry point — bundled by scripts/build-worklet.mjs. */

import { VracEngine } from './engineCore';
import type { FromEngineMessage, ToEngineMessage } from './protocol';

declare const sampleRate: number;

declare class AudioWorkletProcessor {
  readonly port: MessagePort;
  constructor();
}

declare function registerProcessor(
  name: string,
  processorCtor: new () => AudioWorkletProcessor & {
    process(
      inputs: Float32Array[][],
      outputs: Float32Array[][],
      parameters: Record<string, Float32Array>,
    ): boolean;
  },
): void;

class VracProcessor extends AudioWorkletProcessor {
  private readonly engine: VracEngine;

  constructor() {
    super();
    this.engine = new VracEngine(sampleRate);
    this.port.onmessage = (event: MessageEvent<ToEngineMessage>) => {
      const msg = event.data;
      switch (msg.type) {
        case 'sample':
          this.engine.setSample(msg.left, msg.right, msg.sampleRate);
          break;
        case 'params':
          this.engine.setParams({ pads: msg.pads, global: msg.global });
          break;
        case 'trigger':
          this.engine.trigger(msg.pad, msg.velocity, msg.delayFrames ?? 0);
          break;
        case 'audition':
          this.engine.audition(msg.pad, msg.velocity);
          break;
        case 'allOff':
          this.engine.allOff(msg.fast ?? true);
          break;
      }
    };
  }

  process(
    _inputs: Float32Array[][],
    outputs: Float32Array[][],
    _parameters: Record<string, Float32Array>,
  ): boolean {
    this.engine.process(outputs);
    if (this.engine.consumeMeterDue()) {
      const meter: FromEngineMessage = {
        type: 'meter',
        buses: this.engine.getMeter().buses,
        voices: this.engine.activeVoiceCount,
      };
      this.port.postMessage(meter);
    }
    return true;
  }
}

registerProcessor('vrac-engine', VracProcessor);
