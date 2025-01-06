// Sample program for request/reply..reply side.
// This pattern is a classical client server.
// the client sends requests and the server replies to them.
// The client cannot send additional requests until it has
// read the reply.  This server does not attempt to handle
// more than one request at a time.
//
// usage:
//    reply uri
// if URI is not supplied we'll probably segfault.

// Requests supported:

//   STOP -- reply OK and stop.
//   DATE -- Reply OK <date string> and continue
//   PID  -- Reply with OK <our pid> and continue.
//   anything else - Reply with ERROR - unrecognized request. and continue.
//
#include <nng/nng.h>
#include <nng/protocol/reqrep0/rep.h>
#include <iostream>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>


// too useful will need to be in a lib if we do this later.

// Check the status of an nng call and exit with message on failure
static void checkstat(int status,  const char* doing) {
    if (status) {
        std::cerr << doing << ": " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
}
// encapsulate a reply string in an nng_msg and send it to the client

static void
send(nng_socket s, const std::string& msg) {
    nng_msg* pmsg;
    checkstat(
        nng_msg_alloc(&pmsg, msg.size() + 1),
        "Could not allocate reply message"
    );
    checkstat(
        nng_msg_insert(pmsg, msg.c_str(), msg.size()+1),
        "Failed to encapsualte reply message."
    );
    checkstat(
        nng_sendmsg(s, pmsg, 0),
        "Failed to send response."
    );
    // mng_sendmsg takes care of freeing pmsg.
}

// Process one request.
static void
processRequest(nng_socket s, std::string& req) {
    std::string reply;
    if (req == "STOP") {
        reply = "OK";
    } else if (req == "DATE") {
        time_t t = time(nullptr);
        char* dateTime = ctime(&t);
        std::stringstream message;
        message << "OK " << dateTime;
        reply  = message.str();
    } else if (req == "PID") {
        pid_t pid = getpid();
        std::stringstream message;
        message << "OK " << pid;
        reply = message.str();
    } else {
        reply = "ERROR - unrecognized request";
    }
    send(s, reply);

    if (req =="STOP")  {
        exit(EXIT_SUCCESS);
    }
}

int main(int argc, char** argv) {
    nng_socket s;
    const char* uri = argv[1];
    // open the socket and listen on our uri:
    checkstat(
        nng_rep0_open(&s),
        "Failed to open the reply socket."
    );
    // Listen on our URI:

    checkstat(
        nng_listen(s, uri, nullptr, 0), 
        "Failed to listen on URI"
    );

    // Process requests:

    while (1) {
        nng_msg* pmsg;
        checkstat(
            nng_recvmsg(s, &pmsg, 0),
            "Failed to recieve a request"
        );
        std::string request((const char*)nng_msg_body(pmsg));
        processRequest(s, request);

        nng_msg_free(pmsg);
    }


}


