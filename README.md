# Secure Virtual File System: Guide

Welcome! This document will guide you through setting up and using your new Secure Client-Server File System. We've designed this to be robust and secure, but also easy to use.

## 1. Getting Started

Before we can run anything, we need to build the project. We've included a `Makefile` to handle all the heavy lifting for you.

Just open your terminal in the project folder and type:

```bash
make
```

This command builds two executable files: `server` (the brain of the operation) and `client` (what you'll use to talk to the server).

If you ever want to start fresh and remove these built files, simply run:

```bash
make clean
```

## 2. Running the System

To get everything working, you'll need to start the server first, and then connect with the client.

### Step 1: Start the Server

The server does some heavy lifting (like managing users and permissions), so it needs to run with **root privileges**. We also need to tell it where to store everyone's files.

**How to run it:**

You need to provide three arguments:
1.  **Root Directory**: Where you want all the files to be saved. **Bonus:** If this folder doesn't exist yet, we'll create it for you automatically!
2.  **IP Address**: Usually `127.0.0.1` (localhost) if you're testing on your own machine.
3.  **Port**: A number like `8080`.

**Command:**
```bash
sudo ./server <root_directory> <ip_address> <port>
```

**Example:**
```bash
# Let's start the server on localhost port 8080 and save files in ./root
sudo ./server root 127.0.0.1 8080
```

**What you'll see:**
```
Shared memory initialized for concurrency control.
[PARENT] Server started on 127.0.0.1:8080
```

### Step 2: Start the Client

Now that the server is listening, open a **new terminal window** to start your client.

**Command:**
```bash
./client <ip_address> <port>
```

**Example:**
```bash
./client 127.0.0.1 8080
```

**What you'll see:**
```
Connected to server at 127.0.0.1:8080
>
```
You are now connected! The `>` prompt means the system is ready for your commands.

## 3. Using the System

Here is everything you can do once you're connected.

### Creating a User (Server Side)

Before you can log in, you must create a user from the server terminal (where you ran `sudo ./server`).

*   **Command**: `create_user <username> <permissions>`
*   **Example**: `create_user user1 0755`
*   **Expected Output**: `user created`

### Logging In
Before you can touch any files, you need to identify yourself.

*   **Command**: `login <username>`
*   **Example**: `login user1`
*   **Success**: You'll see `Server: Login successful`, and your prompt will update.

### Managing Your Files

Once logged in, you can manage files in your personal directory.

*   **See what's there**:
    *   Command: `list` or `list <foldername>`
    *   Shows you filenames, sizes, and permissions.
    *   Expected Output:
        ```
        Server: ok-
        ..      Size: 4096      Perms: 777
        .       Size: 4096      Perms: 755
        ```

*   **Create something new**:
    *   **File**: `create <filename> <permissions>` (e.g., `create notes.txt 0644`)
        *   Expected Output: `ok-File <filename> created successfully with permissions <permissions>.`
    *   **Folder**: `create -d <dirname> <permissions>` (e.g., `create -d photos 0755`)
        *   Expected Output: `ok-Directory <dirname> created successfully with permissions <permissions>.`
    *   *Note*: Permissions are in octal (like 0644 or 0755).

*   **Delete something**:
    *   Command: `delete <path>` (e.g., `delete notes.txt`)
    *   Expected Output: `ok-Deleted <path>`
    *   *Careful! This is permanent.*

*   **Move or Rename**:
    *   Command: `move <source> <destination>`
    *   Example: `move notes.txt old_notes.txt` (renames the file)
    *   Expected Output: `ok-Moved <source> to <destination>.`

*   **Change Permissions**:
    *   Command: `chmod <path> <permissions>`
    *   Example: `chmod script.sh 0777`
    *   Expected Output: `Server: ok-Permissions for <path> changed to <permissions>.`

*   **Change Directory**:
    *   Command: `cd <path>`
    *   Example: `cd photos`
    *   Expected Output: `Server: ok-Directory changed successfully.`

### Reading and Writing

*   **Read a file**:
    *   Command: `read <filename>`
    *   Want to start reading from the middle? Use `read -offset=10 <filename>`.
    *   Expected Output:
        ```
        Server: ok-
        Hello world!
        ```

*   **Write to a file**:
    *   Command: `write <filename>`
    *   Type your text, and when you're done, type `EOF` on a new line to save it.
    *   Expected Output: `Server: ok-Waiting for data... (Type 'EOF' to finish)`

### Uploading and Downloading

You can move files between your computer and the server. Both commands support a `-b` flag if you want them to run in the background while you do other things!

*   **Upload to Server**:
    *   Command: `upload <local_file> <server_path>`
    *   Example: `upload myphoto.jpg images/photo.jpg`
    *   Background mode: `upload -b myphoto.jpg images/photo.jpg`
    *   Expected Output: `Server: ok-Upload successful.`
    *   Expected Output (Background):
        ```
        Client: Uploading file <local_file> to server port <port>...
        Server: [Background] Command: upload <server_path> <local_file> concluded
        ```

*   **Download from Server**:
    *   Command: `download <server_file> <local_path>`
    *   Example: `download report.pdf ~/Downloads/report.pdf`
    *   Background mode: `download -b report.pdf ~/Downloads/report.pdf`
    *   Expected Output: `Download successful.`
    *   Expected Output (Background):
        ```
        Client: Downloading file to <local_path> from server port <port>...
        [Background] Command: download <server_file> <local_path> concluded
        ```

### Sharing with Friends (User-to-User)

Want to send a file directly to another user on the system? We've got you covered.

1.  **Send a Request**:
    *   Command: `transfer_request <your_file> <username>`
    *   Example: `transfer_request project.zip alice`
    *   *Status*: You'll wait until they respond.
    *   Expected Output:
        ```
        Waiting for response... I'm blocking
        Server: Transfer request handled successfully
        ```
    *   Expected Output (if rejected):
        ```
        Waiting for response... I'm blocking
        Server: err-Transfer rejected by user
        ```

2.  **Accept a Transfer** (If you are the receiver):
    *   Command: `accept <save_as_name> <request_id>`
    *   Example: `accept project.zip 1`

3.  **Reject a Transfer**:
    *   Command: `reject <request_id>`
    *   Example: `reject 1`

## 4. Server Administration

If you are looking at the server terminal (where you ran `sudo ./server`), you have a few admin superpowers:

*   **Create a new user**: `create_user <username> <permissions>`
    *   This sets up a new system user and their home folder.
*   **Shut it down**: `exit`
    *   Stops the server and cleans everything up.

## 5. Concurrency Handling

The system implements advanced concurrency mechanisms to ensure data integrity while allowing multiple users to access files simultaneously.

*   **Shared Memory**: The server uses shared memory (`mmap`) to maintain the state of file locks across all processes (parent and children).
*   **Reader-Writer Locks**: We use a custom implementation of Reader-Writer locks using semaphores.
    *   **Multiple Readers**: multiple users can read the same file at the same time without blocking each other.
    *   **Exclusive Writers**: When a user is writing to a file (or uploading), they get exclusive access. No one else can read or write to that file until they are done.
*   **Waiting**: If you try to access a file that is currently locked by someone else (e.g., trying to read a file while someone is writing to it), your command will wait automatically. You'll see a message like `waiting to read...` or `waiting to write...`, and the operation will proceed as soon as the file becomes available.

## 6. How File Transfers Work

Direct user-to-user file transfers are coordinated by the main server process:

1.  **Request**: When you send a transfer request, your client tells your server process.
2.  **Coordination**: Your server process sends a message through a pipe to the **Parent Server Process**.
3.  **Forwarding**: The Parent Server finds the recipient's server process and forwards the request.
4.  **Pending List**: The Parent Server keeps a list of all pending requests, matching `accept` or `reject` commands to the original sender.
5.  **Completion**: Once accepted, the server securely copies the file from the sender's folder to the recipient's folder, handling all permissions automatically.
