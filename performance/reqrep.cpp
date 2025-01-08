/**
 * This program does performance computations for nng REQ/REP
 * We only support 1:1 though theoretically, many clients could
 * dial in a replyer.
 * 
 * Usage, therefore is
 *     reqrep  uri nmsg msgsize
 * 
 * Where uri - is the URI on which the replier listens 
 *       nmsg - is the number of messages that will be sent.
 *       msgsize - is the size of the request
 * 
 *  Note:
 *   To minimize the impact on timing, we reply with a single
 *   byte of data but request msgsize bytes.
 * 
 *   We then repeat the timing with a 1 byte request and a msgsize
 * reply.  The timings for both are output
 * e.g.
 *     big request seconds   ....
 *     big request req/reps/sec ....
 *     big request KB req /sec ...
 *     big reply seconds ....
 *     big reply  req/reps/sec ...
 *     big reply KB reply/sec   ...
 * 
 *   Are those symmetric?
 * 
 * We teardown/setup the sockets between those tests
 * 
 * Main thread is the dialer/requestor we spin off a thread
 * to be the listener/replier.
 * 
 */
#include <thread>
#include <nng/nng.h>
#include <nng/protocol/reqrep0/req.h>
#include <nng/protocol/reqrep0/rep.h>

#include <iostream>
#include <chrono>
#include <stdlib.h>
#include <unistd.h>


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
 *  replier
 *    Accepts a fixed number of requests, replies to them and
 * exits.  We are the listener.
 * 
 * @param[in] uri - URI on which we listen
 * @param[in] nmsg - number of messages we reply to.
 * @param[in] repsize = size of the reply in bytes.
 * 
 * @note we allocate the reply at the begining and reuse it constantly.
 * 
 */
static void
replier(std::string uri, size_t nmsg, size_t repsize) {
    uint8_t* reply = new uint8_t[repsize];
    nng_socket s;

    checkstat(
        nng_rep0_open(&s),
        "Unable to open reply socket."
    );
    checkstat(
        nng_listen(s, uri.c_str(), nullptr, 0),
        "Reeplier unable to start listening."
    );

    // Process the messages.
    for (int i = 0; i < nmsg; i++) {
        void* request;
        size_t   reqsize;

        checkstat(
            nng_recv(s, &request, &reqsize, NNG_FLAG_ALLOC),
            "Could not receive a request"
        );
        nng_free(request, reqsize);

        // Reply:

        checkstat(
            nng_send(s, reply, repsize, 0),
            "Could not send a reply."
        );

    }

    delete []reply;

    // If we close right away our last reply may not be actually
    // sent-- and there's no flush so
    // - We sleep a bit here
    // - We do the join in the main thread after timing ends.

    sleep(1);
    nng_close(s);
}
/**
 * requestor 
 *    Sends nmsg requests of reqsize to the socket
 *    getting nmsg replies  returning.
 *    It is this function that is timed.
 * 
 * @param s - socket on which to send and recdieve (must have dialed
 *    the replier.
 * @param nmsg -  number of REQ/REP pairs.
 * @param reqsize - size of the request in bytes
 * 
 * @note - we pre-allocate the request once 
 */
static void
requestor(nng_socket s, size_t nmsg, size_t reqsize) {
    uint8_t* request = new uint8_t[reqsize];

    for (int i =0; i < nmsg ; i++) {
        void *reply;
        size_t  repsize;
        checkstat(
            nng_send(s, request, reqsize, 0),
            "Unable to make request"
        );
        checkstat(
            nng_recv(s, &reply, &repsize, NNG_FLAG_ALLOC),
            "Unable to receive a reply to our request"
        );
        nng_free(reply, repsize);
    }

    delete []request;
}

/**
 *  Entry point
 * @note - this is not production code so missing command line
 * parameters most likely result in a segfault.
 */
int main(int argc, char** argv) {
    // pull out the command line parameters:
    std::string uri(argv[1]);
    size_t nmsg = atol(argv[2]);
    size_t msgSize = atol(argv[3]);
    nng_socket s;
    char cr;

    std::cout << "To  start hit enter: ";
    std::cout.flush();
    cr = std::cin.get();

    // large request, small reply.

    // Setup the REQ/REP system:

    std::thread small_replier(replier, uri, nmsg, 1); // 1 byte replies
    sleep(1);    /// Give the thread time to listen

    checkstat(
        nng_req0_open(&s),
        "Could not make requester socket"
    );
    checkstat(
        nng_dial(s, uri.c_str(), nullptr, 0),
        "Could not dial the replier."
    );

    // THis part is timed

    auto rsmall_begin = std::chrono::high_resolution_clock::now();

    requestor(s, nmsg, msgSize);
    
    auto rsmall_end = std::chrono::high_resolution_clock::now();
    auto rsmall_duration = rsmall_end - rsmall_begin;
    double rsmall_sec = 
        (double)std::chrono::duration_cast<std::chrono::milliseconds>(rsmall_duration).count()/1000.0;
    double rsmall_msgrate = (double)nmsg/rsmall_sec;
    double rsmall_bps     = (double)(nmsg * msgSize)/rsmall_sec;

    // join the thread and close the socket.

    small_replier.join();
    nng_close(s);

    // small request, large reqply:

    std::thread big_replier(replier, uri, nmsg, msgSize); 
    sleep(1);      // give it a chance to listen:
    checkstat(
        nng_req0_open(&s),
        "Could not make requester socket"
    );
    checkstat(
        nng_dial(s, uri.c_str(), nullptr, 0),
        "Could not dial the replier."
    );

    // This part is timed:

    auto rbig_begin = std::chrono::high_resolution_clock::now();

    requestor(s,  nmsg, 1);    // Small request size.
    
    auto rbig_end = std::chrono::high_resolution_clock::now();
    auto rbig_duration = rbig_end - rbig_begin;
    double rbig_sec =
        (double)std::chrono::duration_cast<std::chrono::milliseconds>(rbig_duration).count()/1000.0;
    double rbig_msgrate = (double)(nmsg)/rbig_sec;
    double rbig_bps = (double)(nmsg * msgSize)/rbig_sec;

    // shutdown
    
    big_replier.join();
    nng_close(s);

    // report


    std::cout << "Large request timings\n";
    std::cout << "Elapsed sec:  " << rsmall_sec << std::endl;
    std::cout << "msgs/sec   :  " << rsmall_msgrate << std::endl;
    std::cout << "Req KB/sec :  " << rsmall_bps/1024.0 << std::endl;

    std::cout << "Large reply timings\n";
    std::cout << "Elapsed sec:  " << rbig_sec << std::endl;
    std::cout << "msgs/sec   :  " << rbig_msgrate << std::endl;
    std::cout << "Req KB/sec :  " << rbig_bps/1024.0 << std::endl;


}