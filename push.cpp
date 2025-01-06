// sample push side of an nng pipeline.
// - message based.
// - we listen rather than dial (backwards from example)
//
// Usage: push uri
/// where uri is what we listen on.
//
// Demo code so no checking for uri - will segfault without it.

//  We push messages like 'message n'  where n is an increasing
//  integer (incremented after each send).
//  
// To let consumer dial, there's a 2 second delay
// between listening and sending the first msg.
// WE'll send a total of MSG_COUNT msgs then exit
// Curious to see what happens to the pullers when that happens
// (hopefully an error).
//   Note there are conflicting statements in the nng docs
// about whether to use nng_msg - the mdbook doc says yes.
// or not - the ascidoc says no.
//
// I'm gonna use them.
//
#include <nng/nng.h>
#include <nng/protocol/pipeline0/push.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <unistd.h>


static const int MSG_COUNT(50);

// Check the status of an nng call and exit with message on failure
static void checkstat(int status,  const char* doing) {
    if (status) {
        std::cerr << doing << ": " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
}


int main(int argc, char**argv) {
    nng_socket s;
    const char* uri = argv[1];

    // open the push socket.

    checkstat(
        nng_push0_open(&s),
        "Failed to open push socket"
    );
    // listen for connections

    checkstat(
        nng_listen(s, uri, nullptr, 0),
        "Pusher coud not listen for socekt"
    );
    // wait for at least one client:

    sleep(2);

    // Shoot messages:

    for ( int seq =0; seq < MSG_COUNT; seq++) {
        // encode the message:

        std::stringstream strmessage;
        strmessage << "message number " << seq;
        std::string message = strmessage.str();

        //Encapsulate it in an nng_message

        nng_msg* pmsg;
        checkstat(
            nng_msg_alloc(&pmsg, message.size()+1),
            "Could not allocate a message struct"
        );
        checkstat(
            nng_msg_insert(pmsg, message.c_str(), message.size() + 1),
            "Could not append string to message"
        );

        // send it  - the sendmsg function takes ownerhisp of our
        // msg and frees it whent he send completes.

        checkstat(nng_sendmsg(s, pmsg, 0), "Could not send message");
    }

    // shoot the messages.

    exit(EXIT_SUCCESS);
}