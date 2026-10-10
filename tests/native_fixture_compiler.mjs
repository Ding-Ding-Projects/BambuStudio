// Compiles one single-file, non-window C++ fixture into fixture.exe.
//
// Windows runs MSVC from a developer environment, exactly as these fixtures
// were written for. Every other host (the Linux contract gate in particular)
// uses $CXX, or c++, at the same language level and warning level, so the
// same production methods still compile and execute there instead of the
// check failing before it starts because cl.exe does not exist.
import {spawnSync} from 'node:child_process';
import path from 'node:path';

export function compileFixture({source, exe, cwd, include}) {
  if (process.platform === 'win32') {
    return spawnSync('cl.exe', ['/nologo', '/std:c++17', '/EHsc', '/W4',
      ...(include ? ['/I' + include] : []), source, '/Fe:' + exe,
      '/Fo:' + path.join(cwd, 'fixture.obj')], {cwd, stdio: 'inherit'});
  }
  return spawnSync(process.env.CXX || 'c++', ['-std=c++17', '-Wall', '-Wextra',
    ...(include ? ['-I' + include] : []), source, '-o', exe], {cwd, stdio: 'inherit'});
}
