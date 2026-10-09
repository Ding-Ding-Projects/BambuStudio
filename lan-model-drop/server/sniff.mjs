// Model types the drop box accepts. A file is accepted only when its
// extension AND its content agree: a renamed document or program is refused.

export const ACCEPTED_EXTENSIONS = Object.freeze({
  '.3mf': '3mf',
  '.stl': 'stl',
  '.step': 'step',
  '.stp': 'step',
  '.obj': 'obj',
  '.amf': 'amf',
});

export const MODEL_TYPES = Object.freeze(['3mf', 'stl', 'step', 'obj', 'amf']);

// The first part of the body that content checks look at.
export const HEAD_BYTES = 256 * 1024;

export function extensionOf(fileName) {
  const dot = fileName.lastIndexOf('.');
  return dot < 0 ? '' : fileName.slice(dot).toLowerCase();
}

export function typeForName(fileName) {
  return ACCEPTED_EXTENSIONS[extensionOf(fileName)] ?? null;
}

// Collects what the content checks need while the body streams past: the
// first HEAD_BYTES, the total length and whether every byte so far is text.
export class ContentProbe {
  constructor(headLimit = HEAD_BYTES) {
    this.headLimit = headLimit;
    this.parts = [];
    this.headLength = 0;
    this.length = 0;
    this.text = true;
  }

  update(chunk) {
    this.length += chunk.length;
    if (this.headLength < this.headLimit) {
      const part = chunk.subarray(0, this.headLimit - this.headLength);
      this.parts.push(Buffer.from(part));
      this.headLength += part.length;
    }
    if (this.text && !isTextChunk(chunk)) this.text = false;
  }

  result() {
    return { head: Buffer.concat(this.parts, this.headLength), length: this.length, text: this.text };
  }
}

// Printable text: tab, line feed, form feed, carriage return, visible ASCII
// and any byte of a UTF-8 sequence. NUL and the other control bytes are not.
export function isTextChunk(chunk) {
  for (let i = 0; i < chunk.length; i += 1) {
    const byte = chunk[i];
    if (byte >= 0x20 && byte !== 0x7f) continue;
    if (byte === 0x09 || byte === 0x0a || byte === 0x0c || byte === 0x0d) continue;
    return false;
  }
  return true;
}

const ZIP_MAGIC = Buffer.from([0x50, 0x4b, 0x03, 0x04]);

function isZip(head) {
  return head.length >= 4 && head.subarray(0, 4).equals(ZIP_MAGIC);
}

// Text after an optional UTF-8 byte order mark and leading white space.
function leadingText(head) {
  let start = 0;
  if (head.length >= 3 && head[0] === 0xef && head[1] === 0xbb && head[2] === 0xbf) start = 3;
  return head.subarray(start).toString('latin1').replace(/^[\t\n\f\r ]+/, '');
}

function isBinaryStl(head, length) {
  if (length < 84 || head.length < 84) return false;
  const triangles = head.readUInt32LE(80);
  return 84 + 50 * triangles === length;
}

function isAsciiStl(head, text) {
  if (!text) return false;
  const body = leadingText(head);
  return body.startsWith('solid') && body.includes('facet');
}

export function contentMatches(type, { head, length, text }) {
  switch (type) {
    case '3mf':
      return isZip(head);
    case 'stl':
      // Binary STL headers may also begin with "solid", so the exact binary
      // length rule is tried first.
      return isBinaryStl(head, length) || isAsciiStl(head, text);
    case 'step':
      return leadingText(head).startsWith('ISO-10303-21');
    case 'obj':
      return text && /(?:^|[\r\n])[\t ]*v[\t ]/.test(head.toString('latin1'));
    case 'amf': {
      if (isZip(head)) return true;
      const body = leadingText(head);
      return body.startsWith('<') && body.includes('<amf');
    }
    default:
      return false;
  }
}
