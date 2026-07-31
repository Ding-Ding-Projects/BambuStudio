import { useId, useRef } from 'react';
import { useTranslation } from 'react-i18next';
import { AccessibleDialog } from './AccessibleDialog';
import { BilingualText } from './BilingualText';

interface Props {
  open: boolean;
  title: string;
  message?: string;
  confirmText?: string;
  cancelText?: string;
  /** Render confirm button in destructive red style. */
  danger?: boolean;
  onConfirm: () => void | Promise<unknown>;
  onCancel: () => void;
}

/**
 * Reusable modal confirmation dialog that replaces the native `window.confirm()`.
 * The native dialog leaks the page URL (file:// or http://) in its title bar when
 * running inside a WebView2 / CEF host, which looks broken for end users.
 */
export function ConfirmDialog({
  open, title, message,
  confirmText, cancelText,
  danger = false,
  onConfirm, onCancel,
}: Props) {
  const { t } = useTranslation();
  const titleId = useId();
  const messageId = useId();
  const cancelButtonRef = useRef<HTMLButtonElement>(null);

  const handleConfirm = () => {
    void onConfirm();
  };

  return (
    <AccessibleDialog
      open={open}
      onClose={onCancel}
      labelledBy={titleId}
      describedBy={message ? messageId : undefined}
      dataTestId="confirm-dialog"
      dataDanger={danger}
      overlayClassName="flex items-center justify-center z-[1100] p-4"
      panelClassName="fm-dialog-panel w-[360px] max-w-full max-h-full bg-fm-sidebar rounded-xl shadow-[0_8px_24px_rgba(0,0,0,0.4)] overflow-auto"
      initialFocusRef={cancelButtonRef}
    >
      <div>
        <div className="px-6 pt-5 pb-2">
          <h3 id={titleId} data-testid="confirm-dialog-title" className="text-sm font-medium text-fm-text-strong leading-5 m-0">
            <BilingualText>{title}</BilingualText>
          </h3>
        </div>
        {message ? (
          <div id={messageId} className="px-6 pb-5 text-xs text-fm-text-secondary leading-5">
            <BilingualText as="div">{message}</BilingualText>
          </div>
        ) : (
          <div className="h-2" />
        )}
        <div className="flex flex-wrap gap-2 justify-end px-6 pb-5">
          <button
            ref={cancelButtonRef}
            type="button"
            data-testid="confirm-dialog-cancel"
            onClick={onCancel}
            className="fm-action-target inline-flex items-center justify-center min-h-[36px] px-4 rounded-lg border border-fm-border-focus/50 bg-fm-inner text-fm-text-primary text-xs cursor-pointer transition-colors duration-150 hover:bg-fm-hover"
          >
            <BilingualText>{cancelText ?? t('Cancel')}</BilingualText>
          </button>
          <button
            type="button"
            data-testid="confirm-dialog-confirm"
            onClick={handleConfirm}
            className={`fm-action-target inline-flex items-center justify-center min-h-[36px] px-4 rounded-lg border-none text-xs font-medium cursor-pointer transition-colors duration-150 ${
              danger
                ? 'bg-[#d9373b] text-white hover:bg-[#bf2f33]'
                : 'bg-fm-brand text-fm-base hover:bg-fm-brand-hover'
            }`}
          >
            <BilingualText>{confirmText ?? t('OK')}</BilingualText>
          </button>
        </div>
      </div>
    </AccessibleDialog>
  );
}
