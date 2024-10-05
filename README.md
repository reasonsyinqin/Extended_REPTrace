
## Running Architecture



This program operates in three main parts when tracking the execution path of requests in a cloud system:
1. **REPAgent**, which runs on each tracked component and intercepts system calls of cloud system components.
2. **Central Component Registry**, running on a designated node, providing registration, query, and deregistration functions for REPAgent.
3. **Central Generator**, also running on a designated node, responsible for generating the complete request execution path.

## Installation Guide

This program must be run on Linux OS, and has been verified on CentOS 7, CentOS 8, and CentOS 8 Stream.

### Code Files Description

- `centralRegisterServer.c`: Source code for the Central Component Registry.
- `tracer.c`/`tracer.h`: Source code for REPAgent.
- `event_linking.cpp`: Source code for the Central Generator.

### Running Environment

- **Central generator**: Runs in Python 2.7 and requires `treelib==1.6.1` for visualization.

### Configuration

Currently, configuration of this program is done by modifying the code directly. This document lists items that must be configured by the user, other configurations can use default settings.

#### In `tracer.c`:
- `const char *filePath`, `const char *debugFilePath`, `const char *dataFilePath`, `const char *errFilePath`: File paths that must be configured.

#### In `tracer.h`:
- `REG_SERVER_ADDR`: IP address of the Central Component Registry, must match the actual IP address of the host on which it runs.
- `REG_SERVER_PORT`: Port listened to by the Central Component Registry, must match the port configured in `centralRegisterServer.c`.

#### In `centralRegisterServer.c`:
- `SERVER_PORT`: The port listened to by the Central Component Registry, must match the port configured in `tracer.h`.



### Compilation and Execution

Compile `tracer.c` and `centralRegisterServer.c` using the following commands:

```bash
gcc -fPIC -shared -o hook.so tracer.c -ldl -luuid -lcurl -lpthread
gcc -o centralRegister centralRegisterServer.c
```

Python files do not require compilation.

#### Running the Central Component Registry
The Central Component Registry must be run before REPAgent can operate. Execute it directly on the designated node.

#### Running REPAgent
REPAgent uses the LD_PRELOAD mechanism of Linux/Unix systems. Set the `LD_PRELOAD` environment variable to the path where `hook.so` is compiled. Various methods for setting environment variables are summarized below:

- **For services**: Configure environment variables in service configuration files under `/usr/lib/systemd/system/`. For example:

```bash
[Unit]
Description=OpenStack Nova Compute Server
After=syslog.target network.target libvirtd.service

[Service]
Environment=LIBGUESTFS_ATTACH_METHOD=appliance
Type=notify
NotifyAccess=all
TimeoutStartSec=0
Restart=no
User=nova
ExecStart=/usr/bin/nova-compute

[Install]
WantedBy=multi-user.target
```

- **Using `export`**: Directly modify the value of the variable in the terminal.

- **Using `.bashrc` or `.bash_profile`**: Add the `export` command to these files in the user's home directory.

- **Using `/etc/bashrc` or `/etc/profile`**: Modify these system configuration files, which require administrator privileges.

#### Running the Central Request Path Generator

