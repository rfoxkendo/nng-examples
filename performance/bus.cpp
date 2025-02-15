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
 *Bus sends don't deliver reliably.  Therefore we do this as follows:
 *The first uint32_t of each message is a sequence number.  
 * Furthermore, buses don't like to send messages when the bus is partially torn down
 * so we hit on the following scheme:
 * 
 * Each receiver  thread is associated with a future.
 * 
 * 1. The future is set when timing should be done for that thread
 * 2. After which the receiver loops on receiving a termination msg.
 *    which e.g. has a seq of 0xffffffff
 * 3. The main thread, after timing is done does another sleep to
 *   ensure the system is pretty idle and sends the termination
 *   message and, when the termination message is rceived,
 * 4.The receiver thread closes down and finishes.
 * 5.After sending the termination message, the main thread
 *   joins the receiver threads.
 * 
 * As long as the terminate message can be received by every receiver,
 * I think this will be reliable and I _think_ the sleep ensures that it
 * will be received since everyone should be in a condition to receiveit.
 * 
 *  It certainly acts reliable compared with all the other tries.
 *  
 * @todo In the future we can have the tasks count the number of missed messages,
 * collect them in the main thread and output the loss statistics as well as timings.
 * 
 
 */

#include <future>
#include <thread>
#include <functional>
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
 *   It receives messages until the sequence number is at least nmsg.
 *   We then loop reading messages until we see the terminate
 *   message which has as sequence of 0xffffffff
 * @param base - base URI.
 * @param me   - My position on the bus.
 * @param size - Size of bus.
 * @param nmsg - number of messages that will be received.
 * @param received - a promise that will be set when we've read a
 *     message that's got a sequence larger than nmsg.
 * 
 * @note the promise is set with the last sequence number received.
 */
static void
receiver(
    std::string base, size_t size, int me, size_t nmsg, 
    std::promise<int>* received
) {
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

    uint32_t  lastseq = 0;
    
    while (lastseq < nmsg) {
        checkstat(
            nng_recv(s, &pMsg, &msgSize, NNG_FLAG_ALLOC),
            "Unable to receive a message from the bus."
        );
        uint32_t* pSeq = reinterpret_cast<uint32_t*>(pMsg);
        lastseq = *pSeq;
        nng_free(pMsg, msgSize);
    }
    // set our promise:

    received->set_value(lastseq);
    std::cerr << "Member " << me << " Waiting for end\n";
    // Once all threads set the promise value, the
    // main thread will, eventually send a terminate:
    // This is done in a loop because other threads
    // may still be not done so we might get a non
    // terminate message.

    bool done = false;
    while (!done) {
        checkstat(nng_recv(s, &pMsg, &msgSize, NNG_FLAG_ALLOC),
            "Failed read for termination message."
        );
        uint32_t* pFlag = reinterpret_cast<uint32_t*>(pMsg);
        done = (*pFlag == 0xffffffff);
        nng_free(pMsg, msgSize);
    }
    std::cerr << "Member " << me << " sleeping then exiting\n";
    sleep(2);                   // CLose only when everyone's likely got it.
    checkstat(
        nng_close(s),
        "Failed to close receiver socket."
    );
    std::cerr << "Member " << me << " exiting\n";
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
    // For each receiver, we create a task, get and save its future in futures
    // and start the task in a thread which we also save in receivers.


    std::vector<std::thread*> receivers;
    std::vector<std::shared_future<int>> futures;
    for (int i = 1; i < busSize; i++) {  // 1 since we (0) are not threads.
        auto p = new std::promise<int>;
        futures.push_back(p->get_future());
        receivers.push_back(new std::thread(receiver, baseUri, busSize, i, nmsg, p));
    }
    // Now we can do our part to complete the bus:

    setupBus(uris, 0, s);               // We are position 0 on the bus.

    // The bus should be ready, start spraying messages to the reeciever(s):

    uint8_t* message = new uint8_t[msgSize];

    auto start  = std::chrono::high_resolution_clock::now();
    int done = 0;         // _count_ of the done tasks. 
    int seq = 0;
    while (done != futures.size()) {
        uint32_t* msgseq = reinterpret_cast<uint32_t*>(message);
        *msgseq = seq++;
        checkstat(
            nng_send(s, message, msgSize, 0),
            "Failed to send message on the bus"
        );
        // Count the done tasks.

        done = 0;
        for (auto p : futures) {
            auto zero = start-start;    // Zero.
            // Hopefully this is a non-blocking poll
            if (p.wait_for(zero) == std::future_status::ready) done++; 
        }
    }
    // We can end the timing here because everyone signalled they're done.

    auto end = std::chrono::high_resolution_clock::now();
    std::cerr << done << " timing done\n";

    // All futures being ready means all tasks/threads are done:
    // Done timing when all the threads exited -- so they got all the msgs

    for (auto p : futures) {
        p.get();                // Not actually sure I need to do this but...
    }
    // Now wait for everything to get idle and live in their receive for the
    // terminate msg

    sleep(2);                                          // Everyon ready for it.
    std::cerr << "Sending the terminate  msg\n";

    uint32_t* pflag = reinterpret_cast<uint32_t*>(message);
    *pflag = 0xffffffff;       // Terminate flag
    checkstat(
        nng_send(s, message, sizeof(uint32_t), 0),    // Just send the flag.
        "Failed to send terminate message\n"
    );
    std::cerr << "Joining threads\n";
    for (auto p : receivers) {
        p->join();
    }
    std::cerr << "Joined\n";
    
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
    double msgTiming = (double)seq/timing;
    double xferTiming = (double)(seq * msgSize)/timing;   /// Use actual message count.

    std::cout << "Time:       " << timing << std::endl;
    std::cout << "msgs/sec:   " << msgTiming << std::endl;
    std::cout << "KB/sec:     " << xferTiming/1024.0 << std::endl;

    return EXIT_SUCCESS;
}
