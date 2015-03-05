all: rf433recieve

rf433recieve: rf433recieve.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $+ -o $@ -lwiringPi

simpleSniff: simpleSniff.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $+ -o $@ -lwiringPi

X10RFSnifferBit: X10RFSnifferBit.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $+ -o $@ -lwiringPi
	
clean:
	$(RM) *.o send