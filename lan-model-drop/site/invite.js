// Old bookmarks may contain a retired sender code. Never use or retain it.
export function clearLegacyInviteCode(win) {
  const url = new URL(win.location.href);
  if (!url.searchParams.has('code') && !url.hash.startsWith('#code=')) return;
  url.searchParams.delete('code');
  if (url.hash.startsWith('#code=')) url.hash = '';
  win.history.replaceState(win.history.state, '', url.pathname + url.search + url.hash);
}
