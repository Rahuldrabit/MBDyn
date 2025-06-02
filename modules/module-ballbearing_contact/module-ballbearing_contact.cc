/* $Header$ */
/*
 * MBDyn (C) is a multibody analysis code.
 * http://www.mbdyn.org
 *
 * Copyright (C) 1996-2023
 *
 * Pierangelo Masarati  <pierangelo.masarati@polimi.it>
 * Paolo Mantegazza     <paolo.mantegazza@polimi.it>
 *
 * Dipartimento di Ingegneria Aerospaziale - Politecnico di Milano
 * via La Masa, 34 - 20156 Milano, Italy
 * http://www.aero.polimi.it
 *
 * Changing this copyright notice is forbidden.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation (version 2 of the License).
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
  AUTHOR: Reinhard Resch <mbdyn-user@a1.net>
  Copyright (C) 2023(-2023) all rights reserved.

  The copyright of this code is transferred
  to Pierangelo Masarati and Paolo Mantegazza
  for use in the software MBDyn as described
  in the GNU Public License version 2.1
*/

#include <limits>
#include <iostream>
#include <iomanip>
#include <cfloat>
#include <cassert>
#include <cmath>
#include <cstring>
#include <ctime>
#include <type_traits>

#ifdef HAVE_CONFIG_H
#include "mbconfig.h"           /* This goes first in every *.c,*.cc file */
#endif /* HAVE_CONFIG_H */

#include <dataman.h>
#include <userelem.h>

#include "module-ballbearing_contact.h"
#include <vector>
#include <strnodead.h>
#include <sp_gradient.h>
#include <sp_matrix_base.h>
#include <sp_matvecass.h>

using namespace sp_grad;

class BallBearingContact: public UserDefinedElem
{
     using Elem::AssRes;
public:
     BallBearingContact(unsigned uLabel, const DofOwner *pDO,
                        DataManager* pDM, MBDynParser& HP);
     virtual ~BallBearingContact(void);
     virtual void Output(OutputHandler& OH) const override;
     virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const override;
     VariableSubMatrixHandler&
     AssJac(VariableSubMatrixHandler& WorkMat,
            doublereal dCoef,
            const VectorHandler& XCurr,
            const VectorHandler& XPrimeCurr) override;
     virtual void
     AssJac(VectorHandler& JacY,
            const VectorHandler& Y,
            doublereal dCoef,
            const VectorHandler& XCurr,
            const VectorHandler& XPrimeCurr,
            VariableSubMatrixHandler& WorkMat) override;
     SubVectorHandler&
     AssRes(SubVectorHandler& WorkVec,
            doublereal dCoef,
            const VectorHandler& XCurr,
            const VectorHandler& XPrimeCurr) override;
     template <typename T>
     inline void
     AssRes(SpGradientAssVec<T>& WorkVec,
            doublereal dCoef,
            const SpGradientVectorHandler<T>& XCurr,
            const SpGradientVectorHandler<T>& XPrimeCurr,
            enum SpFunctionCall func);
     virtual void
     AfterConvergence(const VectorHandler& X,
                      const VectorHandler& XP) override;
     virtual unsigned int iGetNumPrivData(void) const override;
     virtual unsigned int iGetPrivDataIdx(const char *s) const override;
     virtual doublereal dGetPrivData(unsigned int i) const override;
     int iGetNumConnectedNodes(void) const;
     virtual void GetConnectedNodes(std::vector<const Node *>& connectedNodes) const override;
     virtual void SetValue(DataManager *pDM, VectorHandler& X, VectorHandler& XP,
                           SimulationEntity::Hints *ph) override;
     virtual std::ostream& Restart(std::ostream& out) const override;
     virtual unsigned int iGetNumDof(void) const override;
     virtual std::ostream& DescribeDof(std::ostream& out,
                                       const char *prefix, bool bInitial) const override;
     virtual std::ostream& DescribeEq(std::ostream& out,
                                      const char *prefix, bool bInitial) const override;
     virtual DofOrder::Order GetDofType(unsigned int) const override;
     virtual DofOrder::Order GetEqType(unsigned int i) const override;
     virtual unsigned int iGetInitialNumDof(void) const override;
     virtual void
     InitialWorkSpaceDim(integer* piNumRows, integer* piNumCols) const override;
     virtual VariableSubMatrixHandler&
     InitialAssJac(VariableSubMatrixHandler& WorkMat,
                   const VectorHandler& XCurr) override;
     virtual SubVectorHandler&
     InitialAssRes(SubVectorHandler& WorkVec, const VectorHandler& XCurr) override;

private:
     const DataManager* const pDM;
     const StructNodeAd* pNode1;
     doublereal R;
     doublereal k;
     typedef SpMatrixA<doublereal, 2, 2> Mat2x2;
     Mat2x2 Mk, Mk2, inv_Mk, inv_Mk_sigma0, Ms, Ms2, sigma0, sigma1;
     doublereal gamma, vs, beta;
     doublereal dStictionStateEquScale;
     doublereal dStictionStateDofScale;
     doublereal tPrev;
     doublereal tCurr;
     doublereal dtMax;
     doublereal dFMax;
     doublereal dFMin;
     bool bEnableFriction;
     bool bFirstRes;

     struct Washer {
          Washer()
               : pNode2(0),
                 dPrev(0),
                 dCurr(0),
                 dd_dtPrev(0),
                 dd_dtCurr(0),
                 FnPrev(0),
                 FnCurr(0)
               {

               }
          const StructNodeAd* pNode2;
          SpMatrixA<doublereal, 3, 3> Rt2;
          SpColVectorA<doublereal, 3> o2;
          doublereal dPrev;
          doublereal dCurr;
          doublereal dd_dtPrev;
          doublereal dd_dtCurr;
          doublereal FnPrev;
          doublereal FnCurr;
          SpColVectorA<doublereal, 2> z;
          SpColVectorA<doublereal, 2> zP;
          SpColVectorA<doublereal, 2> uP;
          SpColVectorA<doublereal, 2> tau;
     };

     std::vector<Washer> washers;

     inline void CheckTimeStep(Washer& w, doublereal Fn, doublereal d, doublereal dn_dt);
     static inline void CheckTimeStep(Washer& w, const SpGradient&, const SpGradient&, const SpGradient&){}
     static inline void CheckTimeStep(Washer& w, const GpGradProd&, const GpGradProd&, const GpGradProd&){}
     
     static const int iNumPrivData = 11;

     static const struct PrivData {
          char szPattern[9];
     } rgPrivData[iNumPrivData];
};

const struct BallBearingContact::PrivData
BallBearingContact::rgPrivData[iNumPrivData] = {
     {"d[%u]"},
     {"dP[%u]"},
     {"F[%u]"},
     {"z1[%u]"},
     {"z2[%u]"},
     {"zP1[%u]"},
     {"zP2[%u]"},
     {"uP1[%u]"},
     {"uP2[%u]"},
     {"tau1[%u]"},
     {"tau2[%u]"}
};

BallBearingContact::BallBearingContact(
     unsigned uLabel, const DofOwner *pDO,
     DataManager* pDM, MBDynParser& HP)
     :       UserDefinedElem(uLabel, pDO),
             pDM(pDM),
             pNode1(0),
             gamma(0.),
             vs(0.),
             beta(0.),
             dStictionStateEquScale(0.),
             dStictionStateDofScale(0.),
             tPrev(-std::numeric_limits<doublereal>::max()),
             tCurr(-std::numeric_limits<doublereal>::max()),
             dtMax(std::numeric_limits<doublereal>::max()),
             dFMax(std::numeric_limits<doublereal>::max()),
             dFMin(0.),
             bEnableFriction(false),
             bFirstRes(true)
{
     // help
     if (HP.IsKeyWord("help")) {
          silent_cout(
               "\n"
               "Module:        ballbearing contact\n"
               "\n"
               "       ball bearing contact,\n"
               "               ball, (label) <node1>,\n"
               "               ball radius, (Scalar) <R>,\n"
               "               washers, (Integer) <N>,\n"
               "                       (label) <node2>,\n"
               "                       [offset, (Vec3) <o2>,]\n"
               "                       [orientation, (OrientationMatrix) <Rt2>,\n"
               "                       ...\n"
               "               {elastic modulus, <E> |\n"
               "                elastic modulus ball, (Scalar) <E1>,\n"
               "                poisson ratio ball, (Scalar) <nu1>\n"
               "                elastic modulus washer, (Scalar) <E2>,\n"
               "                poisson ratio washer, (Scalar) <nu2>},\n"
               "                [damping factor, (Scalar) <ks>,]\n"
               "               {coulomb friction coefficient, (Scalar) <mu> |\n"
               "                coulomb friction coefficient x, (Scalar) <mux>,\n"
               "                coulomb friction coefficient y, (Scalar) <muy>},\n"
               "               [{static friction coefficient, (Scalar) <mus> |\n"
               "                 static friction coefficient x, (Scalar) <musx>,\n"
               "                 static friction coefficient y, (Scalar) <musy>},]\n"
               "               [sliding velocity coefficient, (Scalar) <vs>,]\n"
               "               [sliding velocity exponent, (Scalar) <gamma>,]\n"
               "               {micro slip stiffness, (Scalar) <sigma0> |\n"
               "                micro slip stiffness x, (Scalar) <sigma0x>,\n"
               "                micro slip stiffness y, (Scalar) <sigma0y>}\n"
               "               [,{micro slip damping, (Scalar) <sigma1>, |\n"
               "                  micro slip damping x, (Scalar) <sigma1x>,\n"
               "                  micro slip damping y, (Scalar) <sigma1y>}]\n"
               << std::endl);

          if (!HP.IsArg()) {
               /*
                * Exit quietly if nothing else is provided
                */
               throw NoErr(MBDYN_EXCEPT_ARGS);
          }
     }

     if (!HP.IsKeyWord("ball")) {
          silent_cerr("ball bearing contact" << GetLabel()
                      << "): keyword \"ball\" expected at line "
                      << HP.GetLineData() << std::endl);
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     pNode1 = pDM->ReadNode<const StructNodeAd, Node::STRUCTURAL>(HP);

     if (!HP.IsKeyWord("ball" "radius") ) {
          silent_cerr("ball bearing contact(" << GetLabel()
                      << "): keyword \"ball radius\" expected at line "
                      << HP.GetLineData() << std::endl);

          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     R = HP.GetReal();

     if (!HP.IsKeyWord("washers")) {
          silent_cerr("ball bearing contact" << GetLabel()
                      << "): keyword \"washers\" expected at line "
                      << HP.GetLineData() << std::endl);
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     const integer N = HP.GetInt();

     washers.resize(N);

     for (auto& w: washers) {
          w.pNode2 = pDM->ReadNode<const StructNodeAd, Node::STRUCTURAL>(HP);

          if ( HP.IsKeyWord("offset") ) {
               w.o2 = HP.GetPosRel(ReferenceFrame(w.pNode2));
          } else {
               w.o2 = Zero3;
          }

          if (HP.IsKeyWord("orientation")) {
               w.Rt2 = HP.GetRotRel(ReferenceFrame(w.pNode2));
          } else {
               w.Rt2 = Eye3;
          }
     }

     doublereal E;

     if ( HP.IsKeyWord("elastic" "modulus") ) {
          E = HP.GetReal();
     } else {
          if ( !HP.IsKeyWord("elastic" "modulus" "ball") ) {
               silent_cerr("ball bearing contact(" << GetLabel()
                           << "): keyword \"elastic modulus\" or "
                           "\"elastic modulus ball\" expected at line "
                           << HP.GetLineData() << std::endl);

               throw ErrGeneric(MBDYN_EXCEPT_ARGS);
          }

          const doublereal E1 = HP.GetReal();

          if ( !HP.IsKeyWord("poisson" "ratio" "ball") ) {
               silent_cerr("ball bearing contact(" << GetLabel()
                           <<  "): keyword \"poisson ratio ball\" expected at line "
                           << HP.GetLineData() << std::endl);

               throw ErrGeneric(MBDYN_EXCEPT_ARGS);
          }

          const doublereal nu1 = HP.GetReal();

          if ( !HP.IsKeyWord("elastic" "modulus" "washer") ) {
               silent_cerr("ball bearing contact(" << GetLabel()
                           << "): keyword \"elastic modulus washer\" expected at line "
                           << HP.GetLineData() << std::endl);

               throw ErrGeneric(MBDYN_EXCEPT_ARGS);
          }

          const doublereal E2 = HP.GetReal();

          if ( !HP.IsKeyWord("poisson" "ratio" "washer") ) {
               silent_cerr("ball bearing contact(" << GetLabel()
                           << "): keyword \"poisson ratio washer\" expected at line "
                           << HP.GetLineData() << std::endl);

               throw ErrGeneric(MBDYN_EXCEPT_ARGS);
          }

          const doublereal nu2 = HP.GetReal();

          E = 1 / ((1 - nu1 * nu1) / E1 + (1 - nu2 * nu2) / E2);
     }

     k = 4./3. * E * sqrt(R);

     if (HP.IsKeyWord("damping" "factor")) {
          const doublereal ks = HP.GetReal();
          beta = 3./2. * ks * k;
     } else {
          beta = 0.;
     }

     if (HP.IsKeyWord("coulomb" "friction" "coefficient")
         || HP.IsKeyWord("coulomb" "friction" "coefficient" "x")) {
          bEnableFriction = true;

          const doublereal mukx = HP.GetReal();

          doublereal muky;

          if (HP.IsKeyWord("coulomb" "friction" "coefficient" "y")) {
               muky = HP.GetReal();
          } else {
               muky = mukx;
          }

          doublereal musx, musy;

          if (HP.IsKeyWord("static" "friction" "coefficient")
              || HP.IsKeyWord("static" "friction" "coefficient" "x")) {
               musx = HP.GetReal();

               if (HP.IsKeyWord("static" "friction" "coefficient" "y")) {
                    musy = HP.GetReal();
               } else {
                    musy = musx;
               }
          } else {
               musx = mukx;
               musy = muky;
          }

          if (HP.IsKeyWord("sliding" "velocity" "coefficient")) {
               vs = HP.GetReal();
          } else {
               vs = 1.;
          }

          if (HP.IsKeyWord("sliding" "velocity" "exponent")) {
               gamma = HP.GetReal();
          } else {
               gamma = 1.;
          }

          if (!(HP.IsKeyWord("micro" "slip" "stiffness") || HP.IsKeyWord("micro" "slip" "stiffness" "x"))) {
               silent_cerr("ball bearing contact("
                           << GetLabel()
                           << "): keyword \"micro slip stiffness\" or \"micro slip stiffness x\" expected at line "
                           << HP.GetLineData() << std::endl);

               throw ErrGeneric(MBDYN_EXCEPT_ARGS);
          }

          const doublereal sigma0x = HP.GetReal();

          doublereal sigma0y;

          if (HP.IsKeyWord("micro" "slip" "stiffness" "y")) {
               sigma0y = HP.GetReal();
          } else {
               sigma0y = sigma0x;
          }

          doublereal sigma1x, sigma1y;

          if (HP.IsKeyWord("micro" "slip" "damping") || HP.IsKeyWord("micro" "slip" "damping" "x")) {
               sigma1x = HP.GetReal();

               if (HP.IsKeyWord("micro" "slip" "damping" "y")) {
                    sigma1y = HP.GetReal();
               } else {
                    sigma1y = sigma1x;
               }
          } else {
               sigma1x = 0.;
               sigma1y = 0.;
          }

          Mk(1, 1) = mukx;
          Mk(2, 2) = muky;

          Mk2 = Mk * Mk;

          inv_Mk = Inv(Mk);

          Ms(1, 1) = musx;
          Ms(2, 2) = musy;

          Ms2 = Ms * Ms;

          sigma0(1, 1) = sigma0x;
          sigma0(2, 2) = sigma0y;

          sigma1(1, 1) = sigma1x;
          sigma1(2, 2) = sigma1y;

          sigma1 = Mat2x2(sigma1 * Inv(sigma0));

          inv_Mk_sigma0 = inv_Mk * sigma0;

          dStictionStateEquScale = HP.IsKeyWord("stiction" "state" "equation" "scale") ? HP.GetReal() : 1.;
          dStictionStateDofScale = HP.IsKeyWord("stiction" "state" "dof" "scale") ? HP.GetReal() : 1.;
     }

     if (HP.IsKeyWord("max" "force" "increment")) {
          dFMax = HP.GetReal();

          if (dFMax <= 0) {
               silent_cerr("ball bearing contact(" << GetLabel()
                           << "): max force increment must be greater than zero at line "
                           << HP.GetLineData() << std::endl);
               throw ErrGeneric(MBDYN_EXCEPT_ARGS);
          }
     }

     if (HP.IsKeyWord("min" "force" "increment")) {
          dFMin = HP.GetReal();

          if (dFMin <= 0) {
               silent_cerr("ball bearing contact(" << GetLabel()
                           << "): min force increment must be greater than zero at line "
                           << HP.GetLineData() << std::endl);
               throw ErrGeneric(MBDYN_EXCEPT_ARGS);
          }
     }

     SetOutputFlag(pDM->fReadOutput(HP, Elem::LOADABLE));
}

BallBearingContact::~BallBearingContact(void)
{

}

void
BallBearingContact::Output(OutputHandler& OH) const
{

}

void
BallBearingContact::WorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
     integer iNumDof = 12;

     if (bEnableFriction) {
          iNumDof += 2;
     }

     *piNumRows = iNumDof * washers.size();
     *piNumCols = 0;
}

VariableSubMatrixHandler&
BallBearingContact::AssJac(VariableSubMatrixHandler& WorkMat,
                           doublereal dCoef,
                           const VectorHandler& XCurr,
                           const VectorHandler& XPrimeCurr)
{
     SpGradientAssVec<SpGradient>::AssJac(this,
                                          WorkMat.SetSparseGradient(),
                                          dCoef,
                                          XCurr,
                                          XPrimeCurr,
                                          REGULAR_JAC);
     return WorkMat;
}

void
BallBearingContact::AssJac(VectorHandler& JacY,
                           const VectorHandler& Y,
                           doublereal dCoef,
                           const VectorHandler& XCurr,
                           const VectorHandler& XPrimeCurr,
                           VariableSubMatrixHandler& WorkMat)
{
     using namespace sp_grad;
     
     SpGradientAssVec<GpGradProd>::AssJac(this,
                                          JacY,
                                          Y,
                                          dCoef,
                                          XCurr,
                                          XPrimeCurr,
                                          SpFunctionCall::REGULAR_JAC);
}

SubVectorHandler&
BallBearingContact::AssRes(SubVectorHandler& WorkVec,
                           doublereal dCoef,
                           const VectorHandler& XCurr,
                           const VectorHandler& XPrimeCurr)
{
     SpGradientAssVec<doublereal>::AssRes(this,
                                          WorkVec,
                                          dCoef,
                                          XCurr,
                                          XPrimeCurr,
                                          REGULAR_RES);

     return WorkVec;
}

template <typename T>
inline void BallBearingContact::AssRes(SpGradientAssVec<T>& WorkVec,
                                       doublereal dCoef,
                                       const SpGradientVectorHandler<T>& XCurr,
                                       const SpGradientVectorHandler<T>& XPrimeCurr,
                                       enum SpFunctionCall func)
{
     SpColVector<T, 3> X1(3, 1), X1P(3, 1), omega1(3, 3);
     SpMatrix<T, 3, 3> R2(3, 3, 3);
     SpColVector<T, 3> X2(3, 1), X2P(3, 1), omega2(3, 3);
     SpColVector<T, 2> z(2, 1), zP(2, 1);
     SpColVector<T, 2> tau(2, 14), Phi(2, 14);
     T norm_Fn, kappa;

     integer offset = std::numeric_limits<integer>::min();
     const integer iFirstIndex = bEnableFriction ? iGetFirstIndex() : std::numeric_limits<integer>::min();
     const integer iFirstMomIndexNode1 = pNode1->iGetFirstMomentumIndex();
     dtMax = std::numeric_limits<doublereal>::max();

     pNode1->GetXCurr(X1, dCoef, func);
     pNode1->GetVCurr(X1P, dCoef, func);
     pNode1->GetWCurr(omega1, dCoef, func);
     
     for (auto& w: washers) {
          w.pNode2->GetXCurr(X2, dCoef, func);
          w.pNode2->GetVCurr(X2P, dCoef, func);
          w.pNode2->GetRCurr(R2, dCoef, func);
          w.pNode2->GetWCurr(omega2, dCoef, func);

          if (bEnableFriction) {
               offset = 2 * (&w - &washers.front());

               for (index_type j = 1; j <= 2; ++j) {
                    XCurr.dGetCoef(iFirstIndex + j + offset, z(j), dCoef);
                    XPrimeCurr.dGetCoef(iFirstIndex + j + offset, zP(j), 1.);
               }

               z *= dStictionStateDofScale;
               zP *= dStictionStateDofScale;

               for (index_type j = 1; j <= 2; ++j) {
                    w.z(j) = SpGradientTraits<T>::dGetValue(z(j));
                    w.zP(j) = SpGradientTraits<T>::dGetValue(zP(j));
               }
          }

          const SpColVector<T, 3> dX = X1 - X2;

          SpColVector<T, 3> v = Transpose(w.Rt2) * (Transpose(R2) * dX - w.o2);

          const T n = v(3);

          SpGradientTraits<T>::ResizeReset(v(3), 0., 0);

          const T dn_dt = Dot(R2 * w.Rt2.GetCol(3), X1P - X2P - Cross(omega2, dX));
          const T d = R - n;

          if (R > n) {
               norm_Fn = k * pow(d, 3./2.) - beta * sqrt(d) * dn_dt;
          } else {
               SpGradientTraits<T>::ResizeReset(norm_Fn, 0., 0);
          }

          if (norm_Fn < 0.) {
               SpGradientTraits<T>::ResizeReset(norm_Fn, 0., 0);
          }

          CheckTimeStep(w, norm_Fn, d, dn_dt);

          if (bEnableFriction) {
               const SpColVector<T, 3> c1P = X1P - Cross(omega1, R2 * (w.Rt2.GetCol(3) * R));
               const SpColVector<T, 3> c2P = X2P + Cross(omega2, R2 * (w.o2 + w.Rt2 * v));

               SpColVector<T, 3> l = v;
               SpGradientTraits<T>::ResizeReset(l(3), R, 0);
               
               const SpColVector<T, 3> Deltac = X1 - X2 - R2 * (w.o2 + w.Rt2 * l);

               const SpColVector<T, 2> uP = Transpose(SubMatrix<1, 1, 3, 1, 1, 2>(w.Rt2)) * (Transpose(R2) * (c1P - c2P - Cross(omega2, Deltac)));

               tau = Mk * (z + sigma1 * zP) * norm_Fn;

               if constexpr (std::is_same<T, doublereal>::value) {
                    for (int j = 1; j <= 2; ++j) {
                         w.uP(j) = SpGradientTraits<T>::dGetValue(uP(j));
                         w.tau(j) = SpGradientTraits<T>::dGetValue(tau(j));
                    }
               }

               const T Norm_uP2 = Dot(uP, uP);

               if (Norm_uP2 == 0.) {
                    SpGradientTraits<T>::ResizeReset(kappa, 0., 0);
               } else {
                    const T a0 = Norm(Mk2 * uP);
                    const T a1 = a0 / Norm(Mk * uP);
                    const T g = a1 + (Norm(Ms2 * uP) / Norm(Ms * uP) - a1) * exp(-pow(sqrt(Norm_uP2) / vs, gamma));

                    kappa = a0 / g;
               }

               Phi = (inv_Mk_sigma0 * (uP - inv_Mk * z * kappa) - zP) * dStictionStateEquScale;
          }

          const SpColVector<T, 3> R2_Rt2_e3_n = R2 * (w.Rt2.GetCol(3) * n);

          const SpColVector<T, 3> F1 = R2 * (w.Rt2 * SpColVector<T, 3>{-tau(1), -tau(2), norm_Fn});
          const SpColVector<T, 3> M1 = -Cross(R2_Rt2_e3_n, F1);
          const SpColVector<T, 3> F2 = -F1;
          const SpColVector<T, 3> M2 = Cross((dX - R2_Rt2_e3_n), F2);

          const integer iFirstMomIndexNode2 = w.pNode2->iGetFirstMomentumIndex();

          WorkVec.AddItem(iFirstMomIndexNode1 + 1, F1);
          WorkVec.AddItem(iFirstMomIndexNode1 + 4, M1);
          WorkVec.AddItem(iFirstMomIndexNode2 + 1, F2);
          WorkVec.AddItem(iFirstMomIndexNode2 + 4, M2);

          if (bEnableFriction) {
               for (integer j = 1; j <= 2; ++j) {
                    WorkVec.AddItem(iFirstIndex + j + offset, Phi(j));
               }
          }
     }
}

inline void BallBearingContact::CheckTimeStep(Washer& w, doublereal Fn, doublereal d, doublereal dn_dt)
{
     tCurr = pDM->dGetTime();
     w.dCurr = d;
     w.dd_dtCurr = -dn_dt;
     w.FnCurr = Fn;

     if (bFirstRes) {
          bFirstRes = false;
          return;
     }

     if (w.FnPrev < dFMin && w.FnCurr < dFMin) {
          // Bypass time step control
          // because the force is considered
          // to be too small!
          return;
     }

     doublereal a;

     if ((w.dCurr > 0 && w.dPrev < 0) || (w.dCurr < 0 && w.dPrev > 0)) {
          a = 0.;
     } else if (w.dCurr >= 0. && w.dPrev >= 0.) {
          const doublereal FnCurr = k * pow(w.dCurr, 3./2.);
          const doublereal FnPrev = k * pow(w.dPrev, 3./2.);
          const doublereal b = (FnPrev + copysign(dFMax, FnCurr - FnPrev)) / k;

          if (b >= 0.) {
               a = std::pow(b, 2. / 3.);
          } else {
               a = 0.;
          }
     } else {
          return;
     }

     const doublereal dd_dt2 = (w.dd_dtCurr - w.dd_dtPrev) / (tCurr - tPrev);
     const doublereal p = 2 * w.dd_dtPrev / dd_dt2;
     const doublereal q = 2 * (w.dPrev - a) / dd_dt2;
     const doublereal r = 0.25 * p * p - q;

     if (r >= 0) {
          const doublereal dt[2] = {
               -0.5 * p + sqrt(r),
               -0.5 * p - sqrt(r)
          };

          for (int i = 0; i < 2; ++i) {
               if (dt[i] > 0. && dt[i] < dtMax) {
                    dtMax = dt[i];
               }
          }
     }
}

void BallBearingContact::AfterConvergence(const VectorHandler& X,
                                          const VectorHandler& XP)
{
     tPrev = tCurr;

     for (auto& w: washers) {
          w.dPrev = w.dCurr;
          w.dd_dtPrev = w.dd_dtCurr;
          w.FnPrev = w.FnCurr;
     }
}

unsigned int
BallBearingContact::iGetNumPrivData(void) const
{
     return 1u + washers.size() * iNumPrivData;
}

unsigned int BallBearingContact::iGetPrivDataIdx(const char *s) const
{
     if (0 == strcmp(s, "max" "dt")) {
          return 1u;
     } else {
          for (int i = 0; i < iNumPrivData; ++i) {
               unsigned iWasher;

               if (1 != sscanf(s, rgPrivData[i].szPattern, &iWasher)) {
                    continue;
               }

               if (iWasher <= 0 || iWasher > washers.size()) {
                    return 0;
               }

               return (iWasher - 1) * iNumPrivData + i + 2u;
          }

          return 0;
     }
}

doublereal BallBearingContact::dGetPrivData(unsigned int i) const
{
     if (i == 1u) {
          return dtMax;
     }

     const div_t d = div(i - 2u, iNumPrivData);

     if (d.quot < 0 || size_t(d.quot) >= washers.size()) {
          silent_cerr("ballbearingcontact(" << GetLabel()
                      << "): invalid index " << i << std::endl);
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     const Washer& w = washers[d.quot];

     switch (d.rem) {
     case 0:
          return w.dCurr;

     case 1:
          return w.dd_dtCurr;

     case 2:
          return w.FnCurr;

     case 3:
     case 4:
          return w.z(d.rem - 2);

     case 5:
     case 6:
          return w.zP(d.rem - 4);

     case 7:
     case 8:
          return w.uP(d.rem - 6);

     case 9:
     case 10:
          return w.tau(d.rem - 8);

     default:
          silent_cerr("ballbearingcontact(" << GetLabel()
                      << "): invalid index " << i << std::endl);
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }
}

int
BallBearingContact::iGetNumConnectedNodes(void) const
{
     return washers.size() + 1;
}

void
BallBearingContact::GetConnectedNodes(std::vector<const Node *>& connectedNodes) const
{
     connectedNodes.reserve(iGetNumConnectedNodes());
     connectedNodes.clear();
     connectedNodes.push_back(pNode1);

     for (const auto& w: washers) {
          connectedNodes.push_back(w.pNode2);
     }
}

void
BallBearingContact::SetValue(DataManager *pDM,
                             VectorHandler& X, VectorHandler& XP,
                             SimulationEntity::Hints *ph)
{

}

std::ostream&
BallBearingContact::Restart(std::ostream& out) const
{
     return out;
}

unsigned int BallBearingContact::iGetNumDof(void) const
{
     return bEnableFriction ? 2 * washers.size() : 0;
}

std::ostream& BallBearingContact::DescribeDof(std::ostream& out,
                                              const char *prefix, bool bInitial) const
{
     if (bEnableFriction) {
          const integer iIndex = iGetFirstIndex();

          for (size_t i = 0; i < washers.size(); ++i) {
               for (int j = 1; j <= 2; ++j) {
                    out << prefix << iIndex + j + 2 * i << ": stiction state z" << j << "[" << i << "]" << std::endl;
               }
          }
     }

     return out;
}

std::ostream& BallBearingContact::DescribeEq(std::ostream& out,
                                             const char *prefix, bool bInitial) const
{
     if (bEnableFriction) {
          const integer iIndex = iGetFirstIndex();

          for (size_t i = 0; i < washers.size(); ++i) {
               for (int j = 1; j <= 2; ++j) {
                    out << prefix << iIndex + j + 2 * i << ": ode for stiction state z" << j << "[" << i << "]" << std::endl;
               }
          }
     }

     return out;
}

DofOrder::Order BallBearingContact::GetDofType(unsigned int) const
{
     return DofOrder::DIFFERENTIAL;
}

DofOrder::Order BallBearingContact::GetEqType(unsigned int i) const
{
     return DofOrder::DIFFERENTIAL;
}

unsigned int
BallBearingContact::iGetInitialNumDof(void) const
{
     return 0;
}

void
BallBearingContact::InitialWorkSpaceDim(
     integer* piNumRows,
     integer* piNumCols) const
{
     *piNumRows = 0;
     *piNumCols = 0;
}

VariableSubMatrixHandler&
BallBearingContact::InitialAssJac(
     VariableSubMatrixHandler& WorkMat,
     const VectorHandler& XCurr)
{
     WorkMat.SetNullMatrix();

     return WorkMat;
}

SubVectorHandler&
BallBearingContact::InitialAssRes(
     SubVectorHandler& WorkVec,
     const VectorHandler& XCurr)
{
     WorkVec.ResizeReset(0);

     return WorkVec;
}

bool ballbearing_contact_set(void)
{
     UserDefinedElemRead *rf = new UDERead<BallBearingContact>;

     if (!SetUDE("ball" "bearing" "contact", rf))
     {
          delete rf;
          return false;
     }

     return true;
}

#ifndef STATIC_MODULES

extern "C"
{

     int module_init(const char *module_name, void *pdm, void *php)
     {
          if (!ballbearing_contact_set())
          {
               silent_cerr("ballbearing_contact: "
                           "module_init(" << module_name << ") "
                           "failed" << std::endl);

               return -1;
          }

          return 0;
     }

}

#endif // ! STATIC_MODULE
