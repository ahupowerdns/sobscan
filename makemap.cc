#include "sobseq.hh"
#include <iostream>
#include <vector>
#include <fstream>
#include "netmask.hh"
#include <random>
#include "sclasses.hh"
#include "record-types.hh"
#include <thread>

using namespace std;

int main(int argc, char**argv)
{
  if(argc != 2) {
    cout<<"Syntax: makemap prefixesfile\n";
    return EXIT_FAILURE;
  }
  std::string line;
  ifstream netmasks(argv[1]);
  NetmaskTree<bool> table;

  while(getline(netmasks, line)) {
    auto pos = line.find_first_of(" \n\r;");
    if(pos != string::npos)
      line.resize(pos);
    Netmask nm(line);
    table.insert(nm);
  }
  cout<<"\rHave "<<table.size()<<" netmasks"<<endl;
  
  vector<vector<int>> plot;
  plot.resize(256);
  for(auto& c : plot)
    c.resize(256);

  uint32_t numAnnounced=0;
  ComboAddress ca;
  ca.sin4.sin_family=AF_INET;
  
  uint8_t* ptr = (uint8_t*)&ca.sin4.sin_addr.s_addr;
  ptr[3]=0;
  for(int a=0; a < 256; ++a) {
    ptr[0]=a;

    for(int b=0; b < 256; ++b) {
      ptr[1]=b;
      for(int c=0; c < 256; ++c) {
        ptr[2]=c;
        if(table.match(ca)) {
          plot[a][b]++;
          numAnnounced += 256;
        }
      }
    }
  }
  cout<<"\r"<<numAnnounced<<" IPv4 addresses announced ("<< 100.0*numAnnounced/std::numeric_limits<uint32_t>::max()<<"%)"<<endl;

  cout<<"Writing data to file 'denso'"<<endl;
  ofstream denso("denso");
  for(int a=0; a < 256; ++a) {
    for(int b=0; b < 256; ++b) {
      denso << a << '\t' << b << '\t' << plot[a][b] << '\n';
    }
  }
}

