/*
 * Copyright ©2020 Hal Perkins.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Spring Quarter 2020 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <list>

#include "./ServerSocket.h"
#include "./HttpServer.h"

using std::cerr;
using std::cout;
using std::endl;
using std::list;
using std::string;

// Print out program usage, and exit() with EXIT_FAILURE.
void Usage(char *progname);

// Parses the command-line arguments, invokes Usage() on failure.
// "port" is a return parameter to the port number to listen on,
// "path" is a return parameter to the directory containing
// our static files, and "indices" is a return parameter to a
// list of index filenames.  Ensures that the path is a readable
// directory, and the index filenames are readable, and if not,
// invokes Usage() to exit.
void GetPortAndPath(int argc,
                    char **argv,
                    uint16_t *port,
                    string *path,
                    list<string> *indices);

int main(int argc, char **argv) {
  // Print out welcome message.
  cout << "Welcome to http333d, the UW cse333 web server!" << endl;
  cout << "  Copyright 2012 Steven Gribble" << endl;
  cout << "  http://www.cs.washington.edu/homes/gribble" << endl;
  cout << endl;
  cout << "initializing:" << endl;
  cout << "  parsing port number and static files directory..." << endl;

  // Ignore the SIGPIPE signal, otherwise we'll crash out if a client
  // disconnects unexpectedly.
  signal(SIGPIPE, SIG_IGN);

  // Get the port number and list of index files.
  uint16_t portnum;
  string staticdir;
  list<string> indices;
  GetPortAndPath(argc, argv, &portnum, &staticdir, &indices);
  cout << "    port: " << portnum << endl;
  cout << "    path: " << staticdir << endl;

  // Run the server.
  hw4::HttpServer hs(portnum, staticdir, indices);
  if (!hs.Run()) {
    cerr << "  server failed to run!?" << endl;
  }

  cout << "server completed!  Exiting." << endl;
  return EXIT_SUCCESS;
}


void Usage(char *progname) {
  cerr << "Usage: " << progname << " port staticfiles_directory indices+";
  cerr << endl;
  exit(EXIT_FAILURE);
}

void GetPortAndPath(int argc,
                    char **argv,
                    uint16_t *port,
                    string *path,
                    list<string> *indices) {
  // Be sure to check a few things:
  //  (a) that you have a sane number of command line arguments
  //  (b) that the port number is reasonable
  //  (c) that "path" (i.e., argv[2]) is a readable directory
  //  (d) that you have at least one index, and that all indices
  //      are readable files.

  // STEP 1:

  // sanity check for command line arguments
  if (argc < 4) {
    Usage(argv[0]);
  }

  // sanity check for the port number
  *port = atoi(argv[1]);
  if (*port < 1024) {
    cerr << "port number is not reasonable (<1024)" << endl;
    Usage(argv[0]);
  }

  // sanity check for readable directory
  struct stat dirstat;
  if (stat(argv[2], &dirstat) != 0) {
    cerr << "stat error" << gai_strerror(errno) << endl;
    Usage(argv[0]);
  }

  if (!S_ISDIR(dirstat.st_mode)) {
    cerr << argv[2] << " is not a directory." << endl;
    Usage(argv[0]);
  }

  // passed the sanity check for valid and readable directory
  *path = string(argv[2]);

  // checking for requirement (d)
  for (int i = 3; i < argc; i++) {
    string idxfile(argv[i]);

    if (idxfile.length() < 4 ||
        idxfile.substr(idxfile.find_last_of(".") + 1).compare("idx") != 0) {
      cerr << idxfile << " is not a valid index file." << endl;
      continue;
    }
    struct stat file;
    if (stat(argv[i], &file) == -1) {
      cerr << idxfile << " is not readable." << endl;
      continue;
    }

    if (!S_ISREG(file.st_mode)) {
      cerr << idxfile << " is not a regular file." << endl;
      continue;
    }

    indices->push_back(argv[i]);
  }

  // no readable index file passed in
  if (indices->size() == 0) {
    cerr << "No index files were readable" << endl;
    Usage(argv[0]);
  }
}

