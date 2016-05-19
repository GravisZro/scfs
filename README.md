# Circle Filesystem
Circlefs is a FUSE VFS that manages socket files for processes.  It's intended use is to provide a permissions based [IPC](https://en.wikipedia.org/wiki/Inter-process_communication) interface between daemons and client programs.

# Setup
Simply execute circlefs and specify the mount directory.

example:
> sudo circlefs /mc

Note: User must have permissions to the mount directory.

# How To Use
## Daemons
A daemon must create a socket file in a subdirectory matching the username used to execute the daemon.

example:
> sudo -u audio /usr/bin/audiomixerd

/usr/bin/audiomixerd may now create one or more socket files inside directory /mc/audio/

  Note: Directories are automatically created and destroyed.  Daemons can only create socket files.

## Clients
A client only needs to check and open if a specific socket for their daemon exists.
