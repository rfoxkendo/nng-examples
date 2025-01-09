/**
 *  This program performs timings of the push/pull pattern.
 *   Usage:
 *      pushpull URI nmsgs size npullers
 * 
 *   Where:
 *     URI - is the URI used to communicate.
 *     nmsgs - is the number of messages that will be sent.
 *     size  - is the size of each message in bytes.
 *     npullers - is the number of pullers that will be spun off.
 * 
 *   Completing this is a bit tricky as messages are distributed
 *   to the pullers 'fairely' whatever that means (round robin is mentioned).
 *   The point is that we don't know how many messages each puller will get.
 * 
 *   We'll rely on reliable deliver to _someone_ and do somethingg very
 *   similar to what we did in pubsub:
 *    - Send nmsgs - npullers messages with 0 as the first byte.
 *    - Send npullers messages with 1 as the first byte.
 * 
 *    When a puller sees a message with a 1 in the first byte it will immediately
 *    close the socket and exit.
 * 
 *    We will time:
 *    The time required to send all messagse and join to all pullers.
 * 
 *    Fingers crossed that nnmg won't send blocks of messages to a puller with
 *    messages lost if the puller exits before doing an nng_recv on them...as then
 *    we'll hang on the join.
 * 
 * Note:
 *   nng calls push/pull a pipeline.
 * Note:
 *   Sinced nng is batching messages, it sometimes happens that
 * more than one end message is sent to the same puller.   When that
 * happens, program will never exit...because at least one puller won't
 * join.  We had two choices:
 * -  Timing ends prior to the join - but then we have no idea if any pullers
 * ever got all their messages.
 * - Acdept this and have the user run the program again with the same
 * params.  Often, not always that's sufficient.  Note that you can use
 * top to see when the communications end as the program will be
 * compute bound until then.
 */
#include <thread>
#include <nng/nng.h>
#include <nng/protocol/pipeline0/push.h>
#include <nng/protocol/pipeline0/pull.h>

#include <iostream>
#include <chrono>
#include <stdlib.h>
#include <unistd.h>
#include <vector>


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
 * puller
 *    This is the puller process.
 *   We 
 *     1. dial the pusher at the specified URI
 *     2. Receive messages until we get one with the first byte
 *       0.
 * @param uri[in] - THe URI the pusher is listening on .
 * @note we use zero-copy recvs so that we will need to free that recv buffers.
 * 
 */
static void
puller(std::string uri) {
    nng_socket s;
    void*      pMsg;
    size_t     rcvsize;

    // Dial up the pusher:

    checkstat(
        nng_pull0_open(&s),
        "Unable to open a pull socket."
    );
    checkstat(
        nng_dial(s, uri.c_str(), nullptr, 0),
        "Puller dial failed"
    );
    bool done = false;
    while (!done) {
        checkstat(
            nng_recv(s, &pMsg, &rcvsize, NNG_FLAG_ALLOC),
            "Pull of data failed.,"
        );

        uint8_t* p = reinterpret_cast<uint8_t*>(pMsg);
        if (*p) done = true;

        nng_free(pMsg, rcvsize);
    }
    nng_close(s);           // SHutdown the connection.
}

/**
 *  pusher
 *     Push the messages to the pullers; This is (mostly) what's timed.
 * 
 * @param s - socket on which to push  - must be listening.
 * @param nmsg - Number of messages.
 * @param msgSize - size of the messages
 * @param npullers - number of pullers used to determine how to stop.
 * 
 * @note An uncaught error is for nmsg < npullers.
 */
static void
pusher(nng_socket s, size_t nmsg, size_t msgSize, size_t npullers) {
    uint8_t* pMessage = new uint8_t[msgSize];
    pMessage[0] = 0;                     // Keep going.

    // Send the messages with the continue:

    
    for (int i = 0; i < nmsg; i++) {
        checkstat(
            nng_send(s, pMessage, msgSize, 0),
            "Failed to push a messages"
        );
    }
    /* now send the stops -- a few times to make sure everyone gets them.*/

    pMessage[0] = 1;

    // even so some pullers don't get the message due to
    // the distribution 

    for (int i =0; i < npullers*4; i++)  {
        checkstat(
            nng_send(s, pMessage, msgSize, 0),
            "Failed to push end message"
        );
    }


    delete []pMessage;

}
/**
 *  Entry point:
 *     - Set up the push listen.
 *     - Spin off the threads
 *     - Wait a bit for them to all be ready.
 *     - Start timing
 *     - Run the puller
 *     - join the threads
 *     - stop timing
 *     - report statistics.
 * 
 * @note
 *    This is not production code so segfaults will likely happen
 * if paramteers are missing.  See Usage at the start of the file.
 */
int main(int argc, char** argv) {
    // Get the parameters:

    std::string uri(argv[1]);
    const size_t nmsg(atoi(argv[2]));
    const size_t msgSize(atoi(argv[3]));
    const size_t npullers(atoi(argv[4]));
    nng_socket s;
    std::vector<std::thread*> pullers;

    /* Set up the listen: */

    checkstat(
        nng_push0_open(&s),
        "Unable to create push socket."
    );
    checkstat(
        nng_listen(s, uri.c_str(), nullptr, 0),
        "Unable to start pusher listening."
    );
    // Create the theards and wait for them to start:

    for (int i = 0; i < npullers; i++) {
        pullers.push_back(new std::thread(puller, uri));
    }
    std::cout << "Enter to start timing:";
    std::cout.flush();
    std::cin.get();
    std::cout <<  "Lets go\n";

    // By now everything shoulid be going.

    auto start = std::chrono::high_resolution_clock::now();
    pusher(s, nmsg, msgSize, npullers);

    // include the joins in the timings:

    for (auto p : pullers) {
        p->join();
    }
    auto end = std::chrono::high_resolution_clock::now();

    // Compute and report  timings:

    auto duration  = end - start;
    double timing = (double)std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()/1000.0;
    double msgTiming = (double)nmsg/timing;
    double xferTiming = (double)(nmsg * msgSize)/timing;


    std::cout << "Time:       " << timing << std::endl;
    std::cout << "msgs/sec:   " << msgTiming << std::endl;
    std::cout << "KB/sec:     " << xferTiming/1024.0 << std::endl;

    for (auto p : pullers) {
        delete p;
    }
    pullers.clear();
    nng_close(s);

    return EXIT_SUCCESS;
}