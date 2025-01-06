// Example responder to a survey.
// The resonses we give are just our PID (textual).
// The Surveys are:
//   'ALL'  - I always respond.,
//   'EVEN' - I respond if my pid is even (pid % 2 == 0).
//   'ODD'  - I respond if my pid is odd (pid % 2 == 1).
//
// Usage:
//  respndent surveyor
//    surveyor is the URI of the surveying listener.
#include <nng/nng.h>
#include <nng/protocol/survey0/respond.h>   // We are the surveyer.

#include <iostream>
#include <stdlib.h>
#include <sstream>
#include <unistd.h>
#include <time.h>
#include <vector>
#include <stdint.h>
#include <string.h>

// Check the status of an nng call and exit with message on failure
static void 
checkstat(int status,  const char* doing) {
    if (status) {
        std::cerr << doing << ": " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
}

// Get a string from a socket:

static std::string
getmsg(nng_socket s) {
     nng_msg* pMsg;
     checkstat(
        nng_recvmsg(s, &pMsg, 0),
        "Failed to read a survey"
     );
     std::string result((const char*)(nng_msg_body(pMsg)));
     nng_msg_free(pMsg);

     return result;
}
// Send a message with our PID in it.
static void
reply(nng_socket s, pid_t pid) {
    nng_msg* pMsg;
    
    // Stringify the pid.
    
    std::stringstream strmsg;
    strmsg << pid;
    std::string reply(strmsg.str());
    auto len = reply.size() + 1;    // for null terminator.
    checkstat(
        nng_msg_alloc(&pMsg, len),
        "Failed to alloctae reply msg."
    );
    checkstat(
        nng_msg_insert(pMsg, reply.c_str(), len),
        "Failed to encpasulate reply"
    );

    checkstat(
        nng_sendmsg(s, pMsg, 0),
        "Failed to reply to the server"
    );

}
// entry point

int main(int argc, char** argv) {
    const char* uri = argv[1];
    nng_socket s;

    // Open the responder socket:

    checkstat(
        nng_respondent0_open(&s),
        "Failed to open a respondent socket"
    );
    // Dial the surveyor:

    checkstat(
        nng_dial(s, uri, nullptr, 0),
        "Failed to dial the surveyor"
    );

    pid_t me =  getpid();
    bool even = (me % 2) ==  0;
    while(1) {
        bool respond = false;
        auto survey = getmsg(s);


        // Determine if we should respond:

        if (survey == "ALL") respond = true;
        else if ((survey == "EVEN") && even) respond = true;
        else if ((survey == "ODD") && (!even)) respond = true;


        if (respond) {
            reply(s, me);
        }
    }

}