#ifndef OHMMS_TOOLS_EXTERNAL_GAUSSIANPARSERBASE_H
#define OHMMS_TOOLS_EXTERNAL_GAUSSIANPARSERBASE_H
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include "OhmmsPETE/TinyVector.h"
#include "OhmmsData/OhmmsElementBase.h"

struct OhmmsAsciiParser {

  char dbuffer[200];

  template<class T>
  inline void getValue(std::istream& is, T& aval) {
    is.getline( dbuffer, sizeof ( dbuffer ));
    std::istringstream a(dbuffer); a>> aval;
  }

  template<class IT>
  inline void getValues(std::istream& is, IT first, IT last) {
    while(first != last) {
      is.getline( dbuffer, sizeof ( dbuffer ));
      std::istringstream a(dbuffer);
      while(a >> *first){first++;}
    }
  }

  int search(std::istream& is, const std::string& keyword) {
    bool notfound = true;
    while(notfound) {
      std::string aline;
      getline(is,aline,'\n');
      if(aline.find(keyword) < aline.size()) {
        notfound = false;
      } 
    }
    return 1;
  }
};

struct QMCGaussianParserBase {

  typedef double value_type;

  bool SpinRestricted;
  int NumberOfAtoms;
  int NumberOfEls;
  int SizeOfBasisSet;
  std::string Title;
  std::string basisType;
  std::string basisName;
  std::string Normalized;
  std::string CurrentCenter;
  std::vector<TinyVector<value_type,3> > R;
  std::vector<int> GroupID;
  std::vector<int> gShell, gNumber, gBound;
  std::vector<value_type> Qv;
  std::vector<value_type> gExp, gC0, gC1;
  //std::vector<GaussianCombo<value_type> > gExp, gC0, gC1;
  std::string EigVecU, EigVecD;
  xmlNodePtr gridPtr;

  QMCGaussianParserBase();

  QMCGaussianParserBase(int argc, char** argv);
  void createGridNode(int argc, char** argv);

  xmlNodePtr createBasisSet();
  xmlNodePtr createCenter(int iat, int _off);
  void createShell(int n, int ig, int off_, xmlNodePtr abasis);
  xmlNodePtr createDeterminantSet();

  void map2GridFunctors(xmlNodePtr cur);
  virtual void parse(const std::string& fname) = 0;

  static std::vector<std::string> IonName;
  static std::vector<std::string> gShellType;
  static std::vector<int>         gShellID;

  static void init();
};
#endif
