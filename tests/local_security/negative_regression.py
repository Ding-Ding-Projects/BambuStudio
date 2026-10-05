"""Build isolated, deliberately broken copies and require behavioral rejection.

This never changes the checked-out source. Requires a C++17 compiler with the
existing OpenSSL development package. Windows also requires advapi32.
"""
import argparse
import pathlib
import subprocess
import sys
import tempfile
import os

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default='g++')
parser.add_argument('--msvc', action='store_true')
parser.add_argument('--openssl-include', type=pathlib.Path)
parser.add_argument('--openssl-library', type=pathlib.Path)
parser.add_argument('--openssl-dll', type=pathlib.Path)
parser.add_argument('--qr-only', action='store_true')
parser.add_argument('--core-only', action='store_true')
parser.add_argument('--history-only', action='store_true')
parser.add_argument('--libgit2-prefix', type=pathlib.Path)
parser.add_argument('--qr-python-packages', type=pathlib.Path)
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[2]
source = root / 'src/libslic3r/LocalSecurity'
base = (source / 'LocalSecurity.cpp').read_text(encoding='utf-8')
mutations = [
    ('policy-pin', 'case Policy::Pin:return {Factor::Pin};', 'case Policy::Pin:throw Failure(Error::InvalidInput);'),
    ('policy-password', 'case Policy::Password:return {Factor::Password};', 'case Policy::Password:throw Failure(Error::InvalidInput);'),
    ('policy-pin-password', 'case Policy::PinPassword:return {Factor::Pin,Factor::Password};', 'case Policy::PinPassword:throw Failure(Error::InvalidInput);'),
    ('policy-password-totp', 'case Policy::PasswordTotp:return {Factor::Password,Factor::Totp};', 'case Policy::PasswordTotp:throw Failure(Error::InvalidInput);'),
    ('policy-pin-totp', 'case Policy::PinTotp:return {Factor::Pin,Factor::Totp};', 'case Policy::PinTotp:throw Failure(Error::InvalidInput);'),
    ('policy-password-pin-totp', 'case Policy::PasswordPinTotp:return {Factor::Password,Factor::Pin,Factor::Totp};', 'case Policy::PasswordPinTotp:throw Failure(Error::InvalidInput);'),
    ('credential-verification', 'return CRYPTO_memcmp(candidate.data(),r->data()+22,32)==0;', 'return true;'),
    ('factor-order', 'if(f!=*e||!verified)', 'if(!verified)'),
    ('factor-completeness', 'if(++m_step!=m_factors.size())return false;', '++m_step;'),
    ('expiry', 'now>=m_until)relock();', 'false)relock();'),
    ('attempt-budget', 'if(!m_remaining) {unsigned seconds=', 'if(false) {unsigned seconds='),
    ('snapshot-authentication', 'require(EVP_DecryptFinal_ex(ctx.get(),out.data()+total,&n)==1,Error::Corrupt);', 'EVP_DecryptFinal_ex(ctx.get(),out.data()+total,&n);'),
]
with tempfile.TemporaryDirectory(prefix='local-security-negative-') as directory:
    temp = pathlib.Path(directory)
    library = args.openssl_library
    runtime_environment = os.environ.copy()
    if args.msvc:
        if not args.openssl_include:
            parser.error('--msvc requires --openssl-include')
        if args.openssl_dll:
            dll = args.openssl_dll.resolve(strict=True)
            required = ['OPENSSL_cleanse', 'PKCS5_PBKDF2_HMAC', 'CRYPTO_memcmp',
                        'RAND_bytes', 'HMAC', 'EVP_sha1', 'EVP_sha256', 'EVP_sha512',
                        'EVP_CIPHER_CTX_new', 'EVP_CIPHER_CTX_free', 'EVP_aes_256_gcm',
                        'EVP_EncryptInit_ex', 'EVP_EncryptUpdate', 'EVP_EncryptFinal_ex',
                        'EVP_DecryptInit_ex', 'EVP_DecryptUpdate', 'EVP_DecryptFinal_ex',
                        'EVP_CIPHER_CTX_ctrl']
            exports = subprocess.run(['dumpbin', '/exports', str(dll)], check=True,
                                     capture_output=True, text=True, timeout=30).stdout
            names = {line.split()[3] for line in exports.splitlines()
                     if len(line.split()) >= 4 and line.split()[0].isdigit()}
            if not set(required).issubset(names):
                raise RuntimeError('OpenSSL DLL is missing required C ABI exports')
            definition = temp / 'crypto.def'
            definition.write_text('LIBRARY ' + dll.name + '\nEXPORTS\n' + '\n'.join(required) + '\n')
            library = temp / 'crypto.lib'
            subprocess.run(['lib', '/nologo', '/machine:x64', '/def:' + str(definition),
                            '/out:' + str(library)], check=True, capture_output=True, timeout=30)
            runtime_environment['PATH'] = str(dll.parent) + os.pathsep + runtime_environment.get('PATH', '')
        if not library:
            parser.error('--msvc requires --openssl-library or --openssl-dll')
    cases = [] if args.qr_only or args.history_only else [('baseline', '', '')] if args.core_only else [('baseline', '', '')] + mutations + [('restored', '', '')]
    for name, old, new in cases:
        if old and base.count(old) != 1:
            raise RuntimeError('Mutation boundary is not unique: ' + name)
        candidate = temp / 'LocalSecurity.cpp'
        candidate.write_text(base.replace(old, new, 1) if old else base, encoding='utf-8')
        binary = temp / (name + ('.exe' if sys.platform == 'win32' else ''))
        sources = [str(candidate), str(source / 'Authenticator.cpp'), str(source / 'ElementLock.cpp'),
                   str(source / 'SupportTickets.cpp'), str(root / 'tests/local_security/local_security_tests.cpp')]
        if args.msvc:
            command = [args.compiler, '/nologo', '/std:c++17', '/EHsc', '/utf-8', '/MD',
                       '/I' + str(root / 'src'), '/I' + str(source), '/I' + str(args.openssl_include),
                       '/Fo' + str(temp) + os.sep, '/Fe' + str(binary)] + sources + [str(library), 'advapi32.lib']
        else:
            command = [args.compiler, '-std=c++17', '-I', str(root / 'src'), '-I', str(source)] + sources + ['-lcrypto']
            if sys.platform == 'win32':
                command += ['-ladvapi32']
            command += ['-o', str(binary)]
        compiled = subprocess.run(command, timeout=120, capture_output=True, text=True)
        if compiled.returncode:
            raise RuntimeError('Mutation did not compile: ' + name + '\n' + compiled.stdout + compiled.stderr)
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60, env=runtime_environment)
        expected_success = name in ('baseline', 'restored')
        if (result.returncode == 0) != expected_success:
            raise RuntimeError('Behavioral verdict did not detect mutation: ' + name)
        print(('GREEN ' if expected_success else 'RED ') + name, flush=True)
        if args.core_only:
            print(result.stdout.strip(), flush=True)
    if args.history_only:
        if not args.msvc or not args.libgit2_prefix:
            parser.error('--history-only requires --msvc and --libgit2-prefix')
        binary = temp / 'identity-history-tests.exe'
        command = [args.compiler, '/nologo', '/std:c++17', '/EHsc', '/utf-8', '/MD',
                   '/I' + str(root / 'src'), '/I' + str(args.openssl_include),
                   '/I' + str(args.libgit2_prefix / 'include'), '/Fo' + str(temp) + os.sep,
                   '/Fe' + str(binary), str(source / 'LocalSecurity.cpp'), str(source / 'IdentityHistory.cpp'),
                   str(root / 'tests/local_security/identity_history_tests.cpp'), str(library),
                   str(args.libgit2_prefix / 'lib/libgit2package.lib'), 'advapi32.lib', 'ws2_32.lib', 'secur32.lib']
        built = subprocess.run(command, capture_output=True, text=True, timeout=120)
        if built.returncode:
            raise RuntimeError('History driver did not compile:\n' + built.stdout + built.stderr)
        tested = subprocess.run([str(binary)], capture_output=True, text=True, timeout=120, env=runtime_environment)
        print(tested.stdout.strip(), flush=True)
        if tested.returncode:
            raise RuntimeError('History driver failed:\n' + tested.stderr)
    elif args.qr_only:
        if not args.msvc or not args.qr_python_packages:
            parser.error('--qr-only requires --msvc and --qr-python-packages')
        binary = temp / 'pairing-qr-fixture.exe'
        command = [args.compiler, '/nologo', '/std:c++17', '/EHsc', '/utf-8', '/MD',
                   '/I' + str(root / 'src'), '/I' + str(args.openssl_include),
                   '/Fo' + str(temp) + os.sep, '/Fe' + str(binary), str(source / 'LocalSecurity.cpp'),
                   str(root / 'src/slic3r/GUI/LocalSecurity/PairingQr.cpp'),
                   str(root / 'src/slic3r/GUI/third_party/qrcodegen.cpp'),
                   str(root / 'tests/local_security/qr_pairing_fixture.cpp'), str(library), 'advapi32.lib']
        result = subprocess.run(command, capture_output=True, text=True, timeout=120)
        if result.returncode:
            raise RuntimeError('QR fixture did not compile:\n' + result.stdout + result.stderr)
        subprocess.run([sys.executable, str(root / 'tests/local_security/verify_pairing_qr.py'), str(binary),
                        '--python-packages', str(args.qr_python_packages)], check=True, timeout=60, env=runtime_environment)
    elif not args.core_only:
        print('PASS 12 isolated negative mutations; baseline and restored source passed', flush=True)
