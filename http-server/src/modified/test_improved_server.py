#!/usr/bin/env python3

import socket
import json

# Connect to the server
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 8080))

# Test valid JSON command
cmd = {"id": "1", "command": "message"}
sock.sendall((json.dumps(cmd) + '\n').encode())
response = sock.recv(1024)
print("Valid command response:", response.decode())

# Test invalid JSON command (missing parameters for echo)
cmd = {"id": "2", "command": "echo"}
sock.sendall((json.dumps(cmd) + '\n').encode())
response = sock.recv(1024)
print("Missing parameters response:", response.decode())

# Test invalid JSON command (wrong parameter type)
cmd = {"id": "3", "command": "echo", "parameters": {"text": 123}}
sock.sendall((json.dumps(cmd) + '\n').encode())
response = sock.recv(1024)
print("Wrong parameter type response:", response.decode())

# Test exit command
cmd = {"id": "4", "command": "exit"}
sock.sendall((json.dumps(cmd) + '\n').encode())
response = sock.recv(1024)
print("Exit response:", response.decode())

sock.close()