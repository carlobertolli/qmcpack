//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source License.
// See LICENSE file in top directory for details.
//
// Copyright (c) 2016 Jeongnim Kim and QMCPACK developers.
//
// File developed by: Ye Luo, yeluo@anl.gov, Argonne National Laboratory
//
// File created by: Ye Luo, yeluo@anl.gov, Argonne National Laboratory
//////////////////////////////////////////////////////////////////////////////////////
// -*- C++ -*-
/** @file restart.cpp
 * @brief developing restart IO
 */

#include <Configuration.h>
#include <Message/CommOperators.h>
#include <Particle/MCWalkerConfiguration.h>
#include <Particle/HDFWalkerOutput.h>
#include <HDFVersion.h>
#include <Particle/HDFWalkerInput_0_4.h>
#include <OhmmsApp/RandomNumberControl.h>
#include <random/random.hpp>
#include <miniapps/graphite.hpp>
#include <miniapps/pseudo.hpp>
#include <Utilities/Timer.h>
#include <miniapps/common.hpp>
#include <getopt.h>
#include <mpi/collectives.h>

using namespace std;
using namespace qmcplusplus;

void setWalkerOffsets(MCWalkerConfiguration& W, Communicate* myComm)
{
  std::vector<int> nw(myComm->size(),0),nwoff(myComm->size()+1,0);
  nw[myComm->rank()]=W.getActiveWalkers();
  myComm->allreduce(nw);
  for(int ip=0; ip<myComm->size(); ip++)
    nwoff[ip+1]=nwoff[ip]+nw[ip];
  W.setGlobalNumWalkers(nwoff[myComm->size()]);
  W.setWalkerOffsets(nwoff);
}

int main(int argc, char** argv)
{

  OHMMS::Controller->initialize(0, NULL);
  OhmmsInfo("restart");
  Communicate* myComm=OHMMS::Controller;
  myComm->setName("restart");

  typedef QMCTraits::RealType           RealType;
  typedef ParticleSet::ParticlePos_t    ParticlePos_t;
  typedef ParticleSet::ParticleLayout_t LatticeType;
  typedef ParticleSet::TensorType       TensorType;
  typedef ParticleSet::PosType          PosType;
  typedef RandomGenerator_t::uint_type  uint_type;

  //use the global generator

  int na=4;
  int nb=4;
  int nc=1;
  int nsteps=100;
  int iseed=11;
  int AverageWalkersPerNode=0;
  int nwtot;
  std::vector<int> wPerNode;
  RealType Rmax(1.7);

  const int NumThreads=omp_get_max_threads();

  char *g_opt_arg;
  int opt;
  while((opt = getopt(argc, argv, "hg:i:r:")) != -1)
  {
    switch(opt)
    {
      case 'h':
        printf("[-g \"n0 n1 n2\"]\n");
        return 1;
      case 'g': //tiling1 tiling2 tiling3
        sscanf(optarg,"%d %d %d",&na,&nb,&nc);
        break;
      case 'i': //number of MC steps
        nsteps=atoi(optarg);
        break;
      case 's'://random seed
        iseed=atoi(optarg);
        break;
      case 'w'://the number of walkers
        AverageWalkersPerNode=atoi(optarg);
        break;
      case 'r'://rmax
        Rmax=atof(optarg);
        break;
    }
  }

  // set the number of walkers equal to the threads.
  if(!AverageWalkersPerNode) AverageWalkersPerNode=NumThreads;
  //set nwtot, to be random
  nwtot=AverageWalkersPerNode;
  FairDivideLow(nwtot,NumThreads,wPerNode);
  wPerNode.resize(NumThreads+1,0);

  //Random.init(0,1,iseed);
  Tensor<int,3> tmat(na,0,0,0,nb,0,0,0,nc);

  //turn off output
  if(myComm->rank())
  {
    OhmmsInfo::Log->turnoff();
    OhmmsInfo::Warn->turnoff();
  }

  int nptcl=0;
  double t0=0.0,t1=0.0;

  RandomNumberControl::make_seeds();
  std::vector<RandomGenerator_t> myRNG(NumThreads);
  std::vector<uint_type> mt(Random.state_size(),0);
  std::vector<MCWalkerConfiguration> elecs(NumThreads);

  #pragma omp parallel reduction(+:t0)
  {
    int ip=omp_get_thread_num();

    ParticleSet ions;
    MCWalkerConfiguration& els=elecs[ip];
    OHMMS_PRECISION scale=1.0;

    //create generator within the thread
    myRNG[ip]=*RandomNumberControl::Children[ip];
    RandomGenerator_t& random_th=myRNG[ip];

    tile_graphite(ions,tmat,scale);

    const int nions=ions.getTotalNum();
    const int nels=4*nions;
    const int nels3=3*nels;

    #pragma omp master
    nptcl=nels;

    {//create up/down electrons
      els.Lattice.BoxBConds=1;   els.Lattice.set(ions.Lattice);
      vector<int> ud(2); ud[0]=nels/2; ud[1]=nels-ud[0];
      els.create(ud);
      els.R.InUnit=1;
      random_th.generate_uniform(&els.R[0][0],nels3);
      els.convert2Cart(els.R); // convert to Cartiesian
      els.RSoA=els.R;
    }

    // save random seeds and electron configurations.
    *RandomNumberControl::Children[ip]=myRNG[ip];
    //MCWalkerConfiguration els_save(els);

  } //end of omp parallel
  Random.save(mt);

  elecs[0].createWalkers(nwtot);
  setWalkerOffsets(elecs[0], myComm);

  //storage variables for timers
  double h5write = 0.0, h5read = 0.0; //random seed R/W speeds
  double walkerWrite = 0.0, walkerRead =0.0; //walker R/W speeds
  Timer h5clock; //timer for the program

  // dump random seeds
  h5clock.restart(); //start timer
  RandomNumberControl::write("restart",myComm);
  myComm->barrier();
  h5write += h5clock.elapsed(); //store timer

  // flush random seeds to zero
  #pragma omp parallel
  {
    int ip=omp_get_thread_num();
    RandomGenerator_t& random_th=*RandomNumberControl::Children[ip];
    std::vector<uint_type> vt(random_th.state_size(),0);
    random_th.load(vt);
  }
  std::vector<uint_type> mt_temp(Random.state_size(),0);
  Random.load(mt_temp);

  // load random seeds
  h5clock.restart(); //start timer
  RandomNumberControl::read("restart",myComm);
  h5read += h5clock.elapsed(); //store timer

  // validate random seeds
  int mismatch_count=0;
  #pragma omp parallel reduction(+:mismatch_count)
  {
    int ip=omp_get_thread_num();
    RandomGenerator_t& random_th=myRNG[ip];
    std::vector<uint_type> vt_orig(random_th.state_size());
    std::vector<uint_type> vt_load(random_th.state_size());
    random_th.save(vt_orig);
    RandomNumberControl::Children[ip]->save(vt_load);
    for(int i=0; i<random_th.state_size(); i++)
      if(vt_orig[i]!=vt_load[i]) mismatch_count++;
  }
  Random.save(mt_temp);
  for(int i=0; i<Random.state_size(); i++)
    if(mt_temp[i]!=mt[i]) mismatch_count++;

  myComm->allreduce(mismatch_count);

  if(!myComm->rank())
  {
    if(mismatch_count!=0)
      std::cout << "Fail: random seeds mismatch between write and read!\n"
                << "state_size= " << myRNG[0].state_size() << " mismatch_cout=" << mismatch_count << std::endl;
    else
      std::cout << "Pass: random seeds match exactly between write and read!\n";
  }

  // dump electron coordinates.
  h5clock.restart(); //start timer
  HDFWalkerOutput wOut(elecs[0],"restart",myComm);
  wOut.dump(elecs[0],1);
  myComm->barrier();
  walkerWrite += h5clock.elapsed(); //store timer

  if(!myComm->rank())
    std::cout << "Walkers are dumped!\n";
  const char *restart_input = \
"<tmp> \
  <mcwalkerset fileroot=\"restart\" node=\"-1\" version=\"3 0\" collected=\"yes\"/> \
</tmp> \
";

  Libxml2Document doc;
  bool okay = doc.parseFromString(restart_input);
  xmlNodePtr root = doc.getRoot();
  xmlNodePtr restart_leaf = xmlFirstElementChild(root);

  h5clock.restart(); //start timer
  HDFVersion in_version(0,4);
  HDFWalkerInput_0_4 wIn(elecs[0],myComm,in_version);
  wIn.put(restart_leaf);
  walkerRead += h5clock.elapsed(); //store time spent

  //print out hdf5 R/W times
  TinyVector<double,4> timers(h5read, h5write, walkerRead, walkerWrite);
  mpi::reduce(*myComm, timers);
  h5read = timers[0]/myComm->size();
  h5write = timers[1]/myComm->size();
  walkerRead = timers[2]/myComm->size();
  walkerWrite = timers[3]/myComm->size();
  if(myComm->rank() == 0)
  {
    cout << "\nTotal time of writing random seeds to HDF5 file: " << setprecision(2) << h5write << "\n";
    cout << "\nTotal time of reading random seeds in HDF5 file: " << setprecision(2) << h5read << "\n";
    cout << "\nTotal time of writing walkers to HDF5 file: " << setprecision(2) << walkerWrite << "\n";
    cout << "\nTotal time of reading walkers in HDF5 file: " << setprecision(2) << walkerRead << "\n";
  }


  OHMMS::Controller->finalize();

  return 0;
}
