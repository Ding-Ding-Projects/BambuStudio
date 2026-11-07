'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const { randomUUID } = require('node:crypto');
const { Controller } = require('./controller.cjs');
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
