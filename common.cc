#include "common.hh"
#include "netmask.hh"
#include <fstream>
using namespace std;

void loadNetmaskTree(const std::string& name, NetmaskTree<bool> &table)
{
  std::string line;
  ifstream netmasks(name);

  while(getline(netmasks, line)) {
    auto pos = line.find_first_of(" \n\r;");
    if(pos != string::npos)
      line.resize(pos);
    Netmask nm(line);
    if(nm.getBits()) // ignore default routes
      table.insert(nm);
  }
}
