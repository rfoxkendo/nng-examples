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