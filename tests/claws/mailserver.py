#!/usr/bin/env python3
"""A small POP3 and SMTP server for the Claws Mail test (tests/gtk/claws-fake.c).

  mailserver.py DIR POP3PORT SMTPPORT [IMAPPORT [HTTPPORT [SIEVEPORT]]]

Plain text: the fake AcornSSL in the test's fake RISC OS passes bytes
through unencrypted, so "TLS" (pop3s, SMTP STARTTLS) arrives here as plain
text after the client has started it. POP3 serves DIR/pop/* (one message a
file) to any user and password; SMTP offers STARTTLS (and expects it before
MAIL), and writes each message it's given to DIR/smtp/N.eml. Every command
goes to DIR/server.log as "POP3 <cmd>" / "SMTP <cmd>" / "IMAP <cmd>".
IMAP (if IMAPPORT is given): one mailbox, INBOX, holding DIR/imap/*
(UIDs 1, 2 ...), enough of IMAP4rev1 for Claws Mail through libetpan:
LOGIN, LIST/LSUB, SELECT/EXAMINE, STATUS, (UID) SEARCH, (UID) FETCH of
UID, FLAGS, RFC822.SIZE, INTERNALDATE, BODY[...] / BODY.PEEK[...] (whole,
HEADER, TEXT, HEADER.FIELDS) and RFC822*, (UID) STORE, NOOP, IDLE.
HTTP (if HTTPPORT is given; https arrives as plain text too): GET of
DIR/http/NAME, sent chunked, with an ETag; /moved/NAME answers 302 with a
relative Location (/NAME); /auth/NAME wants Basic authentication as
test:secret (401 without); PUT /auth/NAME writes DIR/http/put-NAME.
Logged as "HTTP GET <path>" (and "HTTP PUT <path>", "HTTP auth ok").
ManageSieve (if SIEVEPORT is given): capabilities, STARTTLS (then the
capabilities again), LISTSCRIPTS (riscos-test, active, and holiday),
GETSCRIPT, NOOP, LOGOUT; logged as "SIEVE <cmd>".
MIT licence.
"""
import base64
import os
import socketserver
import sys
import threading

DIR, POP3PORT, SMTPPORT = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
IMAPPORT = int(sys.argv[4]) if len(sys.argv) > 4 else 0
HTTPPORT = int(sys.argv[5]) if len(sys.argv) > 5 else 0
SIEVEPORT = int(sys.argv[6]) if len(sys.argv) > 6 else 0
lock = threading.Lock()


def log(line):
    with lock, open(os.path.join(DIR, "server.log"), "a") as f:
        f.write(line + "\n")


class POP3(socketserver.StreamRequestHandler):
    def send(self, s):
        self.wfile.write((s + "\r\n").encode("latin-1"))

    def handle(self):
        pop = os.path.join(DIR, "pop")
        msgs = [open(os.path.join(pop, n), "rb").read() for n in sorted(os.listdir(pop))]
        deleted = set()
        self.send("+OK test POP3 ready")
        for raw in self.rfile:
            line = raw.decode("latin-1").rstrip("\r\n")
            cmd = line.split(" ")[0].upper()
            log("POP3 " + (line if cmd != "PASS" else "PASS ****"))
            arg = line[len(cmd):].strip()
            if cmd in ("USER", "PASS", "NOOP", "RSET"):
                self.send("+OK")
            elif cmd == "CAPA":
                self.send("+OK\r\nUSER\r\nUIDL\r\nTOP\r\n.")
            elif cmd == "STAT":
                self.send("+OK %d %d" % (len(msgs), sum(map(len, msgs))))
            elif cmd == "LIST":
                self.send("+OK\r\n" + "".join("%d %d\r\n" % (i + 1, len(m)) for i, m in enumerate(msgs)) + ".")
            elif cmd == "UIDL":
                if arg:
                    self.send("+OK %s uid%s" % (arg, arg))
                else:
                    self.send("+OK\r\n" + "".join("%d uid%d\r\n" % (i + 1, i + 1) for i in range(len(msgs))) + ".")
            elif cmd in ("RETR", "TOP"):
                m = msgs[int(arg.split()[0]) - 1]
                body = m.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
                body = b"\r\n".join((b"." + l if l.startswith(b".") else l) for l in body.split(b"\r\n"))
                self.wfile.write(b"+OK\r\n" + body.rstrip(b"\r\n") + b"\r\n.\r\n")
            elif cmd == "DELE":
                deleted.add(int(arg))
                self.send("+OK")
            elif cmd == "QUIT":
                self.send("+OK bye")
                return
            else:
                self.send("-ERR unknown")


class SMTP(socketserver.StreamRequestHandler):
    def send(self, s):
        self.wfile.write((s + "\r\n").encode("latin-1"))

    def handle(self):
        tls = False
        self.send("220 test ESMTP")
        while True:
            raw = self.rfile.readline()
            if not raw:
                return
            line = raw.decode("latin-1").rstrip("\r\n")
            cmd = line.split(" ")[0].upper()
            log("SMTP " + line)
            if cmd in ("EHLO", "HELO"):
                self.send("250-test\r\n250-8BITMIME\r\n250-SIZE 1000000\r\n" +
                          ("250 OK" if tls else "250-STARTTLS\r\n250 OK"))
            elif cmd == "STARTTLS":
                self.send("220 go ahead")
                tls = True
            elif cmd == "MAIL":
                self.send("250 OK" if tls else "530 STARTTLS first")
            elif cmd in ("RCPT", "RSET", "NOOP"):
                self.send("250 OK")
            elif cmd == "DATA":
                self.send("354 end with .")
                data = []
                for raw in self.rfile:
                    if raw in (b".\r\n", b".\n"):
                        break
                    data.append(raw[1:] if raw.startswith(b"..") else raw)
                os.makedirs(os.path.join(DIR, "smtp"), exist_ok=True)
                n = len(os.listdir(os.path.join(DIR, "smtp"))) + 1
                with open(os.path.join(DIR, "smtp", "%d.eml" % n), "wb") as f:
                    f.write(b"".join(data))
                log("SMTP <message %d, %d bytes>" % (n, sum(map(len, data))))
                self.send("250 queued")
            elif cmd == "QUIT":
                self.send("221 bye")
                return
            else:
                self.send("502 unknown")


def imap_tokens(s):
    """IMAP arguments: atoms, "quoted" strings and (lists), flattened as text"""
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == " ":
            i += 1
        elif c == '"':
            j, v = i + 1, ""
            while j < len(s) and s[j] != '"':
                if s[j] == "\\":
                    j += 1
                v += s[j]
                j += 1
            out.append(v)
            i = j + 1
        elif c == "(":
            depth, j = 0, i
            while j < len(s):
                depth += {"(": 1, ")": -1}.get(s[j], 0)
                if depth == 0:
                    break
                j += 1
            out.append(s[i:j + 1])
            i = j + 1
        else:
            j = i
            depth = 0
            while j < len(s) and (s[j] != " " or depth):
                depth += {"[": 1, "]": -1}.get(s[j], 0)
                j += 1
            out.append(s[i:j])
            i = j
    return out


def in_set(n, last, spec):
    for part in spec.split(","):
        a, _, b = part.partition(":")
        a = last if a == "*" else int(a)
        b = a if not b else (last if b == "*" else int(b))
        if min(a, b) <= n <= max(a, b):
            return True
    return False


class IMAP(socketserver.StreamRequestHandler):
    def send(self, s):
        self.wfile.write((s + "\r\n").encode("latin-1"))

    def literal(self, data):
        return b"{%d}\r\n" % len(data) + data

    def section(self, m, sec):
        """BODY[sec] of message m (CRLF lines)"""
        head, _, body = m.partition(b"\r\n\r\n")
        head += b"\r\n\r\n"
        s = sec.upper()
        if s == "":
            return m
        if s == "HEADER":
            return head
        if s == "TEXT":
            return body
        if s.startswith("HEADER.FIELDS"):
            names = [n.lower() for n in sec[sec.index("(") + 1:sec.rindex(")")].split()]
            keep, lines, cur = [], head.split(b"\r\n"), None
            for l in lines:
                if l[:1] in (b" ", b"\t") and cur:
                    keep.append(l)
                    continue
                cur = l.split(b":")[0].decode("latin-1").lower() in names and l
                if cur:
                    keep.append(l)
            return b"\r\n".join(keep) + b"\r\n\r\n"
        if s == "1":
            return body
        return b""

    def fetch(self, tag, uid, args):
        spec, items = args[0], args[1] if len(args) > 1 else "ALL"
        items = items.strip("()")
        last = len(self.msgs)
        for i, m in enumerate(self.msgs):
            n = i + 1
            if not in_set(n, last, spec):
                continue
            parts = []
            if uid and "UID" not in items.upper().split():
                parts.append(b"UID %d" % n)
            for it in imap_tokens(items):
                u = it.upper()
                if u == "UID":
                    parts.append(b"UID %d" % n)
                elif u == "FLAGS":
                    parts.append(b"FLAGS (%s)" % b" ".join(self.flags[i]))
                elif u == "RFC822.SIZE":
                    parts.append(b"RFC822.SIZE %d" % len(m))
                elif u == "INTERNALDATE":
                    parts.append(b'INTERNALDATE "07-Oct-2026 10:00:00 +0100"')
                elif u in ("RFC822", "RFC822.PEEK"):
                    parts.append(b"RFC822 " + self.literal(m))
                elif u == "RFC822.HEADER":
                    parts.append(b"RFC822.HEADER " + self.literal(self.section(m, "HEADER")))
                elif u == "RFC822.TEXT":
                    parts.append(b"RFC822.TEXT " + self.literal(self.section(m, "TEXT")))
                elif u.startswith("BODY[") or u.startswith("BODY.PEEK["):
                    sec = it[it.index("[") + 1:it.rindex("]")]
                    data = self.section(m, sec)
                    rest = it[it.rindex("]") + 1:]
                    if rest.startswith("<"):
                        a, b = (int(x) for x in rest.strip("<>").split("."))
                        data = data[a:a + b]
                        label = "BODY[%s]<%d>" % (sec, a)
                    else:
                        label = "BODY[%s]" % sec
                    parts.append(label.encode("latin-1") + b" " + self.literal(data))
                    if u.startswith("BODY[") and b"\\Seen" not in self.flags[i]:
                        self.flags[i].append(b"\\Seen")
            self.wfile.write(b"* %d FETCH (" % n + b" ".join(parts) + b")\r\n")
        self.send(tag + " OK FETCH done")

    def readline_with_literals(self):
        raw = self.rfile.readline()
        if not raw:
            return None
        line = raw.decode("latin-1").rstrip("\r\n")
        while line.endswith("}") and "{" in line:
            n = int(line[line.rindex("{") + 1:-1].rstrip("+"))
            if not line[line.rindex("{") + 1:-1].endswith("+"):
                self.send("+ go ahead")
            data = self.rfile.read(n).decode("latin-1")
            line = line[:line.rindex("{")] + '"' + data + '"' + self.rfile.readline().decode("latin-1").rstrip("\r\n")
        return line

    def handle(self):
        d = os.path.join(DIR, "imap")
        self.msgs = [open(os.path.join(d, n), "rb").read().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
                     for n in sorted(os.listdir(d))] if os.path.isdir(d) else []
        self.flags = [[] for _ in self.msgs]
        self.send("* OK [CAPABILITY IMAP4rev1 IDLE LITERAL+] test IMAP ready")
        while True:
            line = self.readline_with_literals()
            if line is None:
                return
            tag, _, rest = line.partition(" ")
            cmd, _, arg = rest.partition(" ")
            cmd = cmd.upper()
            uid = False
            if cmd == "UID":
                uid = True
                cmd, _, arg = arg.partition(" ")
                cmd = cmd.upper()
            log("IMAP " + ("%s LOGIN ****" % tag if cmd == "LOGIN" else line))
            args = imap_tokens(arg)
            n = len(self.msgs)
            if cmd == "CAPABILITY":
                self.send("* CAPABILITY IMAP4rev1 IDLE LITERAL+")
                self.send(tag + " OK done")
            elif cmd == "LOGIN":
                self.send(tag + " OK [CAPABILITY IMAP4rev1 IDLE LITERAL+] logged in")
            elif cmd == "NAMESPACE":
                self.send('* NAMESPACE (("" "/")) NIL NIL')
                self.send(tag + " OK done")
            elif cmd in ("LIST", "LSUB", "XLIST"):
                pat = args[1] if len(args) > 1 else ""
                if pat == "":
                    self.send('* %s (\\Noselect) "/" ""' % cmd)
                else:
                    self.send('* %s (\\HasNoChildren) "/" INBOX' % cmd)
                self.send(tag + " OK done")
            elif cmd in ("SELECT", "EXAMINE"):
                self.send("* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)")
                self.send("* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft \\*)]")
                self.send("* %d EXISTS" % n)
                self.send("* 0 RECENT")
                self.send("* OK [UIDVALIDITY 1]")
                self.send("* OK [UIDNEXT %d]" % (n + 1))
                self.send(tag + " OK [%s] done" % ("READ-WRITE" if cmd == "SELECT" else "READ-ONLY"))
            elif cmd == "STATUS":
                unseen = sum(1 for f in self.flags if b"\\Seen" not in f)
                self.send("* STATUS INBOX (MESSAGES %d RECENT 0 UIDNEXT %d UIDVALIDITY 1 UNSEEN %d)" % (n, n + 1, unseen))
                self.send(tag + " OK done")
            elif cmd == "SEARCH":
                self.send("* SEARCH " + " ".join(str(i + 1) for i in range(n)))
                self.send(tag + " OK done")
            elif cmd == "FETCH":
                self.fetch(tag, uid, args)
            elif cmd == "STORE":
                spec, how, fl = args[0], args[1].upper(), args[2].strip("()").encode("latin-1").split()
                for i in range(n):
                    if in_set(i + 1, n, spec):
                        if how.startswith("+"):
                            self.flags[i] = list(dict.fromkeys(self.flags[i] + fl))
                        elif how.startswith("-"):
                            self.flags[i] = [f for f in self.flags[i] if f not in fl]
                        else:
                            self.flags[i] = fl
                        if ".SILENT" not in how:
                            self.wfile.write(b"* %d FETCH (UID %d FLAGS (%s))\r\n" % (i + 1, i + 1, b" ".join(self.flags[i])))
                self.send(tag + " OK done")
            elif cmd == "IDLE":
                self.send("+ idling")
                while True:
                    l = self.rfile.readline()
                    if not l or l.strip().upper() == b"DONE":
                        break
                self.send(tag + " OK done")
            elif cmd == "LOGOUT":
                self.send("* BYE")
                self.send(tag + " OK done")
                return
            elif cmd in ("NOOP", "CHECK", "CLOSE", "EXPUNGE", "SUBSCRIBE", "UNSUBSCRIBE", "ENABLE", "ID", "UNSELECT"):
                self.send(tag + " OK done")
            else:
                self.send(tag + " BAD unknown")


TYPES = {".png": "image/png", ".rss": "application/rss+xml", ".xml": "application/rss+xml",
         ".ics": "text/calendar"}


class HTTP(socketserver.StreamRequestHandler):
    def reply(self, head, body=b""):
        self.wfile.write(head.encode("latin-1") + b"Connection: close\r\n\r\n" + body)

    def handle(self):
        req = self.rfile.readline().decode("latin-1").split()
        headers = {}
        while True:
            line = self.rfile.readline().decode("latin-1").strip()
            if not line:
                break
            k, _, v = line.partition(":")
            headers[k.strip().lower()] = v.strip()
        method = req[0] if req else "GET"
        path = req[1] if len(req) > 1 else "/"
        log("HTTP %s %s" % (method, path))
        if path.startswith("/moved/"):
            self.reply("HTTP/1.1 302 Found\r\nLocation: /%s\r\nContent-Length: 0\r\n" % path[7:])
            return
        if path.startswith("/auth/"):
            if headers.get("authorization") != "Basic " + base64.b64encode(b"test:secret").decode():
                self.reply("HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"test\"\r\n"
                           "Content-Length: 0\r\n")
                return
            log("HTTP auth ok")
        name = os.path.basename(path.split("?")[0])
        if method == "PUT":
            data = self.rfile.read(int(headers.get("content-length", "0")))
            with open(os.path.join(DIR, "http", "put-" + name), "wb") as f:
                f.write(data)
            log("HTTP PUT %d bytes, %s" % (len(data), headers.get("content-type", "")))
            self.reply("HTTP/1.1 201 Created\r\nContent-Length: 0\r\n")
            return
        f = os.path.join(DIR, "http", name)
        if not os.path.isfile(f):
            self.reply("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n")
            return
        data = open(f, "rb").read()
        out = b""
        for i in range(0, len(data), 100):
            c = data[i:i + 100]
            out += b"%x\r\n" % len(c) + c + b"\r\n"
        self.reply("HTTP/1.1 200 OK\r\nContent-Type: %s\r\nETag: \"riscos-%d\"\r\n"
                   "Transfer-Encoding: chunked\r\n" % (TYPES.get(os.path.splitext(name)[1], "application/octet-stream"),
                                                       len(data)), out + b"0\r\n\r\n")


class Sieve(socketserver.StreamRequestHandler):
    SCRIPT = b'require "fileinto";\r\nif header :contains "subject" "RISC OS" { fileinto "riscos"; }\r\n'

    def send(self, s):
        self.wfile.write(s.encode("latin-1") + b"\r\n")

    def caps(self):
        self.send('"IMPLEMENTATION" "riscos-gtk test"')
        self.send('"SASL" "PLAIN"')
        self.send('"SIEVE" "fileinto"')
        self.send('"STARTTLS"')
        self.send('"VERSION" "1.0"')
        self.send('OK "ready"')

    def handle(self):
        self.caps()
        while True:
            line = self.rfile.readline().decode("latin-1").strip()
            if not line:
                return
            cmd = line.split()[0].upper()
            log("SIEVE " + cmd)
            if cmd == "STARTTLS":
                self.send('OK "Begin TLS negotiation now"')
                self.caps()
            elif cmd == "LISTSCRIPTS":
                self.send('"riscos-test" ACTIVE')
                self.send('"holiday"')
                self.send('OK "Listscripts completed"')
            elif cmd == "GETSCRIPT":
                self.wfile.write(b"{%d}\r\n" % len(self.SCRIPT) + self.SCRIPT + b"\r\n")
                self.send('OK "Getscript completed"')
            elif cmd == "NOOP":
                self.send('OK "NOOP completed"')
            elif cmd == "LOGOUT":
                self.send('OK "Logout completed"')
                return
            else:
                self.send('NO "not in this test"')


class Server(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == "__main__":
    pop3 = Server(("127.0.0.1", POP3PORT), POP3)
    smtp = Server(("127.0.0.1", SMTPPORT), SMTP)
    threading.Thread(target=pop3.serve_forever, daemon=True).start()
    if IMAPPORT:
        imap = Server(("127.0.0.1", IMAPPORT), IMAP)
        threading.Thread(target=imap.serve_forever, daemon=True).start()
    if HTTPPORT:
        http = Server(("127.0.0.1", HTTPPORT), HTTP)
        threading.Thread(target=http.serve_forever, daemon=True).start()
    if SIEVEPORT:
        sieve = Server(("127.0.0.1", SIEVEPORT), Sieve)
        threading.Thread(target=sieve.serve_forever, daemon=True).start()
    with open(os.path.join(DIR, "server.ready"), "w") as f:
        f.write("ok\n")
    smtp.serve_forever()
