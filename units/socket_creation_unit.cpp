// STL
#include <memory>
#include <list>
#include <string>
#include <unordered_map>

// PUT
#include <put/socket.h>
#include <put/cxxutils/posix_helpers.h>
#include <put/cxxutils/vfifo.h>
#include <put/cxxutils/vterm.h>
#include <put/cxxutils/hashing.h>
#include <put/cxxutils/configmanip.h>
#include <put/specialized/fileevent.h>
#include <put/specialized/mountpoints.h>


#define UNITTEST_USERNAME  "unittest"
#define UNITTEST_GROUPNAME UNITTEST_USERNAME

#define UNIT_NAME "socket_creation_unit"

int main(int argc, char *argv[]) noexcept
{
  if(scfs_path == nullptr)
    reinitialize_paths();
  if(scfs_path == nullptr)
  {
    terminal::write("%s - %s: SCFS is not mounted\n", UNIT_NAME, "FAILURE");
    return EXIT_FAILURE;
  }

  if(posix::strcmp(posix::getgroupname(posix::getgid()), UNITTEST_GROUPNAME) && // if current groupname is NOT what we want AND
     !posix::setgid(posix::getgroupid(UNITTEST_GROUPNAME))) // unable to change group id
  {
    terminal::write("%s - %s: Unable to change group id to '%s'\n", UNIT_NAME, "FAILURE", UNITTEST_GROUPNAME);
    return EXIT_FAILURE;
  }

  if(posix::strcmp(posix::getusername(posix::getuid()), UNITTEST_USERNAME) && // if current username is NOT what we want AND
     !posix::setuid(posix::getuserid(UNITTEST_USERNAME))) // unable to change user id
  {
    terminal::write("%s - %s: Unable to change user id to '%s'\n", UNIT_NAME, "FAILURE", UNITTEST_USERNAME);
    return EXIT_FAILURE;
  }

//  Application app;
  ServerSocket server;

  std::string base = scfs_path;
  base.append("/" UNITTEST_USERNAME "/io");

  if(!server.bind(base.c_str()))
  {
    terminal::write("%s - %s: Unable to bind test provider to '%s'\n", UNIT_NAME, "FAILURE", base.c_str());
    return EXIT_FAILURE;
  }


  terminal::write("%s - %s\n", UNIT_NAME, "SUCCESS");
  return EXIT_SUCCESS;
//  return app.exec();
}

