// Invite links. Bambu Studio hands out `<site>/#code=<drop code>`: the code
// travels in the fragment, which browsers never send to a server or in a
// Referer header. The page reads it once, fills the code field and removes
// the fragment from the address bar, so the code does not stay in the
// browser history or get copied on with the page address. No DOM here
// beyond the location and history objects passed in: the tests call these
// functions directly.

// A drop code is 4 to 12 digits; people may type it with spaces.
export const CODE_PATTERN = /^\d{4,12}$/;

export function normalizeCode(value) {
  return String(value ?? '').replace(/[\s-]+/g, '');
}

export function validCode(value) {
  return CODE_PATTERN.test(normalizeCode(value));
}

// Reads `code=` from a fragment such as "#code=123456". `present` says the
// fragment carried a code parameter at all; `code` is the code when it is a
// valid one, otherwise null.
export function codeFromFragment(hash) {
  const text = String(hash ?? '').replace(/^#/, '');
  if (text === '') return { present: false, code: null };
  let params;
  try {
    params = new URLSearchParams(text);
  } catch {
    return { present: false, code: null };
  }
  if (!params.has('code')) return { present: false, code: null };
  const code = normalizeCode(params.get('code'));
  return { present: true, code: CODE_PATTERN.test(code) ? code : null };
}

// Takes the invite code out of the current address. When the fragment
// carries a code parameter, valid or not, it is removed with
// history.replaceState, which neither reloads the page nor adds a history
// entry; the path and any query stay as they are. Any other fragment is
// left alone. Returns what codeFromFragment found.
export function takeInviteCode(win) {
  const found = codeFromFragment(win.location.hash);
  if (!found.present) return found;
  try {
    win.history.replaceState(win.history.state, '', `${win.location.pathname}${win.location.search}`);
  } catch {
    // A browser that refuses replaceState still gets the code filled in.
  }
  return found;
}
