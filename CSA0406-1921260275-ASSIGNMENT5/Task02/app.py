# Result portal (demo): small Flask app used for B2 and B3.
import hashlib
import os
import socket

from flask import Flask, jsonify

app = Flask(__name__)


@app.route("/")
def index():
    return "University Result Portal (demo)\n"


@app.route("/health")
def health():
    return jsonify(status="ok", host=socket.gethostname())


@app.route("/dbcheck")
def dbcheck():
    # Compose demo: can the portal container reach the database container over the network?
    host = os.environ.get("DB_HOST", "db")
    try:
        socket.create_connection((host, 5432), timeout=2).close()
        return jsonify(db=host, reachable=True)
    except OSError as e:
        return jsonify(db=host, reachable=False, error=str(e)), 503


@app.route("/result/<reg>")
def result(reg):
    if not (reg.isdigit() and len(reg) == 10):
        return jsonify(error="register number must be 10 digits"), 400
    # DEMO data only: marks are derived from a hash of the register number.
    h = hashlib.sha256(reg.encode()).digest()
    marks = {"OS": 50 + h[0] % 51, "Maths": 50 + h[1] % 51, "Networks": 50 + h[2] % 51}
    return jsonify(reg=reg, marks=marks, demo=True)


if __name__ == "__main__":
    app.run(host="127.0.0.1", port=8000)
