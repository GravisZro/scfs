MCFS is a component of [System X](https://github.com/GravisZro/SystemX)
# Magic Circle Filesystem
MCFS is a FUSE VFS that manages socket files for processes.  It's intended use is to provide a permissions based [IPC](https://en.wikipedia.org/wiki/Inter-process_communication) interface between service daemons and client programs.

# About The Name
Using the term "daemon" for a background process is a metaphor.  The spelling "daemon" is actually the British spelling of demon.  Therefore, the goal is to communicate with a demon which in the occult can be done using a "magic circle" and an incantation.  The metaphor fits well because an incantation is a request in a predetermined structure which is a good description of a [RPC](https://en.wikipedia.org/wiki/Remote_procedure_call).  Therefore, RPCs (incantations) can be generated using [Incanto](https://github.com/GravisZro/incanto).  However, the incantation is not enough on it's own, it needs to be recited in a specific location, a magic circle which in this case is a socket in a file system.

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
