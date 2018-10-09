CXXFLAGS:= -std=gnu++17 -Wall -O3 -MMD -MP -ggdb -Inr/ -Iext/simplesocket -Iext/hello-dns/tdns/

PROGRAMS = makemap

all: $(PROGRAMS)

clean:
	rm -f *~ *.o *.d ext/*/*.o $(PROGRAMS)

-include *.d

makemap: makemap.o ext/simplesocket/swrappers.o ext/simplesocket/comboaddress.o ext/hello-dns/tdns/record-types.o ext/hello-dns/tdns/dnsmessages.o ext/hello-dns/tdns/dns-storage.o
	g++ -std=gnu++17 $^ -o $@ -pthread 
