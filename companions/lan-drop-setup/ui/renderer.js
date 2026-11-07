'use strict';
const $ = id => document.getElementById(id);
let language = localStorage.getItem('setup-language') || 'en', theme = localStorage.getItem('setup-theme') || 'light';
let step = 0, snapshot = null, configuration = null, operation = null, busy = false, message = '', urls = [];
const text = (en, yue) => language === 'yue' ? yue : language === 'both' ? en + ' / ' + yue : en;
const headings = [['Check hosting requirements', '檢查託管所需工具'], ['Configure hosting', '設定託管'], ['Review changes', '確認更改'], ['Starting and verifying', '啟動同驗證'], ['Setup result', '設定結果']];
const errors = {
  docker_missing: ['Docker is not installed. Automatic prerequisite acquisition is not available in this initial implementation.', '尚未安裝 Docker。呢個初步版本未提供自動安裝所需工具。'],
  engine_unavailable: ['Docker is unavailable. Start the existing engine, then check again.', 'Docker 暫時無法使用。請啟動現有引擎，再檢查一次。'],
  compose_unavailable: ['Docker Compose is unavailable.', 'Docker Compose 暫時無法使用。'],
  existing_storage_requires_review: ['Existing stored data needs review before a fresh service can be created. Nothing was removed.', '建立全新服務前，需要先檢查現有儲存資料。冇移除任何資料。'],
  service_changed: ['The service changed after inspection. Inspect again before starting.', '服務喺檢查後有變動。請重新檢查先再啟動。'],
  service_conflict: ['The discovered service does not match this setup. It was preserved.', '搵到嘅服務唔符合呢個設定。已保留原有服務。'],
  configuration_expired: ['Configuration expired. Return to configuration and review again.', '設定已過期。請返回設定，再確認一次。'],
  invalid_configuration: ['Enter a station name and a port from 1024 to 65535.', '請輸入站台名稱，以及 1024 至 65535 之間嘅連接埠。']
};
function errorText(code) { return errors[code] ? text(...errors[code]) : text('The operation did not complete. Safe result code: ', '操作未完成。安全結果代碼：') + code; }
async function request(method, params = {}) {
  const reply = await window.setup.request({ schema: 1, requestId: crypto.randomUUID(), method, params });
  if (!reply.ok) throw new Error(reply.error.code);
  return reply.result;
}
function render(focus = false) {
  document.documentElement.lang = language === 'yue' ? 'yue-HK' : 'en';
  document.body.classList.toggle('dark', theme === 'dark');
  $('heading').textContent = text(...headings[step]);
  $('description').textContent = text('Keep existing service data and credentials. A fresh setup uses a new dedicated volume and a securely generated station credential.', '保留現有服務資料同憑證。全新設定會使用新嘅專用磁碟區，同安全產生嘅站台憑證。');
  $('status').textContent = message;
  $('fields').hidden = step !== 1; $('review').hidden = step !== 2; $('result').hidden = step !== 4;
  $('name-label').textContent = text('Station name', '站台名稱'); $('port-label').textContent = text('Host port', '主機連接埠');
  $('name').disabled = !!snapshot?.containerId; $('port').disabled = !!snapshot?.containerId;
  $('storage').textContent = snapshot?.containerId ? text('Existing storage and station name will be retained.', '會保留現有儲存空間同站台名稱。') : text('Storage: a new dedicated named volume.', '儲存：新嘅專用命名磁碟區。');
  $('credential').textContent = snapshot?.containerId ? text('Existing credential retained. Its value is never displayed.', '保留現有憑證，唔會顯示憑證值。') : text('Generate a station credential inside the service. Custom input is unavailable until its protected server entrypoint is implemented.', '喺服務內產生站台憑證。受保護嘅伺服器輸入功能完成之前，未能使用自訂憑證。');
  $('limitations').textContent = text('Initial implementation: folder selection, automatic prerequisite installation, custom credentials, resume and the full settings inventory remain unfinished.', '初步版本：資料夾選擇、自動安裝所需工具、自訂憑證、繼續上次設定同完整設定項目仍未完成。');
  $('back').textContent = text('Back', '上一步'); $('back').disabled = busy || step === 0 || !!operation && !operation.terminal;
  $('cancel').textContent = text('Cancel and reconcile', '取消並核實狀態'); $('cancel').hidden = !operation || !!operation.terminal;
  $('cancel').disabled = !!operation?.cancelRequested;
  $('next').textContent = step === 0 ? text('Check requirements', '檢查所需工具') : step === 2 ? text('Start service', '啟動服務') : step === 3 ? text('Recheck actual state', '重新檢查實際狀態') : step === 4 ? text('Inspect again', '再次檢查') : text('Review', '確認');
  $('next').disabled = busy || step === 3 && !!operation?.workerActive;
  $('position').textContent = text('Step ', '步驟 ') + (step + 1) + '/5';
  if (configuration) $('review').textContent = text('Port: ', '連接埠：') + configuration.port + '\n' + text('Existing data and credentials are preserved. Only the owned service will be started. No public Internet exposure or host restart.', '會保留現有資料同憑證，只啟動自有服務。唔會公開到互聯網或重新啟動主機。');
  if (step === 4) {
    $('result').replaceChildren();
    const resultText = document.createElement('p');
    resultText.textContent = operation?.terminal?.outcome === 'succeeded' ? text('Service healthy. LAN URLs were checked from this host; another device has not been verified.', '服務正常。已從本機檢查區域網絡網址，尚未驗證另一部裝置。') : text('Operation reconciled. Retained service state: ', '已核實操作結果。保留嘅服務狀態：') + (operation?.terminal?.serviceState || 'unknown');
    $('result').append(resultText);
    if (operation?.terminal?.outcome === 'succeeded') urls.forEach((url, index) => {
      const label = document.createElement('p'); label.textContent = url;
      const open = document.createElement('button'); open.textContent = text('Open sender page', '開啟傳送頁面');
      open.onclick = async () => { if (!await window.setup.open(index)) { message = errorText('open_unavailable'); render(); } };
      $('result').append(label, open);
    });
  }
  if (focus) $('heading').focus();
}
async function action(fn) {
  if (busy) return; busy = true; render();
  try { await fn(); } catch (error) { message = errorText(error.message); }
  finally { busy = false; render(true); }
}
$('next').onclick = () => action(async () => {
  message = '';
  if (step === 0 || step === 4) {
    snapshot = await request('inspect'); operation = null; configuration = null; urls = [];
    $('port').value = snapshot.port; step = 1;
  } else if (step === 1) {
    configuration = await request('configure', { snapshotId: snapshot.snapshotId, port: Number($('port').value), stationName: $('name').value,
      storage: snapshot.containerId ? 'existing' : 'new-volume', credentialMode: snapshot.containerId ? 'preserve' : 'generate' });
    step = 2;
  } else if (step === 2) {
    operation = await request('start', { configurationId: configuration.configurationId, snapshotId: snapshot.snapshotId }); step = 3; void poll();
  } else if (step === 3) {
    const state = await request('inspect', { operationId: operation.operationId });
    operation = state.operation; urls = state.urls; if (operation.terminal) step = 4;
  }
});
$('back').onclick = () => { if (busy || operation && !operation.terminal) return; step = Math.max(0, step - 1); configuration = null; render(true); };
$('cancel').onclick = () => action(async () => { await request('cancel', { operationId: operation.operationId }); message = text('Cancellation requested. Waiting for the owned phase to finish, then reconciling service state.', '已要求取消。等自有階段結束後，會核實服務狀態。'); });
async function poll() {
  while (operation && !operation.terminal && step === 3) {
    try {
      const state = await request('progress', { operationId: operation.operationId, afterSequence: 0 });
      operation = state.operation; urls = state.urls;
      message = state.errorCode ? errorText(state.errorCode) : text('Current phase: ', '目前階段：') + (operation.events.at(-1)?.phase || 'inspect');
      if (operation.terminal) step = 4;
      render();
      if (!operation.workerActive && !operation.terminal) break;
    } catch (error) { message = errorText(error.message); render(); break; }
    await new Promise(resolve => setTimeout(resolve, 750));
  }
}
$('language').value = language; $('theme').value = theme;
$('language').onchange = () => { language = $('language').value; localStorage.setItem('setup-language', language); render(); };
$('theme').onchange = () => { theme = $('theme').value; localStorage.setItem('setup-theme', theme); render(); };
render();
