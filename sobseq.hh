#include <vector>

class SobolSequence
{
 public:
  SobolSequence()
  {
    int j,k,l;
    unsigned int i, ipp;
        
    for (k=0;k<MAXDIM;k++) d_ix[k]=0;
    d_in=0;
    if (d_iv[0] != 1) return;
    
    d_fac=1.0/(1 << MAXBIT);
    for (j=0,k=0;j<MAXBIT;j++,k+=MAXDIM)
      d_iu[j] = &d_iv[k];
    for (k=0;k<MAXDIM;k++) {
      for (j=0;j<d_mdeg[k];j++)
        d_iu[j][k] <<= (MAXBIT-1-j);
      for (j=d_mdeg[k];j<MAXBIT;j++) {
        ipp=d_ip[k];
        i=d_iu[j-d_mdeg[k]][k];
        i ^= (i >> d_mdeg[k]);
        for (l=d_mdeg[k]-1;l>=1;l--) {
          if (ipp & 1) i ^= d_iu[j-l][k];
          ipp >>= 1;
        }
        d_iu[j][k]=i;
      }
    }
  }
  void get(int n, std::vector<double>& x);
  
 private:
  static constexpr int MAXBIT=30,MAXDIM=6;
  int d_mdeg[MAXDIM]={1,2,3,3,4,4};
  unsigned int d_in;
  std::vector<int> d_ix{MAXDIM};
  std::vector<unsigned int*> d_iu{MAXBIT};
  unsigned int d_ip[MAXDIM]={0,1,1,2,1,4};
  unsigned int d_iv[MAXDIM*MAXBIT]=
    {1,1,1,1,1,1,3,1,3,3,1,1,5,7,7,3,3,5,15,11,5,15,13,9};
  double d_fac;
};

void SobolSequence::get(const int n, std::vector<double> &x)
{
  int j,k;
  unsigned int im;
  
  im=d_in++;
  for (j=0;j<MAXBIT;j++) {
    if (!(im & 1)) break;
    im >>= 1;
  }
  if (j >= MAXBIT) throw("MAXBIT too small in sobseq");
  im=j*MAXDIM;
  for (k=0;k < std::min(n,MAXDIM);k++) {
    d_ix[k] ^= d_iv[im+k];
    x[k]=d_ix[k]*d_fac;
  }
}
