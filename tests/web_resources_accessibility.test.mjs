import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'
import path from 'node:path'
import test from 'node:test'
import { fileURLToPath } from 'node:url'

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..')
const read = (relativePath) => readFile(path.join(repoRoot, relativePath), 'utf8')

const projectHtml = [
  'resources/web/model_new/index.html',
  'resources/web/model_new/editor.html',
  'resources/web/model_new/black.html',
]

const guideHtml = [
  'resources/web/guide/3/index.html',
  'resources/web/guide/5/index.html',
  'resources/web/guide/6/index.html',
  'resources/web/guide/11/index.html',
  'resources/web/guide/21/index.html',
  'resources/web/guide/22/index.html',
  'resources/web/guide/23/index.html',
  'resources/web/guide/24/index.html',
  'resources/web/guide/31/index.html',
  'resources/web/guide/32/32.html',
]

const activeGuideJs = [
  'resources/web/guide/js/common.js',
  'resources/web/guide/3/3.js',
  'resources/web/guide/5/5.js',
  'resources/web/guide/6/6.js',
  'resources/web/guide/11/11.js',
  'resources/web/guide/21/21.js',
  'resources/web/guide/22/22.js',
  'resources/web/guide/23/23.js',
  'resources/web/guide/24/24.js',
  'resources/web/guide/31/31.js',
  'resources/web/guide/32/32.js',
]

test('Project WebViews preserve browser zoom and keyboard behavior', async () => {
  for (const file of projectHtml) {
    const source = await read(file)
    assert.doesNotMatch(source, /maximum-scale|user-scalable/i, file)
  }

  const globalApi = await read('resources/web/include/globalapi.js')
  assert.doesNotMatch(globalApi, /(?:mousewheel|wheel)[\s\S]{0,180}(?:ctrlKey|metaKey)[\s\S]{0,180}preventDefault/, 'global API must not suppress Ctrl/Cmd-wheel zoom')
  assert.doesNotMatch(globalApi, /document\.onkeydown\s*=/, 'global API must not replace the page keyboard handler')
  assert.doesNotMatch(globalApi, /keyCode\s*=\s*0|returnValue\s*=\s*false/, 'global API must not cancel keyboard events wholesale')
})

test('Project actions use native interactive controls and responsive focus styles', async () => {
  const indexHtml = await read(projectHtml[0])
  const editorHtml = await read(projectHtml[1])
  const blackHtml = await read(projectHtml[2])
  const indexJs = await read('resources/web/model_new/index.js')
  const editorJs = await read('resources/web/model_new/js/editor.js')
  const galleryJs = await read('resources/web/model_new/js/gallery.js')
  const indexCss = await read('resources/web/model_new/index.css')
  const editorCss = await read('resources/web/model_new/css/editor.css')

  assert.match(indexHtml, /<button[^>]+class="saveBtn trans"[^>]*>Edit<\/button>/)
  assert.match(indexHtml, /<button[^>]+id="projectName"[^>]+disabled/)
  assert.match(editorHtml, /<button[^>]+class="returnBtn"/)
  assert.match(editorHtml, /<button[^>]+class="saveBtn trans"/)
  assert.match(editorHtml, /<label[^>]+for="projectNameInput"/)
  assert.match(blackHtml, /<button[^>]+class="addBtn"/)
  assert.match(blackHtml, /<script>[\s\S]*<\/script>[\s\S]*<\/body>/, 'black page script must remain inside body')

  assert.match(indexJs, /<button type="button">/)
  assert.match(editorJs, /<button type="button">/)
  assert.match(editorJs, /attachment-open/)
  assert.match(editorJs, /attachment-delete/)
  assert.match(galleryJs, /<button type="button" class="bs-gallery-main"/)
  assert.match(galleryJs, /<button type="button" class="bs-gallery-thumb"/)

  assert.match(indexCss, /grid-template-columns:\s*auto minmax\(0, 1fr\)/)
  assert.match(indexCss, /@media \(max-width:\s*760px\)/)
  assert.match(editorCss, /width:\s*min\(100%,\s*1068px\)/)
  assert.match(editorCss, /padding:\s*36px clamp\(/)
  assert.match(editorCss, /:focus-visible/)
  assert.match(editorCss, /\.imageDelete\s*\{[\s\S]*display:\s*inline-flex/)
  assert.match(editorCss, /\.setModelCover\s*\{[\s\S]*display:\s*flex/)
  assert.doesNotMatch(editorCss, /\.imageDelete\s*\{[\s\S]{0,120}display:\s*none/)
  assert.doesNotMatch(editorCss, /\.setModelCover\s*\{[\s\S]{0,220}display:\s*none/)
})

test('showToast exposes severity, live roles, named dismiss, persistence, and stacking', async () => {
  const globalApi = await read('resources/web/include/globalapi.js')
  const toolCss = await read('resources/web/model_new/css/tool.css')

  assert.match(globalApi, /severity === 'warning' \|\| severity === 'error'/)
  assert.match(globalApi, /setAttribute\('role', severity === 'warning' \|\| severity === 'error' \? 'alert' : 'status'\)/)
  assert.match(globalApi, /setAttribute\('aria-live', severity === 'warning' \|\| severity === 'error' \? 'assertive' : 'polite'\)/)
  assert.match(globalApi, /dismiss\.type = 'button'/)
  assert.match(globalApi, /dismiss\.setAttribute\('aria-label', getToastDismissLabel\(\)\)/)
  assert.match(globalApi, /region\.setAttribute\('role', 'region'\)/)
  assert.match(globalApi, /ensureToastRegion\(\)\.appendChild\(toast\)/)
  assert.doesNotMatch(globalApi, /querySelector\('\.toast'\)[\s\S]{0,80}\.remove\(\)/)

  assert.match(toolCss, /\.toast-region\s*\{/)
  assert.match(toolCss, /flex-direction:\s*column-reverse/)
  assert.match(toolCss, /\.toast__dismiss:focus-visible/)
  assert.match(toolCss, /@media \(prefers-reduced-motion:\s*reduce\)/)
})

test('active setup guides preserve zoom and expose native or compatible actions', async () => {
  for (const file of guideHtml) {
    const source = await read(file)
    assert.doesNotMatch(source, /maximum-scale|user-scalable/i, file)
    const liveMarkup = source.replace(/<!--[\s\S]*?-->/g, '')
    assert.doesNotMatch(liveMarkup, /<(?:div|span|img)[^>]+on(?:click|Click)=/i, file)
  }

  for (const file of activeGuideJs) {
    const source = await read(file)
    assert.doesNotMatch(source, /(?:mousewheel|wheel)[\s\S]{0,180}(?:ctrlKey|metaKey)[\s\S]{0,180}preventDefault/, file)
    assert.doesNotMatch(source, /document\.onkeydown\s*=|keyCode\s*=\s*0|returnValue\s*=\s*false/, file)
  }

  const commonJs = await read(activeGuideJs[0])
  const commonCss = await read('resources/web/guide/css/common.css')
  assert.match(commonJs, /function initializeOwnedGuideActions\(root\)/)
  assert.match(commonJs, /MutationObserver/)
  assert.match(commonJs, /event\.key === 'Enter' \|\| event\.key === ' '/)
  assert.match(commonJs, /data-guide-legacy-action/)
  assert.match(commonCss, /:focus-visible/)
  assert.match(commonCss, /@media \(prefers-reduced-motion:\s*reduce\)/)

  const regionGuide = await read('resources/web/guide/11/11.js')
  const printerGuide = await read('resources/web/guide/21/21.js')
  const filamentGuide = await read('resources/web/guide/23/23.js')
  assert.match(regionGuide, /attr\('aria-pressed', 'true'\)/)
  assert.match(printerGuide, /<button type="button" class="ModelCheckBox"/)
  assert.match(printerGuide, /aria-pressed="false"/)
  assert.match(filamentGuide, /<button type="button" onClick="CFEdit/)
  assert.match(filamentGuide, /attr\('aria-pressed', 'true'\)/)
})
