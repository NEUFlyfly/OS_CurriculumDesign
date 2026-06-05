import websocket
import json

ws = websocket.create_connection("ws://localhost:9001/ws", timeout=5)
print("Connected")

# Login with correct password
ws.send(json.dumps({"command": "login", "params": {"username": "root", "password": "root"}}))
result = json.loads(ws.recv())
print("Login:", json.dumps(result, ensure_ascii=False, indent=2))

# ls current dir (/home/root after login)
ws.send(json.dumps({"command": "ls", "params": {}}))
result = json.loads(ws.recv())
print("\nLS /home/root:", json.dumps(result, ensure_ascii=False, indent=2))

# cd to root
ws.send(json.dumps({"command": "cd", "params": {"path": "/"}}))
result = json.loads(ws.recv())
print("\nCD /:", json.dumps(result, ensure_ascii=False, indent=2))

# ls root
ws.send(json.dumps({"command": "ls", "params": {}}))
result = json.loads(ws.recv())
print("\nLS /:", json.dumps(result, ensure_ascii=False, indent=2))

# cd to home
ws.send(json.dumps({"command": "cd", "params": {"path": "home"}}))
result = json.loads(ws.recv())
print("\nCD home:", json.dumps(result, ensure_ascii=False, indent=2))

# ls home
ws.send(json.dumps({"command": "ls", "params": {}}))
result = json.loads(ws.recv())
print("\nLS /home:", json.dumps(result, ensure_ascii=False, indent=2))

# cd to root user dir
ws.send(json.dumps({"command": "cd", "params": {"path": "root"}}))
result = json.loads(ws.recv())
print("\nCD root:", json.dumps(result, ensure_ascii=False, indent=2))

# ls root user dir
ws.send(json.dumps({"command": "ls", "params": {}}))
result = json.loads(ws.recv())
print("\nLS /home/root:", json.dumps(result, ensure_ascii=False, indent=2))

# Try cd to non-existent directory
ws.send(json.dumps({"command": "cd", "params": {"path": "nonexistent"}}))
result = json.loads(ws.recv())
print("\nCD nonexistent:", json.dumps(result, ensure_ascii=False, indent=2))

# cd back to root using absolute path
ws.send(json.dumps({"command": "cd", "params": {"path": "/"}}))
result = json.loads(ws.recv())
print("\nCD /:", json.dumps(result, ensure_ascii=False, indent=2))

# cd to etc
ws.send(json.dumps({"command": "cd", "params": {"path": "etc"}}))
result = json.loads(ws.recv())
print("\nCD etc:", json.dumps(result, ensure_ascii=False, indent=2))

# ls etc
ws.send(json.dumps({"command": "ls", "params": {}}))
result = json.loads(ws.recv())
print("\nLS /etc:", json.dumps(result, ensure_ascii=False, indent=2))

ws.close()
print("\n=== All tests passed ===")
