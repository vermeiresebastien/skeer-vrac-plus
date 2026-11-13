import { useEffect, useState } from 'react';
import { Modal } from './Modal';

export function SavePresetModal({
  open,
  onClose,
  defaultName,
  onSave,
}: {
  open: boolean;
  onClose: () => void;
  defaultName: string;
  onSave: (name: string) => void;
}) {
  const [name, setName] = useState(defaultName);

  useEffect(() => {
    if (open) setName(defaultName === 'RANDOM KIT' ? 'MY KIT' : defaultName);
  }, [open, defaultName]);

  const submit = () => {
    if (!name.trim()) return;
    onSave(name.trim());
  };

  return (
    <Modal
      open={open}
      onClose={onClose}
      eyebrow="PRESETS"
      title="Save this kit"
      footer={
        <>
          <button type="button" className="button button--ghost" onClick={onClose}>
            CANCEL
          </button>
          <button type="button" className="button button--primary" onClick={submit} disabled={!name.trim()}>
            SAVE KIT
          </button>
        </>
      }
    >
      <label className="field">
        <span className="field-label">KIT NAME</span>
        <input
          type="text"
          value={name}
          maxLength={40}
          onChange={(event) => setName(event.currentTarget.value)}
          onKeyDown={(event) => {
            if (event.key === 'Enter') submit();
          }}
          autoFocus
          placeholder="MY KIT"
        />
      </label>
      <p className="form-note">
        Saved locally in this browser. Factory sources are rebuilt automatically, and a custom
        sample is stored in this browser's local sample store so the kit can be restored after a
        reload. To move a kit to another machine, export its WAVs.
      </p>
    </Modal>
  );
}
