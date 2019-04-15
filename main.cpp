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

#include <put/specialized/osdetect.h>
#include <put/specialized/procstat.h>
#include <put/cxxutils/posix_helpers.h>


#if !defined(_XOPEN_SOURCE) || _XOPEN_SOURCE < 600 || \
  defined(__darwin__) || defined(BSD)
# define st_ctim st_ctimespec
# define st_mtim st_mtimespec
# define st_atim st_atimespec
#endif

namespace scfs
{
  inline int return_error(posix::errc val) noexcept
    { return 0 - int(val); }

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
    const char* dir_pos = posix::strchr(path, '/') + 1;
    const char* file_pos = posix::strchr(dir_pos, '/');
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

      pw_ent = posix::getpwnam(dir.c_str());
    }
  }

  void clean_set(std::set<file_entry_t>& dir_set) noexcept
  {
    process_state_t data;
    auto pos = dir_set.begin();
    while(pos != dir_set.end())
    {
      if(!::procstat(pos->pid, data))
        pos = dir_set.erase(pos);
      else
        ++pos;
    }
  }

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
            struct passwd* p = posix::getpwuid(pos->first);
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
          return return_error(posix::errc::no_such_file_or_directory);

        clean_set(pos->second);

        for(const file_entry_t& entry : pos->second)
          filler(buf, entry.name.c_str(), &entry.stat, offset);

        break;
      }

      case Epath::file: // there must have been a parsing error (impossible situation)
        assert(false);
    }

    return posix::success_response;
  }

  int mknod(const char* path, mode_t mode, dev_t rdev) noexcept
  {
    (void)rdev;
    if(!(mode & S_IFSOCK) || mode & (S_ISUID | S_ISGID)) // if not a socket or execution flag is set
      return return_error(posix::errc::permission_denied);

    Epath type;
    passwd* pw_ent = NULL;
    std::string filename;
    struct stat stat = {};
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);

    deconstruct_path(path, type, pw_ent, filename);

    if(fuse_get_context()->uid && // if NOT root AND
       pw_ent->pw_uid != fuse_get_context()->uid) // UID doesn't match
      return return_error(posix::errc::invalid_argument);

    switch(type)
    {
      case Epath::root: // root directory - cannot make root!
      case Epath::directory: // directory (based on username) - cannot make directory!
        return return_error(posix::errc::permission_denied);

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
    return posix::success_response;
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

        statbuf->st_ctim =
        statbuf->st_mtim =
        statbuf->st_atim = inittime;
        statbuf->st_mode = S_IFDIR | // set as directory
                           S_IRUSR | S_IRGRP | S_IROTH | // set readable to everyone
                           S_IXUSR | S_IXGRP | S_IXOTH; // set executable to everyone
        break;

      case Epath::directory: // username (exists if username exists)
        statbuf->st_mode = S_IFDIR | // set as directory
                           S_IRUSR | S_IRGRP | S_IROTH | // set readable to everyone
                           S_IXUSR | S_IXGRP | S_IXOTH; // set executable to everyone
        if(pw_ent == nullptr)
          return return_error(posix::errc::no_such_file_or_directory);
        break;

      case Epath::file:
        auto pos = files.find(pw_ent->pw_uid);
        if(pos == files.end()) // username not registered
          return return_error(posix::errc::no_such_file_or_directory);

        clean_set(pos->second);

        for(const file_entry_t& entry : pos->second) // check every file
        {
          if(entry.name == filename)
          {
            *statbuf = entry.stat;
            return posix::success_response;
          }
        }

        return return_error(posix::errc::no_such_file_or_directory); // no file matched
    }
    return posix::success_response;
  }
}

int main(int argc, char *argv[])
{
  static struct fuse_operations ops;

  clock_gettime(CLOCK_REALTIME, &scfs::inittime);

  ops.readdir = scfs::readdir;
  ops.mknod   = scfs::mknod;
  ops.getattr = scfs::getattr;

  return fuse_main(argc, argv, &ops, NULL);
}
