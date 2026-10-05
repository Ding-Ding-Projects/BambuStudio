"""Build isolated, deliberately broken copies and require behavioral rejection.

This never changes the checked-out source. Requires a C++17 compiler with the
existing OpenSSL development package. Windows also requires advapi32.
"""
import argparse
import pathlib
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default='g++')
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[2]
source = root / 'src/libslic3r/LocalSecurity'
base = (source / 'LocalSecurity.cpp').read_text(encoding='utf-8')
auth = (source / 'Authenticator.cpp').read_text(encoding='utf-8')
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
    for name, old, new in [('baseline', '', '')] + mutations + [('restored', '', '')]:
        if old and base.count(old) != 1:
            raise RuntimeError('Mutation boundary is not unique: ' + name)
        candidate = temp / 'LocalSecurity.cpp'
        candidate.write_text(base.replace(old, new, 1) if old else base, encoding='utf-8')
        binary = temp / (name + ('.exe' if sys.platform == 'win32' else ''))
        command = [args.compiler, '-std=c++17', '-I', str(root / 'src'), '-I', str(source),
                   str(candidate), str(source / 'Authenticator.cpp'), str(source / 'ElementLock.cpp'), str(source / 'SupportTickets.cpp'),
                   str(root / 'tests/local_security/local_security_tests.cpp'), '-lcrypto']
        if sys.platform == 'win32':
            command += ['-ladvapi32']
        compiled = subprocess.run(command + ['-o', str(binary)], timeout=120, capture_output=True, text=True)
        if compiled.returncode:
            raise RuntimeError('Mutation did not compile: ' + name + '\n' + compiled.stderr)
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
        expected_success = name in ('baseline', 'restored')
        if (result.returncode == 0) != expected_success:
            raise RuntimeError('Behavioral verdict did not detect mutation: ' + name)
        print(('GREEN ' if expected_success else 'RED ') + name, flush=True)
print('PASS 12 isolated negative mutations; baseline and restored source passed', flush=True)
