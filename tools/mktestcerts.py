#!/usr/bin/env python3
"""mktestcerts.py <outdir>: throwaway CA + server certificate (DNS name qar.test) for the local HTTPS test."""
import sys, os, datetime
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
out = sys.argv[1]; os.makedirs(out, exist_ok=True)
now = datetime.datetime.now(datetime.timezone.utc)
def name(cn): return x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, cn)])
ca_key = rsa.generate_private_key(65537, 2048); srv_key = rsa.generate_private_key(65537, 2048)
ca = (x509.CertificateBuilder().subject_name(name("QAR Test CA")).issuer_name(name("QAR Test CA")).public_key(ca_key.public_key())
      .serial_number(x509.random_serial_number()).not_valid_before(now - datetime.timedelta(days=1)).not_valid_after(now + datetime.timedelta(days=365))
      .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True).sign(ca_key, hashes.SHA256()))
srv = (x509.CertificateBuilder().subject_name(name("qar.test")).issuer_name(ca.subject).public_key(srv_key.public_key())
       .serial_number(x509.random_serial_number()).not_valid_before(now - datetime.timedelta(days=1)).not_valid_after(now + datetime.timedelta(days=365))
       .add_extension(x509.SubjectAlternativeName([x509.DNSName("qar.test")]), critical=False)
       .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True).sign(ca_key, hashes.SHA256()))
pem = serialization.Encoding.PEM
open(f"{out}/ca.pem", "wb").write(ca.public_bytes(pem))
open(f"{out}/server.pem", "wb").write(srv.public_bytes(pem) + ca.public_bytes(pem))
open(f"{out}/server.key", "wb").write(srv_key.private_bytes(pem, serialization.PrivateFormat.TraditionalOpenSSL, serialization.NoEncryption()))
print("test certificates written to", out)
