'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const { randomUUID } = require('node:crypto');
const { Controller, checkPort } = require('./controller.cjs');
const net = require('node:net');
const envelope = (method, params = {}) => ({ schema: 1, requestId: randomUUID(), method, params });
const fixture = { ownership: 'existing-owned', containerId: 'a'.repeat(64), port: 18833, storage: 'existing', running: false, ready: true };
async function prepared(controller) {
  const snapshot = await controller.dispatch(envelope('inspect'));
  return controller.dispatch(envelope('configure', { snapshotId: snapshot.snapshotId, port: snapshot.port,
    stationName: 'Preserved station', storage: 'existing', credentialMode: 'preserve' }));
}
async function settle(controller) {
  for (let i = 0; i < 30 && !controller.ledger.current.terminal; ++i) await new Promise(resolve => setImmediate(resolve));
}

test('existing service starts in place without pull or credential transport', async () => {
  const calls = [];
  const controller = new Controller('unused', async (_script, request) => {
    calls.push(request);
    if (request.method === 'inspect') return fixture;
    if (request.method === 'start') return { containerId: fixture.containerId };
    if (request.method === 'health') return { identityVerified: true, dataPreserved: true, healthy: true, serviceState: 'running', urls: [] };
    throw new Error('unexpected_method');
  });
  const config = await prepared(controller);
  await controller.dispatch(envelope('start', { configurationId: config.configurationId, snapshotId: config.snapshotId }));
  await settle(controller);
  assert.equal(controller.ledger.view().terminal.outcome, 'succeeded');
  assert.equal(calls.filter(c => c.method === 'start').length, 1);
  assert.equal(calls.some(c => c.method === 'pull'), false);
  assert.equal(calls.some(c => Object.hasOwn(c, 'stationKey')), false);
});

test('cancel waits for the owned phase then reconciles retained service', async () => {
  let release;
  const controller = new Controller('unused', async (_script, request) => {
    if (request.method === 'inspect') return fixture;
    if (request.method === 'start') return new Promise(resolve => { release = () => resolve({ containerId: fixture.containerId }); });
    return { identityVerified: true, dataPreserved: true, healthy: true, serviceState: 'running', urls: [] };
  });
  const config = await prepared(controller);
  const op = await controller.dispatch(envelope('start', { configurationId: config.configurationId, snapshotId: config.snapshotId }));
  await new Promise(resolve => setImmediate(resolve));
  await controller.dispatch(envelope('cancel', { operationId: op.operationId }));
  assert.equal(controller.ledger.view().terminal, null);
  release(); await settle(controller);
  assert.equal(controller.ledger.view().terminal.outcome, 'cancelled');
  assert.equal(controller.ledger.view().terminal.serviceState, 'running');
});

test('configuration cannot change existing port or pass custom credentials', async () => {
  const controller = new Controller('unused', async () => fixture);
  const config = await prepared(controller);
  const params = { snapshotId: config.snapshotId, port: 8833, stationName: 'Other', storage: 'existing', credentialMode: 'preserve' };
  await assert.rejects(controller.dispatch(envelope('configure', params)), /existing_settings_preserved/);
  await assert.rejects(controller.dispatch(envelope('configure', { ...params, port: fixture.port, stationKey: 'not accepted' })), /invalid_configuration/);
});

test('an existing low-numbered port remains preservable', async () => {
  const controller = new Controller('unused', async () => ({ ...fixture, port: 80 }));
  const configuration = await prepared(controller);
  assert.equal(configuration.port, 80);
});

test('fresh configuration rejects an occupied host port', async () => {
  const listener = net.createServer();
  await new Promise(resolve => listener.listen({ host: '0.0.0.0', port: 0, exclusive: true }, resolve));
  try { await assert.rejects(checkPort(listener.address().port), /port_unavailable/); }
  finally { await new Promise(resolve => listener.close(resolve)); }
});

test('fresh preflight rejection reconciles verified absence and permits a new reviewed request', async () => {
  const fresh = { ...fixture, ownership: 'fresh', containerId: '', storage: 'new-volume', dataPresent: false };
  const controller = new Controller('unused', async (_script, request) => {
    if (request.method === 'inspect') return fresh;
    if (request.method === 'pull') return { containerId: '' };
    const error = new Error('port_unavailable'); error.mutationAttempted = false; throw error;
  });
  // Use a real ephemeral port only for validation, never start a service.
  const probe = net.createServer();
  await new Promise(resolve => probe.listen(0, '127.0.0.1', resolve));
  const port = probe.address().port;
  await new Promise(resolve => probe.close(resolve));
  const snapshot = await controller.dispatch(envelope('inspect'));
  const params = { snapshotId: snapshot.snapshotId, port, stationName: 'Fixture', storage: 'new-volume', credentialMode: 'generate' };
  const config = await controller.dispatch(envelope('configure', params));
  const first = await controller.dispatch(envelope('start', { configurationId: config.configurationId, snapshotId: config.snapshotId }));
  await settle(controller);
  assert.deepEqual(controller.ledger.view().terminal, { outcome: 'failed', reconciled: true, serviceState: 'absent', dataPreserved: true });
  const inspected = await controller.dispatch(envelope('inspect', { operationId: first.operationId }));
  assert.equal(inspected.operation.operationId, first.operationId);
  const next = await controller.dispatch(envelope('configure', params));
  assert.notEqual(next.configurationId, config.configurationId);
});
