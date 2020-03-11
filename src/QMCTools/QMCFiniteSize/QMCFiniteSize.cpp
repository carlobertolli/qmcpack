#include "QMCTools/QMCFiniteSize/QMCFiniteSize.h"
#include "OhmmsData/AttributeSet.h"
#include "QMCWaveFunctions/WaveFunctionComponentBuilder.h"
#include <iostream>
#include <cmath>
#include "Configuration.h"
#include "einspline/bspline_eval_d.h"
#include "einspline/nubspline_eval_d.h"
#include "einspline/nugrid.h"
#include "einspline/nubspline_create.h"

namespace qmcplusplus
{
QMCFiniteSize::QMCFiniteSize() : skparser(NULL), ptclPool(NULL), myRcut(0.0), myConst(0.0), P(NULL), h(0.0)
{
  IndexType mtheta = 10;
  IndexType mphi   = 10;
  app_log() << "Building spherical grid. n_theta x n_phi = " << mtheta << " x " << mphi << endl;
  build_spherical_grid(mtheta, mphi);
  h = 0.00001;
}

QMCFiniteSize::QMCFiniteSize(SkParserBase* skparser_i)
    : skparser(skparser_i), ptclPool(NULL), myRcut(0.0), myConst(0.0), P(NULL), h(0.0), sphericalgrid(0)
{
  mtheta = 80;
  mphi   = 80;
  h      = 0.00001;
  build_spherical_grid(mtheta,mphi);
}

bool QMCFiniteSize::validateXML()
{
  xmlXPathContextPtr m_context = XmlDocStack.top()->getXPathContext();
  xmlNodePtr cur               = XmlDocStack.top()->getRoot()->children;

  while (cur != NULL)
  {
    std::string cname((const char*)cur->name);
    bool inputnode = true;
    if (cname == "particleset")
    {
      ptclPool.put(cur);
    }
    else if (cname == "wavefunction")
    {
      wfnPut(cur);
    }
    else if (cname == "include")
    {
      //file is provided
      const xmlChar* a = xmlGetProp(cur, (const xmlChar*)"href");
      if (a)
      {
        pushDocument((const char*)a);
        inputnode = processPWH(XmlDocStack.top()->getRoot());
        popDocument();
      }
    }
    else if (cname == "qmcsystem")
    {
      processPWH(cur);
    }
    else
    {}
    cur = cur->next;
  }

  app_log() << "=========================================================\n";
  app_log() << " Summary of QMC systems \n";
  app_log() << "=========================================================\n";
  ptclPool.get(app_log());
}


bool QMCFiniteSize::wfnPut(xmlNodePtr cur)
{
  std::string id("psi0"), target("e"), role("extra");
  OhmmsAttributeSet pAttrib;
  pAttrib.add(id, "id");
  pAttrib.add(id, "name");
  pAttrib.add(target, "target");
  pAttrib.add(target, "ref");
  pAttrib.add(role, "role");
  pAttrib.put(cur);
  ParticleSet* qp = ptclPool.getParticleSet(target);

  { //check ESHDF should be used to initialize both target and associated ionic system
    xmlNodePtr tcur = cur->children;
    while (tcur != NULL)
    { //check <determinantset/> or <sposet_builder/> to extract the ionic and electronic structure
      std::string cname((const char*)tcur->name);
      if (cname == WaveFunctionComponentBuilder::detset_tag || cname == "sposet_builder")
      {
        qp = ptclPool.createESParticleSet(tcur, target, qp);
      }
      tcur = tcur->next;
    }
  }
}


void QMCFiniteSize::getSkInfo(UBspline_3d_d* spline, vector<RealType>& symmatelem)
{
  symmatelem.resize(6);
  FullPrecRealType sx(0), sy(0), sz(0), sxy(0), sxz(0), syz(0);
  RealType h2 = h * h;

  PosType disp;
  PosType disp_lat;

  disp[0]  = h;
  disp[1]  = 0;
  disp[2]  = 0;
  disp_lat = P->Lattice.k_unit(disp);
  eval_UBspline_3d_d(spline, disp_lat[0], disp_lat[1], disp_lat[2], &sx);

  disp[0]  = 0;
  disp[1]  = h;
  disp[2]  = 0;
  disp_lat = P->Lattice.k_unit(disp);
  eval_UBspline_3d_d(spline, disp_lat[0], disp_lat[1], disp_lat[2], &sy);

  disp[0]  = 0;
  disp[1]  = 0;
  disp[2]  = h;
  disp_lat = P->Lattice.k_unit(disp);
  eval_UBspline_3d_d(spline, disp_lat[0], disp_lat[1], disp_lat[2], &sz);

  disp[0]  = h;
  disp[1]  = h;
  disp[2]  = 0;
  disp_lat = P->Lattice.k_unit(disp);
  eval_UBspline_3d_d(spline, disp_lat[0], disp_lat[1], disp_lat[2], &sxy);

  disp[0]  = h;
  disp[1]  = 0;
  disp[2]  = h;
  disp_lat = P->Lattice.k_unit(disp);
  eval_UBspline_3d_d(spline, disp_lat[0], disp_lat[1], disp_lat[2], &sxz);

  disp[0]  = 0;
  disp[1]  = h;
  disp[2]  = h;
  disp_lat = P->Lattice.k_unit(disp);
  eval_UBspline_3d_d(spline, disp_lat[0], disp_lat[1], disp_lat[2], &syz);

  symmatelem[0] = RealType(sx) / h2;
  symmatelem[1] = RealType(sy) / h2;
  symmatelem[2] = RealType(sz) / h2;
  symmatelem[3] = 0.5 * RealType(sxy - sx - sy) / h2;
  symmatelem[4] = 0.5 * RealType(sxz - sx - sz) / h2;
  symmatelem[5] = 0.5 * RealType(syz - sy - sz) / h2;
}


QMCFiniteSize::RealType QMCFiniteSize::sphericalAvgSk(UBspline_3d_d* spline, RealType k)
{
  RealType sum         = 0.0;
  FullPrecRealType val = 0.0;
  PosType kvec(0);
  IndexType ngrid = sphericalgrid.size();
  for (IndexType i = 0; i < ngrid; i++)
  {
    kvec = P->Lattice.k_unit(k * sphericalgrid[i]); // to reduced coordinates
    bool inx = true;
    bool iny = true;
    bool inz = true;
    if (kvec[0] <= gridx.lower_bound || kvec[0] >= gridx.upper_bound )
      inx = false;
    if (kvec[1] <= gridy.lower_bound || kvec[1] >= gridy.upper_bound )
      iny = false;
    if (kvec[2] <= gridz.lower_bound || kvec[2] >= gridz.upper_bound )
      inz = false;
    if ( !(inx & iny & inz) ) sum += 1;
    else
    {
      eval_UBspline_3d_d(spline, kvec[0], kvec[1], kvec[2], &val);
      sum += RealType(val);
    }
  }

  return sum / RealType(ngrid);
}


bool QMCFiniteSize::processPWH(xmlNodePtr cur)
{
  //return true and will be ignored
  if (cur == NULL)
    return true;
  bool inputnode = true;
  //save the root to grep @tilematrix
  xmlNodePtr cur_root = cur;
  cur                 = cur->children;
  while (cur != NULL)
  {
    std::string cname((const char*)cur->name);
    if (cname == "simulationcell")
    {
      ptclPool.putLattice(cur);
    }
    else if (cname == "particleset")
    {
      ptclPool.putTileMatrix(cur_root);
      ptclPool.put(cur);
    }
    else if (cname == "wavefunction")
    {
      wfnPut(cur);
    }
    else
    {}
    cur = cur->next;
  }
  return inputnode;
}


void QMCFiniteSize::initBreakup()
{
  app_log() << "=========================================================\n";
  app_log() << " Initializing Long Range Breakup (Esler) \n";
  app_log() << "=========================================================\n";
  P      = ptclPool.getParticleSet("e");
  AA     = LRCoulombSingleton::getHandler(*P);
  myRcut = AA->get_rc();
  if (rVs == 0)
  {
    rVs = LRCoulombSingleton::createSpline4RbyVs(AA, myRcut, myGrid);
  }
}


UBspline_3d_d* QMCFiniteSize::getSkSpline(RealType limit)
{
  KContainer Klist = P->SK->KLists;

  vector<TinyVector<int, OHMMS_DIM>> kpts = Klist.kpts;

  vector<RealType> sk(kpts.size()), skerr(kpts.size());

  skparser->get_sk(sk, skerr);
  if (skparser->is_normalized() == false)
  {
    IndexType Ne = P->getTotalNum();
    for (IndexType i = 0; i < sk.size(); i++)
    {
      sk[i] /= RealType(Ne);
      skerr[i] /= RealType(Ne);
    }
  }

  skparser->get_grid(gridx, gridy, gridz);

  Ugrid esgridx;
  Ugrid esgridy;
  Ugrid esgridz;


  //get the einspline grids.
  esgridx = gridx.einspline_grid();
  esgridy = gridy.einspline_grid();
  esgridz = gridz.einspline_grid();

  //setup the einspline boundary conditions.
  BCtype_d bcx;
  BCtype_d bcy;
  BCtype_d bcz;


  //This piece iterates through S(k) and sets
  //pieces beyond the k-cutoff equal to 1.
  //A violent approximation if S(k) is not converged, but
  //better than S(k)=0.
  double kc     = AA->get_kc();
  double kcutsq = kc * kc;

  for (int i = int(gridx.lower_bound), skindex = 0; i <= int(gridx.upper_bound); i++)
    for (int j = int(gridy.lower_bound); j <= int(gridy.upper_bound); j++)
      for (int k = int(gridz.lower_bound); k <= int(gridz.upper_bound); k++)
      {
        PosType v;
        v[0]         = i;
        v[1]         = j;
        v[2]         = k;
        RealType ksq = P->Lattice.ksq(v);

        if (ksq > kcutsq)
          sk[skindex] = limit;
        skindex++;
      }
  //No particular BC's on the edge of S(k).

  bcx.lCode = NATURAL;
  bcx.rCode = NATURAL;
  bcx.lVal  = 1.0;
  bcx.rVal  = 1.0;

  bcy.lCode = NATURAL;
  bcy.rCode = NATURAL;
  bcy.lVal  = 1.0;
  bcy.rVal  = 1.0;

  bcz.lCode = NATURAL;
  bcz.rCode = NATURAL;
  bcz.lVal  = 1.0;
  bcz.rVal  = 1.0;

  //hack for QMC_MIXED_PRECISION to interface to UBspline_3d_d
  vector<FullPrecRealType> sk_fp(sk.begin(), sk.end());
  UBspline_3d_d* spline = create_UBspline_3d_d(esgridx, esgridy, esgridz, bcx, bcy, bcz, sk_fp.data());

  return spline;
}


void QMCFiniteSize::build_spherical_grid(IndexType mtheta, IndexType mphi)
{
  //Spherical grid from https://www.cmu.edu/biolphys/deserno/pdf/sphere_equi.pdf
  RealType alpha = 4.0 * M_PI / (mtheta * mphi);
  RealType d = std::sqrt(alpha);
  RealType Mt = int(std::round(M_PI / d));
  RealType Dt = M_PI / Mt;
  RealType Dp = alpha / Dt;
  int count = 0; 
  for (int m = 0; m < Mt; m++)
  {
    RealType theta = M_PI * (m + 0.5) / Mt;
    RealType Mp = int(std::round(2 * M_PI * std::sin(theta) / Dp));
    for (int n = 0; n < Mp; n++)
    {
      IndexType gindex = m * mtheta + n;
      RealType phi = 2 * M_PI * n / Mp;
      PosType tmp;
      tmp[0] = std::sin(theta)*std::cos(phi);
      tmp[1] = std::sin(theta)*std::sin(phi);
      tmp[2] = std::cos(theta);
      sphericalgrid.push_back(tmp);
    }
  }

}


NUBspline_1d_d* QMCFiniteSize::spline_clamped(vector<RealType>& grid,
                                              vector<RealType>& vals,
                                              RealType lVal,
                                              RealType rVal)
{
  //hack to interface to NUgrid stuff in double prec for MIXED build
  vector<FullPrecRealType> grid_fp(grid.begin(), grid.end());
  NUgrid* grid1d = create_general_grid(grid_fp.data(), grid_fp.size());

  BCtype_d xBC;
  xBC.lVal  = lVal;
  xBC.rVal  = rVal;
  xBC.lCode = DERIV1;
  xBC.rCode = DERIV1;
  //hack to interface to NUgrid stuff in double prec for MIXED build
  vector<FullPrecRealType> vals_fp(vals.begin(), vals.end());
  return create_NUBspline_1d_d(grid1d, xBC, vals_fp.data());
}


//Integrate the spline using Simpson's 5/8 rule.  For Bsplines, this should be exact
//provided your delta is smaller than the smallest bspline mesh spacing.
// JPT 13/03/2018 - Fixed an intermittant segfault that occurred b/c
//                  eval_NUB_spline_1d_d sometimes went out of bounds.
QMCFiniteSize::RealType QMCFiniteSize::integrate_spline(NUBspline_1d_d* spline, RealType a, RealType b, IndexType N)
{
  if (N % 2 != 0) // if N odd, warn that destruction is imminent
  {
    cerr << "Warning in integrate_spline: N must be even!\n";
    N = N - 1; // No risk of overflow
  }

  RealType eps         = (b - a) / RealType(N);
  RealType sum         = 0.0;
  FullPrecRealType tmp = 0.0; //hack to interface to NUBspline_1d_d
  RealType xi          = 0.0;
  for (int i = 1; i < N / 2; i++)
  {
    xi = a + (2 * i - 2) * eps;
    eval_NUBspline_1d_d(spline, xi, &tmp);
    sum += RealType(tmp);

    xi = a + (2 * i - 1) * eps;
    eval_NUBspline_1d_d(spline, xi, &tmp);
    sum += 4 * tmp;

    xi = a + (2 * i) * eps;
    eval_NUBspline_1d_d(spline, xi, &tmp);
    sum += tmp;
  }

  return (eps / 3.0) * sum;
}


bool QMCFiniteSize::execute()
{
  //Initialize the long range breakup.  For now, do the Esler method.
  initBreakup();
  KContainer Klist                        = P->SK->KLists;
  vector<TinyVector<int, OHMMS_DIM>> kpts = Klist.kpts; //These are in reduced coordinates.
                                                        //Easier to spline, but will have to convert
                                                        //for real space integration.

  vector<RealType> sk(kpts.size()), skerr(kpts.size());

  if (!skparser->has_grid())
    skparser->set_grid(kpts);

  cout << "Grid computed.\n";

  sk    = skparser->get_sk_raw();
  skerr = skparser->get_skerr_raw();

  IndexType Ne = P->getTotalNum();
  if (skparser->is_normalized() == false)
  {
    for (int i = 0; i < sk.size(); i++)
    {
      sk[i] /= RealType(Ne);
      skerr[i] /= RealType(Ne);
    }
  }
  //This is the \frac{1}{Omega} \sum_{\mathbf{k}} \frac{v_k}{2} S(\mathbf{k}) term.
  RealType V = 0.5 * AA->evaluate_w_sk(Klist.kshell, sk.data());
  vector<RealType> vsk_1d(Klist.kshell.size());

  // Average within each shell
  for (int ks = 0; ks < Klist.kshell.size() - 1; ks++)
  {
    RealType u = 0;
    RealType n = 0;
    for (int ki = Klist.kshell[ks]; ki < Klist.kshell[ks + 1]; ki++)
    {
      u += sk[ki];
      n++;
    }
    if (n != 0)
    {
      vsk_1d[ks] = u / n;
    }
    else
    {
      vsk_1d[ks] = 0;
    }
  }

  app_log() << fixed;
  app_log() << "\nSpherically averaged raw S(k):\n";
  app_log() << setw(12) << "k" << setw(12) << "S(k)" << setw(12) << "vk"
            << "\n";
  for (int ks = 0; ks < Klist.kshell.size() - 1; ks++)
  {
    app_log() << setw(12) << setprecision(8) << std::sqrt(Klist.ksq[Klist.kshell[ks]]) << setw(12) << setprecision(8)
              << vsk_1d[ks] << setw(12) << setprecision(8) << AA->Fk_symm[ks] << "\n";
  }

  if (vsk_1d[Klist.kshell.size()-2] < 0.99)
  {
    app_log() << "####################################################################\n";
    app_log() << "WARNING: The S(k) in the largest kshell is less than 0.99\n";
    app_log() << "         This code assumes the S(k) is converged to 1.0 at large k\n";
    app_log() << "         You may need to rerun with a larger LR_dim_cutoff\n";
    app_log() << "####################################################################\n";
  }

  UBspline_3d_d* sk3d_spline = getSkSpline();

  vector<RealType> Amat;
  getSkInfo(sk3d_spline, Amat);

  app_log() << "\n=========================================================\n";
  app_log() << " S(k) Info \n";
  app_log() << "=========================================================\n";
  app_log() << "S(k) anisotropy near k=0\n";
  app_log() << "------------------------\n";
  app_log() << "  a_xx = " << Amat[0] << endl;
  app_log() << "  a_yy = " << Amat[1] << endl;
  app_log() << "  a_zz = " << Amat[2] << endl;
  app_log() << "  a_xy = " << Amat[3] << endl;
  app_log() << "  a_xz = " << Amat[4] << endl;
  app_log() << "  a_yz = " << Amat[5] << endl;
  app_log() << "------------------------\n";

  RealType b = (Amat[0] + Amat[1] + Amat[2]) / 3.0;

  app_log() << "Spherically averaged S(k) near k=0\n";
  app_log() << "S(k)=b*k^2   b = " << b << endl;
  app_log() << "------------------------\n";
  app_log() << endl;

  RealType kmax = AA->get_kc();
  RealType nk   = 100;
  RealType kdel = kmax / (nk - 1.0);

  app_log() << "\nSpherically averaged splined S(k):\n";
  app_log() << setw(12) << "k" << setw(12) << "S(k)"
            << "\n";
  for (int k = 0; k < nk; k++)
  {
    RealType kval = kdel * k;
    app_log() << setw(12) << setprecision(8) << kval << setw(12) << setprecision(8) << sphericalAvgSk(sk3d_spline, kval)
              << "\n";
  }

  IndexType ngrid = Klist.kshell.size() - 1;
  vector<RealType> nonunigrid1d(ngrid + 2); //The +2 includes the k=0 point and the k=kmax point.
  nonunigrid1d[0]         = 0.0;
  nonunigrid1d[ngrid + 1] = kmax;

  vector<RealType> k2vksk(ngrid + 2);
  k2vksk[0]         = 0.0;
  k2vksk[ngrid + 1] = 0.0;

  for (int ks = 0; ks < ngrid; ks++)
  {
    RealType kval        = std::sqrt(Klist.ksq[Klist.kshell[ks]]);
    nonunigrid1d[ks + 1] = kval;
    RealType skavg       = sphericalAvgSk(sk3d_spline, kval);
    RealType vk          = AA->Fk_symm[ks];
    k2vksk[ks + 1]       = 0.5 * vk * skavg * kval * kval;
  }

  NUBspline_1d_d* integrand = spline_clamped(nonunigrid1d, k2vksk, 0.0, 0.0);

  //Integrate the spline and compute the thermodynamic limit.
  RealType integratedval = integrate_spline(integrand, 0.0, kmax, 100);
  RealType intnorm       = P->Lattice.Volume / 2.0 / M_PI / M_PI; //The volume factor here is because 1/Vol is
                                                                  //included in QMCPACK's v_k.  See CoulombFunctor.

  // Here are the fsc corrections to potential
  RealType rho     = RealType(Ne) / P->Lattice.Volume;
  RealType rs      = std::pow(3.0 / (4 * M_PI) * P->Lattice.Volume / RealType(Ne), 1.0 / 3.0);
  RealType vlo     = 2 * M_PI * rho * b / RealType(Ne);
  RealType vint    = intnorm * integratedval;
  RealType vfscorr = vint - V;
  RealType tlo     = 1.0 / RealType(Ne * b * 8);

  app_log() << "\n=========================================================\n";
  app_log() << " Finite Size Corrections:\n";
  app_log() << "=========================================================\n";
  app_log() << " System summary:\n";
  app_log() << fixed;
  app_log() << "  Nelec = " << setw(12) << Ne << "\n";
  app_log() << "  Vol   = " << setw(12) << setprecision(8) << P->Lattice.Volume << " [a0^3]\n";
  app_log() << "  Ne/V  = " << setw(12) << setprecision(8) << rho << " [1/a0^3]\n";
  app_log() << "  rs/a0 = " << setw(12) << setprecision(8) << rs << "\n";
  app_log() << "\n";
  app_log() << " Leading Order Corrections:\n";
  app_log() << "  V_LO = " << setw(12) << setprecision(8) << vlo << " [Ha/electron], " << vlo * Ne << " [Ha]\n";
  app_log() << "  T_LO = " << setw(12) << setprecision(8) << tlo << " [Ha/electron], " << tlo * Ne << " [Ha]\n";
  app_log() << "  NB: This is a crude estimate of the kinetic energy correction!\n";
  app_log() << "\n";
  app_log() << " Beyond Leading Order (Integrated corrections):\n";
  app_log() << "  V_Int = " << setw(12) << setprecision(8) << vfscorr << " [Ha/electron], " << vfscorr * Ne
            << " [Ha]\n";
}

} // namespace qmcplusplus
