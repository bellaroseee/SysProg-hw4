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

#include <stdint.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <map>
#include <string>
#include <vector>

#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpConnection.h"

using std::map;
using std::string;
using std::vector;

namespace hw4 {

static const char *kHeaderEnd = "\r\n\r\n";
static const int kHeaderEndLen = 4;

bool HttpConnection::GetNextRequest(HttpRequest *request) {
  // Use "WrappedRead" to read data into the buffer_
  // instance variable.  Keep reading data until either the
  // connection drops or you see a "\r\n\r\n" that demarcates
  // the end of the request header.
  //
  // Once you've seen the request header, use ParseRequest()
  // to parse the header into the *request argument.
  //
  // Very tricky part:  clients can send back-to-back requests
  // on the same socket.  So, you need to preserve everything
  // after the "\r\n\r\n" in buffer_ for the next time the
  // caller invokes GetNextRequest()!

  // STEP 1:
  size_t pos = buffer_.find(kHeaderEnd);
  if (pos == std::string::npos) {
    int byte_read;
    unsigned char buf[1024];
    while (true) {
      byte_read = WrappedRead(fd_, buf, 1024);
      if (byte_read == 0)
        // eof. no bytes read
        break;

      if (byte_read == -1)
        // fatal error
        return false;

      buffer_ += std::string(reinterpret_cast<char*>(buf), byte_read);

      pos = buffer_.find(kHeaderEnd);
      if (pos != std::string::npos)
        break;
    }
  }

  if (pos == std::string::npos)
    return false;

  std::string str_req = buffer_.substr(0, pos + kHeaderEndLen);

  *request = ParseRequest(str_req);
  buffer_ = buffer_.substr(pos + kHeaderEndLen);

  return true;  // You may want to change this.
}

bool HttpConnection::WriteResponse(const HttpResponse &response) {
  string str = response.GenerateResponseString();
  int res = WrappedWrite(fd_,
                         (unsigned char *) str.c_str(),
                         str.length());
  if (res != static_cast<int>(str.length()))
    return false;
  return true;
}

HttpRequest HttpConnection::ParseRequest(const string &request) {
  HttpRequest req("/");  // by default, get "/".

  // Split the request into lines.  Extract the URI from the first line
  // and store it in req.URI.  For each additional line beyond the
  // first, extract out the header name and value and store them in
  // req.headers_ (i.e., HttpRequest::AddHeader).  You should look
  // at HttpRequest.h for details about the HTTP header format that
  // you need to parse.
  //
  // You'll probably want to look up boost functions for (a) splitting
  // a string into lines on a "\r\n" delimiter, (b) trimming
  // whitespace from the end of a string, and (c) converting a string
  // to lowercase.

  // STEP 2:
  if (request.empty())
    return req;

  // split request into lines
  vector<std::string> lines;
  boost::iter_split(lines, request, boost::algorithm::first_finder("\r\n"));
  for (std::string str : lines)
    boost::trim(str);

  // URI from first line
  vector<std::string> tokens;
  boost::split(tokens, lines[0], boost::is_any_of(" "));
  req.set_uri(tokens[1]);

  // Headers
  for (uint i = 1; i < lines.size(); i++) {
    std::string currLine = lines[i];
    size_t index = currLine.find(":");
    if (index == std::string::npos) {
      continue;
    }
    boost::algorithm::to_lower(currLine);
    string key = currLine.substr(0, index);
    string value = currLine.substr(index + 2);
    req.AddHeader(key, value);
  }
  return req;
}

}  // namespace hw4
