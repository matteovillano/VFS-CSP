# Secure File Sharing System

This document describes how to compile, run, and usage the Secure Client-Server File System project.

## 1. Compilation

To compile the project, use the provided `Makefile`. This will generate both the `server` and `client` executables.

```bash
make
```

To clean build artifacts:
```bash
make clean
```

The `Makefile` serves as the automation script for compiling the project.

## 2. Startup Instructions

### Starting the Server

The server must be started first. It requires **root privileges** to manage user permissions and creating jails (chroot). Note that all 3 arguments are mandatory.

**Usage:**
```bash
sudo ./server <root_directory> <ip_address> <port>
```

**Example:**
```bash
# Create a storage directory
mkdir -p /tmp/server_root

# Start server on localhost port 8080
sudo ./server /tmp/server_root 127.0.0.1 8080
```

*   `<root_directory>`: The absolute path to the directory where user files will be stored.
*   `<ip_address>`: The IP address to bind to (e.g., `127.0.0.1`).
*   `<port>`: The port number to listen on (e.g., `8080`).

**Expected Output:**
```
Shared memory initialized for concurrency control.
[PARENT] Server started on 127.0.0.1:8080
```

### Starting the Client

The client connects to the running server.

**Usage:**
```bash
./client <ip_address> <port>
```

**Example:**
```bash
./client 127.0.0.1 8080
```

**Expected Output:**
```
Connected to server at 127.0.0.1:8080
>
```

## 3. Implemented Commands

Once the client is connected, you can interact with the server. Note that you must `login` before performing file operations.

### Authentication

*   **Command**: `login <username>`
*   **Description**: Logs in as an existing user.
*   **Example**: `login user1`
*   **Expected Output**:
    *   Success: `> Welcome user1` (prompt updates)
    *   Failure: `err-User unknown`

### File Management

#### List Directory
*   **Command**: `list [path]`
*   **Description**: Lists the contents of the current directory or the specified path.
*   **Example**: `list` or `list docs`
*   **Expected Output**:
    ```
    file1.txt   Size: 123   Perms: 755
    folder      Size: 4096  Perms: 755
    ```

#### Create File/Directory
*   **Command**:
    *   File: `create <filename> <permissions>`
    *   Directory: `create -d <dirname> <permissions>`
*   **Description**: Creates a new empty file or directory. Permissions are in octal (e.g., 0755).
*   **Example**: `create notes.txt 0644` or `create -d photos 0755`
*   **Expected Output**:
    *   `ok-File notes.txt created successfully with permissions 644.`
    *   `ok-Directory photos created successfully with permissions 755.`

#### Delete File/Directory
*   **Command**: `delete <path>`
*   **Description**: Deletes the file or directory at the specified path.
*   **Example**: `delete notes.txt`
*   **Expected Output**:
    *   `ok-Deleted notes.txt.`

#### Change Directory
*   **Command**: `cd <path>`
*   **Description**: Changes the current working directory.
*   **Example**: `cd photos`
*   **Expected Output**:
    *   `ok-Directory changed successfully.`

#### Move/Rename
*   **Command**: `move <source> <destination>`
*   **Description**: Moves or renames a file or directory.
*   **Example**: `move notes.txt old_notes.txt`
*   **Expected Output**:
    *   `ok-Moved notes.txt to old_notes.txt.`

#### Change Permissions
*   **Command**: `chmod <path> <permissions>`
*   **Description**: Changes the permissions of a file or directory (octal).
*   **Example**: `chmod script.sh 0777`
*   **Expected Output**:
    *   `ok-Permissions for script.sh changed to 777.`

### File I/O

#### Read File
*   **Command**: `read [-offset=<num>] <path>`
*   **Description**: Prints the content of a file to the screen. Option to start reading from an offset.
*   **Example**: `read notes.txt` or `read -offset=10 notes.txt`
*   **Expected Output**: The file contents are displayed.

#### Write to File
*   **Command**: `write [-offset=<num>] <path>`
*   **Description**: writes text from standard input to a file. Overwrites by default unless offset is used. Terminates when you type `EOF`.
*   **Example**: `write newfile.txt`
*   **Expected Output**:
    ```
    ok-Waiting for data... (Type 'EOF' to finish)
    <User types content>
    EOF
    ```

### File Transfer (Upload/Download)

These commands support a `-b` flag for running in the background.

#### Upload
*   **Command**: `upload [-b] <local_path> <remote_path>`
*   **Description**: Uploads a file from your local machine to the server.
*   **Example**: `upload myimage.png images/myimage.png`
*   **Expected Output**:
    *   `Client: Uploading file myimage.png to server port 12345...`
    *   `ok-Upload successful.`

#### Download
*   **Command**: `download [-b] <client_path> <server_path>`
*   **Description**: Downloads a file from the server to your local machine.
*   **Example**: `download server_doc.txt local_doc.txt`
*   **Expected Output**:
    *   `Client: Downloading file local_doc.txt from server port 12345...`
    *   `Download successful.`

### User-to-User File Transfer

These commands allow users to transfer files safely between their own directories.

#### Transfer Request
*   **Command**: `transfer_request <path> <dest_user>`
*   **Description**: Initiates a request to send a file to another user.
*   **Example**: `transfer_request shared_doc.txt bob`
*   **Expected Output**:
    *   `Transfer request sent successfully`
    *   `Waiting for response... I'm blocking` (The client blocks until the receiver accepts or rejects)

#### Accept Transfer
*   **Command**: `accept <req_id> <destination_path>`
*   **Description**: Accepts a pending transfer request.
*   **Example**: `accept 1 received_doc.txt`
*   **Expected Output**:
    *   (On Sender's side): `Transfer request handled successfully`

#### Reject Transfer
*   **Command**: `reject <req_id>`
*   **Description**: Rejects a pending transfer request.
*   **Example**: `reject 1`
*   **Expected Output**:
    *   (On Sender's side): `Transfer request rejected`

## 4. Server Console Commands

The server terminal accepts administrative commands via standard input:

*   **Command**: `create_user <username> <permissions>`
    *   Creates a new OS user and home directory.
*   **Command**: `exit`
    *   Shuts down the server and cleans up child processes.
