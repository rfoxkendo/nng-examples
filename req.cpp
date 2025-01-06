// sample nng REQ client
// Usage:  req uri request
//   WHen used with reply.cpp, request can be
//      DATE, PID or STOP anything else gets an error reply.
// We connect to the server at uri send our request and print the
// ASCII reply we get back.
//
// Test code so if either uri or request are missing probably we segfault

#include <nng/nng.h>
#include <nng/protocol/reqrep0/req.h>

#include <stdlib.h>
#include <string.h>
#include <iostream>

// too useful will need to be in a lib if we do this later.

// Check the status of an nng call and exit with message on failure
static void checkstat(int status,  const char* doing) {
    if (status) {
        std::cerr << doing << ": " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
}

//
// Make the request and get the reply.
//
static nng_msg*
request(nng_socket s, const char* request) {
    //
    // Format and send the request.
    nng_msg* r;
    size_t len = strlen(request) + 1;
    checkstat(
        nng_msg_alloc(&r, len),
        "Failed to allocate request message"
    );
    checkstat(
        nng_msg_insert(r, request, len),
        "Failed to encapsualte request"
    );
    checkstat(
        nng_sendmsg(s, r, 0),
        "Failed to make request "
    );
    // get the reply:

    checkstat(
        nng_recvmsg(s, &r, 0),
        "Failed to get server response."
    );

    return r;
}
//
int main(int argc, char** argv) {
    nng_socket s;
    const char* uri = argv[1];
    const char* req = argv[2];

    // make the req socket and dial the server.

    checkstat(
        nng_req0_open(&s),
        "Failed to  open the req0 socket"
    );
    checkstat(
        nng_dial(s, uri, nullptr, 0),
        "Failed to dial the server"
    );

    nng_msg* reply = request(s, req);
    std::cout << "Reply: " << (const char*)nng_msg_body(reply) << std::endl;
    nng_msg_free(reply);

    exit(EXIT_SUCCESS);
}