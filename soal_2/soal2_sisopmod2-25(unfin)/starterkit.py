#!/usr/bin/env python3

import os, sys, time, base64
import shutil, signal #ist
from datetime import datetime
from pathlib import Path
from threading import Thread

# Konstanta
STARTER_KIT_DIR = Path("./starter_kit")
QUARANTINE_DIR = Path("./quarantine")
LOG_FILE = Path("activity.log")
PID_FILE = Path("decryptor.pid")

def log(msg):
    now = datetime.now().strftime("[%d-%m-%Y][%H:%M:%S]")
    with LOG_FILE.open("a") as f:
        f.write(f"{now} - {msg}\n")


def decode_filename(encoded):
    try:
        decoded = base64.b64decode(encoded).decode()
        return decoded
    except Exception:
        return None


def move_files(src_dir, dst_dir, log_template):
    for f in src_dir.glob("*"):
        if f.is_file():
            dst = dst_dir / f.name
            shutil.move(str(f), str(dst))
            log(log_template.format(f.name))


def delete_files_in(dir_path):
    for f in dir_path.glob("*"):
        if f.is_file():
            f.unlink()
            log(f"{f.name} - Successfully deleted.")


def decrypt_loop():
    while True:
        for f in STARTER_KIT_DIR.glob("*"):
            if f.is_file():
                decoded = decode_filename(f.name)
                if decoded and decoded != f.name:
                    new_path = STARTER_KIT_DIR / decoded
                    try:
                        f.rename(new_path)
                    except Exception:
                        continue
        time.sleep(5)


def start_decrypt_daemon():
    pid = os.fork()
    if pid > 0:
        # Parent: simpan PID
        with PID_FILE.open("w") as f:
            f.write(str(pid))
        log(f"Successfully started decryption process with PID {pid}.")
        sys.exit(0)
    else:
        # Child: daemon
        Thread(target=decrypt_loop, daemon=True).start()
        while True:
            time.sleep(60)


def shutdown_decrypt():
    if not PID_FILE.exists():
        print("No decryptor PID file found.")
        return
    pid = int(PID_FILE.read_text())
    try:
        os.kill(pid, signal.SIGTERM)
        log(f"Successfully shut off decryption process with PID {pid}.")
        PID_FILE.unlink()
    except ProcessLookupError:
        print("No such process.")
        PID_FILE.unlink()


def usage():
    print("Usage:")
    print("./starterkit.py --decrypt")
    print("./starterkit.py --shutdown")
    print("./starterkit.py --quarantine")
    print("./starterkit.py --return")
    print("./starterkit.py --eradicate")


def main():
    if len(sys.argv) != 2:
        usage()
        return

    command = sys.argv[1]
    if command == "--decrypt":
        start_decrypt_daemon()
    elif command == "--shutdown":
        shutdown_decrypt()
    elif command == "--quarantine":
        move_files(STARTER_KIT_DIR, QUARANTINE_DIR, "{} - Successfully moved to quarantine directory.")
    elif command == "--return":
        move_files(QUARANTINE_DIR, STARTER_KIT_DIR, "{} - Successfully returned to starter kit directory.")
    elif command == "--eradicate":
        delete_files_in(QUARANTINE_DIR)
    else:
        usage()

if __name__ == "__main__":
    main()