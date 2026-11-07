'use strict';
const { app, BrowserWindow, ipcMain, dialog, shell, session } = require('electron');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { Controller } = require('./controller.cjs');
let window, controller, closing = false;
const page = pathToFileURL(path.join(__dirname, 'ui', 'index.html')).href;
if (!app.requestSingleInstanceLock()) app.quit();
else {
  app.on('second-instance', () => { if (window) { window.restore(); window.focus(); } });
  app.whenReady().then(() => {
    const resources = app.isPackaged ? process.resourcesPath : path.resolve(__dirname, '../..');
    controller = new Controller(path.join(resources, 'lan-model-drop', 'wizard-host.ps1'));
    session.defaultSession.setPermissionRequestHandler((_contents, _permission, reply) => reply(false));
    session.defaultSession.setPermissionCheckHandler(() => false);
    window = new BrowserWindow({ width: 960, height: 720, minWidth: 640, minHeight: 520, show: false,
      autoHideMenuBar: true, webPreferences: { preload: path.join(__dirname, 'preload.cjs'), contextIsolation: true,
        sandbox: true, nodeIntegration: false, webSecurity: true, spellcheck: false } });
    window.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
    window.webContents.on('will-navigate', event => event.preventDefault());
    window.once('ready-to-show', () => window.show());
    window.on('close', event => {
      const op = controller.ledger.current;
      if (!op || op.terminal || closing) return;
      event.preventDefault();
      if (window.closePrompt) return;
      window.closePrompt = true;
      void dialog.showMessageBox(window, { type: 'question', buttons: ['Stay active', 'Cancel and reconcile'], defaultId: 0,
        cancelId: 0, message: 'Setup is still active or unresolved.', detail: 'Cancellation preserves the service and its data. The window stays open until its actual state is reconciled.' })
        .then(({ response }) => { if (response === 1) controller.ledger.cancel(op.id); })
        .finally(() => { window.closePrompt = false; });
    });
    ipcMain.handle('setup:request', async (event, request) => {
      if (event.sender !== window.webContents || event.senderFrame !== window.webContents.mainFrame || event.senderFrame.url !== page) {
        return { schema: 1, ok: false, error: { code: 'sender_rejected' } };
      }
      try {
        if (Buffer.byteLength(JSON.stringify(request), 'utf8') > 8192) throw new Error('request_too_large');
        const result = await controller.dispatch(request);
        return { schema: 1, requestId: request.requestId, ok: true, result };
      } catch (error) {
        return { schema: 1, requestId: request?.requestId, ok: false,
          error: { code: /^[a-z_]{1,64}$/.test(error.message) ? error.message : 'request_failed' } };
      }
    });
    ipcMain.handle('setup:open', async (event, index) => {
      if (event.sender !== window.webContents || event.senderFrame !== window.webContents.mainFrame || event.senderFrame.url !== page || !Number.isInteger(index)) return false;
      const candidate = controller.urls[index];
      if (!candidate || controller.ledger.current?.terminal?.outcome !== 'succeeded') return false;
      const url = new URL(candidate);
      if (url.protocol !== 'http:' || url.username || url.password || url.search || url.hash || url.pathname !== '/' ||
          !/^(10\.|192\.168\.|172\.(1[6-9]|2\d|3[01])\.)/.test(url.hostname)) return false;
      await shell.openExternal(url.href); return true;
    });
    void window.loadURL(page);
  });
  app.on('window-all-closed', () => app.quit());
}
