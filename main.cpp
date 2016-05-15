#define FUSE_USE_VERSION 30
#include <fuse.h>

// POSIX
#include <sys/types.h>
#include <sys/stat.h> // mode info
#include <pwd.h> // user id
#include <grp.h> // group id
#include <unistd.h>
#include <time.h>
#include <limits.h> // PATH_MAX

// STL
#include <system_error>
#include <map>
#include <set>
#include <string>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <iostream>

namespace posix
{
  static const int success_response = 0;
  static const int error_response = -1;
  static int success(void) { errno = 0; return success_response; }
  static int error(std::errc err) { errno = *reinterpret_cast<int*>(&err); return error_response; }
}

namespace circlefs
{
  enum class Epath
  {
    root,
    directory,
    file,
  };

  struct file_entry_t
  {
    bool operator < (const file_entry_t& other) const // for locating files by name
      { return name < other.name; }

    std::string name;
    pid_t pid;
    struct stat stat;
  };

  std::map<uid_t, std::set<file_entry_t>> files;

  void deconstruct_path(const char* path, Epath& type, passwd*& pw_ent, std::string& filename)
  {
    const char* dir_pos = std::strchr(path, '/') + 1;
    const char* file_pos = std::strchr(dir_pos, '/');
    std::string dir;

    if(path[1] == '\0')
    {
      type = Epath::root;
      pw_ent = nullptr;
      filename.clear();
    }
    else
    {
      if(file_pos == nullptr)
      {
        dir = dir_pos;
        filename.clear();
        type = Epath::directory;
      }
      else
      {
        dir = std::string(dir_pos, file_pos - dir_pos);
        filename = file_pos + 1;
        type = Epath::file;
      }

      pw_ent = ::getpwnam(dir.c_str());
    }
  }

  void clean_set(std::set<file_entry_t>& dir_set)
  {
    char path[PATH_MAX + 1];
    struct stat info;

    auto pos = dir_set.begin();
    while(pos != dir_set.end())
    {
      // Linux only
      sprintf(path, "/proc/%d", pos->pid);
      if(stat( path, &info ) != posix::success_response || !(info.st_mode & S_IFDIR)) // if process
        pos = dir_set.erase(pos);
      else
        ++pos;
    }
  }

  int readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fileInfo)
  {
    (void)fileInfo;
    filler(buf, "." , nullptr, 0);
    filler(buf, "..", nullptr, 0);

    Epath type;
    passwd* pw_ent;
    std::string filename;

    deconstruct_path(path, type, pw_ent, filename);

    switch(type)
    {
      case Epath::root: // root directory (fill with usernames in use)
      {
        auto pos = files.begin();
        while(pos != files.end())
        {
          clean_set(pos->second);

          if(pos->second.empty())
            pos = files.erase(pos);
          else
          {
            filler(buf, ::getpwuid(pos->first)->pw_name, nullptr, offset);
            ++pos;
          }
        }
        break;
      }

      case Epath::directory: // list files in directory (based on username)
      {
        auto pos = files.find(pw_ent->pw_uid);
        if(pos == files.end()) // username has no files
        {
          posix::error(std::errc::no_such_file_or_directory);
          return 0 - errno;
        }

        clean_set(pos->second);

        for(const file_entry_t& entry : pos->second)
          filler(buf, entry.name.c_str(), &entry.stat, offset);

        break;
      }

      case Epath::file: // there must have been a parsing error (impossible situation)
        assert(false);
    }

    return posix::success();
  }

  int mknod(const char* path, mode_t mode, dev_t rdev)
  {
    (void)rdev;
    if(!(mode & S_IFSOCK) || mode & (S_ISUID | S_ISGID)) // if not a socket or execution flag is set
      return posix::error(std::errc::permission_denied);

    Epath type;
    passwd* pw_ent = nullptr;
    std::string filename;
    struct stat stat = {};
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);

    deconstruct_path(path, type, pw_ent, filename);

    switch(type)
    {
      case Epath::root: // root directory - cannot make root!
      case Epath::directory: // directory (based on username) - cannot make directory!
        return posix::error(std::errc::permission_denied);

      case Epath::file: // create a node file!
        fuse_context* ctx = fuse_get_context();
        auto& dir = files[pw_ent->pw_uid];
        auto entry = dir.find({ filename, 0, stat });
        if(entry != dir.end())
          dir.erase(entry);

        stat.st_uid = pw_ent->pw_uid;
        stat.st_gid = pw_ent->pw_gid;
        stat.st_mode = mode;
        stat.st_atim =
        stat.st_ctim =
        stat.st_mtim = time;

        dir.insert({ filename, ctx->pid, stat });
        break;
    }
    return posix::success();
  }

  int getattr(const char* path, struct stat* statbuf)
  {
    Epath type;
    passwd* pw_ent = nullptr;
    std::string filename;

    deconstruct_path(path, type, pw_ent, filename);

    switch(type)
    {
      case Epath::root:      // root directory (always exists)
        statbuf->st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
        posix::success();
        break;

      case Epath::directory: // username (exists if username exists)
        statbuf->st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
        if(pw_ent == nullptr)
          posix::error(std::errc::no_such_file_or_directory);
        else
          posix::success();
        break;

      case Epath::file:
        auto pos = files.find(pw_ent->pw_uid);
        if(pos == files.end()) // username not registered
        {
          posix::error(std::errc::no_such_file_or_directory);
          break;
        }

        clean_set(pos->second);

        for(const file_entry_t& entry : pos->second) // check every file
        {
          if(entry.name == filename)
          {
            *statbuf = entry.stat;
            posix::success();
            return 0 - errno;
          }
        }

        posix::error(std::errc::no_such_file_or_directory); // no file matched
        break;
    }
    return 0 - errno;
  }
}

int main(int argc, char *argv[])
{
  static struct fuse_operations ops;

  ops.readdir   = circlefs::readdir;
  ops.mknod     = circlefs::mknod;
  ops.getattr   = circlefs::getattr;

  return fuse_main(argc, argv, &ops, nullptr);
}
