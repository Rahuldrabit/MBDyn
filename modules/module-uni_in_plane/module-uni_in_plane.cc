/* $Header$ */
/*
 * MBDyn (C) is a multibody analysis code.
 * http://www.mbdyn.org
 *
 * Copyright (C) 1996-2023
 *
 * Pierangelo Masarati	<pierangelo.masarati@polimi.it>
 * Paolo Mantegazza	<paolo.mantegazza@polimi.it>
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
  Copyright (C) 2015(-2023) all rights reserved.

  The copyright of this code is transferred
  to Pierangelo Masarati and Paolo Mantegazza
  for use in the software MBDyn as described
  in the GNU Public License version 2.1
*/

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <limits>
#include <memory>
#include <vector>

using namespace std;

#ifdef HAVE_CONFIG_H
#include "mbconfig.h"           /* This goes first in every *.c,*.cc file */
#endif /* HAVE_CONFIG_H */

#include <dataman.h>
#include <userelem.h>

#include <strnodead.h>
#include <sp_gradient.h>
#include <sp_matrix_base.h>
#include <sp_matvecass.h>
#include "module-uni_in_plane.h"

template <typename StructNodeType>
struct StructNodeTraits;

template <>
struct StructNodeTraits<StructNodeAd> {
     typedef StructNodeAd StructNodeType1;

     typedef sp_grad::SpColVectorA<doublereal, 3> StructNodeOffsetType1;

     template <typename T>
     struct ContactPointTraits {
          typedef std::vector<T> ContactPointVecType;
          static void
          resize(ContactPointVecType& ContactPointVec, size_t N) {
               ContactPointVec.resize(N);
          }
     };

     template <typename T>
     struct RotationMatrixTraits {
          typedef sp_grad::SpMatrixA<T, 3, 3> RotationMatrixType1;
          static void
          GetRCurr(const StructNodeAd* pNode1,
                   RotationMatrixType1& R1,
                   doublereal dCoef,
                   sp_grad::SpFunctionCall func) {
               pNode1->GetRCurr(R1, dCoef, func);
          }
     };

     template <typename T>
     struct AngularVelocityTraits {
          typedef sp_grad::SpColVectorA<T, 3> AngularVelocityType1;

          static void
          GetWCurr(const StructNodeAd* pNode1,
                   AngularVelocityType1& omega1,
                   doublereal dCoef,
                   sp_grad::SpFunctionCall func) {
               pNode1->GetWCurr(omega1, dCoef, func);
          }

          template <typename Tomega1, typename TR1_o1>
          static auto
          Cross(const Tomega1& omega1, const TR1_o1& R1_o1)->decltype(sp_grad::Cross(omega1, R1_o1)) {
               return sp_grad::Cross(omega1, R1_o1);
          }
     };

     template <typename T>
     struct MomentTraits {
          typedef sp_grad::SpColVectorA<T, 3> MomentType1;

          template <typename TR1_o1, typename TF1>
          static auto
          Cross(const TR1_o1& R1_o1, const TF1& F1)->decltype(sp_grad::Cross(R1_o1, F1)) {
               return sp_grad::Cross(R1_o1, F1);
          }
     };

     template <typename T>
     static void
     AddMomentNode1(sp_grad::SpGradientAssVec<T>& WorkVec,
                    const integer iFirstMomentumIndexNode1,
                    const sp_grad::SpColVector<T, 3>& M1) {
          WorkVec.AddItem(iFirstMomentumIndexNode1 + 4, M1);
     }
};

template <>
struct StructNodeTraits<StructDispNodeAd> {
     typedef StructDispNodeAd StructNodeType1;

     struct StructNodeOffsetType1 {
          StructNodeOffsetType1(const Vec3&) {
          }

          operator const Vec3&() const {
               return ::Zero3;
          }
     };

     template <typename T>
     struct ContactPointTraits {
          typedef std::array<T, 1> ContactPointVecType;
          static void
          resize(ContactPointVecType& ContactPointVec, size_t N) {
               ASSERT(ContactPointVec.size() == N);
          }
     };

     template <typename T>
     struct RotationMatrixTraits {
          struct RotationMatrixType1 {
               const Vec3& operator*(const StructNodeOffsetType1& o1) const {
                    return ::Zero3;
               }
          };

          static void
          GetRCurr(const StructDispNodeAd* pNode1,
                   RotationMatrixType1& R1,
                   doublereal dCoef,
                   sp_grad::SpFunctionCall func) {
          }
     };

     template <typename T>
     struct AngularVelocityTraits {
          struct AngularVelocityType1 {};

          static void
          GetWCurr(const StructDispNodeAd* pNode1,
                   AngularVelocityType1& omega1,
                   doublereal dCoef,
                   sp_grad::SpFunctionCall func) {
          }

          static const Vec3&
          Cross(const AngularVelocityType1& omega1,
                const Vec3& R1_o1) {
               return ::Zero3;
          }
     };

     template <typename T>
     struct MomentTraits {
          typedef sp_grad::SpColVectorA<doublereal, 3> MomentType1;
          static const Vec3&
          Cross(const Vec3& R1_o1, const sp_grad::SpColVector<T, 3>& F1) {
               return ::Zero3;
          }
     };

     template <typename T>
     static void
     AddMomentNode1(sp_grad::SpGradientAssVec<T>& WorkVec,
                    const integer iFirstMomentumIndexNode1,
                    const sp_grad::SpColVector<doublereal, 3>& M1) {
     }
};

template <typename StructNodeType1>
class UniInPlaneFriction: public UserDefinedElem
{
     using Elem::AssRes;
public:
     UniInPlaneFriction(unsigned uLabel, const DofOwner *pDO,
                        DataManager* pDM, MBDynParser& HP);
     virtual ~UniInPlaneFriction(void);
     virtual unsigned int iGetNumDof(void) const override;
     virtual DofOrder::Order GetDofType(unsigned int i) const override;
     virtual DofOrder::Order GetEqType(unsigned int i) const override;
     virtual DofOrder::Equality GetEqualityType(unsigned int i) const override;
     virtual std::ostream& DescribeDof(std::ostream& out, const char *prefix, bool bInitial) const override;
     virtual std::ostream& DescribeEq(std::ostream& out, const char *prefix, bool bInitial) const override;
     virtual unsigned int iGetNumPrivData(void) const override;
     virtual unsigned int iGetPrivDataIdx(const char *s) const override;
     virtual doublereal dGetPrivData(unsigned int i) const override;
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
     AssRes(sp_grad::SpGradientAssVec<T>& WorkVec,
            doublereal dCoef,
            const sp_grad::SpGradientVectorHandler<T>& XCurr,
            const sp_grad::SpGradientVectorHandler<T>& XPrimeCurr,
            enum sp_grad::SpFunctionCall func);
     virtual void AfterConvergence(const VectorHandler& X,
                                   const VectorHandler& XP) override;
     int iGetNumConnectedNodes(void) const;
     void GetConnectedNodes(std::vector<const Node *>& connectedNodes) const override;
     void SetValue(DataManager *pDM, VectorHandler& X, VectorHandler& XP,
                   SimulationEntity::Hints *ph) override;
     std::ostream& Restart(std::ostream& out) const override;
     virtual unsigned int iGetInitialNumDof(void) const override;
     virtual void
     InitialWorkSpaceDim(integer* piNumRows, integer* piNumCols) const override;
     VariableSubMatrixHandler&
     InitialAssJac(VariableSubMatrixHandler& WorkMat,
                   const VectorHandler& XCurr) override;
     SubVectorHandler&
     InitialAssRes(SubVectorHandler& WorkVec, const VectorHandler& XCurr) override;

private:
     static const int iNumDofGradient = 15;

     struct ContactPoint
     {
          inline explicit ContactPoint(const Vec3& offset = Zero3, doublereal s = std::numeric_limits<doublereal>::max());

          inline void AfterConvergence() {
               lambdaPrev = lambda;
          }

          static inline void
          UpdateReaction(const sp_grad::SpColVector<sp_grad::SpGradient, 3>&,
                         const sp_grad::SpColVector<sp_grad::SpGradient, 3>&,
                         const sp_grad::SpColVector<sp_grad::SpGradient, 3>&,
                         const sp_grad::SpColVector<sp_grad::SpGradient, 3>&,
                         const sp_grad::SpGradient&,
                         const sp_grad::SpGradient&) {

          }

          static inline void
          UpdateReaction(const sp_grad::SpColVector<sp_grad::GpGradProd, 3>&,
                         const sp_grad::SpColVector<sp_grad::GpGradProd, 3>&,
                         const sp_grad::SpColVector<sp_grad::GpGradProd, 3>&,
                         const sp_grad::SpColVector<sp_grad::GpGradProd, 3>&,
                         const sp_grad::GpGradProd&,
                         const sp_grad::GpGradProd&) {

          }

          inline void
          UpdateReaction(const sp_grad::SpColVector<doublereal, 3>& F1,
                         const sp_grad::SpColVector<doublereal, 3>& M1,
                         const sp_grad::SpColVector<doublereal, 3>& F2,
                         const sp_grad::SpColVector<doublereal, 3>& M2,
                         doublereal dXn,
                         doublereal lambda);

          static inline void
          UpdateFriction(const sp_grad::SpColVector<sp_grad::SpGradient, 2>& U,
                         const sp_grad::SpColVector<sp_grad::SpGradient, 2>& tau,
                         const sp_grad::SpColVector<sp_grad::SpGradient, 2>& z,
                         const sp_grad::SpColVector<sp_grad::SpGradient, 2>& zP) {
          }

          static inline void
          UpdateFriction(const sp_grad::SpColVector<sp_grad::GpGradProd, 2>& U,
                         const sp_grad::SpColVector<sp_grad::GpGradProd, 2>& tau,
                         const sp_grad::SpColVector<sp_grad::GpGradProd, 2>& z,
                         const sp_grad::SpColVector<sp_grad::GpGradProd, 2>& zP) {
          }

          inline void UpdateFriction(const sp_grad::SpColVector<doublereal, 2>& U,
                                     const sp_grad::SpColVector<doublereal, 2>& tau,
                                     const sp_grad::SpColVector<doublereal, 2>& z,
                                     const sp_grad::SpColVector<doublereal, 2>& zP);

          typename StructNodeTraits<StructNodeType1>::StructNodeOffsetType1 o1;
          doublereal s;
          doublereal lambda, lambdaPrev, dXn;
          sp_grad::SpColVectorA<doublereal, 2> U, tau, z, zP;
          sp_grad::SpColVectorA<doublereal, 3> F1, M1, F2, M2;
     };

     static const int iNumPrivData = 22;
     static const int iNumPrivDataGlobal = 1;

     static const struct PrivateData
     {
          enum Index
          {
               LAMBDA,
               DXN,
               TAU1,
               TAU2,
               U1,
               U2,
               Z1,
               Z2,
               ZP1,
               ZP2,
               F1X,
               F1Y,
               F1Z,
               M1X,
               M1Y,
               M1Z,
               F2X,
               F2Y,
               F2Z,
               M2X,
               M2Y,
               M2Z
          };

          char name[10];
     } rgPrivData[iNumPrivData];

private:
     typedef typename StructNodeTraits<StructNodeType1>::template ContactPointTraits<ContactPoint>::ContactPointVecType ContactPointVec_t;

     const DataManager* const pDM;
     const typename StructNodeTraits<StructNodeType1>::StructNodeType1* pNode1;
     ContactPointVec_t ContactPoints1;
     const StructNodeAd* pNode2;
     sp_grad::SpColVectorA<doublereal, 3> o2;
     sp_grad::SpMatrixA<doublereal, 3, 3> Rp;
     doublereal epsilon, dFmax, tCurr, tPrev;
     bool bEnableFriction;
     sp_grad::SpMatrixA<doublereal, 2, 2> Mk, Ms, Mk2, Ms2, invMk2_sigma0;
     sp_grad::SpMatrixA<doublereal, 2, 2> sigma0, sigma1, sigma2;
     doublereal vs, a;
     DriveOwner m_oInitialAssembly, m_oOffset;
     bool bEnableMCP;
};

template <typename StructNodeType1>
UniInPlaneFriction<StructNodeType1>::ContactPoint::ContactPoint(const Vec3& offset, doublereal s)
     :o1(offset),
      s(s),
      lambda(0.),
      lambdaPrev(0.),
      dXn(0.),
      F1(Zero3),
      M1(Zero3),
      F2(Zero3),
      M2(Zero3)
{

}

template <typename StructNodeType1>
void UniInPlaneFriction<StructNodeType1>::ContactPoint::UpdateReaction(
     const sp_grad::SpColVector<doublereal, 3>& F1,
     const sp_grad::SpColVector<doublereal, 3>& M1,
     const sp_grad::SpColVector<doublereal, 3>& F2,
     const sp_grad::SpColVector<doublereal, 3>& M2,
     const doublereal dXn,
     const doublereal lambda)
{
     this->F1 = F1;
     this->M1 = M1;
     this->F2 = F2;
     this->M2 = M2;
     this->dXn = dXn;
     this->lambda = lambda;
}

template <typename StructNodeType1>
void UniInPlaneFriction<StructNodeType1>::ContactPoint::UpdateFriction(const sp_grad::SpColVector<doublereal, 2>& U,
                                                                       const sp_grad::SpColVector<doublereal, 2>& tau,
                                                                       const sp_grad::SpColVector<doublereal, 2>& z,
                                                                       const sp_grad::SpColVector<doublereal, 2>& zP)
{
     this->U = U;
     this->tau = tau;
     this->z = z;
     this->zP = zP;
}

template <typename StructNodeType1>
const typename UniInPlaneFriction<StructNodeType1>::PrivateData UniInPlaneFriction<StructNodeType1>::rgPrivData[iNumPrivData] =
{
     {"lambda"},
     {"dXn"},
     {"tau1"},
     {"tau2"},
     {"U1"},
     {"U2"},
     {"z1"},
     {"z2"},
     {"zP1"},
     {"zP2"},
     {"F1x"},
     {"F1y"},
     {"F1z"},
     {"M1x"},
     {"M1y"},
     {"M1z"},
     {"F2x"},
     {"F2y"},
     {"F2z"},
     {"M2x"},
     {"M2y"},
     {"M2z"}
};

template <typename StructNodeType1>
UniInPlaneFriction<StructNodeType1>::UniInPlaneFriction(
     unsigned uLabel, const DofOwner *pDO,
     DataManager* pDM, MBDynParser& HP)
     :       UserDefinedElem(uLabel, pDO),
             pDM(pDM),
             pNode1(0),
             pNode2(0),
             o2(Zero3),
             Rp(Eye3),
             epsilon(0.),
             dFmax(0.),
             bEnableFriction(false),
             bEnableMCP(false)
{
     tCurr = tPrev = pDM->dGetTime();

     using namespace sp_grad;
     // help
     if (HP.IsKeyWord("help"))
     {
          silent_cout(
               "\n"
               "Module:        UniInPlaneFriction\n"
               "\n"
               "	This element implements the unilateral contact between a point and a plane\n"
               "\n"
               "	unilateral in plane,\n"
               "		node1, (label) <node1>,\n"
               "			[ offset, (integer) <count>,\n"
               "			  (Vec3) <offset1>,\n"
               "			  [stiffness, (real) <stiffness1>,]\n"
               "			  [ ... , ] ]\n"
               "		epsilon, (real) <epsilon>,\n"
               "		node2, (label) <node2>,\n"
               "			[ offset, (Vec3) <offset>, ]\n"
               "			[ hinge, (Mat3x3) <hinge>, ]\n"
               "		[ initial assembly, (DriveCaller) <initial_assembly>, ]\n"
               "		[ offset plane, (DriveCaller) <normal_offset> ]"
               "\n"
               << std::endl);

          if (!HP.IsArg())
          {
               /*
                * Exit quietly if nothing else is provided
                */
               throw NoErr(MBDYN_EXCEPT_ARGS);
          }
     }

     if ( !HP.IsKeyWord("node1") )
     {
          silent_cerr("unilateral in plane(" << GetLabel() << "): keyword \"node1\" expected at line " << HP.GetLineData() << std::endl);
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     pNode1 = pDM->ReadNode<StructNodeType1, Node::STRUCTURAL>(HP);

     if (!pNode1) {
          silent_cerr("unilateral in plane(" << GetLabel() << "): structural node expected at line " << HP.GetLineData() << std::endl);
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     if ( HP.IsKeyWord("offset") )
     {
          const size_t N = HP.GetInt();
          const size_t Nmax = ContactPoints1.max_size();

          if (N < 1u || N > Nmax)
          {
               silent_cerr("unilateral in plan(" << GetLabel()
                           << "): number of offsets must be greater than zero at line "
                           << HP.GetLineData() << std::endl);
               throw ErrGeneric(MBDYN_EXCEPT_ARGS);
          }

          const ReferenceFrame refNode1(pNode1);

          StructNodeTraits<StructNodeType1>::template ContactPointTraits<ContactPoint>::resize(ContactPoints1, N);

          for (size_t i = 0u; i < N; ++i)
          {
               ContactPoints1[i].o1 = HP.GetPosRel(refNode1);
               ContactPoints1[i].s = HP.IsKeyWord("stiffness") ? HP.GetReal() : std::numeric_limits<doublereal>::max();
          }
     }
     else
     {
          StructNodeTraits<StructNodeType1>::template ContactPointTraits<ContactPoint>::resize(ContactPoints1, 1);
     }

     if (HP.IsKeyWord("enable" "mcp")) {
          bEnableMCP = HP.GetYesNoOrBool();
     }

     if (bEnableMCP) {
          epsilon = 0.;
     } else {
          if ( !HP.IsKeyWord("epsilon") )
          {
               silent_cerr("unilateral in plane(" << GetLabel() << "): keyword \"epsilon\" expected at line " << HP.GetLineData() << std::endl);
               throw ErrGeneric(MBDYN_EXCEPT_ARGS);
          }

          epsilon = HP.GetReal();

          if ( epsilon <= 0 )
          {
               silent_cerr("unilateral in plane(" << GetLabel() << "): epsilon must be greater than zero at line " << HP.GetLineData() << std::endl);
               throw ErrGeneric(MBDYN_EXCEPT_ARGS);
          }
     }

     dFmax = HP.IsKeyWord("max" "force" "increment") ? HP.GetReal() : std::numeric_limits<doublereal>::max();

     if ( !HP.IsKeyWord("node2") )
     {
          silent_cerr("unilateral in plane(" << GetLabel() << "): keyword \"node2\" expected at line " << HP.GetLineData() << std::endl);
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     pNode2 = pDM->ReadNode<StructNodeAd, Node::STRUCTURAL>(HP);

     if (!pNode2) {
          silent_cerr("unilateral in plane(" << GetLabel() << "): structural node expected at line " << HP.GetLineData() << std::endl);
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     const ReferenceFrame refNode2(pNode2);

     if ( HP.IsKeyWord("offset") )
          o2 = HP.GetPosRel(refNode2);

     if ( HP.IsKeyWord("hinge") )
     {
          Rp = HP.GetRotRel(refNode2);
     }

     if (HP.IsKeyWord("coulomb" "friction" "coefficient") || HP.IsKeyWord("coulomb" "friction" "coefficient" "x"))
     {
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
               a = HP.GetReal();
          } else {
               a = 1.;
          }

          if (!(HP.IsKeyWord("micro" "slip" "stiffness") || HP.IsKeyWord("micro" "slip" "stiffness" "x"))) {
               silent_cerr("unilateral in plane("
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

          if (HP.IsKeyWord("viscous" "friction" "coefficient") || HP.IsKeyWord("viscous" "friction" "coefficient" "x"))
          {
               sigma2(1, 1) = HP.GetReal();
          }

          if (HP.IsKeyWord("viscous" "friction" "coefficient" "y"))
          {
               sigma2(2, 2) = HP.GetReal();
          }
          else
          {
               sigma2(2, 2) = sigma2(1, 1);
          }

          Mk(1, 1) = mukx;
          Mk(2, 2) = muky;

          Mk2 = Mk * Mk;

          Ms(1, 1) = musx;
          Ms(2, 2) = musy;

          Ms2 = Ms * Ms;

          sigma0(1, 1) = sigma0x;
          sigma0(2, 2) = sigma0y;

          sigma1(1, 1) = sigma1x;
          sigma1(2, 2) = sigma1y;

          invMk2_sigma0 = Inv(Mk2) * sigma0;
     }

     m_oInitialAssembly.Set(HP.IsKeyWord("initial" "assembly") ? HP.GetDriveCaller() : new OneDriveCaller);
     m_oOffset.Set(HP.IsKeyWord("offset" "plane") ? HP.GetDriveCaller() : new NullDriveCaller);

     SetOutputFlag(pDM->fReadOutput(HP, Elem::LOADABLE));

     std::ostream& out = pDM->GetLogFile();

     for (auto it = ContactPoints1.begin(); it != ContactPoints1.end(); ++it)
     {
          // FIXME: support for this element to be added to mbdyn2easyanim.awk
          out << "totaljoint: " << GetLabel() << "." << it - ContactPoints1.begin()
              << " " << pNode2->GetLabel()
              << " " << o2
              << " " << Rp
              << " " << Rp
              << " " << pNode1->GetLabel()
              << " " << it->o1
              << " " << Eye3
              << " " << Eye3
              << std::endl;
     }
}

template <typename StructNodeType1>
UniInPlaneFriction<StructNodeType1>::~UniInPlaneFriction(void)
{
     // destroy private data
}

template <typename StructNodeType1>
unsigned int UniInPlaneFriction<StructNodeType1>::iGetNumDof(void) const
{
     return ContactPoints1.size() * (1 + 2 * bEnableFriction);
}

template <typename StructNodeType1>
DofOrder::Order UniInPlaneFriction<StructNodeType1>::GetDofType(unsigned int i) const
{
     const int iNumDof = 1 + 2 * bEnableFriction;

     switch (i % iNumDof)
     {
     case 0:
          return DofOrder::ALGEBRAIC;

     case 1:
     case 2:
          return DofOrder::DIFFERENTIAL;

     default:
          ASSERT(false);
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }
}

template <typename StructNodeType1>
DofOrder::Order UniInPlaneFriction<StructNodeType1>::GetEqType(unsigned int i) const
{
     return GetDofType(i);
}

template <typename StructNodeType1>
DofOrder::Equality UniInPlaneFriction<StructNodeType1>::GetEqualityType(unsigned int i) const
{
     if (!bEnableMCP) {
          return DofOrder::EQUALITY;
     }

     switch (GetDofType(i)) {
     case DofOrder::ALGEBRAIC:
          return DofOrder::INEQUALITY;
     default:
          return DofOrder::EQUALITY;
     }
}

template <typename StructNodeType1>
std::ostream& UniInPlaneFriction<StructNodeType1>::DescribeDof(std::ostream& out, const char *prefix, bool bInitial) const
{
     integer iIndex = iGetFirstIndex();

     const int N = ContactPoints1.size();

     for ( int i = 1; i <= N; ++i )
     {
          out << prefix << ++iIndex << ": reaction force [lambda(" << i << ")]" << std::endl;

          if (bEnableFriction)
          {
               for (int j = 1; j <= 2; ++j)
               {
                    out << prefix << ++iIndex << ": stiction state [z" << j << "(" << i << ")]" << std::endl;
               }
          }
     }

     return out;
}

template <typename StructNodeType1>
std::ostream& UniInPlaneFriction<StructNodeType1>::DescribeEq(std::ostream& out, const char *prefix, bool bInitial) const
{
     integer iIndex = iGetFirstIndex();

     const int N = ContactPoints1.size();

     for ( int i = 1; i <= N; ++i )
     {
          out << prefix << ++iIndex << ": constraint equation c[" << i << "]" << std::endl;

          if (bEnableFriction)
          {
               for (int j = 1; j <= 2; ++j)
               {
                    out << prefix << ++iIndex << ": stiction state variation Phi[" << j << "," << i << "]" << std::endl;
               }
          }
     }

     return out;
}

template <typename StructNodeType1>
unsigned int UniInPlaneFriction<StructNodeType1>::iGetNumPrivData(void) const
{
     return iNumPrivDataGlobal + ContactPoints1.size() * iNumPrivData;
}

template <typename StructNodeType1>
unsigned int UniInPlaneFriction<StructNodeType1>::iGetPrivDataIdx(const char *s) const
{
     if (0 == strcmp(s, "max" "dt"))
     {
          return 1u;
     }

     std::istringstream is(s);
     std::string tok1;
     char tok2;
     unsigned int i;

     std::getline(is, tok1, '[');

     is >> i >> tok2;

     if ( !is.good() ||
          tok2 != ']' ||
          i == 0 ||
          i > ContactPoints1.size() )
     {
          goto error_return;
     }

     for (int j = 0; j < iNumPrivData; ++j)
     {
          if (tok1 == rgPrivData[j].name)
          {
               return iNumPrivDataGlobal + (i - 1) * iNumPrivData + j + 1;
          }
     }

error_return:
     silent_cerr("unilateral in plane(" << GetLabel() << "): private data \"" << s << "\" not available" << std::endl);
     return 0;
}

template <typename StructNodeType1>
doublereal UniInPlaneFriction<StructNodeType1>::dGetPrivData(unsigned int i) const
{
     if (i == 1u)
     {
          doublereal dtmax = std::numeric_limits<doublereal>::max();

          for (auto j = ContactPoints1.begin(); j != ContactPoints1.end(); ++j)
          {
               const doublereal dF = j->lambda - j->lambdaPrev;

               if (std::abs(dF) > dFmax)
               {
                    const doublereal dt = tCurr - tPrev;
                    dtmax = std::min(dtmax, std::abs(dt / dF * dFmax));
               }
          }

          return dtmax;
     }

     const div_t idx = div(i - 1 - iNumPrivDataGlobal, iNumPrivData);

     if ( idx.quot < 0 || ::size_t(idx.quot) >= ContactPoints1.size() || idx.rem < 0 || idx.rem >= iNumPrivData )
     {
          silent_cerr("unilateral in plane(" << GetLabel() << "): index " << i << " out of range" << std::endl);
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     const ContactPoint& cont = ContactPoints1[idx.quot];

     switch (idx.rem)
     {
     case PrivateData::LAMBDA:
          return cont.lambda;

     case PrivateData::DXN:
          return cont.dXn;

     case PrivateData::TAU1:
     case PrivateData::TAU2:
          return cont.tau(idx.rem - PrivateData::TAU1 + 1);

     case PrivateData::U1:
     case PrivateData::U2:
          return cont.U(idx.rem - PrivateData::U1 + 1);

     case PrivateData::Z1:
     case PrivateData::Z2:
          return cont.z(idx.rem - PrivateData::Z1 + 1);

     case PrivateData::ZP1:
     case PrivateData::ZP2:
          return cont.zP(idx.rem - PrivateData::ZP1 + 1);

     case PrivateData::F1X:
     case PrivateData::F1Y:
     case PrivateData::F1Z:
          return cont.F1(idx.rem - PrivateData::F1X + 1);

     case PrivateData::M1X:
     case PrivateData::M1Y:
     case PrivateData::M1Z:
          return cont.M1(idx.rem - PrivateData::M1X + 1);

     case PrivateData::F2X:
     case PrivateData::F2Y:
     case PrivateData::F2Z:
          return cont.F2(idx.rem - PrivateData::F2X + 1);

     case PrivateData::M2X:
     case PrivateData::M2Y:
     case PrivateData::M2Z:
          return cont.M2(idx.rem - PrivateData::M2X + 1);

     default:
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }
}

template <typename StructNodeType1>
void
UniInPlaneFriction<StructNodeType1>::Output(OutputHandler& OH) const
{
     if ( bToBeOutput() )
     {
          if ( OH.UseText(OutputHandler::LOADABLE) )
          {
               std::ostream& os = OH.Loadable();

               os << std::setw(8) << GetLabel();

               for (auto it = ContactPoints1.begin(); it != ContactPoints1.end(); ++it)
                    os << " " << it->dXn << " " << it->F1 << " " << it->M1 << " " << it->F2 << " " << it->M2;

               os << std::endl;
          }
     }
}

template <typename StructNodeType1>
void
UniInPlaneFriction<StructNodeType1>::WorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
     *piNumRows = ContactPoints1.size() * iNumDofGradient;
     *piNumCols = 0;
}

template <typename StructNodeType1>
VariableSubMatrixHandler&
UniInPlaneFriction<StructNodeType1>::AssJac(VariableSubMatrixHandler& WorkMatVar,
                                            doublereal dCoef,
                                            const VectorHandler& XCurr,
                                            const VectorHandler& XPrimeCurr)
{
     using namespace sp_grad;

     SpGradientAssVec<SpGradient>::AssJac(this, WorkMatVar.SetSparseGradient(), dCoef, XCurr, XPrimeCurr, REGULAR_JAC);

     return WorkMatVar;
}

template <typename StructNodeType1>
void
UniInPlaneFriction<StructNodeType1>::AssJac(VectorHandler& JacY,
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

template <typename StructNodeType1>
SubVectorHandler&
UniInPlaneFriction<StructNodeType1>::AssRes(SubVectorHandler& WorkVec,
                                            doublereal dCoef,
                                            const VectorHandler& XCurr,
                                            const VectorHandler& XPrimeCurr)
{
     using namespace sp_grad;

     tCurr = pDM->dGetTime();

     SpGradientAssVec<doublereal>::AssRes(this, WorkVec, dCoef, XCurr, XPrimeCurr, REGULAR_RES);

     return WorkVec;
}

template <typename StructNodeType1>
template <typename T>
inline void
UniInPlaneFriction<StructNodeType1>::AssRes(sp_grad::SpGradientAssVec<T>& WorkVec,
                                            doublereal dCoef,
                                            const sp_grad::SpGradientVectorHandler<T>& XCurr,
                                            const sp_grad::SpGradientVectorHandler<T>& XPrimeCurr,
                                            enum sp_grad::SpFunctionCall func)
{
     using namespace sp_grad;

     const integer iFirstMomentumIndexNode1 = pNode1->iGetFirstMomentumIndex();
     const integer iFirstMomentumIndexNode2 = pNode2->iGetFirstMomentumIndex();
     integer iDofIndex = iGetFirstIndex();

     const integer N = ContactPoints1.size();
     const doublereal alpha = m_oInitialAssembly.dGet();

     typedef typename StructNodeTraits<StructNodeType1>::template RotationMatrixTraits<T> RotationMatrixTraits1;
     typedef typename RotationMatrixTraits1::RotationMatrixType1 RotationMatrixType1;

     SpColVectorA<T, 3> X1, X2;
     RotationMatrixType1 R1;
     SpMatrixA<T, 3, 3> R2;
     SpColVectorA<T, 2> z, zP, tau;
     T lambda, kappa;

     for (integer i = 1; i <= N; ++i)
     {
          ContactPoint& cont = ContactPoints1[i - 1];

          pNode1->GetXCurr(X1, dCoef, func);
          RotationMatrixTraits1::GetRCurr(pNode1, R1, dCoef, func);
          pNode2->GetXCurr(X2, dCoef, func);
          pNode2->GetRCurr(R2, dCoef, func);

          XCurr.dGetCoef(++iDofIndex, lambda, 1.);

          const auto& o1 = cont.o1;
          const SpColVector<T, 3> dX = X1 + R1 * o1 - X2 - R2 * o2;
          const T dXn = Dot(Rp.GetCol(3), Transpose(R2) * dX) - m_oOffset.dGet() + lambda / cont.s;

          if (bEnableMCP)
          {
               WorkVec.AddItem(iDofIndex, dXn / dCoef);
          }
          else if (alpha != 0.)
          {
               const T d = 0.5 * (dXn - lambda);
               const T c = 0.5 * (dXn + lambda) - sqrt(d * d + epsilon);
               WorkVec.AddItem(iDofIndex, c * (alpha / dCoef));
          }
          else
          {
               WorkVec.AddItem(iDofIndex, lambda / dCoef);
          }

          SpColVector<T, 3> F1p = Rp.GetCol(3) * lambda;

          if (bEnableFriction)
          {
               typedef typename StructNodeTraits<StructNodeType1>::template AngularVelocityTraits<T> AngularVelocityTraits;
               typedef typename AngularVelocityTraits::AngularVelocityType1 AngularVelocityType1;

               SpColVectorA<T, 3> X1P, X2P, omega2;
               AngularVelocityType1 omega1;

               pNode1->GetVCurr(X1P, dCoef, func);
               AngularVelocityTraits::GetWCurr(pNode1, omega1, dCoef, func);
               pNode2->GetVCurr(X2P, dCoef, func);
               pNode2->GetWCurr(omega2, dCoef, func);

               const SpColVector<T, 3> dXP = X1P + AngularVelocityTraits::Cross(omega1, R1 * o1) - X2P - Cross(omega2, R2 * o2);
               const SpColVector<T, 2> U = Transpose(SubMatrix<1, 1, 3, 1, 1, 2>(Rp)) * (Transpose(R2) * (dXP + Cross(dX, omega2)));
               const T norm_U2 = Dot(U, U);

               if (norm_U2 == 0.) {
                    SpGradientTraits<T>::ResizeReset(kappa, 0., 0);
               } else {
                    const SpColVector<T, 2> Mk_U = Mk * U;
                    const SpColVector<T, 2> Ms_U = Ms * U;
                    const SpColVector<T, 2> Mk2_U = Mk2 * U;
                    const SpColVector<T, 2> Ms2_U = Ms2 * U;
                    const T norm_Mk2_U = sqrt(Dot(Mk2_U, Mk2_U));
                    const T a0 = norm_Mk2_U / sqrt(Dot(Mk_U, Mk_U));
                    const T a1 = sqrt(Dot(Ms2_U, Ms2_U)) / sqrt(Dot(Ms_U, Ms_U));
                    const T g = a0 + (a1 - a0) * exp(-pow(sqrt(norm_U2) / vs, a));

                    kappa = norm_Mk2_U / g;
               }

               for (int i = 1; i <= 2; ++i)
               {
                    XCurr.dGetCoef(iDofIndex + i, z(i), dCoef);
                    XPrimeCurr.dGetCoef(iDofIndex + i, zP(i), 1.);
               }

               const SpColVector<T, 2> Phi = (U - (invMk2_sigma0 * z) * kappa - zP) * alpha;

               for (index_type i = 1; i <= 2; ++i)
               {
                    ++iDofIndex;

                    if (alpha != 0.)
                    {
                         WorkVec.AddItem(iDofIndex, Phi(i));
                    }
                    else
                    {
                         WorkVec.AddItem(iDofIndex, z(i));
                    }
               }

               tau = (sigma0 * z + sigma1 * zP) * lambda + sigma2 * U;

               for (index_type i = 1; i <= 2; ++i)
               {
                    F1p -= Rp.GetCol(i) * tau(i);
               }

               cont.UpdateFriction(U, tau, z, zP);
          }

          F1p *= alpha;

          typedef typename StructNodeTraits<StructNodeType1>::template MomentTraits<T> MomentTraits;
          typedef typename MomentTraits::MomentType1 MomentType1;

          const SpColVector<T, 3> F1 = R2 * F1p;
          const MomentType1 M1 = MomentTraits::Cross(R1 * o1, F1);
          const SpColVector<T, 3> F2 = -F1;
          const SpColVector<T, 3> M2 = Cross(X1 + R1 * o1 - X2, F2);

          WorkVec.AddItem(iFirstMomentumIndexNode1 + 1, F1);
          StructNodeTraits<StructNodeType1>::AddMomentNode1(WorkVec, iFirstMomentumIndexNode1, M1);
          WorkVec.AddItem(iFirstMomentumIndexNode2 + 1, F2);
          WorkVec.AddItem(iFirstMomentumIndexNode2 + 4, M2);

          cont.UpdateReaction(F1, M1, F2, M2, dXn, lambda);
     }
}

template <typename StructNodeType1>
void UniInPlaneFriction<StructNodeType1>::AfterConvergence(const VectorHandler& X,
                                                           const VectorHandler& XP)
{
     tPrev = tCurr;

     for (auto i = ContactPoints1.begin(); i != ContactPoints1.end(); ++i)
     {
          i->AfterConvergence();
     }
}

template <typename StructNodeType1>
int
UniInPlaneFriction<StructNodeType1>::iGetNumConnectedNodes(void) const
{
     return 2;
}

template <typename StructNodeType1>
void
UniInPlaneFriction<StructNodeType1>::GetConnectedNodes(std::vector<const Node *>& connectedNodes) const
{
     connectedNodes.resize(iGetNumConnectedNodes());
     connectedNodes[0] = pNode1;
     connectedNodes[1] = pNode2;
}

template <typename StructNodeType1>
void
UniInPlaneFriction<StructNodeType1>::SetValue(DataManager *pDM,
                                              VectorHandler& X, VectorHandler& XP,
                                              SimulationEntity::Hints *ph)
{
     const int N = ContactPoints1.size();

     integer iDofIndex = iGetFirstIndex();

     for ( int i = 0; i < N; ++i )
     {
          X.PutCoef(++iDofIndex, ContactPoints1[i].lambda);

          if (bEnableFriction)
          {
               for (int j = 1; j <= 2; ++j)
               {
                    X.PutCoef(++iDofIndex, ContactPoints1[i].z(j));
                    XP.PutCoef(iDofIndex, ContactPoints1[i].zP(j));
               }
          }
     }
}

template <typename StructNodeType1>
std::ostream&
UniInPlaneFriction<StructNodeType1>::Restart(std::ostream& out) const
{
     return out;
}

template <typename StructNodeType1>
unsigned int
UniInPlaneFriction<StructNodeType1>::iGetInitialNumDof(void) const
{
     return 0;
}

template <typename StructNodeType1>
void
UniInPlaneFriction<StructNodeType1>::InitialWorkSpaceDim(
     integer* piNumRows,
     integer* piNumCols) const
{
     *piNumRows = 0;
     *piNumCols = 0;
}

template <typename StructNodeType1>
VariableSubMatrixHandler&
UniInPlaneFriction<StructNodeType1>::InitialAssJac(
     VariableSubMatrixHandler& WorkMat,
     const VectorHandler& XCurr)
{
     WorkMat.SetNullMatrix();

     return WorkMat;
}

template <typename StructNodeType1>
SubVectorHandler&
UniInPlaneFriction<StructNodeType1>::InitialAssRes(
     SubVectorHandler& WorkVec,
     const VectorHandler& XCurr)
{
     WorkVec.ResizeReset(0);

     return WorkVec;
}

bool uni_in_plane_set(void)
{
     UserDefinedElemRead *rf = new UDERead<UniInPlaneFriction<StructNodeAd>>;

     if (!SetUDE("unilateral" "in" "plane", rf))
     {
          delete rf;
          return false;
     }

     rf = new UDERead<UniInPlaneFriction<StructDispNodeAd>>;

     if (!SetUDE("unilateral" "disp" "in" "plane", rf))
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
          if (!unilateral_in_plane_friction_set())
          {
               silent_cerr("contact: "
                           "module_init(" << module_name << ") "
                           "failed" << std::endl);

               return -1;
          }

          return 0;
     }

}

#endif // ! STATIC_MODULE
