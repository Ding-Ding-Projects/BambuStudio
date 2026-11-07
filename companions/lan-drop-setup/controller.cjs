'use strict';
const { spawn } = require('node:child_process');
const { randomUUID } = require('node:crypto');
const path = require('node:path');
const net = require('node:net');
const { OperationLedger, validateEnvelope } = require('./operation.cjs');

function backend(script, request) {
  return new Promise((resolve, reject) => {
    const executable = path.join(process.env.SystemRoot, 'System32', 'WindowsPowerShell', 'v1.0', 'powershell.exe');
    const child = spawn(executable, ['-NoLogo', '-NoProfile', '-NonInteractive', '-File', script], {
      windowsHide: true, stdio: ['pipe', 'pipe', 'pipe'], shell: false
    });
    let output = '', overflow = false;
    child.stdout.on('data', chunk => {
      if (Buffer.byteLength(output) + chunk.length > 8192) { overflow = true; return; }
      output += chunk.toString('utf8');
    });
    child.stderr.on('data', () => {});
    child.stdin.on('error', () => {});
    child.on('error', () => reject(new Error('backend_unavailable')));
    child.on('close', () => {
      try {
        if (overflow) throw new Error();
        const response = JSON.parse(output);
        if (response.schema !== 1 || typeof response.ok !== 'boolean') throw new Error();
        if (!response.ok) {
          const code = response.error?.code;
          const error = new Error(/^[a-z_]{1,64}$/.test(code) ? code : 'backend_failed');
          if (typeof response.error?.mutationAttempted === 'boolean') error.mutationAttempted = response.error.mutationAttempted;
          reject(error);
        } else resolve(response.result);
      } catch { reject(new Error('backend_invalid_result')); }
    });
    child.stdin.end(JSON.stringify({ schema: 1, ...request }) + '\n');
    // Cancellation is cooperative between bounded backend phases. No PID kill.
  });
}

function checkPort(port) {
  return new Promise((resolve, reject) => {
    const probe = net.createServer();
    probe.once('error', () => reject(new Error('port_unavailable')));
    probe.listen({ host: '0.0.0.0', port, exclusive: true }, () => probe.close(error => error ? reject(new Error('port_unavailable')) : resolve()));
  });
}

class Controller {
  constructor(script, invoke = backend) {
    this.script = script; this.invoke = invoke; this.ledger = new OperationLedger();
    this.snapshot = null; this.configuration = null; this.urls = []; this.busy = false;
  }
  call(request) { return this.invoke(this.script, request); }
  async inspect(operationId) {
    if (operationId) this.ledger.require(operationId);
    const result = await this.call({ method: 'inspect' });
    if (!operationId) {
      this.snapshot = { ...result, snapshotId: randomUUID() };
      this.configuration = null;
    } else if (!this.ledger.require(operationId).workerActive) {
      const op = this.ledger.require(operationId);
      if (result.containerId) {
        if (op.containerId && op.containerId !== result.containerId) throw new Error('service_changed');
        const observation = await this.call({ method: 'health', containerId: result.containerId });
        this.urls = observation.urls || [];
        this.ledger.reconcile(operationId, observation);
      } else if (!op.startAttempted && !op.containerId && result.dataPresent === false) {
        this.ledger.reconcile(operationId, { identityVerified: true, dataPreserved: true, serviceState: 'absent', healthy: false });
      }
    }
    return operationId ? { operation: this.ledger.view(), urls: this.urls } : this.snapshot;
  }
  async configure(params) {
    if (!this.snapshot || params.snapshotId !== this.snapshot.snapshotId) throw new Error('stale_snapshot');
    if (this.ledger.current && !this.ledger.current.terminal) throw new Error('operation_unresolved');
    if (Object.keys(params).some(k => !['snapshotId', 'port', 'stationName', 'storage', 'credentialMode'].includes(k))) throw new Error('invalid_configuration');
    if (params.credentialMode !== (this.snapshot.containerId ? 'preserve' : 'generate')) throw new Error('custom_credential_unavailable');
    if (params.storage !== (this.snapshot.containerId ? 'existing' : 'new-volume')) throw new Error('folder_storage_unavailable');
    if (!Number.isInteger(params.port) || params.port < (this.snapshot.containerId ? 1 : 1024) || params.port > 65535 ||
        typeof params.stationName !== 'string' || !params.stationName.trim() || params.stationName.length > 80 || /[\x00-\x1f\x7f]/.test(params.stationName)) throw new Error('invalid_configuration');
    if (this.snapshot.containerId && params.port !== this.snapshot.port) throw new Error('existing_settings_preserved');
    if (!this.snapshot.containerId) await checkPort(params.port);
    this.configuration = { ...params, configurationId: randomUUID(), expires: Date.now() + 600000, containerId: this.snapshot.containerId };
    return { ...this.configuration };
  }
  async run(id, config) {
    const op = this.ledger.require(id);
    op.containerId = config.containerId; op.startAttempted = false;
    try {
      const current = await this.call({ method: 'inspect' });
      if (current.containerId !== config.containerId) throw new Error('service_changed');
      if (!op.cancelRequested && !config.containerId) {
        this.ledger.event('pull', 'active');
        await this.call({ method: 'pull', ...config });
      }
      if (!op.cancelRequested) {
        this.ledger.event('start', 'active'); op.startAttempted = true;
        const result = await this.call({ method: 'start', ...config });
        op.containerId = result.containerId;
      }
    } catch (error) {
      op.errorCode = /^[a-z_]{1,64}$/.test(error.message) ? error.message : 'operation_failed';
      if (error.mutationAttempted === false) op.startAttempted = false;
    }
    finally {
      this.ledger.workerExited(id);
      try { await this.inspect(id); } catch { op.errorCode ||= 'reconciliation_required'; }
    }
  }
  async dispatch(request) {
    validateEnvelope(request);
    const p = request.params;
    if (request.method === 'cancel') {
      if (Object.keys(p).some(k => k !== 'operationId')) throw new Error('invalid_request');
      return this.ledger.cancel(p.operationId);
    }
    if (request.method === 'progress') {
      if (Object.keys(p).some(k => !['operationId', 'afterSequence'].includes(k))) throw new Error('invalid_request');
      const op = this.ledger.require(p.operationId);
      return { operation: this.ledger.view(p.afterSequence || 0), urls: this.urls, errorCode: op.errorCode || null };
    }
    if (this.busy) throw new Error('request_in_progress');
    this.busy = true;
    try {
      if (request.method === 'inspect') {
        if (Object.keys(p).some(k => k !== 'operationId')) throw new Error('invalid_request');
        return await this.inspect(p.operationId);
      }
      if (request.method === 'configure') return await this.configure(p);
      if (Object.keys(p).some(k => !['configurationId', 'snapshotId'].includes(k))) throw new Error('invalid_request');
      if (this.ledger.receipts.has(request.requestId)) return this.ledger.start(request.requestId);
      const config = this.configuration;
      if (!config || config.configurationId !== p.configurationId || config.snapshotId !== p.snapshotId || config.expires < Date.now()) throw new Error('configuration_expired');
      const result = this.ledger.start(request.requestId);
      this.configuration = null;
      void this.run(result.operationId, config);
      return result;
    } finally { this.busy = false; }
  }
}
module.exports = { Controller, backend, checkPort };
