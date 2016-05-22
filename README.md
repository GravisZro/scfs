# Magic Circle Filesystem
MCFS is a FUSE VFS that manages socket files for processes.  It's intended use is to provide a permissions based [IPC](https://en.wikipedia.org/wiki/Inter-process_communication) interface between service daemons and client programs.

# Setup
Simply execute mcfs and specify the mount directory.

example:
> sudo mcfs /mc

Note: User must have permissions to the mount directory.

# How To Use
## Service Daemons
A service daemon must create a socket file in a subdirectory matching the username used to execute the daemon.
<br>Tentative requirement: The username used must be part of the "services" group.

example:
> sudo -u audio /usr/bin/audiomixerd

/usr/bin/audiomixerd may now create one or more socket files inside directory /mc/audio/
<br>Note: Directories are automatically created and destroyed.  Service daemons can only create socket files.

## Client Programs
An client program only needs to try to open a specific socket for their daemon.
