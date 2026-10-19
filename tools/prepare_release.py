import argparse
import hashlib
import json
import pathlib
import re
import shutil


PROJECT = pathlib.Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser(description="Prepare GitHub OTA release assets")
    parser.add_argument("repository", help="GitHub repository as OWNER/REPOSITORY")
    parser.add_argument("version", help="Version in app_config.h, for example 1.2.0")
    args = parser.parse_args()

    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", args.repository):
        raise SystemExit("repository must use OWNER/REPOSITORY format")
    if not re.fullmatch(r"\d+\.\d+\.\d+", args.version):
        raise SystemExit("version must use MAJOR.MINOR.PATCH format")

    config = (PROJECT / "include" / "app_config.h").read_text(encoding="utf-8")
    match = re.search(r'#define\s+APP_VERSION\s+"([^"]+)"', config)
    if not match or match.group(1) != args.version:
        actual = match.group(1) if match else "missing"
        raise SystemExit(
            f"APP_VERSION is {actual}; set it to {args.version} and rebuild first"
        )

    firmware = PROJECT / ".pio" / "build" / "jc2432w328c" / "firmware.bin"
    if not firmware.is_file():
        raise SystemExit("firmware.bin is missing; run build.cmd first")

    output = PROJECT / "dist"
    output.mkdir(exist_ok=True)
    copied_firmware = output / "firmware.bin"
    shutil.copy2(firmware, copied_firmware)
    digest = hashlib.sha256(copied_firmware.read_bytes()).hexdigest()

    manifest = {
        "version": args.version,
        "url": (
            f"https://github.com/{args.repository}/releases/latest/"
            "download/firmware.bin"
        ),
        "sha256": digest,
    }
    (output / "version.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Prepared {copied_firmware}")
    print(f"Prepared {output / 'version.json'}")
    print(f"SHA-256: {digest}")


if __name__ == "__main__":
    main()

