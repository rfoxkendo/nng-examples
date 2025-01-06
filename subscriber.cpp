// Subscriber for PUB/SUB pattern.
// Usage:
//   subscriber publisher-uri subscription
//
// Note use an empty string ("") to get everything.
// 

#include <nng/nng.h>
#include <nng/protocol/pubsub0/sub.h>

#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>


// Check the status of an nng call and exit with message on failure
static void 
checkstat(int status,  const char* doing) {
    if (status) {
        std::cerr << doing << ": " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char** argv) {
    nng_socket s;
    const char* uri = argv[1];
    const char* subscription = argv[2];


    // Dial up the server.
    checkstat(
        nng_sub0_open(&s),
        "Failed to open subscription socket"
    );
    checkstat(
        nng_dial(s, uri, nullptr, 0),
        "Failed to dial the publisher."
    );
    

    // set our subscription.

    checkstat(
        nng_setopt(s, NNG_OPT_SUB_SUBSCRIBE, subscription, strlen(subscription)),
        "Failed to set subscription string"
    );



    // get and print what we got:

    while (true) {
        nng_msg* pMsg;

        checkstat(
            nng_recvmsg(s, &pMsg, 0),
            "Failed to receive from publisher"
        );
        std::cout << getpid() << " Got " << (const char*)nng_msg_body(pMsg) << std::endl;

        nng_msg_free(pMsg);
    }
}


