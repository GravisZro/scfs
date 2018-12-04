#define FUSE_USE_VERSION 30
#include <fuse.h>

// POSIX
#include <sys/types.h>
#include <sys/stat.h> // mode info
#include <pwd.h> // user id
#include <grp.h> // group id
#include <unistd.h>

// POSIX++
#include <climits> // PATH_MAX
#include <cstdint>
#include <cassert>
#include <cstring>
#include <ctime>

// STL
#include <map>
#include <set>
#include <string>
#include <system_error>

namespace posix {
  constexpr int success = 0;
  constexpr int errorcode(std::errc err) { return 0 - int(err); }
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
    bool operator < (const file_entry_t& other) const noexcept // for locating files by name
      { return name < other.name; }

    std::string name;
    pid_t pid;
    struct stat stat;
  };

  static std::map<uid_t, std::set<file_entry_t>> files;
  static struct timespec inittime;

  void deconstruct_path(const char* path, Epath& type, passwd*& pw_ent, std::string& filename) noexcept
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
      if(file_pos == NULL)
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

  void clean_set(std::set<file_entry_t>& dir_set) noexcept
  {
    char path[PATH_MAX + 1];
    struct stat info;

    auto pos = dir_set.begin();
    while(pos != dir_set.end())
    {
//      ::procstat()
      // Linux only
      std::snprintf(path, PATH_MAX, "/proc/%d", pos->pid);
      if(stat( path, &info ) != posix::success || !(info.st_mode & S_IFDIR)) // if process
        pos = dir_set.erase(pos);
      else
        ++pos;
    }
  }
/*
  struct timespec get_oldest(std::set<file_entry_t>& dir_set)
  {
    struct timespec oldest = dir_set.begin()->stat.st_atim;
    auto pos = dir_set.begin();
    while(pos != dir_set.end())
    {
      if(pos->stat.st_atim.tv_sec < oldest.tv_sec)
        oldest.tv_sec = pos->stat.st_atim.tv_sec;
    }
    return oldest;
  }
*/
  int readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fileInfo) noexcept
  {
    (void)fileInfo;
    filler(buf, "." , NULL, 0);
    filler(buf, "..", NULL, 0);

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
            struct passwd* p = ::getpwuid(pos->first);
            if(p != NULL)
              filler(buf, p->pw_name, NULL, offset);
            ++pos;
          }
        }
        break;
      }

      case Epath::directory: // list files in directory (based on username)
      {
        auto pos = files.find(pw_ent->pw_uid);
        if(pos == files.end()) // username has no files
          return posix::errorcode(std::errc::no_such_file_or_directory);

        clean_set(pos->second);

        for(const file_entry_t& entry : pos->second)
          filler(buf, entry.name.c_str(), &entry.stat, offset);

        break;
      }

      case Epath::file: // there must have been a parsing error (impossible situation)
        assert(false);
    }

    return posix::success;
  }

  int mknod(const char* path, mode_t mode, dev_t rdev) noexcept
  {
    (void)rdev;
    if(!(mode & S_IFSOCK) || mode & (S_ISUID | S_ISGID)) // if not a socket or execution flag is set
      return posix::errorcode(std::errc::permission_denied);

    Epath type;
    passwd* pw_ent = NULL;
    std::string filename;
    struct stat stat = {};
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);

    deconstruct_path(path, type, pw_ent, filename);

    if(fuse_get_context()->uid && // if NOT root AND
       pw_ent->pw_uid != fuse_get_context()->uid) // UID doesn't match
      return posix::errorcode(std::errc::invalid_argument);

    switch(type)
    {
      case Epath::root: // root directory - cannot make root!
      case Epath::directory: // directory (based on username) - cannot make directory!
        return posix::errorcode(std::errc::permission_denied);

      case Epath::file: // create a node file!
        fuse_context* ctx = fuse_get_context();
        auto& dir = files[pw_ent->pw_uid];
        auto entry = dir.find({ filename, 0, stat });
        if(entry != dir.end())
          dir.erase(entry);

        stat.st_uid = pw_ent->pw_uid;
        stat.st_gid = pw_ent->pw_gid;
        stat.st_mode = mode;
        stat.st_ctim =
        stat.st_mtim =
        stat.st_atim = time;

        dir.insert({ filename, ctx->pid, stat });
        break;
    }
    return posix::success;
  }

  int getattr(const char* path, struct stat* statbuf) noexcept
  {
    Epath type;
    passwd* pw_ent = NULL;
    std::string filename;

    deconstruct_path(path, type, pw_ent, filename);
    statbuf->st_size = 0;

    switch(type)
    {
      case Epath::root:      // root directory (always exists)

        statbuf->st_atim =
        statbuf->st_ctim =
        statbuf->st_mtim = inittime;
        statbuf->st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
        break;

      case Epath::directory: // username (exists if username exists)
        statbuf->st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
        if(pw_ent == nullptr)
          return posix::errorcode(std::errc::no_such_file_or_directory);
        break;

      case Epath::file:
        auto pos = files.find(pw_ent->pw_uid);
        if(pos == files.end()) // username not registered
          return posix::errorcode(std::errc::no_such_file_or_directory);

        clean_set(pos->second);

        for(const file_entry_t& entry : pos->second) // check every file
        {
          if(entry.name == filename)
          {
            *statbuf = entry.stat;
            return posix::success;
          }
        }

        return posix::errorcode(std::errc::no_such_file_or_directory); // no file matched
    }
    return posix::success;
  }
}

int main(int argc, char *argv[])
{
  static struct fuse_operations ops;

  clock_gettime(CLOCK_REALTIME, &circlefs::inittime);

  ops.readdir = circlefs::readdir;
  ops.mknod   = circlefs::mknod;
  ops.getattr = circlefs::getattr;

  return fuse_main(argc, argv, &ops, NULL);
}
