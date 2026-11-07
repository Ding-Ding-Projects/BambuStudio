'use strict';

const { randomUUID } = require('node:crypto');
const UUID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const METHODS = new Set(['inspect', 'configure', 'start', 'progress', 'cancel']);

function validateEnvelope(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value) ||
      Object.keys(value).some(key => !['schema', 'requestId', 'method', 'params'].includes(key)) ||
      value.schema !== 1 || !UUID.test(value.requestId) || !METHODS.has(value.method) ||
      !value.params || typeof value.params !== 'object' || Array.isArray(value.params)) {
    throw new Error('invalid_request');
  }
  return value;
}

// Keeps process ownership in the backend. This ledger never acts on a PID.
class OperationLedger {
  constructor({ capacity = 128 } = {}) {
    if (!Number.isSafeInteger(capacity) || capacity < 1 || capacity > 128) throw new Error('invalid_capacity');
    this.current = null;
    this.capacity = capacity;
    this.receipts = new Map();
  }

  start(requestId) {
    if (!UUID.test(requestId)) throw new Error('invalid_request');
    if (this.receipts.has(requestId)) return this.view(0, this.receipts.get(requestId));
    if (this.current && !this.current.terminal) throw new Error('operation_unresolved');
    // Never evict an identifier and accidentally reinterpret its replay as new.
    // A full session remains inspectable but cannot accept another start.
    if (this.receipts.size >= this.capacity) throw new Error('session_capacity');
    this.current = {
      id: randomUUID(), requestId, workerActive: true, cancelRequested: false,
      sequence: 0, events: [], terminal: null
    };
    this.event('inspect', 'active');
    this.receipts.set(requestId, this.current);
    return this.view();
  }

  require(id) {
    if (!this.current || id !== this.current.id) throw new Error('operation_unknown');
    return this.current;
  }

  event(phase, state) {
    if (!['inspect', 'acquire', 'validate', 'pull', 'start', 'local-health', 'lan-health', 'reconcile'].includes(phase) ||
        !['active', 'cancelling', 'terminal'].includes(state)) throw new Error('invalid_progress');
    const op = this.current;
    if (!op || op.terminal) throw new Error('operation_terminal');
    op.events.push({ schema: 1, operationId: op.id, sequence: ++op.sequence, phase, state });
    // The UI receives a bounded recent history and an explicit sequence cursor.
    if (op.events.length > 32) op.events.shift();
  }

  cancel(id) {
    const op = this.require(id);
    if (!op.terminal && !op.cancelRequested) {
      op.cancelRequested = true;
      this.event('reconcile', 'cancelling');
    }
    return { accepted: true, operationId: id, terminal: op.terminal };
  }

  workerExited(id) { this.require(id).workerActive = false; }

  reconcile(id, observation) {
    const op = this.require(id);
    if (op.terminal) return this.view();
    if (op.workerActive || observation?.identityVerified !== true || observation?.dataPreserved !== true ||
        !['running', 'stopped', 'absent'].includes(observation.serviceState)) return this.view();
    const outcome = op.cancelRequested ? 'cancelled' : observation.healthy === true && observation.serviceState === 'running' ? 'succeeded' : 'failed';
    this.event('reconcile', 'terminal');
    op.terminal = { outcome, reconciled: true, serviceState: observation.serviceState, dataPreserved: true };
    return this.view();
  }

  view(afterSequence = 0, operation = this.current) {
    if (!Number.isSafeInteger(afterSequence) || afterSequence < 0) throw new Error('invalid_cursor');
    if (!operation) return null;
    const op = operation;
    return { operationId: op.id, workerActive: op.workerActive, cancelRequested: op.cancelRequested,
      sequence: op.sequence, events: op.events.filter(e => e.sequence > afterSequence).map(e => ({ ...e })),
      terminal: op.terminal ? { ...op.terminal } : null };
  }
}

module.exports = { OperationLedger, validateEnvelope };
