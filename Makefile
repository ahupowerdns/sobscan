CXXFLAGS:= -std=gnu++14 -Wall -O3 -MMD -MP -ggdb -Iext/simplesocket -Iext/hello-dns/tdns/

PROGRAMS = makemap dnsscan

all: $(PROGRAMS)

clean:
	rm -f *~ *.o *.d ext/*/*.o $(PROGRAMS)

-include *.d

makemap: makemap.o ext/simplesocket/comboaddress.o common.o
	g++ -std=gnu++14 $^ -o $@

dnsscan: dnsscan.o ext/simplesocket/swrappers.o ext/simplesocket/comboaddress.o ext/hello-dns/tdns/record-types.o ext/hello-dns/tdns/dnsmessages.o ext/hello-dns/tdns/dns-storage.o common.o
	g++ -std=gnu++14 $^ -o $@ -pthread

