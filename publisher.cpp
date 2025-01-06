// Example of an nng publisher.
// Every two seconds we publish the following:
// DATE - followed by the date and time string
// PID - Followed by our PID.
// NAME - followed by our argv[0]..
//
//  Subscribers can select what they want from these.
// Usage:
//    publiser uri

#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>

#include <iostream>
#include <stdlib.h>
#include <sstream>
#include <unistd.h>
#include <time.h>


// Check the status of an nng call and exit with message on failure
static void 
checkstat(int status,  const char* doing) {
    if (status) {
        std::cerr << doing << ": " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
}

// send a string to our socket.

static void
send(nng_socket s, const std::string& msg) {
    nng_msg* pMsg;
    auto l = msg.size() + 1;

    checkstat(
        nng_msg_alloc(&pMsg, l),
        "Failed to allocate message"
    );
    checkstat(
        nng_msg_insert(pMsg, msg.c_str(), l),
        "Failed to encapsulate message"
    );
    checkstat(
        nng_sendmsg(s, pMsg, 0),
        "Failed to send message"
    );


}

// Publish our messages.

static void 
publish(nng_socket s, const char* name) {
    // Name

    std::stringstream namemsg;
    namemsg << "NAME" << " " << name;
    std::string namestr(namemsg.str());
    send(s, namestr);

    // PID

    std::stringstream pidmsg;
    pidmsg <<  "PID" << " " << getpid();
    std::string pidstr(pidmsg.str());
    send(s, pidstr);

     //Timne.

     time_t t = time(nullptr);
     const char* time = ctime(&t);

     std::stringstream timemsg;
     timemsg << "TIME" << " " << time;
     std::string timestr(timemsg.str());
    send(s, timestr);
}


int main(int argc,char** argv) {
    nng_socket s;
    const char* uri = argv[1];
    const char* name = argv[0];

    // Open the socket and listen

    checkstat(
        nng_pub0_open(&s),
        "Failed to open pub socket."
    );
    checkstat(
        nng_listen(s, uri, nullptr, 0),
        "Failed to start listner"
    );

    // every second, publish

    while (1) {
        publish(s, name);
        sleep(2);
    }

}