"""Decode the public synthetic pairing fixture using independent ZXing code."""
import argparse
import io
import pathlib
import subprocess
import sys
import urllib.parse
import base64

parser = argparse.ArgumentParser()
parser.add_argument('fixture', type=pathlib.Path)
parser.add_argument('--python-packages', type=pathlib.Path)
args = parser.parse_args()
if args.python_packages:
    sys.path.insert(0, str(args.python_packages))
from PIL import Image
import zxingcpp

generated = subprocess.run([str(args.fixture.resolve())], check=True, capture_output=True, timeout=30).stdout
assert len(generated) <= 1024 * 1024, 'Fixture output exceeded bounds'
image = Image.open(io.BytesIO(generated))
decoded = zxingcpp.read_barcode(image, formats=zxingcpp.BarcodeFormat.QRCode)
assert decoded is not None, 'Generated QR did not decode'
uri = urllib.parse.urlparse(decoded.text)
assert uri.scheme == 'otpauth' and uri.netloc == 'totp'
assert urllib.parse.unquote(uri.path) == '/Public RFC fixture:alice@example.test'
params = urllib.parse.parse_qs(uri.query, strict_parsing=True)
assert params == {
    'secret': [base64.b32encode(b'12345678901234567890123456789012').decode().rstrip('=')],
    'issuer': ['Public RFC fixture'], 'algorithm': ['SHA256'], 'digits': ['8'], 'period': ['45']
}, 'Decoded parameters did not match the enrollment'
pixels = image.load()
assert all(pixels[x, y] == 255 for y in range(16) for x in range(image.width)), 'Quiet zone lost'
print('PASS independent ZXing decode, exact enrollment parameters and four-module quiet zone')
