// What the sender page does with each answer of the drop box (protocol 1),
// and the checks it makes before sending anything. No DOM: the tests import
// this module directly.

export const ACCEPTED_EXTENSIONS = Object.freeze(['.3mf', '.stl', '.step', '.stp', '.obj', '.amf']);

export function extensionOf(name) {
  const text = String(name ?? '');
  const dot = text.lastIndexOf('.');
  return dot < 0 ? '' : text.slice(dot).toLowerCase();
}

// Checks a chosen file before it is queued. The drop box checks again,
// including the content; this only spares people a pointless upload.
// Returns null when the file can be sent, else the reason as a message key
// with its substitutions.
export function precheck({ name, size }, maxBytes) {
  if (!ACCEPTED_EXTENSIONS.includes(extensionOf(name))) return { key: 'reasonType' };
  if (!(size > 0)) return { key: 'reasonEmpty' };
  if (Number.isFinite(maxBytes) && maxBytes > 0 && size > maxBytes) return { key: 'reasonTooLarge', size: maxBytes };
  return null;
}

// Minutes to wait after a lockout, from Retry-After in seconds.
export function lockMinutes(retryAfter) {
  const seconds = Number(retryAfter);
  return Number.isFinite(seconds) && seconds > 0 ? Math.max(1, Math.ceil(seconds / 60)) : 5;
}

// Maps one answer to what happens next:
//   sent       the file is in the drop box
//   reason     why this file was not sent (message key and substitutions)
//   stop       the remaining files are not tried, because they would fail
//              the same way (wrong code, lockout, full box, no connection)
//   live       the message for the whole send, when it stops
//   codeError  the drop code field shows this message
export function outcomeFor({ status, body, retryAfter }, maxBytes) {
  const error = body && typeof body === 'object' ? body.error : undefined;
  if (status === 201 && body && body.ok === true) return { sent: true };
  if (status === 0) return { sent: false, reason: { key: 'reasonNetwork' }, stop: true, live: { key: 'liveNetwork' } };
  if (status === 401 || error === 'wrong_code') {
    return { sent: false, reason: { key: 'reasonWrongCode' }, stop: true, live: { key: 'liveWrongCode' }, codeError: { key: 'codeWrong' } };
  }
  if (status === 429 || error === 'too_many_attempts') {
    const minutes = lockMinutes(retryAfter);
    return { sent: false, reason: { key: 'reasonLocked' }, stop: true, live: { key: 'liveLocked', values: [minutes] } };
  }
  if (status === 507 || error === 'queue_full') return { sent: false, reason: { key: 'reasonFull' }, stop: true, live: { key: 'liveFull' } };
  if (status === 413 || error === 'too_large') return { sent: false, reason: { key: 'reasonTooLarge', size: maxBytes } };
  if (status === 415 || error === 'unsupported_type') return { sent: false, reason: { key: 'reasonType' } };
  if (status === 400 || error === 'bad_request') return { sent: false, reason: { key: 'reasonName' } };
  return {
    sent: false,
    reason: { key: 'reasonServer', values: [status] },
    stop: status >= 500,
    live: status >= 500 ? { key: 'liveServer', values: [status] } : undefined,
  };
}
