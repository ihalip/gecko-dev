#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
A fake ADB binary
"""

from __future__ import absolute_import

import os
import socket
import SocketServer
import sys
import thread

HOST = '127.0.0.1'
PORT = 5037

class ADBServer(SocketServer.BaseRequestHandler):
    def sendData(self, data):
        header = 'OKAY%04x' % len(data)
        all_data = header + data
        total_length = len(all_data)
        sent_length = 0
        # Make sure send all data to the client.
        # Though the data length here is pretty small but sometimes when the
        # client is on heavy load (e.g. MOZ_CHAOSMODE) we can't send the whole
        # data at once.
        while sent_length < total_length:
            sent = self.request.send(all_data[sent_length:])
            sent_length = sent_length + sent

    def handle(self):
        while True:
            data = self.request.recv(4096)
            if 'kill-server' in data:
                def shutdown(server):
                    server.shutdown()
                    thread.exit()
                thread.start_new_thread(shutdown, (server, ))
                self.request.close()
                break
            elif 'host:version' in data:
                self.sendData('001F')
                self.request.close()
                break
            elif 'host:track-devices' in data:
                self.sendData('1234567890\tdevice')
                break

if len(sys.argv) == 2:
    if sys.argv[1] == 'start-server':
        # daemonize
        if os.fork() > 0:
            sys.exit(0)
        os.setsid()
        if os.fork() > 0:
            sys.exit(0)

        # Create a SocketServer with 'False' for bind_and_activate to set
        # allow_reuse_address before binding.
        server = SocketServer.TCPServer((HOST, PORT), ADBServer, False)
        server.allow_reuse_address = True
        server.server_bind()
        server.server_activate()
        server.serve_forever()
    elif sys.argv[1] == 'kill-server':
        sock = socket.socket()
        sock.connect((HOST, PORT))
        sock.send('kill-server')
        sock.shutdown(socket.SHUT_RDWR)
        sock.close()
