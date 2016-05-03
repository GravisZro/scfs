
// PLATFORM SPECIFIC
#define FUSE_USE_VERSION 30
#include <fuse.h>

// POSIX
#include <sys/types.h>
#include <sys/stat.h> // mode info
#include <pwd.h> // user id
#include <grp.h> // group id
#include <unistd.h>
#include <time.h>

// STL
#include <system_error>
#include <map>
#include <set>
#include <string>
#include <sstream>
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
  struct file_entry_t
  {
    bool operator < (const file_entry_t& other) const // for locating files by name
      { return name < other.name; }

    std::string name;
    mode_t mode;
    pid_t pid;
  };

  std::map<uid_t, std::set<file_entry_t>> files;


  enum class Epath
  {
    root,
    directory,
    file,
  };

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


  int readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fileInfo)
  {
    (void)fileInfo;
    filler(buf, "." , nullptr, 0);
    filler(buf, "..", nullptr, 0);

    Epath type;
    struct stat statbuf;
    passwd* pw_ent;
    std::string filename;

    deconstruct_path(path, type, pw_ent, filename);

    switch(type)
    {
      case Epath::root: // root directory (fill with usernames in use)
      {
        for(const auto& dir : files)
          filler(buf, ::getpwuid(dir.first)->pw_name, nullptr, offset);
        break;
      }

      case Epath::directory: // list files in directory (based on username)
      {
        auto matches = files.find(pw_ent->pw_uid);
        if(matches == files.end()) // username has no files
        {
          posix::error(std::errc::no_such_file_or_directory);
          return 0 - errno;
        }

        for(const file_entry_t& entry : matches->second)
        {
          statbuf.st_mode = entry.mode;
          filler(buf, entry.name.c_str(), &statbuf, offset);
          std::cout << "name: " << entry.name << std::endl;
        }

        break;
      }

      case Epath::file: // there must have been a parsing error (impossible situation)
      {
        assert(false);
      }
    }

    return posix::success();
  }

  int mknod(const char* path, mode_t mode, dev_t rdev)
  //int create(const char* path, mode_t mode, struct fuse_file_info* fileInfo)
  {
//    (void)fileInfo;
    std::cout << "mode: " << std::oct << mode << std::dec << std::endl;
    if(!(mode & S_IFSOCK) || mode & (/*S_IXUSR | S_IXGRP | S_IXOTH |*/ S_ISUID | S_ISGID)) // if not a socket or execution flag is set
      return posix::error(std::errc::permission_denied);

    Epath type;
    passwd* pw_ent = nullptr;
    std::string filename;

    deconstruct_path(path, type, pw_ent, filename);

    switch(type)
    {
      case Epath::root: // root directory - cannot make root!
      case Epath::directory: // directory (based on username) - cannot make directory!
        return posix::error(std::errc::permission_denied);

      case Epath::file: // create a node file!
        fuse_context* ctx = fuse_get_context();
        auto& dir = files[pw_ent->pw_uid];
        auto entry = dir.find({ filename, 0, 0 });
        if(entry != dir.end())
          dir.erase(entry);
        dir.insert({ filename, mode, ctx->pid });
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
      case Epath::root:      // root directory
      case Epath::directory: // username (always exists)
        statbuf->st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IXGRP | S_IXOTH;
        return posix::success();

      case Epath::file:
        auto matches = files.find(pw_ent->pw_uid);
        if(matches == files.end()) // username not registered
        {
          posix::error(std::errc::no_such_file_or_directory);
          break;
        }

        for(const file_entry_t& entry : matches->second) // check every file
        {
          if(entry.name == filename)
          {
            statbuf->st_uid = pw_ent->pw_uid;
            statbuf->st_gid = pw_ent->pw_gid;
            statbuf->st_mode = entry.mode;
            return posix::success();
          }
        }

        posix::error(std::errc::no_such_file_or_directory); // no file matched
        break;
    }
    return 0 - errno;
  }

  int open(const char* path, struct fuse_file_info* fileInfo)
  {
//    if(strcmp(path, hello_path) != 0)
//      return posix::error(std::errc::no_such_file_or_directory);

    if((fileInfo->flags & O_ACCMODE) != O_RDONLY)
      return posix::error(std::errc::permission_denied);

    return posix::success();
  }


  int read(const char* path,
           char* buf,
           size_t size,
           off_t offset,
           struct fuse_file_info* fileInfo)
  {
    return posix::success();
    /*
    try
    {
      std::ostringstream stream;
      stream.rdbuf()->pubsetbuf(buf, size);

      fs_element_ptr fs_pos = find_child(path); // find_child() may throw
      if(fs_pos->mixer_element)
      {
        stream << fs_pos->mixer_element->string();
        size = stream.str().size();
      }
    }
    catch(errno_t err)
    {
      return err;
    }

    return (errno_t)size;
    */
  }

  int write(const char* path,
            const char* buf,
            size_t size,
            off_t offset,
            struct fuse_file_info* fileInfo)
  {
    return posix::success();
    //return posix::error(std::errc::permission_denied);
    /*
    try
    {
      fs_element_ptr fs_pos = find_child(path); // find_child() may throw
      if(fs_pos->mixer_element)
      {
        std::stringstream stream;
        snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_UNKNOWN;
        long value;

        stream << buf; // buffer input
        stream >> value; // extract numeric value

        if(stream) // couldnt read a number (either NaN or empty stream)
          throw(ERRNO_INVAL); // invalid, bail out!

        if(stream.peek() == '%') // value is a percentage
          value = (fs_pos->mixer_element->max - fs_pos->mixer_element->min) * value / 100;

        if(buf[0] == '+' || buf[0] == '-') // value is relative
          value += fs_pos->mixer_element->get_volume(channel);

        // todo, determine which channel(s) to modify

        fs_pos->mixer_element->set_volume(channel, value); // set value
      }
    }
    catch(errno_t err)
    {
      return err;
    }

    return (errno_t)size;
    */
  }

}


int main(int argc, char *argv[])
{
  static struct fuse_operations ops;
  ops.getdir    = nullptr;
  ops.utime     = nullptr;
//  ops.read      = circlefs::read;
//  ops.write     = circlefs::write;

//  ops.getxattr  = circlefs::getxattr;
//  ops.listxattr = circlefs::listxattr;

  ops.readdir   = circlefs::readdir;
//  ops.create    = circlefs::create;
  ops.mknod     = circlefs::mknod;
  ops.getattr   = circlefs::getattr;

  return fuse_main(argc, argv, &ops, nullptr);
}
