import { useCallback, useEffect, useRef, useState } from 'react';
import type { SourceSample } from '../types';
import { decodeAudioBlob } from '../lib/audioFiles';
import { Modal } from './Modal';
import { Icon } from './Icons';
import { Waveform } from './Waveform';

type RecordStatus = 'idle' | 'connecting' | 'ready' | 'recording' | 'decoding' | 'recorded' | 'error';

function pickMimeType(): string | undefined {
  if (typeof MediaRecorder === 'undefined') return undefined;
  const candidates = ['audio/webm;codecs=opus', 'audio/webm', 'audio/mp4', 'audio/ogg;codecs=opus'];
  return candidates.find((type) => MediaRecorder.isTypeSupported(type));
}

export function RecordModal({
  open,
  onClose,
  onSample,
}: {
  open: boolean;
  onClose: () => void;
  onSample: (sample: SourceSample) => void;
}) {
  const [status, setStatus] = useState<RecordStatus>('idle');
  const [error, setError] = useState<string | null>(null);
  const [devices, setDevices] = useState<MediaDeviceInfo[]>([]);
  const [deviceId, setDeviceId] = useState<string>('');
  const [seconds, setSeconds] = useState(0);
  const [level, setLevel] = useState(0);
  const [recorded, setRecorded] = useState<SourceSample | null>(null);
  const streamRef = useRef<MediaStream | null>(null);
  const recorderRef = useRef<MediaRecorder | null>(null);
  const chunksRef = useRef<BlobPart[]>([]);
  const timerRef = useRef<number | null>(null);
  const audioCtxRef = useRef<AudioContext | null>(null);
  const analyserRef = useRef<AnalyserNode | null>(null);
  const rafRef = useRef<number>(0);
  const startedAtRef = useRef(0);

  const stopMeter = useCallback(() => {
    cancelAnimationFrame(rafRef.current);
  }, []);

  const meter = useCallback(() => {
    const analyser = analyserRef.current;
    if (!analyser) return;
    const data = new Float32Array(analyser.fftSize);
    analyser.getFloatTimeDomainData(data as Float32Array<ArrayBuffer>);
    let peak = 0;
    for (let i = 0; i < data.length; i++) {
      const value = Math.abs(data[i]);
      if (value > peak) peak = value;
    }
    setLevel(Math.min(1, peak * 2.4));
    rafRef.current = requestAnimationFrame(meter);
  }, []);

  const cleanupStream = useCallback(() => {
    stopMeter();
    if (timerRef.current) window.clearInterval(timerRef.current);
    timerRef.current = null;
    if (recorderRef.current && recorderRef.current.state !== 'inactive') {
      try {
        recorderRef.current.stop();
      } catch {
        /* already stopped */
      }
    }
    recorderRef.current = null;
    streamRef.current?.getTracks().forEach((track) => track.stop());
    streamRef.current = null;
    analyserRef.current = null;
    if (audioCtxRef.current && audioCtxRef.current.state !== 'closed') {
      void audioCtxRef.current.close();
    }
    audioCtxRef.current = null;
  }, [stopMeter]);

  useEffect(() => {
    if (!open) {
      cleanupStream();
      setStatus('idle');
      setError(null);
      setSeconds(0);
      setLevel(0);
      setRecorded(null);
      setDeviceId('');
      setDevices([]);
      return;
    }
    navigator.mediaDevices
      ?.enumerateDevices()
      .then((all) => setDevices(all.filter((device) => device.kind === 'audioinput')))
      .catch(() => undefined);
  }, [open, cleanupStream]);

  useEffect(() => cleanupStream, [cleanupStream]);

  const connect = async () => {
    setError(null);
    setStatus('connecting');
    // Whatever a previous attempt left behind is released before this one starts, and again below
    // if it fails partway. A MediaStream whose tracks were never stopped keeps the browser's
    // recording light on after the modal closes, and a second meter() would start another
    // requestAnimationFrame chain that stopMeter() can never cancel: rafRef only ever holds the
    // newest frame id, so the older chain keeps re-arming itself.
    cleanupStream();
    try {
      if (!navigator.mediaDevices?.getUserMedia) throw new Error('This browser does not expose audio inputs.');
      const stream = await navigator.mediaDevices.getUserMedia({
        audio: deviceId
          ? { deviceId: { exact: deviceId }, echoCancellation: false, noiseSuppression: false, autoGainControl: false }
          : { echoCancellation: false, noiseSuppression: false, autoGainControl: false },
      });
      streamRef.current = stream;
      const Ctor = window.AudioContext ?? (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
      const ctx = new Ctor();
      audioCtxRef.current = ctx;
      const analyser = ctx.createAnalyser();
      analyser.fftSize = 1024;
      analyser.smoothingTimeConstant = 0.7;
      const source = ctx.createMediaStreamSource(stream);
      source.connect(analyser);
      analyserRef.current = analyser;
      if (ctx.state === 'suspended') await ctx.resume();
      const all = await navigator.mediaDevices.enumerateDevices();
      setDevices(all.filter((device) => device.kind === 'audioinput'));
      setStatus('ready');
      meter();
    } catch (err) {
      cleanupStream();
      console.error('[SliceForge] input failed', err);
      setError(err instanceof Error ? err.message : 'Could not open the input');
      setStatus('error');
    }
  };

  const startRecording = () => {
    const stream = streamRef.current;
    if (!stream) return;
    setError(null);
    setRecorded(null);
    chunksRef.current = [];
    const mimeType = pickMimeType();
    let recorder: MediaRecorder;
    try {
      recorder = new MediaRecorder(stream, mimeType ? { mimeType } : undefined);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Recording is not supported by this browser.');
      setStatus('error');
      return;
    }
    recorder.ondataavailable = (event) => {
      if (event.data.size > 0) chunksRef.current.push(event.data);
    };
    recorder.onstop = async () => {
      const blob = new Blob(chunksRef.current, { type: recorder.mimeType || 'audio/webm' });
      setStatus('decoding');
      try {
        const sample = await decodeAudioBlob(blob, 'RECORDED INPUT');
        setRecorded(sample);
        setStatus('recorded');
      } catch (err) {
        console.error('[SliceForge] recording decode failed', err);
        setError('The recording could not be decoded. Try a different input or browser.');
        setStatus('error');
      }
    };
    recorder.start(100);
    recorderRef.current = recorder;
    setStatus('recording');
    setSeconds(0);
    startedAtRef.current = performance.now();
    if (timerRef.current) window.clearInterval(timerRef.current);
    timerRef.current = window.setInterval(() => {
      const elapsed = (performance.now() - startedAtRef.current) / 1000;
      setSeconds(elapsed);
      if (elapsed >= 120) stopRecording();
    }, 100);
  };

  const stopRecording = () => {
    if (timerRef.current) window.clearInterval(timerRef.current);
    timerRef.current = null;
    const recorder = recorderRef.current;
    if (recorder && recorder.state !== 'inactive') recorder.stop();
  };

  const close = () => {
    cleanupStream();
    onClose();
  };

  const useRecording = () => {
    if (!recorded) return;
    onSample(recorded);
    cleanupStream();
    onClose();
  };

  return (
    <Modal
      open={open}
      onClose={close}
      eyebrow="RECORD IN"
      title="Capture an input"
      footer={
        <>
          <span className="modal-foot-note">Up to 2 minutes · recorded and decoded locally</span>
          <button type="button" className="button button--ghost" onClick={close}>
            CANCEL
          </button>
          <button
            type="button"
            className="button button--primary"
            onClick={useRecording}
            disabled={!recorded}
          >
            USE RECORDING
          </button>
        </>
      }
    >
      <p className="modal-lead">
        Route a microphone, instrument or DAW output into the browser and capture it as your next
        kit source.
      </p>

      <div className="record-device-row">
        <label className="field field--inline">
          <span className="field-label">INPUT</span>
          <select
            value={deviceId}
            onChange={(event) => {
              setDeviceId(event.currentTarget.value);
              if (status === 'ready') void connect();
            }}
            disabled={status === 'recording' || status === 'decoding'}
          >
            <option value="">System default input</option>
            {devices.map((device, index) => (
              <option value={device.deviceId} key={device.deviceId || index}>
                {device.label || `Audio input ${index + 1}`}
              </option>
            ))}
          </select>
        </label>
        {status === 'idle' || status === 'error' ? (
          <button type="button" className="button button--ghost" onClick={connect}>
            <Icon name="power" size={14} /> CONNECT INPUT
          </button>
        ) : (
          <span className="record-connected">
            <span className="status-led" /> INPUT CONNECTED
          </span>
        )}
      </div>

      <div className={`record-deck ${status === 'recording' ? 'is-recording' : ''}`}>
        <button
          type="button"
          className="record-button"
          disabled={!['ready', 'recording', 'recorded'].includes(status)}
          onClick={status === 'recording' ? stopRecording : startRecording}
          title={status === 'recording' ? 'Stop recording' : 'Start recording'}
        >
          <span className="record-button-inner" />
        </button>
        <div className="record-readout">
          <span className="record-timer">{formatTimer(seconds)}</span>
          <span className="record-state">
            {status === 'recording'
              ? 'RECORDING'
              : status === 'decoding'
                ? 'DECODING…'
                : status === 'recorded'
                  ? 'TAKE READY'
                  : status === 'connecting'
                    ? 'CONNECTING…'
                    : status === 'error'
                      ? 'INPUT ERROR'
                      : 'ARMED'}
          </span>
        </div>
        <div className="record-level" aria-label="Input level">
          {Array.from({ length: 24 }).map((_, index) => (
            <span
              key={index}
              className={`record-level-seg ${level * 24 > index ? 'is-on' : ''} ${index > 18 ? 'is-hot' : ''}`}
            />
          ))}
        </div>
      </div>

      {recorded && (
        <div className="record-preview">
          <div className="record-preview-head">
            <span className="mini-label">TAKE · {recorded.duration.toFixed(1)}s</span>
            <button
              type="button"
              className="text-button"
              onClick={() => {
                setRecorded(null);
                setStatus('ready');
              }}
            >
              Record again
            </button>
          </div>
          <Waveform sample={recorded} slices={[]} start={0} compact ariaLabel="Recorded audio" />
        </div>
      )}

      {error && (
        <div className="inline-error">
          <Icon name="info" size={14} /> {error}
        </div>
      )}

      <div className="record-tips">
        <Icon name="info" size={14} />
        <p>
          Use headphones to avoid feedback when recording a microphone. For a DAW, route the track
          to a loopback device or use your interface's virtual output. The input is decoded to a
          sample and auto-sliced on load.
        </p>
      </div>
    </Modal>
  );
}

function formatTimer(seconds: number): string {
  const whole = Math.floor(seconds);
  const tenths = Math.floor((seconds - whole) * 10);
  return `00:${String(whole).padStart(2, '0')}.${tenths}`;
}
