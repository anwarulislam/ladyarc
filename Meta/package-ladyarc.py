#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause

import argparse
import os
import subprocess
import tarfile
import tempfile
from pathlib import Path


def sign_adhoc(bundle, entitlements):
    # Installation adjusts library paths. Sign the installed code from the inside out.
    magic = {bytes.fromhex(value) for value in ("feedface", "cefaedfe", "feedfacf", "cffaedfe", "cafebabe", "bebafeca", "cafebabf", "bfbafeca")}
    paths = sorted(bundle.rglob("*"), key=lambda path: len(path.parts), reverse=True)
    for path in paths:
        if path.is_symlink():
            continue
        if path.is_dir():
            if path.suffix not in {".app", ".framework", ".xpc", ".appex"}:
                continue
        elif path.is_file():
            with path.open("rb") as file:
                if file.read(4) not in magic:
                    continue
        else:
            continue
        subprocess.run(["codesign", "--force", "--sign", "-", str(path)], check=True)
    subprocess.run(["codesign", "--force", "--sign", "-", "--entitlements", str(entitlements), str(bundle)], check=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(bundle)], check=True)


def package(runtime, output, platform, arch):
    if platform == "macOS":
        bundle = runtime / "Ladyarc.app"
        (runtime / "bundle" / "Ladybird.app").rename(bundle)
        (runtime / "bundle").rmdir()
        sign_adhoc(bundle, runtime / "entitlements" / "Ladybird.release.entitlements")
        executable = Path("Ladyarc.app/Contents/MacOS/Ladybird")
    else:
        executable = Path("bin/ladyarc")
        (runtime / executable).symlink_to("Ladybird")

    if not os.access(runtime / executable, os.X_OK):
        raise RuntimeError(f"Missing browser executable: {executable}")
    if not any(path.is_file() and os.access(path, os.X_OK) for path in runtime.rglob("WebContent")):
        raise RuntimeError("Missing WebContent helper")

    output.mkdir(parents=True, exist_ok=True)
    archive = output / f"ladyarc-{platform}-{arch}.tar.xz"
    with tarfile.open(archive, "w:xz", preset=3) as tar:
        for entry in sorted(runtime.iterdir()):
            tar.add(entry, arcname=entry.name)

    with tempfile.TemporaryDirectory(prefix="ladyarc-smoke-") as directory:
        unpacked = Path(directory)
        # This is the archive created above from our own installation. Preserve framework symlinks.
        with tarfile.open(archive, "r:xz") as tar:
            tar.extractall(unpacked, filter="fully_trusted")
        fixture = unpacked / "smoke.html"
        fixture.write_text("<!doctype html><title>Smoke test</title><p>Ladyarc package rendered this page.</p>", encoding="utf-8")
        env = os.environ.copy()
        env.pop("LADYBIRD_SOURCE_DIR", None)
        env.pop("LADYBIRD_MAIN_SOURCE_DIR", None)
        result = subprocess.run(
            [str(unpacked / executable), "--headless=text", "--temporary-profile", fixture.as_uri()],
            cwd=unpacked, env=env, capture_output=True, text=True, timeout=120,
        )
        if result.returncode or "Ladyarc package rendered this page." not in result.stdout:
            raise RuntimeError(f"Packaged browser failed its rendering check ({result.returncode})\n{result.stdout}\n{result.stderr}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Package and launch an installed Ladyarc runtime")
    parser.add_argument("runtime", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("platform", choices=["Linux", "macOS"])
    parser.add_argument("arch")
    args = parser.parse_args()
    package(args.runtime.resolve(), args.output.resolve(), args.platform, args.arch)
