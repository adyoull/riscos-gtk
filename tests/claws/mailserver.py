#!/usr/bin/env python3
"""A small POP3 and SMTP server for the Claws Mail test (tests/gtk/claws-fake.c).

  mailserver.py DIR POP3PORT SMTPPORT

Plain text: the fake AcornSSL in the test's fake RISC OS passes bytes
through unencrypted, so "TLS" (pop3s, SMTP STARTTLS) arrives here as plain
text after the client has started it. POP3 serves DIR/pop/* (one message a
file) to any user and password; SMTP offers STARTTLS (and expects it before
MAIL), and writes each message it's given to DIR/smtp/N.eml. Every command
goes to DIR/server.log as "POP3 <cmd>" / "SMTP <cmd>". MIT licence.
"""
import os
import socketserver
import sys
import threading

DIR, POP3PORT, SMTPPORT = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
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


class Server(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == "__main__":
    pop3 = Server(("127.0.0.1", POP3PORT), POP3)
    smtp = Server(("127.0.0.1", SMTPPORT), SMTP)
    threading.Thread(target=pop3.serve_forever, daemon=True).start()
    with open(os.path.join(DIR, "server.ready"), "w") as f:
        f.write("ok\n")
    smtp.serve_forever()
