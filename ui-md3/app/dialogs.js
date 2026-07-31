/*
 * Modal behaviour for the prototype's dialogs and the version-history drawer.
 *
 * The templates declare the semantics (role="dialog", aria-modal, a label and a
 * data-dialog-close control); this module supplies the behaviour a modal owes a
 * keyboard user and cannot express in markup:
 *
 *   - focus moves into the dialog when it opens, and back to whatever opened it
 *     when it closes;
 *   - Tab is trapped inside it;
 *   - Escape closes it, through the dialog's own close control so the app's
 *     state stays the single source of truth;
 *   - the rest of the shell is marked inert, so the obscured UI behind the
 *     scrim stops being tabbable.
 *
 * The app re-renders through keyed patching rather than remounting, so the open
 * dialog is discovered with a MutationObserver instead of a lifecycle hook.
 */
(function bambuDialogs(global) {
  'use strict';

  var doc = global.document;
  var SELECTOR = '[role="dialog"][aria-modal="true"]';
  var FOCUSABLE = 'a[href],button:not([disabled]),input:not([disabled]),select:not([disabled]),' +
    'textarea:not([disabled]),[tabindex]:not([tabindex="-1"])';

  var current = null;
  var opener = null;

  function focusable(dialog) {
    return [].slice.call(dialog.querySelectorAll(FOCUSABLE)).filter(function (node) {
      var rect = node.getBoundingClientRect();
      return rect.width > 0 && rect.height > 0;
    });
  }

  /*
   * Walks from the dialog up to the app root, marking every sibling along the
   * way inert. Marking only the root's children would achieve nothing: the
   * dialog is nested inside that same child, so the whole shell would be its
   * own ancestor and nothing would be excluded.
   */
  function setInert(dialog) {
    var app = doc.getElementById('app');
    if (!app) return;
    var node = dialog;
    while (node && node !== app && node.parentElement) {
      var parent = node.parentElement;
      [].slice.call(parent.children).forEach(function (sibling) {
        if (sibling !== node) sibling.inert = true;
      });
      node = parent;
    }
  }

  function clearInert() {
    var app = doc.getElementById('app');
    if (!app) return;
    [].slice.call(app.querySelectorAll('[inert]')).forEach(function (node) {
      node.inert = false;
    });
  }

  function open(dialog) {
    current = dialog;
    opener = doc.activeElement && doc.activeElement !== doc.body ? doc.activeElement : null;
    setInert(dialog);
    var targets = focusable(dialog);
    var first = dialog.querySelector('[data-dialog-initial]') || targets[0] || dialog;
    if (!dialog.hasAttribute('tabindex')) dialog.tabIndex = -1;
    first.focus();
  }

  function close() {
    current = null;
    clearInert();
    if (opener && doc.contains(opener)) opener.focus();
    opener = null;
  }

  function onKeydown(event) {
    if (!current) return;
    if (event.key === 'Escape') {
      event.preventDefault();
      var closer = current.querySelector('[data-dialog-close]');
      if (closer) closer.click();
      return;
    }
    if (event.key !== 'Tab') return;
    var targets = focusable(current);
    if (!targets.length) return;
    var first = targets[0];
    var last = targets[targets.length - 1];
    if (event.shiftKey && doc.activeElement === first) {
      event.preventDefault();
      last.focus();
    } else if (!event.shiftKey && doc.activeElement === last) {
      event.preventDefault();
      first.focus();
    } else if (!current.contains(doc.activeElement)) {
      event.preventDefault();
      first.focus();
    }
  }

  function scan() {
    var dialog = doc.querySelector(SELECTOR);
    if (dialog === current) return;
    if (!dialog) {
      close();
      return;
    }
    if (current) clearInert();
    open(dialog);
  }

  function start() {
    doc.addEventListener('keydown', onKeydown, true);
    var host = doc.getElementById('app');
    if (!host) return;
    new global.MutationObserver(scan).observe(host, { childList: true, subtree: true });
    scan();
  }

  if (doc.readyState === 'loading') doc.addEventListener('DOMContentLoaded', start);
  else start();

  global.BambuDialogs = { scan: scan, active: function () { return current; } };
})(typeof window !== 'undefined' ? window : globalThis);
