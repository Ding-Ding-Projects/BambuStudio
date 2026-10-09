// Service worker entry point. The listeners are registered synchronously at
// start-up so the browser can wake this worker for a download, a menu click or
// a settings change.
import { createCaptureService } from './capture-service.js';

createCaptureService(chrome).install();
