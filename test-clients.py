import socket
import time
import threading

SERVER_IP = "127.0.0.1"
SERVER_PORT = 1234
NUM_CONNECTIONS = 10000  # Number of clients to keep open
REQUESTS_PER_CLIENT = 1  # Each client sends this many requests

def client_task(client_id):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((SERVER_IP, SERVER_PORT))

        for i in range(REQUESTS_PER_CLIENT):
            message = f"Client {client_id} message {i}"
            sock.sendall(len(message).to_bytes(4, 'little') + message.encode())

            # Receive a response
            response = sock.recv(1024)
            print(f"Client {client_id} received: {response.decode()}")

            time.sleep(0.1)  # Small delay before the next request

        sock.close()
    except Exception as e:
        print(f"Client {client_id} error: {e}")

# Spawn a fixed number of connections
threads = []
for i in range(NUM_CONNECTIONS):
    t = threading.Thread(target=client_task, args=(i,))
    threads.append(t)
    t.start()

# Wait for all clients to finish
for t in threads:
    t.join()

print("Test completed.")
