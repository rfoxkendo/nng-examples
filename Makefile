PROGRAMS=bus pull push reply req onetoones onetoonec publisher subscriber \
	surveyor respondent
all: $(PROGRAMS)


# bus.

bus: bus.cpp
	$(CXX) -g -o bus bus.cpp -lnng

#push pull

pull: pull.cpp
	$(CXX) -g -o pull pull.cpp -lnng

push: push.cpp
	$(CXX) -g -o push push.cpp -lnng

# req/rep

reply: reply.cpp
	$(CXX) -g -o reply reply.cpp -lnng

req: req.cpp
	$(CXX) -g -o req req.cpp -lnng

# Peer one:one

onetoones: onetoones.cpp
	$(CXX) -g -o onetoones onetoones.cpp -lnng

onetoonec: onetoonec.cpp
	$(CXX) -g -o onetoonec onetoonec.cpp -lnng

# pub/sub.

publisher: publisher.cpp
	$(CXX) -g -o publisher publisher.cpp -lnng

subscriber: subscriber.cpp
	$(CXX) -g -o subscriber subscriber.cpp -lnng

#surveyor responder

surveyor: surveyor.cpp
	$(CXX) -g -o surveyor surveyor.cpp -lnng

respondent: respondent.cpp
	$(CXX) -g -o respondent respondent.cpp -lnng
clean:
	rm -f $(PROGRAMS)

