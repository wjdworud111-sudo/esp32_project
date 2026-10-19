#!/usr/bin/env python3
"""Build the ESP-IDF compact certificate bundle from certifi's Mozilla roots."""

from __future__ import annotations

import re
import struct
from pathlib import Path

import certifi
from cryptography import x509
from cryptography.hazmat.primitives import serialization


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "data" / "cert" / "x509_crt_bundle.bin"
MAX_CERTS = 200


def main() -> None:
    pem_data = Path(certifi.where()).read_bytes()
    certs = [
        x509.load_pem_x509_certificate(block)
        for block in re.findall(
            b"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----",
            pem_data,
            re.DOTALL,
        )
    ]
    if not certs or len(certs) > MAX_CERTS:
        raise SystemExit(f"Expected 1-{MAX_CERTS} trusted roots, found {len(certs)}")

    certs.sort(key=lambda cert: cert.subject.public_bytes())
    # Arduino-ESP32 2.0.x uses the ESP-IDF 4.x bundle format: a
    # big-endian certificate count followed by sequential records.
    data = bytearray(struct.pack(">H", len(certs)))
    for cert in certs:
        subject = cert.subject.public_bytes()
        public_key = cert.public_key().public_bytes(
            serialization.Encoding.DER,
            serialization.PublicFormat.SubjectPublicKeyInfo,
        )
        if len(subject) > 0xFFFF or len(public_key) > 0xFFFF:
            raise SystemExit("Certificate field is too large for the ESP-IDF bundle format")
        data.extend(struct.pack(">HH", len(subject), len(public_key)))
        data.extend(subject)
        data.extend(public_key)

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_bytes(data)
    print(f"Wrote {OUTPUT.relative_to(ROOT)} ({len(certs)} roots, {OUTPUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
