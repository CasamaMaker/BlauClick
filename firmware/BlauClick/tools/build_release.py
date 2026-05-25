#!/usr/bin/env python3
"""
BlauLink release builder.

Compila tots els entorns i el filesystem, i fusiona els binaris en un sol
.bin per entorn que es pot gravar amb:
    esptool.py write_flash 0x0 BlauLink_<version>_<env>.bin

Ús:
    python tools/build_release.py
    python tools/build_release.py --envs esp32c3,esp32
    python tools/build_release.py --skip-build   # només fa el merge
"""

import argparse
import configparser
import pathlib
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).parent.parent


def _resolve_build_dir() -> pathlib.Path:
    """Llegeix build_dir de platformio.ini; si no n'hi ha, usa el valor per defecte."""
    cfg = configparser.RawConfigParser()
    cfg.read(ROOT / "platformio.ini", encoding="utf-8")
    raw = cfg.get("platformio", "build_dir", fallback=None)
    if raw:
        return pathlib.Path(raw.strip())
    return ROOT / ".pio" / "build"


BUILD_DIR = _resolve_build_dir()

ENVIRONMENTS = ["esp32c3", "esp32", "esp32s3", "esp32s2", "esp32c6"]

CHIP_MAP = {
    "esp32c3": "esp32c3",
    "esp32":   "esp32",
    "esp32s3": "esp32s3",
    "esp32s2": "esp32s2",
    "esp32c6": "esp32c6",
}

# Offset del bootloader a la flash segons el chip.
# ESP32 clàssic: 0x1000. Tots els altres (C3/S2/S3/C6): 0x0000.
BOOTLOADER_OFFSET = {
    "esp32":   0x1000,
    "esp32c3": 0x0000,
    "esp32s2": 0x0000,
    "esp32s3": 0x0000,
    "esp32c6": 0x0000,
}

# Noms i subtipus de particions de filesystem reconeguts
FS_NAMES    = {b"spiffs", b"littlefs", b"ffat", b"fs"}
FS_SUBTYPES = {0x82, 0x83}


def _find_pio_home() -> pathlib.Path | None:
    """Retorna el directori arrel de PlatformIO, o None si no es troba."""
    candidates = [
        pathlib.Path.home() / ".platformio",  # ubicació estàndard
        pathlib.Path("C:/") / ".platformio",  # Windows amb instal·lació a C:\
    ]
    for p in candidates:
        if (p / "penv").exists():
            return p
    return None


def _find_in_path(name: str) -> list[str] | None:
    """Comprova si una comanda és al PATH."""
    try:
        subprocess.run([name, "--version"], check=True, capture_output=True)
        return [name]
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def find_pio() -> list[str]:
    """Retorna la crida a pio. Cerca al PATH i a PlatformIO."""
    if cmd := _find_in_path("pio"):
        return cmd
    pio_home = _find_pio_home()
    if pio_home:
        for rel in ["penv/Scripts/pio.exe", "penv/Scripts/pio", "penv/bin/pio"]:
            candidate = pio_home / rel
            if candidate.exists():
                return [str(candidate)]
    sys.exit(
        "ERROR: pio no trobat al PATH.\n"
        "Afegeix PlatformIO al PATH o instal·la el PlatformIO CLI:\n"
        "  https://docs.platformio.org/en/latest/core/installation/index.html"
    )


def find_esptool() -> list[str]:
    """Retorna la crida a esptool. Cerca al Python actual, PATH i PlatformIO."""
    # 1. python -m esptool (esptool instal·lat al Python actual)
    try:
        subprocess.run([sys.executable, "-m", "esptool", "version"], check=True, capture_output=True)
        return [sys.executable, "-m", "esptool"]
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    # 2. esptool directament al PATH
    if cmd := _find_in_path("esptool"):
        return cmd

    # 3. PlatformIO penv i paquet tool-esptoolpy
    pio_home = _find_pio_home()
    if pio_home:
        for rel in ["penv/Scripts/esptool.exe", "penv/Scripts/esptool", "penv/bin/esptool"]:
            candidate = pio_home / rel
            if candidate.exists():
                return [str(candidate)]
        script = pio_home / "packages" / "tool-esptoolpy" / "esptool.py"
        if script.exists():
            return [sys.executable, str(script)]

    sys.exit(
        "ERROR: esptool no trobat.\n"
        "Opcions:\n"
        "  pip install esptool\n"
        "  o verifica que PlatformIO estigui instal·lat"
    )


def run(cmd: list, **kwargs):
    print(f"  $ {' '.join(str(c) for c in cmd)}")
    subprocess.run(cmd, check=True, **kwargs)


def get_version() -> str:
    """Llegeix FIRMWARE_VERSION de src/config.h."""
    import re
    config_h = ROOT / "src" / "config.h"
    if config_h.exists():
        m = re.search(r'#define\s+FIRMWARE_VERSION\s+"([^"]+)"', config_h.read_text(encoding="utf-8"))
        if m:
            return f"v{m.group(1)}"
    # Fallback: tag de git (sense el sufix de commits)
    result = subprocess.run(
        ["git", "describe", "--tags", "--abbrev=0"],
        capture_output=True, text=True, cwd=ROOT,
    )
    return result.stdout.strip() if result.returncode == 0 else "dev"


def find_boot_app0() -> pathlib.Path | None:
    """Cerca boot_app0.bin al paquet del framework de PlatformIO."""
    pio_home = _find_pio_home()
    if not pio_home:
        return None
    candidates = [
        pio_home / "packages" / "framework-arduinoespressif32" / "tools" / "partitions" / "boot_app0.bin",
        pio_home / "packages" / "framework-arduino-espressif32" / "tools" / "partitions" / "boot_app0.bin",
    ]
    return next((c for c in candidates if c.exists()), None)


def find_fs_partition(partitions_bin: pathlib.Path) -> tuple[int, int]:
    """Parseja partitions.bin i retorna (offset, size) de la partició filesystem."""
    data = partitions_bin.read_bytes()
    for i in range(0, len(data), 32):
        entry = data[i:i + 32]
        if len(entry) < 32 or entry[0:2] != b"\xaa\x50":
            continue
        p_type, p_sub = entry[2], entry[3]
        offset, size = struct.unpack_from("<II", entry, 4)
        name = entry[12:28].rstrip(b"\x00")
        if p_type == 1 and (name in FS_NAMES or p_sub in FS_SUBTYPES):
            return offset, size
    raise RuntimeError(f"No s'ha trobat cap partició de filesystem a {partitions_bin}")


def merge(env: str, output_path: pathlib.Path, esptool_cmd: list):
    env_build = BUILD_DIR / env
    chip = CHIP_MAP[env]

    bootloader_bin  = env_build / "bootloader.bin"
    partitions_bin  = env_build / "partitions.bin"
    firmware_bin    = env_build / "firmware.bin"
    littlefs_bin    = env_build / "littlefs.bin"

    for f in (bootloader_bin, partitions_bin, firmware_bin):
        if not f.exists():
            sys.exit(f"ERROR: {f} no existeix.\nCompila primer amb:  pio run -e {env}")
    if not littlefs_bin.exists():
        sys.exit(f"ERROR: {littlefs_bin} no existeix.\nConstrueix el filesystem amb:  pio run -e {env} -t buildfs")

    boot_offset = BOOTLOADER_OFFSET.get(env, 0x0000)
    fs_offset, _ = find_fs_partition(partitions_bin)
    boot_app0 = find_boot_app0()

    cmd = [
        *esptool_cmd,
        "--chip", chip,
        "merge_bin",
        "-o", str(output_path),
        "--flash-mode", "qio",
        "--flash-size", "4MB",
        hex(boot_offset), str(bootloader_bin),
        "0x8000",         str(partitions_bin),
    ]
    if boot_app0:
        cmd += ["0xe000", str(boot_app0)]
    cmd += ["0x10000", str(firmware_bin)]
    cmd += [hex(fs_offset), str(littlefs_bin)]

    run(cmd)


def main():
    parser = argparse.ArgumentParser(
        description="BlauLink release builder",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--envs",
        default=",".join(ENVIRONMENTS),
        help=f"Entorns separats per comes (per defecte: {','.join(ENVIRONMENTS)})",
    )
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Salta la compilació i fa només el merge dels binaris existents",
    )
    args = parser.parse_args()

    envs = [e.strip() for e in args.envs.split(",") if e.strip()]
    unknown = [e for e in envs if e not in CHIP_MAP]
    if unknown:
        sys.exit(f"ERROR: Entorns desconeguts: {unknown}\nVàlids: {list(CHIP_MAP)}")

    pio_cmd    = find_pio()
    esptool_cmd = find_esptool()
    version = get_version()
    release_dir = ROOT / "release" / version
    release_dir.mkdir(parents=True, exist_ok=True)

    print(f"\n=== BlauLink Release Builder — {version} ===\n")
    print(f"Entorns: {envs}")
    print(f"Sortida: {release_dir}\n")

    for env in envs:
        print(f"\n{'─'*40}")
        print(f"  {env}")
        print(f"{'─'*40}")

        if not args.skip_build:
            print("  [1/2] Compilant firmware...")
            run([*pio_cmd, "run", "-e", env], cwd=ROOT)
            print("  [2/2] Construint filesystem (LittleFS)...")
            run([*pio_cmd, "run", "-e", env, "-t", "buildfs"], cwd=ROOT)

        output = release_dir / f"BlauLink_{version}_{env}.bin"
        print(f"  Fusionant → {output.name}")
        merge(env, output, esptool_cmd)
        size_kb = output.stat().st_size / 1024
        print(f"  ✓  {size_kb:.0f} KB")

    print(f"\n{'='*40}")
    print(f"  Release llest a: {release_dir}")
    print(f"{'='*40}")
    for f in sorted(release_dir.glob("*.bin")):
        print(f"  {f.name}  ({f.stat().st_size / 1024:.0f} KB)")
    print(f"\nGravar al micro:")
    print(f"  esptool.py write_flash 0x0 BlauLink_{version}_<env>.bin\n")


if __name__ == "__main__":
    main()
