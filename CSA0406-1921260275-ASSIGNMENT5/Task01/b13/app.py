# Result portal (demo): small Flask app used for B1.3, B2 and B3.
import hashlib
import socket

from flask import Flask, jsonify

app = Flask(__name__)


@app.route("/")
def index():
    return "University Result Portal (demo)\n"


@app.route("/health")
def health():
    return jsonify(status="ok", host=socket.gethostname())


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
