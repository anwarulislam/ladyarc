#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause

import functools
import http.server
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading


def main():
    with tempfile.TemporaryDirectory(prefix="ladyarc-spaces-") as directory:
        for name in ("work", "personal", "read"):
            setup = "" if name == "read" else f'document.cookie = "login={name}; path=/"; localStorage.setItem("login", "{name}");'
            Path(directory, f"{name}.html").write_text(
                f"""<!doctype html><meta charset="utf-8"><script>
                {setup}
                const cookie = document.cookie.split("; ").find(part => part.startsWith("login="));
                document.title = (cookie ? cookie.slice(6) : "empty") + "/" + (localStorage.getItem("login") || "empty");
                </script>""", encoding="utf-8",
            )
        handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=directory)
        with http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler) as server:
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                env = os.environ.copy()
                env["LADYARC_TEST_ORIGIN"] = f"http://127.0.0.1:{server.server_port}/"
                if not env.get("DISPLAY") and not env.get("WAYLAND_DISPLAY"):
                    env.setdefault("QT_QPA_PLATFORM", "offscreen")
                subprocess.run([sys.argv[1], "--temporary-profile"], env=env, check=True, timeout=120)
            finally:
                server.shutdown()
                thread.join()


if __name__ == "__main__":
    main()
