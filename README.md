# Magic Circle Filesystem
MCFS is a FUSE VFS that manages socket files for processes.  It's intended use is to provide a permissions based [IPC](https://en.wikipedia.org/wiki/Inter-process_communication) interface between daemons and agent programs.

# Setup
Simply execute mcfs and specify the mount directory.

example:
> sudo mcfs /mc

Note: User must have permissions to the mount directory.

# How To Use
## Daemons
A daemon must create a socket file in a subdirectory matching the username used to execute the daemon.  The username used must also be part of the "services" group.

example:
> sudo -u audio /usr/bin/audiomixerd

/usr/bin/audiomixerd may now create one or more socket files inside directory /mc/audio/

  Note: Directories are automatically created and destroyed.  Daemons can only create socket files.

## Agents
An agent only needs to try to open a specific socket for their daemon.
