#!/usr/bin/env python3
"""
BlauLux / BlauClick — Merge & Flash Tool (unificat)

Eina única per a BlauLux i BlauClick.
Selecciona el projecte amb els radiobuttons a dalt a l'esquerra.

Ús:
    python tools/flash_tool_merge.py
"""

import configparser
import os
import re
import pathlib
import queue
import struct
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

_THIS_ROOT  = pathlib.Path(__file__).resolve().parent.parent
_GITHUB_DIR = _THIS_ROOT.parent

PROJECTS = {
    "BlauLux":   _GITHUB_DIR / "BlauLux",
    "BlauClick": _GITHUB_DIR / "BlauClick",
}
_DEFAULT_PROJECT = _THIS_ROOT.name if _THIS_ROOT.name in PROJECTS else next(iter(PROJECTS))

BOOTLOADER_OFFSET = {
    "esp32":   "0x1000",
    "esp32c3": "0x0000",
    "esp32s2": "0x0000",
    "esp32s3": "0x0000",
    "esp32c6": "0x0000",
}
PARTITIONS_OFFSET = "0x8000"
BOOT_APP0_OFFSET  = "0xE000"
FIRMWARE_OFFSET   = "0x10000"

CHIPS = ["esp32c3", "esp32", "esp32s3", "esp32s2", "esp32c6"]

_FS_NAMES    = {b"spiffs", b"littlefs", b"ffat", b"fs"}
_FS_SUBTYPES = {0x82, 0x83}


def _find_pio_home() -> pathlib.Path | None:
    for p in [pathlib.Path.home() / ".platformio", pathlib.Path("C:/") / ".platformio"]:
        if (p / "penv").exists():
            return p
    return None


def _find_boot_app0() -> pathlib.Path | None:
    pio = _find_pio_home()
    if pio:
        pkgs = sorted((pio / "packages").glob("framework-arduinoespressif32*"), reverse=True)
        for pkg in pkgs:
            c = pkg / "tools" / "partitions" / "boot_app0.bin"
            if c.exists():
                return c
    return None


def _find_fs_offset(partitions_bin: pathlib.Path) -> str:
    """Parseja partitions.bin i retorna l'offset (hex) de la partició filesystem."""
    if not partitions_bin.is_file():
        return ""
    data = partitions_bin.read_bytes()
    for i in range(0, len(data), 32):
        entry = data[i:i + 32]
        if len(entry) < 32 or entry[0:2] != b"\xaa\x50":
            continue
        p_type, p_sub = entry[2], entry[3]
        offset, _ = struct.unpack_from("<II", entry, 4)
        name = entry[12:28].rstrip(b"\x00")
        if p_type == 1 and (name in _FS_NAMES or p_sub in _FS_SUBTYPES):
            return hex(offset)
    return ""


def find_esptool() -> list[str] | None:
    try:
        subprocess.run([sys.executable, "-m", "esptool", "version"],
                       check=True, capture_output=True)
        return [sys.executable, "-m", "esptool"]
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    try:
        subprocess.run(["esptool", "--version"], check=True, capture_output=True)
        return ["esptool"]
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    pio = _find_pio_home()
    if pio:
        for rel in ["penv/Scripts/esptool.exe", "penv/Scripts/esptool", "penv/bin/esptool"]:
            c = pio / rel
            if c.exists():
                return [str(c)]
        s = pio / "packages" / "tool-esptoolpy" / "esptool.py"
        if s.exists():
            return [sys.executable, str(s)]
    return None


def list_serial_ports() -> list[str]:
    try:
        from serial.tools import list_ports
        return sorted(p.device for p in list_ports.comports())
    except ImportError:
        pass
    try:
        import winreg
        key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\DEVICEMAP\SERIALCOMM")
        ports, i = [], 0
        while True:
            try:
                _, val, _ = winreg.EnumValue(key, i)
                ports.append(val)
                i += 1
            except OSError:
                break
        return sorted(ports)
    except Exception:
        pass
    return []


class FileRow:
    """Fila: etiqueta + entrada adreça (editable) + entrada fitxer (editable) + browse."""

    def __init__(self, parent: tk.Widget, label: str, row_idx: int,
                 default_dir: pathlib.Path | None = None):
        self._default_dir = default_dir or pathlib.Path.cwd()
        self.addr_var = tk.StringVar()
        self.path_var = tk.StringVar()

        tk.Label(parent, text=label, width=14, anchor="w").grid(
            row=row_idx, column=0, padx=(6, 4), pady=3, sticky="w")

        tk.Entry(parent, textvariable=self.addr_var, width=10,
                 font=("Consolas", 9)).grid(
            row=row_idx, column=1, padx=(0, 6), pady=3, sticky="w")

        tk.Entry(parent, textvariable=self.path_var,
                 font=("Consolas", 9)).grid(
            row=row_idx, column=2, padx=(0, 4), pady=3, sticky="ew")

        tk.Button(parent, text="…", width=2,
                  command=self._browse).grid(
            row=row_idx, column=3, padx=(0, 6), pady=3)

    def _browse(self):
        current = self.path_var.get()
        initial = str(pathlib.Path(current).parent) if current else str(self._default_dir)
        path = filedialog.askopenfilename(
            title="Selecciona el fitxer binari",
            initialdir=initial,
            filetypes=[("Fitxers binaris", "*.bin"), ("Tots els fitxers", "*.*")],
        )
        if path:
            self.path_var.set(path)

    def get(self) -> tuple[str, str] | None:
        """Retorna (adreça, path) si tots dos camps estan plens, o None si la fila és buida."""
        addr = self.addr_var.get().strip()
        path = self.path_var.get().strip()
        if addr and path:
            return addr, path
        return None

    def set(self, addr: str, path: str):
        self.addr_var.set(addr)
        self.path_var.set(path)


class MergeFlashTool(tk.Tk):
    def __init__(self):
        super().__init__()
        self.resizable(True, False)
        self._log_queue: queue.Queue[str] = queue.Queue()
        self._project_var = tk.StringVar(value=_DEFAULT_PROJECT)
        self._proj_root = PROJECTS[_DEFAULT_PROJECT]
        self._build_dir = self._resolve_build_dir()
        self.title(f"{_DEFAULT_PROJECT} — Merge & Flash Tool")
        self._build_ui()
        self._refresh_ports()
        self._on_chip_changed()
        self._poll_log()

    # ── Helpers dinàmics per projecte ────────────────────────────────

    def _resolve_build_dir(self) -> pathlib.Path:
        cfg = configparser.RawConfigParser()
        cfg.read(self._proj_root / "platformio.ini", encoding="utf-8")
        raw = cfg.get("platformio", "build_dir", fallback=None)
        if raw:
            return pathlib.Path(raw.strip())
        return self._proj_root / ".pio" / "build"

    def _get_version(self) -> str:
        config_h = self._proj_root / "src" / "config.h"
        if config_h.exists():
            m = re.search(r'#define\s+FIRMWARE_VERSION\s+"([^"]+)"',
                          config_h.read_text(encoding="utf-8"))
            if m:
                return f"v{m.group(1)}"
        return "v0.0"

    # ── UI ────────────────────────────────────────────────────────────

    def _build_ui(self):
        outer = tk.Frame(self, padx=12, pady=10)
        outer.pack(fill="both", expand=True)

        # ── Projecte + Chip + Port ────────────────────────────────────
        top = tk.Frame(outer)
        top.pack(fill="x", pady=(0, 6))

        tk.Label(top, text="Projecte:", anchor="w").pack(side="left", padx=(0, 4))
        for name in PROJECTS:
            ttk.Radiobutton(top, text=name, variable=self._project_var, value=name,
                            command=self._on_project_changed).pack(side="left", padx=(0, 6))

        ttk.Separator(top, orient="vertical").pack(side="left", fill="y", padx=(6, 12))

        tk.Label(top, text="Chip:", anchor="w").pack(side="left", padx=(0, 4))
        self._chip_var = tk.StringVar(value=CHIPS[0])
        chip_cb = ttk.Combobox(top, textvariable=self._chip_var,
                               values=[*CHIPS, "Tots"], state="readonly", width=12)
        chip_cb.pack(side="left", padx=(0, 20))
        chip_cb.bind("<<ComboboxSelected>>", lambda _: self._on_chip_changed())

        tk.Label(top, text="Port:", anchor="w").pack(side="left", padx=(0, 4))
        self._port_var = tk.StringVar()
        self._port_cb = ttk.Combobox(top, textvariable=self._port_var, width=12)
        self._port_cb.pack(side="left", padx=(0, 6))
        tk.Button(top, text="Refresh", command=self._refresh_ports).pack(side="left")

        # ── Radiobuttons mode ─────────────────────────────────────────
        ttk.Separator(outer, orient="horizontal").pack(fill="x", pady=(4, 2))
        self._mode_var = tk.StringVar(value="components")
        radio_frame = tk.Frame(outer)
        radio_frame.pack(fill="x", pady=(2, 4))
        ttk.Radiobutton(radio_frame, text="Components individuals",
                        variable=self._mode_var, value="components",
                        command=self._on_mode_changed).pack(side="left", padx=(6, 20))
        ttk.Radiobutton(radio_frame, text="Fitxer fusionat  (0x0)",
                        variable=self._mode_var, value="merged",
                        command=self._on_mode_changed).pack(side="left")

        # ── Contenidor fix per als dos modes (manté la posició) ──────
        self._selection_area = tk.Frame(outer)
        self._selection_area.pack(fill="x")

        # ── Vista: components ─────────────────────────────────────────
        self._components_frame = tk.Frame(self._selection_area)
        self._components_frame.pack(fill="x")
        self._components_frame.columnconfigure(2, weight=1)

        hdr = tk.Frame(self._components_frame)
        hdr.pack(fill="x")
        hdr.columnconfigure(2, weight=1)
        tk.Label(hdr, text="Component", width=14, anchor="w",
                 font=("TkDefaultFont", 8, "bold")).grid(
            row=0, column=0, padx=(6, 4), sticky="w")
        tk.Label(hdr, text="Adreça", width=10, anchor="w",
                 font=("TkDefaultFont", 8, "bold")).grid(
            row=0, column=1, padx=(0, 6), sticky="w")
        tk.Label(hdr, text="Fitxer", anchor="w",
                 font=("TkDefaultFont", 8, "bold")).grid(
            row=0, column=2, padx=(0, 4), sticky="w")

        files_inner = tk.Frame(self._components_frame)
        files_inner.pack(fill="x")
        files_inner.columnconfigure(2, weight=1)

        self._rows: list[FileRow] = []
        for i, label in enumerate([
            "bootloader.bin", "partitions.bin", "boot_app0.bin",
            "firmware.bin",   "littlefs.bin",
        ]):
            self._rows.append(FileRow(files_inner, label, i, default_dir=self._build_dir))

        # ── Vista: merged ─────────────────────────────────────────────
        self._merged_frame = tk.Frame(self._selection_area)

        hdr_m = tk.Frame(self._merged_frame)
        hdr_m.pack(fill="x")
        hdr_m.columnconfigure(2, weight=1)
        tk.Label(hdr_m, text="Component", width=14, anchor="w",
                 font=("TkDefaultFont", 8, "bold")).grid(
            row=0, column=0, padx=(6, 4), sticky="w")
        tk.Label(hdr_m, text="Adreça", width=10, anchor="w",
                 font=("TkDefaultFont", 8, "bold")).grid(
            row=0, column=1, padx=(0, 6), sticky="w")
        tk.Label(hdr_m, text="Fitxer", anchor="w",
                 font=("TkDefaultFont", 8, "bold")).grid(
            row=0, column=2, padx=(0, 4), sticky="w")

        inner_m = tk.Frame(self._merged_frame)
        inner_m.pack(fill="x")
        inner_m.columnconfigure(2, weight=1)
        self._merged_row = FileRow(inner_m, "merged.bin", 0)
        self._merged_row.set("0x0", "")

        # ── Vista: Tots els xips ──────────────────────────────────────
        self._tots_frame = tk.Frame(self._selection_area)
        tk.Label(
            self._tots_frame,
            text="Mode «Tots els xips»: MERGE i EXPORT OTA s'executaran per a cada xip "
                 f"({', '.join(CHIPS)}) de forma seqüencial.",
            fg="#666666", font=("TkDefaultFont", 9, "italic"),
            wraplength=520, justify="left",
        ).pack(padx=6, pady=12, anchor="w")

        # ── Botons FLASH / MERGE / ERASE / EXPORT OTA ─────────────────
        ttk.Separator(outer, orient="horizontal").pack(fill="x", pady=8)
        btn_frame = tk.Frame(outer)
        btn_frame.pack()

        self._flash_btn = tk.Button(
            btn_frame, text="  FLASH  ",
            bg="#c0392b", fg="white",
            activebackground="#e74c3c", activeforeground="white",
            font=("TkDefaultFont", 11, "bold"),
            relief="flat", padx=22, pady=8, cursor="hand2",
            command=self._start_flash,
        )
        self._flash_btn.pack(side="left", padx=12)

        self._merge_btn = tk.Button(
            btn_frame, text="  MERGE  ",
            bg="#1a6b8a", fg="white",
            activebackground="#2196b8", activeforeground="white",
            font=("TkDefaultFont", 11, "bold"),
            relief="flat", padx=22, pady=8, cursor="hand2",
            command=self._start_merge,
        )
        self._merge_btn.pack(side="left", padx=12)

        self._erase_btn = tk.Button(
            btn_frame, text="  ERASE  ",
            bg="#5d6d7e", fg="white",
            activebackground="#7f8c8d", activeforeground="white",
            font=("TkDefaultFont", 11, "bold"),
            relief="flat", padx=22, pady=8, cursor="hand2",
            command=self._start_erase,
        )
        self._erase_btn.pack(side="left", padx=12)

        self._ota_btn = tk.Button(
            btn_frame, text="  EXPORT OTA  ",
            bg="#27ae60", fg="white",
            activebackground="#2ecc71", activeforeground="white",
            font=("TkDefaultFont", 11, "bold"),
            relief="flat", padx=22, pady=8, cursor="hand2",
            command=self._start_export_ota,
        )
        self._ota_btn.pack(side="left", padx=12)

        # ── Consola ───────────────────────────────────────────────────
        ttk.Separator(outer, orient="horizontal").pack(fill="x", pady=(8, 4))
        tk.Label(outer, text="Consola:", anchor="w").pack(anchor="w")
        log_frame = tk.Frame(outer)
        log_frame.pack(fill="both", expand=True, pady=(2, 0))

        self._log = tk.Text(
            log_frame, width=84, height=14,
            state="disabled",
            bg="#1e1e1e", fg="#d4d4d4",
            font=("Consolas", 9),
            relief="flat", bd=0, wrap="char",
        )
        sb = ttk.Scrollbar(log_frame, orient="vertical", command=self._log.yview)
        self._log.configure(yscrollcommand=sb.set)
        self._log.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")

    # ── Handlers canvi estat ──────────────────────────────────────────

    def _on_project_changed(self):
        project = self._project_var.get()
        self._proj_root      = PROJECTS[project]
        self._build_dir = self._resolve_build_dir()
        self.title(f"{project} — Merge & Flash Tool")
        self._on_chip_changed()

    def _on_mode_changed(self):
        if self._chip_var.get() == "Tots":
            return
        if self._mode_var.get() == "components":
            self._merged_frame.pack_forget()
            self._components_frame.pack(fill="x")
        else:
            self._components_frame.pack_forget()
            self._merged_frame.pack(fill="x")
            self._auto_fill_merged()
        self._update_button_states()

    def _auto_fill_merged(self):
        chip      = self._chip_var.get()
        version   = self._get_version()
        candidate = self._proj_root / "release" / version / f"{self._proj_root.name}_{version}_{chip}.bin"
        if candidate.is_file():
            self._merged_row.set("0x0", str(candidate))

    def _on_chip_changed(self):
        chip = self._chip_var.get()

        if chip == "Tots":
            self._components_frame.pack_forget()
            self._merged_frame.pack_forget()
            self._tots_frame.pack(fill="x")
            self._update_button_states()
            return

        self._tots_frame.pack_forget()
        if self._mode_var.get() == "components":
            self._merged_frame.pack_forget()
            self._components_frame.pack(fill="x")
        else:
            self._components_frame.pack_forget()
            self._merged_frame.pack(fill="x")

        env_dir   = self._build_dir / chip
        boot_app0 = _find_boot_app0()
        fs_offset = _find_fs_offset(env_dir / "partitions.bin")

        defs = [
            (BOOTLOADER_OFFSET.get(chip, "0x0000"), str(env_dir / "bootloader.bin")),
            (PARTITIONS_OFFSET,                     str(env_dir / "partitions.bin")),
            (BOOT_APP0_OFFSET,                      str(boot_app0) if boot_app0 else ""),
            (FIRMWARE_OFFSET,                       str(env_dir / "firmware.bin")),
            (fs_offset,                             str(env_dir / "littlefs.bin")),
        ]
        for row, (addr, path) in zip(self._rows, defs):
            row.set(addr, path)
        if self._mode_var.get() == "merged":
            self._auto_fill_merged()

        self._update_button_states()

    def _update_button_states(self, busy: bool = False):
        is_all         = self._chip_var.get() == "Tots"
        is_merged_mode = self._mode_var.get() == "merged"

        self._flash_btn.configure(state="disabled" if (busy or is_all) else "normal")
        self._erase_btn.configure(state="disabled" if (busy or is_all) else "normal")
        self._ota_btn.configure(state="disabled" if busy else "normal")
        merge_disabled = busy or (not is_all and is_merged_mode)
        self._merge_btn.configure(state="disabled" if merge_disabled else "normal")

    def _refresh_ports(self):
        ports = list_serial_ports()
        self._port_cb["values"] = ports
        if ports and not self._port_var.get():
            self._port_cb.current(0)

    # ── Consola ───────────────────────────────────────────────────────

    def _log_append(self, text: str):
        self._log.configure(state="normal")
        self._log.insert("end", text)
        self._log.see("end")
        self._log.configure(state="disabled")

    def _poll_log(self):
        try:
            while True:
                self._log_append(self._log_queue.get_nowait())
        except queue.Empty:
            pass
        self.after(80, self._poll_log)

    # ── Gestió busy / comandes ────────────────────────────────────────

    def _set_busy(self, busy: bool):
        self._update_button_states(busy=busy)

    def _run_cmd_raw(self, cmd: list) -> bool:
        """Executa una comanda, registra la sortida. Retorna True si èxit."""
        env = os.environ.copy()
        env["PYTHONUTF8"] = "1"
        env["PYTHONIOENCODING"] = "utf-8"
        self._log_queue.put(f"\n{'─' * 64}\n")
        self._log_queue.put("$ " + " ".join(str(c) for c in cmd) + "\n")
        self._log_queue.put(f"{'─' * 64}\n\n")
        try:
            proc = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                encoding="utf-8",
                errors="replace",
                bufsize=1,
                env=env,
            )
            for line in proc.stdout:
                self._log_queue.put(line)
            proc.wait()
            if proc.returncode == 0:
                self._log_queue.put("\n✓  Operació completada correctament.\n")
                return True
            else:
                self._log_queue.put(f"\n✗  Error (codi de sortida {proc.returncode}).\n")
                return False
        except Exception as exc:
            self._log_queue.put(f"\n✗  Excepció: {exc}\n")
            return False

    def _run_cmd(self, cmd: list):
        try:
            self._run_cmd_raw(cmd)
        finally:
            self.after(0, lambda: self._set_busy(False))

    # ── Recollida entrades ────────────────────────────────────────────

    def _collect_entries(self) -> list[str] | None:
        """Valida i retorna la llista [addr, path, ...] de les files plenes, o None si hi ha error."""
        entries: list[str] = []
        for row in self._rows:
            result = row.get()
            if result is None:
                continue
            addr, path = result
            if not pathlib.Path(path).is_file():
                messagebox.showerror("Fitxer no trobat", f"{path}")
                return None
            entries.extend([addr, path])
        if not entries:
            messagebox.showerror("Error", "No hi ha cap fitxer seleccionat.")
            return None
        return entries

    def _collect_chip_entries(self, chip: str) -> list[str]:
        """Retorna [addr, path, ...] per un xip des del build dir (ignorant fitxers que falten)."""
        env_dir   = self._build_dir / chip
        boot_app0 = _find_boot_app0()
        fs_offset = _find_fs_offset(env_dir / "partitions.bin")

        candidates = [
            (BOOTLOADER_OFFSET.get(chip, "0x0000"), env_dir / "bootloader.bin"),
            (PARTITIONS_OFFSET,                     env_dir / "partitions.bin"),
            (BOOT_APP0_OFFSET,                      boot_app0),
            (FIRMWARE_OFFSET,                       env_dir / "firmware.bin"),
            (fs_offset,                             env_dir / "littlefs.bin" if fs_offset else None),
        ]
        entries = []
        for addr, path in candidates:
            if path and pathlib.Path(path).is_file():
                entries.extend([addr, str(path)])
        return entries

    # ── Accions ───────────────────────────────────────────────────────

    def _start_merge(self):
        esptool = find_esptool()
        if esptool is None:
            messagebox.showerror(
                "esptool no trobat",
                "No s'ha pogut localitzar esptool.\n\nInstal·la'l amb:\n  pip install esptool",
            )
            return

        version = self._get_version()
        rel_dir = self._proj_root / "release" / version
        rel_dir.mkdir(parents=True, exist_ok=True)

        if self._chip_var.get() == "Tots":
            self._set_busy(True)
            threading.Thread(
                target=self._merge_all_chips_thread,
                args=(esptool, version, rel_dir),
                daemon=True,
            ).start()
            return

        entries = self._collect_entries()
        if entries is None:
            return

        chip   = self._chip_var.get()
        output = filedialog.asksaveasfilename(
            title="Desa el binari fusionat",
            defaultextension=".bin",
            initialdir=str(rel_dir),
            initialfile=f"{self._proj_root.name}_{version}_{chip}.bin",
            filetypes=[("Fitxers binaris", "*.bin"), ("Tots els fitxers", "*.*")],
        )
        if not output:
            return

        cmd = [*esptool, "--chip", chip, "merge_bin", "--output", output, *entries]
        self._set_busy(True)
        self._log_queue.put(f"\nSortida: {output}\n")
        threading.Thread(target=self._run_cmd, args=(cmd,), daemon=True).start()

    def _merge_all_chips_thread(self, esptool: list, version: str, rel_dir: pathlib.Path):
        root_name = self._proj_root.name
        try:
            for chip in CHIPS:
                entries = self._collect_chip_entries(chip)
                if not entries:
                    self._log_queue.put(f"\n⚠  {chip}: no s'han trobat fitxers de build. Saltant.\n")
                    continue
                output = str(rel_dir / f"{root_name}_{version}_{chip}.bin")
                self._log_queue.put(f"\n▶  Processant {chip}...\n")
                self._run_cmd_raw([*esptool, "--chip", chip, "merge_bin", "--output", output, *entries])
                self._log_queue.put(f"   Sortida: {output}\n")
            self._log_queue.put(f"\n✓  MERGE completat per a tots els xips.\n")
        finally:
            self.after(0, lambda: self._set_busy(False))

    def _start_flash(self):
        esptool = find_esptool()
        if esptool is None:
            messagebox.showerror(
                "esptool no trobat",
                "No s'ha pogut localitzar esptool.\n\nInstal·la'l amb:\n  pip install esptool",
            )
            return
        port = self._port_var.get().strip()
        if not port:
            messagebox.showerror("Error", "Selecciona o escriu un port sèrie.")
            return
        if self._mode_var.get() == "merged":
            result = self._merged_row.get()
            if result is None:
                messagebox.showerror("Error", "Selecciona el fitxer .bin fusionat.")
                return
            addr, path = result
            if not pathlib.Path(path).is_file():
                messagebox.showerror("Fitxer no trobat", path)
                return
            entries = [addr, path]
        else:
            entries = self._collect_entries()
            if entries is None:
                return

        cmd = [
            *esptool,
            "--chip",   self._chip_var.get(),
            "--port",   port,
            "--baud",   "460800",
            "--before", "default-reset",
            "--after",  "hard-reset",
            "write_flash",
            "--flash_size", "detect",
            *entries,
        ]
        self._set_busy(True)
        threading.Thread(target=self._run_cmd, args=(cmd,), daemon=True).start()

    def _start_erase(self):
        esptool = find_esptool()
        if esptool is None:
            messagebox.showerror("esptool no trobat", "No s'ha pogut localitzar esptool.")
            return
        port = self._port_var.get().strip()
        if not port:
            messagebox.showerror("Error", "Selecciona o escriu un port sèrie.")
            return
        if not messagebox.askyesno(
            "Confirmar esborrament",
            f"S'esborrarà tota la flash del dispositiu a {port}.\n\nContinues?",
        ):
            return

        cmd = [
            *esptool,
            "--chip",  self._chip_var.get(),
            "--port",  port,
            "--baud",  "460800",
            "erase_flash",
        ]
        self._set_busy(True)
        threading.Thread(target=self._run_cmd, args=(cmd,), daemon=True).start()

    def _start_export_ota(self):
        if self._chip_var.get() == "Tots":
            version = self._get_version()
            rel_dir = self._proj_root / "release" / version
            rel_dir.mkdir(parents=True, exist_ok=True)
            self._set_busy(True)
            threading.Thread(
                target=self._export_ota_all_thread,
                args=(version, rel_dir),
                daemon=True,
            ).start()
            return

        fw_result  = self._rows[3].get()
        lfs_result = self._rows[4].get()

        if fw_result is None or not pathlib.Path(fw_result[1]).is_file():
            messagebox.showerror("Fitxer no trobat",
                                 "firmware.bin no existeix. Compila primer amb PlatformIO.")
            return
        if lfs_result is None or not pathlib.Path(lfs_result[1]).is_file():
            messagebox.showerror("Fitxer no trobat",
                                 "littlefs.bin no existeix. Executa 'Build Filesystem Image' primer.")
            return

        chip    = self._chip_var.get()
        version = self._get_version()
        rel_dir = self._proj_root / "release" / version
        rel_dir.mkdir(parents=True, exist_ok=True)

        output = filedialog.asksaveasfilename(
            title="Desa el binari OTA",
            defaultextension=".bin",
            initialdir=str(rel_dir),
            initialfile=f"{self._proj_root.name}_{version}_{chip}_ota.bin",
            filetypes=[("Fitxers binaris", "*.bin"), ("Tots els fitxers", "*.*")],
        )
        if not output:
            return

        fw_data  = pathlib.Path(fw_result[1]).read_bytes()
        lfs_data = pathlib.Path(lfs_result[1]).read_bytes()

        with open(output, "wb") as f:
            f.write(b"BLAU")
            f.write(struct.pack("<II", len(fw_data), len(lfs_data)))
            f.write(fw_data)
            f.write(lfs_data)

        self._log_queue.put(f"\n{'─' * 64}\n")
        self._log_queue.put(f"EXPORT OTA → {output}\n")
        self._log_queue.put(f"  firmware:  {len(fw_data):,} bytes\n")
        self._log_queue.put(f"  littlefs:  {len(lfs_data):,} bytes\n")
        self._log_queue.put(f"  total:     {12 + len(fw_data) + len(lfs_data):,} bytes\n")
        self._log_queue.put(f"{'─' * 64}\n")
        self._log_queue.put("✓  Binari OTA generat. Puja'l via web > Configuració > Actualitzar.\n")

    def _export_ota_all_thread(self, version: str, rel_dir: pathlib.Path):
        root_name = self._proj_root.name
        try:
            for chip in CHIPS:
                env_dir  = self._build_dir / chip
                fw_path  = env_dir / "firmware.bin"
                lfs_path = env_dir / "littlefs.bin"
                if not fw_path.is_file() or not lfs_path.is_file():
                    self._log_queue.put(f"\n⚠  {chip}: firmware.bin o littlefs.bin no trobats. Saltant.\n")
                    continue
                output   = str(rel_dir / f"{root_name}_{version}_{chip}_ota.bin")
                fw_data  = fw_path.read_bytes()
                lfs_data = lfs_path.read_bytes()
                with open(output, "wb") as f:
                    f.write(b"BLAU")
                    f.write(struct.pack("<II", len(fw_data), len(lfs_data)))
                    f.write(fw_data)
                    f.write(lfs_data)
                self._log_queue.put(f"\n{'─' * 64}\n")
                self._log_queue.put(f"EXPORT OTA [{chip}] → {output}\n")
                self._log_queue.put(f"  firmware:  {len(fw_data):,} bytes\n")
                self._log_queue.put(f"  littlefs:  {len(lfs_data):,} bytes\n")
                self._log_queue.put(f"  total:     {12 + len(fw_data) + len(lfs_data):,} bytes\n")
                self._log_queue.put(f"{'─' * 64}\n")
            self._log_queue.put("✓  Export OTA completat per a tots els xips. Puja via web > Configuració > Actualitzar.\n")
        finally:
            self.after(0, lambda: self._set_busy(False))


if __name__ == "__main__":
    app = MergeFlashTool()
    app.mainloop()
