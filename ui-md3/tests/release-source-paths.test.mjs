import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// cmake/modules/ReleaseSourcePaths.cmake maps physical build roots to logical
// names with MSVC /pathmap, for the application and, through
// CMAKE_PROJECT_INCLUDE, for every CMake dependency build
// (docs/integration/delivery-package-version.md). Dependencies copy
// CMAKE_C_FLAGS and CMAKE_CXX_FLAGS into generated CMake code: libpng 1.6.35
// writes `set(CMAKE_C_FLAGS @CMAKE_C_FLAGS@)` into scripts/genout.cmake, and
// the original OCCT package config quoted both. A native root such as D:\a\...
// in either variable then fails with "Invalid character escape '\a'" and stops
// the dependency build. The mapping therefore travels as C and C++ compile
// options and the module leaves both flag variables exactly as it found them.
//
// The fixture presents an MSVC identity without a compiler (project languages
// NONE), so it runs on any host with CMake. The user-profile root carries
// backslashes and the '\U' escape the hosted build rejected. It has no drive
// colon, which a non-Windows host reads as a path-list separator, and no space,
// which a non-Windows host escapes in its native spelling. The temporary
// project path does contain spaces.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = (...parts) => readFileSync(path.join(repoDir, ...parts), 'utf8').replace(/\r\n/g, '\n');
const cmakePath = (file) => file.replaceAll('\\', '/');
const modulePath = cmakePath(path.join(repoDir, 'cmake', 'modules', 'ReleaseSourcePaths.cmake'));
const cmakeMissing = spawnSync('cmake', ['--version']).status !== 0 && 'cmake is not installed';

const profile = String.raw`\\fixture-host\UserProfile\fixture`;
const profileForward = '//fixture-host/UserProfile/fixture';
const initialC = '/DWIN32 /D_WINDOWS /W3';
const initialCxx = '/DWIN32 /D_WINDOWS /W3 /GR /EHsc';
const languageGate = /^\$<\$<COMPILE_LANGUAGE:C,CXX>:(.*)>$/;

const fixtureProject = `cmake_minimum_required(VERSION 3.15)
project(ReleaseSourcePathsFixture NONE)
if(FIXTURE_ROUTE STREQUAL "application")
    include("${modulePath}")
endif()
set(facts "\${CMAKE_BINARY_DIR}/facts")
file(WRITE "\${facts}/c_flags" "\${CMAKE_C_FLAGS}")
file(WRITE "\${facts}/cxx_flags" "\${CMAKE_CXX_FLAGS}")
get_directory_property(options COMPILE_OPTIONS)
list(JOIN options "\\n" options_text)
file(WRITE "\${facts}/compile_options" "\${options_text}")
file(WRITE "\${facts}/path_flags" "\${_bambu_path_flags}")
file(WRITE "\${facts}/policy" "\${BAMBU_RELEASE_SOURCE_PATH_POLICY}")
file(WRITE "\${facts}/exe_linker_flags" "\${CMAKE_EXE_LINKER_FLAGS}")
# Re-embed the flags the way libpng's genout template and the original OCCT
# package config do, then parse the generated code.
file(WRITE "\${CMAKE_BINARY_DIR}/reembed.cmake.in" [=[
set(CMAKE_C_FLAGS @CMAKE_C_FLAGS@)
set(reembedded_c "@CMAKE_C_FLAGS@")
set(reembedded_cxx "@CMAKE_CXX_FLAGS@")
]=])
configure_file("\${CMAKE_BINARY_DIR}/reembed.cmake.in" "\${CMAKE_BINARY_DIR}/reembed.cmake" @ONLY)
function(parse_reembedded_flags)
    include("\${CMAKE_BINARY_DIR}/reembed.cmake")
endfunction()
parse_reembedded_flags()
message(STATUS "REEMBEDDED_FLAGS_PARSED")
`;

// The MSVC identity the module requires, set before project() as a
// CMAKE_PROJECT_INCLUDE_BEFORE file so both routes see it.
const msvcIdentity = `set(MSVC 1)
set(MSVC_VERSION 1951)
set(CMAKE_C_COMPILER_ID MSVC)
set(CMAKE_CXX_COMPILER_ID MSVC)
`;

function configure(route) {
  const directory = mkdtempSync(path.join(tmpdir(), 'release source paths '));
  try {
    const source = path.join(directory, 'source');
    const binary = path.join(directory, 'build');
    mkdirSync(source);
    writeFileSync(path.join(source, 'CMakeLists.txt'), fixtureProject);
    writeFileSync(path.join(directory, 'msvc-identity.cmake'), msvcIdentity);
    const args = ['-S', source, '-B', binary, `-DFIXTURE_ROUTE=${route}`,
      `-DCMAKE_PROJECT_INCLUDE_BEFORE=${cmakePath(path.join(directory, 'msvc-identity.cmake'))}`,
      `-DCMAKE_C_FLAGS=${initialC}`, `-DCMAKE_CXX_FLAGS=${initialCxx}`];
    // deps/CMakeLists.txt hands every dependency the module this way.
    if (route === 'dependency') args.push(`-DCMAKE_PROJECT_INCLUDE=${modulePath}`);
    const result = spawnSync('cmake', args, { encoding: 'utf8', env: { ...process.env, USERPROFILE: profile } });
    const fact = (name) => {
      try { return readFileSync(path.join(binary, 'facts', name), 'utf8'); } catch { return undefined; }
    };
    return {
      result, source, binary,
      cFlags: fact('c_flags'), cxxFlags: fact('cxx_flags'), pathFlags: fact('path_flags'),
      policy: fact('policy'), exeLinkerFlags: fact('exe_linker_flags'),
      options: fact('compile_options')?.split('\n') ?? [],
    };
  } finally {
    rmSync(directory, { recursive: true, force: true });
  }
}

const sameRoot = (left, right) => process.platform === 'win32'
  ? cmakePath(left).toLowerCase() === cmakePath(right).toLowerCase()
  : left === right;

for (const route of ['application', 'dependency']) {
  test(`the ${route} route keeps both flag variables free of path maps`, { skip: cmakeMissing }, () => {
    const fixture = configure(route);
    const output = `${fixture.result.stdout}${fixture.result.stderr}`;
    assert.equal(fixture.cFlags, initialC, 'CMAKE_C_FLAGS must leave the module unchanged');
    assert.equal(fixture.cxxFlags, initialCxx, 'CMAKE_CXX_FLAGS must leave the module unchanged');
    assert.equal(fixture.result.status, 0, `configure must succeed:\n${output}`);
    assert.match(output, /REEMBEDDED_FLAGS_PARSED/, 'flags re-embedded the way libpng and OCCT do must parse');
    assert.doesNotMatch(output, /Invalid (?:character escape|escape sequence)/);
  });

  test(`the ${route} route maps every root as C and C++ compile options`, { skip: cmakeMissing }, () => {
    const fixture = configure(route);
    assert.equal(fixture.result.status, 0, `${fixture.result.stdout}${fixture.result.stderr}`);
    for (const option of fixture.options) {
      assert.match(option, languageGate, `${option} must reach only C and C++ sources`);
    }
    const options = fixture.options.map((option) => option.match(languageGate)?.[1]);
    assert.equal(options[0], '/experimental:deterministic');
    const maps = options.slice(1).map((option) => {
      const match = option.match(/^\/pathmap:(.+)=([^=]+)$/);
      assert.ok(match, `${option} must be one unquoted /pathmap:<physical>=<logical> option`);
      assert.ok(!option.includes('"'), `${option} must not carry command-line quotes`);
      return { physical: match[1], logical: match[2] };
    });
    // The user-profile root keeps its native backslash spelling and gains its
    // normalized forward-slash spelling, exactly as before this change.
    assert.deepEqual(maps.filter((map) => map.logical === 'build/host').map((map) => map.physical), [profile, profileForward]);
    const mapped = (logical, root) => maps.some((map) => map.logical === logical && sameRoot(map.physical, root));
    assert.ok(mapped('source/BambuStudio', repoDir), 'the repository root maps to source/BambuStudio');
    assert.ok(mapped('source/ReleaseSourcePathsFixture', fixture.source), 'the project source root maps to its logical name');
    assert.ok(mapped('build/ReleaseSourcePathsFixture', fixture.binary), 'the project build root maps to its logical name');
    // Each root's spellings are adjacent, and roots are emitted longest first
    // (measured by the forward-slash spelling, which every root has).
    const labels = maps.map((map) => map.logical).filter((label, index, all) => label !== all[index - 1]);
    assert.deepEqual(labels, [...new Set(labels)], 'the spellings of one root are emitted together');
    const lengths = labels.map((label) => maps.find((map) => map.logical === label && !map.physical.includes('\\'))?.physical.length);
    assert.ok(lengths.every(Number.isInteger), 'every root has a forward-slash spelling');
    assert.deepEqual(lengths, [...lengths].sort((a, b) => b - a), 'longer physical prefixes are emitted first');
    assert.equal(fixture.policy, 'msvc-pathmap-v1');
    assert.match(fixture.exeLinkerFlags, /\/PDBALTPATH:%_PDB%/);
    // Producers built outside CMake still read the same map as one quoted
    // command-line string through the _CL_ environment variable.
    assert.ok(fixture.pathFlags.startsWith('/experimental:deterministic /pathmap:"'));
    assert.ok(fixture.pathFlags.includes(`/pathmap:"${profile}=build/host" /pathmap:"${profileForward}=build/host"`));
  });
}

test('every reader of the policy and of the quoted map uses the module as written', () => {
  const module = read('cmake', 'modules', 'ReleaseSourcePaths.cmake');
  const policy = module.match(/set\(BAMBU_RELEASE_SOURCE_PATH_POLICY "([^"]+)" CACHE INTERNAL/)?.[1];
  assert.equal(policy, 'msvc-pathmap-v1');
  assert.ok(read('scripts', 'windows', 'Invoke-OneClickBuild.ps1').includes(`BAMBU_RELEASE_SOURCE_PATH_POLICY:INTERNAL=${policy}`));
  assert.match(read('deps', 'CMakeLists.txt'),
    /-DCMAKE_PROJECT_INCLUDE:FILEPATH=\$\{PROJECT_SOURCE_DIR\}\/\.\.\/cmake\/modules\/ReleaseSourcePaths\.cmake/);
  assert.match(read('deps', 'OpenSSL', 'OpenSSL.cmake'), /"_CL_=\$ENV\{_CL_\} \$\{_bambu_path_flags\}"/);
  assert.doesNotMatch(module, /(?:string\(APPEND|set\()\s*CMAKE_(?:C|CXX)_FLAGS\b/,
    'the module must not write CMAKE_C_FLAGS or CMAKE_CXX_FLAGS');
});
