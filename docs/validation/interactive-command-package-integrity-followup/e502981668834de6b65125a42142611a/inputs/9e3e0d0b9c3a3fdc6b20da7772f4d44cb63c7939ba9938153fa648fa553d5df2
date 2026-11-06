"""Fresh loopback TLS fixtures shared by host integration tests."""
import contextlib
import http.server
import os
import pathlib
import shutil
import ssl
import subprocess
import threading


def openssl_executable():
    selected = os.environ.get('CODET_TEST_OPENSSL') or 'openssl'
    executable = shutil.which(selected)
    if executable is None:
        raise RuntimeError('Host TLS tests require the openssl executable; install OpenSSL '
                           'or set CODET_TEST_OPENSSL to its absolute path: ' + selected)
    return str(pathlib.Path(executable).resolve())


def create_certificate(root):
    certificate, key, config = root/'localhost.pem', root/'localhost.key', root/'certificate.cnf'
    config.write_text('[req]\ndistinguished_name=dn\nx509_extensions=ext\nprompt=no\n'
                      '[dn]\nCN=localhost\n[ext]\nsubjectAltName=IP:127.0.0.1\n'
                      'basicConstraints=critical,CA:TRUE\n'
                      'keyUsage=critical,keyCertSign,digitalSignature,keyEncipherment\n')
    subprocess.run([openssl_executable(), 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                    '-days', '1', '-config', str(config), '-keyout', str(key), '-out', str(certificate)],
                   check=True, capture_output=True, timeout=15)
    return certificate, key


@contextlib.contextmanager
def tls_server(root):
    certificate, key = create_certificate(root)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certificate, key)

    class SecureHandler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_POST(self):
            self.rfile.read(int(self.headers['Content-Length']))
            self.server.requests.append(self.path)
            data = (b'data: {"choices":[{"delta":{"content":"trusted TLS fixture"},"finish_reason":null}]}\n\n'
                    b'data: {"choices":[{"delta":{},"finish_reason":"stop"}]}\n\ndata: [DONE]\n\n')
            self.send_response(200)
            self.send_header('Content-Type', 'text/event-stream')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)

    class SecureServer(http.server.ThreadingHTTPServer):
        def process_request_thread(self, connection, address):
            connection.settimeout(2)
            wrapped = None
            try:
                wrapped = context.wrap_socket(connection, server_side=True)
                super().process_request_thread(wrapped, address)
            except (ssl.SSLError, OSError):
                connection.close()
            finally:
                if wrapped is not None:
                    wrapped.close()

    server = SecureServer(('127.0.0.1', 0), SecureHandler)
    server.certificate = certificate
    server.requests = []
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    primary = None
    try:
        yield server
    except BaseException as error:
        primary = error
        raise
    finally:
        try:
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)
            if thread.is_alive():
                raise RuntimeError('TLS fixture did not stop')
        except BaseException:
            if primary is None:
                raise
