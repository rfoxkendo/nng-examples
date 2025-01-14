PROGRAMS=pair pubsub reqrep pushpull survey bus

all: $(PROGRAMS)

FLAGS=-lnng -std=c++20 -g -O0

pair: pair.cpp
	$(CXX) -o pair pair.cpp $(FLAGS)

pubsub: pubsub.cpp
	$(CXX) -o pubsub pubsub.cpp $(FLAGS)

reqrep: reqrep.cpp
	$(CXX) -o reqrep reqrep.cpp $(FLAGS)

pushpull: pushpull.cpp
	$(CXX) -o pushpull pushpull.cpp $(FLAGS)

survey: survey.cpp
	$(CXX) -o survey survey.cpp $(FLAGS)

bus: bus.cpp
	$(CXX) -o bus bus.cpp $(FLAGS)
clean:
	rm -f $(PROGRAMS)
