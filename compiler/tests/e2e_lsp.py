#!/usr/bin/env python3
# Integration test v1.17.0 (Phase 36): LSP end-to-end via stdio.
# Harness: framed JSON-RPC di atas `nusa lsp`; asersi per fitur.
import json
import subprocess
import sys

NUSA_BIN = sys.argv[1]
fails = []


def check(name, cond, extra=""):
    if cond:
        print("case %s OK" % name)
    else:
        print("E2E FAIL [%s] %s" % (name, extra))
        fails.append(name)


class Client:
    def __init__(self):
        self.proc = subprocess.Popen(
            [NUSA_BIN, "lsp"], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        self.rid = 0

    def send(self, obj):
        body = json.dumps(obj).encode()
        self.proc.stdin.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
        self.proc.stdin.flush()

    def recv(self):
        headers = {}
        while True:
            line = self.proc.stdout.readline().decode().strip()
            if not line:
                break
            k, _, v = line.partition(":")
            headers[k.strip().lower()] = v.strip()
        n = int(headers["content-length"])
        data = b""
        while len(data) < n:
            chunk = self.proc.stdout.read(n - len(data))
            if not chunk:
                break
            data += chunk
        return json.loads(data)

    def req(self, method, params):
        self.rid += 1
        self.send({"jsonrpc": "2.0", "id": self.rid, "method": method,
                   "params": params})
        return self.recv()

    def notify(self, method, params):
        self.send({"jsonrpc": "2.0", "method": method, "params": params})

    def close(self):
        try:
            self.proc.wait(timeout=10)
        except Exception:
            self.proc.kill()
        return self.proc.returncode


URI = "file:///tmp/lsp_e2e.ns"
GOOD = ("fn add(a: i64, b: i64) -> i64 {\n"
        "    return a + b;\n"
        "}\n"
        "enum Color { Red, Green }\n"
        "fn main() {\n"
        "    let total = add(1, 2);\n"
        "    print(total);\n"
        "}\n")
BAD = "fn main() {\n    let x = nope;\n}\n"

c = Client()

# 1. initialize: capabilities + serverInfo.
r = c.req("initialize", {"capabilities": {}})
caps = r["result"]["capabilities"]
check("init", caps.get("hoverProvider") is True and
      caps.get("definitionProvider") is True and
      caps.get("documentSymbolProvider") is True and
      caps.get("documentFormattingProvider") is True and
      caps.get("renameProvider") is True and
      caps.get("referencesProvider") is True and
      "completionProvider" in caps and
      r["result"]["serverInfo"]["name"] == "nusa-lsp", str(caps))
c.notify("initialized", {})

# 2. didOpen BAD → 1 diagnostic menunjuk 'nope'.
c.notify("textDocument/didOpen", {"textDocument": {
    "uri": URI, "languageId": "nusa", "version": 1, "text": BAD}})
d = c.recv()
dgs = d["params"]["diagnostics"]
check("diag-err", len(dgs) == 1 and "nope" in dgs[0]["message"] and
      dgs[0]["range"]["start"]["line"] == 1, str(dgs))

# 3. didChange GOOD → diagnostics kosong.
c.notify("textDocument/didChange", {"textDocument": {"uri": URI, "version": 2},
                                    "contentChanges": [{"text": GOOD}]})
d = c.recv()
check("diag-ok", d["params"]["diagnostics"] == [], str(d))

# 4. completion prefix "ad" → add (bukan print).
r = c.req("textDocument/completion", {
    "textDocument": {"uri": URI},
    "position": {"line": 5, "character": 19}})
labels = [i["label"] for i in r["result"]]
check("complete", "add" in labels and "print" not in labels, str(labels))
# prefix kosong di awal → keywords ada.
r = c.req("textDocument/completion", {
    "textDocument": {"uri": URI},
    "position": {"line": 0, "character": 0}})
labels = [i["label"] for i in r["result"]]
check("complete-kw", "fn" in labels and "match" in labels, str(labels[:12]))

# 5. hover: fn + keyword + unknown.
r = c.req("textDocument/hover", {
    "textDocument": {"uri": URI},
    "position": {"line": 5, "character": 17}})
check("hover-fn", "fn add(a: i64, b: i64) -> i64" in
      r["result"]["contents"]["value"], str(r))
r = c.req("textDocument/hover", {
    "textDocument": {"uri": URI},
    "position": {"line": 0, "character": 1}})
check("hover-kw", "keyword" in str(r), str(r))
r = c.req("textDocument/hover", {
    "textDocument": {"uri": URI},
    "position": {"line": 5, "character": 14}})
check("hover-null", r["result"] is None, str(r))

# 6. definition: panggilan add → baris 0; local total → baris 5.
r = c.req("textDocument/definition", {
    "textDocument": {"uri": URI},
    "position": {"line": 5, "character": 17}})
check("def-fn", r["result"]["range"]["start"] == {"line": 0, "character": 3},
      str(r))
r = c.req("textDocument/definition", {
    "textDocument": {"uri": URI},
    "position": {"line": 6, "character": 12}})
check("def-local", r["result"]["range"]["start"]["line"] == 5, str(r))

# 7. documentSymbol: add/main/Color + varian Red/Green.
r = c.req("textDocument/documentSymbol", {
    "textDocument": {"uri": URI}})
syms = {s["name"]: s for s in r["result"]}
check("symbols", set(["add", "main", "Color"]) <= set(syms.keys()),
      str(list(syms.keys())))
kids = [k["name"] for k in syms["Color"].get("children", [])]
check("symbols-kids", kids == ["Red", "Green"], str(kids))
check("symbols-range", syms["add"]["range"]["start"] == {"line": 0, "character": 0} and
      syms["add"]["range"]["end"]["line"] == 2, str(syms["add"]["range"]))

# 8. formatting: berantakan → rapi; unknown-uri → null.
messy = "fn   main(){print(   \"hi\" );}\n"
c.notify("textDocument/didOpen", {"textDocument": {
    "uri": URI + "2", "languageId": "nusa", "version": 1, "text": messy}})
c.recv()  # diagnostics (kosong)
r = c.req("textDocument/formatting", {
    "textDocument": {"uri": URI + "2"}, "options": {}})
check("format", r["result"][0]["newText"] ==
      "fn main() {\n    print(\"hi\");\n}\n", str(r))
r = c.req("textDocument/formatting", {
    "textDocument": {"uri": "file:///nope.ns"}, "options": {}})
check("format-null", r["result"] is None, str(r))

# 9. rename local total → sum (2 lokasi: def + use).
r = c.req("textDocument/rename", {
    "textDocument": {"uri": URI},
    "position": {"line": 6, "character": 12}, "newName": "sum"})
edits = r["result"]["changes"][URI]
check("rename-n", len(edits) == 2, str(edits))
lines = sorted(e["range"]["start"]["line"] for e in edits)
check("rename-lines", lines == [5, 6], str(edits))
cols = sorted(e["range"]["start"]["character"] for e in edits)
check("rename-cols", cols == [8, 10], str(edits))
check("rename-text", all(e["newText"] == "sum" for e in edits), str(edits))
# rename di posisi kosong → null.
r = c.req("textDocument/rename", {
    "textDocument": {"uri": URI},
    "position": {"line": 5, "character": 0}, "newName": "sum"})
check("rename-null", r["result"] is None, str(r))

# 10. references local total (includeDeclaration true → 2 lokasi).
r = c.req("textDocument/references", {
    "textDocument": {"uri": URI},
    "position": {"line": 6, "character": 12},
    "context": {"includeDeclaration": True}})
locs = r["result"]
check("refs-n", len(locs) == 2, str(locs))
lines = sorted(l["range"]["start"]["line"] for l in locs)
check("refs-lines", lines == [5, 6], str(locs))
# tanpa deklarasi → hanya use (baris 6).
r = c.req("textDocument/references", {
    "textDocument": {"uri": URI},
    "position": {"line": 5, "character": 9},
    "context": {"includeDeclaration": False}})
locs = r["result"]
check("refs-nodecl", len(locs) == 1 and
      locs[0]["range"]["start"]["line"] == 6, str(locs))
# posisi kosong → array kosong.
r = c.req("textDocument/references", {
    "textDocument": {"uri": URI},
    "position": {"line": 5, "character": 0},
    "context": {"includeDeclaration": True}})
check("refs-null", r["result"] == [], str(r))

# 11. method tak dikenal → -32601.
r = c.req("workspace/wobble", {})
check("bad-method", r.get("error", {}).get("code") == -32601, str(r))

# 12. didClose membersihkan; shutdown/exit 0.
c.notify("textDocument/didClose", {"textDocument": {"uri": URI}})
d = c.recv()
check("close", d["params"]["diagnostics"] == [], str(d))
r = c.req("shutdown", None)
check("shutdown", r["result"] is None, str(r))
c.notify("exit", None)
check("exit", c.close() == 0, "")

if fails:
    print("E2E LSP FAIL: %s" % fails)
    sys.exit(1)
print("E2E LSP PASS")
