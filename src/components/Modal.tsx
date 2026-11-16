import { useEffect, useRef, type ReactNode } from 'react';
import { createPortal } from 'react-dom';
import { Icon } from './Icons';

export interface ModalProps {
  open: boolean;
  onClose: () => void;
  eyebrow?: string;
  title: string;
  children: ReactNode;
  footer?: ReactNode;
  wide?: boolean;
  className?: string;
}

export function Modal({ open, onClose, eyebrow, title, children, footer, wide = false, className = '' }: ModalProps) {
  const dialogRef = useRef<HTMLDivElement | null>(null);
  // Held in a ref so the effect below can depend on `open` alone. onClose is an inline arrow in
  // most callers, so it is a new function on every render: as a dependency it re-ran this whole
  // effect on each keystroke inside the modal, and the 30 ms autofocus timer re-fired with it —
  // the caret jumped back into the dialog while the user was still typing.
  const closeRef = useRef(onClose);
  closeRef.current = onClose;

  useEffect(() => {
    if (!open) return;
    const previousFocus = document.activeElement as HTMLElement | null;
    const previousOverflow = document.body.style.overflow;
    document.body.style.overflow = 'hidden';
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        event.preventDefault();
        closeRef.current();
      }
      if (event.key === 'Tab' && dialogRef.current) {
        const focusables = Array.from(
          dialogRef.current.querySelectorAll<HTMLElement>(
            'button:not([disabled]), [href], input:not([disabled]), select:not([disabled]), textarea:not([disabled]), [tabindex]:not([tabindex="-1"])',
          ),
        );
        if (focusables.length === 0) return;
        const first = focusables[0];
        const last = focusables[focusables.length - 1];
        if (event.shiftKey && document.activeElement === first) {
          event.preventDefault();
          last.focus();
        } else if (!event.shiftKey && document.activeElement === last) {
          event.preventDefault();
          first.focus();
        }
      }
    };
    document.addEventListener('keydown', onKeyDown);
    const timer = window.setTimeout(() => {
      const target = dialogRef.current?.querySelector<HTMLElement>('[data-autofocus]') ?? dialogRef.current;
      target?.focus();
    }, 30);
    return () => {
      document.removeEventListener('keydown', onKeyDown);
      document.body.style.overflow = previousOverflow;
      window.clearTimeout(timer);
      previousFocus?.focus?.();
    };
  }, [open]);

  if (!open) return null;

  return createPortal(
    <div
      className="modal-backdrop"
      onMouseDown={(event) => {
        if (event.target === event.currentTarget) onClose();
      }}
    >
      <div
        ref={dialogRef}
        className={`modal ${wide ? 'modal--wide' : ''} ${className}`}
        role="dialog"
        aria-modal="true"
        aria-label={title}
        tabIndex={-1}
      >
        <header className="modal-head">
          <div>
            {eyebrow && <span className="modal-eyebrow">{eyebrow}</span>}
            <h2 className="modal-title">{title}</h2>
          </div>
          <button type="button" className="icon-button modal-close" onClick={onClose} aria-label="Close">
            <Icon name="close" size={16} />
          </button>
        </header>
        <div className="modal-body">{children}</div>
        {footer && <footer className="modal-foot">{footer}</footer>}
      </div>
    </div>,
    document.body,
  );
}
