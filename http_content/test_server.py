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
        if self.path =='/logs':
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
            self.wfile.write(f"[info] logs counter at {log_counter}".encode())
            log_counter += 1
        else:
            super().do_GET()

with socketserver.TCPServer(("", args.port), Handler) as httpd:
    print("serving at port", args.port)
    httpd.serve_forever()

