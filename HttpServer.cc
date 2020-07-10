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

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

#include "./FileReader.h"
#include "./HttpConnection.h"
#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpServer.h"
#include "./libhw3/QueryProcessor.h"

using std::cerr;
using std::cout;
using std::endl;
using std::list;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;

namespace hw4 {
///////////////////////////////////////////////////////////////////////////////
// Constants, internal helper functions
///////////////////////////////////////////////////////////////////////////////
static const char *kThreegleStr =
  "<html><head><title>333gle</title></head>\n"
  "<body>\n"
  "<center style=\"font-size:500%;\">\n"
  "<span style=\"position:relative;bottom:-0.33em;color:orange;\">3</span>"
    "<span style=\"color:red;\">3</span>"
    "<span style=\"color:gold;\">3</span>"
    "<span style=\"color:blue;\">g</span>"
    "<span style=\"color:green;\">l</span>"
    "<span style=\"color:red;\">e</span>\n"
  "</center>\n"
  "<p>\n"
  "<div style=\"height:20px;\"></div>\n"
  "<center>\n"
  "<form action=\"/query\" method=\"get\">\n"
  "<input type=\"text\" size=30 name=\"terms\" />\n"
  "<input type=\"submit\" value=\"Search\" />\n"
  "</form>\n"
  "</center><p>\n";

// static
const int HttpServer::kNumThreads = 100;

// This is the function that threads are dispatched into
// in order to process new client connections.
void HttpServer_ThrFn(ThreadPool::Task *t);

// Given a request, produce a response.
HttpResponse ProcessRequest(const HttpRequest &req,
                            const string &basedir,
                            const list<string> *indices);

// Process a file request.
HttpResponse ProcessFileRequest(const string &uri,
                                const string &basedir);

// Process a query request.
HttpResponse ProcessQueryRequest(const string &uri,
                                 const list<string> *indices);


///////////////////////////////////////////////////////////////////////////////
// HttpServer
///////////////////////////////////////////////////////////////////////////////
bool HttpServer::Run(void) {
  // Create the server listening socket.
  int listen_fd;
  cout << "  creating and binding the listening socket..." << endl;
  if (!ss_.BindAndListen(AF_INET6, &listen_fd)) {
    cerr << endl << "Couldn't bind to the listening socket." << endl;
    return false;
  }

  // Spin, accepting connections and dispatching them.  Use a
  // threadpool to dispatch connections into their own thread.
  cout << "  accepting connections..." << endl << endl;
  ThreadPool tp(kNumThreads);
  while (1) {
    HttpServerTask *hst = new HttpServerTask(HttpServer_ThrFn);
    hst->basedir = staticfileDirpath_;
    hst->indices = &indices_;
    if (!ss_.Accept(&hst->client_fd,
                    &hst->caddr,
                    &hst->cport,
                    &hst->cdns,
                    &hst->saddr,
                    &hst->sdns)) {
      // The accept failed for some reason, so quit out of the server.
      // (Will happen when kill command is used to shut down the server.)
      break;
    }
    // The accept succeeded; dispatch it.
    tp.Dispatch(hst);
  }
  return true;
}

void HttpServer_ThrFn(ThreadPool::Task *t) {
  // Cast back our HttpServerTask structure with all of our new
  // client's information in it.
  unique_ptr<HttpServerTask> hst(static_cast<HttpServerTask *>(t));
  cout << "  client " << hst->cdns << ":" << hst->cport << " "
       << "(IP address " << hst->caddr << ")" << " connected." << endl;

  // Read in the next request, process it, write the response.

  // Use the HttpConnection class to read and process the next
  // request from our current client, then write out our response.  If
  // the client sends a "Connection: close\r\n" header, then shut down
  // the connection -- we're done.
  //
  // Hint: the client can make multiple requests on our single connection,
  // so we should keep the connection open between requests rather than
  // creating/destroying the same connection repeatedly.

  // STEP 1:
  bool done = false;
  HttpConnection conn(hst->client_fd);

  while (!done) {
    HttpRequest req;
    HttpResponse resp;

    // read the next request
    if (!conn.GetNextRequest(&req)) {
      close(hst->client_fd);
      done = true;
    }

    // process next request
    resp = ProcessRequest(req, hst->basedir, hst->indices);
    if (!conn.WriteResponse(resp)) {
      close(hst->client_fd);
      done = true;
    }

    // close connection when "Connection: close\r\n"
    if (req.GetHeaderValue("connection").compare("close") == 0) {
      close(hst->client_fd);
      done = true;
    }
  }
}

HttpResponse ProcessRequest(const HttpRequest &req,
                            const string &basedir,
                            const list<string> *indices) {
  // Is the user asking for a static file?
  if (req.uri().substr(0, 8) == "/static/") {
    return ProcessFileRequest(req.uri(), basedir);
  }

  // The user must be asking for a query.
  return ProcessQueryRequest(req.uri(), indices);
}

HttpResponse ProcessFileRequest(const string &uri,
                                const string &basedir) {
  // The response we'll build up.
  HttpResponse ret;

  // Steps to follow:
  //  - use the URLParser class to figure out what filename
  //    the user is asking for.
  //
  //  - use the FileReader class to read the file into memory
  //
  //  - copy the file content into the ret.body
  //
  //  - depending on the file name suffix, set the response
  //    Content-type header as appropriate, e.g.,:
  //      --> for ".html" or ".htm", set to "text/html"
  //      --> for ".jpeg" or ".jpg", set to "image/jpeg"
  //      --> for ".png", set to "image/png"
  //      etc.
  //
  // be sure to set the response code, protocol, and message
  // in the HttpResponse as well.
  string fname = "";

  // STEP 2:
  URLParser parser;
  parser.Parse(uri);
  // get the filename user is asking
  fname += parser.path().substr(8);

  // read file to memory
  FileReader freader(basedir, fname);
  string contents;

  // if succesfully read file,
  if (freader.ReadFile(&contents)) {
    ret.set_protocol("HTTP/1.1");
    ret.set_response_code(200);
    ret.set_message("OK");

    ret.AppendToBody(contents);

    string suffix = fname.substr(fname.find_last_of(".") + 1);

    // setting response content type
    if (suffix.compare("html") == 0 || suffix.compare("htm") == 0)
      ret.set_content_type("text/html");
    else if (suffix.compare("csv") == 0)
      ret.set_content_type("text/csv");
    else if (suffix.compare("css") == 0)
      ret.set_content_type("text/css");
    else if (suffix.compare("ics") == 0)
      ret.set_content_type("text/calendar");
    else if (suffix.compare("js") == 0)
      ret.set_content_type("text/javascript");
    else if (suffix.compare("txt") == 0)
      ret.set_content_type("text/plain");
    else if (suffix.compare("jpg") == 0 || suffix.compare("jpeg") == 0)
      ret.set_content_type("image/jpeg");
    else if (suffix.compare("gif") == 0)
      ret.set_content_type("image/gif");
    else if (suffix.compare("png") == 0)
      ret.set_content_type("image/png");
    else if (suffix.compare("tiff") == 0)
      ret.set_content_type("image/tiff");
    else if (suffix.compare("xml") == 0)
      ret.set_content_type("text/xml");
    else if (suffix.compare("svg") == 0)
      ret.set_content_type("image/svg+xml");
    else
      ret.set_content_type("application/octet-stream");

    return ret;
  }


  // If you couldn't find the file, return an HTTP 404 error.
  ret.set_protocol("HTTP/1.1");
  ret.set_response_code(404);
  ret.set_message("Not Found");
  ret.AppendToBody("<html><body>Couldn't find file \""
                   + EscapeHTML(fname)
                   + "\"</body></html>");
  return ret;
}

HttpResponse ProcessQueryRequest(const string &uri,
                                 const list<string> *indices) {
  // The response we're building up.
  HttpResponse ret;

  // Your job here is to figure out how to present the user with
  // the same query interface as our solution_binaries/http333d server.
  // A couple of notes:
  //
  //  - no matter what, you need to present the 333gle logo and the
  //    search box/button
  //
  //  - if the user had previously typed in a search query, you also
  //    need to display the search results.
  //
  //  - you'll want to use the URLParser to parse the uri and extract
  //    search terms from a typed-in search query.  convert them
  //    to lower case.
  //
  //  - you'll want to create and use a hw3::QueryProcessor to process
  //    the query against the search indices
  //
  //  - in your generated search results, see if you can figure out
  //    how to hyperlink results to the file contents, like we did
  //    in our solution_binaries/http333d.

  // STEP 3:
  // always present 333gle logo and search box/button
  ret.AppendToBody(string(kThreegleStr));

  // use URLParser to parse uri
  URLParser parser;
  parser.Parse(uri);
  // Get the users' query, convert to lower case
  string query = parser.args()["terms"];
  boost::to_lower(query);
  boost::trim(query);

  // Check if user has inputted a query previously.
  if (uri.find("query?terms=") != std::string::npos) {
    std::vector<std::string> query_list;
    boost::split(query_list, query,
                 boost::is_any_of(" "), boost::token_compress_on);

    // use HW3 QueryProcessor
    hw3::QueryProcessor qp(*indices, true);
    std::vector<hw3::QueryProcessor::QueryResult> results;
    results = qp.ProcessQuery(query_list);

    // If there are matches in the query, list them out
    if (results.size() != 0) {
      // prepare results
      ret.AppendToBody("<p><br>\n");

      if (results.size() == 1) {
        ret.AppendToBody("1 result found for <b>");
      } else {
        ret.AppendToBody(std::to_string(results.size()));
        ret.AppendToBody(" results found for <b>");
      }

      ret.AppendToBody(EscapeHTML(query));

      ret.AppendToBody("</b>\n");
      ret.AppendToBody("<p>\n\n");
      ret.AppendToBody("<ul>\n");
      // list out all the results
      for (auto q : results) {
        ret.AppendToBody(" <li> <a href=\"");
        if (q.documentName.substr(0, 7).compare("http://") != 0)
          ret.AppendToBody("/static/");
        ret.AppendToBody(q.documentName);
        ret.AppendToBody("\">");
        ret.AppendToBody(EscapeHTML(q.documentName));
        ret.AppendToBody("</a> [");
        ret.AppendToBody(std::to_string(q.rank));
        ret.AppendToBody("]<br>\n");
      }
      ret.AppendToBody("</ul>\n");
    } else {
      // No match
      ret.AppendToBody("<p><br>\nNo results found for ");
      ret.AppendToBody("<b>");
      ret.AppendToBody(EscapeHTML(query));
      ret.AppendToBody("</b>\n<p>\n\n");
    }
  }
  // close the body
  ret.AppendToBody("</body>\n</html>\n");

  ret.set_response_code(200);
  ret.set_message("OK");
  ret.set_protocol("HTTP/1.1");

  return ret;
}

}  // namespace hw4
