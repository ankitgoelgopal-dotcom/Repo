#!/usr/bin/env python3
"""Local HTTPS test server (TLS 1.2): https_server.py <certdir> [port]   -- reachable from the guest as 10.0.2.2:<port>"""
import http.server, ssl, sys
cert = sys.argv[1]; port = int(sys.argv[2]) if len(sys.argv) > 2 else 8443
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = b"<html><body>QAR-OS TLS test page</body></html>"
        self.send_response(200); self.send_header("Content-Length", str(len(body))); self.send_header("Connection", "close"); self.end_headers(); self.wfile.write(body)
    def log_message(self, *a): pass
srv = http.server.HTTPServer(("0.0.0.0", port), H)
ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); ctx.maximum_version = ssl.TLSVersion.TLSv1_2
ctx.load_cert_chain(f"{cert}/server.pem", f"{cert}/server.key")
srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
srv.serve_forever()
