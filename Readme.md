# Sample programs for nng communication patterns.

There is a Makefile which will build all of the programs for 
a default installation of nng (tested using Debian bookworm with instalation
from debian package).

Note the latest versions of the FRIBDAQ bookworm and bullseye images have nng installed fromp packages (at this point in time not installed in production).

These programs are test quality.  If you miss a parameter they will likely segfault.

The programs:

## bus

A bus with four participants note that the function steupBus will work 
if you change the uris vector to add or subtract bus memgers.  It sets up the connections needed to ensure all members work.

You must manually start all of the bus members normaly this would be done with
e.g.

```bash
./bus 0 & ./bus 1 & ./bus 2& ./bus 3&
```

The program parameter is the position of the program on the bus and selects the URI it will listen on.

Each instance of the program will send two messages to the bus:

```
Joined the bus
```

and

```
I'll send another
```

Each instance will report the messages it receives so all instances should receive the message sent by everyone but itself.  The received message reports will be of the form:

```
Got a message <me> : <message>
```

where
*  <me> is the bus position number of the instance
*  <message> is the message gotten.

## push pull

The push pull communication pattern features one pusher and an arbitrary number of pullers.  Each push goes to exactly one puller.  Distribution is documented to be round robin which implies a live crashed consumer will bring things to a halt (not tested) and that this pattern does not load level unless the number of pullers is large enough to accomodate the worst case processing for a work unit.


The pusher will push a fixed number of messages determined by the MSG_COUNT constant in push.cpp .  Each message will be of the form

``
message number <seq>
```

where <seq> is the mesage number.

Both push and pull just need a URI that will be used to advertise the push and dial from the puller.  Pull will just report the received messages forever.

One way to run:
```bash
./push <uri> & ./pull <uri> & ... 
```
for as many pullers as you want <uri> is the URI that will be used for communication.  Can be an ipc: or tcp: protocol uri.

## req / reply

The REQ/REQ pair.  Where req makes requests and gets replies.

The reply program just runs on URI.  It accepts textual request and replies to them.  The requests can be any of

* ```STOP``` - the reply program replies ```OK``` and exits.
* ```DATE``` - the reply program replies with ```OK``` followed by a date/time string.
* ```PID``` - the reply program replies with ```OK``` followed by its process id.

for anything else the reply is ```ERROR - unrecognized request```

The request program makes one request of the reply program and outputs the 
response.

Usually one would 

```bash
./reply <uri> &

./req <uri> <request>
```

## onetoonec onetoones

Illustrates the pair communication pattern.  A pair is exactly two end points. Either can initiate communication, there is no expectation of a reply. The distinction between client and server is, therefore, only who listens and who dials.

onetoones - the server is started with
```bash
./onetones <uri> &
```

It gets a message from its peer and echoes it.  It then sends another message, unsollicited by the peer, containing it's pid among other text.  It repeats
this communciation pattern forever.

onetonec - the client is started with

```bash
./onetoonec <uri>  <msg>
```
It dials up the server, and setnds the messages passed on the command line. 
It then receives and prints the echo and the other message the server sends then exits.  Unlike REQ/REP only one instance of ./onetoonec can connect at any time, however serially, several instances can run and exit, allowing the next one to connect.

## publishyer subscriber

This illustrates the PUB/SUB pattern.  The publisher, started by

```bash
./publisher <uri> &
```

Periodically publishes three message:
*  ```DATE <datestring>```
*  ```PID <processid> ```
* ```NAME <argv0>```

Subscribers can the subscsribe to one of  all of those 'topics':

```
./subscriber <uri> TOPIC &
```

Where normally TOPIC is any of:
*  ```DATE``` To get the date messages.
*  ```PID``` to get the PID messages.
*  ```NAME``` to get the name messages.
*  ```""```  to get all messages.

The subscsriber outputs the messages it gets.

## surveyor respondent

Implements the surveyor communication pattern.  Surveyor broadcasts messages to all repondents that are currently dialed in.   Respondents can reply withim the lifetime of the survey.  They need not reply.


The surveyor, started by:

```bash
./surveyor <uri> &
```

 periodically sends out surveys that can any of:

 *  ```ALL``` - everyone please respond.
 *  ```EVEN``` - Responde if your PID is even.
 *  ```ODD``` - Respond if your PID is odd.


 Responses are collected and output until the survey expires (see nng_duration definition).  After the survey expires, sometime later a different survey is sent.  The suveys cycle between the surveys above.

 the repondent, startedvia e.g.

 ```bash
 ./resondent <uri> &
 ```

 Sends its PID back for each survey that matches its PID (ALL or the appropriate4 ODD oro EVEN depending on its actual PID value).
