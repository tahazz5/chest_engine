"""Local graphical demo: python3 ui/server.py, then http://localhost:8000."""
import json
from pathlib import Path
import re
import subprocess
from http.server import BaseHTTPRequestHandler, HTTPServer

ROOT = Path(__file__).resolve().parents[1]


def position(moves):
    if not isinstance(moves, list) or len(moves) > 500 or any(
        not isinstance(m, str) or not re.fullmatch(r"[a-h][1-8][a-h][1-8][qrbn]?", m)
        for m in moves
    ):
        raise ValueError("Invalid move history")
    # Replay the small demo game so each browser owns its own history. The
    # engine remains responsible for legal moves, the opponent, and endings.
    output = subprocess.run(
        [str(ROOT / "build-release/chess_engine"), "--play"],
        input="\n".join([*moves, "coups", "quit", ""]),
        text=True, capture_output=True, check=True, timeout=20,
    ).stdout
    if "Coup illegal" in output:
        raise ValueError("Illegal move")
    rows = re.findall(r"^[1-8]  ([.PNBRQKpnbrqk ]+)$", output, re.M)
    board = "".join(rows[-8:]).replace(" ", "")
    ending = re.search(r"(?:Echec et mat\.|Pat :|Nulle :)[^\n]*", output)
    legal_lines = re.findall(r"Blancs > ((?:[a-h][1-8][a-h][1-8][qrbn]? )+)\n", output)
    return {"board": board, "legal": legal_lines[-1].split() if legal_lines and not ending else [],
            "status": ending.group() if ending else "Your turn · White",
            "reply": re.findall(r"Ordinateur : (\w+)", output)[-1:]}


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path not in ("/", "/index.html"):
            self.send_error(404)
            return
        body = (ROOT / "ui/index.html").read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        try:
            size = int(self.headers.get("Content-Length", 0))
            if self.path != "/position" or not 0 < size <= 10000:
                raise ValueError("Invalid request")
            result = position(json.loads(self.rfile.read(size))["moves"])
            body = json.dumps(result).encode()
            self.send_response(200)
        except (ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
            body = json.dumps({"error": str(error)}).encode()
            self.send_response(400)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(body)


if __name__ == "__main__":
    print("Play at http://localhost:8000", flush=True)
    HTTPServer(("127.0.0.1", 8000), Handler).serve_forever()
