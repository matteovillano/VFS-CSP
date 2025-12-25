# Secure Client-Server File System

This project implements a robust, multi-process client-server application in C designed for secure file management on Linux. It features privilege separation, process isolation, and a custom concurrency control mechanism.

## Features

- **User Management**: Create and delete users with persistent storage.
- **Secure File Operations**: Create, list, read, write, move, delete, and change permissions of files.
- **File Transfer**: dedicated `upload` and `download` commands using separate ephemeral sockets for data transfer.
- **Security**: 
    - **Privilege Separation**: Server runs as root but drops privileges to specific users for operations.
    - **Jail**: Uses `chroot` to restrict user access to their specific directories.
    - **Group Consistency**: New users are assigned the administrator's group ID, avoiding creating files as `root`.
- **Concurrency**: 
    - Handles multiple clients simultaneously via `fork()`.
    - Implements a Reader-Writer lock system using shared memory and semaphores to coordinate file access across processes.

## Compilation

The project uses a standard `Makefile`.

```bash
# Build the server and client
make

# Clean build artifacts
make clean
```

## Usage

### Starting the Server
The server requires root privileges to handle user switching and chrooting.

```bash
sudo ./server <root_directory> [ip] [port]
```
- `<root_directory>`: Base directory where all user data will be stored.
- `[ip]`: Optional bind address (default: 127.0.0.1).
- `[port]`: Optional port (default: 8080).

### Starting the Client
The client connects to the server to send commands.

```bash
./client [ip] [port]
```

## Commands

Once connected, the following commands are available:

### User Management
- `create_user <username> <permissions>`: Create a new user (admin only).
- `delete_user <username>`: Delete a user and their data (admin only).
- `login <username>`: Login as a specific user.

### File Operations
- `create <filename> <permissions>`: Create a file.
- `create -d <dirname> <permissions>`: Create a directory.
- `delete <path>`: Delete a file or directory.
- `list [path]`: List contents of the current or specified directory.
- `cd <path>`: Change directory.
- `move <source> <dest>`: Move/Rename a file.
- `chmod <path> <permissions>`: Change file permissions (octal, e.g., 755).
- `read <path>`: Display file contents.
- `write <path>`: Write data to a file (terminate with `EOF\n`).

### File Transfer
- `upload <local_path> <remote_path>`: Upload a file to the server.
    - *Note*: Uploaded files are automatically set to `777` permissions.
- `download <remote_path> <local_path>`: Download a file from the server.

## Technical Implementation

This project relies heavily on Linux system programming concepts:

### 1. Architecture & Process Management
- **Forking Server**: The main server loop listens for connections and `fork()`s a new child process for each authenticated session (`src/server/server.c`, `src/server/child.c`).
- **I/O Multiplexing**: Uses `select()` to handle non-blocking I/O between the socket and pipes (`src/server/server_main.c`).
- **IPC**: Uses `pipe()` for communication between the parent server process (handling shared state/routing) and child session processes.
- **Lifecycle**: Uses `prctl(PR_SET_PDEATHSIG, SIGKILL)` to ensure child processes terminate if the parent server crashes or exits.

### 2. Security & Isolation
- **Chroot**: When a user logs in, the process calls `chroot()` to the user's home directory to prevent accessing the wider file system.
- **Privilege Dropping**: Uses `setuid()` and `setgid()` to switch the process identity from root to the specific target user.
- **Group Management**: Captures `SUDO_GID` at startup to ensure newly created users belong to the admin's group rather than root (`src/server/users.c`).

### 3. Concurrency Control
- **Shared Memory**: Uses `mmap()` (`src/server/concurrency.c`) to create a shared memory region accessible by all forked processes.
- **Synchronization**: Implements a **Reader-Writer lock** using POSIX semaphores (`sem_t`) in the shared memory region. This allows multiple concurrent readers but exclusive writers for file safety.

### 4. Networking
- **Protocol**: Custom text-based protocol over TCP.
- **Data Transfer**: `upload` and `download` use a **secondary ephemeral socket**.
    1.  Client requests transfer.
    2.  Server opens a new socket on port 0 (OS assigns random free port).
    3.  Server sends port number to client via control channel.
    4.  Client connects to the new port for raw data streaming.
    5.  Connection closes on completion.

## Project Structure

- `src/server/`: Server implementation (logic, users, ops, concurrency).
- `src/client/`: Client implementation (UI, input handling).
- `src/common/`: Shared utilities (networking, paths).
- `include/`: Header files.
