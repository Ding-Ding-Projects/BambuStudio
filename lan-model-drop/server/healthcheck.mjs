// Container health check: exits 0 when /healthz answers as expected.
import http from 'node:http';
import { LISTEN_PORT, PROTOCOL, SERVICE } from './config.mjs';

const request = http.get({ host: '127.0.0.1', port: LISTEN_PORT, path: '/healthz', timeout: 3000 }, (response) => {
  let body = '';
  response.setEncoding('utf8');
  response.on('data', (chunk) => {
    if (body.length < 4096) body += chunk;
  });
  response.on('end', () => {
    try {
      const health = JSON.parse(body);
      const healthy = response.statusCode === 200 && health.ok === true && health.service === SERVICE && health.protocol === PROTOCOL;
      process.exit(healthy ? 0 : 1);
    } catch {
      process.exit(1);
    }
  });
});
request.on('timeout', () => request.destroy(new Error('timeout')));
request.on('error', () => process.exit(1));
