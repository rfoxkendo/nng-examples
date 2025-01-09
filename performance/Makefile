PROGRAMS=pair pubsub reqrep pushpull

all: $(PROGRAMS)

FLAGS=-lnng -std=c++20 -g

pair: pair.cpp
	$(CXX) -o pair pair.cpp $(FLAGS)

pubsub: pubsub.cpp
	$(CXX) -o pubsub pubsub.cpp $(FLAGS)

reqrep: reqrep.cpp
	$(CXX) -o reqrep reqrep.cpp $(FLAGS)

pushpull: pushpull.cpp
	$(CXX) -o pushpull pushpull.cpp $(FLAGS)

clean:
	rm -f $(PROGRAMS)
