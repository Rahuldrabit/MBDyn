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
        Copyright (C) 2022(-2023) all rights reserved.

        The copyright of this code is transferred
        to Pierangelo Masarati and Paolo Mantegazza
        for use in the software MBDyn as described
        in the GNU Public License version 2.1
*/

#include "mbconfig.h"           /* This goes first in every *.c,*.cc file */

/* FIXME: gravity in modal elements is eXperimental; undefine to disable */
#define MODAL_USE_GRAVITY

#include "modalad.h"

ModalAd::ModalAd(unsigned int uL,
                 const ModalNodeAd* pR,
                 const Vec3& x0,
                 const Mat3x3& R0,
                 const DofOwner* pDO,
                 unsigned int NM,         /* numero modi */
                 unsigned int NI,         /* numero nodi d'interfaccia */
                 unsigned int NF,         /* numero nodi FEM */
                 doublereal dMassTmp,     /* inv. inerzia (m, m.stat., d'in.) */
                 const Vec3& STmp,
                 const Mat3x3& JTmp,
                 std::vector<unsigned int>&& uModeNumber,
                 MatNxN&& oGenMass,
                 MatNxN&& oGenStiff,
                 MatNxN&& oGenDamp,
                 std::vector<std::string>&& IdFEMNodes,    /* label nodi FEM */
                 Mat3xN&& oN,               /* posizione dei nodi FEM */
                 std::vector<Modal::StrNodeData>&& snd,
                 Mat3xN&& oPHItStrNode,     /* forme modali nodi d'interfaccia */
                 Mat3xN&& oPHIrStrNode,
                 Mat3xN&& oModeShapest,     /* autovettori: servono a aeromodal */
                 Mat3xN&& oModeShapesr,
                 Mat3xN&& oInv3,            /* invarianti d'inerzia I3...I11 */
                 Mat3xN&& oInv4,
                 Mat3xN&& oInv5,
                 Mat3xN&& oInv8,
                 Mat3xN&& oInv9,
                 Mat3xN&& oInv10,
                 Mat3xN&& oInv11,
                 VecN&& aa,
                 VecN&& bb,
                 const std::vector<unsigned>& rgGenStressStiffIdx,
                 std::vector<MatNxN>&& rgGenStressStiff,
                 flag fOut)
: Modal(uL, pR, x0, R0, pDO, NM, NI, NF, dMassTmp, STmp, JTmp, std::move(uModeNumber), std::move(oGenMass), std::move(oGenStiff), std::move(oGenDamp), std::move(IdFEMNodes), std::move(oN), std::move(snd), std::move(oPHItStrNode), std::move(oPHIrStrNode), std::move(oModeShapest), std::move(oModeShapesr), std::move(oInv3), std::move(oInv4), std::move(oInv5), std::move(oInv8), std::move(oInv9), std::move(oInv10), std::move(oInv11), std::move(aa), std::move(bb), fOut),
 pModalNode{pR},
 rgModalStressStiff{std::move(rgGenStressStiff)}
{
     ASSERT(rgModalStressStiff.empty() || rgModalStressStiff.size() == 12u + SND.size() * 6);
     ASSERT(rgModalStressStiff.size() == rgGenStressStiffIdx.size());
     ASSERT(pModalNode == nullptr || pModalNode->GetStructNodeType() == StructNode::MODAL);

     if (!rgModalStressStiff.empty()) {
          for (unsigned i = 0u; i < rgGenStressStiffIdx.size(); ++i) {
               unsigned uIndex = rgGenStressStiffIdx[i];

               ASSERT(uIndex >= 1u); // one based index from *.fem file
               ASSERT(uIndex <= 12u + SND.size() * 6u);

               --uIndex; // zero based index

               switch (uIndex) {
               case 0u:
               case 1u:
               case 2u:
               case 3u:
               case 4u:
               case 5u:
                    oStressStiffIndexW.Insert(i, uIndex);
                    break;
               case 6u:
               case 7u:
               case 8u:
                    oStressStiffIndexWP.Insert(i, uIndex - 6u);
                    break;
               case 9u:
               case 10u:
               case 11u:
                    oStressStiffIndexVP.Insert(i, uIndex - 9u);
                    break;
               default: {
                    uIndex -= 12u;

                    const unsigned uNodeIdx = uIndex / 6u;
                    const unsigned uDofIdx = uIndex % 6u;

                    ASSERT(uNodeIdx >= 0u);
                    ASSERT(uNodeIdx < SND.size());
                    ASSERT(uDofIdx >= 0u);
                    ASSERT(uDofIdx < 6u);

                    switch (uDofIdx) {
                    case 0u:
                    case 1u:
                    case 2u:
                         SND[uNodeIdx].oStressStiffIndexF.Insert(i, uDofIdx);
                         break;
                    case 3u:
                    case 4u:
                    case 5u:
                         SND[uNodeIdx].oStressStiffIndexM.Insert(i, uDofIdx - 3u);
                         break;
                    default:
                         ASSERT(0);
                    }
               }
               }
          }
     }

     ASSERT(rgModalStressStiff.empty() || rgModalStressStiff.size() <= 12u + SND.size() * 6);
     ASSERT(pModalNode == nullptr || pModalNode->GetStructNodeType() == StructNode::MODAL);
}

ModalAd::~ModalAd(void)
{
}

void
ModalAd::WorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
     *piNumRows = (pModalNode ? 12 : 0) + 2 * NModes  + 2 * NModes * NStrNodes + 12 * NStrNodes;
     *piNumCols = 0;
}

VariableSubMatrixHandler&
ModalAd::AssJac(VariableSubMatrixHandler& WorkMat,
                doublereal dCoef,
                const VectorHandler& XCurr,
                const VectorHandler& XPrimeCurr)
{
     DEBUGCOUT("Entering ModalAd::AssJac()" << std::endl);

     SpGradientSubMatrixHandler& WorkMatSp = WorkMat.SetSparseGradient();

     sp_grad::SpGradientAssVec<sp_grad::SpGradient>::AssJac(this,
                                                            WorkMatSp,
                                                            dCoef,
                                                            XCurr,
                                                            XPrimeCurr,
                                                            sp_grad::SpFunctionCall::REGULAR_JAC);
     return WorkMat;
}

void
ModalAd::AssJac(VectorHandler& JacY,
                const VectorHandler& Y,
                doublereal dCoef,
                const VectorHandler& XCurr,
                const VectorHandler& XPrimeCurr,
                VariableSubMatrixHandler& WorkMat)
{
     DEBUGCOUT("Entering ModalAd::AssJac()" << std::endl);
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
ModalAd::AssRes(SubVectorHandler& WorkVec,
                doublereal dCoef,
                const VectorHandler& XCurr,
                const VectorHandler& XPrimeCurr)
{
     DEBUGCOUT("Entering ModalAd::AssRes()" << std::endl);

     sp_grad::SpGradientAssVec<doublereal>::AssRes(this,
                                                   WorkVec,
                                                   dCoef,
                                                   XCurr,
                                                   XPrimeCurr,
                                                   sp_grad::SpFunctionCall::REGULAR_RES);

     return WorkVec;
}

void
ModalAd::UpdateStrNodeData(ModalAd::StrNodeData& oNode,
                           const sp_grad::SpColVector<doublereal, 3>& d1tot,
                           const sp_grad::SpMatrix<doublereal, 3, 3>& R1tot,
                           const sp_grad::SpColVector<doublereal, 3>& F,
                           const sp_grad::SpColVector<doublereal, 3>& M,
                           const sp_grad::SpMatrix<doublereal, 3, 3>& R2)
{
     using namespace sp_grad;

     for (index_type i = 1; i <= 3; ++i) {
          oNode.d1tot(i) = d1tot(i);
          oNode.F(i) = F(i);
          oNode.M(i) = M(i);
     }

     for (index_type j = 1; j <= 3; ++j) {
          for (index_type i = 1; i <= 3; ++i) {
               oNode.R1tot(i, j) = R1tot(i, j);
               oNode.R2(i, j) = R2(i, j);
          }
     }
}

void
ModalAd::UpdateModalNode(const sp_grad::SpColVector<doublereal, 3>& xTmp,
                         const sp_grad::SpMatrix<doublereal, 3, 3>& RTmp)
{
     using namespace sp_grad;

     for (index_type i = 1; i <= 3; ++i) {
          x(i) = xTmp(i);
     }

     for (index_type j = 1; j <= 3; ++j) {
          for (index_type i = 1; i <= 3; ++i) {
               R(i, j) = RTmp(i, j);
          }
     }
}

void
ModalAd::UpdateState(const sp_grad::SpColVector<doublereal, sp_grad::SpMatrixSize::DYNAMIC>& aTmp,
                     const sp_grad::SpColVector<doublereal, sp_grad::SpMatrixSize::DYNAMIC>& aPrimeTmp,
                     const sp_grad::SpColVector<doublereal, sp_grad::SpMatrixSize::DYNAMIC>& bTmp,
                     const sp_grad::SpColVector<doublereal, sp_grad::SpMatrixSize::DYNAMIC>& bPrimeTmp)
{
     using namespace sp_grad;

     for (index_type i = 1; i <= a.iGetNumRows(); ++i) {
          a(i) = aTmp(i);
     }

     for (index_type i = 1; i <= aPrime.iGetNumRows(); ++i) {
          aPrime(i) = aPrimeTmp(i);
     }

     for (index_type i = 1; i <= b.iGetNumRows(); ++i) {
          b(i) = bTmp(i);
     }

     for (index_type i = 1; i <= bPrime.iGetNumRows(); ++i) {
          bPrime(i) = bPrimeTmp(i);
     }
}

void
ModalAd::UpdateInvariants(const sp_grad::SpColVector<doublereal, 3>& Inv3jajTmp,
                          const sp_grad::SpMatrix<doublereal, 3, 3>& Inv8jajTmp,
                          const sp_grad::SpMatrix<doublereal, 3, 3>& Inv9jkajakTmp)
{
     using namespace sp_grad;

     for (index_type i = 1; i <= Inv3jajTmp.iGetNumRows(); ++i) {
          Inv3jaj(i) = Inv3jajTmp(i);
     }

     for (index_type j = 1; j <= Inv8jajTmp.iGetNumCols(); ++j) {
          for (index_type i = 1; i <= Inv8jajTmp.iGetNumRows(); ++i) {
               Inv8jaj(i, j) = Inv8jajTmp(i, j);
          }
     }

     for (index_type j = 1; j <= Inv9jkajakTmp.iGetNumCols(); ++j) {
          for (index_type i = 1; i <= Inv9jkajakTmp.iGetNumRows(); ++i) {
               Inv9jkajak(i, j) = Inv9jkajakTmp(i, j);
          }
     }
}

#ifdef DEBUG
#define DEBUG_DUMP_GRAD_VEC_SIZE(varname, tplname)                      \
     if (std::is_same<sp_grad::SpGradient, tplname>::value) {           \
          for (sp_grad::index_type i = 1; i <= varname.iGetNumRows(); ++i) { \
               DEBUGCERR(#varname "(" << i << "):" << SpGradientTraits<T>::iGetSize(varname(i)) << "\n"); \
          }                                                             \
     }
#define DEBUG_DUMP_GRAD_MAT_SIZE(varname, tplname)                      \
     if (std::is_same<sp_grad::SpGradient, tplname>::value) {           \
          for (sp_grad::index_type i = 1; i <= varname.iGetNumRows(); ++i) { \
               for (sp_grad::index_type j = 1; j <= varname.iGetNumCols(); ++j) { \
                    DEBUGCERR(#varname "(" << i << "):" << SpGradientTraits<T>::iGetSize(varname(i, j)) << "\n"); \
               }                                                        \
          }                                                             \
     }
#define DEBUG_DUMP_GRAD_SCALAR_SIZE(varname, tplname)                      \
     if (std::is_same<sp_grad::SpGradient, tplname>::value) {           \
          DEBUGCERR(#varname ":" << SpGradientTraits<T>::iGetSize(varname) << "\n"); \
     }
#define DEBUG_DUMP_EXPR(expr) DEBUGCERR(#expr " = " << (expr) << "\n")
#else
#define DEBUG_DUMP_GRAD_VEC_SIZE(varname, tplname) {}
#define DEBUG_DUMP_GRAD_MAT_SIZE(varname, tplname) {}
#define DEBUG_DUMP_GRAD_SCALAR_SIZE(varname, tplname) {}
#define DEBUG_DUMP_EXPR(expr) static_cast<void>(0)
#endif

template <typename T>
void
ModalAd::AssRes(sp_grad::SpGradientAssVec<T>& WorkVec,
                const doublereal dCoef,
                const sp_grad::SpGradientVectorHandler<T>& XCurr,
                const sp_grad::SpGradientVectorHandler<T>& XPrimeCurr,
                const sp_grad::SpFunctionCall func)
{
     using namespace sp_grad;

     const integer iModalIndex = iGetFirstIndex();

     SpColVector<T> a(NModes, 1), aPrime(NModes, 1), b(NModes, 1), bPrime(NModes, 1);

     for (unsigned int i = 1; i <= NModes; ++i) {
          XCurr.dGetCoef(iModalIndex + i, a(i), dCoef);
          XPrimeCurr.dGetCoef(iModalIndex + i, aPrime(i), 1.);
     }

     for (unsigned int i = 1; i <= NModes; ++i) {
          XCurr.dGetCoef(iModalIndex + NModes + i, b(i), dCoef);
          XPrimeCurr.dGetCoef(iModalIndex + NModes + i, bPrime(i), 1.);
     }

#ifdef MODAL_USE_GRAVITY
     /* forza di gravita' (decidere come inserire g) */
     /* FIXME: use a reasonable reference point where compute gravity */
     ::Vec3 GravityAcceleration(::Zero3);
     const bool bGravity = GravityOwner::bGetGravity(this->x, GravityAcceleration);
#endif /* MODAL_USE_GRAVITY */

     // For optimum efficiency, memory should be always preallocated, although it might look ugly
     const index_type iSize_MaPP_CaP_Ka = NModes * 3
          + (rgModalStressStiff.size() > 0) * ((12 + NModes) * 6 + 2 * 3 * (6 + NModes) + SND.size() * (3 * (NModes + 6) + 3 * (NModes + 9)))
          + (oInv3.iGetNumCols() > 0) * (6 + bGravity * 3)
          + (oInv4.iGetNumCols() > 0) * (6 + NModes)
          + (oInv5.iGetNumCols() > 0) * (NModes + 3 + 3)
          + (oInv8.iGetNumCols() > 0 && oInv9.iGetNumCols() > 0) * (NModes + 3 + 3);

     SpColVector<T> MaPP_CaP_Ka(NModes, iSize_MaPP_CaP_Ka);

     MaPP_CaP_Ka = -(oModalMass * bPrime) - oModalDamp * b - oModalStiff * a;

     DEBUG_DUMP_EXPR(NModes);
     DEBUG_DUMP_EXPR(rgModalStressStiff.size());
     DEBUG_DUMP_GRAD_VEC_SIZE(MaPP_CaP_Ka, T);

     SpColVector<T, 3> Inv3jaj(3, NModes), Inv3jaPj(3, NModes), Inv3jaPPj(3, NModes);

     if (oInv3.iGetNumCols()) {
          Inv3jaj = oInv3 * a;
          Inv3jaPj = oInv3 * b;
          Inv3jaPPj = oInv3 * bPrime;
     }

     SpMatrix<T, 3, 3> Inv8jaj(3, 3, oInv8.iGetNumCols() ? NModes : 0), Inv8jaPj(3, 3, oInv8.iGetNumCols() ? NModes : 0);
     SpMatrix<T, 3> Inv5jaj(3, NModes, oInv5.iGetNumCols() ? NModes : 0), Inv5jaPj(3, NModes, oInv5.iGetNumCols() ? NModes: 0);
     SpMatrix<T, 3, 3> Inv9jkajak(3, 3, (oInv8.iGetNumCols() && oInv9.iGetNumCols()) ? 2 * NModes * NModes : 0);
     SpMatrix<T, 3, 3> Inv9jkajaPk(3, 3, (oInv8.iGetNumCols() && oInv9.iGetNumCols()) ? 2 * NModes * NModes: 0);
     SpMatrix<T, 3, 3> Inv10jaPj(3, 3, oInv10.iGetNumCols() ? NModes : 0);

     if (oInv5.iGetNumCols() || oInv8.iGetNumCols() || oInv9.iGetNumCols() || oInv10.iGetNumCols()) {
          for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
               const T& a_jMode = a(jMode);
               const T& aP_jMode = b(jMode);

               if (oInv5.iGetNumCols()) {
                    Inv5jaj += SubMatrix<1, 1, 3>(oInv5, (jMode - 1) * NModes + 1, 1, NModes) * a_jMode;
                    Inv5jaPj += SubMatrix<1, 1, 3>(oInv5, (jMode - 1) * NModes + 1, 1, NModes) * aP_jMode;
               }

               if (oInv8.iGetNumCols()) {
                    Inv8jaj += SubMatrix<3, 3>(oInv8, 1, 1, (jMode - 1) * 3 + 1, 1) * a_jMode;
                    Inv8jaPj += SubMatrix<3, 3>(oInv8, 1, 1, (jMode - 1) * 3 + 1, 1) * aP_jMode;

                    if (oInv9.iGetNumCols()) {
                         for (unsigned int kMode = 1; kMode <= NModes; kMode++) {
                              const index_type iOffset = (jMode - 1) * 3 * NModes + (kMode - 1) * 3 + 1;
                              Inv9jkajak += SubMatrix<3, 3>(oInv9, 1, 1, iOffset, 1) * a_jMode * a(kMode);
                              Inv9jkajaPk += SubMatrix<3, 3>(oInv9, 1, 1, iOffset, 1) * a_jMode * b(kMode);
                         }
                    }
               }

               if (oInv10.iGetNumCols()) {
                    Inv10jaPj += SubMatrix<3, 3>(oInv10, 1, 1, (jMode - 1) * 3 + 1, 1) * aP_jMode;
               }
          }
     }

     const integer iRigidIndex = pModalNode ? pModalNode->iGetFirstIndex() : -1;

     SpColVector<T, 3> x{this->x}, vP(3, 1), wP(3, 1), w(3, 1), RTw(3, 6);
     SpMatrix<T, 3, 3> R{this->R};
     SpColVector<T, 3> FTmp(3, 0), MTmp(3, 0);

     if (pModalNode) {
          pModalNode->GetXCurr(x, dCoef, func);
          pModalNode->GetXPPCurr(vP, dCoef, func);
          pModalNode->GetWPCurr(wP, dCoef, func);

          for (index_type i = 1; i <= 3; ++i) {
               XCurr.dGetCoef(iRigidIndex + 9 + i, w(i), dCoef);
          }

          pModalNode->GetRCurr(R, dCoef, func);

          RTw = Transpose(R) * w;

          SpMatrix<T, 3, 3> J(3, 3, (NModes + 1) * NModes);

          J = Inv7;

          if (oInv8.iGetNumCols()) {
               J += Inv8jaj + Transpose(Inv8jaj);
               if (oInv9.iGetNumCols()) {
                    J -= Inv9jkajak;
               }
          }

          SpMatrix<T, 3, 3> Jtmp = EvalUnique(J);

          J = (R * Jtmp) * Transpose(R);

          SpColVector<T, 3> STmp(3, NModes);

          STmp = Inv2;

          if (oInv3.iGetNumCols()) {
               STmp += Inv3jaj;
          }

          const SpColVector<T, 3> S = R * STmp;

          FTmp = vP * -dMass + Cross(S, wP) - Cross(w, Cross(w, S));

          if (oInv3.iGetNumCols()) {
               FTmp -= R * Inv3jaPPj + Cross(w, R * Inv3jaPj) * 2.;
          }

#ifdef MODAL_USE_GRAVITY
          if (bGravity) {
               FTmp += GravityAcceleration * dMass;
          }
#endif /* MODAL_USE_GRAVITY */

          MTmp = -Cross(S, vP) - J * wP - Cross(w, J * w);

          if (oInv4.iGetNumCols()) {
               MTmp -= R * (oInv4 * bPrime);
          }

          if (oInv5.iGetNumCols()) {
               MTmp -= R * (Inv5jaj * bPrime);
          }

          if (oInv8.iGetNumCols()) {
               SpMatrix<T, 3, 3> Tmp = Inv8jaPj;
               if (oInv9.iGetNumCols()) {
                    Tmp -= Inv9jkajaPk;
               }
               MTmp -= R * (Tmp * RTw * 2.);
          }

          if (oInv10.iGetNumCols()) {
               SpColVector<T, 3> VTmp = (Inv10jaPj + Transpose(Inv10jaPj)) * RTw;
               if (oInv11.iGetNumCols()) {
                    VTmp += Cross(w, (R * (oInv11 * b)));
               }
               MTmp -= R * VTmp;
          }

#ifdef MODAL_USE_GRAVITY
          if (bGravity) {
               MTmp += Cross(S, GravityAcceleration);
          }
#endif

          if (!rgModalStressStiff.empty()) {
               for (unsigned i = 0; i < oStressStiffIndexW.uGetSize(); ++i) {
                    const unsigned uIndexMatK0 = oStressStiffIndexW.uGetIndexMat(i);
                    const unsigned uIndexVecWq = oStressStiffIndexW.uGetIndexVec(i);

                    ASSERT(uIndexMatK0 >= 0u);
                    ASSERT(uIndexMatK0 < rgModalStressStiff.size());
                    ASSERT(uIndexVecWq >= 0u);
                    ASSERT(uIndexVecWq < 6);

                    static constexpr index_type idx1[] = {1, 2, 3, 1, 2, 3};
                    static constexpr index_type idx2[] = {1, 2, 3, 2, 3, 1};

                    const T RTwqi = RTw(idx1[uIndexVecWq]) * RTw(idx2[uIndexVecWq]);

                    DEBUG_DUMP_GRAD_SCALAR_SIZE(RTwqi, T);

                    MaPP_CaP_Ka -= (rgModalStressStiff[uIndexMatK0] * a) * RTwqi;
               }

               DEBUG_DUMP_GRAD_VEC_SIZE(MaPP_CaP_Ka, T);

               for (unsigned i = 0; i < oStressStiffIndexWP.uGetSize(); ++i) {
                    const unsigned uIndexMatK0 = oStressStiffIndexWP.uGetIndexMat(i);
                    const unsigned uIndexVecWP = oStressStiffIndexWP.uGetIndexVec(i);

                    ASSERT(uIndexMatK0 >= 0u);
                    ASSERT(uIndexMatK0 < rgModalStressStiff.size());
                    ASSERT(uIndexVecWP >= 0u);
                    ASSERT(uIndexVecWP < 3u);

                    const T RTwPi = Dot(R.GetCol(uIndexVecWP + 1), wP);

                    DEBUG_DUMP_GRAD_SCALAR_SIZE(RTwPi, T);

                    MaPP_CaP_Ka -= (rgModalStressStiff[uIndexMatK0] * a) * RTwPi;
               }

               DEBUG_DUMP_GRAD_VEC_SIZE(MaPP_CaP_Ka, T);

               for (unsigned i = 0; i < oStressStiffIndexVP.uGetSize(); ++i) {
                    const unsigned uIndexMatK0 = oStressStiffIndexVP.uGetIndexMat(i);
                    const unsigned uIndexVecVP = oStressStiffIndexVP.uGetIndexVec(i);

                    ASSERT(uIndexMatK0 >= 0u);
                    ASSERT(uIndexMatK0 < rgModalStressStiff.size());
                    ASSERT(uIndexVecVP >= 0u);
                    ASSERT(uIndexVecVP < 3u);

                    const T RTvPi = Dot(R.GetCol(uIndexVecVP + 1), vP - GravityAcceleration);

                    DEBUG_DUMP_GRAD_SCALAR_SIZE(RTvPi, T);

                    MaPP_CaP_Ka -= (rgModalStressStiff[uIndexMatK0] * a) * RTvPi;
               }

               DEBUG_DUMP_GRAD_VEC_SIZE(MaPP_CaP_Ka, T);
          }
     }

     for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
          const index_type jOffset = (jMode - 1) * 3 + 1;
          T& d = MaPP_CaP_Ka(jMode);

          DEBUG_DUMP_GRAD_SCALAR_SIZE(d, T);

          if (oInv3.iGetNumCols()) {
               const SpColVector<T, 3> RInv3j = R * SubMatrix<3, 1>(oInv3, 1, 1, jMode, 1);

               d -= Dot(RInv3j, vP);

               DEBUG_DUMP_GRAD_SCALAR_SIZE(d, T);

#ifdef MODAL_USE_GRAVITY
               if (bGravity) {
                    d += Dot(RInv3j, GravityAcceleration);
               }

               DEBUG_DUMP_GRAD_SCALAR_SIZE(d, T);
#endif /* MODAL_USE_GRAVITY */
          }

          if (oInv4.iGetNumCols()) {
               SpColVector<T, 3> Inv4j(3, NModes);

               Inv4j = oInv4.GetVec(jMode);

               if (oInv5.iGetNumCols()) {
                    Inv4j += Inv5jaj.GetCol(jMode);

                    d -= Dot(R * Inv5jaPj.GetCol(jMode), w) * 2.;

                    DEBUG_DUMP_GRAD_SCALAR_SIZE(d, T);
               }

               d -= Dot(R * Inv4j, wP);

               DEBUG_DUMP_GRAD_SCALAR_SIZE(d, T);
          }

          if (oInv8.iGetNumCols() || oInv9.iGetNumCols() || oInv10.iGetNumCols()) {
               SpMatrix<T, 3, 3> MatTmp(3, 3, NModes);

               if (oInv8.iGetNumCols()) {
                    MatTmp = Transpose(SubMatrix<3, 3>(oInv8, 1, 1, jOffset, 1));

                    if (oInv9.iGetNumCols()) {
                         for (unsigned int kModem1 = 0; kModem1 < NModes; kModem1++) {
                              const index_type kOffset = (jMode - 1) * 3 * NModes + kModem1 * 3 + 1;

                              MatTmp -= SubMatrix<3, 3>(oInv9, 1, 1, kOffset, 1) * a(kModem1 + 1);
                         }
                    }
               }

               if (oInv10.iGetNumCols()) {
                    MatTmp += SubMatrix<3, 3>(oInv10, 1, 1, jOffset, 1);
               }

               DEBUG_DUMP_GRAD_MAT_SIZE(MatTmp, T);

               d += Dot(w, R * (MatTmp * RTw));

               DEBUG_DUMP_GRAD_SCALAR_SIZE(d, T);
          }
     }

     for (unsigned int iCnt = 1; iCnt <= NModes; iCnt++) {
          WorkVec.AddItem(iModalIndex + iCnt, b(iCnt) - aPrime(iCnt));
     }

     for (unsigned int iStrNode = 1; iStrNode <= NStrNodes; iStrNode++) {
          const index_type iStrNodem1 = iStrNode - 1;
          const integer iNodeFirstMomIndex = SND[iStrNodem1].pNodeAd->iGetFirstMomentumIndex();

          SpColVector<T, 3> PHIta(3, NModes), PHIra(3, NModes);

          for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
               const index_type iOffset = (jMode - 1) * NStrNodes + iStrNode;
               PHIta += SubMatrix<3, 1>(oPHIt, 1, 1, iOffset, 1) * a(jMode);
               PHIra += SubMatrix<3, 1>(oPHIr, 1, 1, iOffset, 1) * a(jMode);
          }

          const SpColVector<T, 3> d1tot = R * (PHIta + SND[iStrNodem1].OffsetFEM);
          const SpMatrix<T, 3, 3> R1tot = R * MatCrossVec(PHIra, 1.);

          SpColVector<T, 3> F(3, 1);

          for (index_type i = 1; i <= 3; ++i) {
               XCurr.dGetCoef(iModalIndex + 2 * NModes + 6 * iStrNodem1 + i, F(i), 1.);
          }

          SpColVector<T, 3> x2(3, 1);

          const StructNodeAd* const pStrNodem1 = SND[iStrNodem1].pNodeAd;

          ASSERT(pStrNodem1 != nullptr);

          pStrNodem1->GetXCurr(x2, dCoef, func);

          SpMatrix<T, 3, 3> R2(3, 3, 3);

          pStrNodem1->GetRCurr(R2, dCoef, func);

          const SpColVector<T, 3> dTmp2(R2 * SND[iStrNodem1].OffsetMB);

          if (pModalNode) {
               FTmp -= F;
               MTmp -= Cross(d1tot, F);
          }

          SpColVector<T, 3> vtemp = Transpose(R) * F;

          for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
               const index_type iOffset = (jMode - 1) * NStrNodes + iStrNode;
               const T d = Dot(-vtemp, oPHIt.GetVec(iOffset));
               WorkVec.AddItem(iModalIndex + NModes + jMode, d);
          }

          if (!rgModalStressStiff.empty()) {
               DEBUG_DUMP_GRAD_VEC_SIZE(vtemp, T);

               for (unsigned i = 0; i < SND[iStrNodem1].oStressStiffIndexF.uGetSize(); ++i) {
                    const unsigned uIndexMatK0 = SND[iStrNodem1].oStressStiffIndexF.uGetIndexMat(i);
                    const unsigned uIndexVecF = SND[iStrNodem1].oStressStiffIndexF.uGetIndexVec(i);

                    ASSERT(uIndexMatK0 >= 0);
                    ASSERT(uIndexMatK0 < rgModalStressStiff.size());
                    ASSERT(uIndexVecF >= 0);
                    ASSERT(uIndexVecF < 3u);

                    MaPP_CaP_Ka += (rgModalStressStiff[uIndexMatK0] * a) * vtemp(uIndexVecF + 1);
               }

               DEBUG_DUMP_GRAD_VEC_SIZE(MaPP_CaP_Ka, T);
          }

          SpColVector<T, 3> M(3, 1);

          for (index_type i = 1; i <= 3; ++i) {
               XCurr.dGetCoef(iModalIndex + 2 * NModes + 6 * iStrNodem1 + 3 + i, M(i), 1.);
          }

          const SpMatrix<T, 3, 3> DeltaR = Transpose(R2) * R1tot;
          const SpColVector<T, 3> ThetaCurr = VecRotMat(DeltaR);
          const SpColVector<T, 3> R2_M = R2 * M;

          if (pModalNode) {
               MTmp -= R2_M;
          }

          vtemp = Transpose(R) * R2_M;

          for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
               const index_type iOffset = (jMode - 1) * NStrNodes + iStrNode;
               const T d = Dot(-vtemp, oPHIr.GetVec(iOffset));
               WorkVec.AddItem(iModalIndex + NModes + jMode, d);
          }

          if (!rgModalStressStiff.empty()) {
               DEBUG_DUMP_GRAD_VEC_SIZE(vtemp, T);

               for (unsigned i = 0; i < SND[iStrNodem1].oStressStiffIndexM.uGetSize(); ++i) {
                    const unsigned uIndexMatK0 = SND[iStrNodem1].oStressStiffIndexM.uGetIndexMat(i);
                    const unsigned uIndexVecM = SND[iStrNodem1].oStressStiffIndexM.uGetIndexVec(i);

                    ASSERT(uIndexMatK0 >= 0u);
                    ASSERT(uIndexMatK0 < rgModalStressStiff.size());
                    ASSERT(uIndexVecM >= 0u);
                    ASSERT(uIndexVecM < 3u);

                    MaPP_CaP_Ka += (rgModalStressStiff[uIndexMatK0] * a) * vtemp(uIndexVecM + 1);
               }

               DEBUG_DUMP_GRAD_VEC_SIZE(MaPP_CaP_Ka, T);
          }

          ASSERT(dCoef != 0.);

          const SpColVector<T, 3> f1 = (x2 + dTmp2 - x - d1tot) / dCoef;
          const SpColVector<T, 3> f2 = ThetaCurr / -dCoef;

          WorkVec.AddItem(iModalIndex + 2 * NModes + 6 * iStrNodem1 + 1, f1);
          WorkVec.AddItem(iModalIndex + 2 * NModes + 6 * iStrNodem1 + 4, f2);

          const SpColVector<T, 3> MTmp2 = Cross(dTmp2, F) + R2_M;

          WorkVec.AddItem(iNodeFirstMomIndex + 1, F);
          WorkVec.AddItem(iNodeFirstMomIndex + 4, MTmp2);

          UpdateStrNodeData(SND[iStrNodem1], d1tot, R1tot, F, M, R2);
     }

     for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
          ASSERT(SpGradientTraits<T>::iGetSize(MaPP_CaP_Ka(jMode)) <= iSize_MaPP_CaP_Ka);

          DEBUG_DUMP_EXPR(iSize_MaPP_CaP_Ka);
          DEBUG_DUMP_GRAD_SCALAR_SIZE(MaPP_CaP_Ka(jMode), T);

          WorkVec.AddItem(iModalIndex + NModes + jMode, MaPP_CaP_Ka(jMode));
     }

     if (pModalNode) {
          WorkVec.AddItem(iRigidIndex + 7, FTmp);
          WorkVec.AddItem(iRigidIndex + 10, MTmp);
     }

     if (pModalNode) {
          UpdateModalNode(x, R);
     }

     UpdateState(a, aPrime, b, bPrime);
     UpdateInvariants(Inv3jaj, Inv8jaj, Inv9jkajak);
}
