// File and sender names arrive percent-encoded in request headers. They are
// display text only: the drop box stores every file under a random id and
// never uses a received name as a path.
import { extensionOf } from './sniff.mjs';

export const MAX_FILE_NAME = 200;
export const MAX_SENDER = 40;

const CONTROL = /[\u0000-\u001f\u007f-\u009f]/gu;
const RESERVED_CHARACTERS = /[<>:"/\\|?*]/gu;
// Characters that reorder or hide text: the Arabic letter mark, left-to-right and right-to-left
// marks, the bidirectional embeddings, overrides and isolates, and the byte order mark. A name
// carrying them could read differently from what it is, so Bambu Studio removes them from every
// name it shows; the drop box removes them too, so a name made of nothing else is refused here,
// where the sender sees why, instead of by the station after the sender was told it arrived.
const INVISIBLE = /[\u061c\u200e\u200f\u202a-\u202e\u2066-\u2069\ufeff]/gu;
// Names Windows keeps for devices; the station saves files on Windows.
const WINDOWS_DEVICE = /^(?:con|prn|aux|nul|com[0-9¹²³]|lpt[0-9¹²³])$/iu;

export class NameError extends Error {
  constructor(message) {
    super(message);
    this.name = 'NameError';
  }
}

// White space at either end, as String.prototype.trim() takes it, and the zero-width space, which
// Bambu Studio also counts as space at the ends of a name.
function trimSpace(text) {
  return text.replace(/^[\s\u200b]+|[\s\u200b]+$/gu, '');
}

function decodeHeader(value, what) {
  if (typeof value !== 'string') throw new NameError(`${what} is missing.`);
  try {
    return decodeURIComponent(value);
  } catch {
    throw new NameError(`${what} is not valid percent-encoding.`);
  }
}

// The base name only: anything up to the last slash or backslash goes, then
// control, invisible and reserved characters, then surrounding white space and
// the dots and spaces Windows drops from the end of a name.
export function cleanFileName(header) {
  const decoded = decodeHeader(header, 'The file name');
  const base = decoded.split(/[\\/]/u).pop() ?? '';
  let name = trimSpace(base.replace(CONTROL, '').replace(INVISIBLE, '').replace(RESERVED_CHARACTERS, '')).replace(/[. ]+$/u, '');
  if (name === '' || /^\.+$/u.test(name)) throw new NameError('The file name is empty.');

  // The extension keeps its case as sent; only the type check ignores case.
  const extensionLength = extensionOf(name).length;
  const extension = extensionLength ? name.slice(-extensionLength) : '';
  let stem = extensionLength ? name.slice(0, -extensionLength) : name;
  if (trimSpace(stem) === '') throw new NameError('The file name has no name before its extension.');
  if (WINDOWS_DEVICE.test(trimSpace(stem))) stem = `_${stem}`;

  // At most MAX_FILE_NAME characters, shortening the stem so the extension
  // stays.
  const room = MAX_FILE_NAME - [...extension].length;
  const stemCharacters = [...stem];
  if (stemCharacters.length > room) stem = stemCharacters.slice(0, Math.max(room, 1)).join('').trimEnd();
  name = `${stem}${extension}`;
  return name;
}

// Optional sender name: empty when absent, otherwise at most MAX_SENDER
// characters after decoding and cleaning (control characters become spaces,
// invisible ones go, runs of white space become one space).
export function cleanSender(header) {
  if (header === undefined || header === '') return '';
  const decoded = decodeHeader(header, 'The sender name');
  const sender = trimSpace(decoded.replace(CONTROL, ' ').replace(INVISIBLE, '').replace(/\s+/gu, ' '));
  if ([...sender].length > MAX_SENDER) throw new NameError(`The sender name is longer than ${MAX_SENDER} characters.`);
  return sender;
}
