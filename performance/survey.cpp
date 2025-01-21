/**
 *  This program times the survey responder.  Since a new survey is
 * initiated by a new output to the servey, we can survey as fast as possible.
 * In this timing all surveyed threads will respond to the survey so we don't
 * need to worry about a timout.
 *   Both the cases of a large survey and small (1 bytes) responses 
 * and smalle (1 byte) survey and large repsonses are measured.
 * Note that time througputs measured for the latter case will take into
 * account the number of respondents  e.g. 1 survey/sec where 10 respondents
 * responde with 1Mbyte will be output as 10MB/sec not 1MB/sec.
 * 
 * Usage:
 *    survey uri nreq size nresp
 * 
 * Where
 *    uri - uri on which the survey will listen for responses.
 *    nreq - Number of survey requests that will be sent out.
 *    size - Size of the large survey or large response depending on
 *           the phase of the measurement.
 *    nresp - Number of respondents.
 * 
 * In this case; surveys are broadcast out to the responders so we can
 * use the number of surveys to determine when to exit.
 * In order to not have to worry about unflushed data on socket close,
 * The surveyor will actually send one more survey after receiving the
 * 'last' response (small survey) and that will be the signal to the
 * respondents to close their socket and exit. This ensures the surveyor
 * got the last response before the respondent exits.
 * Otherwise the surveyor might need to wait in a timeout for the last
 * response read.
 *   Note that for this last send:
 * No response is read but the join with responders is done after the survey.
 * 
 * 
 * @note this is not production quality code so missing parameters probably
 * cause segfaults.
 */
#include <thread>
#include <nng/nng.h>
#include <nng/protocol/survey0/survey.h>
#include <nng/protocol/survey0/respond.h>

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

/** Set RECVMAXSZ which, hopefully allows us to 
* receive large responses.
* @param   socket  - Socket who's options we set.
* @param   nresp   - Number of respondenrs.
* @param   respsize - Size of the response from each.
*
*  We set the above quantities to 2*nresp*respsize.
*/
static void
setOptions(nng_socket socket, int nresp, int respsize) {
    size_t max = respsize*nresp*2;
    checkstat(
        nng_setopt(socket, NNG_OPT_RECVMAXSZ, &max, sizeof(max)),
        "Unable to set RECVMAXSZ"
    );
}

/**
 *  responder
 *    This is the responder thread.  We answer the specified number
 * of surveys and, then read one more after which we close the socket
 * and finsh.
 * 
 * @param uri - Uri of the serveyor.
 * @param nsurveys - number of surveys to expect.
 * @param msgsize - size of  our response message.
 * 
 *
 */
static void
responder(std::string uri, size_t nsurveys, size_t msgsize) {
    uint8_t* reply = new uint8_t[msgsize];   // one alloc for all replies.
    nng_socket s;
    void* pMsg;     // survey msg.
    size_t rcvSize; // Received size of survey.
    
    // Setup to receive surveys.

    checkstat(
        nng_respondent0_open(&s), "Unable to open responder socket"
    );
    setOptions(s, 0, 0);     // Unlimited 
    checkstat(
        nng_dial(s, uri.c_str(), nullptr, 0),
        "Unagle to dial into the survey "
    );

    // Now process the normal surveys:

    for (int i=0; i < nsurveys; i++) {
        
        // accept survey:

        checkstat(
            nng_recv(s, &pMsg, &rcvSize, NNG_FLAG_ALLOC),
            "Unable to get a survey."
        );
        nng_free(pMsg, rcvSize);
        
        // Send response:

        checkstat(
            nng_send(s, reply, msgsize, 0),
            "Unable to respond to survey"
        );
    }
    // Get that last survey.. by now the surveyor must have our last
    // response.

    checkstat(
        nng_recv(s, &pMsg, &rcvSize, NNG_FLAG_ALLOC),
        "Unable to get extra measure survey"
    );
    nng_close(s);                              // not gonna even respond.
    delete[] reply;
}
/**
 * survey
 *   Do one survey and collect all of the responses.  The assumption is
 *   that all respondents will respond.
 * 
 * @param s - socket used to survey and collect responses.
 * @param p - Pointer to the survey message.
 * @param size - size of the survey message.
 * @param nresp - number of responses to collect.
 */
static void
survey(nng_socket s, void* p, size_t size, size_t nresp)  {
    checkstat(
        nng_send(s, p, size, 0), 
        "Failed to send functional surveyt"
    );
    for (int i = 0; i < nresp; i++) {
        void* presponse;
        size_t rcvSize;
        checkstat(
            nng_recv(s, &presponse, &rcvSize, NNG_FLAG_ALLOC),
            "Failed to receive a functional response"
        );
    }
}
/**
 * surveyor
 *    Performs the survey.
 * @param s       - socket on which we survey.
 * @param nsurvey - number of surveys for which we get reponses.
 * @param size    - Size of the survey msg.
 * @param nresp   - Number of responders.
 * @note we send an extra survey but don't wait for a response.
 *      this ensures the responders have all got their last response
 *      flushed to the socket prior to exit.
 *      The caller can, after we return, join the responder thread(s).
 */
static void
surveyor(nng_socket s, size_t nsurvey, size_t size, size_t nresp) {
    // Don't realloc the mssage each time.

    uint8_t* pMsg = new uint8_t[size];

    // do the survey/respond game

    for (int i=0; i < nsurvey; i++) {
        survey(s, pMsg, size, nresp);   // Surevey and collect responses.
    }

    // do the extra survey (1 byte).
    // This signals the responder it can exit.
    checkstat(
        nng_send(s, pMsg, 1, 0),
        "Unable to send ending survey"
    );
}

//  Entry point.
//
int main(int argc, char** argv) {
    std::string uri(argv[1]);
    size_t nreq = atoi(argv[2]);
    size_t msgSize = atoi(argv[3]);
    size_t nSurveyed = atoi(argv[4]);

    nng_socket s;
    std::vector<std::thread*> threads;

    // Set up to measure large survey small response:

    // Start the surveyer:

    checkstat(
        nng_surveyor0_open(&s),
        "Failed to open survey socket."
    );
    setOptions(s, 0,0);                 // Unlimited.
    checkstat(
        nng_listen(s, uri.c_str(), nullptr, 0),
        "Surveyor failed to start listening"
    );
    checkstat(
        nng_setopt_ms(s, NNG_OPT_SURVEYOR_SURVEYTIME, 8000),
        "Failed to set survey max response time."
    );
    // Start the respondeents - reply size 1 bytes.

    for (int i =0; i < nSurveyed; i++) {
        threads.push_back(new std::thread(responder, uri, nreq, 1));
    }
    sleep(1);                 // Let the threads start.
    // Ready to time:

    auto bigstart = std::chrono::high_resolution_clock::now();
    surveyor(s, nreq, msgSize, nSurveyed);
    for (auto p: threads) {
        p->join();
    }
    auto bigend = std::chrono::high_resolution_clock::now();

    threads.clear(); 
    nng_close(s),
    
    // set up to measure small survey big response.

    checkstat(
        nng_surveyor0_open(&s),
        "Failed to open second survey socket"
    );
    checkstat(
        nng_listen(s, uri.c_str(), nullptr, 0),
        "Failed to start listening for second survey"
    );
    checkstat(
        nng_setopt_ms(s, NNG_OPT_SURVEYOR_SURVEYTIME, 8000),
        "Failed to set survey max response time."
    );
    for (int i =0; i < nSurveyed; i++) {
        threads.push_back(new std::thread(responder, uri, nreq, msgSize));
    }
    sleep(1);        
    // ready to time.

    auto smallstart = std::chrono::high_resolution_clock::now();
    surveyor(s, nreq, 1, nSurveyed);     // survey one byte
    for (auto p:threads ) {
        p->join();
    }
    auto smallend = std::chrono::high_resolution_clock::now();
    threads.clear();
    nng_close(s);

    // Now we can compute the timings.
    // we don't count the 1byte messages in the througput

    // Big survey small response:

    auto bigduration = bigend - bigstart;
    double bigsecs  = 
        (double)std::chrono::duration_cast<std::chrono::milliseconds>(bigduration).count()/1000.0;
    double bigmsgs  = (double)nreq/bigsecs;
    double bigkbs   = (double)(nreq * msgSize)/(1024.0*bigsecs);

    // Small survey big response... remember to multiple the kbps by number
    // of responders.

    auto smallduration = smallend - smallstart;
    double smallsecs = (double)std::chrono::duration_cast<std::chrono::milliseconds>(smallduration).count()/1000.0;
    double smallmsgs = (double)nreq/smallsecs;
    double smallkbs    = (double)(nreq*nSurveyed*msgSize)/(1024*smallsecs);

    // output the report

    std::cout << "Big survey small response:\n";
    std::cout << "Time         : " << bigsecs << std::endl;
    std::cout << "Surveys/sec  : " << bigmsgs << std::endl;
    std::cout << "Kb/sec       : " << bigkbs << std::endl;

    std::cout << "Small survey, big response:\n";
    std::cout << "Time         : " << smallsecs << std::endl;
    std::cout << "Surveys/sec  : " << smallmsgs << std::endl;
    std::cout << "Kb/sec       : " << smallkbs << std::endl;
}
