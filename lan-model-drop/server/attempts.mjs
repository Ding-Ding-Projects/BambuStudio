// Upload attempts per client: 60 per minute, then a five-minute cooldown.

export const UPLOAD_LIMIT = 60;
export const UPLOAD_WINDOW_MS = 60 * 1000;
export const LOCKOUT_MS = 5 * 60 * 1000;
const MAX_TRACKED = 10000;

// IPv4 clients reached through an IPv6 socket appear as ::ffff:a.b.c.d.
export function clientAddress(socket) {
  const address = socket?.remoteAddress ?? 'unknown';
  return address.startsWith('::ffff:') && address.includes('.') ? address.slice(7) : address;
}

export class AttemptLimiter {
  constructor({ now = Date.now } = {}) {
    this.now = now;
    this.entries = new Map();
  }

  // Milliseconds the address stays locked out, or 0.
  lockedFor(address) {
    const entry = this.entries.get(address);
    if (!entry) return 0;
    const left = entry.lockedUntil - this.now();
    return left > 0 ? left : 0;
  }

  recordAttempt(address) {
    const now = this.now();
    this.prune(now);
    const entry = this.entries.get(address) ?? { attempts: [], lockedUntil: 0 };
    entry.attempts = entry.attempts.filter((time) => now - time < UPLOAD_WINDOW_MS);
    entry.attempts.push(now);
    if (entry.attempts.length >= UPLOAD_LIMIT) {
      entry.lockedUntil = now + LOCKOUT_MS;
      entry.attempts = [];
    }
    this.entries.delete(address);
    this.entries.set(address, entry);
  }

  prune(now) {
    for (const [address, entry] of this.entries) {
      const recent = entry.attempts.some((time) => now - time < UPLOAD_WINDOW_MS);
      if (!recent && entry.lockedUntil <= now) this.entries.delete(address);
    }
    // A flood of addresses cannot grow memory without bound: the oldest
    // entries go first.
    while (this.entries.size >= MAX_TRACKED) this.entries.delete(this.entries.keys().next().value);
  }
}
