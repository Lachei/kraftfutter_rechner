import argparse
import http.server
import socketserver
import os

parser = argparse.ArgumentParser(
                    prog='TestIotServer',
                    description='Emulates the pico iot device')

parser.add_argument('-p', '--port', help='The port the server listens to, default: 8080', default=8080);

args = parser.parse_args()

log_counter = 0
hostname = "A beatiful thing"
ap_active = "true"

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory='.', **kwargs)

    def guess_type(self, path):
        file = filename = os.path.basename(path)
        if file == 'internet' or file == 'overview' or file == 'settings':
            return 'text/html'
        if file == 'style':
            return 'text/css'
        # default fallback to parent class type guessing
        return super().guess_type(path)

    def do_GET(self):
        global log_counter
        global hostname
        global ap_active
        if self.path == '/logs':
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
            self.wfile.write(f"[info] logs counter at {log_counter}".encode())
            log_counter += 1
        elif self.path == '/discovered_wifis':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write('[{"ssid":"test1","rssi":-61,"connected":false},{"ssid":"test2","rssi":-31,"connected":true}]'.encode())
        elif self.path == '/ap_active':
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
            self.wfile.write(f'{ap_active}'.encode())
        elif self.path == '/host_name':
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
            self.wfile.write(f'{hostname}'.encode())
        else:
            super().do_GET()

    def do_POST(self):
        global hostname
        global ap_active
        if self.path == '/host_name':
            content_len = int(self.headers.get('content-length', 0))
            hostname = self.rfile.read(content_len).decode()
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
        elif self.path == '/ap_active':
            content_len = int(self.headers.get('content-length', 0))
            ap_active = self.rfile.read(content_len).decode()
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
        elif self.path == '/wifi_connect':
            content_len = int(self.headers.get('content-length', 0))
            wifi = self.rfile.read(content_len).decode()
            ssid, pwd = wifi.split(' ')
            print(f'Tried to connect to wifi {ssid} with pwd {pwd}')
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
        else:
            super().do_POST()

with socketserver.TCPServer(("", args.port), Handler) as httpd:
    print("serving at port", args.port)
    httpd.serve_forever()

