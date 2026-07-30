import {
  useEffect,
  useRef,
  type CSSProperties,
  type MouseEvent as ReactMouseEvent,
  type ReactNode,
  type RefObject,
} from 'react';

const dialogStack: symbol[] = [];
const FOCUSABLE_SELECTOR = [
  'a[href]',
  'button:not([disabled])',
  'input:not([disabled]):not([type="hidden"])',
  'select:not([disabled])',
  'textarea:not([disabled])',
  '[tabindex]:not([tabindex="-1"])',
].join(',');

function getFocusableElements(container: HTMLElement): HTMLElement[] {
  return Array.from(container.querySelectorAll<HTMLElement>(FOCUSABLE_SELECTOR))
    .filter((element) => !element.hasAttribute('hidden') && element.getClientRects().length > 0);
}

interface Props {
  open: boolean;
  onClose: () => void;
  labelledBy: string;
  describedBy?: string;
  children: ReactNode;
  dataTestId?: string;
  dataAttributes?: Record<string, string | number | boolean | undefined>;
  dataDanger?: boolean;
  overlayClassName?: string;
  panelClassName: string;
  panelStyle?: CSSProperties;
  initialFocusRef?: RefObject<HTMLElement | null>;
  closeOnBackdrop?: boolean;
}

/**
 * Shared modal shell for Filament Manager dialogs.
 *
 * It deliberately owns only accessibility and overlay behavior, leaving each
 * dialog's existing visual anatomy and bridge/test contracts untouched.
 */
export function AccessibleDialog({
  open,
  onClose,
  labelledBy,
  describedBy,
  children,
  dataTestId,
  dataAttributes,
  dataDanger,
  overlayClassName = '',
  panelClassName,
  panelStyle,
  initialFocusRef,
  closeOnBackdrop = false,
}: Props) {
  const panelRef = useRef<HTMLDivElement>(null);
  const tokenRef = useRef(Symbol('filament-manager-dialog'));
  const restoreFocusRef = useRef<HTMLElement | null>(null);

  useEffect(() => {
    if (!open) return;

    const token = tokenRef.current;
    restoreFocusRef.current = document.activeElement instanceof HTMLElement
      ? document.activeElement
      : null;
    dialogStack.push(token);

    const focusInitialControl = () => {
      const panel = panelRef.current;
      if (!panel || dialogStack[dialogStack.length - 1] !== token) return;
      const initial = initialFocusRef?.current;
      const target = initial && panel.contains(initial)
        ? initial
        : getFocusableElements(panel)[0] ?? panel;
      target.focus({ preventScroll: true });
    };
    const frame = window.requestAnimationFrame(focusInitialControl);

    const handleKeyDown = (event: KeyboardEvent) => {
      if (dialogStack[dialogStack.length - 1] !== token) return;

      if (event.key === 'Escape') {
        event.preventDefault();
        event.stopPropagation();
        onClose();
        return;
      }

      if (event.key !== 'Tab') return;
      const panel = panelRef.current;
      if (!panel) return;
      const focusable = getFocusableElements(panel);
      if (focusable.length === 0) {
        event.preventDefault();
        panel.focus();
        return;
      }

      const active = document.activeElement;
      const first = focusable[0];
      const last = focusable[focusable.length - 1];
      if (event.shiftKey && (active === first || !panel.contains(active))) {
        event.preventDefault();
        last.focus();
      } else if (!event.shiftKey && (active === last || !panel.contains(active))) {
        event.preventDefault();
        first.focus();
      }
    };

    document.addEventListener('keydown', handleKeyDown, true);
    return () => {
      window.cancelAnimationFrame(frame);
      document.removeEventListener('keydown', handleKeyDown, true);
      const index = dialogStack.lastIndexOf(token);
      if (index >= 0) dialogStack.splice(index, 1);

      const restoreTarget = restoreFocusRef.current;
      if (restoreTarget?.isConnected) {
        window.requestAnimationFrame(() => restoreTarget.focus({ preventScroll: true }));
      }
    };
  }, [initialFocusRef, onClose, open]);

  if (!open) return null;

  const handleBackdropClick = (event: ReactMouseEvent<HTMLDivElement>) => {
    if (closeOnBackdrop && event.target === event.currentTarget) onClose();
  };

  return (
    <div
      className={`fixed inset-0 bg-black/50 z-[1000] ${overlayClassName}`}
      onMouseDown={handleBackdropClick}
      data-dialog-overlay="true"
    >
      <div
        ref={panelRef}
        data-testid={dataTestId}
        data-danger={dataDanger === undefined ? undefined : (dataDanger ? 'true' : 'false')}
        {...Object.fromEntries(
          Object.entries(dataAttributes ?? {}).map(([key, value]) => [
            `data-${key}`,
            typeof value === 'boolean' ? String(value) : value,
          ]),
        )}
        className={panelClassName}
        style={panelStyle}
        role="dialog"
        aria-modal="true"
        aria-labelledby={labelledBy}
        aria-describedby={describedBy}
        tabIndex={-1}
        onMouseDown={(event) => event.stopPropagation()}
      >
        {children}
      </div>
    </div>
  );
}
