// Measure the performance of sending messages on a pair socket
// with nng.  Usage:
//    pair uri nmsg msgsize
//  
//      uri - the URI on which both sender and receiver connect.
//      nmsg - Number of messages.
//      msgsize - size in bytes of each message to be sent.
//
//  The way this, and all of our performance measures works is
// a reeiver thread is started and listens on the URI
// it then receives nmsg messagess of msgsize.
// and exits.
//
// Meanwhile the mainthread, dials the receiver and
// prompts for start of the measurement.
//
//  The time is gotten and nmsg msgsize messages are
//  sent to the receiver thread.
//
// When the last message is sent:
//    - The sender thread joins the receiver to ensure
//      the messages were all received before
//    - getting the time again.
//    - The following are computed and output:
//       * Total time to send the messages.
//       * message/second
//       * kbytes/sec.
//
// Then we exit.
//

#include <thread>
#include <nng/nng.h>
#include <nng/protocol/pair0/pair.h>

#include <iostream>
#include <chrono>
#include <stdlib.h>


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
 * The receiver thread:
 *    Make a pair0 socket.
 *    start a listener.
 *    receive the appropriate number of messages.
 *    return.
 * 
 * @param uri[in]  - The uri on which we are to listen.
 * @param nmsg[in] - number of messages to receive
 * 
 * @note we recieve with the NNG_FLAG_ALLOC flag so that
 *    zero copy is used on the assumption this removes a 
 *    single copy of the data at the cost of having to 
 *    free the message.
 */
void
receiver(std::string uri, size_t nmsg) {
    nng_socket s;
    void* pData;                      // Where data goes.
    size_t rcvSize;

    // Set up our side of the pair:

    checkstat(
        nng_pair0_open(&s),
        "Creating the receiver socket."
    );
    checkstat(
        nng_listen(s, uri.c_str(), nullptr, 0),
        "Receiver listening on socket."
    );
    // Receive the data:

    for (int i=0; i < nmsg; i++) {
        checkstat(
            nng_recv(s, &pData, &rcvSize, NNG_FLAG_ALLOC),
            "Receiver receiving a message"
        );
        nng_free(pData);                            // Release dynamic storage.
    }
    return;                                        // exit thread.
}

/**
 * The sender:   Given a socket sends the message and then returns.
 * 
 * @param[in] s - the socket on which to send.
 * @param[in] nmsg - The number of messages to send.
 * @param[in] size - the message size in bytes.
 * 
 * @note - we only allocate the message buffer once and it
 *        just has crap so we are timing the sends only.
 */
void
sender(nng_socket s, size_t nmsg, size_t size) {
    uint8_t pData = new uint8_t[size];               // Data buffer.

    for (int i = 0; i < nmsg; i++) {
        checkstat(
            nng_send(s, pData, size, 0),
            "Sender sending a message"
        );
    }
    delete []pData;
}

/**
 *  entry point.
 * @note test quality code so we don't check argc.
 */
int main(int argc, char** argv) {
    // Get our parameters.
    std::string uri(argv[1])
    size_t nmsg = atoul(argv[2]);
    size_t msgSize = atoul(argv[3]);
    std::string cr;

    nng_socket s;
    // start the receiver:

    std::thread thrReceiver(receiver, uri, nmsg);

    // Wait a bit for it to start/listen:

    sleep(1);

    // Set up the sender side of the pair:

    checkstat(
        nng_pair0_open(&s),
        "Sender failed to open the socket"
    );
    checkstat(
        nng_dial(s, uri.c_str(), nullptr, 0),
        "Sender failed to dial the receiver"
    );

    std::cout << "Hit entery to start tiing: ";
    std::cout.flush();
    std::cin >> cr;

    // start time:

    std::crono::high_resolution_clock clock;
    auto start = clock.now();
    sender(s, nmgs, msgSize);
    thReceiver.join();                  // Ensures the sender got them all.
    auto end = clock.now();
    auto duration = end - start;       // and std::duration.

    // Milliseconds is good enough I think

    std::milli ms(duration);
    // Turn that into fp seconds:

    double timing = (double)ms.count()/1000;   
    double msgTiming = (double)nmsg/timing;
    double xferTiming = (double)(nmsg * msgSize)/timing;

    std::cout << "Time:       " << timing << std::endl;
    std::cout << "msgs/sec:   " << msgTiming << std:endl;
    std::cout << "KB/sec:     " << xferTiming/1024.0 << std::endl;



}