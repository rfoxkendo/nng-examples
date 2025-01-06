// Example of a surveyor 
// Surveyors  commmunicate with an arbitrary number of responders.
//   A survery sets a survey time length  -- we'll use 2 seconds.
//   That is 2*1000 ms.
//  
//  We will send out alternating survey - every 5 seconds with messages :
//    ALL - everyone resonds - with their PID.
//    EVEN - only even pids respond with their PID
//    ODD  - Only odd pids respond with their PID.
//   
#include <nng/nng.h>
#include <nng/protocol/survey0/survey.h>   // We are the surveyer.

#include <iostream>
#include <stdlib.h>
#include <sstream>
#include <unistd.h>
#include <time.h>
#include <vector>
#include <stdint.h>
#include <string.h>

std::vector<const char*> surveys = {
    "ALL", "EVEN", "ODD"
};


static const nng_duration LIFETIME(2*1000);     // UNits of ms.

// Check the status of an nng call and exit with message on failure
static void 
checkstat(int status,  const char* doing) {
    if (status) {
        std::cerr << doing << ": " << nng_strerror(status) << std::endl;
        exit(EXIT_FAILURE);
    }
}
// Send a string to a socket.

static void
send(nng_socket s , const char* msg) {
    nng_msg *pMsg; 
    auto l = strlen(msg);

    checkstat(
        nng_msg_alloc(&pMsg, l),
        "Failed to allocate a message block."
    );
    checkstat(
        nng_msg_insert(pMsg, msg, l),
        "Failed to encapsulate a send message"
    );
    checkstat(
        nng_sendmsg(s, pMsg, 0),
        "Failed to send a survey message."
    );
}

static int
receiveAndPrint(nng_socket s) {
    nng_msg* pmsg;
    int status;

    status = nng_recvmsg(s, &pmsg, 0);
    if (status == 0) {
        std::cout << "survey response: " << (const char*)(nng_msg_body(pmsg)) << std::endl;

        nng_msg_free(pmsg);
    } 
    return status;
}

// Recieve and print a string from a socket:

// Survey - conduct a survey getting responses for the whole life of the survey:

static void
survey(nng_socket s, const char* survey) {
    std::cout << "Surveying " << survey << std::endl;
    send(s, survey);       // Send the survey msg.
    int stat;
    while ((stat = receiveAndPrint(s)) == 0 ) 
        ;
    if (stat != NNG_ETIMEDOUT ) {
        checkstat(stat, "Suvey response failure");
    }
    // timed out is a normal completion.
}

int main(int argc, char** argv) {
    nng_socket s;
    const char* uri = argv[1];

    // create the survey socket and listen on the URI

    checkstat(
        nng_surveyor0_open(&s),
        "Failed to create the surveyor socket"
    );
    checkstat(
        nng_listen(s, uri, nullptr, 0),
        "Failed to listen on the survey"
    );

    // Set the survey lifetime.

    checkstat(
        nng_setopt_ms(s, NNG_OPT_SURVEYOR_SURVEYTIME, LIFETIME),
        "Failed to set survey max response time."
    );

    // Survey and report responses.

    for (int i =0; true; i++) {    // kinda cool
        sleep(5);
        int index = i % surveys.size();    // choose the survey:

        survey(s, surveys[index]);
        std::cerr << "Survey done\n";
    }
}



