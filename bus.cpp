
#include <nng/nng.h>
#include <nng/protocol/bus0/bus.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
// Sample bus program.
// -  Initializes nng
// -  Open a bus.
// -  Listens on the bus.
//  
// -  Receives messages from the bus until control-C e.g.

// This is based on 
//  https://nanomsg.org/gettingstarted/nng/bus.html
//
// But uses messages instead.
//
// Note the shape of the computation must be
// know in advance.  Each participant listens on one
// URI and dials all the others (I think).
// We will make a four item system and
// argv[1] will be a number 0, 3 indicating which one we listen on:

std::vector<const char*> uris {
    "tcp://localhost:3000", "tcp://localhost:3001", "tcp://localhost:3002", "tcp://localhost:3003"
};

/*
    Dial a URI in the bus.
*/
static void dial(nng_socket s, const char* uri) {
    int status;
    if (status = nng_dial(s, uri, nullptr, 0)) {
        std::cerr << "Failed to dial " << uri << " : " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
}
/*
    setup the bus -listen on our URI and dial all others:
*/

void setupBus(nng_socket s, int me) {
    // Now listen and then dial our URI:
    int status;

    // start the listener.

    std::cout << me << " listening on " << uris[me] << std::endl;
    if (status = nng_listen(s, uris[me], nullptr, 0)) {
        std::cout << "Unable to listen on " << uris[me] << " : " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
    sleep(2);                         // Wait for them all to get going.

    // Set up the mesh. With the exception of the first and, last 'guy' we dial
    // everyone after 'me' on the list of URIS.
    // The first guy does not dial the last guy
    // the last guy dials only the first guy.

    auto start = uris.begin() + me;
    start++;                 // Start with the next one 
    
    if (me == 0) {
        auto end = uris.end() - 1;
        std::cout << "First ending before " << *end << std::endl;
        for (auto p = start; p != end; p++) {
            std::cout << me << "(first) dialing " << *p << std::endl;
            dial(s, *p);
        }
    } else if (start != uris.end()) {
        for (auto p = start; p != uris.end(); p++) {
            std::cout << "me "  << " dialing " << *p << std::endl;
            dial(s, *p);
        }
    } else {
        std::cout << me << "(last)  dialing "  << uris[0] << std::endl;
        dial(s, uris[0]);
    }
    


    sleep(2);
}
/*
   send the bus a message prefixed with our pid:
*/

static void sendBusMsg(nng_socket s, int id, const char* msg) {
    
    nng_msg* pmsg;
    int status;
    
    // Construct the message:

    std::stringstream strmsg;
    strmsg << "ID: " << id << " " << msg;

    std::string msgString = strmsg.str();

    // Allocate the nng msg and fill its body with msgString

    if(status = nng_msg_alloc(&pmsg, msgString.size() + 1)) {
        std::cerr << "Failed to allocate message: " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
    if (status = nng_msg_insert(pmsg, msgString.c_str(), msgString.size()+1)) {
        std::cerr << "Failed to fill message body: " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << id << " sending message " << (char*)nng_msg_body(pmsg) << std::endl;
    std::cout << "Should be " << msgString.c_str()  << std::endl;
    // send the message and deallocate it:

    if(status = nng_sendmsg(s, pmsg, 0)) {
        std::cerr << "Failed to send the message: " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
    // Free the message is done by nng_sendmsg

    // nng_msg_free(pmsg);
        

}
static void receiveBusMsgs(int me, nng_socket s) {
    while (1) {
        nng_msg* pmsg;
        int status;
        
        // Recieve a message from the bus:

        if (status = nng_recvmsg(s, &pmsg, 0)) {
            std::cerr << "Failed to receive a message from the bus: " << nng_strerror(status) << std::endl;
            exit(EXIT_FAILURE);
        }

        // Extract and output the string.

        std::cout << "Got a message " << me << " : " << (char*)nng_msg_body(pmsg) << std::endl;

        // Free the message for next time around.

        nng_msg_free(pmsg);
    }
}

int main(int argc, char** argv) {
    nng_socket s;
    int status;

    int me = atoi(argv[1]);
    if (status = nng_bus0_open(&s))  {
        std::cerr << "Failed to opent he socket: " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }


    setupBus(s, me);

    sendBusMsg(s, me, "Joined the bus");
    sendBusMsg(s, me, "I'll send another");

    receiveBusMsgs(me, s);
    nng_fini();
}