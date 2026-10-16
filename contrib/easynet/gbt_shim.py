#!/usr/bin/env python3
"""easynet: a shim between cpuminer-opt and the enforcer's block-template server, for solo mining against your own
enforcer, e.g. to get your own sidechain proposal (M1) into a block (EASYNET.md, section 6).

It fixes two mismatches:
- cpuminer-opt sends {"method":..,"params":..,"id":..} with no "jsonrpc" member; the enforcer (jsonrpsee) only
  accepts JSON-RPC 2.0 and answers "Invalid request". The shim adds "jsonrpc":"2.0" to each request and
  "error": null to each successful answer (1.0 clients look for it).
- The enforcer's template gives the coinbase in segwit form (`coinbasetxn`), and cpuminer-opt hashes it as it is,
  so its merkle root is wrong and the node rejects the block (bad-txnmrklroot). The shim hands cpuminer the coinbase
  without its witness, and puts the witness back when the block is submitted.
Long-poll requests are held as long as the enforcer holds them. Listens on 127.0.0.1 only. Needs only Python 3.
  gbt_shim.py LISTEN_PORT UPSTREAM_URL
  e.g. gbt_shim.py 8123 http://127.0.0.1:8122/   then   cpuminer -a sha256d -o http://127.0.0.1:8123 -u x -p x
"""
import json, sys, urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PORT, UPSTREAM = int(sys.argv[1]), sys.argv[2]
witness = None  # the coinbase witness from the latest template (the same 32-byte reserved value every time)

def varint(b, i):
    n = b[i]
    if n < 0xfd:
        return n, i + 1
    k = {0xfd: 2, 0xfe: 4, 0xff: 8}[n]
    return int.from_bytes(b[i + 1:i + 1 + k], "little"), i + 1 + k

def end_of_outputs(b, i):
    """Index just past the outputs of the transaction whose inputs count starts at b[i]."""
    n, i = varint(b, i)
    for _ in range(n):
        i += 36
        l, i = varint(b, i)
        i += l + 4
    n, i = varint(b, i)
    for _ in range(n):
        i += 8
        l, i = varint(b, i)
        i += l
    return i

def strip_coinbase(resp):
    """Give cpuminer the template's coinbase without its witness, so that it hashes to the coinbase's txid."""
    global witness
    r = resp.get("result") if isinstance(resp, dict) else None
    cb = r.get("coinbasetxn") if isinstance(r, dict) else None
    if isinstance(cb, dict) and isinstance(cb.get("data"), str):
        b = bytes.fromhex(cb["data"])
        if len(b) > 6 and b[4] == 0 and b[5] == 1:
            i = end_of_outputs(b, 6)
            witness = b[i:-4]
            cb["data"] = (b[:4] + b[6:i] + b[-4:]).hex()
    return resp

def restore_witness(req):
    """Put the coinbase witness back into a block cpuminer submits."""
    if witness and req.get("method") == "submitblock" and req.get("params"):
        b = bytes.fromhex(req["params"][0])
        _, at = varint(b, 80)  # the coinbase follows the header and the transaction count
        if not (b[at + 4] == 0 and b[at + 5] == 1):
            i = end_of_outputs(b, at + 4)
            req["params"][0] = (b[:at + 4] + b"\x00\x01" + b[at + 4:i] + witness + b[i:]).hex()
    return req

def fix(req):
    if isinstance(req, dict):
        if "jsonrpc" not in req:
            req["jsonrpc"] = "2.0"
            if "id" not in req or req["id"] is None:
                req["id"] = 0
        req = restore_witness(req)
    return req

def unfix(resp):
    if isinstance(resp, dict) and "result" in resp and "error" not in resp:
        resp["error"] = None
    return strip_coinbase(resp)

class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        try:
            req = json.loads(body)
            req = [fix(r) for r in req] if isinstance(req, list) else fix(req)
            up = urllib.request.Request(UPSTREAM, data=json.dumps(req).encode(),
                                        headers={"Content-Type": "application/json"})
            try:
                raw = urllib.request.urlopen(up, timeout=900).read()
            except urllib.error.HTTPError as e:
                raw = e.read()
            resp = json.loads(raw)
            resp = [unfix(r) for r in resp] if isinstance(resp, list) else unfix(resp)
            out, code = json.dumps(resp).encode(), 200
        except Exception as e:  # report, never crash the shim
            out, code = json.dumps({"result": None, "error": {"code": -1, "message": f"shim: {e}"}, "id": None}).encode(), 200
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(out)))
        self.end_headers()
        self.wfile.write(out)

    def log_message(self, *args):
        pass

ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
