// sample 1:1 server.
// 1:1 server means we are the listener part of a pair pattern.
// we'll keep running.  Whenever we receive a message
// We'll echo it back and send our pid to the  client in
// a second messagte (pair allows either side to initiate).
//
// Only one client can connect at a time... we'll keep
// going because I'm curious about two things:
// 
// WHat happens when two try to connect.
// What happens when the original partner exits and aother one later dials.
//
// Usage:
//    oneotones uri
//   URI determines what we listen on. Test code so missing URI probably segfaults.
#include <nng/nng.h>
#include <nng/protocol/pair0/pair.h>

#include <string>
#include <sstream>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>


// too useful will need to be in a lib if we do this later.

// Check the status of an nng call and exit with message on failure
static void checkstat(int status,  const char* doing) {
    if (status) {
        std::cerr << doing << ": " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
}

// Service the socket;
//    Get a request.
//    Echo it.
//    Send a message with our pid to demo we can intiate.
//

static void
service(nng_socket s) {
    nng_msg* preq;

    // get the message:

    checkstat(
        nng_recvmsg(s, &preq, 0), 
        "Faield to get msg"
    );
    // echo back:

    checkstat(
        nng_sendmsg(s, preq, 0),
        "Failed to echo"
    );

    // preq is deleted by sendmsg.

    // Fill in our unsolicited msg.

    std::stringstream strmsg;
    strmsg << "My Pid, by the way is, " << getpid();
    std::string msg(strmsg.str());
    size_t len = msg.size() + 1;

    checkstat(
        nng_msg_alloc(&preq, len),
        "Unable to allocate message"
    );
    checkstat(
        nng_msg_insert(preq, msg.c_str(), len),
        "Failed to encapsualate message"
    );
    checkstat(
        nng_sendmsg(s, preq, 0),
        "Failed to send unsolicited msg."
    );
}

int main(int argc, char** argv) {
    nng_socket s;
    static const char* uri = argv[1];

    // make a socket and listen on uri:

    checkstat(
        nng_pair0_open(&s),
        "Failed to open the socket (pair0)"
    );
    checkstat(
        nng_listen(s, uri, nullptr, 0),
        "Failed to start listener "
    );

    while(1) {
        service(s);                  // Service our peer.
    }
}