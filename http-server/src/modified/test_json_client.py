#!/usr/bin/env python3

import socket
import json
import time

def send_command(sock, command):
    """Send a command and wait for response"""
    request = json.dumps(command)
    print(f"Sending: {request}")
    sock.sendall((request + '\n').encode('utf-8'))
    
    # Receive response
    response = sock.recv(1024).decode('utf-8')
    print(f"Received: {response}")
    return response

def test_json_protocol():
    # Connect to the server
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect(('localhost', 8080))
        print("Connected to server")
        
        # Test commands one by one
        commands = [
            {
                "id": "1",
                "command": "message"
            },
            {
                "id": "2",
                "command": "echo",
                "parameters": {
                    "text": "Hello, JSON World!"
                }
            },
            {
                "id": "3",
                "command": "pow",
                "parameters": {
                    "base": 2,
                    "exponent": 10
                }
            },
            {
                "id": "4",
                "command": "time"
            }
        ]
        
        # Send each command and wait for response
        for cmd in commands:
            response = send_command(sock, cmd)
            time.sleep(1)  # Wait a bit between commands
            
        # Send exit command
        exit_cmd = {
            "id": "5",
            "command": "exit"
        }
        send_command(sock, exit_cmd)
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()
        print("Connection closed")

if __name__ == "__main__":
    test_json_protocol()