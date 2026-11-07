'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const { randomUUID } = require('node:crypto');
const { OperationLedger, validateEnvelope } = require('./operation.cjs');

test('active and unresolved operations block duplicate workers', () => {
  const ledger = new OperationLedger(), request = randomUUID();
  const first = ledger.start(request);
  assert.equal(ledger.start(request).operationId, first.operationId);
  assert.throws(() => ledger.start(randomUUID()), /operation_unresolved/);
  ledger.workerExited(first.operationId);
  assert.throws(() => ledger.start(randomUUID()), /operation_unresolved/);
});

test('cancellation requires worker exit and verified reconciliation', () => {
  const ledger = new OperationLedger(), id = ledger.start(randomUUID()).operationId;
  const observation = { identityVerified: true, dataPreserved: true, serviceState: 'running', healthy: true };
  ledger.cancel(id);
  assert.equal(ledger.reconcile(id, observation).terminal, null);
  ledger.workerExited(id);
  assert.equal(ledger.reconcile(id, { ...observation, identityVerified: false }).terminal, null);
  assert.equal(ledger.reconcile(id, { ...observation, dataPreserved: false }).terminal, null);
  const result = ledger.reconcile(id, observation);
  assert.equal(result.operationId, id);
  assert.deepEqual(result.terminal, { outcome: 'cancelled', reconciled: true, serviceState: 'running', dataPreserved: true });
  assert.notEqual(ledger.start(randomUUID()).operationId, id);
});

test('unknown operation and arbitrary envelope fields are rejected', () => {
  const ledger = new OperationLedger(); ledger.start(randomUUID());
  assert.throws(() => ledger.cancel(randomUUID()), /operation_unknown/);
  const request = { schema: 1, requestId: randomUUID(), method: 'inspect', params: {} };
  assert.equal(validateEnvelope(request), request);
  for (const bad of [{ ...request, command: 'anything' }, { ...request, schema: 2 }, { ...request, method: 'execute' }]) {
    assert.throws(() => validateEnvelope(bad), /invalid_request/);
  }
});

test('bounded progress snapshots cannot mutate operation ownership', () => {
  const ledger = new OperationLedger(), id = ledger.start(randomUUID()).operationId;
  for (let i = 0; i < 40; ++i) ledger.event('pull', 'active');
  const view = ledger.view();
  assert.equal(view.events.length, 32);
  view.events[0].operationId = randomUUID();
  assert.equal(ledger.view().events[0].operationId, id);
  assert.throws(() => ledger.view(-1), /invalid_cursor/);
});

test('completed request replay survives later operations and capacity exhaustion', () => {
  const ledger = new OperationLedger({ capacity: 2 });
  const firstRequest = randomUUID(), secondRequest = randomUUID();
  function complete(request) {
    const id = ledger.start(request).operationId;
    ledger.workerExited(id);
    ledger.reconcile(id, { identityVerified: true, dataPreserved: true, serviceState: 'running', healthy: true });
    return id;
  }
  const first = complete(firstRequest), second = complete(secondRequest);
  assert.equal(ledger.start(firstRequest).operationId, first);
  assert.equal(ledger.start(firstRequest).terminal.outcome, 'succeeded');
  assert.equal(ledger.view().operationId, second);
  assert.throws(() => ledger.start(randomUUID()), /session_capacity/);
  assert.equal(ledger.start(secondRequest).operationId, second);
});
