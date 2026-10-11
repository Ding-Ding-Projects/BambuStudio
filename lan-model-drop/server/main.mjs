// Starts the LAN model drop service: node server/main.mjs
import { createDropServer } from './app.mjs';
import { ConfigError, LISTEN_PORT, readConfig } from './config.mjs';

function log(message) {
  process.stdout.write(`${new Date().toISOString()} ${message}\n`);
}

let config;
try {
  config = readConfig(process.env);
} catch (error) {
  if (error instanceof ConfigError) {
    process.stderr.write(`lan-model-drop: ${error.message}\n`);
    process.exit(2);
  }
  throw error;
}

const drop = createDropServer(config, { log });
const address = await drop.listen(LISTEN_PORT, '0.0.0.0');
log(`lan-model-drop listening on port ${address.port} inside the container (station "${config.stationName}")`);

// The service is process 1 in its container, so it ends itself on a stop
// signal instead of waiting to be killed.
let stopping = false;
for (const signal of ['SIGTERM', 'SIGINT']) {
  process.on(signal, async () => {
    if (stopping) return;
    stopping = true;
    log(`stopping on ${signal}`);
    const forced = setTimeout(() => process.exit(0), 5000);
    forced.unref();
    await drop.close();
    process.exit(0);
  });
}
