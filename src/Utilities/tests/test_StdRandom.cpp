//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source License.
// See LICENSE file in top directory for details.
//
// Copyright (c) 2020 QMCPACK developers.
//
// File developed by: Peter Doak, doakpw@ornl.gov, Oak Ridge National Lab
//
// File created by: Peter Doak, doakpw@ornl.gov, Oak Ridge National Lab
//////////////////////////////////////////////////////////////////////////////////////


#include "catch.hpp"

#include "Utilities/StdRandom.h"

#include <vector>

namespace qmcplusplus
{

TEST_CASE("StdRandom save and load", "[utilities]")
{
  using DoubleRNG = StdRandom<double>;
  DoubleRNG rng;

  rng.init(111);

  std::vector<double> rng_doubles(100,0.0);
  for(auto& elem : rng_doubles)
    elem = rng();

  std::vector<DoubleRNG::uint_type> state;

  rng.save(state);

  CHECK(state.size() == rng.state_size());

  DoubleRNG rng2;
  rng2.init(110);
  rng2.load(state);

  CHECK(rng2() == rng());
}

} // namespace qmcplusplus
