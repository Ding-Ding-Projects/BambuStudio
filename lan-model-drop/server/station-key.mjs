// Prints the station key for pasting into Bambu Studio:
//   docker compose exec lan-model-drop node server/station-key.mjs
// The key is printed only here, on request; the service never logs it.
import fs from 'node:fs';
import path from 'node:path';
import { ConfigError, readConfig } from './config.mjs';
import { STATION_KEY_FILE } from './secrets.mjs';

try {
  const config = readConfig(process.env);
  if (config.stationKey) {
    process.stdout.write(`${config.stationKey}\n`);
  } else {
    const file = path.join(config.dataDir, STATION_KEY_FILE);
    let key = '';
    try {
      key = fs.readFileSync(file, 'utf8').trim();
    } catch (error) {
      if (error.code !== 'ENOENT') throw error;
    }
    if (!key) {
      process.stderr.write('No station key yet. Start the service once; it creates the key on its first start.\n');
      process.exit(1);
    }
    process.stdout.write(`${key}\n`);
  }
} catch (error) {
  process.stderr.write(`lan-model-drop: ${error instanceof ConfigError ? error.message : 'could not read the station key.'}\n`);
  process.exit(1);
}
