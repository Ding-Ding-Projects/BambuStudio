import { useId, useRef } from 'react';
import { useTranslation } from 'react-i18next';
import useStore from '../../store/AppStore';
import type { CloudSyncHistoryEntry } from './types';
import { AccessibleDialog } from './AccessibleDialog';
import { BilingualText } from './BilingualText';

interface Props {
  open: boolean;
  onClose: () => void;
}

function formatTs(ts: number): string {
  try {
    return new Date(ts).toLocaleString([], { hour12: false });
  } catch {
    return String(ts);
  }
}

function rowColorClass(entry: CloudSyncHistoryEntry): string {
  if (entry.status === 'error') return 'text-[#ff6b6b]';
  return entry.kind === 'pull' ? 'text-fm-brand' : 'text-fm-text-primary';
}

/**
 * Modal list of recent cloud sync events (pulls, pushes, failures) with
 * timestamps. Sourced from `filament.cloudSyncHistory`, which the bridge
 * fills from every `sync/pull_done`, `sync/push_done`, `sync/push_failed`
 * report — independent of the debug log switch, so it also works in
 * Release builds.
 */
export function CloudHistoryPopover({ open, onClose }: Props) {
  const { t } = useTranslation();
  const history = useStore((s) => s.filament.cloudSyncHistory);
  const clearHistory = useStore((s) => s.filament.clearCloudSyncHistory);
  const titleId = useId();
  const closeButtonRef = useRef<HTMLButtonElement>(null);

  // Newest first for display while keeping the underlying store in arrival order.
  const rows = [...history].reverse();

  return (
    <AccessibleDialog
      open={open}
      onClose={onClose}
      labelledBy={titleId}
      overlayClassName="flex items-center justify-center z-[1100] p-4"
      panelClassName="fm-dialog-panel w-[520px] max-w-full max-h-[80vh] bg-fm-sidebar rounded-xl shadow-[0_8px_24px_rgba(0,0,0,0.4)] overflow-hidden flex flex-col"
      initialFocusRef={closeButtonRef}
      closeOnBackdrop
    >
      <div className="flex flex-wrap items-center justify-between gap-3 px-5 pt-4 pb-3 border-b border-fm-border-focus/30">
        <h3 id={titleId} className="text-sm font-medium text-fm-text-strong leading-5 m-0">
          <BilingualText>{t('Sync History')}</BilingualText>
        </h3>
        <div className="flex flex-wrap items-center gap-2">
          <button
            type="button"
            onClick={clearHistory}
            disabled={rows.length === 0}
            className="fm-action-target inline-flex items-center justify-center min-h-[36px] px-3 rounded-lg border border-fm-border-focus/50 bg-fm-inner text-fm-text-secondary text-xs cursor-pointer transition-colors duration-150 hover:bg-fm-hover disabled:opacity-40 disabled:cursor-default"
          >
            <BilingualText>{t('Clear')}</BilingualText>
          </button>
          <button
            ref={closeButtonRef}
            type="button"
            onClick={onClose}
            className="fm-action-target inline-flex items-center justify-center min-h-[36px] px-3 rounded-lg border border-fm-border-focus/50 bg-fm-inner text-fm-text-primary text-xs cursor-pointer transition-colors duration-150 hover:bg-fm-hover"
          >
            <BilingualText>{t('Close')}</BilingualText>
          </button>
        </div>
      </div>

      <div className="flex-1 overflow-y-auto">
        {rows.length === 0 ? (
          <div className="flex items-center justify-center min-h-[200px] p-5 text-xs text-fm-text-secondary">
            <BilingualText>{t('No sync history yet.')}</BilingualText>
          </div>
        ) : (
          <ul className="m-0 p-0 list-none">
            {rows.map((entry) => (
              <li
                key={entry.id}
                className="flex flex-wrap items-start gap-3 px-5 py-2.5 border-b border-fm-border-focus/15 last:border-b-0"
              >
                <span className="shrink-0 mt-[1px] text-[11px] font-mono text-fm-text-secondary tabular-nums">
                  {formatTs(entry.ts)}
                </span>
                <BilingualText
                  className={`shrink-0 mt-[1px] inline-flex min-h-[24px] items-center justify-center px-2 rounded-full text-[11px] ${
                    entry.status === 'error'
                      ? 'bg-fm-danger/15 text-fm-danger'
                      : entry.kind === 'pull'
                        ? 'bg-fm-brand/15 text-fm-brand'
                        : 'bg-fm-inner text-fm-text-secondary'
                  }`}
                >
                  {entry.status === 'error'
                    ? t('Failed')
                    : entry.kind === 'pull'
                      ? t('Pull')
                      : t('Push')}
                </BilingualText>
                <span className={`flex-1 min-w-[12rem] text-xs leading-5 break-words ${rowColorClass(entry)}`}>
                  {entry.summary}
                </span>
              </li>
            ))}
          </ul>
        )}
      </div>
    </AccessibleDialog>
  );
}
