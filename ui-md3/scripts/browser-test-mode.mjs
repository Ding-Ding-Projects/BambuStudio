/*
 * Browser-only controls for deterministic capture and runtime-test sessions.
 *
 * This hook runs before the page's first document. It keeps the catalogue and
 * renderer intact while setting the automatic startup probability to zero, so
 * tests never gain a public opt-out and can still invoke the real renderer.
 */
const DIM_SUM_STARTUP_SUPPRESSION = `(() => {
  let catalogue;
  Object.defineProperty(window, 'BAMBU_DIM_SUM', {
    configurable: true,
    enumerable: true,
    get() { return catalogue; },
    set(value) {
      catalogue = value && typeof value === 'object'
        ? Object.assign({}, value, { chance: 0 })
        : value;
    }
  });
})();`;

export async function suppressAutomaticDimSum(session) {
  if (!session || typeof session.send !== 'function')
    throw new TypeError('A connected DevTools session is required');
  await session.send('Page.addScriptToEvaluateOnNewDocument', {
    source: DIM_SUM_STARTUP_SUPPRESSION,
  });
}
