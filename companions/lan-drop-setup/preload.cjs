'use strict';
const { contextBridge, ipcRenderer } = require('electron');
contextBridge.exposeInMainWorld('setup', Object.freeze({
  request: request => ipcRenderer.invoke('setup:request', request),
  open: index => ipcRenderer.invoke('setup:open', index)
}));
