# Redis Clone in C++

## ⚙️ Project Overview
This project is a **Redis-inspired in-memory key-value store**, implemented in **C++**. It features **basic data persistence, socket-based client-server communication, and command parsing**.

### 🚀 Goals
- Improve **socket programming** in C++.
- Strengthen **multithreading and concurrency** skills.
- Design an efficient **event-driven server**.
- Implement **core Redis-like commands** (`SET`, `GET`, `DEL`, etc.).

---

## ⚙️ Features
- ✅ **Client-server architecture** using **TCP sockets**  
- ✅ **Multi-client support** using `poll()` for efficient I/O  
- ✅ **Basic Redis-like commands** (`SET`, `GET`, `DEL`, `EXISTS`, etc.)  
- ✅ **Simple in-memory storage** with **hash maps**  

### 💚 Planned Features
- **Basic persistence (optional JSON/flat file storage)**  
- **Logging for debugging**  
- **LRU caching & eviction policies**
- **Pub/Sub messaging**
- **Persistence with Append-Only File (AOF)**
- **Transaction support (MULTI/EXEC/DISCARD)**
- ️**Full C implementation for better performance & learning**

---

## 📦 Installation & Setup

### ✨ Prerequisites
- **C++17 or later** (GCC/Clang/MSVC)
- **CMake** (optional for build automation)

### 🔄 Clone the Repo
```sh
git clone https://github.com/yourusername/redis-clone.git
cd redis-clone
```

### 💡 Compile & Run
```sh
g++ -std=c++17 -Wall -Wextra -o server server.cpp
./server
```

To run a **test client**:
```sh
g++ -std=c++17 -Wall -Wextra -o client client.cpp
./client
```

Alternatively (and a more recommended route), use **CMake**:
```sh
mkdir build && cd build
cmake ..
make
./server
```

<!-- ---

## 🖥️ Usage

### 👁️ Start the Server
```sh
./server
```
You should see:
```
Server started on port 6379...
```

### 🎯 Connect Using `netcat`
```sh
nc localhost 6379
```

#### ⚖️ Supported Commands
```sh
SET key value
GET key
DEL key
EXISTS key
```

Example:
```
> SET foo bar
OK
> GET foo
bar
> EXISTS foo
1
> DEL foo
OK
> GET foo
(nil)
```

---

## 📄 Code Structure
```
redis-clone/
│── src/
│   ├── server.cpp      # Main server logic
│   ├── client.cpp      # Test client
│   ├── database.cpp    # In-memory data storage
│   ├── commands.cpp    # Command processing
│── include/
│   ├── server.h        # Server header file
│   ├── database.h      # Data storage header
│── tests/
│   ├── test_commands.cpp
│   ├── test_sockets.cpp
│── README.md           # This file
│── CMakeLists.txt      # CMake build system
```

---

## 💪 Performance Considerations
### 🗒️ Current
- Uses `std::unordered_map` for fast **O(1) lookups**.
- Uses `poll()` for **non-blocking I/O**.
- Uses **one thread per client** (but planning event-driven model).

### 🚀 Future Optimizations
- Implement **epoll/kqueue** for better **scalability**.
- Use **a thread pool** instead of **per-client threads**.
- Optimize **serialization & persistence** with memory-mapped files.

---

## 🛠️ Plans for C Port
- **Replace C++ STL with C standard libraries (`std::unordered_map` → `hash table`)**.
- **Optimize memory usage** by avoiding unnecessary heap allocations.
- **Reduce dependencies** (remove C++ STL, use POSIX-only calls).
- **Improve event-driven model** using `epoll` (Linux) or `kqueue` (macOS).

---

## 💪 Contributing
1. **Fork the repo** 📌
2. **Create a new branch**: `git checkout -b feature-xyz`
3. **Commit changes**: `git commit -m "Add feature xyz"`
4. **Push & create a PR** 🎉

---

## 📝 License
This project is licensed under the **MIT License**.

---

## 📞 Contact
💬 **Have questions?** Open an issue or reach out!  
🌟 **GitHub**: [yourusername](https://github.com/yourusername)
```
 -->
