/*
 * MBDyn (C) is a multibody analysis code.
 * http://www.mbdyn.org
 *
 * Copyright (C) 1996-2023
 *
 * Pierangelo Masarati  <masarati@aero.polimi.it>
 * Paolo Mantegazza     <mantegazza@aero.polimi.it>
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

#include <array>
#include <memory>

#include "presnodead.h"
#include "strnodead.h"
#include "sp_matvecass.h"
#include "solidpress.h"
#include "solidinteg.h"
#include "solidshape.h"

template <sp_grad::index_type iNumNodes>
class PressureFromNodes {
public:
     PressureFromNodes()
          :rgNodes{nullptr} {
     }

     template <typename T>
     inline void
     GetNodalPressure(sp_grad::SpColVector<T, iNumNodes>& p,
                      doublereal dCoef,
                      sp_grad::SpFunctionCall func) const;


     void SetNode(sp_grad::index_type i, const ScalarNodeAd* pNode) {
          ASSERT(i >= 0);
          ASSERT(i < iNumNodes);

          rgNodes[i] = pNode;
     }

     const ScalarNodeAd* pGetNode(sp_grad::index_type i) const {
          ASSERT(i >= 0);
          ASSERT(i < iNumNodes);

          return rgNodes[i];
     }

     static inline constexpr int
     GetNumConnectedNodes() { return iNumNodes; }

     inline void
     GetConnectedNodes(std::vector<const Node*>& connectedNodes) const {
          for (const ScalarNodeAd* pNode: rgNodes) {
               connectedNodes.push_back(pNode);
          }
     }

     void PrintLogFile(std::ostream& of) const {
          for (const ScalarNodeAd* pNode: rgNodes) {
               of << ' ' << pNode->GetLabel();
          }
     }
private:
     std::array<const ScalarNodeAd*, iNumNodes> rgNodes;
};

template <sp_grad::index_type iNumDrives>
class PressureFromDrives {
public:
     PressureFromDrives() {}
     PressureFromDrives(PressureFromDrives&& oPressureTmp)
          :rgDrives(std::move(oPressureTmp.rgDrives)) {
     }

     template <typename T>
     inline void
     GetNodalPressure(sp_grad::SpColVector<T, iNumDrives>& p,
                      doublereal dCoef,
                      sp_grad::SpFunctionCall func) const;

     void SetDrive(sp_grad::index_type i, std::unique_ptr<DriveCaller>&& pDrive) {
          ASSERT(i >= 0);
          ASSERT(i < iNumDrives);

          rgDrives[i] = std::move(pDrive);
     }

     static inline constexpr int
     GetNumConnectedNodes() { return 0; }

     static inline void
     GetConnectedNodes(std::vector<const Node*>&) {
     }

     void PrintLogFile(std::ostream& of) const {
          for (const auto& pDrive: rgDrives) {
               of << ' ' << pDrive->GetLabel();
          }
     }
private:
     std::array<std::unique_ptr<DriveCaller>, iNumDrives> rgDrives;
};

template <sp_grad::index_type iNumDrives>
class SurfLoadFromDrives {
public:
     SurfLoadFromDrives() {}
     SurfLoadFromDrives(SurfLoadFromDrives&& oLoadTmp)
          :rgDrives(std::move(oLoadTmp.rgDrives)) {
     }

     template <typename T>
     inline void
     GetNodalLoad(sp_grad::SpMatrix<T, 3, iNumDrives>& F,
                  doublereal dCoef,
                  sp_grad::SpFunctionCall func) const {
          using namespace sp_grad;

          for (index_type j = 1; j <= iNumDrives; ++j) {
               const Vec3 Fj = rgDrives[j - 1]->Get();

               for (index_type i = 1; i <= 3; ++i) {
                    F(i, j) = Fj(i);
               }
          }
     }

     void SetDrive(sp_grad::index_type i, std::unique_ptr<TplDriveCaller<Vec3>>&& pDrive) {
          ASSERT(i >= 0);
          ASSERT(i < iNumDrives);

          rgDrives[i] = std::move(pDrive);
     }

     static inline constexpr int
     GetNumConnectedNodes() { return 0; }

     static inline void
     GetConnectedNodes(std::vector<const Node*>&) {
     }

     void PrintLogFile(std::ostream& of) const {
     }
private:
     std::array<std::unique_ptr<TplDriveCaller<Vec3>>, iNumDrives> rgDrives;
};

template <typename ElementType, typename CollocationType, typename PressureSource>
class SurfaceLoad: public SurfaceLoadElem {
public:
     using SurfaceLoadElem::AssRes;
     using SurfaceLoadElem::AssJac;
     using SurfaceLoadElem::InitialAssRes;
     using SurfaceLoadElem::InitialAssJac;

     static constexpr sp_grad::index_type iNumNodes = ElementType::iNumNodes;
     static constexpr sp_grad::index_type iNumEvalPoints = CollocationType::iNumEvalPoints;
     static constexpr sp_grad::index_type iNumDof = iNumNodes * 3;

     SurfaceLoad(unsigned uLabel,
                 const std::array<const StructDispNodeAd*, iNumNodes>& rgNodes,
                 PressureSource&& oPressureTmp,
                 flag fOut);
     virtual ~SurfaceLoad();

     virtual int
     GetNumConnectedNodes() const override;

     virtual void
     GetConnectedNodes(std::vector<const Node*>& connectedNodes) const override;

protected:
     template <typename T>
     inline void
     AssVector(sp_grad::SpGradientAssVec<T>& WorkVec,
               sp_grad::SpColVector<T, iNumDof>& R,
               integer (StructDispNode::*pfnGetFirstIndex)(void) const);

     template <typename T>
     inline void
     GetNodalPosition(sp_grad::SpColVector<T, 3 * iNumNodes>& x,
                      doublereal dCoef,
                      sp_grad::SpFunctionCall func) const;

     inline void
     UpdateTotalForce(const sp_grad::SpColVector<doublereal, iNumDof>& R);

     inline void
     UpdateTotalForce(const sp_grad::SpColVector<sp_grad::SpGradient, iNumDof>& R) {}

     inline void
     UpdateTotalForce(const sp_grad::SpColVector<sp_grad::GpGradProd, iNumDof>& R) {}

     std::array<const StructDispNodeAd*, iNumNodes> rgNodes;
     PressureSource oPressure;

     struct CollocData {
          static constexpr sp_grad::index_type iNumNodes = SurfaceLoad::iNumNodes;

          sp_grad::SpColVectorA<doublereal, iNumNodes> HA;
          sp_grad::SpMatrixA<doublereal, 3, 3 * iNumNodes> Hf, dHf_dr, dHf_ds;
     };

     template <typename CollocDataType>
     inline void InitCollocData(std::array<CollocDataType, iNumEvalPoints>& rgCollocData);
     doublereal A0;
};

template <typename ElementType, typename CollocationType, typename PressureSource>
class PressureLoad: public SurfaceLoad<ElementType, CollocationType, PressureSource> {
     typedef SurfaceLoad<ElementType, CollocationType, PressureSource> BaseType;
public:
     using SurfaceLoadElem::AssRes;
     using SurfaceLoadElem::AssJac;
     using SurfaceLoadElem::InitialAssRes;
     using SurfaceLoadElem::InitialAssJac;

     using BaseType::iNumNodes;
     using BaseType::iNumEvalPoints;
     using BaseType::iNumDof;

     PressureLoad(unsigned uLabel,
                  const std::array<const StructDispNodeAd*, iNumNodes>& rgNodes,
                  PressureSource&& oPressureTmp,
                  flag fOut);
     virtual ~PressureLoad();

     virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const override;

     template <typename T>
     inline void
     AssRes(sp_grad::SpGradientAssVec<T>& WorkVec,
            doublereal dCoef,
            const sp_grad::SpGradientVectorHandler<T>& XCurr,
            const sp_grad::SpGradientVectorHandler<T>& XPrimeCurr,
            enum sp_grad::SpFunctionCall func);

     virtual SubVectorHandler&
     AssRes(SubVectorHandler& WorkVec,
            doublereal dCoef,
            const VectorHandler& XCurr,
            const VectorHandler& XPrimeCurr) override;

     virtual VariableSubMatrixHandler&
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

     template <typename T>
     inline void
     InitialAssRes(sp_grad::SpGradientAssVec<T>& WorkVec,
                   const sp_grad::SpGradientVectorHandler<T>& XCurr,
                   enum sp_grad::SpFunctionCall func);

     virtual VariableSubMatrixHandler&
     InitialAssJac(VariableSubMatrixHandler& WorkMat,
                   const VectorHandler& XCurr) override;

     virtual SubVectorHandler&
     InitialAssRes(SubVectorHandler& WorkVec,
                   const VectorHandler& XCurr) override;

     virtual void
     InitialWorkSpaceDim(integer* piNumRows,
                         integer* piNumCols) const override;

protected:
     template <typename T>
     inline void
     AssPressureLoad(sp_grad::SpColVector<T, iNumNodes * 3>& f,
                     doublereal dCoef,
                     enum sp_grad::SpFunctionCall func);

     std::array<typename BaseType::CollocData, iNumEvalPoints> rgCollocData;
};

enum class SurfaceTractionType {
     RELATIVE,
     ABSOLUTE
};

template <SurfaceTractionType eType>
struct SurfaceTractionHelper;

template <>
struct SurfaceTractionHelper<SurfaceTractionType::RELATIVE> {
     template <typename T, sp_grad::index_type iNumCols, typename ElementType>
     static inline void
     AssSurfaceTraction(ElementType& oElem,
                        sp_grad::SpColVector<T, iNumCols>& f,
                        doublereal dCoef,
                        enum sp_grad::SpFunctionCall func) {
          oElem.AssSurfaceTractionRel(f, dCoef, func);
     }

     template <typename ElementType, size_t uNumCols>
     static inline void InitCollocData(ElementType& oElem, const std::array<Mat3x3, uNumCols>& Rf) {
          oElem.InitCollocDataRel(Rf);
     }
};

template <>
struct SurfaceTractionHelper<SurfaceTractionType::ABSOLUTE> {
     template <typename T, sp_grad::index_type iNumCols, typename ElementType>
     static inline void
     AssSurfaceTraction(ElementType& oElem,
                        sp_grad::SpColVector<T, iNumCols>& f,
                        doublereal dCoef,
                        enum sp_grad::SpFunctionCall func) {
          oElem.AssSurfaceTractionAbs(f, dCoef, func);
     }

     template <typename ElementType, size_t uNumCols>
     static inline void InitCollocData(ElementType& oElem, const std::array<Mat3x3, uNumCols>& Rf) {
          oElem.InitCollocDataAbs(Rf);
     }
};

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
class SurfaceTraction: public SurfaceLoad<ElementType, CollocationType, PressureSource> {
     typedef SurfaceLoad<ElementType, CollocationType, PressureSource> BaseType;
public:
     using SurfaceLoadElem::AssRes;
     using SurfaceLoadElem::AssJac;
     using SurfaceLoadElem::InitialAssRes;
     using SurfaceLoadElem::InitialAssJac;

     using BaseType::iNumNodes;
     using BaseType::iNumEvalPoints;
     using BaseType::iNumDof;

     SurfaceTraction(unsigned uLabel,
                     const std::array<const StructDispNodeAd*, iNumNodes>& rgNodes,
                     PressureSource&& oPressureTmp,
                     const std::array<Mat3x3, iNumEvalPoints>& Rf,
                     flag fOut);
     virtual ~SurfaceTraction();

     virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const override;

     template <typename T>
     inline void
     AssRes(sp_grad::SpGradientAssVec<T>& WorkVec,
            doublereal dCoef,
            const sp_grad::SpGradientVectorHandler<T>& XCurr,
            const sp_grad::SpGradientVectorHandler<T>& XPrimeCurr,
            enum sp_grad::SpFunctionCall func);

     virtual SubVectorHandler&
     AssRes(SubVectorHandler& WorkVec,
            doublereal dCoef,
            const VectorHandler& XCurr,
            const VectorHandler& XPrimeCurr) override;

     virtual VariableSubMatrixHandler&
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

     template <typename T>
     inline void
     InitialAssRes(sp_grad::SpGradientAssVec<T>& WorkVec,
                   const sp_grad::SpGradientVectorHandler<T>& XCurr,
                   enum sp_grad::SpFunctionCall func);

     virtual VariableSubMatrixHandler&
     InitialAssJac(VariableSubMatrixHandler& WorkMat,
                   const VectorHandler& XCurr) override;

     virtual SubVectorHandler&
     InitialAssRes(SubVectorHandler& WorkVec,
                   const VectorHandler& XCurr) override;

     virtual void
     InitialWorkSpaceDim(integer* piNumRows,
                         integer* piNumCols) const override;

     template <typename T>
     inline void
     AssSurfaceTractionRel(sp_grad::SpColVector<T, iNumNodes * 3>& f,
                           doublereal dCoef,
                           enum sp_grad::SpFunctionCall func);

     template <typename T>
     inline void
     AssSurfaceTractionAbs(sp_grad::SpColVector<T, iNumNodes * 3>& f,
                           doublereal dCoef,
                           enum sp_grad::SpFunctionCall func);

     inline void InitCollocDataRel(const std::array<Mat3x3, iNumEvalPoints>& Rf);
     inline void InitCollocDataAbs(const std::array<Mat3x3, iNumEvalPoints>& Rf);
private:
     struct CollocData: public BaseType::CollocData {
          sp_grad::SpMatrixA<doublereal, 3, 3> Rrel;
          doublereal dA;
     };

     std::array<CollocData, iNumEvalPoints> rgCollocData;
};

template <sp_grad::index_type iNumDrives>
template <typename T>
inline void
PressureFromDrives<iNumDrives>::GetNodalPressure(sp_grad::SpColVector<T, iNumDrives>& p,
                                                 doublereal dCoef,
                                                 sp_grad::SpFunctionCall func) const
{
     using namespace sp_grad;

     for (index_type j = 1; j <= iNumDrives; ++j) {
          SpGradientTraits<T>::ResizeReset(p(j), rgDrives[j - 1]->dGet(), 0);
     }
}

template <sp_grad::index_type iNumNodes>
template <typename T>
inline void
PressureFromNodes<iNumNodes>::GetNodalPressure(sp_grad::SpColVector<T, iNumNodes>& p,
                                               doublereal dCoef,
                                               sp_grad::SpFunctionCall func) const
{
     using namespace sp_grad;

     for (index_type j = 1; j <= iNumNodes; ++j) {
          rgNodes[j - 1]->GetX(p(j), dCoef, func);
     }
}

SurfaceLoadElem::SurfaceLoadElem(unsigned uLabel,
                                 flag fOut)
     :InitialAssemblyElem(uLabel, fOut),
      Ftot(::Zero3)
{
}

SurfaceLoadElem::~SurfaceLoadElem()
{
}

Elem::Type SurfaceLoadElem::GetElemType() const
{
     return Elem::SURFACE_LOAD;
}

void
SurfaceLoadElem::SetValue(DataManager *pDM,
                          VectorHandler& X, VectorHandler& XP,
                          SimulationEntity::Hints *ph)
{
}

std::ostream& SurfaceLoadElem::Restart(std::ostream& out) const
{
     out << "## pressure load element: Restart not implemented yet\n";

     return out;
}

void SurfaceLoadElem::Restart(RestartData& oData, RestartData::RestartAction eAction)
{
}

unsigned int SurfaceLoadElem::iGetInitialNumDof() const
{
     return 0;
}

bool SurfaceLoadElem::bIsDeformable() const
{
     return false;
}

void SurfaceLoadElem::Output(OutputHandler& OH) const
{
     using namespace sp_grad;

     if (bToBeOutput() && OH.UseText(OutputHandler::SURFACE_LOADS)) {
          if (OH.UseText(OutputHandler::SURFACE_LOADS)) {
               std::ostream& of = OH.SurfaceLoads();

               of << std::setw(8) << GetLabel() << ' ' << Ftot << '\n';
          }
     }
}

template <typename ElementType, typename CollocationType, typename PressureSource>
SurfaceLoad<ElementType, CollocationType, PressureSource>::SurfaceLoad(unsigned uLabel,
                                                                       const std::array<const StructDispNodeAd*, iNumNodes>& rgNodesTmp,
                                                                       PressureSource&& oPressureTmp,
                                                                       flag fOut)
     :SurfaceLoadElem(uLabel, fOut),
      rgNodes(rgNodesTmp),
      oPressure(std::move(oPressureTmp)),
      A0(0.)
{
}

template <typename ElementType, typename CollocationType, typename PressureSource>
SurfaceLoad<ElementType, CollocationType, PressureSource>::~SurfaceLoad()
{
}

template <typename ElementType, typename CollocationType, typename PressureSource>
template <typename T>
inline void
SurfaceLoad<ElementType, CollocationType, PressureSource>::AssVector(sp_grad::SpGradientAssVec<T>& WorkVec,
                                                                     sp_grad::SpColVector<T, iNumDof>& R,
                                                                     integer (StructDispNode::*pfnGetFirstIndex)(void) const)
{
     using namespace sp_grad;

     for (index_type i = 1; i <= iNumNodes; ++i) {
          const index_type iEqIndex = (rgNodes[i - 1]->*pfnGetFirstIndex)();

          for (index_type j = 1; j <= 3; ++j) {
               WorkVec.AddItem(iEqIndex + j, R((i - 1) * 3 + j));
          }
     }

     UpdateTotalForce(R);
}

template <typename ElementType, typename CollocationType, typename PressureSource>
int
SurfaceLoad<ElementType, CollocationType, PressureSource>::GetNumConnectedNodes() const
{
     return iNumNodes + oPressure.GetNumConnectedNodes();
}

template <typename ElementType, typename CollocationType, typename PressureSource>
void
SurfaceLoad<ElementType, CollocationType, PressureSource>::GetConnectedNodes(std::vector<const Node*>& connectedNodes) const
{
     connectedNodes.reserve(GetNumConnectedNodes());
     connectedNodes.clear();

     for (const Node* pNode:rgNodes) {
          connectedNodes.push_back(pNode);
     }

     oPressure.GetConnectedNodes(connectedNodes);
}

template <typename ElementType, typename CollocationType, typename PressureSource>
template <typename T>
inline void
SurfaceLoad<ElementType, CollocationType, PressureSource>::GetNodalPosition(sp_grad::SpColVector<T, 3 * iNumNodes>& x,
                                                                            doublereal dCoef,
                                                                            sp_grad::SpFunctionCall func) const
{
     using namespace sp_grad;

     SpColVector<T, 3> Xj(3, 1);

     for (index_type j = 1; j <= iNumNodes; ++j) {
          rgNodes[j - 1]->GetXCurr(Xj, dCoef, func);

          for (index_type i = 1; i <= 3; ++i) {
               x(i + (j - 1) * 3) = std::move(Xj(i));
          }
     }
}

template <typename ElementType, typename CollocationType, typename PressureSource>
inline void
SurfaceLoad<ElementType, CollocationType, PressureSource>::UpdateTotalForce(const sp_grad::SpColVector<doublereal, iNumDof>& R)
{
     using namespace sp_grad;

     Ftot = ::Zero3;

     for (index_type j = 1; j <= iNumNodes; ++j) {
          for (index_type i = 1; i <= 3; ++i) {
               Ftot(i) += R((j - 1) * 3 + i);
          }
     }
}

template <typename ElementType, typename CollocationType, typename PressureSource>
template <typename CollocDataType>
inline void
SurfaceLoad<ElementType, CollocationType, PressureSource>::InitCollocData(std::array<CollocDataType, iNumEvalPoints>& rgCollocData)
{
     using namespace sp_grad;

     SpColVector<doublereal, 2> r(2, 0);
     SpMatrix<doublereal, iNumNodes, 2> hd(iNumNodes, 2, 0);

     SpColVector<doublereal, iNumDof> x(iNumDof, 0);

     GetNodalPosition(x, 1., sp_grad::SpFunctionCall::REGULAR_RES);

     for (index_type i = 0; i < iNumEvalPoints; ++i) {
          CollocationType::GetPosition(i, r);
          ElementType::ShapeFunction(r, rgCollocData[i].HA);
          ElementType::ShapeFunctionDeriv(r, hd);

          for (index_type k = 1; k <= iNumNodes; ++k) {
               for (index_type j = 1; j <= 3; ++j) {
                    rgCollocData[i].Hf(j, (k - 1) * 3 + j) = rgCollocData[i].HA(k);
                    rgCollocData[i].dHf_dr(j, (k - 1) * 3 + j) = hd(k, 1);
                    rgCollocData[i].dHf_ds(j, (k - 1) * 3 + j) = hd(k, 2);
               }
          }

          const SpColVector<doublereal, 3> n1 = rgCollocData[i].dHf_dr * x;
          const SpColVector<doublereal, 3> n2 = rgCollocData[i].dHf_ds * x;
          const SpColVector<doublereal, 3> n3 = Cross(n1, n2);
          const doublereal alpha = CollocationType::dGetWeight(i);
          const doublereal detJA = Norm(n3);

          if (detJA <= 0.) {
               silent_cerr("surface load(" << this->GetLabel() << "): surface normal vector is singular ("
                           << n1 << "," << n2 << ", " << n3 << ")\n");
               throw ErrGeneric(MBDYN_EXCEPT_ARGS);
          }

          A0 += alpha * detJA;
     }

     DEBUGCERR("SurfaceLoad(" << GetLabel() << "): A0=" << A0 << "\n");
}

template <typename ElementType, typename CollocationType, typename PressureSource>
PressureLoad<ElementType, CollocationType, PressureSource>::PressureLoad(unsigned uLabel,
                                                                         const std::array<const StructDispNodeAd*, iNumNodes>& rgNodesTmp,
                                                                         PressureSource&& oPressureTmp,
                                                                         flag fOut)
:BaseType(uLabel, rgNodesTmp, std::move(oPressureTmp), fOut)
{
     BaseType::InitCollocData(rgCollocData);
}

template <typename ElementType, typename CollocationType, typename PressureSource>
PressureLoad<ElementType, CollocationType, PressureSource>::~PressureLoad()
{
}

template <typename ElementType, typename CollocationType, typename PressureSource>
void PressureLoad<ElementType, CollocationType, PressureSource>::WorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
     *piNumRows = iNumDof;
     *piNumCols = 0;
}

template <typename ElementType, typename CollocationType, typename PressureSource>
template <typename T>
inline void
PressureLoad<ElementType, CollocationType, PressureSource>::AssRes(sp_grad::SpGradientAssVec<T>& WorkVec,
                                                                   doublereal dCoef,
                                                                   const sp_grad::SpGradientVectorHandler<T>& XCurr,
                                                                   const sp_grad::SpGradientVectorHandler<T>& XPrimeCurr,
                                                                   enum sp_grad::SpFunctionCall func)
{
     using namespace sp_grad;

     constexpr index_type iCapacity = iNumDof + PressureSource::GetNumConnectedNodes();

     SpColVector<T, iNumNodes * 3> R(iNumDof, iCapacity);

     AssPressureLoad(R, dCoef, func);

     ASSERT(R.iGetMaxSize() <= iCapacity);

     this->AssVector(WorkVec, R, &StructDispNodeAd::iGetFirstMomentumIndex);
}

template <typename ElementType, typename CollocationType, typename PressureSource>
template <typename T>
inline void
PressureLoad<ElementType, CollocationType, PressureSource>::AssPressureLoad(sp_grad::SpColVector<T, iNumNodes * 3>& R,
                                                                            doublereal dCoef,
                                                                            enum sp_grad::SpFunctionCall func)
{
     using namespace sp_grad;

     T p_i;

     SpColVector<T, 3 * iNumNodes> x(3 * iNumNodes, 1);
     SpColVector<T, iNumNodes> p(iNumNodes, 1);
     SpColVector<T, 3> n1(3, iNumDof), n2(3, iNumDof), n(3, iNumDof), F_i(3, iNumDof + iNumNodes);

     this->GetNodalPosition(x, dCoef, func);
     this->oPressure.GetNodalPressure(p, dCoef, func);

     sp_grad::SpGradExpDofMapHelper<T> oDofMap;

     oDofMap.GetDofStat(x);
     oDofMap.GetDofStat(p);
     oDofMap.Reset();
     oDofMap.InsertDof(x);
     oDofMap.InsertDof(p);
     oDofMap.InsertDone();

     for (auto& g: R) {
          oDofMap.InitDofMap(g);
     }

     for (index_type i = 0; i < iNumEvalPoints; ++i) {
          const doublereal alpha = CollocationType::dGetWeight(i);

          p_i = Dot(rgCollocData[i].HA, p);
          n1.MapAssign(rgCollocData[i].dHf_dr * x, oDofMap);
          n2.MapAssign(rgCollocData[i].dHf_ds * x, oDofMap);
          n.MapAssign(Cross(n1, n2), oDofMap);
          F_i.MapAssign(n * (-alpha * p_i), oDofMap);

          for (index_type k = 1; k <= iNumDof; ++k) {
               for (index_type j = 1; j <= 3; ++j) {
                    oDofMap.Add(R(k), rgCollocData[i].Hf(j, k) * F_i(j));
               }
          }
     }
}

template <typename ElementType, typename CollocationType, typename PressureSource>
SubVectorHandler&
PressureLoad<ElementType, CollocationType, PressureSource>::AssRes(SubVectorHandler& WorkVec,
                                                                   doublereal dCoef,
                                                                   const VectorHandler& XCurr,
                                                                   const VectorHandler& XPrimeCurr)
{
     DEBUGCOUTFNAME("PressureLoad::AssRes");

     sp_grad::SpGradientAssVec<doublereal>::AssRes(this,
                                                   WorkVec,
                                                   dCoef,
                                                   XCurr,
                                                   XPrimeCurr,
                                                   sp_grad::SpFunctionCall::REGULAR_RES);

     return WorkVec;
}

template <typename ElementType, typename CollocationType, typename PressureSource>
VariableSubMatrixHandler&
PressureLoad<ElementType, CollocationType, PressureSource>::AssJac(VariableSubMatrixHandler& WorkMat,
                                                                   doublereal dCoef,
                                                                   const VectorHandler& XCurr,
                                                                   const VectorHandler& XPrimeCurr)
{
     DEBUGCOUTFNAME("PressureLoad::AssJac");

     sp_grad::SpGradientAssVec<sp_grad::SpGradient>::AssJac(this,
                                                            WorkMat.SetSparseGradient(),
                                                            dCoef,
                                                            XCurr,
                                                            XPrimeCurr,
                                                            sp_grad::SpFunctionCall::REGULAR_JAC);
     return WorkMat;
}

template <typename ElementType, typename CollocationType, typename PressureSource>
void
PressureLoad<ElementType, CollocationType, PressureSource>::AssJac(VectorHandler& JacY,
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

template <typename ElementType, typename CollocationType, typename PressureSource>
template <typename T>
void
PressureLoad<ElementType, CollocationType, PressureSource>::InitialAssRes(sp_grad::SpGradientAssVec<T>& WorkVec,
                                                                          const sp_grad::SpGradientVectorHandler<T>& XCurr,
                                                                          enum sp_grad::SpFunctionCall func)
{
     using namespace sp_grad;

     SpColVector<T, iNumNodes * 3> R(iNumDof, (iNumDof + iNumNodes) * iNumEvalPoints);

     AssPressureLoad(R, 1., func);

     this->AssVector(WorkVec, R, &StructDispNodeAd::iGetFirstPositionIndex);
}

template <typename ElementType, typename CollocationType, typename PressureSource>
VariableSubMatrixHandler&
PressureLoad<ElementType, CollocationType, PressureSource>::InitialAssJac(VariableSubMatrixHandler& WorkMat,
                                                                          const VectorHandler& XCurr)
{
     using namespace sp_grad;

     SpGradientAssVec<SpGradient>::InitialAssJac(this,
                                                 WorkMat.SetSparseGradient(),
                                                 XCurr,
                                                 sp_grad::INITIAL_ASS_JAC);

     return WorkMat;
}

template <typename ElementType, typename CollocationType, typename PressureSource>
SubVectorHandler&
PressureLoad<ElementType, CollocationType, PressureSource>::InitialAssRes(SubVectorHandler& WorkVec,
                                                                          const VectorHandler& XCurr)
{
     using namespace sp_grad;

     SpGradientAssVec<doublereal>::InitialAssRes(this,
                                                 WorkVec,
                                                 XCurr,
                                                 sp_grad::INITIAL_ASS_RES);

     return WorkVec;
}

template <typename ElementType, typename CollocationType, typename PressureSource>
void
PressureLoad<ElementType, CollocationType, PressureSource>::InitialWorkSpaceDim(integer* piNumRows,
                                                                                integer* piNumCols) const
{
     *piNumRows = iNumDof;
     *piNumCols = 0;
}

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
SurfaceTraction<ElementType, CollocationType, PressureSource, eType>::SurfaceTraction(unsigned uLabel,
                                                                                      const std::array<const StructDispNodeAd*, iNumNodes>& rgNodesTmp,
                                                                                      PressureSource&& oPressureTmp,
                                                                                      const std::array<Mat3x3, iNumEvalPoints>& Rf,
                                                                                      flag fOut)
:BaseType(uLabel, rgNodesTmp, std::move(oPressureTmp), fOut)
{
     using namespace sp_grad;

     BaseType::InitCollocData(rgCollocData);

     SurfaceTractionHelper<eType>::InitCollocData(*this, Rf);
}

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
void SurfaceTraction<ElementType, CollocationType, PressureSource, eType>::InitCollocDataRel(const std::array<Mat3x3, iNumEvalPoints>& Rf)
{
     using namespace sp_grad;

     SpColVector<doublereal, 3 * iNumNodes> x(3 * iNumNodes, 0);

     this->GetNodalPosition(x, 1., SpFunctionCall::REGULAR_RES);

     for (index_type i = 0; i < iNumEvalPoints; ++i) {
          SpMatrix<doublereal, 3, 3> Relem(3, 3, 0);
          SpColVector<doublereal, 3> e1 = rgCollocData[i].dHf_dr * x;
          SpColVector<doublereal, 3> e2 = rgCollocData[i].dHf_ds * x;
          SpColVector<doublereal, 3> e3 = Cross(e1, e2);

          e2 = Cross(e3, e1);

          const doublereal Norm_e1 = Norm(e1);
          const doublereal Norm_e2 = Norm(e2);
          const doublereal Norm_e3 = Norm(e3);

          if (Norm_e1 == 0. || Norm_e2 == 0. || Norm_e3 == 0.) {
               silent_cerr("traction(" << this->GetLabel() << "): orientation matrix is singular ("
                           << Norm_e1 << "," << Norm_e2 << ", " << Norm_e3 << ")\n");
               throw ErrGeneric(MBDYN_EXCEPT_ARGS);
          }

          for (index_type j = 1; j <= 3; ++j) {
               Relem(j, 1) = e1(j) / Norm_e1;
               Relem(j, 2) = e2(j) / Norm_e2;
               Relem(j, 3) = e3(j) / Norm_e3;
          }

          rgCollocData[i].dA = Norm_e3;
          rgCollocData[i].Rrel = Transpose(Relem) * Rf[i];
     }
}

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
void SurfaceTraction<ElementType, CollocationType, PressureSource, eType>::InitCollocDataAbs(const std::array<Mat3x3, iNumEvalPoints>& Rf)
{
     using namespace sp_grad;

     SpColVector<doublereal, 3 * iNumNodes> x(3 * iNumNodes, 0);

     this->GetNodalPosition(x, 1., SpFunctionCall::REGULAR_RES);

     for (index_type i = 0; i < iNumEvalPoints; ++i) {
          SpColVector<doublereal, 3> e1 = rgCollocData[i].dHf_dr * x;
          SpColVector<doublereal, 3> e2 = rgCollocData[i].dHf_ds * x;
          SpColVector<doublereal, 3> e3 = Cross(e1, e2);

          const doublereal Norm_e3 = Norm(e3);

          if (Norm_e3 == 0.) {
               silent_cerr("traction(" << this->GetLabel() << "): orientation matrix is singular ("
                           << Norm_e3 << ")\n");
               throw ErrGeneric(MBDYN_EXCEPT_ARGS);
          }

          rgCollocData[i].dA = Norm_e3;
          rgCollocData[i].Rrel = Rf[i];
     }
}

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
SurfaceTraction<ElementType, CollocationType, PressureSource, eType>::~SurfaceTraction()
{
}

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
void SurfaceTraction<ElementType, CollocationType, PressureSource, eType>::WorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
     *piNumRows = iNumDof;
     *piNumCols = 0;
}

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
template <typename T>
inline void
SurfaceTraction<ElementType, CollocationType, PressureSource, eType>::AssRes(sp_grad::SpGradientAssVec<T>& WorkVec,
                                                                             doublereal dCoef,
                                                                             const sp_grad::SpGradientVectorHandler<T>& XCurr,
                                                                             const sp_grad::SpGradientVectorHandler<T>& XPrimeCurr,
                                                                             enum sp_grad::SpFunctionCall func)
{
     using namespace sp_grad;

     constexpr index_type iCapacity = iNumDof * (eType == SurfaceTractionType::RELATIVE);

     SpColVector<T, iNumNodes * 3> R(iNumDof, iCapacity);

     SurfaceTractionHelper<eType>::AssSurfaceTraction(*this, R, dCoef, func);

     ASSERT(R.iGetMaxSize() <= iCapacity);

     this->AssVector(WorkVec, R, &StructDispNodeAd::iGetFirstMomentumIndex);
}

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
template <typename T>
inline void
SurfaceTraction<ElementType, CollocationType, PressureSource, eType>::AssSurfaceTractionRel(sp_grad::SpColVector<T, iNumNodes * 3>& R,
                                                                                            doublereal dCoef,
                                                                                            enum sp_grad::SpFunctionCall func)
{
     using namespace sp_grad;

     T Norm_e1, Norm_e2, dA;
     SpColVector<doublereal, 3> fl_i(3, 0), frel_i(3, 0);
     SpMatrix<T, 3, 3> Relem_i(3, 3, iNumDof);
     SpColVector<T, 3 * iNumNodes> x(3 * iNumNodes, 1);
     SpMatrix<doublereal, 3, iNumNodes> fl_n(3, iNumNodes, 0);
     SpColVector<T, 3> e1(3, iNumDof), e2(3, iNumDof), e3(3, iNumDof), Fg_i(3, iNumDof + iNumNodes);

     this->GetNodalPosition(x, dCoef, func);
     this->oPressure.GetNodalLoad(fl_n, dCoef, func);

     sp_grad::SpGradExpDofMapHelper<T> oDofMap;

     oDofMap.GetDofStat(x);
     oDofMap.Reset();
     oDofMap.InsertDof(x);
     oDofMap.InsertDone();

     for (auto& g: R) {
          oDofMap.InitDofMap(g);
     }

     for (index_type i = 0; i < iNumEvalPoints; ++i) {
          const doublereal alpha = CollocationType::dGetWeight(i);

          for (index_type j = 1; j <= 3; ++j) {
               fl_i(j) = Dot(rgCollocData[i].HA, Transpose(fl_n.GetRow(j))) * alpha;
          }

          frel_i = rgCollocData[i].Rrel * fl_i;

          e1.MapAssign(rgCollocData[i].dHf_dr * x, oDofMap);
          e2.MapAssign(rgCollocData[i].dHf_ds * x, oDofMap);

          e3.MapAssign(Cross(e1, e2), oDofMap);
          e2.MapAssign(Cross(e3, e1), oDofMap);

          oDofMap.MapAssign(Norm_e1, Norm(e1));
          oDofMap.MapAssign(Norm_e2, Norm(e2));
          oDofMap.MapAssign(dA, Norm(e3));

          if (Norm_e1 == 0. || Norm_e2 == 0. || dA == 0.) {
               silent_cerr("traction(" << this->GetLabel() << "): orientation matrix is singular\n");
               throw NonlinearSolver::ErrSimulationDiverged(MBDYN_EXCEPT_ARGS);
          }

          for (index_type k = 1; k <= 3; ++k) {
               oDofMap.MapAssign(Relem_i(k, 1), e1(k) * dA / Norm_e1);
               oDofMap.MapAssign(Relem_i(k, 2), e2(k) * dA / Norm_e2);
               oDofMap.MapAssign(Relem_i(k, 3), e3(k));
          }

          Fg_i.MapAssign(Relem_i * frel_i, oDofMap);

          for (index_type k = 1; k <= iNumDof; ++k) {
               for (index_type j = 1; j <= 3; ++j) {
                    oDofMap.Add(R(k), rgCollocData[i].Hf(j, k) * Fg_i(j));
               }
          }
     }
}

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
template <typename T>
inline void
SurfaceTraction<ElementType, CollocationType, PressureSource, eType>::AssSurfaceTractionAbs(sp_grad::SpColVector<T, iNumNodes * 3>& R,
                                                                                            doublereal dCoef,
                                                                                            enum sp_grad::SpFunctionCall func)
{
     using namespace sp_grad;

     SpColVector<doublereal, 3> fl_i(3, 0);
     SpMatrix<doublereal, 3, iNumNodes> fl_n(3, iNumNodes, 0);
     SpColVector<doublereal, 3> Fg_i(3, iNumDof + iNumNodes);

     this->oPressure.GetNodalLoad(fl_n, dCoef, func);

     for (index_type i = 0; i < iNumEvalPoints; ++i) {
          const doublereal alpha = CollocationType::dGetWeight(i);

          for (index_type j = 1; j <= 3; ++j) {
               fl_i(j) = Dot(rgCollocData[i].HA, Transpose(fl_n.GetRow(j))) * alpha;
          }

          Fg_i = (rgCollocData[i].Rrel * fl_i) * rgCollocData[i].dA;

          for (index_type k = 1; k <= iNumDof; ++k) {
               for (index_type j = 1; j <= 3; ++j) {
                    R(k) += rgCollocData[i].Hf(j, k) * Fg_i(j);
               }
          }
     }
}

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
SubVectorHandler&
SurfaceTraction<ElementType, CollocationType, PressureSource, eType>::AssRes(SubVectorHandler& WorkVec,
                                                                             doublereal dCoef,
                                                                             const VectorHandler& XCurr,
                                                                             const VectorHandler& XPrimeCurr)
{
     DEBUGCOUTFNAME("SurfaceTraction::AssRes");

     sp_grad::SpGradientAssVec<doublereal>::AssRes(this,
                                                   WorkVec,
                                                   dCoef,
                                                   XCurr,
                                                   XPrimeCurr,
                                                   sp_grad::SpFunctionCall::REGULAR_RES);

     return WorkVec;
}

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
VariableSubMatrixHandler&
SurfaceTraction<ElementType, CollocationType, PressureSource, eType>::AssJac(VariableSubMatrixHandler& WorkMat,
                                                                             doublereal dCoef,
                                                                             const VectorHandler& XCurr,
                                                                             const VectorHandler& XPrimeCurr)
{
     DEBUGCOUTFNAME("SurfaceTraction::AssJac");

     sp_grad::SpGradientAssVec<sp_grad::SpGradient>::AssJac(this,
                                                            WorkMat.SetSparseGradient(),
                                                            dCoef,
                                                            XCurr,
                                                            XPrimeCurr,
                                                            sp_grad::SpFunctionCall::REGULAR_JAC);
     return WorkMat;
}

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
void
SurfaceTraction<ElementType, CollocationType, PressureSource, eType>::AssJac(VectorHandler& JacY,
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

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
template <typename T>
void
SurfaceTraction<ElementType, CollocationType, PressureSource, eType>::InitialAssRes(sp_grad::SpGradientAssVec<T>& WorkVec,
                                                                                    const sp_grad::SpGradientVectorHandler<T>& XCurr,
                                                                                    enum sp_grad::SpFunctionCall func)
{
     using namespace sp_grad;

     SpColVector<T, iNumNodes * 3> R(iNumDof, (iNumDof + iNumNodes) * iNumEvalPoints);

     SurfaceTractionHelper<eType>::AssSurfaceTraction(*this, R, 1., func);

     this->AssVector(WorkVec, R, &StructDispNodeAd::iGetFirstPositionIndex);
}

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
VariableSubMatrixHandler&
SurfaceTraction<ElementType, CollocationType, PressureSource, eType>::InitialAssJac(VariableSubMatrixHandler& WorkMat,
                                                                                    const VectorHandler& XCurr)
{
     using namespace sp_grad;

     SpGradientAssVec<SpGradient>::InitialAssJac(this,
                                                 WorkMat.SetSparseGradient(),
                                                 XCurr,
                                                 sp_grad::INITIAL_ASS_JAC);

     return WorkMat;
}

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
SubVectorHandler&
SurfaceTraction<ElementType, CollocationType, PressureSource, eType>::InitialAssRes(SubVectorHandler& WorkVec,
                                                                                    const VectorHandler& XCurr)
{
     using namespace sp_grad;

     SpGradientAssVec<doublereal>::InitialAssRes(this,
                                                 WorkVec,
                                                 XCurr,
                                                 sp_grad::INITIAL_ASS_RES);

     return WorkVec;
}

template <typename ElementType, typename CollocationType, typename PressureSource, SurfaceTractionType eType>
void
SurfaceTraction<ElementType, CollocationType, PressureSource, eType>::InitialWorkSpaceDim(integer* piNumRows,
                                                                                          integer* piNumCols) const
{
     *piNumRows = iNumDof;
     *piNumCols = 0;
}

struct UnilateralContactParam {
     UnilateralContactParam(doublereal epsilon, doublereal dContactScale, doublereal gref, doublereal pref)
          :epsilon(epsilon),
           dContactScale(dContactScale),
           gref(gref),
           pref(pref) {
     }
     const doublereal epsilon;
     const doublereal dContactScale;
     doublereal gref;
     const doublereal pref;
};

struct ContactTraitsEquality {
     static constexpr DofOrder::Equality eEqualityType = DofOrder::EQUALITY;
};

struct ContactTraitsInequality {
     static constexpr DofOrder::Equality eEqualityType = DofOrder::INEQUALITY;
};

template <typename ElementType, typename CollocationType, typename ContactTraits>
class UnilateralInPlaneContact: public SurfaceLoad<ElementType, CollocationType, PressureFromNodes<ElementType::iNumNodes>>, private UnilateralContactParam  {
     typedef SurfaceLoad<ElementType, CollocationType, PressureFromNodes<ElementType::iNumNodes>> BaseType;
public:
     using BaseType::AssRes;
     using BaseType::AssJac;
     using BaseType::iNumNodes;
     using BaseType::iNumEvalPoints;
     using BaseType::iNumDof;

     UnilateralInPlaneContact(unsigned uLabel,
                              const std::array<const StructDispNodeAd*, iNumNodes>& rgNodes,
                              const StructNodeAd* pNode0,
                              const Mat3x3& Rn0,
                              const Vec3& o0,
                              const UnilateralContactParam& oContactParam,
                              PressureFromNodes<iNumNodes>&& oPressureTmp,
                              flag fOut);
     virtual ~UnilateralInPlaneContact();

     virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const override;

     template <typename T>
     inline void
     AssRes(sp_grad::SpGradientAssVec<T>& WorkVec,
            doublereal dCoef,
            const sp_grad::SpGradientVectorHandler<T>& XCurr,
            const sp_grad::SpGradientVectorHandler<T>& XPrimeCurr,
            enum sp_grad::SpFunctionCall func);

     virtual SubVectorHandler&
     AssRes(SubVectorHandler& WorkVec,
            doublereal dCoef,
            const VectorHandler& XCurr,
            const VectorHandler& XPrimeCurr) override;

     virtual VariableSubMatrixHandler&
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

     virtual void
     AfterConvergence(const VectorHandler& X,
                      const VectorHandler& XP) override;

     virtual VariableSubMatrixHandler&
     InitialAssJac(VariableSubMatrixHandler& WorkMat,
                   const VectorHandler& XCurr) override;

     virtual SubVectorHandler&
     InitialAssRes(SubVectorHandler& WorkVec,
                   const VectorHandler& XCurr) override;

     virtual void
     InitialWorkSpaceDim(integer* piNumRows,
                         integer* piNumCols) const override;

     virtual void Output(OutputHandler& OH) const override;

protected:
     template <typename T>
     void DistanceNormalToPlane(sp_grad::index_type iNode, const sp_grad::SpColVector<T, 3>& X0, const sp_grad::SpMatrix<T, 3, 3>& R0, const sp_grad::SpColVector<T, 3 * iNumNodes>& x, T& g, const sp_grad::SpGradExpDofMapHelper<T>& oDofMap) const;

     template <typename T>
     inline void
     AssContactLoad(sp_grad::SpGradientAssVec<T>& WorkVec, doublereal dCoef, sp_grad::SpFunctionCall func);

     inline void
     UpdateTotalForceRigidBody(const sp_grad::SpColVector<doublereal, 3>& F0, const sp_grad::SpColVector<doublereal, 3>& M0) {
          F0tot = F0;
          M0tot = M0;
     }

     inline void
     UpdateTotalForceRigidBody(const sp_grad::SpColVector<sp_grad::SpGradient, 3>& F0, const sp_grad::SpColVector<sp_grad::SpGradient, 3>& M0) {}

     inline void
     UpdateTotalForceRigidBody(const sp_grad::SpColVector<sp_grad::GpGradProd, 3>& F0, const sp_grad::SpColVector<sp_grad::GpGradProd, 3>& M0) {}

     std::array<typename BaseType::CollocData, iNumEvalPoints> rgCollocData;
     const StructNodeAd* const pNode0;
     const Mat3x3 Rn0;
     const Vec3 o0;
     sp_grad::SpColVector<doublereal, 3> F0tot, M0tot;
};

template <typename ElementType, typename CollocationType, typename ContactTraits>
UnilateralInPlaneContact<ElementType, CollocationType, ContactTraits>::UnilateralInPlaneContact(unsigned uLabel,
                                                                                                const std::array<const StructDispNodeAd*, iNumNodes>& rgNodesTmp,
                                                                                                const StructNodeAd* pNode0,
                                                                                                const Mat3x3& Rn0,
                                                                                                const Vec3& o0,
                                                                                                const UnilateralContactParam& oContactParam,
                                                                                                PressureFromNodes<iNumNodes>&& oPressureTmp,
                                                                                                flag fOut)
:BaseType(uLabel, rgNodesTmp, std::move(oPressureTmp), fOut),
 UnilateralContactParam(oContactParam),
 pNode0(pNode0),
 Rn0(Rn0),
 o0(o0),
 F0tot(3, 0),
 M0tot(3, 0)
{
     using namespace sp_grad;

     BaseType::InitCollocData(rgCollocData);
}

template <typename ElementType, typename CollocationType, typename ContactTraits>
UnilateralInPlaneContact<ElementType, CollocationType, ContactTraits>::~UnilateralInPlaneContact()
{
}

template <typename ElementType, typename CollocationType, typename ContactTraits>
void UnilateralInPlaneContact<ElementType, CollocationType, ContactTraits>::WorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
     *piNumRows = iNumNodes * (3 + 1) + 6;
     *piNumCols = 0;
}

template <typename ElementType, typename CollocationType, typename ContactTraits>
template <typename T>
inline void
UnilateralInPlaneContact<ElementType, CollocationType, ContactTraits>::AssRes(sp_grad::SpGradientAssVec<T>& WorkVec,
                                                                              doublereal dCoef,
                                                                              const sp_grad::SpGradientVectorHandler<T>& XCurr,
                                                                              const sp_grad::SpGradientVectorHandler<T>& XPrimeCurr,
                                                                              enum sp_grad::SpFunctionCall func)
{
     AssContactLoad(WorkVec, dCoef, func);
}

template <typename ElementType, typename CollocationType, typename ContactTraits>
template <typename T>
void UnilateralInPlaneContact<ElementType, CollocationType, ContactTraits>::DistanceNormalToPlane(const sp_grad::index_type iNode, const sp_grad::SpColVector<T, 3>& X0, const sp_grad::SpMatrix<T, 3, 3>& R0, const sp_grad::SpColVector<T, 3 * iNumNodes>& x, T& gi, const sp_grad::SpGradExpDofMapHelper<T>& oDofMap) const
{
     ASSERT(iNode >= 1);
     ASSERT(iNode <= iNumNodes);

     using namespace sp_grad;

     gi = Dot(Rn0.GetCol(3), Transpose(R0) * (SubMatrix<3, 1>(x, 3 * (iNode - 1) + 1, 1, 1, 1) - X0) - o0, oDofMap);
}

template <typename ElementType, typename CollocationType, typename ContactTraits>
template <typename T>
inline void
UnilateralInPlaneContact<ElementType, CollocationType, ContactTraits>::AssContactLoad(sp_grad::SpGradientAssVec<T>& WorkVec,
                                                                                      doublereal dCoef,
                                                                                      enum sp_grad::SpFunctionCall func)
{
     using namespace sp_grad;
     using std::pow;

     ASSERT(func & sp_grad::REGULAR_FLAG);

     SpColVector<T, 3 * iNumNodes> x(3 * iNumNodes, 1);
     SpColVector<T, iNumNodes> p(iNumNodes, 1);
     SpColVector<T, 3> n1(3, iNumDof), n2(3, iNumDof), dF(3, iNumDof + iNumNodes);
     SpColVector<T, 3> X0(3, 1);
     SpMatrix<T, 3, 3> R0(3, 3, 3);

     this->GetNodalPosition(x, dCoef, func);
     this->oPressure.GetNodalPressure(p, dCoef, func);

     pNode0->GetXCurr(X0, dCoef, func);
     pNode0->GetRCurr(R0, dCoef, func);

     SpGradExpDofMapHelper<T> oDofMap;

     oDofMap.GetDofStat(x);
     oDofMap.GetDofStat(p);
     oDofMap.GetDofStat(X0);
     oDofMap.GetDofStat(R0);
     oDofMap.Reset();
     oDofMap.InsertDof(x);
     oDofMap.InsertDof(p);
     oDofMap.InsertDof(X0);
     oDofMap.InsertDof(R0);
     oDofMap.InsertDone();

     SpColVector<T, iNumNodes * 3> Rp(iNumNodes * 3, oDofMap);
     SpColVector<T, iNumNodes> Rc(iNumNodes, oDofMap);
     SpColVector<T, 3> F0(3, oDofMap);
     SpColVector<T, 3> M0(3, oDofMap);
     SpColVector<T, 3> xi(3, iNumNodes * 3);
     SpColVector<T, iNumNodes> g(iNumNodes, 0);

     const SpColVector<T, 3> n0(R0 * Rn0.GetCol(3), oDofMap);

     for (index_type i = 1; i <= iNumNodes; ++i) {
          DistanceNormalToPlane(i, X0, R0, x, g(i), oDofMap);
     }

     for (index_type i = 0; i < iNumEvalPoints; ++i) {
          const doublereal alpha = CollocationType::dGetWeight(i);

          const T p_i = Dot(rgCollocData[i].HA, p);

          n1.MapAssign(rgCollocData[i].dHf_dr * x, oDofMap);
          n2.MapAssign(rgCollocData[i].dHf_ds * x, oDofMap);
          xi.MapAssign(rgCollocData[i].Hf * x, oDofMap);
          const SpColVector<T, 3> n3 = Cross(n1, n2, oDofMap);
          const T detJA = Norm(n3, oDofMap);

          if constexpr(ContactTraits::eEqualityType == DofOrder::EQUALITY) {
               const T g_i = Dot(rgCollocData[i].HA, g);
               const T w_i = oDofMap.MapEval((0.5 * (g_i / gref + p_i / pref) - sqrt(pow(0.5 * (g_i / gref - p_i / pref), 2) + epsilon)) * detJA);

               for (index_type k = 1; k <= iNumNodes; ++k) {
                    oDofMap.Add(Rc(k), rgCollocData[i].HA(k) * alpha * dContactScale / dCoef * w_i);
               }
          }

          dF.MapAssign(n0 * (alpha * p_i * detJA), oDofMap);

          for (index_type k = 1; k <= iNumDof; ++k) {
               for (index_type j = 1; j <= 3; ++j) {
                    oDofMap.Add(Rp(k), rgCollocData[i].Hf(j, k) * dF(j));
               }
          }

          F0.Sub(dF, oDofMap);
          M0.Sub(Cross(xi - X0, dF, oDofMap), oDofMap);
     }

     this->AssVector(WorkVec, Rp, &StructDispNodeAd::iGetFirstMomentumIndex);

     if constexpr(ContactTraits::eEqualityType != DofOrder::EQUALITY) {
           Rc.MapAssign(g * (dContactScale * this->A0 / (gref * dCoef)), oDofMap);
     }

     for (index_type i = 1; i <= iNumNodes; ++i) {
          WorkVec.AddItem(this->oPressure.pGetNode(i - 1)->iGetFirstIndex() + 1, Rc(i));
     }

     WorkVec.AddItem(pNode0->iGetFirstMomentumIndex() + 1, F0);
     WorkVec.AddItem(pNode0->iGetFirstMomentumIndex() + 4, M0);

     UpdateTotalForceRigidBody(F0, M0);
}

template <typename ElementType, typename CollocationType, typename ContactTraits>
SubVectorHandler&
UnilateralInPlaneContact<ElementType, CollocationType, ContactTraits>::AssRes(SubVectorHandler& WorkVec,
                                                                              doublereal dCoef,
                                                                              const VectorHandler& XCurr,
                                                                              const VectorHandler& XPrimeCurr)
{
     DEBUGCOUTFNAME("PressureLoad::AssRes");

     sp_grad::SpGradientAssVec<doublereal>::AssRes(this,
                                                   WorkVec,
                                                   dCoef,
                                                   XCurr,
                                                   XPrimeCurr,
                                                   sp_grad::SpFunctionCall::REGULAR_RES);

     return WorkVec;
}

template <typename ElementType, typename CollocationType, typename ContactTraits>
VariableSubMatrixHandler&
UnilateralInPlaneContact<ElementType, CollocationType, ContactTraits>::AssJac(VariableSubMatrixHandler& WorkMat,
                                                                              doublereal dCoef,
                                                                              const VectorHandler& XCurr,
                                                                              const VectorHandler& XPrimeCurr)
{
     DEBUGCOUTFNAME("UnilateralInPlaneContact::AssJac");

     sp_grad::SpGradientAssVec<sp_grad::SpGradient>::AssJac(this,
                                                            WorkMat.SetSparseGradient(),
                                                            dCoef,
                                                            XCurr,
                                                            XPrimeCurr,
                                                            sp_grad::SpFunctionCall::REGULAR_JAC);
     return WorkMat;
}

template <typename ElementType, typename CollocationType, typename ContactTraits>
void
UnilateralInPlaneContact<ElementType, CollocationType, ContactTraits>::AssJac(VectorHandler& JacY,
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

template <typename ElementType, typename CollocationType, typename ContactTraits>
void
UnilateralInPlaneContact<ElementType, CollocationType, ContactTraits>::AfterConvergence(const VectorHandler& X,
                                                                                        const VectorHandler& XP)
{
}

template <typename ElementType, typename CollocationType, typename ContactTraits>
VariableSubMatrixHandler&
UnilateralInPlaneContact<ElementType, CollocationType, ContactTraits>::InitialAssJac(VariableSubMatrixHandler& WorkMat,
                                                                                     const VectorHandler& XCurr)
{
     WorkMat.SetNullMatrix();

     return WorkMat;
}

template <typename ElementType, typename CollocationType, typename ContactTraits>
SubVectorHandler&
UnilateralInPlaneContact<ElementType, CollocationType, ContactTraits>::InitialAssRes(SubVectorHandler& WorkVec,
                                                                                     const VectorHandler& XCurr)
{
     WorkVec.ResizeReset(0);

     return WorkVec;
}

template <typename ElementType, typename CollocationType, typename ContactTraits>
void
UnilateralInPlaneContact<ElementType, CollocationType, ContactTraits>::InitialWorkSpaceDim(integer* piNumRows,
                                                                                           integer* piNumCols) const
{
     *piNumRows = 0;
     *piNumCols = 0;
}

template <typename ElementType, typename CollocationType, typename ContactTraits>
void UnilateralInPlaneContact<ElementType, CollocationType, ContactTraits>::Output(OutputHandler& OH) const
{
     using namespace sp_grad;

     if (this->bToBeOutput() && OH.UseText(OutputHandler::SURFACE_LOADS)) {
          if (OH.UseText(OutputHandler::SURFACE_LOADS)) {
               std::ostream& of = OH.SurfaceLoads();

               of << std::setw(8) << this->GetLabel() << ' ' << this->Ftot << ' ' << F0tot << ' ' << M0tot << '\n';
          }
     }
}

template <typename ElementType, typename CollocationType>
SurfaceLoadElem*
ReadPressureLoad(DataManager* const pDM, MBDynParser& HP, const unsigned int uLabel)
{
     DEBUGCOUTFNAME("ReadPressureLoad");

     using namespace sp_grad;

     constexpr index_type iNumNodes = ElementType::iNumNodes;

     typedef PressureLoad<ElementType, CollocationType, PressureFromNodes<iNumNodes>> PressureLoadFromNodes;
     typedef PressureLoad<ElementType, CollocationType, PressureFromDrives<iNumNodes>> PressureLoadFromDrives;

     std::array<const StructDispNodeAd*, iNumNodes> rgNodes;

     enum { PRESSURE_FROM_NODES,
            PRESSURE_FROM_DRIVES,
            PRESSURE_UNKNOWN
     } ePressureSource = PRESSURE_UNKNOWN;

     PressureFromNodes<iNumNodes> oPressureFromNodes;
     PressureFromDrives<iNumNodes> oPressureFromDrives;

     for (index_type i = 0; i < iNumNodes; ++i) {
          rgNodes[i] = pDM->ReadNode<const StructDispNodeAd, Node::STRUCTURAL>(HP);
     }

     if (HP.IsKeyWord("from" "nodes")) {
          ePressureSource = PRESSURE_FROM_NODES;

          for (index_type i = 0; i < iNumNodes; ++i) {
               oPressureFromNodes.SetNode(i, pDM->ReadNode<const ScalarNodeAd, Node::ABSTRACT>(HP));
          }
     } else if (HP.IsKeyWord("from" "drives")) {
          ePressureSource = PRESSURE_FROM_DRIVES;

          std::unique_ptr<DriveCaller> pDrive;

          for (index_type i = 0; i < iNumNodes; ++i) {
               pDrive.reset(HP.GetDriveCaller());

               oPressureFromDrives.SetDrive(i, std::move(pDrive));
          }
     } else {
          silent_cerr("keyword \"from nodes\" or \"from drives\" expected at line " << HP.GetLineData() << "\n");
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     const flag fOut = pDM->fReadOutput(HP, Elem::SURFACE_LOAD);

     if (HP.IsArg()) {
          silent_cerr("semicolon expected "
                      "at line " << HP.GetLineData() << std::endl);
          throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     std::ostream& out = pDM->GetLogFile();

     out << ElementType::ElementName() << ": " << uLabel;

     for (sp_grad::index_type i = 0; i < iNumNodes; ++i) {
          out << ' ' << rgNodes[i]->GetLabel();
     }

     SurfaceLoadElem* pElem = nullptr;

     switch (ePressureSource) {
     case PRESSURE_FROM_NODES:
          oPressureFromNodes.PrintLogFile(out);

          SAFENEWWITHCONSTRUCTOR(pElem,
                                 PressureLoadFromNodes,
                                 PressureLoadFromNodes(uLabel, rgNodes, std::move(oPressureFromNodes), fOut));
          break;
     case PRESSURE_FROM_DRIVES:
          oPressureFromDrives.PrintLogFile(out);

          SAFENEWWITHCONSTRUCTOR(pElem,
                                 PressureLoadFromDrives,
                                 PressureLoadFromDrives(uLabel, rgNodes, std::move(oPressureFromDrives), fOut));
          break;
     default:
          ASSERT(0);
     }

     out << '\n';

     return pElem;
}

template SurfaceLoadElem* ReadPressureLoad<Quadrangle4, Gauss2x2>(DataManager*, MBDynParser&, unsigned int);
template SurfaceLoadElem* ReadPressureLoad<Quadrangle8, Gauss3x3>(DataManager*, MBDynParser&, unsigned int);
template SurfaceLoadElem* ReadPressureLoad<Quadrangle9, Gauss3x3>(DataManager*, MBDynParser&, unsigned int);
template SurfaceLoadElem* ReadPressureLoad<Quadrangle8r, Gauss3x3>(DataManager*, MBDynParser&, unsigned int);
template SurfaceLoadElem* ReadPressureLoad<Triangle6h, CollocTria6h>(DataManager*, MBDynParser&, unsigned int);

template <typename ElementType, typename CollocationType>
SurfaceLoadElem*
ReadTractionLoad(DataManager* const pDM, MBDynParser& HP, const unsigned int uLabel)
{
     DEBUGCOUTFNAME("ReadTractionLoad");

     using namespace sp_grad;

     constexpr index_type iNumNodes = ElementType::iNumNodes;
     constexpr index_type iNumEvalPoints = CollocationType::iNumEvalPoints;

     typedef SurfaceTraction<ElementType, CollocationType, SurfLoadFromDrives<iNumNodes>, SurfaceTractionType::RELATIVE> SurfaceTractionRel;
     typedef SurfaceTraction<ElementType, CollocationType, SurfLoadFromDrives<iNumNodes>, SurfaceTractionType::ABSOLUTE> SurfaceTractionAbs;

     const SurfaceTractionType eTractionType = HP.IsKeyWord("absolute") ? SurfaceTractionType::ABSOLUTE : SurfaceTractionType::RELATIVE;

     std::array<const StructDispNodeAd*, iNumNodes> rgNodes;

     SurfLoadFromDrives<iNumNodes> oSurfLoadFromDrives;

     for (index_type i = 0; i < iNumNodes; ++i) {
          rgNodes[i] = pDM->ReadNode<const StructDispNodeAd, Node::STRUCTURAL>(HP);
     }

     if (HP.IsKeyWord("from" "drives")) {
          std::unique_ptr<TplDriveCaller<Vec3>> pDrive;

          for (index_type i = 0; i < iNumNodes; ++i) {
               pDrive.reset(ReadDC3D(pDM, HP));

               oSurfLoadFromDrives.SetDrive(i, std::move(pDrive));
          }
     } else {
          silent_cerr("keyword \"from drives\" expected at line " << HP.GetLineData() << "\n");
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     ReferenceFrame oGlobalRefFrame;
     std::array<Mat3x3, iNumEvalPoints> Rf;
     const bool bReadOrientation = HP.IsKeyWord("orientation");

     for (Mat3x3& Rf_i: Rf) {
          Rf_i = bReadOrientation ? HP.GetRotAbs(oGlobalRefFrame) : Eye3;
     }

     const flag fOut = pDM->fReadOutput(HP, Elem::SURFACE_LOAD);

     if (HP.IsArg()) {
          silent_cerr("semicolon expected "
                      "at line " << HP.GetLineData() << std::endl);
          throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     std::ostream& out = pDM->GetLogFile();

     out << ElementType::ElementName() << ": " << uLabel;

     for (sp_grad::index_type i = 0; i < iNumNodes; ++i) {
          out << ' ' << rgNodes[i]->GetLabel();
     }

     oSurfLoadFromDrives.PrintLogFile(out);

     out << '\n';

     SurfaceLoadElem* pElem = nullptr;

     switch (eTractionType) {
     case SurfaceTractionType::RELATIVE:
          SAFENEWWITHCONSTRUCTOR(pElem,
                                 SurfaceTractionRel,
                                 SurfaceTractionRel(uLabel, rgNodes, std::move(oSurfLoadFromDrives), Rf, fOut));
          break;
     case SurfaceTractionType::ABSOLUTE:
          SAFENEWWITHCONSTRUCTOR(pElem,
                                 SurfaceTractionAbs,
                                 SurfaceTractionAbs(uLabel, rgNodes, std::move(oSurfLoadFromDrives), Rf, fOut));
          break;
     }

     return pElem;
}

template SurfaceLoadElem* ReadTractionLoad<Quadrangle4, Gauss2x2>(DataManager*, MBDynParser&, unsigned int);
template SurfaceLoadElem* ReadTractionLoad<Quadrangle8, Gauss3x3>(DataManager*, MBDynParser&, unsigned int);
template SurfaceLoadElem* ReadTractionLoad<Quadrangle9, Gauss3x3>(DataManager*, MBDynParser&, unsigned int);
template SurfaceLoadElem* ReadTractionLoad<Quadrangle8r, Gauss3x3>(DataManager*, MBDynParser&, unsigned int);
template SurfaceLoadElem* ReadTractionLoad<Triangle6h, CollocTria6h>(DataManager*, MBDynParser&, unsigned int);

template <typename ElementType, typename CollocationType>
SurfaceLoadElem*
ReadUnilateralInPlaneContact(DataManager* const pDM, MBDynParser& HP, const unsigned int uLabel)
{
     DEBUGCOUTFNAME("ReadUnilateralInPlaneContact");

     using namespace sp_grad;

     constexpr index_type iNumNodes = ElementType::iNumNodes;

     typedef UnilateralInPlaneContact<ElementType, CollocationType, ContactTraitsEquality> UnilateralInPlaneElemEquality;
     typedef UnilateralInPlaneContact<ElementType, CollocationType, ContactTraitsInequality> UnilateralInPlaneElemInequality;

     std::array<const StructDispNodeAd*, iNumNodes> rgNodes;

     PressureFromNodes<iNumNodes> oPressureFromNodes;

     for (index_type i = 0; i < iNumNodes; ++i) {
          rgNodes[i] = pDM->ReadNode<const StructDispNodeAd, Node::STRUCTURAL>(HP);
     }

     DofOrder::Equality eEqualityType = DofOrder::EQUALITY;

     for (index_type i = 0; i < iNumNodes; ++i) {
          oPressureFromNodes.SetNode(i, pDM->ReadNode<const ScalarAlgebraicNodeAd, Node::ABSTRACT>(HP));

          const DofOrder::Equality eEqualityTypeNode = oPressureFromNodes.pGetNode(i)->GetEqualityType(0);

          if (i == 0) {
               eEqualityType = eEqualityTypeNode;
          } else if (eEqualityType != eEqualityTypeNode) {
               silent_cerr("unilateral in plane contact(" << uLabel << "): all the nodes must be either equality or inequality nodes at line " << HP.GetLineData() << "\n");
               throw ErrGeneric(MBDYN_EXCEPT_ARGS);
          }
     }

     const StructNodeAd* const pNode0 = pDM->ReadNode<const StructNodeAd, Node::STRUCTURAL>(HP);

     const Vec3 o0 = HP.IsKeyWord("offset") ? HP.GetPosRel(ReferenceFrame(pNode0)) : Zero3;

     const Mat3x3 Rn0 = HP.IsKeyWord("orientation") ? HP.GetRotRel(ReferenceFrame(pNode0)) : Eye3;

     const doublereal epsilon = HP.IsKeyWord("epsilon") ? HP.GetReal() : 1e-8;

     if (epsilon <= 0.) {
          silent_cerr("unilateral in plane contact(" << uLabel << "): epsilon must be greater than zero at line " << HP.GetLineData() << "\n");
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     const doublereal dContactScale = HP.IsKeyWord("scale") ? HP.GetReal() : 1.;

     if (dContactScale <= 0.) {
          silent_cerr("unilateral in plane contact(" << uLabel << "): scale must be greater than zero at line " << HP.GetLineData() << "\n");
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     const doublereal gref = HP.IsKeyWord("reference" "gap") ? HP.GetReal() : 1.;

     const doublereal pref = HP.IsKeyWord("reference" "pressure") ? HP.GetReal() : 1.;

     const flag fOut = pDM->fReadOutput(HP, Elem::SURFACE_LOAD);

     if (HP.IsArg()) {
          silent_cerr("semicolon expected "
                      "at line " << HP.GetLineData() << std::endl);
          throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     std::ostream& out = pDM->GetLogFile();

     out << ElementType::ElementName() << ": " << uLabel;

     for (sp_grad::index_type i = 0; i < iNumNodes; ++i) {
          out << ' ' << rgNodes[i]->GetLabel();
     }

     oPressureFromNodes.PrintLogFile(out);

     out << '\n';

     SurfaceLoadElem* pElem = nullptr;

     UnilateralContactParam oParam(epsilon, dContactScale, gref, pref);

     switch (eEqualityType) {
     case DofOrder::EQUALITY:
          SAFENEWWITHCONSTRUCTOR(pElem,
                                 UnilateralInPlaneElemEquality,
                                 UnilateralInPlaneElemEquality(uLabel, rgNodes, pNode0, Rn0, o0, oParam, std::move(oPressureFromNodes), fOut));
          break;
     case DofOrder::INEQUALITY:
          SAFENEWWITHCONSTRUCTOR(pElem,
                                 UnilateralInPlaneElemInequality,
                                 UnilateralInPlaneElemInequality(uLabel, rgNodes, pNode0, Rn0, o0, oParam, std::move(oPressureFromNodes), fOut));
          break;
     default:
          ASSERT(0);
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
     }

     return pElem;
}

template SurfaceLoadElem* ReadUnilateralInPlaneContact<Quadrangle4, Gauss2x2>(DataManager*, MBDynParser&, unsigned int);
template SurfaceLoadElem* ReadUnilateralInPlaneContact<Quadrangle8, Gauss3x3>(DataManager*, MBDynParser&, unsigned int);
template SurfaceLoadElem* ReadUnilateralInPlaneContact<Quadrangle9, Gauss3x3>(DataManager*, MBDynParser&, unsigned int);
template SurfaceLoadElem* ReadUnilateralInPlaneContact<Quadrangle8r, Gauss3x3>(DataManager*, MBDynParser&, unsigned int);
template SurfaceLoadElem* ReadUnilateralInPlaneContact<Triangle6h, CollocTria6h>(DataManager*, MBDynParser&, unsigned int);

template SurfaceLoadElem* ReadUnilateralInPlaneContact<Quadrangle4, Gauss2x2Lumped>(DataManager*, MBDynParser&, unsigned int);
template SurfaceLoadElem* ReadUnilateralInPlaneContact<Quadrangle8, Gauss3x3Lumped>(DataManager*, MBDynParser&, unsigned int);
template SurfaceLoadElem* ReadUnilateralInPlaneContact<Quadrangle9, Gauss3x3Lumped>(DataManager*, MBDynParser&, unsigned int);
template SurfaceLoadElem* ReadUnilateralInPlaneContact<Quadrangle8r, Gauss3x3Lumped>(DataManager*, MBDynParser&, unsigned int);
