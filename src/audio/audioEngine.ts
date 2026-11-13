/* Main-thread audio host: owns the AudioContext, the worklet node, the eight
 * stereo buses and the global master chain. The React UI talks to this class
 * and never touches Web Audio directly. */

import type { GlobalParams, PadParams, SourceSample } from '../types';
import { clamp } from '../types';
import { MasterChain } from './masterChain';
import type { FromEngineMessage, ToEngineMessage } from './protocol';
import { BUS_COUNT } from './engineCore';

export type AudioStatus = 'idle' | 'starting' | 'ready' | 'suspended' | 'error';

type StatusListener = (status: AudioStatus, detail?: string) => void;
type MeterListener = (buses: number[]) => void;

const WORKLET_URL = `${import.meta.env.BASE_URL}vrac-worklet.js`;

export class AudioEngine {
  private ctx: AudioContext | null = null;
  private node: AudioWorkletNode | null = null;
  private chain: MasterChain | null = null;
  private busGains: GainNode[] = [];
  private analyser: AnalyserNode | null = null;
  private analyserData = new Float32Array(1024);
  private startPromise: Promise<void> | null = null;
  private latestPads: PadParams[] | null = null;
  private latestGlobal: GlobalParams | null = null;
  private latestSample: SourceSample | null = null;
  private pendingTriggers: Array<{ pad: number; velocity: number }> = [];
  private pendingAuditions: Array<{ pad: PadParams; velocity: number }> = [];
  private disposed = false;
  private listeners = new Set<StatusListener>();
  private meterListeners = new Set<MeterListener>();

  status: AudioStatus = 'idle';
  lastError: string | null = null;

  get sampleRate(): number {
    return this.ctx?.sampleRate ?? 48000;
  }

  get isReady(): boolean {
    return this.status === 'ready';
  }

  onStatus(listener: StatusListener): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  onMeter(listener: MeterListener): () => void {
    this.meterListeners.add(listener);
    return () => this.meterListeners.delete(listener);
  }

  private setStatus(status: AudioStatus, detail?: string): void {
    this.status = status;
    this.lastError = status === 'error' ? detail ?? 'Audio failed to start' : null;
    for (const listener of this.listeners) listener(status, detail);
  }

  async start(): Promise<void> {
    if (this.status === 'ready') {
      if (this.ctx?.state !== 'running') {
        try {
          await this.ctx?.resume();
        } catch {
          /* ignored — the next gesture will try again */
        }
      }
      return;
    }
    if (this.startPromise) return this.startPromise;

    this.startPromise = this.doStart().finally(() => {
      this.startPromise = null;
    });
    return this.startPromise;
  }

  private async doStart(): Promise<void> {
    if (this.disposed) return;
    this.setStatus('starting');
    try {
      const Ctor = window.AudioContext ?? (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
      let ctx: AudioContext;
      try {
        ctx = new Ctor({ sampleRate: 48000, latencyHint: 'interactive' });
      } catch {
        ctx = new Ctor({ latencyHint: 'interactive' });
      }
      this.ctx = ctx;

      await ctx.audioWorklet.addModule(WORKLET_URL);
      if (this.disposed) {
        await ctx.close();
        return;
      }

      this.chain = new MasterChain(ctx);
      this.analyser = ctx.createAnalyser();
      this.analyser.fftSize = 1024;
      this.analyser.smoothingTimeConstant = 0.75;
      this.chain.connect(this.analyser);
      this.analyser.connect(ctx.destination);

      const node = new AudioWorkletNode(ctx, 'vrac-engine', {
        numberOfInputs: 0,
        numberOfOutputs: BUS_COUNT,
        outputChannelCount: new Array(BUS_COUNT).fill(2) as number[],
      });
      this.node = node;
      this.busGains = [];
      for (let bus = 0; bus < BUS_COUNT; bus++) {
        const gain = ctx.createGain();
        gain.gain.value = 1;
        this.node.connect(gain, bus);
        gain.connect(this.chain.input);
        this.busGains.push(gain);
      }

      node.port.onmessage = (event: MessageEvent<FromEngineMessage>) => {
        const msg = event.data;
        if (msg.type === 'meter') {
          for (const listener of this.meterListeners) listener(msg.buses);
        }
      };

      if (this.latestSample) this.postSample(this.latestSample);
      if (this.latestPads && this.latestGlobal) {
        this.postParams(this.latestPads, this.latestGlobal);
        this.chain.setGlobal(this.latestGlobal, true);
      }
      this.flushPending();

      if (ctx.state === 'suspended') {
        await ctx.resume();
      }
      this.setStatus('ready');
      ctx.onstatechange = () => {
        if (!this.ctx) return;
        if (this.ctx.state === 'running' && this.status === 'suspended') this.setStatus('ready');
        if (this.ctx.state === 'suspended' && this.status === 'ready') this.setStatus('suspended');
      };
    } catch (error) {
      console.error('[VRAC+] audio failed to start', error);
      this.setStatus('error', error instanceof Error ? error.message : String(error));
    }
  }

  private postSample(sample: SourceSample): void {
    if (!this.node) return;
    const message: ToEngineMessage = {
      type: 'sample',
      left: sample.left,
      right: sample.right,
      sampleRate: sample.sampleRate,
    };
    try {
      this.node.port.postMessage(message);
    } catch (error) {
      console.error('[VRAC+] could not send sample to worklet', error);
    }
  }

  setSample(sample: SourceSample): void {
    this.latestSample = sample;
    if (this.node) this.postSample(sample);
  }

  setParams(pads: PadParams[], global: GlobalParams): void {
    this.latestPads = pads;
    this.latestGlobal = global;
    if (this.chain) this.chain.setGlobal(global);
    if (this.node) this.postParams(pads, global);
  }

  private postParams(pads: PadParams[], global: GlobalParams): void {
    if (!this.node) return;
    const message: ToEngineMessage = { type: 'params', pads, global };
    this.node.port.postMessage(message);
  }

  /** Re-send the newest state immediately before a trigger so mute/solo and
   * the current knob positions are guaranteed to be in effect for this hit. */
  private syncParams(): void {
    if (this.node && this.latestPads && this.latestGlobal) {
      this.postParams(this.latestPads, this.latestGlobal);
    }
  }

  trigger(pad: number, velocity = 0.85): void {
    if (this.status === 'idle' || this.status === 'error') {
      this.pendingTriggers.push({ pad, velocity });
      void this.start();
      return;
    }
    if (this.status === 'suspended') void this.ctx?.resume();
    if (this.node) {
      this.syncParams();
      const message: ToEngineMessage = { type: 'trigger', pad, velocity };
      this.node.port.postMessage(message);
    } else {
      this.pendingTriggers.push({ pad, velocity });
    }
  }

  audition(pad: PadParams, velocity = 0.85): void {
    if (this.status === 'idle' || this.status === 'error') {
      this.pendingAuditions.push({ pad, velocity });
      void this.start();
      return;
    }
    if (this.status === 'suspended') void this.ctx?.resume();
    if (this.node) {
      this.syncParams();
      const message: ToEngineMessage = { type: 'audition', pad, velocity };
      this.node.port.postMessage(message);
    }
  }

  allOff(): void {
    if (this.node) {
      const message: ToEngineMessage = { type: 'allOff', fast: true };
      this.node.port.postMessage(message);
    }
  }

  private flushPending(): void {
    if (!this.node) return;
    for (const item of this.pendingTriggers) {
      const message: ToEngineMessage = { type: 'trigger', pad: item.pad, velocity: item.velocity };
      this.node.port.postMessage(message);
    }
    this.pendingTriggers = [];
    for (const item of this.pendingAuditions) {
      const message: ToEngineMessage = { type: 'audition', pad: item.pad, velocity: item.velocity };
      this.node.port.postMessage(message);
    }
    this.pendingAuditions = [];
  }

  /** Peak of the final stereo mix, 0..1. */
  getOutputLevel(): number {
    if (!this.analyser || !this.ctx || this.ctx.state === 'suspended') return 0;
    const data = this.analyserData;
    this.analyser.getFloatTimeDomainData(data as Float32Array<ArrayBuffer>);
    let peak = 0;
    for (let i = 0; i < data.length; i++) {
      const v = Math.abs(data[i]);
      if (v > peak) peak = v;
    }
    return clamp(peak, 0, 1.2);
  }

  async suspend(): Promise<void> {
    this.allOff();
    if (this.ctx && this.ctx.state === 'running') {
      await this.ctx.suspend();
    }
  }

  dispose(): void {
    this.disposed = true;
    this.allOff();
    this.listeners.clear();
    this.meterListeners.clear();
    void this.ctx?.close();
    this.ctx = null;
    this.node = null;
    this.chain = null;
  }
}

export const audioEngine = new AudioEngine();
