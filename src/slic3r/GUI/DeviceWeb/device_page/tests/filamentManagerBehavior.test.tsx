// @vitest-environment jsdom
import { act, cleanup, fireEvent, render, screen } from '@testing-library/react';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { AccessibleDialog } from '../src/features/filament-manager/AccessibleDialog';
import { FilterDropdown, FilamentTabs } from '../src/features/filament-manager/FilamentManagerPage';
import { ToastStack } from '../src/features/filament-manager/ToastStack';

vi.mock('react-i18next', () => ({
  useTranslation: () => ({ t: (key: string) => key }),
}));

beforeEach(() => {
  vi.stubGlobal('requestAnimationFrame', (callback: FrameRequestCallback) => {
    callback(0);
    return 1;
  });
  vi.stubGlobal('cancelAnimationFrame', () => undefined);
  Object.defineProperty(HTMLElement.prototype, 'getClientRects', {
    configurable: true,
    value() {
      return [{ width: 1, height: 1 }];
    },
  });
});

afterEach(() => {
  cleanup();
  vi.unstubAllGlobals();
});

describe('AccessibleDialog focus lifecycle', () => {
  it('keeps focus stable across rerenders and restores the original opener', () => {
    const opener = document.createElement('button');
    document.body.append(opener);
    opener.focus();

    const firstClose = vi.fn();
    const secondClose = vi.fn();
    const view = render(
      <AccessibleDialog
        open
        onClose={firstClose}
        labelledBy="dialog-title"
        panelClassName="panel"
      >
        <h2 id="dialog-title">Dialog</h2>
        <button>First</button>
        <button>Second</button>
      </AccessibleDialog>,
    );

    expect(document.activeElement).toBe(screen.getByRole('button', { name: 'First' }));
    screen.getByRole('button', { name: 'Second' }).focus();

    view.rerender(
      <AccessibleDialog
        open
        onClose={secondClose}
        labelledBy="dialog-title"
        panelClassName="panel"
      >
        <h2 id="dialog-title">Dialog</h2>
        <button>First</button>
        <button>Second</button>
      </AccessibleDialog>,
    );

    expect(document.activeElement).toBe(screen.getByRole('button', { name: 'Second' }));
    fireEvent.keyDown(document, { key: 'Escape' });
    expect(secondClose).toHaveBeenCalledOnce();
    expect(firstClose).not.toHaveBeenCalled();

    view.rerender(
      <AccessibleDialog
        open={false}
        onClose={secondClose}
        labelledBy="dialog-title"
        panelClassName="panel"
      >
        <h2 id="dialog-title">Dialog</h2>
      </AccessibleDialog>,
    );
    expect(document.activeElement).toBe(opener);
    opener.remove();
  });
});

describe('Filament navigation', () => {
  it.each([
    ['ArrowRight', 'ams'],
    ['ArrowDown', 'ams'],
    ['ArrowLeft', 'ams'],
    ['ArrowUp', 'ams'],
  ] as const)('moves %s to the adjacent tab', (key, expected) => {
    const onSelect = vi.fn();
    render(<FilamentTabs tab="all" isLoggedIn onSelect={onSelect} />);
    const allTab = screen.getByRole('tab', { name: 'All' });
    allTab.focus();
    fireEvent.keyDown(allTab, { key });
    expect(onSelect).toHaveBeenCalledWith(expected);
    expect(document.activeElement).toBe(screen.getByRole('tab', { name: 'In Printer' }));
  });

  it('focuses the selected filter option, roves, and closes with Escape', () => {
    const onClose = vi.fn();
    const onSelect = vi.fn();
    render(
      <FilterDropdown
        label="Brand"
        options={['Bambu', 'Generic']}
        current="Bambu"
        onSelect={onSelect}
        onClose={onClose}
      />,
    );

    const options = screen.getAllByRole('option');
    expect(document.activeElement).toBe(options[1]);
    expect(options.map((option) => option.tabIndex)).toEqual([-1, 0, -1]);
    fireEvent.keyDown(options[1], { key: 'ArrowDown' });
    expect(document.activeElement).toBe(options[2]);
    fireEvent.keyDown(options[2], { key: 'ArrowUp' });
    expect(document.activeElement).toBe(options[1]);
    fireEvent.keyDown(options[1], { key: 'Home' });
    expect(document.activeElement).toBe(options[0]);
    fireEvent.keyDown(options[0], { key: 'End' });
    expect(document.activeElement).toBe(options[2]);
    fireEvent.keyDown(options[2], { key: 'Escape' });
    expect(onClose).toHaveBeenCalledOnce();
    fireEvent.click(options[1]);
    expect(onSelect).toHaveBeenCalledWith('Bambu');
  });
});

describe('ToastStack announcements', () => {
  it('mounts empty announcers before inserting polite and assertive text', () => {
    const view = render(<ToastStack toasts={[]} onDismiss={vi.fn()} autoDismissMs={0} />);
    const polite = screen.getByRole('status');
    const assertive = screen.getByRole('alert');
    expect(polite.textContent).toBe('');
    expect(assertive.textContent).toBe('');

    act(() => {
      view.rerender(
        <ToastStack
          toasts={[
            { id: 1, level: 'info', text: 'Saved', op: 'save' },
            { id: 2, level: 'error', text: 'Save failed', op: 'save' },
          ]}
          onDismiss={vi.fn()}
          autoDismissMs={0}
        />,
      );
    });

    expect(polite.textContent).toContain('Saved');
    expect(assertive.textContent).toContain('Save failed');
    expect(screen.getAllByText('Saved')).toHaveLength(2);
    expect(screen.getAllByText('Save failed')).toHaveLength(2);
  });
});
