# Mini Dropbox

Final project for LabReSiD (Laboratorio Reti e Sistemi Distribuiti:
Networking & Distributed Systems Lab).

A simple storage service, with a server running as daemon on a remote
machine which accepts multiple parallel connections.

## Architecture

### Protocol Stack

```text
+-----------------------------------+   +---------------------------+
|         Control Messages          |   |       Data Messages       |
|  (AUTH_*, DOWNLOAD_*, UPLOAD_*,   |   | (SEND_CHUNK, CHUNK_OK,    |
|           LIST_*, RM_*)           |   |       CHUNK_AGAIN)        |
+-----------------------------------+   +---------------------------+
+-------------------------------------------------------------------+
|                          Message Protocol                         |
+-------------------------------------------------------------------+
+-------------------------------------------------------------------+
|                              TCP/IP                               |
+-------------------------------------------------------------------+
```

### Message Structure

```text
|<---------- Message Header ---------->|
+------------+------------+------------+---------...---------+
|   Length   |    Type    |  Sent At   |        Data         |
+------------+------------+------------+---------...---------+
|<------------------------- Message -------------...-------->|
```

_Data_ here is the content of one of the types of messages, the one
specified by the _Type_ field, as specified above
in the [Protocol Stack](#protocol-stack) section.

## Features

### Client

- [ ] Authenticate via locally generated token (UUID) sent to server
    (not secure, use over `ssh` tunnel for encryption)
- [ ] Send files in 4KB chunks to make it scalable (configurable via macro)
- [ ] Receive files in 4KB chunks (configurable via macro)
- [ ] Per chunk checksum (possibly calculated in parallel to the
    sending of the token), if checksum doesn't match re request chunk
- [ ] Resume interrupted download
- [ ] Resume interrupted upload
- [ ] List remote files
- [ ] Delete remote files

### Server

- [ ] Support multiple parallel transfers
- [ ] FSM to handle states (i.e. REQUESTING,TRANSFERING,FINISHED)
- [ ] Thread pool, keeping threads inactive via condition variables
- [ ] Store each user's files in a token-named after their token
- [ ] Limited user storage space 10GB (same for everyone)
- [ ] Completely prevent path traversal using mount namespaces
        and pivot_root
