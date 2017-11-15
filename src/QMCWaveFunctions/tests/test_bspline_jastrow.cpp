//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source License.
// See LICENSE file in top directory for details.
//
// Copyright (c) 2016 Jeongnim Kim and QMCPACK developers.
//
// File developed by: Mark Dewing, markdewing@gmail.com, University of Illinois at Urbana-Champaign
//
// File created by: Mark Dewing, markdewing@gmail.com, University of Illinois at Urbana-Champaign
//////////////////////////////////////////////////////////////////////////////////////
    
    

#include "catch.hpp"

#include "OhmmsData/Libxml2Doc.h"
#include "OhmmsPETE/OhmmsMatrix.h"
#include "Utilities/OhmmsInfo.h"
#include "Lattice/ParticleBConds.h"
#include "Particle/ParticleSet.h"
#include "Particle/DistanceTableData.h"
#include "Particle/DistanceTable.h"
#include "Particle/SymmetricDistanceTableData.h"
#include "QMCWaveFunctions/OrbitalBase.h"
#include "QMCWaveFunctions/TrialWaveFunction.h"
#include "QMCWaveFunctions/Jastrow/TwoBodyJastrowOrbital.h"
#include "QMCWaveFunctions/Jastrow/BsplineFunctor.h"
#include "QMCWaveFunctions/Jastrow/BsplineJastrowBuilder.h"
#include "ParticleBase/ParticleAttribOps.h"
#ifdef ENABLE_SOA
#include "QMCWaveFunctions/Jastrow/J2OrbitalSoA.h"
#endif

#include <stdio.h>
#include <string>

using std::string;

namespace qmcplusplus
{

TEST_CASE("BSpline functor zero", "[wavefunction]")
{

  BsplineFunctor<double> bf;

  double r = 1.2;
  double u = bf.evaluate(r);
  REQUIRE(u == 0.0);
}

TEST_CASE("BSpline functor one", "[wavefunction]")
{

  BsplineFunctor<double> bf;

  bf.resize(1);

  double r = 1.2;
  double u = bf.evaluate(r);
  REQUIRE(u == 0.0);
}

TEST_CASE("BSpline builder Jastrow", "[wavefunction]")
{

  Communicate *c;
  OHMMS::Controller->initialize(0, NULL);
  c = OHMMS::Controller;
  OhmmsInfo("testlogfile");

  ParticleSet ions_;
  ParticleSet elec_;

  ions_.setName("ion");
  ions_.create(1);
  ions_.R[0][0] = 2.0;
  ions_.R[0][1] = 0.0;
  ions_.R[0][2] = 0.0;

  elec_.setName("elec");
  std::vector<int> ud(2); ud[0]=ud[1]=1;
  elec_.create(ud);
  elec_.R[0][0] = 1.00;
  elec_.R[0][1] = 0.0;
  elec_.R[0][2] = 0.0;
  elec_.R[1][0] = 0.0;
  elec_.R[1][1] = 0.0;
  elec_.R[1][2] = 0.0;

  SpeciesSet &tspecies =  elec_.getSpeciesSet();
  int upIdx = tspecies.addSpecies("u");
  int downIdx = tspecies.addSpecies("d");
  int chargeIdx = tspecies.addAttribute("charge");
  tspecies(chargeIdx, upIdx) = -1;
  tspecies(chargeIdx, downIdx) = -1;

#ifdef ENABLE_SOA
  elec_.addTable(ions_,DT_SOA);
#else
  elec_.addTable(ions_,DT_AOS);
#endif
  elec_.resetGroups();
  elec_.update();


  TrialWaveFunction psi = TrialWaveFunction(c);

const char *particles = \
"<tmp> \
<jastrow name=\"J2\" type=\"Two-Body\" function=\"Bspline\" print=\"yes\"> \
   <correlation rcut=\"10\" size=\"10\" speciesA=\"u\" speciesB=\"d\"> \
      <coefficients id=\"ud\" type=\"Array\"> 0.02904699284 -0.1004179 -0.1752703883 -0.2232576505 -0.2728029201 -0.3253286875 -0.3624525145 -0.3958223107 -0.4268582166 -0.4394531176</coefficients> \
    </correlation> \
</jastrow> \
</tmp> \
";
  Libxml2Document doc;
  bool okay = doc.parseFromString(particles);
  REQUIRE(okay);

  xmlNodePtr root = doc.getRoot();

  xmlNodePtr jas1 = xmlFirstElementChild(root);

  BsplineJastrowBuilder jastrow(elec_, psi);
  bool build_okay = jastrow.put(jas1);
  REQUIRE(build_okay);

  OrbitalBase *orb = psi.getOrbitals()[0];

#ifdef ENABLE_SOA
  typedef J2OrbitalSoA<BsplineFunctor<OrbitalBase::RealType> > J2Type;
#else
  typedef TwoBodyJastrowOrbital<BsplineFunctor<OrbitalBase::RealType> > J2Type;
#endif
  J2Type *j2 = dynamic_cast<J2Type *>(orb);
  REQUIRE(j2 != NULL);

  double logpsi = psi.evaluateLog(elec_);
  REQUIRE(logpsi == Approx(0.1012632641)); // note: number not validated

  double KE = -0.5*(Dot(elec_.G,elec_.G)+Sum(elec_.L));
  REQUIRE(KE == Approx(-0.1616624771)); // note: number not validated


  struct JValues
  {
   double r;
   double u;
   double du;
   double ddu;
  };

  // Cut and paste from output of gen_bspline_jastrow.py
   const int N = 20;
   JValues Vals[N] = {
    {0.00,    0.1374071801,            -0.5,    0.7866949593},
    {0.60,  -0.04952403966,   -0.1706645865,    0.3110897524},
    {1.20,    -0.121361995,  -0.09471371432,     0.055337302},
    {1.80,   -0.1695590431,  -0.06815900213,    0.0331784053},
    {2.40,   -0.2058414025,  -0.05505192964,   0.01049597156},
    {3.00,   -0.2382237097,  -0.05422744821, -0.002401552969},
    {3.60,   -0.2712606182,  -0.05600918024, -0.003537553803},
    {4.20,   -0.3047843679,  -0.05428535477,    0.0101841028},
    {4.80,   -0.3347515004,  -0.04506573714,   0.01469003611},
    {5.40,   -0.3597048574,  -0.03904232165,  0.005388015505},
    {6.00,   -0.3823503292,  -0.03657502025,  0.003511355265},
    {6.60,   -0.4036800017,  -0.03415678101,  0.007891305516},
    {7.20,   -0.4219818468,  -0.02556305518,   0.02075444724},
    {7.80,   -0.4192355508,   0.06799438701,    0.3266190181},
    {8.40,   -0.3019238309,      0.32586994,    0.2880861726},
    {9.00,  -0.09726352421,    0.2851358014,   -0.4238666348},
    {9.60, -0.006239062395,   0.04679296796,   -0.2339648398},
    {10.20,               0,               0,               0},
    {10.80,               0,               0,               0},
    {11.40,               0,               0,               0}
   };


  BsplineFunctor<OrbitalBase::RealType> *bf = j2->F[0];

  for (int i = 0; i < N; i++) {
    OrbitalBase::RealType dv = 0.0;
    OrbitalBase::RealType ddv = 0.0;
    OrbitalBase::RealType val = bf->evaluate(Vals[i].r,dv,ddv);
    REQUIRE(Vals[i].u == Approx(val));
    REQUIRE(Vals[i].du == Approx(dv));
    REQUIRE(Vals[i].ddu == Approx(ddv));
  }

#if 0
  // write out values of the Bspline functor
  //BsplineFunctor<double> *bf = j2->F[0];
  printf("NumParams = %d\n",bf->NumParams);
  printf("CuspValue = %g\n",bf->CuspValue);
  printf("DeltaR = %g\n",bf->DeltaR);
  printf("SplineCoeffs size = %d\n",bf->SplineCoefs.size());
  for (int j = 0; j < bf->SplineCoefs.size(); j++)
  {
    printf("%d %g\n",j,bf->SplineCoefs[j]);
  }
  printf("\n");

  for (int i = 0; i < 20; i++) {
    double r = 0.6*i;
    elec_.R[0][0] = r;
    elec_.update();
    double logpsi = psi.evaluateLog(elec_);
    //double alt_val = bf->evaluate(r);
    double dv = 0.0;
    double ddv = 0.0;
    double alt_val = bf->evaluate(r,dv,ddv);
    printf("%g %g %g %g %g\n",r,logpsi,alt_val,dv,ddv);
  }
#endif


}
}

