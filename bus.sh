#!/bin/bash

./bus2 node0 tcp://localhost:2000 \
       tcp://localhost:2001 tcp://localhost:2002 tcp://localhost:2003 &
./bus2 node1 tcp://localhost:2001 \
       tcp://localhost:2002 tcp://localhost:2003 tcp://localhost:2004 &
./bus2 node2 tcp://localhost:2002 \
       tcp://localhost:2003 tcp://localhost:2004 &
./bus2 node3 tcp://localhost:2003 \
       tcp://localhost:2004 &
./bus2 node4 tcp://localhost:2004 tcp://localhost:2000 tcp://localhost:2001 & 
       
