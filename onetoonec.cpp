// client of a pair - client is just the guy that dials.
// We send a message to the server expect it back again 
// and a second message as well.
//
// Usage:
//   onetoonec uri message
//
// Where uri is who we dial and message is what we send.
//

#include <nng/nng.h>
#include <nng/protocol/pair0/pair.h>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <string.h>

// too useful will need to be in a lib if we do this later.

// Check the status of an nng call and exit with message on failure
static void checkstat(int status,  const char* doing) {
    if (status) {
        std::cerr << doing << ": " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
}
// encapulate and send a string.
static void 
send(nng_socket s, const char* msg) {
    nng_msg* pmsg;
    int l = strlen(msg) + 1;     // strorage needed.
    checkstat(
        nng_msg_alloc(&pmsg, l),
        "Failed to allocate nng_msg"
    );
    checkstat(
        nng_msg_insert(pmsg, msg, l),
        "Failed to encapsulate string"
    );
    checkstat(
        nng_sendmsg(s, pmsg, 0),
        "Failed to send message"
    );
}

// get and print a message:

static void
recvAndPrint(nng_socket s) {
    nng_msg* pmsg;
    // receive:

    checkstat(
        nng_recvmsg(s, &pmsg, 0),
        "Failed to receive a message"
    );
    std::cout << "Got: " << (const char*)(nng_msg_body(pmsg)) << std::endl;

    nng_msg_free(pmsg);
}

// get and print two messages.
static void
recv(nng_socket s) {
    recvAndPrint(s);
    recvAndPrint(s);
}

int main(int argc, char** argv) {
    nng_socket s;
    const char* uri = argv[1];
    const char* msg = argv[2];

    // Make the socdket and dial the server.

    checkstat(
        nng_pair0_open(&s),
        "Failed to open pair."
    );
    checkstat(
        nng_dial(s, uri, nullptr, 0),
        "Failed to dial the server"
    );


    // send our message:

    send(s, msg);
    // handle the reply.
    recv(s);

    exit(EXIT_SUCCESS);

}