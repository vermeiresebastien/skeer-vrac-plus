import { useEffect, useState } from 'react';
import { audioEngine } from '../audio/audioEngine';

export function OutputMeters() {
  const [buses, setBuses] = useState<number[]>(new Array(8).fill(0));
  const [master, setMaster] = useState(0);

  useEffect(() => {
    const unsubscribe = audioEngine.onMeter((next) => {
      setBuses(next);
    });
    let raf = 0;
    let last = 0;
    const loop = (time: number) => {
      if (time - last > 55) {
        last = time;
        setMaster(audioEngine.getOutputLevel());
      }
      raf = requestAnimationFrame(loop);
    };
    raf = requestAnimationFrame(loop);
    return () => {
      unsubscribe();
      cancelAnimationFrame(raf);
    };
  }, []);

  return (
    <div className="output-strip" aria-label="Output and master meters">
      <div className="output-strip-label">
        <span>8 STEREO OUTS</span>
        <span className="output-voices">{buses.filter((v) => v > 0.002).length}/8 ACTIVE</span>
      </div>
      <div className="bus-meters">
        {buses.map((level, index) => (
          <div className="bus-meter" key={index} title={`Output ${index + 1}`}>
            <span className="bus-number">{String(index + 1).padStart(2, '0')}</span>
            <span className="bus-track">
              <span
                className="bus-fill"
                style={{ width: `${Math.min(100, Math.max(0, level * 130))}%` }}
              />
            </span>
          </div>
        ))}
      </div>
      <div className="master-meter" title="Master output">
        <span className="master-label">MIX</span>
        <span className="master-track">
          <span
            className="master-fill"
            style={{ width: `${Math.min(100, master * 118)}%` }}
          />
        </span>
        <span className="master-peak">{master > 0.002 ? `${Math.round(20 * Math.log10(master))} dB` : '— dB'}</span>
      </div>
    </div>
  );
}
