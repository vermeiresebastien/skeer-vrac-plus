import { Icon } from './Icons';

export type ToastKind = 'info' | 'success' | 'error' | 'learn';

export interface Toast {
  id: number;
  message: string;
  kind: ToastKind;
}

export function Toasts({ toasts, onDismiss }: { toasts: Toast[]; onDismiss: (id: number) => void }) {
  return (
    <div className="toast-stack" role="status" aria-live="polite">
      {toasts.map((toast) => (
        <div className={`toast toast--${toast.kind}`} key={toast.id}>
          <span className="toast-icon">
            <Icon
              name={toast.kind === 'success' ? 'check' : toast.kind === 'error' ? 'info' : toast.kind === 'learn' ? 'midi' : 'info'}
              size={14}
            />
          </span>
          <span>{toast.message}</span>
          <button type="button" className="toast-close" aria-label="Dismiss" onClick={() => onDismiss(toast.id)}>
            <Icon name="close" size={11} />
          </button>
        </div>
      ))}
    </div>
  );
}
