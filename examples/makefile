CXXFLAGS += -std=c++14 -O2
LDLIBS += -lasound

%.exe : %.cc ../modo.hh
	$(CXX) -o $@ $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) $< $(LDLIBS)

%.wav : %.exe
	./$< $@

%.ogg : %.wav
	oggenc $<
