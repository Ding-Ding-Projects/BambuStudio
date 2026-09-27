import { useEffect, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import type { CloudToast } from './types';

interface Props {
  toasts: CloudToast[];
  onDismiss: (id: number) => void;
  /** Auto-dismiss interval in ms (0 = never). */
  autoDismissMs?: number;
}

export function ToastStack({ toasts, onDismiss, autoDismissMs = 5000 }: Props) {
  const { t } = useTranslation();
  const [politeAnnouncement, setPoliteAnnouncement] = useState('');
  const [assertiveAnnouncement, setAssertiveAnnouncement] = useState('');
  const announcedToastIdsRef = useRef(new Set<number>());

  useEffect(() => {
    const newToasts = toasts.filter((toast) => !announcedToastIdsRef.current.has(toast.id));
    if (newToasts.length === 0) return;

    newToasts.forEach((toast) => announcedToastIdsRef.current.add(toast.id));
    const latestPolite = [...newToasts].reverse().find((toast) => toast.level === 'info');
    const latestAssertive = [...newToasts].reverse().find((toast) => toast.level !== 'info');
    if (latestPolite) setPoliteAnnouncement(latestPolite.text);
    if (latestAssertive) setAssertiveAnnouncement(latestAssertive.text);
  }, [toasts]);

  useEffect(() => {
    if (!autoDismissMs) return;
    const transientToasts = toasts.filter((toast) => toast.level === 'info');
    if (transientToasts.length === 0) return;
    const timers = transientToasts.map((toast) =>
      window.setTimeout(() => onDismiss(toast.id), autoDismissMs),
    );
    return () => timers.forEach((id) => window.clearTimeout(id));
  }, [toasts, autoDismissMs, onDismiss]);

  // The stack stays mounted even when empty. A live region has to already be in
  // the accessibility tree when a message lands — mount the region and its first
  // child in the same tick and most screen readers treat the whole thing as new
  // content and say nothing. Empty it has no size and no pointer events, so it
  // costs the layout nothing to leave standing.
  return (
    <div
      aria-label={t('Notifications')}
      className="fm-toast-stack fixed bottom-6 right-6 flex flex-col gap-2 z-[2000] pointer-events-none"
    >
      <span className="sr-only" role="status" aria-live="polite" aria-atomic="true">
        {politeAnnouncement}
      </span>
      <span className="sr-only" role="alert" aria-live="assertive" aria-atomic="true">
        {assertiveAnnouncement}
      </span>
      {toasts.map((toast) => {
        // Status tones are chrome, not data: the same danger / warning / brand
        // triple SpoolTable maps its remaining-weight bar to, so a red dot means
        // the same thing everywhere in the feature. Going through the fm tokens
        // also lets the dot re-tint under [data-theme="light"] — the hexes that
        // used to live here (#ff6b6b / #ffb84d / #50e81d) were eyeballed against
        // the dark surface and collapsed to 2.5:1 / 1.6:1 / 1.5:1 on the light
        // bg-fm-sidebar, below the 3:1 WCAG 1.4.11 floor for a meaningful
        // graphic. The tokens hold 5.4:1 or better in both themes.
        const dotClass = toast.level === 'error' ? 'bg-fm-danger'
          : toast.level === 'warn' ? 'bg-fm-warning'
          : 'bg-fm-brand';
        return (
          <div
            key={toast.id}
            className="pointer-events-auto flex items-start gap-3 max-w-[360px] bg-fm-sidebar border border-fm-border rounded-lg px-4 py-3 shadow-[0_4px_20px_rgba(0,0,0,0.25)] text-fm-text-primary text-xs leading-[19px]"
          >
            <span
              aria-hidden="true"
              className={`mt-[4px] inline-block size-[8px] rounded-full shrink-0 ${dotClass}`}
            />
            <span className="flex-1 break-words">{toast.text}</span>
            <button
              className="fm-toast-dismiss shrink-0 size-9 inline-flex items-center justify-center rounded-full bg-transparent border-none text-fm-text-detail cursor-pointer hover:text-fm-text-strong focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-fm-brand"
              onClick={() => onDismiss(toast.id)}
              aria-label={t('Dismiss')}
            >
              <svg width="10" height="10" viewBox="0 0 10 10" fill="none">
                <path d="M2 2l6 6M8 2l-6 6" stroke="currentColor" strokeWidth="1.2" />
              </svg>
            </button>
          </div>
        );
      })}
    </div>
  );
}
