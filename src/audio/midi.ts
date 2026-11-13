/* Web MIDI: pad triggering, controller learn and mapping. */

export interface MidiInputInfo {
  id: string;
  name: string;
  manufacturer: string;
  state: string;
}

type NoteHandler = (padIndex: number, velocity: number) => void;
type CcHandler = (cc: number, value: number) => void;
type LearnHandler = (cc: number) => void;
type StateHandler = (status: MidiStatus, inputs: MidiInputInfo[], message?: string) => void;

export type MidiStatus = 'off' | 'requesting' | 'ready' | 'error' | 'unsupported';

interface WebMidiInput {
  id: string;
  name?: string | null;
  manufacturer?: string | null;
  state: string;
  onmidimessage: ((event: { data: Uint8Array | null }) => void) | null;
}

interface WebMidiAccess {
  inputs: Map<string, WebMidiInput>;
  onstatechange: ((event: unknown) => void) | null;
}

interface WebMidiOptions {
  sysex?: boolean;
}

type RequestMidiAccess = (options?: WebMidiOptions) => Promise<WebMidiAccess>;

function getRequestMidiAccess(): RequestMidiAccess | null {
  if (typeof navigator === 'undefined') return null;
  const midi = (navigator as unknown as { requestMIDIAccess?: RequestMidiAccess }).requestMIDIAccess;
  return midi ? midi.bind(navigator) : null;
}

export class MidiHub {
  private access: WebMidiAccess | null = null;
  status: MidiStatus = 'off';
  inputs: MidiInputInfo[] = [];
  onNote: NoteHandler | null = null;
  onCC: CcHandler | null = null;
  onLearn: LearnHandler | null = null;
  onState: StateHandler | null = null;
  learnTarget: string | null = null;
  learnArmedAt = 0;

  get isSupported(): boolean {
    return getRequestMidiAccess() !== null;
  }

  async enable(): Promise<boolean> {
    if (this.status === 'ready') {
      this.refreshInputs();
      return true;
    }
    const request = getRequestMidiAccess();
    if (!request) {
      this.setStatus('unsupported', 'Web MIDI is not available in this browser.');
      return false;
    }
    this.setStatus('requesting');
    try {
      this.access = await request({ sysex: false });
      this.access.onstatechange = () => this.refreshInputs();
      this.refreshInputs();
      this.setStatus('ready');
      return true;
    } catch (error) {
      this.setStatus('error', error instanceof Error ? error.message : 'MIDI access denied');
      return false;
    }
  }

  private refreshInputs(): void {
    if (!this.access) return;
    this.inputs = [];
    this.access.inputs.forEach((input) => {
      this.inputs.push({
        id: input.id,
        name: input.name || 'MIDI input',
        manufacturer: input.manufacturer || '',
        state: input.state,
      });
      input.onmidimessage = (event) => this.handleMessage(event.data);
    });
    this.onState?.(this.status, this.inputs);
  }

  private handleMessage(data: Uint8Array | null): void {
    if (!data || data.length < 2) return;
    const status = data[0] & 0xf0;
    if (status === 0x90 && data.length >= 3) {
      const note = data[1];
      const velocity = data[2] / 127;
      if (velocity > 0 && note >= 36 && note <= 51) {
        this.onNote?.(note - 36, Math.max(0.05, velocity));
      }
    } else if (status === 0xb0 && data.length >= 3) {
      const cc = data[1];
      const value = data[2] / 127;
      if (this.learnTarget) {
        this.learnTarget = null;
        this.onLearn?.(cc);
      } else {
        this.onCC?.(cc, value);
      }
    }
  }

  armLearn(target: string): void {
    this.learnTarget = target;
    this.learnArmedAt = Date.now();
  }

  clearLearn(): void {
    this.learnTarget = null;
  }

  private setStatus(status: MidiStatus, message?: string): void {
    this.status = status;
    this.onState?.(status, this.inputs, message);
  }
}

export const midiHub = new MidiHub();
