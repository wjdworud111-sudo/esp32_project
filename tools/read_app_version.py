import pathlib
import re


project = pathlib.Path(__file__).resolve().parents[1]
config = (project / "include" / "app_config.h").read_text(encoding="utf-8")
match = re.search(r'#define\s+APP_VERSION\s+"(\d+\.\d+\.\d+)"', config)
if not match:
    raise SystemExit("APP_VERSION is missing or is not MAJOR.MINOR.PATCH")
print(match.group(1))
