/**
 * This program  does timing of the bus pattern of communication. 
 * While bus supports all particpants broadcasting to all other 
 * participants, we'll just test the case of one participant (position 0)
 * sending messages to all other members of the bus.
 * 
 * Usage:
 *    bus basexport  nmsg size bussize
 * Where:
 *    baseexport is the specification from which the transport
 * endpoints will be constructed....see below.
 *    nmsg  - number of messages participant 0 sends
 *    size  - size in bytes of these messages
 *    bussize - number of participants on the bus. Must be at least 2.
 * 
 * Note as documented above, only participant 0 sends messages.
 * 
 * Because of the need to set up a fully connected mesh where everyone
 * listens and everyone has to dial to the subsequent bus members,
 * basexport is the base of some transport with a %d in it that
 * will be replaced with the position of the particpant on the bus
 * e.g.:  bus tcp://localhost:300%d 10000 1024 3
 * 
 * Will have
 *    Position     transport
 *      0          tcp://localhost:3000
 *      1          tcp://localhost:3001
 *      2          tcp://localhost:3002
 * 
 * Etc.  With TCP transport, best to keep the bus size less than 10.
 * 
 * Bus sends are not reliable so we need to figure out how to stop.  Furthermore,
 * std::thread does not hava tryjoin so we can't just send messages until 
 * threads exit.
 */

#include <thread>
#include <nng/nng.h>
#include <nng/protocol/bus0/bus.h>

#include <iostream>
#include <chrono>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <stdio.h>   // Simplest way to construct names.


/**
 * checkstat
 *    Check the status of an nng call and output a message/exit
 * if the result is not ok.
 * 
 * @param status - nng status returned from a call.
 * @param doing  - text that will describe what failed.
 * 
 */
static void 
checkstat(int status,  const char* doing) {
    if (status) {
        std::cerr << doing << ": " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
}


/**
 * constructEndpoints
 *  Utility to construct the vector of services that will be used by the
 *  bus.
 * 
 * @param base  - bases endpoint name.
 * @param size  - Size of the bus.
 * @return std::vector<std::string>   - vector containing endpoint names 
 * @note this will be in order of participant number.
 */
static std::vector<std::string>
constructEndpoints(const char* base, size_t size) {
    char endpoint[500];         // Should be big enough.
    std::vector<std::string> result;

    for (int i =0; i < size; i++) {
        snprintf(endpoint, sizeof(endpoint), base, i);
        result.push_back(std::string(endpoint));
    }
    return result;
}

/**
 * setupBus
 *    Sets up the bus interconnectivity:
 *    - Starts listening on my socket.
 *    - Waits for a couple of seconds for the bus to get populated.
 *    - Dials the appropriate partners.
 * 
 * @param uris - URIs of the bus participants.
 * @param me   - My bus position.
 * @param s - socket 
 */
static void
setupBus(std::vector<std::string> uris, int me, nng_socket s) {
    // Start listening:
    checkstat(
        nng_listen(s, uris[me].c_str(), nullptr, 0),
        "Bus member not able to listen"
    );
    // Wait for the bus to get populated;

    sleep(2);

    // Now dial those we need to dial:

    auto start = uris.begin() + me;
    start++;                 // Start with the next one 
    
    if (me == 0) {
        auto end = uris.end() - 1;
        
        for (auto p = start; p != end; p++) {
            
            checkstat(
                nng_dial(s, p->c_str(), nullptr, 0),
                "Unable to dial a bus member"
            );
        }
    } else if (start != uris.end()) {
        for (auto p = start; p != uris.end(); p++) {
            checkstat(
                nng_dial(s, p->c_str(), nullptr, 0),
                "Unable to dial a bus member."
            );
            
        }
    } else {
        checkstat (
            nng_dial(s, uris[0].c_str(), nullptr, 0),
            "Unable to dial bus member."
        );
    }
    

    // Ensure everyone else is dialed.

    sleep(3);
}

/**
 * receiver
 *    This function is a receiver (not bus 0 thread).
 *   It receives the specified number of messges then exits.
 * 
 * @param base - base URI.
 * @param me   - My position on the bus.
 * @param size - Size of bus.
 * @param nmsg - number of messages that will be received.
 * 
 * 
 */
static void
receiver(std::string base, size_t size, int me, size_t nmsg) {
    nng_socket s;
    void*  pMsg;
    size_t msgSize;
    std::vector<std::string> busUris = constructEndpoints(base.c_str(), size);

    // Open the bus socket and set myself up on the bus:

    checkstat(
        nng_bus0_open(&s),
        "Unable to open bus socket."
    );
    setupBus(busUris, me, s);

    // Recieve nmsg bus messages using zero copy 
    // then exit:

    for (int i = 0; i < nmsg; i++ ) {
        checkstat(
            nng_recv(s, &pMsg, &msgSize, NNG_FLAG_ALLOC),
            "Unable to receive a message from the bus."
        );
        nng_free(pMsg, msgSize);
    }

    checkstat(
        nng_close(s),
        "Failed to close receiver socket."
    );
    // Done.
}


/**
 * Program entry point.  This is not production quality code so
 * missing parmaeters will cause bus errors most likely.
 * 
 */
int main(int argc, char** argv) {
    // get the parameters:

    std::string baseUri(argv[1]);
    size_t nmsg = atoi(argv[2]);
    size_t msgSize = atoi(argv[3]);
    size_t busSize = atoi(argv[4]);
    nng_socket s;
    auto uris = constructEndpoints(baseUri.c_str(), busSize);

    checkstat(
        nng_bus0_open(&s),
        "Unable to open sender socket"
    );

    // we need to start the other threads (receivers) before we can
    // setup our bus access since 
    // setupBus blocks waiting for everyone else.

    std::vector<std::thread*> receivers;
    for (int i = 1; i < busSize; i++) {  // 1 since we (0) are not threads.
        receivers.push_back( new std::thread(receiver, baseUri, busSize, i, nmsg));
    }
    // Now we can do our part to complete the bus:

    setupBus(uris, 0, s);               // We are position 0 on the bus.

    // The bus should be ready, start spraying messages to the reeciever(s):

    uint8_t* message = new uint8_t[msgSize];

    auto start  = std::chrono::high_resolution_clock::now();
    for (int i =0; i < nmsg; i++ ) {
        checkstat(
            nng_send(s, message, msgSize, 0),
            "Failed to send message on the bus"
        );
    }
    // since there's no reliable delivery, we send 20% more at 1 byte each _sigh_

    auto extra = nmsg/20;
    for (int i = 0; i < extra; i++) {
        checkstat(
            nng_send(s, message, 1, 0),
            "Failed to send extra msg."
        );
    }

    // Done timing when all the threads exited -- so they got all the msgs

    for (auto p : receivers) {
        p->join();
    }
    auto end = std::chrono::high_resolution_clock::now();


    // Release resources:

    checkstat(
        nng_close(s),
        "Failed to close sender socket"
    );
    // Release storage
    for (auto p: receivers) {
        delete p;                  
    }
    receivers.clear();
    delete  []message;

    // Compute and report the timngs::


    auto duration = end - start;
    
    double timing = (double)std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()/1000.0;
    double msgTiming = (double)nmsg/timing;
    double xferTiming = (double)(nmsg * msgSize)/timing;

    std::cout << "Time:       " << timing << std::endl;
    std::cout << "msgs/sec:   " << msgTiming << std::endl;
    std::cout << "KB/sec:     " << xferTiming/1024.0 << std::endl;

    return EXIT_SUCCESS;
}