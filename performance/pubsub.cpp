/**
 *  This program does performance computations for a 
 * publish/subscribe communication pattern.
 * We're interersted in the mgs/sec, kb/sec and
 * those as a function of the number of subscribers.
 * 
 * Usage therefore is:
 *   pubusb uri  nmsg msgsize nsub
 * 
 * Where
 *    uri -is the uri to use for the publisher.
 *    nmsg - are the number of messages we will publish
 *    msgsize - are the size of those messages.
 *    nsub  - are the number of subscriber threads to spin off.
 * 
 * Each subscriber will do an all inclusive subscription which, I guess?
 * is the worst case.  It will read messages until it receives one with
 * a non zero first byte at which point it will exit (the non-zero first
 * byte will be used by the publisher to indicate the end of the
 * publication stream).
 * 
 * The publisher will create a message bufer which will, initially start
 * witha  zero in byte 0 - it will then publish nmsg-1 messages, change
 * the first byte to a 1 and send anohter mksg.
 * 
 * The threads will all be joined to to ensure they received all of the
 * publications timing will be computed from just before the first publication
 * to just after the last join.
 */

#include <thread>
#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>

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
 * subscriber
 *    This is the thread that will subscribe to the publisher and consume messages
 * Messages are consumed until the first byte of the recevied message is nonzero at which
 * point we return...and must be joined.
 * 
 * We start doing this sort of thing because when we get to push/pull e.g,
 * We consumers don't know how many messages they will get.
 * 
 * @param  uri[in] - Uri that we will dial into.
 *
 */
void
subscriber(std::string uri) {
    nng_socket s;
    void*     pmsg;
    size_t    rcvSize;
    bool      done(false);

    // set up the subscription:

    checkstat(
        nng_sub0_open(&s),
        "Subscriber not able to  open a socket."
    );
    checkstat(
        nng_dial(s, uri.c_str(), nullptr, 0),
        "Subscsriber could not dial into the publisher"
    );
    checkstat(
        nng_setopt(s, NNG_OPT_SUB_SUBSCRIBE, "", 0),
        "Subscsriber could not set subscription"
    );

    // Read to start receiving msgs

    while(! done) {
        checkstat(
            nng_recv(s, &pmsg, &rcvSize, NNG_FLAG_ALLOC),
            "Failed to receive subscription msg"
        );
        uint8_t* p = reinterpret_cast<uint8_t*>(pmsg);
        if (*p) done = true;                // Last msg?

        nng_free(pmsg, rcvSize);
    }

}

/**
 *  publisher
 *     Publish the data to the subscriber threads.
 * 
 *   @param s[in] - socket setup to publish
 *   @param nmsg[in] - number of messages to publish.
 *   @param size[in] - bytes in each msg.
 * 
 * @note the first byte of all but he last message is 0.
 * @note we allocate the message block only once.
 */
void
publisher(nng_socket s, size_t nmsg, size_t size) {
    // The messgae block:

    uint8_t* pMessage = new uint8_t[size];
    pMessage[0] = 0;                            // not the last.

    // Send all but the last msg.

    for (int i =0; i < size-1; i++) {
        checkstat(
            nng_send(s, pMessage, size, 0), 
            "Publisher, publishing a message"
        );
    }


    // set the end flag and send the last msg.

    pMessage[0] = 1;
    checkstat(
        nng_send(s, pMessage, size, 0),
        "Publishing last message"
    );

    // Delete the storage

    delete []pMessage;
}


// Entry point.

int main(int arg, char** argv) {
    nng_socket s;      // Publisher socket.
    std::vector<std::thread*>  subscribers; // so we can join all
    char cr;

    // get the program parameters.

    std::string uri(argv[1]);
    size_t nmsg = atol(argv[1]);
    size_t msgSize = atol(argv[2]);
    size_t nSubs = atol(argv[3]);

    // Set up the publication socket -- must be done before
    // we start the subscribers:

    checkstat(
        nng_pub0_open(&s),
        "Publisher could not open socket"
    );
    checkstat(
        nng_listen(s, uri.c_str(), nullptr, 0),
        "Publisher could not start listening"
    );

    // Start the subscribers:
    // Actually could probably just have threads in the
    // vector as they're documented to copy construct.
    //
    for (int i=0; i < nSubs; i++) {
        subscribers.push_back(new std::thread(subscriber, uri));
    }

    // start when ready:

    std::cout << "Enter to start timing: ";
    std::cout.flush();
    cr = std::cin.get();
    std::cout << "Let's go\n";

    auto start = std::chrono::high_resolution_clock::now();
    publisher(s, nmsg, msgSize);   // publish
    // join the subscribers

    for (auto p : subscribers) {
        p->join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = end - start;
    
    double timing = (double)std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()/1000.0;
    double msgTiming = (double)nmsg/timing;
    double xferTiming = (double)(nmsg * msgSize)/timing;

    std::cout << "Time:       " << timing << std::endl;
    std::cout << "msgs/sec:   " << msgTiming << std::endl;
    std::cout << "KB/sec:     " << xferTiming/1024.0 << std::endl;

    // free the thread resources just in case:

    for (auto p: subscribers) {
        delete p;
    }
    subscribers.clear();

    return EXIT_SUCCESS;

}