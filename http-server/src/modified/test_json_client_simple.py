#!/usr/bin/env python3

import socket
import json

# Connect to the server
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 8080))

# Send message command
cmd = {"id": "1", "command": "message"}
sock.sendall((json.dumps(cmd) + '\n').encode())
response = sock.recv(1024)
print("Response:", response.decode())

# Send echo command
cmd = {"id": "2", "command": "echo", "parameters": {"text": "Hello JSON"}}
sock.sendall((json.dumps(cmd) + '\n').encode())
response = sock.recv(1024)
print("Response:", response.decode())

# Send pow command
cmd = {"id": "3", "command": "pow", "parameters": {"base": 2, "exponent": 8}}
sock.sendall((json.dumps(cmd) + '\n').encode())
response = sock.recv(1024)
print("Response:", response.decode())

# Send time command
cmd = {"id": "4", "command": "time"}
sock.sendall((json.dumps(cmd) + '\n').encode())
response = sock.recv(1024)
print("Response:", response.decode())

# Send exit command
cmd = {"id": "5", "command": "exit"}
sock.sendall((json.dumps(cmd) + '\n').encode())
response = sock.recv(1024)
print("Response:", response.decode())

sock.close()