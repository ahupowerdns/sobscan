#include "sobseq.hh"
#include <vector>
#include <fstream>
#include "netmask.hh"
#include <random>
#include "sclasses.hh"
#include "record-types.hh"
#include <thread>
#include "common.hh"

using namespace std;

auto makeIPStr(double a, double b, double c, double d)
{
  ostringstream ipstream;
  ipstream << ((int)(256*a)) << "." <<
    ((int)(256*b)) << "." <<
    ((int)(256*c)) << "." <<
    ((int)(256*d));
    
  return ipstream.str();
}

auto makeDNSQuery(const std::string& name)
{
  DNSName dn = makeDNSName(name);
  DNSType dt = DNSType::TXT;
  DNSMessageWriter dmw(dn, dt);
          
  dmw.dh.rd = true;
  dmw.randomizeID();
  return dmw.serialize();
}

uint32_t g_openResolvers;
void listenerThread(int s, unsigned int* counter, string name)
{
  ComboAddress dest("0.0.0.0");
  for(;;) {
    auto resp=SRecvfrom(s, 1500, dest);
    try {
      DNSMessageReader dmr(resp);
      DNSSection rrsection;
      uint32_t ttl;
      
      cout<<name<<" response from "<<dest.toStringWithPort() <<", "<<(RCode)dmr.dh.rcode<<endl;
      std::unique_ptr<RRGen> rr;
      DNSName dn;
      DNSType dt;
      while(dmr.getRR(rrsection, dn, dt, ttl, rr)) {
        cout << " "<<dn<< " IN " << dt << " " << ttl << " " <<rr->toString()<<endl;
        if(dn == DNSName({"whoami-ecs", "lua", "powerdns", "org"}) && dt == DNSType::TXT) {
          ++g_openResolvers;
          break;
        }
      }
      
      (*counter)++;
    }
    catch(std::exception& e) {
      cout<<"Error parsing DNS response from "<<dest.toString()<<": "<<e.what()<<endl;
    }
      
  }
}

int main(int argc, char**argv)
{
  if(argc != 2) {
    cout<<"Syntax: dnsscan prefixesfile\n";
    return EXIT_FAILURE;
  }
  NetmaskTree<bool> table;

  loadNetmaskTree(argv[1], table);
  
  SobolSequence ss;
  vector<double> ip(4);

  std::random_device rd{};
  std::mt19937 gen{rd()};
  std::uniform_real_distribution<> d(0, 1.0);
  
  unsigned int sobmatches=0, rndmatches=0;

  Socket sobsock(AF_INET, SOCK_DGRAM), rndsock(AF_INET, SOCK_DGRAM);

  uint32_t sobresponses=0, rndresponses=0;
  std::thread sobthread(listenerThread, (int)sobsock, &sobresponses, "sob");
  std::thread rndthread(listenerThread, (int)rndsock, &rndresponses, "rnd");
  
  sobthread.detach();
  rndthread.detach();
  string dnsquery = makeDNSQuery("whoami-ecs.lua.powerdns.org");
  ofstream sobplot("sobplot"), rndplot("rndplot"), oresplot("oresplot"), comboplot("comboplot");
  for(unsigned int n = 0; (sobmatches + rndmatches) < 100000 ; ++n) {
    ss.get(4, ip);
    auto str = makeIPStr(ip[0], ip[1], ip[2], ip[3]);
    ComboAddress dst(str, 53);
    if(table.match(dst)) {
      ++sobmatches;
      SSendto(sobsock, dnsquery, dst);
    }
    str = makeIPStr(d(gen), d(gen), d(gen), d(gen));
    dst=ComboAddress(str, 53);
    if(table.match(dst)) {
      SSendto(rndsock, dnsquery, dst);
      ++rndmatches;
    }
    usleep(1000);


    if(!(n%1024)) {
      cout<<n<<endl;
      sobplot << sobmatches << '\t' << sobresponses << '\t' << (100.0*sobresponses/sobmatches) << endl;
      rndplot << rndmatches << '\t' << rndresponses << '\t' << (100.0*rndresponses/rndmatches) << endl;
      oresplot << (sobmatches + rndmatches) << '\t' << g_openResolvers << '\t' << (100.0*g_openResolvers / (sobmatches + rndmatches)) << endl;
      comboplot << (sobmatches + rndmatches) << '\t' << sobresponses + rndresponses << '\t' << 100.0*(sobresponses+rndresponses)/(sobmatches+rndmatches) << endl;
    }
    
  }
  cout<<"\nDone"<<endl;
}
