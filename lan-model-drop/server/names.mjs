// File and sender names arrive percent-encoded in request headers. They are
// display text only: the drop box stores every file under a random id and
// never uses a received name as a path.
import { extensionOf } from './sniff.mjs';

export const MAX_FILE_NAME = 200;
export const MAX_SENDER = 40;

const CONTROL = /[\u0000-\u001f\u007f-\u009f]/gu;
const RESERVED_CHARACTERS = /[<>:"/\\|?*]/gu;
// Names Windows keeps for devices; the station saves files on Windows.
const WINDOWS_DEVICE = /^(?:con|prn|aux|nul|com[0-9¹²³]|lpt[0-9¹²³])$/iu;

export class NameError extends Error {
  constructor(message) {
    super(message);
    this.name = 'NameError';
  }
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
// control and reserved characters, then surrounding white space and the dots
// and spaces Windows drops from the end of a name.
export function cleanFileName(header) {
  const decoded = decodeHeader(header, 'The file name');
  const base = decoded.split(/[\\/]/u).pop() ?? '';
  let name = base.replace(CONTROL, '').replace(RESERVED_CHARACTERS, '').trim().replace(/[. ]+$/u, '');
  if (name === '' || /^\.+$/u.test(name)) throw new NameError('The file name is empty.');

  // The extension keeps its case as sent; only the type check ignores case.
  const extensionLength = extensionOf(name).length;
  const extension = extensionLength ? name.slice(-extensionLength) : '';
  let stem = extensionLength ? name.slice(0, -extensionLength) : name;
  if (stem.trim() === '') throw new NameError('The file name has no name before its extension.');
  if (WINDOWS_DEVICE.test(stem.trim())) stem = `_${stem}`;

  // At most MAX_FILE_NAME characters, shortening the stem so the extension
  // stays.
  const room = MAX_FILE_NAME - [...extension].length;
  const stemCharacters = [...stem];
  if (stemCharacters.length > room) stem = stemCharacters.slice(0, Math.max(room, 1)).join('').trimEnd();
  name = `${stem}${extension}`;
  return name;
}

// Optional sender name: empty when absent, otherwise at most MAX_SENDER
// characters after decoding and cleaning.
export function cleanSender(header) {
  if (header === undefined || header === '') return '';
  const decoded = decodeHeader(header, 'The sender name');
  const sender = decoded.replace(CONTROL, ' ').replace(/\s+/gu, ' ').trim();
  if ([...sender].length > MAX_SENDER) throw new NameError(`The sender name is longer than ${MAX_SENDER} characters.`);
  return sender;
}
