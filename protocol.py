#!/usr/bin/python3
import json
import socket

SOCKET = '/var/run/nbfc_service.socket'

def communicate(socket_file, data):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client_socket:
        client_socket.connect(socket_file)

        if isinstance(data, str):
            pass
        else:
            data = json.dumps(data)

        message = "%s\nEND" % data
        client_socket.sendall(message.encode('utf-8'))

        response = b''
        while True:
            data = client_socket.recv(1024)
            response += data
            if b'\nEND' in response:
                break
        
        response = response.decode('utf-8')
        response = response.replace('\nEND', '')
        response = json.loads(response)
        return response

