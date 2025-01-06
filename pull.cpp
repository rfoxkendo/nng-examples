//  This is a sample pull part of a pipeline for nng.
// Pipeline is really not a good name for this comunication pattern
// It's really a single producer, multi-consumer pattern.
// Usage:
//    pull from 
//
// Where from is the URI of the pusher.
//
// Note : this is demo code so no protection against from not supplied.


#include <nng/nng.h>
#include <nng/protocol/pipeline0/pull.h>
#include <unistd.h>          // for getpid to id myself.
#include <stdlib.h>
#include <iostream>


// Check the status of an nng call and exit with message on failure
static void checkstat(int status,  const char* doing) {
    if (status) {
        std::cerr << doing << ": " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
}


int main(int argc, char** argv) {
    auto uri = argv[1];                        // Better be there.
    int status;
    nng_socket s;

    // Make the socket:

    checkstat(
        nng_pull0_open(&s), 
        "Could not open the pull socket"
    );

    //  Note!!!!!!  The example in https://nanomsg.org/gettingstarted/nng/pipeline.html
    // has the puller listening and pusher dialing.  I think that's
    // totally backwards from what makes sense.  This example will
    // try to turn that around to something sensible so we dial:

    checkstat(
        nng_dial(s, uri, nullptr, 0),
        "Unable to dial the push program"
    );

    // This loop gets messages from the pusher and outputs them
    // prefixed by our pid so, with mulitple pullers we know
    // who got the message:

    while (true) {
        nng_msg* pmsg;
        checkstat(
            nng_recvmsg(s, &pmsg, 0), "Unable to receive mssage"
        );
        std::cerr << getpid() << " Received : " << (const char*)nng_msg_body(pmsg) << std::endl;
        nng_msg_free(pmsg);
    }
}
