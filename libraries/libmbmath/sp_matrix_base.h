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
        Copyright (C) 2020(-2023) all rights reserved.

        The copyright of this code is transferred
        to Pierangelo Masarati and Paolo Mantegazza
        for use in the software MBDyn as described
        in the GNU Public License version 2.1
*/

#ifndef __SP_MATRIX_BASE_H__INCLUDED__
#define __SP_MATRIX_BASE_H__INCLUDED__

#include <algorithm>
#include <cstdlib>
#include <new>

#include "matvec3.h"
#include "matvec3n.h"
#include "RotCoeff.hh"
#include "Rot.hh"
#include "tpls.h"

#include "sp_matrix_base_fwd.h"
#include "sp_gradient.h"
#include "binary_conversion.h"

namespace sp_grad {
     template <typename ValueType, typename ScalarExpr, index_type NumRows = 1, index_type NumCols = 1>
     class SpMatElemScalarExpr;

     template <typename ValueType, typename Expr>
     class SpMatElemUniqueExpr;

     namespace util {
          template <typename UValue, typename UExpr, typename VValue, typename VExpr>
          struct MatrixSizeHelper {
               static index_type iGetNumRows(const SpMatElemExprBase<UValue, UExpr>& u,
                                             const SpMatElemExprBase<VValue, VExpr>& v) noexcept {
                    SP_GRAD_ASSERT(u.iGetNumRows() == v.iGetNumRows());
                    return u.iGetNumRows();
               }

               static index_type iGetNumCols(const SpMatElemExprBase<UValue, UExpr>& u,
                                             const SpMatElemExprBase<VValue, VExpr>& v) noexcept {
                    SP_GRAD_ASSERT(u.iGetNumCols() == v.iGetNumCols());
                    return u.iGetNumCols();
               }
          };

          template <typename UValue, typename UExpr, typename VValue, typename VExpr>
          struct MatrixSizeHelper<UValue, SpMatElemScalarExpr<UValue, UExpr>, VValue, VExpr> {
               static index_type iGetNumRows(const SpMatElemExprBase<UValue, SpMatElemScalarExpr<UValue, UExpr> >& u,
                                             const SpMatElemExprBase<VValue, VExpr>& v) noexcept {
                    SP_GRAD_ASSERT(u.iGetNumRows() == 1);
                    return v.iGetNumRows();
               }

               static index_type iGetNumCols(const SpMatElemExprBase<UValue, SpMatElemScalarExpr<UValue, UExpr> >& u,
                                             const SpMatElemExprBase<VValue, VExpr>& v) noexcept {
                    SP_GRAD_ASSERT(u.iGetNumCols() == 1);
                    return v.iGetNumCols();
               }
          };

          template <typename UValue, typename UExpr, typename VValue, typename VExpr>
          struct MatrixSizeHelper<UValue, UExpr, VValue, SpMatElemScalarExpr<VValue, VExpr> > {
               static index_type iGetNumRows(const SpMatElemExprBase<UValue, UExpr>& u,
                                             const SpMatElemExprBase<VValue, SpMatElemScalarExpr<VValue, VExpr> >& v) {
                    SP_GRAD_ASSERT(v.iGetNumRows() == 1);
                    return u.iGetNumRows();
               }

               static index_type iGetNumCols(const SpMatElemExprBase<UValue, UExpr>& u,
                                             const SpMatElemExprBase<VValue, SpMatElemScalarExpr<VValue, VExpr> >& v) {
                    SP_GRAD_ASSERT(v.iGetNumCols() == 1);
                    return u.iGetNumCols();
               }
          };

          template <typename ValueType, SpGradCommon::ExprEvalFlags eCompr>
          struct ComprEvalHelper;

          template <SpGradCommon::ExprEvalFlags eCompr>
          struct ComprEvalHelper<doublereal, eCompr> {
               // It makes no sense to allocate a class SpGradExprDofMap for a doublereal
               static const SpGradCommon::ExprEvalFlags eExprEvalFlags = SpGradCommon::ExprEvalDuplicate;
          };

          template <SpGradCommon::ExprEvalFlags eCompr>
          struct ComprEvalHelper<SpGradient, eCompr> {
               static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = eCompr;
          };

          template <SpGradCommon::ExprEvalFlags eCompr>
          struct ComprEvalHelper<GpGradProd, eCompr> {
               // It makes no sense to allocate a class SpGradExprDofMap for a doublereal
               static const SpGradCommon::ExprEvalFlags eExprEvalFlags = SpGradCommon::ExprEvalDuplicate;
          };

          template <typename Expr, bool bUseTempExpr>
          struct TempExprHelper;

          template <typename Expr>
          struct TempExprHelper<Expr, true> {
               typedef typename remove_all<Expr>::type ExprType;
               typedef typename ExprType::ValueType ValueType;

               template <typename Value, typename ExprType>
               constexpr static SpMatElemUniqueExpr<Value, const SpMatElemExprBase<Value, ExprType>&>
               EvalUnique(const SpMatElemExprBase<Value, ExprType>& oExpr) {
                    static_assert(std::is_same<decltype(oExpr), Expr>::value, "data type does not match");
                    return decltype(EvalUnique(oExpr)){oExpr};
               }

               // Don't use const here in order to enable move constructors
               typedef SpMatrixBase<ValueType, ExprType::iNumRowsStatic, ExprType::iNumColsStatic> Type;
          };

          template <typename Expr>
          struct TempExprHelper<Expr, false> {
               template <typename Value, typename ExprType>
               constexpr static const SpMatElemExprBase<Value, ExprType>&
               EvalUnique(const SpMatElemExprBase<Value, ExprType>& oExpr) {
                    static_assert(std::is_same<decltype(oExpr), Expr>::value, "data type does not match");
                    return oExpr;
               }

               class Type: public SpMatElemExprBase<typename util::remove_all<Expr>::type::ValueType, TempExprHelper<Expr, false>::Type> {
               private:
                    typedef typename util::remove_all<Expr>::type ExprType;

               public:
                    typedef typename ExprType::ValueType ValueType;
                    typedef typename ExprType::DerivedType DerivedType;
                    static constexpr index_type iNumElemOps = ExprType::iNumElemOps;
                    static constexpr unsigned uMatAccess = ExprType::uMatAccess;
                    static constexpr index_type iNumRowsStatic = ExprType::iNumRowsStatic;
                    static constexpr index_type iNumColsStatic = ExprType::iNumColsStatic;
                    static constexpr SpMatOpType eMatOpType = ExprType::eMatOpType;
                    static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = ExprType::eExprEvalFlags;

                    Type(const Expr& oExpr)
                         :oExpr(oExpr) {
                    }

                    Type(const Expr& oExpr, const SpGradExpDofMapHelper<ValueType>&)
                         :oExpr(oExpr) {
                    }

                    operator const Expr&() const {
                         return oExpr;
                    }

                    template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                              SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                              typename ValueTypeA,
                              index_type NumRowsA, index_type NumColsA>
                    inline void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
                         oExpr.template Eval<eTransp, eCompr>(A);
                    }

                    template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                              SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                              typename Func,
                              typename ValueTypeA,
                              index_type NumRowsA, index_type NumColsA>
                    inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
                         oExpr.template AssignEval<eTransp, eCompr, Func>(A);
                    }

                    template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                              SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                              typename ValueTypeA,
                              index_type NumRowsA, index_type NumColsA>
                    inline void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A, const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
                         oExpr.template Eval<eTransp, eCompr>(A, oDofMap);
                    }

                    template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                              SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                              typename Func,
                              typename ValueTypeA,
                              index_type NumRowsA, index_type NumColsA>
                    inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A, const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
                         oExpr.template AssignEval<eTransp, eCompr, Func>(A, oDofMap);
                    }

                    index_type iGetNumRows() const {
                         return oExpr.iGetNumRows();
                    }

                    index_type iGetNumCols() const {
                         return oExpr.iGetNumCols();
                    }

                    constexpr doublereal dGetValue(index_type i, index_type j) const {
                         return oExpr.dGetValue(i, j);
                    }

#ifdef SP_GRAD_DEBUG
                    template <typename ExprType, typename ExprB>
                    constexpr bool bHaveRefTo(const SpMatElemExprBase<ExprType, ExprB>& A) const {
                         return oExpr.bHaveRefTo(A);
                    }
#endif
                    constexpr index_type iGetSize(index_type i, index_type j) const {
                         return oExpr.iGetSize(i, j);
                    }

                    index_type iGetMaxSizeElem() const {
                         return oExpr.iGetMaxSizeElem();
                    }

                    index_type iGetMaxSize() const {
                         return oExpr.iGetMaxSize();
                    }

                    template <typename ValueTypeB>
                    void InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type i, index_type j) const {
                         oExpr.InsertDeriv(g, dCoef, i, j);
                    }

                    void GetDofStat(SpGradDofStat& s, index_type i, index_type j) const {
                         oExpr.GetDofStat(s, i, j);
                    }

                    void InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
                         oExpr.InsertDof(oExpDofMap, i, j);
                    }

                    template <typename ValueTypeB>
                    void AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
                         oExpr.AddDeriv(g, dCoef, oExpDofMap, i, j);
                    }

                    const ValueType* begin() const {
                         return oExpr.begin();
                    }

                    const ValueType* end() const {
                         return oExpr.end();
                    }

                    inline constexpr index_type iGetRowOffset() const noexcept { return oExpr.iGetRowOffset(); }
                    inline constexpr index_type iGetColOffset() const noexcept { return oExpr.iGetColOffset(); }

                    constexpr const DerivedType* pGetRep() const noexcept {
                         return oExpr.pGetRep();
                    }
               private:
                    const Expr oExpr;
               };
          };

          template <index_type iSizeStatic>
          struct MatrixDataSizeHelper {
               static_assert(iSizeStatic > 0, "invalid static array size");

               static constexpr index_type iGetSizeStatic(index_type) {
                    return iSizeStatic;
               }
          };

          template <>
          struct MatrixDataSizeHelper<SpMatrixSize::DYNAMIC> {
               static_assert(SpMatrixSize::DYNAMIC < 0, "invalid constant");

               static index_type iGetSizeStatic(index_type iSize) {
                    SP_GRAD_ASSERT(iSize >= 0);
                    return iSize;
               }
          };
     }

     template <typename ValueType, typename BinaryFunc, typename LhsExpr, typename RhsExpr>
     class SpMatElemBinExpr
          :public SpMatElemExprBase<ValueType, SpMatElemBinExpr<ValueType, BinaryFunc, LhsExpr, RhsExpr> > {
          typedef typename util::remove_all<LhsExpr>::type LhsExprType;
          typedef typename util::remove_all<RhsExpr>::type RhsExprType;
          static constexpr bool bUseTempExprLhs = !(LhsExprType::uMatAccess & util::MatAccessFlag::ELEMENT_WISE);
          static constexpr bool bUseTempExprRhs = !(RhsExprType::uMatAccess & util::MatAccessFlag::ELEMENT_WISE);
          typedef typename util::TempExprHelper<LhsExpr, bUseTempExprLhs>::Type LhsTempExpr;
          typedef typename util::TempExprHelper<RhsExpr, bUseTempExprRhs>::Type RhsTempExpr;
          typedef typename util::remove_all<LhsTempExpr>::type LhsTempExprType;
          typedef typename util::remove_all<RhsTempExpr>::type RhsTempExprType;
          typedef typename LhsTempExprType::ValueType LhsValueType;
          typedef typename RhsTempExprType::ValueType RhsValueType;
          typedef typename LhsTempExprType::DerivedType LhsDerivedType;
          typedef typename RhsTempExprType::DerivedType RhsDerivedType;
          typedef util::MatrixSizeHelper<LhsValueType, LhsDerivedType, RhsValueType, RhsDerivedType> MatSizeHelp;
          static constexpr bool bMinOneScalarOp = LhsExprType::eMatOpType == SpMatOpType::SCALAR ||
               RhsExprType::eMatOpType == SpMatOpType::SCALAR;
     public:
          static constexpr index_type iNumElemOps = LhsTempExprType::iNumElemOps + RhsTempExprType::iNumElemOps + 1;
          static constexpr unsigned uMatAccess = util::MatAccessFlag::ELEMENT_WISE;
          static constexpr index_type iNumRowsStatic = LhsExprType::eMatOpType == SpMatOpType::MATRIX ? LhsExprType::iNumRowsStatic : RhsExprType::iNumRowsStatic;
          static constexpr index_type iNumColsStatic = LhsExprType::eMatOpType == SpMatOpType::MATRIX ? LhsExprType::iNumColsStatic : RhsExprType::iNumColsStatic;
          static constexpr SpMatOpType eMatOpType = SpMatOpType::MATRIX;
          static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = util::ExprEvalFlagsHelper<LhsExprType::eExprEvalFlags,
                                                                                              RhsExprType::eExprEvalFlags>::eExprEvalFlags;
          static_assert(LhsExprType::eMatOpType == SpMatOpType::MATRIX || RhsExprType::eMatOpType == SpMatOpType::MATRIX,
                        "At least one operand must be a matrix! Use SpGradient instead if both operands are scalar!");
          static_assert(LhsExprType::iNumRowsStatic == RhsExprType::iNumRowsStatic || bMinOneScalarOp,
                        "Number of rows of two matrix operands do not match!");
          static_assert(LhsExprType::iNumColsStatic == RhsExprType::iNumColsStatic || bMinOneScalarOp,
                        "Number of columns of two matrix operands do not match!");

          constexpr SpMatElemBinExpr(const LhsExprType& u, const RhsExprType& v) noexcept
               :u(u), v(v) {
          }

          SpMatElemBinExpr(SpMatElemBinExpr&& oExpr)
               :u(std::move(oExpr.u)), v(std::move(oExpr.v)) {
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = eExprEvalFlags,
                    typename ValueTypeA, index_type iNumRowsA, index_type iNumColsA>
          void Eval(SpMatrixBase<ValueTypeA, iNumRowsA, iNumColsA>& A) const {
               this->template ElemEval<eTransp, eCompr>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = eExprEvalFlags,
                    typename ValueTypeA, index_type iNumRowsA, index_type iNumColsA>
          void Eval(SpMatrixBase<ValueTypeA, iNumRowsA, iNumColsA>& A,
                    const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemEval<eTransp, eCompr>(A, oDofMap);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                                 const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A, oDofMap);
          }

          constexpr doublereal dGetValue(index_type i, index_type j) const {
               return BinaryFunc::f(u.dGetValue(i, j), v.dGetValue(i, j));
          }

          constexpr index_type iGetSize(index_type i, index_type j) const {
               return u.iGetSize(i, j) + v.iGetSize(i, j);
          }

          constexpr index_type iGetNumRows() const {
               return MatSizeHelp::iGetNumRows(u, v);
          }

          constexpr index_type iGetNumCols() const {
               return MatSizeHelp::iGetNumCols(u, v);
          }

          constexpr index_type iGetMaxSize() const {
               return this->iGetMaxSizeElem();
          }

          template <typename ValueTypeB>
          void InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type i, index_type j) const {
               const doublereal ui = u.dGetValue(i, j);
               const doublereal vi = v.dGetValue(i, j);

               u.InsertDeriv(g, BinaryFunc::df_du(ui, vi) * dCoef, i, j);
               v.InsertDeriv(g, BinaryFunc::df_dv(ui, vi) * dCoef, i, j);
          }

          void GetDofStat(SpGradDofStat& s, index_type i, index_type j) const {
               u.GetDofStat(s, i, j);
               v.GetDofStat(s, i, j);
          }

          void InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               u.InsertDof(oExpDofMap, i, j);
               v.InsertDof(oExpDofMap, i, j);
          }

          template <typename ValueTypeB>
          void AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               const doublereal ui = u.dGetValue(i, j);
               const doublereal vi = v.dGetValue(i, j);

               u.AddDeriv(g, BinaryFunc::df_du(ui, vi) * dCoef, oExpDofMap, i, j);
               v.AddDeriv(g, BinaryFunc::df_dv(ui, vi) * dCoef, oExpDofMap, i, j);
          }

#ifdef SP_GRAD_DEBUG
          template <typename ExprType, typename Expr>
          constexpr bool bHaveRefTo(const SpMatElemExprBase<ExprType, Expr>& A) const {
               return u.bHaveRefTo(A) || v.bHaveRefTo(A);
          }
#endif
          const ValueType* begin() const = delete;
          const ValueType* end() const = delete;
          inline constexpr index_type iGetRowOffset() const = delete;
          inline constexpr index_type iGetColOffset() const = delete;
     private:
          LhsTempExpr u;
          RhsTempExpr v;
     };

     template <typename ValueType, typename LhsExpr, typename RhsExpr>
     class SpMatCrossExpr
          :public SpMatElemExprBase<ValueType, SpMatCrossExpr<ValueType, LhsExpr, RhsExpr> > {
          typedef typename util::remove_all<LhsExpr>::type LhsExprType;
          typedef typename util::remove_all<RhsExpr>::type RhsExprType;
          static constexpr bool bUseTempExprLhs = !(LhsExprType::uMatAccess & util::MatAccessFlag::ELEMENT_WISE) || LhsExprType::iNumElemOps > 0;
          static constexpr bool bUseTempExprRhs = !(RhsExprType::uMatAccess & util::MatAccessFlag::ELEMENT_WISE) || RhsExprType::iNumElemOps > 0;

          typedef util::TempExprHelper<LhsExpr, bUseTempExprLhs> LhsTempHelper;
          typedef util::TempExprHelper<RhsExpr, bUseTempExprRhs> RhsTempHelper;
          typedef typename LhsTempHelper::Type LhsTempExpr;
          typedef typename RhsTempHelper::Type RhsTempExpr;
          typedef typename util::remove_all<LhsTempExpr>::type LhsTempExprType;
          typedef typename util::remove_all<RhsTempExpr>::type RhsTempExprType;
          typedef typename LhsTempExprType::ValueType LhsValueType;
          typedef typename RhsTempExprType::ValueType RhsValueType;
          typedef typename LhsTempExprType::DerivedType LhsDerivedType;
          typedef typename RhsTempExprType::DerivedType RhsDerivedType;
          typedef util::MatrixSizeHelper<LhsValueType, LhsDerivedType, RhsValueType, RhsDerivedType> MatSizeHelp;

          static constexpr index_type x = 1, y = 2, z = 3;
          static constexpr index_type e1[3] = {y, z, x};
          static constexpr index_type e2[3] = {z, x, y};
          static constexpr index_type e3[3] = {z, x, y};
          static constexpr index_type e4[3] = {y, z, x};
     public:
          static constexpr index_type iNumElemOps = LhsTempExprType::iNumElemOps + RhsTempExprType::iNumElemOps + 1;
          static constexpr unsigned uMatAccess = util::MatAccessFlag::ELEMENT_WISE;
          static constexpr index_type iNumRowsStatic = 3;
          static constexpr index_type iNumColsStatic = 1;
          static constexpr SpMatOpType eMatOpType = SpMatOpType::MATRIX;
          static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = util::ExprEvalFlagsHelper<LhsExprType::eExprEvalFlags,
                                                                                              RhsExprType::eExprEvalFlags>::eExprEvalFlags;
          static_assert(LhsExprType::eMatOpType == SpMatOpType::MATRIX && RhsExprType::eMatOpType == SpMatOpType::MATRIX,
                        "Both operands must be vectors!");
          static_assert(LhsExprType::iNumRowsStatic == 3 && RhsExprType::iNumRowsStatic == 3,
                        "Both operands must be 3x1 vectors");
          static_assert(LhsExprType::iNumColsStatic == 1 && RhsExprType::iNumColsStatic == 1,
                        "Both operands must be 3x1 vectors");

          constexpr SpMatCrossExpr(const LhsExprType& u, const RhsExprType& v) noexcept
               :u(LhsTempHelper::EvalUnique(u)), v(RhsTempHelper::EvalUnique(v)) {
          }

          constexpr SpMatCrossExpr(const LhsExprType& u, const RhsExprType& v, const SpGradExpDofMapHelper<ValueType>& oDofMap) noexcept
               :u(LhsTempHelper::EvalUnique(u), oDofMap), v(RhsTempHelper::EvalUnique(v), oDofMap) {
          }

          SpMatCrossExpr(SpMatCrossExpr&& oExpr)
               :u(std::move(oExpr.u)), v(std::move(oExpr.v)) {
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = eExprEvalFlags,
                    typename ValueTypeA, index_type iNumRowsA, index_type iNumColsA>
          void Eval(SpMatrixBase<ValueTypeA, iNumRowsA, iNumColsA>& A) const {
               this->template ElemEval<eTransp, eCompr>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = eExprEvalFlags,
                    typename ValueTypeA, index_type iNumRowsA, index_type iNumColsA>
          void Eval(SpMatrixBase<ValueTypeA, iNumRowsA, iNumColsA>& A,
                    const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemEval<eTransp, eCompr>(A, oDofMap);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                                 const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A, oDofMap);
          }

          doublereal dGetValue(index_type i, index_type j) const {
               SP_GRAD_ASSERT(j == 1);
               SP_GRAD_ASSERT(i >= 1);
               SP_GRAD_ASSERT(i <= 3);

               static_assert(sizeof(e1) / sizeof(e1[0]) == 3, "invalid array size");
               static_assert(sizeof(e2) / sizeof(e2[0]) == 3, "invalid array size");
               static_assert(sizeof(e3) / sizeof(e3[0]) == 3, "invalid array size");
               static_assert(sizeof(e4) / sizeof(e4[0]) == 3, "invalid array size");

               --i;

               return u.dGetValue(e1[i], j) * v.dGetValue(e2[i], j) - u.dGetValue(e3[i], j) * v.dGetValue(e4[i], j);
          }

          index_type iGetSize(index_type i, index_type j) const {
               SP_GRAD_ASSERT(j == 1);
               SP_GRAD_ASSERT(i >= 1);
               SP_GRAD_ASSERT(i <= 3);

               static_assert(sizeof(e1) / sizeof(e1[0]) == 3, "invalid array size");
               static_assert(sizeof(e2) / sizeof(e2[0]) == 3, "invalid array size");
               static_assert(sizeof(e3) / sizeof(e3[0]) == 3, "invalid array size");
               static_assert(sizeof(e4) / sizeof(e4[0]) == 3, "invalid array size");

               --i;

               return u.iGetSize(e1[i], j)
                    + v.iGetSize(e2[i], j)
                    + u.iGetSize(e3[i], j)
                    + v.iGetSize(e4[i], j);
          }

          constexpr index_type iGetNumRows() const {
               return iNumRowsStatic;
          }

          constexpr index_type iGetNumCols() const {
               return iNumColsStatic;
          }

          constexpr index_type iGetMaxSize() const {
               return this->iGetMaxSizeElem();
          }

          template <typename ValueTypeB>
          void InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type i, index_type j) const {
               SP_GRAD_ASSERT(j == 1);
               SP_GRAD_ASSERT(i >= 1);
               SP_GRAD_ASSERT(i <= 3);

               static_assert(sizeof(e1) / sizeof(e1[0]) == 3, "invalid array size");
               static_assert(sizeof(e2) / sizeof(e2[0]) == 3, "invalid array size");
               static_assert(sizeof(e3) / sizeof(e3[0]) == 3, "invalid array size");
               static_assert(sizeof(e4) / sizeof(e4[0]) == 3, "invalid array size");

               --i;

               u.InsertDeriv(g, dCoef * v.dGetValue(e2[i], j), e1[i], j);
               v.InsertDeriv(g, dCoef * u.dGetValue(e1[i], j), e2[i], j);
               u.InsertDeriv(g, -dCoef * v.dGetValue(e4[i], j), e3[i], j);
               v.InsertDeriv(g, -dCoef * u.dGetValue(e3[i], j), e4[i], j);
          }

          void GetDofStat(SpGradDofStat& s, index_type i, index_type j) const {
               SP_GRAD_ASSERT(j == 1);
               SP_GRAD_ASSERT(i >= 1);
               SP_GRAD_ASSERT(i <= 3);

               static_assert(sizeof(e1) / sizeof(e1[0]) == 3, "invalid array size");
               static_assert(sizeof(e2) / sizeof(e2[0]) == 3, "invalid array size");
               static_assert(sizeof(e3) / sizeof(e3[0]) == 3, "invalid array size");
               static_assert(sizeof(e4) / sizeof(e4[0]) == 3, "invalid array size");

               --i;

               u.GetDofStat(s, e1[i], j);
               v.GetDofStat(s, e2[i], j);
               u.GetDofStat(s, e3[i], j);
               v.GetDofStat(s, e4[i], j);
          }

          void InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               SP_GRAD_ASSERT(j == 1);
               SP_GRAD_ASSERT(i >= 1);
               SP_GRAD_ASSERT(i <= 3);

               static_assert(sizeof(e1) / sizeof(e1[0]) == 3, "invalid array size");
               static_assert(sizeof(e2) / sizeof(e2[0]) == 3, "invalid array size");
               static_assert(sizeof(e3) / sizeof(e3[0]) == 3, "invalid array size");
               static_assert(sizeof(e4) / sizeof(e4[0]) == 3, "invalid array size");

               --i;

               u.InsertDof(oExpDofMap, e1[i], j);
               v.InsertDof(oExpDofMap, e2[i], j);
               u.InsertDof(oExpDofMap, e3[i], j);
               v.InsertDof(oExpDofMap, e4[i], j);
          }

          template <typename ValueTypeB>
          void AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               SP_GRAD_ASSERT(j == 1);
               SP_GRAD_ASSERT(i >= 1);
               SP_GRAD_ASSERT(i <= 3);

               static_assert(sizeof(e1) / sizeof(e1[0]) == 3, "invalid array size");
               static_assert(sizeof(e2) / sizeof(e2[0]) == 3, "invalid array size");
               static_assert(sizeof(e3) / sizeof(e3[0]) == 3, "invalid array size");
               static_assert(sizeof(e4) / sizeof(e4[0]) == 3, "invalid array size");

               --i;

               u.AddDeriv(g, dCoef * v.dGetValue(e2[i], j), oExpDofMap, e1[i], j);
               v.AddDeriv(g, dCoef * u.dGetValue(e1[i], j), oExpDofMap, e2[i], j);
               u.AddDeriv(g, -dCoef * v.dGetValue(e4[i], j), oExpDofMap, e3[i], j);
               v.AddDeriv(g, -dCoef * u.dGetValue(e3[i], j), oExpDofMap, e4[i], j);
          }

#ifdef SP_GRAD_DEBUG
          template <typename ExprType, typename Expr>
          constexpr bool bHaveRefTo(const SpMatElemExprBase<ExprType, Expr>& A) const {
               return u.bHaveRefTo(A) || v.bHaveRefTo(A);
          }
#endif
          const ValueType* begin() const = delete;
          const ValueType* end() const = delete;
          inline constexpr index_type iGetRowOffset() const = delete;
          inline constexpr index_type iGetColOffset() const = delete;
     private:
          LhsTempExpr u;
          RhsTempExpr v;
     };

     template <typename ValueType, typename LhsExpr, typename RhsExpr>
     constexpr index_type SpMatCrossExpr<ValueType, LhsExpr, RhsExpr>::e1[3];
     template <typename ValueType, typename LhsExpr, typename RhsExpr>
     constexpr index_type SpMatCrossExpr<ValueType, LhsExpr, RhsExpr>::e2[3];
     template <typename ValueType, typename LhsExpr, typename RhsExpr>
     constexpr index_type SpMatCrossExpr<ValueType, LhsExpr, RhsExpr>::e3[3];
     template <typename ValueType, typename LhsExpr, typename RhsExpr>
     constexpr index_type SpMatCrossExpr<ValueType, LhsExpr, RhsExpr>::e4[3];

     template <typename ValueType, typename UnaryFunc, typename Expr>
     class SpMatElemUnaryExpr
          :public SpMatElemExprBase<ValueType, SpMatElemUnaryExpr<ValueType, UnaryFunc, Expr> > {
          typedef typename util::remove_all<Expr>::type ExprType;
          static constexpr bool bUseTempExpr = !(ExprType::uMatAccess & util::MatAccessFlag::ELEMENT_WISE);
          typedef typename util::TempExprHelper<Expr, bUseTempExpr>::Type TempExpr;
          typedef typename util::remove_all<TempExpr>::type TempExprType;
     public:
          static constexpr index_type iNumElemOps = TempExprType::iNumElemOps + 1;
          static constexpr unsigned uMatAccess = util::MatAccessFlag::ELEMENT_WISE;
          static constexpr index_type iNumRowsStatic = ExprType::iNumRowsStatic;
          static constexpr index_type iNumColsStatic = ExprType::iNumColsStatic;
          static constexpr SpMatOpType eMatOpType = SpMatOpType::MATRIX;
          static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = ExprType::eExprEvalFlags;

          static_assert(ExprType::eMatOpType == SpMatOpType::MATRIX,
                        "Operand must be a matrix! Use SpGradient for scalar expressions!");

          constexpr explicit SpMatElemUnaryExpr(const Expr& u) noexcept
               :u(u) {
          }

          SpMatElemUnaryExpr(SpMatElemUnaryExpr&& oExpr) noexcept
               :u(std::move(oExpr.u)) {
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = eExprEvalFlags, typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemEval<eTransp, eCompr>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = eExprEvalFlags, typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                    const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemEval<eTransp, eCompr>(A, oDofMap);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                          const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A, oDofMap);
          }

          constexpr doublereal dGetValue(index_type i, index_type j) const {
               return UnaryFunc::f(u.dGetValue(i, j));
          }

          constexpr index_type iGetSize(index_type i, index_type j) const {
               return u.iGetSize(i, j);
          }

          constexpr index_type iGetNumRows() const {
               return u.iGetNumRows();
          }

          constexpr index_type iGetNumCols() const {
               return u.iGetNumCols();
          }

          constexpr index_type iGetMaxSize() const {
               return u.iGetMaxSize();
          }

          template <typename ValueTypeB>
          void InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type i, index_type j) const {
               u.InsertDeriv(g, UnaryFunc::df_du(u.dGetValue(i, j)) * dCoef, i, j);
          }

          void GetDofStat(SpGradDofStat& s, index_type i, index_type j) const {
               u.GetDofStat(s, i, j);
          }

          void InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               u.InsertDof(oExpDofMap, i, j);
          }

          template <typename ValueTypeB>
          void AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               u.AddDeriv(g, UnaryFunc::df_du(u.dGetValue(i, j)) * dCoef, oExpDofMap, i, j);
          }

#ifdef SP_GRAD_DEBUG
          template <typename ExprTypeB, typename ExprB>
          constexpr bool bHaveRefTo(const SpMatElemExprBase<ExprTypeB, ExprB>& A) const {
               return u.bHaveRefTo(A);
          }
#endif
          const ValueType* begin() const = delete;
          const ValueType* end() const = delete;
          inline constexpr index_type iGetRowOffset() const = delete;
          inline constexpr index_type iGetColOffset() const = delete;
     private:
          TempExpr u;
     };

     template <typename ValueType, typename Expr>
     class SpMatElemUniqueExpr
          :public SpMatElemExprBase<ValueType, SpMatElemUniqueExpr<ValueType, Expr> > {
          typedef typename util::remove_all<Expr>::type ExprType;
     public:
          static constexpr index_type iNumElemOps = ExprType::iNumElemOps;
          static constexpr unsigned uMatAccess = ExprType::uMatAccess;
          static constexpr index_type iNumRowsStatic = ExprType::iNumRowsStatic;
          static constexpr index_type iNumColsStatic = ExprType::iNumColsStatic;
          static constexpr SpMatOpType eMatOpType = ExprType::eMatOpType;
          static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = util::ComprEvalHelper<ValueType, SpGradCommon::ExprEvalUnique>::eExprEvalFlags;

          static_assert(ExprType::eMatOpType == SpMatOpType::MATRIX,
                        "Operand must be a matrix! Use SpGradient for scalar expressions!");

          constexpr explicit SpMatElemUniqueExpr(const Expr& u) noexcept
               :u(u) {
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = eExprEvalFlags, typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               u.template Eval<eTransp, eExprEvalFlags>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = eExprEvalFlags, typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A, const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               u.template Eval<eTransp, eExprEvalFlags>(A, oDofMap);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                                 const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A, oDofMap);
          }

          constexpr doublereal dGetValue(index_type i, index_type j) const {
               return u.dGetValue(i, j);
          }

          constexpr index_type iGetSize(index_type i, index_type j) const {
               return u.iGetSize(i, j);
          }

          constexpr index_type iGetNumRows() const {
               return u.iGetNumRows();
          }

          constexpr index_type iGetNumCols() const {
               return u.iGetNumCols();
          }

          constexpr index_type iGetMaxSize() const {
               return u.iGetMaxSize();
          }

          template <typename ValueTypeB>
          void InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type i, index_type j) const {
               u.InsertDeriv(g, dCoef, i, j);
          }

          void GetDofStat(SpGradDofStat& s, index_type i, index_type j) const {
               u.GetDofStat(s, i, j);
          }

          void InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               u.InsertDof(oExpDofMap, i, j);
          }

          template <typename ValueTypeB>
          void AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               u.AddDeriv(g, dCoef, oExpDofMap, i, j);
          }

#ifdef SP_GRAD_DEBUG
          template <typename ExprTypeB, typename ExprB>
          constexpr bool bHaveRefTo(const SpMatElemExprBase<ExprTypeB, ExprB>& A) const {
               return u.bHaveRefTo(A);
          }
#endif
          const ValueType* begin() const { return u.begin(); }
          const ValueType* end() const { return u.end(); }
          inline constexpr index_type iGetRowOffset() const { return u.iGetRowOffset(); }
          inline constexpr index_type iGetColOffset() const { return u.iGetColOffset(); }
     private:
          const Expr u;
     };

     template <typename ValueType, typename Expr>
     class SpMatElemTranspExpr
          :public SpMatElemExprBase<ValueType, SpMatElemTranspExpr<ValueType, Expr> > {
          typedef typename util::remove_all<Expr>::type ExprType;
     public:
          static constexpr index_type iNumElemOps = ExprType::iNumElemOps;
          static constexpr unsigned uMatAccess = ExprType::uMatAccess;
          static constexpr index_type iNumRowsStatic = ExprType::iNumColsStatic;
          static constexpr index_type iNumColsStatic = ExprType::iNumRowsStatic;
          static constexpr SpMatOpType eMatOpType = SpMatOpType::MATRIX;
          static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = ExprType::eExprEvalFlags;

          static_assert(ExprType::eMatOpType == SpMatOpType::MATRIX, "Operand must be a matrix! A scalar cannot be transposed!");

          constexpr explicit SpMatElemTranspExpr(const Expr& u) noexcept
               :u(u) {
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               u.template Eval<util::TranspMatEvalHelper<eTransp>::Value, eCompr>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemAssign<util::TranspMatEvalHelper<eTransp>::Value, eCompr, Func>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                    const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               u.template Eval<util::TranspMatEvalHelper<eTransp>::Value, eCompr>(A, oDofMap);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                                 const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemAssign<util::TranspMatEvalHelper<eTransp>::Value, eCompr, Func>(A, oDofMap);
          }

          constexpr doublereal dGetValue(index_type i, index_type j) const {
               return u.dGetValue(j, i);
          }

          constexpr index_type iGetSize(index_type i, index_type j) const {
               return u.iGetSize(j, i);
          }

          constexpr index_type iGetNumRows() const noexcept {
               return u.iGetNumCols();
          }

          constexpr index_type iGetNumCols() const noexcept {
               return u.iGetNumRows();
          }

          constexpr index_type iGetMaxSize() const noexcept {
               return u.iGetMaxSize();
          }

          template <typename ValueTypeB>
          void InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type i, index_type j) const {
               u.InsertDeriv(g, dCoef, j, i);
          }

          void GetDofStat(SpGradDofStat& s, index_type i, index_type j) const noexcept {
               u.GetDofStat(s, j, i);
          }

          void InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               u.InsertDof(oExpDofMap, j, i);
          }

          template <typename ValueTypeB>
          void AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               u.AddDeriv(g, dCoef, oExpDofMap, j, i);
          }

#ifdef SP_GRAD_DEBUG
          template <typename ExprTypeB, typename ExprB>
          constexpr bool bHaveRefTo(const SpMatElemExprBase<ExprTypeB, ExprB>& A) const noexcept {
               return u.bHaveRefTo(A);
          }
#endif
          const Expr& Transpose() const { return u; }

          const ValueType* begin() const noexcept { return u.begin(); }
          const ValueType* end() const noexcept { return u.end(); }
          inline constexpr index_type iGetRowOffset() const noexcept { return u.iGetColOffset(); }
          inline constexpr index_type iGetColOffset() const noexcept { return u.iGetRowOffset(); }
     private:
          const Expr u;
     };

     template <typename ValueType, typename Expr>
     class SpMatColVecExpr
          :public SpMatElemExprBase<ValueType, SpMatColVecExpr<ValueType, Expr> > {
          typedef typename util::remove_all<Expr>::type ExprType;
     public:
          static constexpr index_type iNumElemOps = ExprType::iNumElemOps;
          static constexpr unsigned uMatAccess = ExprType::uMatAccess;
          static constexpr index_type iNumRowsStatic = ExprType::iNumRowsStatic;
          static constexpr index_type iNumColsStatic = 1;
          static constexpr SpMatOpType eMatOpType = SpMatOpType::MATRIX;
          static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = ExprType::eExprEvalFlags;

          static_assert(ExprType::eMatOpType == SpMatOpType::MATRIX, "Operand must be a matrix!");

          constexpr SpMatColVecExpr(const Expr& u, index_type iCol) noexcept
               :u(u), iCol(iCol) {
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemEval<eTransp, eCompr>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A, const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemEval<eTransp, eCompr>(A, oDofMap);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                                 const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A, oDofMap);
          }

          doublereal dGetValue(index_type i, index_type j) const {
               SP_GRAD_ASSERT(j == 1);

               return u.dGetValue(i, iCol);
          }

          index_type iGetSize(index_type i, index_type j) const {
               SP_GRAD_ASSERT(j == 1);

               return u.iGetSize(i, iCol);
          }

          constexpr index_type iGetNumRows() const noexcept {
               return u.iGetNumRows();
          }

          static constexpr index_type iGetNumCols() noexcept {
               return 1;
          }

          constexpr index_type iGetMaxSize() const noexcept {
               index_type iMaxSize = 0;

               for (index_type i = 1; i <= u.iGetNumRows(); ++i) {
                    iMaxSize = std::max(iMaxSize, u.iGetSize(i, iCol));
               }

               return iMaxSize;
          }

          template <typename ValueTypeB>
          void InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type i, index_type j) const {
               SP_GRAD_ASSERT(j == 1);

               u.InsertDeriv(g, dCoef, i, iCol);
          }

          void GetDofStat(SpGradDofStat& s, index_type i, index_type j) const noexcept {
               SP_GRAD_ASSERT(j == 1);

               u.GetDofStat(s, i, iCol);
          }

          void InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               SP_GRAD_ASSERT(j == 1);

               u.InsertDof(oExpDofMap, i, iCol);
          }

          template <typename ValueTypeB>
          void AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               SP_GRAD_ASSERT(j == 1);

               u.AddDeriv(g, dCoef, oExpDofMap, i, iCol);
          }

#ifdef SP_GRAD_DEBUG
          template <typename ExprTypeB, typename ExprB>
          constexpr bool bHaveRefTo(const SpMatElemExprBase<ExprTypeB, ExprB>& A) const noexcept {
               return u.bHaveRefTo(A);
          }
#endif
          const ValueType* begin() const { return u.begin() + (iCol - 1) * u.iGetColOffset(); }
          const ValueType* end() const { return u.begin() + iCol * u.iGetColOffset(); }
          inline constexpr index_type iGetRowOffset() const { return u.iGetRowOffset(); }
          inline constexpr index_type iGetColOffset() const { return u.iGetColOffset(); }
     private:
          const Expr u;
          const index_type iCol;
     };

     template <typename ValueType, typename Expr>
     class SpMatRowVecExpr
          :public SpMatElemExprBase<ValueType, SpMatRowVecExpr<ValueType, Expr> > {
          typedef typename util::remove_all<Expr>::type ExprType;
     public:
          static constexpr index_type iNumElemOps = ExprType::iNumElemOps;
          static constexpr unsigned uMatAccess = ExprType::uMatAccess;
          static constexpr index_type iNumRowsStatic = 1;
          static constexpr index_type iNumColsStatic = ExprType::iNumColsStatic;
          static constexpr SpMatOpType eMatOpType = SpMatOpType::MATRIX;
          static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = ExprType::eExprEvalFlags;

          static_assert(ExprType::eMatOpType == SpMatOpType::MATRIX, "Operand must be a matrix!");

          constexpr SpMatRowVecExpr(const Expr& u, index_type iRow) noexcept
               :u(u), iRow(iRow) {
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemEval<eTransp, eCompr>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                    const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemEval<eTransp, eCompr>(A, oDofMap);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                                 const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A, oDofMap);
          }

          doublereal dGetValue(index_type i, index_type j) const {
               SP_GRAD_ASSERT(i == 1);

               return u.dGetValue(iRow, j);
          }

          index_type iGetSize(index_type i, index_type j) const {
               SP_GRAD_ASSERT(i == 1);

               return u.iGetSize(iRow, j);
          }

          static constexpr index_type iGetNumRows() noexcept {
               return 1;
          }

          constexpr index_type iGetNumCols() const noexcept {
               return u.iGetNumCols();
          }

          constexpr index_type iGetMaxSize() const noexcept {
               index_type iMaxSize = 0;

               for (index_type j = 1; j <= u.iGetNumCols(); ++j) {
                    iMaxSize = std::max(iMaxSize, u.iGetSize(iRow, j));
               }

               return iMaxSize;
          }

          template <typename ValueTypeB>
          void InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type i, index_type j) const {
               SP_GRAD_ASSERT(i == 1);

               u.InsertDeriv(g, dCoef, iRow, j);
          }

          void GetDofStat(SpGradDofStat& s, index_type i, index_type j) const noexcept {
               SP_GRAD_ASSERT(i == 1);

               u.GetDofStat(s, iRow, j);
          }

          void InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               SP_GRAD_ASSERT(i == 1);

               u.InsertDof(oExpDofMap, iRow, j);
          }

          template <typename ValueTypeB>
          void AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               SP_GRAD_ASSERT(i == 1);

               u.AddDeriv(g, dCoef, oExpDofMap, iRow, j);
          }

#ifdef SP_GRAD_DEBUG
          template <typename ExprTypeB, typename ExprB>
          constexpr bool bHaveRefTo(const SpMatElemExprBase<ExprTypeB, ExprB>& A) const noexcept {
               return u.bHaveRefTo(A);
          }
#endif

          const ValueType* begin() const { return u.begin() + (iRow - 1) * u.iGetRowOffset(); }
          const ValueType* end() const { return u.begin() + iRow * u.iGetRowOffset(); }
          inline constexpr index_type iGetRowOffset() const { return u.iGetRowOffset(); }
          inline constexpr index_type iGetColOffset() const { return u.iGetColOffset(); }

     private:
          const Expr u;
          const index_type iRow;
     };

     template <typename ValueType, typename Expr>
     class SpSubMatDynExpr
          :public SpMatElemExprBase<ValueType, SpSubMatDynExpr<ValueType, Expr> > {
          typedef typename util::remove_all<Expr>::type ExprType;
     public:
          static constexpr index_type iNumElemOps = ExprType::iNumElemOps;
          static constexpr unsigned uMatAccess = ExprType::uMatAccess;
          static constexpr index_type iNumRowsStatic = SpMatrixSize::DYNAMIC;
          static constexpr index_type iNumColsStatic = SpMatrixSize::DYNAMIC;
          static constexpr SpMatOpType eMatOpType = SpMatOpType::MATRIX;
          static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = ExprType::eExprEvalFlags;

          static_assert(ExprType::eMatOpType == SpMatOpType::MATRIX, "Operand must be a matrix! A scalar cannot be transposed!");

          explicit SpSubMatDynExpr(const Expr& u, index_type iRowStart, index_type iRowStep, index_type iNumRows, index_type iColStart, index_type iColStep, index_type iNumCols) noexcept
               :u(u),
                iRowStart(iRowStart),
                iRowStep(iRowStep),
                iNumRows(iNumRows),
                iColStart(iColStart),
                iColStep(iColStep),
                iNumCols(iNumCols)
          {
               SP_GRAD_ASSERT(iRowStart >= 1);
               SP_GRAD_ASSERT(iColStart >= 1);
               SP_GRAD_ASSERT(iRowStart <= u.iGetNumRows());
               SP_GRAD_ASSERT(iColStart <= u.iGetNumCols());
               SP_GRAD_ASSERT(iRowStep >= 1);
               SP_GRAD_ASSERT(iColStep >= 1);
               SP_GRAD_ASSERT(iNumRows >= 1);
               SP_GRAD_ASSERT(iNumCols >= 1);
               SP_GRAD_ASSERT(iNumRows <= u.iGetNumRows());
               SP_GRAD_ASSERT(iNumCols <= u.iGetNumCols());

#ifdef SP_GRAD_DEBUG
               for (index_type i = 1; i <= iGetNumRows(); ++i) {
                    SP_GRAD_ASSERT(iGetRowIndex(i) >= 1);
                    SP_GRAD_ASSERT(iGetRowIndex(i) <= u.iGetNumRows());
               }

               for (index_type j = 1; j <= iGetNumCols(); ++j) {
                    SP_GRAD_ASSERT(iGetColIndex(j) >= 1);
                    SP_GRAD_ASSERT(iGetColIndex(j) <= u.iGetNumCols());
               }
#endif
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemEval<eTransp, eCompr>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                    const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemEval<eTransp, eCompr>(A, oDofMap);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                                 const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A, oDofMap);
          }

          constexpr doublereal dGetValue(index_type i, index_type j) const {
               return u.dGetValue(iGetRowIndex(i), iGetColIndex(j));
          }

          constexpr index_type iGetSize(index_type i, index_type j) const {
               return u.iGetSize(iGetRowIndex(i), iGetColIndex(j));
          }

          constexpr index_type iGetNumRows() const noexcept {
               return iNumRows;
          }

          constexpr index_type iGetNumCols() const noexcept {
               return iNumCols;
          }

          constexpr index_type iGetMaxSize() const noexcept {
               return u.iGetMaxSize();
          }

          template <typename ValueTypeB>
          void InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type i, index_type j) const {
               u.InsertDeriv(g, dCoef, iGetRowIndex(i), iGetColIndex(j));
          }

          void GetDofStat(SpGradDofStat& s, index_type i, index_type j) const noexcept {
               u.GetDofStat(s, iGetRowIndex(i), iGetColIndex(j));
          }

          void InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               u.InsertDof(oExpDofMap, iGetRowIndex(i), iGetColIndex(j));
          }

          template <typename ValueTypeB>
          void AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               u.AddDeriv(g, dCoef, oExpDofMap, iGetRowIndex(i), iGetColIndex(j));
          }

#ifdef SP_GRAD_DEBUG
          template <typename ExprTypeB, typename ExprB>
          constexpr bool bHaveRefTo(const SpMatElemExprBase<ExprTypeB, ExprB>& A) const noexcept {
               return u.bHaveRefTo(A);
          }
#endif

          const ValueType* begin() const noexcept { return u.begin() + (iRowStart - 1) * u.iGetRowOffset() + (iColStart - 1) * u.iGetColOffset(); }
          const ValueType* end() const noexcept { return begin() + iGetRowOffset() * iNumRows * iGetColOffset() * iNumCols; }
          inline constexpr index_type iGetRowOffset() const noexcept { return u.iGetRowOffset() * iRowStep; }
          inline constexpr index_type iGetColOffset() const noexcept { return u.iGetColOffset() * iColStep; }

     private:
          index_type iGetRowIndex(index_type i) const noexcept {
               SP_GRAD_ASSERT(i >= 1);
               SP_GRAD_ASSERT(i <= iNumRows);

               const index_type iRowIndex = iRowStart + (i - 1) * iRowStep;

               SP_GRAD_ASSERT(iRowIndex >= 1);
               SP_GRAD_ASSERT(iRowIndex <= u.iGetNumRows());

               return iRowIndex;
          }

          index_type iGetColIndex(index_type j) const noexcept {
               SP_GRAD_ASSERT(j >= 1);
               SP_GRAD_ASSERT(j <= iNumCols);

               const index_type iColIndex = iColStart + (j - 1) * iColStep;

               SP_GRAD_ASSERT(iColIndex >= 1);
               SP_GRAD_ASSERT(iColIndex <= u.iGetNumCols());

               return iColIndex;
          }
          const Expr u;
          const index_type iRowStart, iRowStep, iNumRows, iColStart, iColStep, iNumCols;
     };

     template <typename ValueType, typename Expr, index_type iRowStart, index_type iRowStep, index_type iNumRows, index_type iColStart, index_type iColStep, index_type iNumCols>
     class SpSubMatStatExpr
          :public SpMatElemExprBase<ValueType, SpSubMatStatExpr<ValueType, Expr, iRowStart, iRowStep, iNumRows, iColStart, iColStep, iNumCols> > {
          typedef typename util::remove_all<Expr>::type ExprType;
     public:
          static constexpr index_type iNumElemOps = ExprType::iNumElemOps;
          static constexpr unsigned uMatAccess = ExprType::uMatAccess;
          static constexpr index_type iNumRowsStatic = iNumRows;
          static constexpr index_type iNumColsStatic = iNumCols;
          static constexpr SpMatOpType eMatOpType = SpMatOpType::MATRIX;
          static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = ExprType::eExprEvalFlags;

          static_assert(ExprType::iNumRowsStatic != SpMatrixSize::DYNAMIC, "Operand row size must dynamic");
          static_assert(ExprType::iNumColsStatic != SpMatrixSize::DYNAMIC, "Operand column size must be dynamic");
          static_assert(ExprType::eMatOpType == SpMatOpType::MATRIX, "Operand must be a matrix! A scalar cannot be transposed!");
          static_assert(iRowStart >= 1, "invalid row index");
          static_assert(iColStart >= 1, "invalid column index");
          static_assert(iRowStart <= ExprType::iNumRowsStatic, "invalid row index");
          static_assert(iColStart <= ExprType::iNumColsStatic, "invalid column index");
          static_assert(iRowStep >= 1, "invalid row step");
          static_assert(iColStep >= 1, "invalid column step");
          static_assert(iNumRows >= 1, "invalid number of rows");
          static_assert(iNumCols >= 1, "invalid number of columns");
          static_assert(iRowStart + (iNumRows - 1) * iRowStep <= ExprType::iNumRowsStatic, "row index out of range");
          static_assert(iColStart + (iNumCols - 1) * iColStep <= ExprType::iNumColsStatic, "column index out of range");

          explicit SpSubMatStatExpr(const Expr& u) noexcept
               :u(u)
               {
#ifdef SP_GRAD_DEBUG
                    for (index_type i = 1; i <= iGetNumRows(); ++i) {
                         SP_GRAD_ASSERT(iGetRowIndex(i) >= 1);
                         SP_GRAD_ASSERT(iGetRowIndex(i) <= u.iGetNumRows());
                    }

                    for (index_type j = 1; j <= iGetNumCols(); ++j) {
                         SP_GRAD_ASSERT(iGetColIndex(j) >= 1);
                         SP_GRAD_ASSERT(iGetColIndex(j) <= u.iGetNumCols());
                    }
#endif
               }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemEval<eTransp, eCompr>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A, const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemEval<eTransp, eCompr>(A, oDofMap);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A, const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A, oDofMap);
          }

          constexpr doublereal dGetValue(index_type i, index_type j) const {
               return u.dGetValue(iGetRowIndex(i), iGetColIndex(j));
          }

          constexpr index_type iGetSize(index_type i, index_type j) const {
               return u.iGetSize(iGetRowIndex(i), iGetColIndex(j));
          }

          static constexpr index_type iGetNumRows() noexcept {
               return iNumRowsStatic;
          }

          static constexpr index_type iGetNumCols() noexcept {
               return iNumColsStatic;
          }

          constexpr index_type iGetMaxSize() const noexcept {
               return u.iGetMaxSize();
          }

          template <typename ValueTypeB>
          void InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type i, index_type j) const {
               u.InsertDeriv(g, dCoef, iGetRowIndex(i), iGetColIndex(j));
          }

          void GetDofStat(SpGradDofStat& s, index_type i, index_type j) const noexcept {
               u.GetDofStat(s, iGetRowIndex(i), iGetColIndex(j));
          }

          void InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               u.InsertDof(oExpDofMap, iGetRowIndex(i), iGetColIndex(j));
          }

          template <typename ValueTypeB>
          void AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               u.AddDeriv(g, dCoef, oExpDofMap, iGetRowIndex(i), iGetColIndex(j));
          }

#ifdef SP_GRAD_DEBUG
          template <typename ExprTypeB, typename ExprB>
          constexpr bool bHaveRefTo(const SpMatElemExprBase<ExprTypeB, ExprB>& A) const noexcept {
               return u.bHaveRefTo(A);
          }
#endif
          const ValueType* begin() const noexcept { return u.begin() + (iRowStart - 1) * u.iGetRowOffset() + (iColStart - 1) * u.iGetColOffset(); }
          const ValueType* end() const noexcept { return begin() + iGetRowOffset() * iNumRows * iGetColOffset() * iNumCols; }
          inline constexpr index_type iGetRowOffset() const noexcept { return u.iGetRowOffset() * iRowStep; }
          inline constexpr index_type iGetColOffset() const noexcept { return u.iGetColOffset() * iColStep; }

     private:
          static index_type iGetRowIndex(index_type i) noexcept {
               SP_GRAD_ASSERT(i >= 1);
               SP_GRAD_ASSERT(i <= iNumRows);

               const index_type iRowIndex = iRowStart + (i - 1) * iRowStep;

               SP_GRAD_ASSERT(iRowIndex >= 1);
               SP_GRAD_ASSERT(iRowIndex <= ExprType::iNumRowsStatic);

               return iRowIndex;
          }

          static index_type iGetColIndex(index_type j) noexcept {
               SP_GRAD_ASSERT(j >= 1);
               SP_GRAD_ASSERT(j <= iNumCols);

               const index_type iColIndex = iColStart + (j - 1) * iColStep;

               SP_GRAD_ASSERT(iColIndex >= 1);
               SP_GRAD_ASSERT(iColIndex <= ExprType::iNumColsStatic);

               return iColIndex;
          }

          const Expr u;
     };

     template <typename ValueType, typename Expr, index_type iNumRows, index_type iNumCols>
     class SpSubMatStatResExpr: public SpMatElemExprBase<ValueType, SpSubMatStatResExpr<ValueType, Expr, iNumRows, iNumCols> > {
          typedef typename util::remove_all<Expr>::type ExprType;
     public:
          static constexpr index_type iNumElemOps = ExprType::iNumElemOps;
          static constexpr unsigned uMatAccess = ExprType::uMatAccess;
          static constexpr index_type iNumRowsStatic = iNumRows;
          static constexpr index_type iNumColsStatic = iNumCols;
          static constexpr SpMatOpType eMatOpType = SpMatOpType::MATRIX;
          static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = ExprType::eExprEvalFlags;

          static_assert(ExprType::eMatOpType == SpMatOpType::MATRIX, "Operand must be a matrix! A scalar cannot be transposed!");
          static_assert(iNumRows >= 1, "invalid number of rows");
          static_assert(iNumCols >= 1, "invalid number of columns");
          static_assert(iNumRows != SpMatrixSize::DYNAMIC, "static matrix size required");
          static_assert(iNumCols != SpMatrixSize::DYNAMIC, "static matrix size required");

          explicit SpSubMatStatResExpr(const Expr& u, index_type iRowStart, index_type iRowStep, index_type iColStart, index_type iColStep) noexcept
               :u(u),
                iRowStart(iRowStart),
                iRowStep(iRowStep),
                iColStart(iColStart),
                iColStep(iColStep)
               {
#ifdef SP_GRAD_DEBUG
                    SP_GRAD_ASSERT(iRowStart >= 1);
                    SP_GRAD_ASSERT(iColStart >= 1);
                    SP_GRAD_ASSERT(iRowStart <= u.iGetNumRows());
                    SP_GRAD_ASSERT(iColStart <= u.iGetNumCols());
                    SP_GRAD_ASSERT(iRowStep >= 1);
                    SP_GRAD_ASSERT(iColStep >= 1);
                    SP_GRAD_ASSERT(iNumRows >= 1);
                    SP_GRAD_ASSERT(iNumCols >= 1);
                    SP_GRAD_ASSERT(iNumRows <= u.iGetNumRows());
                    SP_GRAD_ASSERT(iNumCols <= u.iGetNumCols());

                    for (index_type i = 1; i <= iGetNumRows(); ++i) {
                         SP_GRAD_ASSERT(iGetRowIndex(i) >= 1);
                         SP_GRAD_ASSERT(iGetRowIndex(i) <= u.iGetNumRows());
                    }

                    for (index_type j = 1; j <= iGetNumCols(); ++j) {
                         SP_GRAD_ASSERT(iGetColIndex(j) >= 1);
                         SP_GRAD_ASSERT(iGetColIndex(j) <= u.iGetNumCols());
                    }
#endif
               }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemEval<eTransp, eCompr>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                    const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemEval<eTransp, eCompr>(A, oDofMap);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                                 const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A, oDofMap);
          }

          constexpr doublereal dGetValue(index_type i, index_type j) const {
               return u.dGetValue(iGetRowIndex(i), iGetColIndex(j));
          }

          constexpr index_type iGetSize(index_type i, index_type j) const {
               return u.iGetSize(iGetRowIndex(i), iGetColIndex(j));
          }

          static constexpr index_type iGetNumRows() noexcept {
               return iNumRowsStatic;
          }

          static constexpr index_type iGetNumCols() noexcept {
               return iNumColsStatic;
          }

          constexpr index_type iGetMaxSize() const noexcept {
               return u.iGetMaxSize();
          }

          template <typename ValueTypeB>
          void InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type i, index_type j) const {
               u.InsertDeriv(g, dCoef, iGetRowIndex(i), iGetColIndex(j));
          }

          void GetDofStat(SpGradDofStat& s, index_type i, index_type j) const noexcept {
               u.GetDofStat(s, iGetRowIndex(i), iGetColIndex(j));
          }

          void InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               u.InsertDof(oExpDofMap, iGetRowIndex(i), iGetColIndex(j));
          }

          template <typename ValueTypeB>
          void AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               u.AddDeriv(g, dCoef, oExpDofMap, iGetRowIndex(i), iGetColIndex(j));
          }

#ifdef SP_GRAD_DEBUG
          template <typename ExprTypeB, typename ExprB>
          constexpr bool bHaveRefTo(const SpMatElemExprBase<ExprTypeB, ExprB>& A) const noexcept {
               return u.bHaveRefTo(A);
          }
#endif
          const ValueType* begin() const noexcept { return u.begin() + (iRowStart - 1) * u.iGetRowOffset() + (iColStart - 1) * u.iGetColOffset(); }
          const ValueType* end() const noexcept { return begin() + iGetRowOffset() * iNumRows * iGetColOffset() * iNumCols; }
          inline constexpr index_type iGetRowOffset() const noexcept { return u.iGetRowOffset() * iRowStep; }
          inline constexpr index_type iGetColOffset() const noexcept { return u.iGetColOffset() * iColStep; }

     private:
          index_type iGetRowIndex(index_type i) const noexcept {
               SP_GRAD_ASSERT(i >= 1);
               SP_GRAD_ASSERT(i <= iNumRows);

               const index_type iRowIndex = iRowStart + (i - 1) * iRowStep;

               SP_GRAD_ASSERT(iRowIndex >= 1);
               SP_GRAD_ASSERT(iRowIndex <= u.iGetNumRows());

               return iRowIndex;
          }

          index_type iGetColIndex(index_type j) const noexcept {
               SP_GRAD_ASSERT(j >= 1);
               SP_GRAD_ASSERT(j <= iNumCols);

               const index_type iColIndex = iColStart + (j - 1) * iColStep;

               SP_GRAD_ASSERT(iColIndex >= 1);
               SP_GRAD_ASSERT(iColIndex <= u.iGetNumCols());

               return iColIndex;
          }

          const Expr u;
          const index_type iRowStart, iRowStep, iColStart, iColStep;
     };

     template <typename ValueType, typename Expr, index_type iRowStart, index_type iRowStep, index_type iNumRows>
     class SpSubMatStatRowExpr
     :public SpMatElemExprBase<ValueType, SpSubMatStatRowExpr<ValueType, Expr, iRowStart, iRowStep, iNumRows> > {
          typedef typename util::remove_all<Expr>::type ExprType;
     public:
          static constexpr index_type iNumElemOps = ExprType::iNumElemOps;
          static constexpr unsigned uMatAccess = ExprType::uMatAccess;
          static constexpr index_type iNumRowsStatic = iNumRows;
          static constexpr index_type iNumColsStatic = SpMatrixSize::DYNAMIC;
          static constexpr SpMatOpType eMatOpType = SpMatOpType::MATRIX;
          static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = ExprType::eExprEvalFlags;

          static_assert(ExprType::iNumRowsStatic >= 1, "Number of rows of operand must be static");
          static_assert(ExprType::iNumColsStatic == iNumColsStatic, "Number of columns of operand must be dynamic");
          static_assert(iNumRows >= 1, "Number of rows must be static");
          static_assert(iRowStart >= 1, "invalid row index");
          static_assert(iRowStep >= 1, "invalid row step");
          static_assert(iRowStart + (iNumRows - 1) * iRowStep <= ExprType::iNumRowsStatic, "index out of range");
          static_assert(ExprType::eMatOpType == SpMatOpType::MATRIX, "Operand must be a matrix! A scalar cannot be transposed!");

          explicit SpSubMatStatRowExpr(const Expr& u, index_type iColStart, index_type iColStep, index_type iNumCols) noexcept
               :u(u),
                iColStart(iColStart),
                iColStep(iColStep),
                iNumCols(iNumCols)
          {
               SP_GRAD_ASSERT(iRowStart >= 1);
               SP_GRAD_ASSERT(iColStart >= 1);
               SP_GRAD_ASSERT(iRowStart <= u.iGetNumRows());
               SP_GRAD_ASSERT(iColStart <= u.iGetNumCols());
               SP_GRAD_ASSERT(iRowStep >= 1);
               SP_GRAD_ASSERT(iColStep >= 1);
               SP_GRAD_ASSERT(iNumRows >= 1);
               SP_GRAD_ASSERT(iNumCols >= 1);
               SP_GRAD_ASSERT(iNumRows <= u.iGetNumRows());
               SP_GRAD_ASSERT(iNumCols <= u.iGetNumCols());

#ifdef SP_GRAD_DEBUG
               for (index_type i = 1; i <= iGetNumRows(); ++i) {
                    SP_GRAD_ASSERT(iGetRowIndex(i) >= 1);
                    SP_GRAD_ASSERT(iGetRowIndex(i) <= u.iGetNumRows());
               }

               for (index_type j = 1; j <= iGetNumCols(); ++j) {
                    SP_GRAD_ASSERT(iGetColIndex(j) >= 1);
                    SP_GRAD_ASSERT(iGetColIndex(j) <= u.iGetNumCols());
               }
#endif
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemEval<eTransp, eCompr>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A, const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemEval<eTransp, eCompr>(A, oDofMap);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A, const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A, oDofMap);
          }

          constexpr doublereal dGetValue(index_type i, index_type j) const {
               return u.dGetValue(iGetRowIndex(i), iGetColIndex(j));
          }

          constexpr index_type iGetSize(index_type i, index_type j) const {
               return u.iGetSize(iGetRowIndex(i), iGetColIndex(j));
          }

          static constexpr index_type iGetNumRows() noexcept {
               return iNumRows;
          }

          constexpr index_type iGetNumCols() const noexcept {
               return iNumCols;
          }

          constexpr index_type iGetMaxSize() const noexcept {
               return u.iGetMaxSize();
          }

          template <typename ValueTypeB>
          void InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type i, index_type j) const {
               u.InsertDeriv(g, dCoef, iGetRowIndex(i), iGetColIndex(j));
          }

          void GetDofStat(SpGradDofStat& s, index_type i, index_type j) const noexcept {
               u.GetDofStat(s, iGetRowIndex(i), iGetColIndex(j));
          }

          void InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               u.InsertDof(oExpDofMap, iGetRowIndex(i), iGetColIndex(j));
          }

          template <typename ValueTypeB>
          void AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
               u.AddDeriv(g, dCoef, oExpDofMap, iGetRowIndex(i), iGetColIndex(j));
          }

#ifdef SP_GRAD_DEBUG
          template <typename ExprTypeB, typename ExprB>
          constexpr bool bHaveRefTo(const SpMatElemExprBase<ExprTypeB, ExprB>& A) const noexcept {
               return u.bHaveRefTo(A);
          }
#endif
          const ValueType* begin() const noexcept { return u.begin() + (iRowStart - 1) * u.iGetRowOffset() + (iColStart - 1) * u.iGetColOffset(); }
          const ValueType* end() const noexcept { return begin() + iGetRowOffset() * iNumRows * iGetColOffset() * iNumCols; }
          inline constexpr index_type iGetRowOffset() const noexcept { return u.iGetRowOffset() * iRowStep; }
          inline constexpr index_type iGetColOffset() const noexcept { return u.iGetColOffset() * iColStep; }

     private:
          static index_type iGetRowIndex(index_type i) noexcept {
               SP_GRAD_ASSERT(i >= 1);
               SP_GRAD_ASSERT(i <= iNumRows);

               const index_type iRowIndex = iRowStart + (i - 1) * iRowStep;

               return iRowIndex;
          }

          index_type iGetColIndex(index_type j) const noexcept {
               SP_GRAD_ASSERT(j >= 1);
               SP_GRAD_ASSERT(j <= iNumCols);

               const index_type iColIndex = iColStart + (j - 1) * iColStep;

               SP_GRAD_ASSERT(iColIndex >= 1);
               SP_GRAD_ASSERT(iColIndex <= u.iGetNumCols());

               return iColIndex;
          }

          const Expr u;
          const index_type iColStart, iColStep, iNumCols;
     };

     template <typename LhsValue, typename RhsValue, typename LhsExpr, typename RhsExpr>
     class SpMatMulExpr
          :public SpMatElemExprBase<typename util::ResultType<LhsValue, RhsValue>::Type,
                                    SpMatMulExpr<LhsValue, RhsValue, LhsExpr, RhsExpr> > {
          typedef typename util::remove_all<LhsExpr>::type LhsExprType;
          typedef typename util::remove_all<RhsExpr>::type RhsExprType;
          enum OpCntEstimate: index_type {
               ASSUMED_OPERATION_COUNT = LhsExprType::iNumColsStatic == SpMatrixSize::DYNAMIC ? 10 : LhsExprType::iNumColsStatic
          };
     public:
          typedef typename util::ResultType<LhsValue, RhsValue>::Type ValueType;

          static constexpr index_type iNumElemOps = (util::remove_all<LhsExpr>::type::iNumElemOps +
                                                     util::remove_all<RhsExpr>::type::iNumElemOps + 2) * ASSUMED_OPERATION_COUNT;

          static constexpr unsigned uMatAccess = util::MatAccessFlag::MATRIX_WISE;
          static constexpr index_type iNumRowsStatic = LhsExprType::iNumRowsStatic;
          static constexpr index_type iNumColsStatic = RhsExprType::iNumColsStatic;
          static constexpr SpMatOpType eMatOpType = SpMatOpType::MATRIX;
          static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = util::ExprEvalFlagsHelper<LhsExprType::eExprEvalFlags,
                                                                                              RhsExprType::eExprEvalFlags>::eExprEvalFlags;

          static_assert(LhsExprType::eMatOpType == SpMatOpType::MATRIX, "Left hand side of matrix product must be a matrix");
          static_assert(RhsExprType::eMatOpType == SpMatOpType::MATRIX, "Right hand side of matrix product must be a matrix");
          static_assert(LhsExprType::iNumColsStatic == RhsExprType::iNumRowsStatic, "Incompatible matrix sizes in matrix product");

          SpMatMulExpr(const LhsExpr& u, const RhsExpr& v) noexcept
               :u(u), v(v) {
               SP_GRAD_ASSERT(u.iGetNumCols() == v.iGetNumRows());
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          inline void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const;

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const=delete;

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          inline void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                           const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const;

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                          const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const=delete;

          doublereal dGetValue(index_type, index_type) const = delete;

          index_type iGetSize(index_type i, index_type j) const = delete;

          constexpr index_type iGetNumRows() const {
               return u.iGetNumRows();
          }

          constexpr index_type iGetNumCols() const {
               return v.iGetNumCols();
          }

          constexpr index_type iGetMaxSize() const = delete;

          template <typename ValueTypeB>
          void InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type i, index_type j) const = delete;

          void GetDofStat(SpGradDofStat& s, index_type i, index_type j) const = delete;

          void InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const = delete;

          template <typename ValueTypeB>
          void AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const = delete;

#ifdef SP_GRAD_DEBUG
          template <typename ExprType, typename Expr>
          constexpr bool bHaveRefTo(const SpMatElemExprBase<ExprType, Expr>& A) const {
               return u.bHaveRefTo(A) || v.bHaveRefTo(A);
          }
#endif
          const ValueType* begin() const = delete;
          const ValueType* end() const = delete;
          inline constexpr index_type iGetRowOffset() const = delete;
          inline constexpr index_type iGetColOffset() const = delete;

     private:
          const LhsExpr u;
          const RhsExpr v;
     };

     namespace util {
          template <typename Expr>
          struct ScalarExprEvalFlagsHelper {
               static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = Expr::eExprEvalFlags;
          };

          template <>
          struct ScalarExprEvalFlagsHelper<doublereal> {
               static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = SpGradCommon::ExprEvalDuplicate;
          };

          template <>
          struct ScalarExprEvalFlagsHelper<GpGradProd> {
               static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = SpGradCommon::ExprEvalDuplicate;
          };
     }

     template <typename ValueType, typename ScalarExpr, index_type NumRows, index_type NumCols>
     class SpMatElemScalarExpr: public SpMatElemExprBase<ValueType, SpMatElemScalarExpr<ValueType, ScalarExpr, NumRows, NumCols> > {
          enum OpCntEstimate: index_type {
               EST_NUMBER_OF_SCALAR_OPS = 1
          };
          typedef typename util::remove_all<ScalarExpr>::type ScalarExprType;
     public:
          static constexpr index_type iNumElemOps = EST_NUMBER_OF_SCALAR_OPS;
          static constexpr unsigned uMatAccess = util::MatAccessFlag::ELEMENT_WISE;
          static constexpr index_type iNumRowsStatic = NumRows;
          static constexpr index_type iNumColsStatic = NumCols;
          static constexpr SpMatOpType eMatOpType = SpMatOpType::SCALAR;
          static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = util::ScalarExprEvalFlagsHelper<typename util::remove_all<ScalarExpr>::type>::eExprEvalFlags;

          inline constexpr explicit SpMatElemScalarExpr(const ScalarExpr& u) noexcept;

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemEval<eTransp, eCompr>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                    const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemEval<eTransp, eCompr>(A, oDofMap);
          }

          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          inline void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                                 const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
               this->template ElemAssign<eTransp, eCompr, Func>(A, oDofMap);
          }

          inline constexpr doublereal dGetValue(index_type, index_type) const noexcept;

          inline constexpr index_type iGetSize(index_type, index_type) const noexcept;

          inline static constexpr index_type iGetNumRows() noexcept;

          inline static constexpr index_type iGetNumCols() noexcept;

          inline constexpr index_type iGetMaxSize() const noexcept;

          template <typename ValueTypeB>
          inline void InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type, index_type) const;

          inline void GetDofStat(SpGradDofStat& s, index_type, index_type) const noexcept;

          inline void InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const;

          template <typename ValueTypeB>
          inline void AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const;

#ifdef SP_GRAD_DEBUG
          template <typename ExprTypeB, typename ExprB>
          inline constexpr bool bHaveRefTo(const SpMatElemExprBase<ExprTypeB, ExprB>& A) const noexcept;
#endif
          const ValueType* begin() const = delete;
          const ValueType* end() const = delete;
          inline constexpr index_type iGetRowOffset() const = delete;
          inline constexpr index_type iGetColOffset() const = delete;
     private:
          const ScalarExpr u;
     };

     template <typename ValueType, index_type NumRows, index_type NumCols>
     class SpMatrixBase: public SpMatElemExprBase<ValueType, SpMatrixBase<ValueType, NumRows, NumCols> > {
          static_assert(NumRows >= 1 || NumRows == SpMatrixSize::DYNAMIC, "invalid number of rows");
          static_assert(NumCols >= 1 || NumCols == SpMatrixSize::DYNAMIC, "invalid number of columns");
          friend class SpMatrixBase<const ValueType, NumRows, NumCols>;
     public:
          static constexpr index_type iNumElemOps = 0;
          static constexpr unsigned uMatAccess = util::MatAccessFlag::ELEMENT_WISE |
               util::MatAccessFlag::ITERATORS |
               util::MatAccessFlag::MATRIX_WISE;
          static constexpr index_type iNumRowsStatic = NumRows;
          static constexpr index_type iNumColsStatic = NumCols;
          static constexpr SpMatOpType eMatOpType = SpMatOpType::MATRIX;
          static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = SpGradCommon::ExprEvalDuplicate;

          inline SpMatrixBase();
          inline SpMatrixBase(const SpMatrixBase& oMat);
          inline SpMatrixBase(SpMatrixBase&& oMat);
          inline SpMatrixBase(index_type iNumRows, index_type iNumCols, index_type iNumDeriv);
          template <typename ValueTypeExpr, typename Expr>
          inline SpMatrixBase(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr);
          template <typename ValueTypeExpr, typename Expr>
          inline SpMatrixBase& MapAssign(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr,
                                         const SpGradExpDofMapHelper<ValueType>& oDofMap);
          template <typename ValueTypeExpr, typename Expr>
          inline SpMatrixBase(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr,
                              const SpGradExpDofMapHelper<ValueType>& oDofMap);
          inline SpMatrixBase(index_type iNumRows, index_type iNumCols, const SpGradExpDofMapHelper<ValueType>& oDofMap);
          inline SpMatrixBase(const std::initializer_list<ValueType>& rgValues);
          inline ~SpMatrixBase();
          inline void ResizeReset(index_type iNumRows, index_type iNumCols, index_type iNumDeriv);
          inline void ResizeReset(index_type iNumRows, index_type iNumCols, const SpGradExpDofMapHelper<ValueType>& oDofMap);
          inline SpMatrixBase& operator=(SpMatrixBase&& oMat);
          template <typename ValueTypeExpr, typename Expr>
          inline SpMatrixBase& operator=(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr);
          inline SpMatrixBase& operator=(const SpMatrixBase& oMat);
          template <typename Expr>
          inline SpMatrixBase& operator*=(const SpGradBase<Expr>& b);
          inline SpMatrixBase& operator*=(const SpGradient& b);
          inline SpMatrixBase& operator*=(const GpGradProd& b);
          inline SpMatrixBase& operator*=(doublereal b);
          template <typename Expr>
          inline SpMatrixBase& operator/=(const SpGradBase<Expr>& b);
          inline SpMatrixBase& operator/=(const SpGradient& b);
          inline SpMatrixBase& operator/=(const GpGradProd& b);
          inline SpMatrixBase& operator/=(const doublereal b);
          template <typename ValueTypeExpr, typename Expr>
          inline SpMatrixBase& operator+=(const SpMatElemExprBase<ValueTypeExpr, Expr>& b);
          template <typename ValueTypeExpr, typename Expr>
          inline SpMatrixBase& operator-=(const SpMatElemExprBase<ValueTypeExpr, Expr>& b);
          template <typename ValueTypeExpr, typename Expr>
          inline SpMatrixBase& Add(const SpMatElemExprBase<ValueTypeExpr, Expr>& b, const SpGradExpDofMapHelper<ValueType>& oDofMap);
          template <typename ValueTypeExpr, typename Expr>
          inline SpMatrixBase& Sub(const SpMatElemExprBase<ValueTypeExpr, Expr>& b, const SpGradExpDofMapHelper<ValueType>& oDofMap);
          inline index_type iGetNumRows() const;
          inline index_type iGetNumCols() const;
          inline index_type iGetSize(index_type i, index_type j) const;
          inline doublereal dGetValue(index_type i, index_type j) const;
          template <typename ValueType_B>
          inline void InsertDeriv(ValueType_B& g, doublereal dCoef, index_type i, index_type j) const;
          inline void GetDofStat(SpGradDofStat& s, index_type i, index_type j) const;
          inline void InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const;
          template <typename ValueTypeB>
          inline void AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const;
#ifdef SP_GRAD_DEBUG
          template <typename ExprType, typename Expr>
          inline constexpr bool bHaveRefTo(const SpMatElemExprBase<ExprType, Expr>& A) const noexcept;
#endif
          inline const ValueType& GetElem(index_type i) const;
          inline ValueType& GetElem(index_type i);
          inline const ValueType& GetElem(index_type i, index_type j) const;
          inline ValueType& GetElem(index_type i, index_type j);
          inline ValueType* begin();
          inline ValueType* end();
          inline const ValueType* begin() const;
          inline const ValueType* end() const;
          static inline constexpr index_type iGetRowOffset() noexcept { return 1; }
          inline constexpr index_type iGetColOffset() const noexcept { return util::MatrixDataSizeHelper<NumRows>::iGetSizeStatic(iGetNumRows()); }
          inline bool bHaveSameRep(const SpMatrixBase& A) const noexcept;
          template <typename ValueTypeB>
          constexpr static inline bool bIsOwnerOf(const SpMatrixData<ValueTypeB>*) noexcept { return false; }
          inline bool bIsOwnerOf(const SpMatrixData<ValueType>* pDataB) const noexcept;
          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          inline void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const;
          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const;
          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
          inline void Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                           const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const;
          template <util::MatTranspEvalFlag eTransp = util::MatTranspEvalFlag::DIRECT,
                    SpGradCommon::ExprEvalFlags eCompr = SpGradCommon::ExprEvalDuplicate,
                    typename Func,
                    typename ValueTypeA,
                    index_type NumRowsA, index_type NumColsA>
          void AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                          const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const;

          inline index_type iGetMaxSize() const;
          inline constexpr SpMatColVecExpr<ValueType, const SpMatrixBase&> GetCol(index_type iCol) const {
               return decltype(GetCol(iCol))(*this, iCol);
          }
          inline constexpr SpMatRowVecExpr<ValueType, const SpMatrixBase&> GetRow(index_type iRow) const {
               return decltype(GetRow(iRow))(*this, iRow);
          }
          SpMatrixBase<doublereal, NumRows, NumCols> GetValue() const {
               SpMatrixBase<doublereal, NumRows, NumCols> A(iGetNumRows(), iGetNumCols(), 0);

               for (index_type i = 1; i <= pData->iGetNumElem(); ++i) {
                    A.GetElem(i) = SpGradientTraits<ValueType>::dGetValue(GetElem(i));
               }

               return A;
          }

          void MakeUnique();

#ifdef SP_GRAD_DEBUG
          bool bValid() const;
          static bool bValid(const SpGradient& g);
          static bool bValid(const GpGradProd& g);
          static bool bValid(doublereal d);
#endif
     private:
          typedef SpMatrixDataHandler<ValueType, iNumRowsStatic, iNumColsStatic> SpMatrixHandlerType;
          typedef typename SpMatrixHandlerType::SpMatrixDataType SpMatrixDataType;

          static inline SpMatrixDataType* pGetNullData();

          static constexpr bool bStaticSize = iNumRowsStatic != SpMatrixSize::DYNAMIC && iNumColsStatic != SpMatrixSize::DYNAMIC;
          SpMatrixHandlerType pData;
     };

     template <typename ValueType, index_type NumRows, index_type NumCols>
     inline std::ostream& operator<<(std::ostream& os, const SpMatrixBase<ValueType, NumRows, NumCols>& A);

     template <typename ValueType,
               index_type NumRows = SpMatrixSize::DYNAMIC,
               index_type NumCols = SpMatrixSize::DYNAMIC>
     class SpMatrix: public SpMatrixBase<ValueType, NumRows, NumCols> {
     public:
          inline SpMatrix() {
               static_assert(NumRows == SpMatrixSize::DYNAMIC || NumCols == SpMatrixSize::DYNAMIC,
                             "Do not forget to allocate memory for this matrix!\n"
                             "It will not be allocated by default.\n"
                             "If you need a preallocated matrix, use SpMatrixA instead!");
          }
          inline SpMatrix(index_type iNumRows, index_type iNumCols, index_type iNumDeriv);
          inline SpMatrix(const SpMatrix& oMat)=default;
          inline SpMatrix(SpMatrix&& oMat)=default;
          inline SpMatrix(SpMatrixBase<ValueType, NumRows, NumCols>&& oMat)
               :SpMatrixBase<ValueType, NumRows, NumCols>(std::move(oMat)) {
          }
          inline SpMatrix(index_type iNumRows, index_type iNumCols, const SpGradExpDofMapHelper<ValueType>& oDofMap)
               :SpMatrixBase<ValueType, NumRows, NumCols>(iNumRows, iNumCols, oDofMap) {
          }
          inline SpMatrix(const std::initializer_list<ValueType>& rgValues);
          template <typename ValueTypeExpr, typename Expr>
          inline SpMatrix(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr);
          template <typename ValueTypeExpr, typename Expr>
          inline SpMatrix(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr,
                          const SpGradExpDofMapHelper<ValueType>& oDofMap);
          inline SpMatrix& operator=(SpMatrix&& oMat)=default;
          inline SpMatrix& operator=(const SpMatrix& oMat)=default;
          inline SpMatrix& operator=(const Mat3x3& oMat);
          template <typename ValueTypeExpr, typename Expr>
          inline SpMatrix& operator=(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr);
          inline ValueType& operator()(index_type iRow, index_type iCol);
          inline const ValueType& operator() (index_type iRow, index_type iCol) const;
     };

     template <typename ValueType,
               index_type NumRows = SpMatrixSize::DYNAMIC>
     class SpColVector: public SpMatrixBase<ValueType, NumRows, 1> {
     public:
          inline SpColVector() {
               static_assert(NumRows == SpMatrixSize::DYNAMIC,
                             "Do not forget to allocate memory for this vector!\n"
                             "It will not be allocated by default.\n"
                             "If you need a preallocated vector, use SpColVectorA instead!");
          }
          inline SpColVector(index_type iNumRows, index_type iNumDeriv);
          inline SpColVector(const SpColVector& oVec)=default;
          inline SpColVector(SpColVector&& oVec)=default;
          inline SpColVector(SpMatrixBase<ValueType, NumRows, 1>&& oMat)
               :SpMatrixBase<ValueType, NumRows, 1>(std::move(oMat)) {
          }
          inline SpColVector(index_type iNumRows, const SpGradExpDofMapHelper<ValueType>& oDofMap)
               :SpMatrixBase<ValueType, NumRows, 1>(iNumRows, 1, oDofMap) {
          }
          template <typename ValueTypeExpr, typename Expr>
          inline SpColVector(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr);
          template <typename ValueTypeExpr, typename Expr>
          inline SpColVector(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr,
                             const SpGradExpDofMapHelper<ValueType>& oDofMap);
          inline SpColVector(const std::initializer_list<ValueType>& rgValues);
          inline void ResizeReset(index_type iNumRows, index_type iNumDeriv);
          inline void ResizeReset(index_type iNumRows, const SpGradExpDofMapHelper<ValueType>& oDofMap);
          inline SpColVector& operator=(const SpColVector& oVec)=default;
          inline SpColVector& operator=(SpColVector&& oVec)=default;
          inline SpColVector& operator=(const Vec3& oVec);
          template <typename ValueTypeExpr, typename Expr>
          inline SpColVector& operator=(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr);
          inline ValueType& operator()(index_type iRow);
          inline const ValueType& operator() (index_type iRow) const;
     };

     template <typename ValueType,
               index_type NumCols = SpMatrixSize::DYNAMIC>
     class SpRowVector: public SpMatrixBase<ValueType, 1, NumCols> {
     public:
          inline SpRowVector() {
               static_assert(NumCols == SpMatrixSize::DYNAMIC,
                             "Do not forget to allocate memory for this vector!\n"
                             "It will not be allocated by default.\n"
                             "If you need a preallocated vector, use SpRowVectorA instead!");
          }
          inline SpRowVector(index_type iNumCols, index_type iNumDeriv);
          inline SpRowVector(const SpRowVector& oVec)=default;
          inline SpRowVector(SpRowVector&& oVec)=default;
          inline SpRowVector(SpMatrixBase<ValueType, 1, NumCols>&& oMat)
               :SpMatrixBase<ValueType, 1, NumCols>(std::move(oMat)) {
          }
          inline SpRowVector(index_type iNumCols, const SpGradExpDofMapHelper<ValueType>& oDofMap)
               :SpMatrixBase<ValueType, 1, NumCols>(1, iNumCols, oDofMap) {
          }
          template <typename ValueTypeExpr, typename Expr>
          inline SpRowVector(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr);
          template <typename ValueTypeExpr, typename Expr>
          inline SpRowVector(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr,
                             const SpGradExpDofMapHelper<ValueType>& oDofMap);
          inline SpRowVector(const std::initializer_list<ValueType>& rgValues);
          inline void ResizeReset(index_type iNumCols, index_type iNumDeriv);
          inline void ResizeReset(index_type iNumCols, const SpGradExpDofMapHelper<ValueType>& oDofMap);
          inline SpRowVector& operator=(const SpRowVector& oVec)=default;
          inline SpRowVector& operator=(SpRowVector&& oVec)=default;
          template <typename ValueTypeExpr, typename Expr>
          inline SpRowVector& operator=(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr);
          inline ValueType& operator()(index_type iCol);
          inline const ValueType& operator() (index_type iCol) const;
     };

     template <typename ValueType,
               index_type NumRows = SpMatrixSize::DYNAMIC,
               index_type NumCols = SpMatrixSize::DYNAMIC,
               index_type NumDer = 0>
     class SpMatrixA: public SpMatrix<ValueType, NumRows, NumCols> {
     public:
          inline SpMatrixA()
               :SpMatrix<ValueType, NumRows, NumCols>(NumRows, NumCols, NumDer) {
               static_assert(NumRows != SpMatrixSize::DYNAMIC, "matrix size cannot be dynamic for SpMatrixA; use SpMatrix instead");
               static_assert(NumCols != SpMatrixSize::DYNAMIC, "matrix size cannot be dynamic for SpMatrixA; use SpMatrix instead");
               static_assert(NumDer != SpMatrixSize::DYNAMIC, "matrix size cannot be dynamic for SpMatrixA; use SpMatrix instead");
          }

          inline explicit SpMatrixA(index_type iNumDeriv)
               :SpMatrix<ValueType, NumRows, NumCols>(NumRows, NumCols, iNumDeriv) {
          }

          inline SpMatrixA(const SpMatrixA& oMat)=default;
          inline SpMatrixA(SpMatrixA&& oMat)=default;
          inline SpMatrixA(const std::initializer_list<ValueType>& rgValues)
               :SpMatrix<ValueType, NumRows, NumCols>(rgValues) {
          }

          template <typename ValueTypeExpr, typename Expr>
          inline SpMatrixA(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr)
               :SpMatrix<ValueType, NumRows, NumCols>(oExpr) {
          }

          template <typename ValueTypeExpr, typename Expr>
          inline SpMatrixA(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr, const SpGradExpDofMapHelper<ValueType>& oDofMap)
               :SpMatrix<ValueType, NumRows, NumCols>(oExpr, oDofMap) {
          }

          inline SpMatrixA(SpMatrixBase<ValueType, NumRows, NumCols>&& oMat)
               :SpMatrix<ValueType, NumRows, NumCols>(std::move(oMat)) {
          }

          inline void ResizeReset(index_type iNumDer) {
               SpMatrix<ValueType, NumRows, NumCols>::ResizeReset(NumRows, NumCols, iNumDer);
          }

          inline void ResizeReset(const SpGradExpDofMapHelper<ValueType>& oDofMap) {
               SpMatrix<ValueType, NumRows, NumCols>::ResizeReset(NumRows, NumCols, oDofMap);
          }

          inline SpMatrixA& operator=(SpMatrixA&& oMat)=default;
          inline SpMatrixA& operator=(const SpMatrixA& oMat)=default;
          inline SpMatrixA& operator=(const Mat3x3& oMat) {
               SpMatrix<ValueType, NumRows, NumCols>::operator=(oMat);
               return *this;
          }
          template <typename ValueTypeExpr, typename Expr>
          inline SpMatrixA& operator=(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr) {
               SpMatrix<ValueType, NumRows, NumCols>::operator=(oExpr);
               return *this;
          }
     };

     template <typename ValueType,
               index_type NumRows = SpMatrixSize::DYNAMIC,
               index_type NumDer = 0>
     class SpColVectorA: public SpColVector<ValueType, NumRows> {
     public:
          inline SpColVectorA()
               :SpColVector<ValueType, NumRows>(NumRows, NumDer) {
               static_assert(NumRows != SpMatrixSize::DYNAMIC, "matrix size cannot be dynamic for SpColVectorA; use SpColVector instead");
               static_assert(NumDer != SpMatrixSize::DYNAMIC, "matrix size cannot be dynamic for SpColVectorA; use SpColVector instead");
          }

          inline explicit SpColVectorA(index_type iNumDeriv)
               :SpColVector<ValueType, NumRows>(NumRows, iNumDeriv) {
          }

          inline SpColVectorA(const SpColVectorA&)=default;
          inline SpColVectorA(SpColVectorA&&)=default;
          inline SpColVectorA(const std::initializer_list<ValueType>& rgValues)
               :SpColVector<ValueType, NumRows>(rgValues) {
          }
          template <typename ValueTypeExpr, typename Expr>
          inline SpColVectorA(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr)
               :SpColVector<ValueType, NumRows>(oExpr) {
          }

          template <typename ValueTypeExpr, typename Expr>
          inline SpColVectorA(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr, const SpGradExpDofMapHelper<ValueType>& oDofMap)
               :SpColVector<ValueType, NumRows>(oExpr, oDofMap) {
          }

          inline SpColVectorA(SpMatrixBase<ValueType, NumRows, 1>&& oMat)
               :SpColVector<ValueType, NumRows>(std::move(oMat)) {
          }

          inline void ResizeReset(index_type iNumDeriv) {
               SpColVector<ValueType, NumRows>::ResizeReset(NumRows, iNumDeriv);
          }

          inline void ResizeReset(const SpGradExpDofMapHelper<ValueType>& oDofMap) {
               SpColVector<ValueType, NumRows>::ResizeReset(NumRows, oDofMap);
          }

          inline SpColVectorA& operator=(SpColVectorA&& oMat)=default;
          inline SpColVectorA& operator=(const SpColVectorA& oMat)=default;
          inline SpColVectorA& operator=(const Mat3x3& oMat) {
               SpColVector<ValueType, NumRows>::operator=(oMat);
               return *this;
          }
          template <typename ValueTypeExpr, typename Expr>
          inline SpColVectorA& operator=(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr) {
               SpColVector<ValueType, NumRows>::operator=(oExpr);
               return *this;
          }
     };

     template <typename ValueType,
               index_type NumCols = SpMatrixSize::DYNAMIC,
               index_type NumDer = 0>
     class SpRowVectorA: public SpRowVector<ValueType, NumCols> {
     public:
          inline SpRowVectorA()
               :SpRowVector<ValueType, NumCols>(NumCols, NumDer) {
               static_assert(NumCols != SpMatrixSize::DYNAMIC, "matrix size cannot be dynamic for SpRowVectorA; use SpRowVector instead");
               static_assert(NumDer != SpMatrixSize::DYNAMIC, "matrix size cannot be dynamic for SpRowVectorA; use SpRowVector instead");
          }

          inline explicit SpRowVectorA(index_type iNumDeriv)
               :SpRowVector<ValueType, NumCols>(NumCols, iNumDeriv) {
          }

          inline SpRowVectorA(const SpRowVectorA&)=default;
          inline SpRowVectorA(SpRowVectorA&&)=default;

          inline SpRowVectorA(const std::initializer_list<ValueType>& rgValues)
               :SpRowVector<ValueType, NumCols>(rgValues) {
          }

          template <typename ValueTypeExpr, typename Expr>
          inline SpRowVectorA(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr)
               :SpRowVector<ValueType, NumCols>(oExpr) {
          }

          template <typename ValueTypeExpr, typename Expr>
          inline SpRowVectorA(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr, const SpGradExpDofMapHelper<ValueType>& oDofMap)
               :SpRowVector<ValueType, NumCols>(oExpr, oDofMap) {
          }

          inline SpRowVectorA(SpMatrixBase<ValueType, 1, NumCols>&& oMat)
               :SpRowVector<ValueType, NumCols>(std::move(oMat)) {
          }

          inline void ResizeReset(index_type iNumDeriv) {
               SpRowVector<ValueType, NumCols>::ResizeReset(NumCols, iNumDeriv);
          }

          inline void ResizeReset(const SpGradExpDofMapHelper<ValueType>& oDofMap) {
               SpRowVector<ValueType, NumCols>::ResizeReset(NumCols, oDofMap);
          }

          inline SpRowVectorA& operator=(SpRowVectorA&& oMat)=default;
          inline SpRowVectorA& operator=(const SpRowVectorA& oMat)=default;

          inline SpRowVectorA& operator=(const Mat3x3& oMat) {
               SpRowVector<ValueType, NumCols>::operator=(oMat);
               return *this;
          }

          template <typename ValueTypeExpr, typename Expr>
          inline SpRowVectorA& operator=(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr) {
               SpRowVector<ValueType, NumCols>::operator=(oExpr);
               return *this;
          }
     };

     template <typename ValueType>
     index_type SpMatrixData<ValueType>::iGetNumRows() const noexcept {
          SP_GRAD_ASSERT(iNumRows >= 0);
          SP_GRAD_ASSERT(iNumCols >= 0);
          return iNumRows;
     }

     template <typename ValueType>
     index_type SpMatrixData<ValueType>::iGetNumCols() const noexcept {
          SP_GRAD_ASSERT(iNumRows >= 0);
          SP_GRAD_ASSERT(iNumCols >= 0);

          return iNumCols;
     }

     template <typename ValueType>
     index_type SpMatrixData<ValueType>::iGetNumElem() const noexcept {
          SP_GRAD_ASSERT(iNumRows >= 0);
          SP_GRAD_ASSERT(iNumCols >= 0);

          return iNumRows * iNumCols;
     }

     template <typename ValueType>
     index_type SpMatrixData<ValueType>::iGetRefCnt() const noexcept {
          SP_GRAD_ASSERT(iRefCnt >= 0);

          return iRefCnt;
     }

     template <typename ValueType>
     index_type SpMatrixData<ValueType>::iGetMaxDeriv() const noexcept {
          SP_GRAD_ASSERT(iNumDeriv >= 0);

          return iNumDeriv;
     }

     template <typename ValueType>
     bool SpMatrixData<ValueType>::bCheckSize(index_type iNumRowsReq, index_type iNumColsReq, index_type iNumDerivReq) const noexcept {
          return iNumRowsReq == iNumRows && iNumColsReq == iNumCols && iNumDerivReq <= iNumDeriv && iRefCnt <= 1;
     }

     template <typename ValueType>
     SpMatrixData<ValueType>* SpMatrixData<ValueType>::pGetNullData() noexcept {
          SP_GRAD_ASSERT(oNullData.pNullData != nullptr);
          SP_GRAD_ASSERT(oNullData.pNullData->iRefCnt >= 0);
          SP_GRAD_ASSERT(oNullData.pNullData->iNumRows == 0);
          SP_GRAD_ASSERT(oNullData.pNullData->iNumCols == 0);

          return oNullData.pNullData;
     }

     template <typename ValueType>
     SpMatrixData<ValueType>::NullData::NullData()
          :pNullData{new SpNullData<ValueType>} {
     }

     template <typename ValueType>
     SpMatrixData<ValueType>::NullData::~NullData()
     {
          SP_GRAD_ASSERT(pNullData != nullptr);
          SP_GRAD_ASSERT(pNullData->iRefCnt == 1);
          SP_GRAD_ASSERT(pNullData->iNumRows == 0);
          SP_GRAD_ASSERT(pNullData->iNumCols == 0);
          SP_GRAD_ASSERT(pNullData->iNumDeriv == 0);

          pNullData->DecRef();

          pNullData = nullptr;
     }

     template <typename ValueType>
     typename SpMatrixData<ValueType>::NullData SpMatrixData<ValueType>::oNullData;

     template <typename ValueType>
     SpNullData<ValueType>::SpNullData() noexcept
     :SpMatrixData<ValueType>(&SpNullData::Cleanup, 0, 0, 1, 0, nullptr) {
     }

     template <typename ValueType>
     SpNullData<ValueType>::~SpNullData() noexcept {
          SP_GRAD_ASSERT(this->iRefCnt == 0);
     }

     template <typename ValueType>
     void SpNullData<ValueType>::Cleanup(SpMatrixData<ValueType>* p) {
          auto pData = static_cast<SpNullData*>(p);

          SP_GRAD_ASSERT(pData->pfnCleanup == &SpNullData::Cleanup);
          SP_GRAD_ASSERT(pData->iRefCnt == 0);

          delete pData;
     }

     namespace util {
          template <typename ValueType>
          void SpMatrixDataTraits<ValueType>::Construct(SpMatrixData<ValueType>& oData, index_type iNumDeriv, void* pExtraMem) {
               const index_type iNumElem = oData.iGetNumElem();

               for (index_type i = 1; i <= iNumElem; ++i) {
                    new(oData.pGetData(i)) ValueType{}; // Enforce default constructor also for doublereal!
               }
          }

          template <typename ValueType>
          size_t constexpr SpMatrixDataTraits<ValueType>::uOffsetDeriv(size_t uSizeGrad) noexcept {
               return 0;
          }

          template <typename ValueType>
          constexpr size_t SpMatrixDataTraits<ValueType>::uSizeDeriv(index_type iNumDeriv, index_type iNumItems) noexcept {
               return 0;
          }

          template <typename ValueType>
          constexpr size_t SpMatrixDataTraits<ValueType>::uAlign() noexcept {
               return 0;
          }

          void SpMatrixDataTraits<SpGradient>::Construct(SpMatrixData<SpGradient>& oData, index_type iNumDeriv, void* pExtraMem) {
               static_assert(alignof(SpDerivData) == alignof(SpDerivRec), "alignment does not match");

               SP_GRAD_ASSERT(reinterpret_cast<size_t>(pExtraMem) % alignof(SpDerivData) == 0);

               const size_t uBytesDeriv = sizeof(SpDerivData) + iNumDeriv * sizeof(SpDerivRec);
               const index_type iNumElem = oData.iGetNumElem();

               for (index_type i = 1; i <= iNumElem; ++i) {
                    size_t uBytesOffset = (i - 1) * uBytesDeriv;
                    auto pDerivData = reinterpret_cast<SpDerivData*>(reinterpret_cast<char*>(pExtraMem) + uBytesOffset);

                    SP_GRAD_ASSERT(reinterpret_cast<size_t>(pDerivData) % alignof(SpDerivData) == 0);

                    new(pDerivData) SpDerivData(0., iNumDeriv, 0, true, 0, &oData);
                    new(oData.pGetData(i)) SpGradient(pDerivData);
               }
          }

          constexpr size_t SpMatrixDataTraits<SpGradient>::uOffsetDeriv(size_t uSizeGrad) noexcept {
               return uSizeGrad % alignof(SpDerivData)
                    ? alignof(SpDerivData) - uSizeGrad % alignof(SpDerivData)
                    : 0;
          }

          constexpr size_t SpMatrixDataTraits<SpGradient>::uSizeDeriv(index_type iNumDeriv, index_type iNumItems) noexcept {
               return (sizeof(SpDerivData) + sizeof(SpDerivRec) * iNumDeriv) * iNumItems;
          }

          constexpr size_t SpMatrixDataTraits<SpGradient>::uAlign() noexcept {
               return alignof(SpDerivData);
          }
     }

     template <typename ValueType>
     SpMatrixData<ValueType>::SpMatrixData(void (*pfnCleanup)(SpMatrixData*),
                                           index_type iNumRows,
                                           index_type iNumCols,
                                           index_type iRefCnt,
                                           index_type iNumDeriv,
                                           void* pExtraMem)
          :pfnCleanup(pfnCleanup),
           iNumRows(iNumRows),
           iNumCols(iNumCols),
           iRefCnt(iRefCnt),
           iNumDeriv(iNumDeriv) {
     }

     template <typename ValueType>
     SpMatrixData<ValueType>::~SpMatrixData() {
          SP_GRAD_ASSERT(iRefCnt == 0);
     }

     template <typename ValueType>
     ValueType* SpMatrixData<ValueType>::pGetData() noexcept {
          return this->rgData;
     }

     template <typename ValueType>
     ValueType* SpMatrixData<ValueType>::pGetData(index_type iRow) noexcept {
          SP_GRAD_ASSERT(iRow >= 1);
          SP_GRAD_ASSERT(iRow <= this->iNumRows * this->iNumCols);

          return pGetData() + iRow - 1;
     }

     template <typename ValueType>
     const ValueType* SpMatrixData<ValueType>::pGetData() const noexcept {
          return this->rgData;
     }

     template <typename ValueType>
     const ValueType* SpMatrixData<ValueType>::pGetData(index_type iRow) const noexcept {
          SP_GRAD_ASSERT(iRow >= 1);
          SP_GRAD_ASSERT(iRow <= this->iNumRows * this->iNumCols);

          return pGetData() + iRow - 1;
     }

     template <typename ValueType>
     void SpMatrixData<ValueType>::IncRef() noexcept {
          SP_GRAD_ASSERT(this->iRefCnt >= 0);

          ++this->iRefCnt;
     }

     template <typename ValueType>
     void SpMatrixData<ValueType>::DecRef() {
          SP_GRAD_ASSERT(iRefCnt >= 1);

          if (--iRefCnt == 0) {
               (*pfnCleanup)(this);
          }
     }

     template <typename ValueType>
     void SpMatrixData<ValueType>::Attach(ValueType* pData) noexcept {
          if (!bIsOwnerOf(pData)) {
               IncRef();
          }
     }

     template <typename ValueType>
     void SpMatrixData<ValueType>::Detach(ValueType* pData) {
          if (!bIsOwnerOf(pData)) {
               DecRef();
          }
     }

     template <typename ValueType>
     ValueType* SpMatrixData<ValueType>::begin() noexcept {
          return pGetData();
     }

     template <typename ValueType>
     ValueType* SpMatrixData<ValueType>::end() noexcept {
          return pGetData() + this->iGetNumElem();
     }

     template <typename ValueType>
     const ValueType* SpMatrixData<ValueType>::begin() const noexcept {
          return pGetData();
     }

     template <typename ValueType>
     const ValueType* SpMatrixData<ValueType>::end() const noexcept {
          return pGetData() + this->iGetNumElem();
     }

     template <typename ValueType>
     constexpr bool SpMatrixData<ValueType>::bIsOwnerOf(const ValueType* pData) const noexcept {
          return pData >= begin() && pData < end();
     }

     template <typename ValueType>
     SpMatrixDataDynamic<ValueType>::SpMatrixDataDynamic(index_type iNumRows,
                                                         index_type iNumCols,
                                                         index_type iRefCnt,
                                                         index_type iNumDeriv,
                                                         void* pExtraMem)
          :SpMatrixData<ValueType>(&SpMatrixDataDynamic::Cleanup, iNumRows, iNumCols, iRefCnt, iNumDeriv, pExtraMem) {
          util::SpMatrixDataTraits<ValueType>::Construct(*this, iNumDeriv, pExtraMem);
     }

     template <typename ValueType>
     SpMatrixDataDynamic<ValueType>::~SpMatrixDataDynamic()
     {
          SP_GRAD_ASSERT(this->iRefCnt == 0);

          for (auto& g: *this) {
               g.~ValueType();
          }
     }

     template <typename ValueType>
     void SpMatrixDataDynamic<ValueType>::Cleanup(SpMatrixData<ValueType>* p) {
          auto pData = static_cast<SpMatrixDataDynamic*>(p);

          SP_GRAD_ASSERT(pData->pfnCleanup == &SpMatrixDataDynamic::Cleanup);
          SP_GRAD_ASSERT(pData != pData->pGetNullData());

          pData->~SpMatrixDataDynamic();

#if defined(HAVE_ALIGNED_MALLOC)
          _aligned_free(pData);
#else
          free(pData);
#endif
     }

     template <typename ValueType>
     SpMatrixDataDynamic<ValueType>* SpMatrixDataDynamic<ValueType>::pGetNullData() noexcept {
          return static_cast<SpMatrixDataDynamic<ValueType>*>(SpMatrixData<ValueType>::pGetNullData());
     }

     template <typename ValueType>
     const ValueType* SpMatrixDataDynamic<ValueType>::pGetData(index_type iRow, index_type iCol) const noexcept {
          SP_GRAD_ASSERT(iRow >= 1);
          SP_GRAD_ASSERT(iRow <= this->iGetNumRows());
          SP_GRAD_ASSERT(iCol >= 1);
          SP_GRAD_ASSERT(iCol <= this->iGetNumCols());

          return this->pGetData() + (iCol - 1) * this->iGetNumRows() + iRow - 1;
     }

     template <typename ValueType>
     ValueType* SpMatrixDataDynamic<ValueType>::pGetData(index_type iRow, index_type iCol) noexcept {
          SP_GRAD_ASSERT(iRow >= 1);
          SP_GRAD_ASSERT(iRow <= this->iGetNumRows());
          SP_GRAD_ASSERT(iCol >= 1);
          SP_GRAD_ASSERT(iCol <= this->iGetNumCols());

          return this->pGetData() + (iCol - 1) * this->iGetNumRows() + iRow - 1;
     }

     template <typename ValueType>
     SpMatrixDataDynamic<ValueType>*
     SpMatrixDataDynamic<ValueType>::pAllocate(index_type iNumRows, index_type iNumCols, index_type iNumDeriv) {
          const index_type iNumItems = iNumRows * iNumCols;
          const size_t uSizeGrad = sizeof(SpMatrixDataDynamic) + sizeof(ValueType) * iNumItems;
          const size_t uOffsetDeriv = util::SpMatrixDataTraits<ValueType>::uOffsetDeriv(uSizeGrad);
          const size_t uSizeDeriv = util::SpMatrixDataTraits<ValueType>::uSizeDeriv(iNumDeriv, iNumItems);
          const size_t uSize = uSizeGrad + uOffsetDeriv + uSizeDeriv;
          constexpr size_t uAlignVal = alignof(ValueType) > alignof(SpMatrixDataDynamic)
               ? alignof(ValueType) : alignof(SpMatrixDataDynamic);
          constexpr size_t uAlignDer = util::SpMatrixDataTraits<ValueType>::uAlign();
          constexpr size_t uAlign = uAlignVal > uAlignDer ? uAlignVal : uAlignDer;

#if defined(HAVE_POSIX_MEMALIGN)
          char* pMem;
          if (0 != posix_memalign(reinterpret_cast<void**>(&pMem), uAlign, uSize)) {
               pMem = nullptr;
          }
#elif defined(HAVE_MEMALIGN)
          char* pMem = reinterpret_cast<char*>(memalign(uAlign, uSize));
#elif defined(HAVE_ALIGNED_MALLOC)
          char* pMem = reinterpret_cast<char*>(_aligned_malloc(uSize, uAlign));
#elif defined(HAVE_ALIGNED_ALLOC)
          char* pMem = reinterpret_cast<char*>(aligned_alloc(uAlign, uSize));
#else
          char* pMem = reinterpret_cast<char*>(malloc(uSize));
#endif

          if (!pMem) {
               throw std::bad_alloc();
          }

          void* pExtraMem = pMem + uSizeGrad + uOffsetDeriv;

          return new(pMem) SpMatrixDataDynamic(iNumRows, iNumCols, 0, iNumDeriv, pExtraMem);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixDataStatic<ValueType, NumRows, NumCols>::SpMatrixDataStatic()
          :SpMatrixData<ValueType>(&SpMatrixDataStatic::Cleanup, NumRows, NumCols, 0, 0, nullptr)
     {
          constexpr index_type iNumItems = NumRows * NumCols;

          static_assert(NumRows > 0, "invalid number of rows");
          static_assert(NumCols > 0, "invalid number of columns");
          static_assert(iNumItems > 0, "invalid matrix size");

          for (index_type i = 0; i < iNumItems; ++i) {
               SpGradientTraits<ValueType>::ZeroInit(rgDataExt[i]);
          }
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixDataStatic<ValueType, NumRows, NumCols>::~SpMatrixDataStatic()
     {
          SP_GRAD_ASSERT(this->iGetRefCnt() == 0);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     index_type SpMatrixDataStatic<ValueType, NumRows, NumCols>::iGetNumRows() const noexcept {
          ASSERT(this->iNumRows == NumRows);

          return NumRows;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     index_type SpMatrixDataStatic<ValueType, NumRows, NumCols>::iGetNumCols() const noexcept {
          ASSERT(this->iNumCols == NumCols);

          return NumCols;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     inline SpMatrixDataStatic<ValueType, NumRows, NumCols>&
     SpMatrixDataStatic<ValueType, NumRows, NumCols>::operator=(SpMatrixDataStatic&& oRhs) {
          SP_GRAD_ASSERT(&oRhs != pGetNullData());

          SP_GRAD_ASSERT(this->iNumRows == NumRows);
          SP_GRAD_ASSERT(this->iNumCols == NumCols);
          SP_GRAD_ASSERT(this->iNumDeriv == 0);
          SP_GRAD_ASSERT(oRhs.iNumRows == NumRows);
          SP_GRAD_ASSERT(oRhs.iNumCols == NumCols);
          SP_GRAD_ASSERT(oRhs.iNumDeriv == 0);

          constexpr index_type iNumItems = NumRows * NumCols;

          for (index_type i = 0; i < iNumItems; ++i) {
               rgDataExt[i] = std::move(oRhs.rgDataExt[i]);
          }

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     inline void SpMatrixDataStatic<ValueType, NumRows, NumCols>::Cleanup(SpMatrixData<ValueType>* p) {
#ifdef SP_GRAD_DEBUG
          auto pData = static_cast<SpMatrixDataStatic*>(p);

          SP_GRAD_ASSERT(pData->pfnCleanup == &SpMatrixDataStatic::Cleanup);
          SP_GRAD_ASSERT(pData->iRefCnt == 0);
#endif
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixDataStatic<ValueType, NumRows, NumCols>*
     SpMatrixDataStatic<ValueType, NumRows, NumCols>::pGetNullData() noexcept {
          return static_cast<SpMatrixDataStatic<ValueType, NumRows, NumCols>*>(SpMatrixData<ValueType>::pGetNullData());
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     const ValueType*
     SpMatrixDataStatic<ValueType, NumRows, NumCols>::pGetData(index_type iRow, index_type iCol) const noexcept {
          SP_GRAD_ASSERT(iRow >= 1);
          SP_GRAD_ASSERT(iRow <= NumRows);
          SP_GRAD_ASSERT(iCol >= 1);
          SP_GRAD_ASSERT(iCol <= NumCols);

          return pGetData() + (iCol - 1) * NumRows + iRow - 1;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     ValueType* SpMatrixDataStatic<ValueType, NumRows, NumCols>::pGetData(index_type iRow, index_type iCol) noexcept {
          SP_GRAD_ASSERT(iRow >= 1);
          SP_GRAD_ASSERT(iRow <= NumRows);
          SP_GRAD_ASSERT(iCol >= 1);
          SP_GRAD_ASSERT(iCol <= NumCols);

          return pGetData() + (iCol - 1) * NumRows + iRow - 1;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     inline SpMatrixDataHandler<ValueType, NumRows, NumCols>::SpMatrixDataHandler()
     {
          oData.IncRef();
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     inline SpMatrixDataHandler<ValueType, NumRows, NumCols>::~SpMatrixDataHandler()
     {
          oData.DecRef();
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     inline SpMatrixDataHandler<ValueType, NumRows, NumCols>&
     SpMatrixDataHandler<ValueType, NumRows, NumCols>::operator=(SpMatrixDataHandler<ValueType, NumRows, NumCols>&& oRhs)
     {
          oData = std::move(oRhs.oData);

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     inline void SpMatrixDataHandler<ValueType, NumRows, NumCols>::Allocate(index_type iNumRows, index_type iNumCols, index_type iNumDeriv)
     {
          SP_GRAD_ASSERT(iNumRows == NumRows);
          SP_GRAD_ASSERT(iNumCols == NumCols);

          for (auto& oItem: oData) {
               SpGradientTraits<ValueType>::ResizeReset(oItem, 0., iNumDeriv);
          }
     }

     template <typename ValueType>
     inline SpMatrixDataHandler<ValueType, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC>::SpMatrixDataHandler()
          :pData(SpMatrixDataType::pGetNullData())
     {
          pData->IncRef();
     }

     template <typename ValueType>
     inline SpMatrixDataHandler<ValueType, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC>::~SpMatrixDataHandler()
     {
          ASSERT(pData != nullptr);
          pData->DecRef();
     }

     template <typename ValueType>
     inline SpMatrixDataHandler<ValueType, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC>&
     SpMatrixDataHandler<ValueType, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC>::operator=(SpMatrixDataHandler&& oRhs)
     {
          std::swap(pData, oRhs.pData);

          return *this;
     }

     template <typename ValueType>
     inline void SpMatrixDataHandler<ValueType, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC>::Allocate(index_type iNumRows, index_type iNumCols, index_type iNumDeriv)
     {
          auto pNewData = SpMatrixDataType::pAllocate(iNumRows, iNumCols, iNumDeriv);

          pData->DecRef();

          pData = pNewData;

          pData->IncRef();
     }

     namespace util {
          template <MatTranspEvalFlag eTransp, typename Expr, typename ValueType, index_type NumRows, index_type NumCols>
          void MatEvalHelperCompr<SpGradCommon::ExprEvalDuplicate>::ElemEval(const Expr& oExpr, SpMatrixBase<ValueType, NumRows, NumCols>& A) {
               static_assert(eExprEvalFlags == SpGradCommon::ExprEvalDuplicate, "invalid expression flags");

               oExpr.template ElemEvalUncompr<eTransp>(A);
          }

          template <MatTranspEvalFlag eTransp, typename Expr, typename ValueType, index_type NumRows, index_type NumCols>
          void MatEvalHelperCompr<SpGradCommon::ExprEvalUnique>::ElemEval(const Expr& oExpr, SpMatrixBase<ValueType, NumRows, NumCols>& A) {
               static_assert(eExprEvalFlags == SpGradCommon::ExprEvalUnique, "invalid expression flags");

               oExpr.template ElemEvalCompr<eTransp>(A);
          }

          template <MatTranspEvalFlag eTransp, typename Expr, typename ValueType, index_type NumRows, index_type NumCols>
          void MatEvalHelperCompr<SpGradCommon::ExprEvalDuplicate>::ElemEval(const Expr& oExpr, SpMatrixBase<ValueType, NumRows, NumCols>& A, const SpGradExpDofMapHelper<ValueType>&) {
               static_assert(eExprEvalFlags == SpGradCommon::ExprEvalDuplicate, "invalid expression flags");

               constexpr bool bIsGradProd = std::is_same<ValueType, GpGradProd>::value;
               constexpr bool bIsDouble = std::is_same<ValueType, doublereal>::value;
               constexpr bool bIsSpGradient = std::is_same<ValueType, SpGradient>::value;

               static_assert(bIsGradProd || bIsDouble, "data type not supported");
               static_assert(!bIsSpGradient, "data type not handled by this function"); // For efficiency reasons we should always use compressed evaluation if oDofMap is provided

               oExpr.template ElemEvalUncompr<eTransp>(A);
          }

          template <MatTranspEvalFlag eTransp, typename Expr, typename ValueType, index_type NumRows, index_type NumCols>
          void MatEvalHelperCompr<SpGradCommon::ExprEvalUnique>::ElemEval(const Expr& oExpr, SpMatrixBase<ValueType, NumRows, NumCols>& A, const SpGradExpDofMapHelper<ValueType>& oDofMap) {
               static_assert(eExprEvalFlags == SpGradCommon::ExprEvalUnique, "invalid expression flags");

               oExpr.template ElemEvalCompr<eTransp>(A, oDofMap);
          }

          template <typename ValueType, index_type NumRows, index_type NumCols>
          void MatEvalHelperTransp<MatTranspEvalFlag::DIRECT>::ResizeReset(SpMatrixBase<ValueType, NumRows, NumCols>& A,
                                                                           index_type iNumRows,
                                                                           index_type iNumCols,
                                                                           index_type iMaxSize) {
               A.ResizeReset(iNumRows, iNumCols, iMaxSize);
          }

          template <typename ValueType, index_type NumRows, index_type NumCols>
          ValueType& MatEvalHelperTransp<MatTranspEvalFlag::DIRECT>::GetElem(SpMatrixBase<ValueType, NumRows, NumCols>& A, index_type i, index_type j) {
               return A.GetElem(i, j);
          }

          template <typename ValueType, index_type NumRows, index_type NumCols>
          void MatEvalHelperTransp<MatTranspEvalFlag::TRANSPOSED>::ResizeReset(SpMatrixBase<ValueType, NumRows, NumCols>& A,
                                                                               index_type iNumRows,
                                                                               index_type iNumCols,
                                                                               index_type iMaxSize) {
               A.ResizeReset(iNumCols, iNumRows, iMaxSize);
          }

          template <typename ValueType, index_type NumRows, index_type NumCols>
          ValueType& MatEvalHelperTransp<MatTranspEvalFlag::TRANSPOSED>::GetElem(SpMatrixBase<ValueType, NumRows, NumCols>& A, index_type i, index_type j) {
               return A.GetElem(j, i);
          }

          template <typename ValueTypeA, typename ValueTypeB>
          struct ElemAssignHelper {
               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignCompr(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A, const SpMatElemExprBase<ValueTypeB, ExprB>& B) = delete;

               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignCompr(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A, const SpMatElemExprBase<ValueTypeB, ExprB>& B, SpGradExpDofMapHelper<ValueTypeA>& oDofMap) = delete;

               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignUncompr(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A, const SpMatElemExprBase<ValueTypeB, ExprB>& B) = delete;
          };

          template <>
          struct ElemAssignHelper<doublereal, doublereal> {
               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignCompr(SpMatrixBase<doublereal, NumRowsA, NumColsA>& A, const SpMatElemExprBase<doublereal, ExprB>& B) {
                    typedef typename remove_all<ExprB>::type ExprTypeB;
                    constexpr index_type iNumRowsStatic = ExprTypeB::iNumRowsStatic;
                    constexpr index_type iNumColsStatic = ExprTypeB::iNumColsStatic;

                    const index_type iNumRows = A.iGetNumRows();
                    const index_type iNumCols = A.iGetNumCols();

                    static_assert(NumRowsA == iNumRowsStatic, "Number of rows does not match");
                    static_assert(NumColsA == iNumColsStatic, "Number of columns does not match");

                    SP_GRAD_ASSERT(B.iGetNumRows() == iNumRows || B.iGetNumRows() == SpMatrixSize::DYNAMIC);
                    SP_GRAD_ASSERT(B.iGetNumCols() == iNumCols || B.iGetNumCols() == SpMatrixSize::DYNAMIC);

                    SP_GRAD_ASSERT(!B.bHaveRefTo(A));

                    for (index_type j = 1; j <= iNumCols; ++j) {
                         for (index_type i = 1; i <= iNumRows; ++i) {
                              doublereal& Aij = A.GetElem(i, j);
                              Aij = Func::f(Aij, B.dGetValue(i, j));
                         }
                    }
               }

               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignUncompr(SpMatrixBase<doublereal, NumRowsA, NumColsA>& A, const SpMatElemExprBase<doublereal, ExprB>& B) {
                    ElemAssignCompr<eTransp, Func>(A, B);
               }

               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignCompr(SpMatrixBase<doublereal, NumRowsA, NumColsA>& A, const SpMatElemExprBase<doublereal, ExprB>& B, const SpGradExpDofMapHelper<doublereal>&) {
                    ElemAssignCompr<eTransp, Func>(A, B);
               }
          };


          template <>
          struct ElemAssignHelper<SpGradient, doublereal> {
               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignCompr(SpMatrixBase<SpGradient, NumRowsA, NumColsA>& A, const SpMatElemExprBase<doublereal, ExprB>& B) {
                    typedef typename remove_all<ExprB>::type ExprTypeB;
                    constexpr index_type iNumRowsStatic = ExprTypeB::iNumRowsStatic;
                    constexpr index_type iNumColsStatic = ExprTypeB::iNumColsStatic;

                    const index_type iNumRows = A.iGetNumRows();
                    const index_type iNumCols = A.iGetNumCols();

                    static_assert(NumRowsA == iNumRowsStatic, "Number of rows does not match");
                    static_assert(NumColsA == iNumColsStatic, "Number of columns does not match");

                    SP_GRAD_ASSERT(B.iGetNumRows() == iNumRows || B.iGetNumRows() == SpMatrixSize::DYNAMIC);
                    SP_GRAD_ASSERT(B.iGetNumCols() == iNumCols || B.iGetNumCols() == SpMatrixSize::DYNAMIC);

                    for (index_type j = 1; j <= iNumCols; ++j) {
                         for (index_type i = 1; i <= iNumRows; ++i) {
                              SpGradient& Aij = A.GetElem(i, j);
                              const doublereal uij = Aij.dGetValue();
                              const doublereal vij = B.dGetValue(i, j);
                              const doublereal fij = Func::f(uij, vij);
                              const doublereal dfij_du = Func::df_du(uij, vij);
                              Aij.template InitDerivAssign<Func>(fij, dfij_du, Aij.iGetSize());
                         }
                    }
               }

               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignCompr(SpMatrixBase<SpGradient, NumRowsA, NumColsA>& A, const SpMatElemExprBase<doublereal, ExprB>& B, const SpGradExpDofMapHelper<SpGradient>& oDofMap) {
                    typedef typename remove_all<ExprB>::type ExprTypeB;
                    constexpr index_type iNumRowsStatic = ExprTypeB::iNumRowsStatic;
                    constexpr index_type iNumColsStatic = ExprTypeB::iNumColsStatic;

                    const index_type iNumRows = A.iGetNumRows();
                    const index_type iNumCols = A.iGetNumCols();

                    static_assert(NumRowsA == iNumRowsStatic, "Number of rows does not match");
                    static_assert(NumColsA == iNumColsStatic, "Number of columns does not match");

                    SP_GRAD_ASSERT(B.iGetNumRows() == iNumRows || B.iGetNumRows() == SpMatrixSize::DYNAMIC);
                    SP_GRAD_ASSERT(B.iGetNumCols() == iNumCols || B.iGetNumCols() == SpMatrixSize::DYNAMIC);

                    for (index_type j = 1; j <= iNumCols; ++j) {
                         for (index_type i = 1; i <= iNumRows; ++i) {
                              SpGradient& Aij = A.GetElem(i, j);
                              const doublereal uij = Aij.dGetValue();
                              const doublereal vij = B.dGetValue(i, j);
                              const doublereal fij = Func::f(uij, vij);
                              const doublereal dfij_du = Func::df_du(uij, vij);
                              Aij.template InitDerivAssign<Func>(fij, dfij_du, oDofMap.GetDofMap());
                         }
                    }
               }

               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignUncompr(SpMatrixBase<SpGradient, NumRowsA, NumColsA>& A, const SpMatElemExprBase<doublereal, ExprB>& B) {
                    ElemAssignCompr<eTransp, Func>(A, B);
               }
          };

          template <>
          struct ElemAssignHelper<SpGradient, SpGradient> {
               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignCompr(SpMatrixBase<SpGradient, NumRowsA, NumColsA>& A, const SpMatElemExprBase<SpGradient, ExprB>& B) {
                    typedef typename remove_all<ExprB>::type ExprTypeB;
                    constexpr index_type iNumRowsStatic = ExprTypeB::iNumRowsStatic;
                    constexpr index_type iNumColsStatic = ExprTypeB::iNumColsStatic;

                    const index_type iNumRows = A.iGetNumRows();
                    const index_type iNumCols = A.iGetNumCols();

                    static_assert(NumRowsA == iNumRowsStatic, "Number of rows does not match");
                    static_assert(NumColsA == iNumColsStatic, "Number of columns does not match");

                    SP_GRAD_ASSERT(B.iGetNumRows() == iNumRows || B.iGetNumRows() == SpMatrixSize::DYNAMIC);
                    SP_GRAD_ASSERT(B.iGetNumCols() == iNumCols || B.iGetNumCols() == SpMatrixSize::DYNAMIC);

                    SP_GRAD_ASSERT(!B.bHaveRefTo(A));
                    SP_GRAD_ASSERT(A.bValid());

                    SpGradExpDofMap oDofMap;

                    for (index_type j = 1; j <= iNumCols; ++j) {
                         for (index_type i = 1; i <= iNumRows; ++i) {
                              SpGradient& Aij = A.GetElem(i, j);

                              if (!Aij.bIsUnique()) {
                                   Aij.MakeUnique();
                              }

                              SpGradDofStat oDofStat;

                              Aij.GetDofStat(oDofStat);
                              B.GetDofStat(oDofStat, i, j);

                              oDofMap.Reset(oDofStat);

                              Aij.InsertDof(oDofMap);
                              B.InsertDof(oDofMap, i, j);

                              oDofMap.InsertDone();

                              const doublereal uij = Aij.dGetValue();
                              const doublereal vij = B.dGetValue(i, j);
                              const doublereal fij = Func::f(uij, vij);
                              const doublereal dfij_du = Func::df_du(uij, vij);
                              const doublereal dfij_dv = Func::df_dv(uij, vij);

                              Aij.template InitDerivAssign<Func>(fij, dfij_du, oDofMap);

                              B.AddDeriv(Aij, dfij_dv, oDofMap, i, j);

                              SP_GRAD_ASSERT(Aij.bIsUnique());
                         }
                    }

                    SP_GRAD_ASSERT(A.bValid());
               }

               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignCompr(SpMatrixBase<SpGradient, NumRowsA, NumColsA>& A,
                                                  const SpMatElemExprBase<SpGradient, ExprB>& B,
                                                  const SpGradExpDofMapHelper<SpGradient>& oDofMap) {
                    typedef typename remove_all<ExprB>::type ExprTypeB;
                    constexpr index_type iNumRowsStatic = ExprTypeB::iNumRowsStatic;
                    constexpr index_type iNumColsStatic = ExprTypeB::iNumColsStatic;

                    const index_type iNumRows = A.iGetNumRows();
                    const index_type iNumCols = A.iGetNumCols();

                    static_assert(NumRowsA == iNumRowsStatic, "Number of rows does not match");
                    static_assert(NumColsA == iNumColsStatic, "Number of columns does not match");

                    SP_GRAD_ASSERT(B.iGetNumRows() == iNumRows || B.iGetNumRows() == SpMatrixSize::DYNAMIC);
                    SP_GRAD_ASSERT(B.iGetNumCols() == iNumCols || B.iGetNumCols() == SpMatrixSize::DYNAMIC);

                    SP_GRAD_ASSERT(!B.bHaveRefTo(A));
                    SP_GRAD_ASSERT(A.bValid());

                    for (index_type j = 1; j <= iNumCols; ++j) {
                         for (index_type i = 1; i <= iNumRows; ++i) {
                              SpGradient& Aij = A.GetElem(i, j);

                              SP_GRAD_ASSERT(Aij.bIsUnique());

                              const doublereal uij = Aij.dGetValue();
                              const doublereal vij = B.dGetValue(i, j);
                              const doublereal fij = Func::f(uij, vij);
                              const doublereal dfij_du = Func::df_du(uij, vij);
                              const doublereal dfij_dv = Func::df_dv(uij, vij);

                              Aij.template InitDerivAssign<Func>(fij, dfij_du, oDofMap.GetDofMap());

                              B.AddDeriv(Aij, dfij_dv, oDofMap.GetDofMap(), i, j);

                              SP_GRAD_ASSERT(Aij.bIsUnique());
                         }
                    }

                    SP_GRAD_ASSERT(A.bValid());
               }

               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignUncompr(SpMatrixBase<SpGradient, NumRowsA, NumColsA>& A, const SpMatElemExprBase<SpGradient, ExprB>& B) {
                    typedef typename remove_all<ExprB>::type ExprTypeB;
                    constexpr index_type iNumRowsStatic = ExprTypeB::iNumRowsStatic;
                    constexpr index_type iNumColsStatic = ExprTypeB::iNumColsStatic;

                    const index_type iNumRows = A.iGetNumRows();
                    const index_type iNumCols = A.iGetNumCols();

                    static_assert(NumRowsA == iNumRowsStatic, "Number of rows does not match");
                    static_assert(NumColsA == iNumColsStatic, "Number of columns does not match");

                    SP_GRAD_ASSERT(B.iGetNumRows() == iNumRows || B.iGetNumRows() == SpMatrixSize::DYNAMIC);
                    SP_GRAD_ASSERT(B.iGetNumCols() == iNumCols || B.iGetNumCols() == SpMatrixSize::DYNAMIC);

                    SP_GRAD_ASSERT(!B.bHaveRefTo(A));
                    SP_GRAD_ASSERT(A.bValid());

                    for (index_type j = 1; j <= iNumCols; ++j) {
                         for (index_type i = 1; i <= iNumRows; ++i) {
                              SpGradient& Aij = A.GetElem(i, j);

                              const doublereal uij = Aij.dGetValue();
                              const doublereal vij = B.dGetValue(i, j);
                              const doublereal fij = Func::f(uij, vij);
                              const doublereal dfij_du = Func::df_du(uij, vij);
                              const doublereal dfij_dv = Func::df_dv(uij, vij);

                              Aij.template InitDerivAssign<Func>(fij, dfij_du, Aij.iGetSize() + B.iGetSize(i, j));

                              B.InsertDeriv(Aij, dfij_dv, i, j);

                              SP_GRAD_ASSERT(!Aij.bIsUnique());
                         }
                    }

                    SP_GRAD_ASSERT(A.bValid());
               }
          };

          template <>
          struct ElemAssignHelper<GpGradProd, GpGradProd> {
               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignUncompr(SpMatrixBase<GpGradProd, NumRowsA, NumColsA>& A, const SpMatElemExprBase<GpGradProd, ExprB>& B) {
                    typedef typename remove_all<ExprB>::type ExprTypeB;
                    constexpr index_type iNumRowsStatic = ExprTypeB::iNumRowsStatic;
                    constexpr index_type iNumColsStatic = ExprTypeB::iNumColsStatic;

                    const index_type iNumRows = A.iGetNumRows();
                    const index_type iNumCols = A.iGetNumCols();

                    static_assert(NumRowsA == iNumRowsStatic, "Number of rows does not match");
                    static_assert(NumColsA == iNumColsStatic, "Number of columns does not match");

                    SP_GRAD_ASSERT(B.iGetNumRows() == iNumRows || B.iGetNumRows() == SpMatrixSize::DYNAMIC);
                    SP_GRAD_ASSERT(B.iGetNumCols() == iNumCols || B.iGetNumCols() == SpMatrixSize::DYNAMIC);

                    SP_GRAD_ASSERT(!B.bHaveRefTo(A));
                    SP_GRAD_ASSERT(A.bValid());

                    SpGradExpDofMap oDofMap;

                    for (index_type j = 1; j <= iNumCols; ++j) {
                         for (index_type i = 1; i <= iNumRows; ++i) {
                              GpGradProd& Aij = A.GetElem(i, j);

                              const doublereal uij = Aij.dGetValue();
                              const doublereal udij = Aij.dGetDeriv();
                              const doublereal vij = B.dGetValue(i, j);
                              const doublereal fij = Func::f(uij, vij);
                              const doublereal dfij_du = Func::df_du(uij, vij);
                              const doublereal dfij_dv = Func::df_dv(uij, vij);

                              Aij.Reset(fij, dfij_du * udij);
                              B.InsertDeriv(Aij, dfij_dv, i, j);
                         }
                    }

                    SP_GRAD_ASSERT(A.bValid());
               }

               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignCompr(SpMatrixBase<GpGradProd, NumRowsA, NumColsA>& A, const SpMatElemExprBase<GpGradProd, ExprB>& B) {
                    ElemAssignHelper<GpGradProd, GpGradProd>::template ElemAssignUncompr<eTransp, Func>(A, B);
               }

               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignCompr(SpMatrixBase<GpGradProd, NumRowsA, NumColsA>& A, const SpMatElemExprBase<GpGradProd, ExprB>& B, const SpGradExpDofMapHelper<GpGradProd>&) {
                    ElemAssignHelper<GpGradProd, GpGradProd>::template ElemAssignUncompr<eTransp, Func>(A, B);
               }
          };

          template <>
          struct ElemAssignHelper<GpGradProd, doublereal> {
               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignUncompr(SpMatrixBase<GpGradProd, NumRowsA, NumColsA>& A, const SpMatElemExprBase<doublereal, ExprB>& B) {
                    typedef typename remove_all<ExprB>::type ExprTypeB;
                    constexpr index_type iNumRowsStatic = ExprTypeB::iNumRowsStatic;
                    constexpr index_type iNumColsStatic = ExprTypeB::iNumColsStatic;

                    const index_type iNumRows = A.iGetNumRows();
                    const index_type iNumCols = A.iGetNumCols();

                    static_assert(NumRowsA == iNumRowsStatic, "Number of rows does not match");
                    static_assert(NumColsA == iNumColsStatic, "Number of columns does not match");

                    SP_GRAD_ASSERT(B.iGetNumRows() == iNumRows || B.iGetNumRows() == SpMatrixSize::DYNAMIC);
                    SP_GRAD_ASSERT(B.iGetNumCols() == iNumCols || B.iGetNumCols() == SpMatrixSize::DYNAMIC);
                    SP_GRAD_ASSERT(!B.bHaveRefTo(A));
                    SP_GRAD_ASSERT(A.bValid());

                    SpGradExpDofMap oDofMap;

                    for (index_type j = 1; j <= iNumCols; ++j) {
                         for (index_type i = 1; i <= iNumRows; ++i) {
                              GpGradProd& Aij = A.GetElem(i, j);

                              const doublereal uij = Aij.dGetValue();
                              const doublereal udij = Aij.dGetDeriv();
                              const doublereal vij = B.dGetValue(i, j);
                              const doublereal fij = Func::f(uij, vij);
                              const doublereal dfij_du = Func::df_du(uij, vij);

                              Aij.Reset(fij, dfij_du * udij);
                         }
                    }

                    SP_GRAD_ASSERT(A.bValid());
               }

               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignCompr(SpMatrixBase<GpGradProd, NumRowsA, NumColsA>& A, const SpMatElemExprBase<doublereal, ExprB>& B) {
                    ElemAssignHelper<GpGradProd, doublereal>::template ElemAssignUncompr<eTransp, Func>(A, B);
               }

               template <MatTranspEvalFlag eTransp, typename Func, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssignCompr(SpMatrixBase<GpGradProd, NumRowsA, NumColsA>& A, const SpMatElemExprBase<doublereal, ExprB>& B, const SpGradExpDofMapHelper<GpGradProd>&) {
                    ElemAssignHelper<GpGradProd, doublereal>::template ElemAssignUncompr<eTransp, Func>(A, B);
               }
          };

          template <SpGradCommon::ExprEvalFlags eFlags>
          struct ElemAssignHelperCompr {
               template <MatTranspEvalFlag eTransp, typename Func, typename ValueA, typename ValueB, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssign(SpMatrixBase<ValueA, NumRowsA, NumColsA>& A, const SpMatElemExprBase<ValueB, ExprB>& B) = delete;

               template <MatTranspEvalFlag eTransp, typename Func, typename ValueA, typename ValueB, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssign(SpMatrixBase<ValueA, NumRowsA, NumColsA>& A, const SpMatElemExprBase<ValueB, ExprB>& B, const SpGradExpDofMapHelper<ValueA>& oDofMap) = delete;
          };

          template <>
          struct ElemAssignHelperCompr<SpGradCommon::ExprEvalUnique> {
               template <MatTranspEvalFlag eTransp, typename Func, typename ValueA, typename ValueB, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssign(SpMatrixBase<ValueA, NumRowsA, NumColsA>& A, const SpMatElemExprBase<ValueB, ExprB>& B) {
                    ElemAssignHelper<ValueA, ValueB>::template ElemAssignCompr<eTransp, Func>(A, B);
               }

               template <MatTranspEvalFlag eTransp, typename Func, typename ValueA, typename ValueB, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssign(SpMatrixBase<ValueA, NumRowsA, NumColsA>& A, const SpMatElemExprBase<ValueB, ExprB>& B, const SpGradExpDofMapHelper<ValueA>& oDofMap) {
                    ElemAssignHelper<ValueA, ValueB>::template ElemAssignCompr<eTransp, Func>(A, B, oDofMap);
               }
          };

          template <>
          struct ElemAssignHelperCompr<SpGradCommon::ExprEvalDuplicate> {
               template <MatTranspEvalFlag eTransp, typename Func, typename ValueA, typename ValueB, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssign(SpMatrixBase<ValueA, NumRowsA, NumColsA>& A, const SpMatElemExprBase<ValueB, ExprB>& B) {
                    ElemAssignHelper<ValueA, ValueB>::template ElemAssignUncompr<eTransp, Func>(A, B);
               }

               template <MatTranspEvalFlag eTransp, typename Func, typename ValueA, typename ValueB, typename ExprB, index_type NumRowsA, index_type NumColsA>
               static inline void ElemAssign(SpMatrixBase<ValueA, NumRowsA, NumColsA>& A, const SpMatElemExprBase<ValueB, ExprB>& B, const SpGradExpDofMapHelper<ValueA>& oDofMap) {
                    ElemAssignHelper<ValueA, ValueB>::template ElemAssignCompr<eTransp, Func>(A, B, oDofMap);
               }
          };
     }

     template <typename ValueType, typename DERIVED>
     template <util::MatTranspEvalFlag eTransp,
               SpGradCommon::ExprEvalFlags eCompr,
               typename ValueTypeA,
               index_type NumRowsA, index_type NumColsA>
     void SpMatElemExprBase<ValueType, DERIVED>::ElemEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
          util::MatEvalHelperCompr<eCompr>::template ElemEval<eTransp>(*this, A);
     }

     template <typename ValueType, typename DERIVED>
     template <util::MatTranspEvalFlag eTransp,
               SpGradCommon::ExprEvalFlags eCompr,
               typename Func,
               typename ValueTypeA,
               index_type NumRowsA, index_type NumColsA>
     void SpMatElemExprBase<ValueType, DERIVED>::ElemAssign(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
          util::ElemAssignHelperCompr<eCompr>::template ElemAssign<eTransp, Func>(A, *this);
     }

     template <typename ValueType, typename DERIVED>
     template <util::MatTranspEvalFlag eTransp,
               SpGradCommon::ExprEvalFlags eCompr,
               typename ValueTypeA,
               index_type NumRowsA, index_type NumColsA>
     void SpMatElemExprBase<ValueType, DERIVED>::ElemEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A, const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
          util::MatEvalHelperCompr<eCompr>::template ElemEval<eTransp>(*this, A, oDofMap);
     }

     template <typename ValueType, typename DERIVED>
     template <util::MatTranspEvalFlag eTransp,
               SpGradCommon::ExprEvalFlags eCompr,
               typename Func,
               typename ValueTypeA,
               index_type NumRowsA, index_type NumColsA>
     void SpMatElemExprBase<ValueType, DERIVED>::ElemAssign(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A, const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
          util::ElemAssignHelperCompr<eCompr>::template ElemAssign<eTransp, Func>(A, *this, oDofMap);
     }

     template <typename ValueType, typename DERIVED>
     template <util::MatTranspEvalFlag eTransp, typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
     void SpMatElemExprBase<ValueType, DERIVED>::ElemEvalUncompr(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
          const index_type iNumRows = iGetNumRows();
          const index_type iNumCols = iGetNumCols();
          const index_type iMaxSize = iGetMaxSize();

          typedef util::MatEvalHelperTransp<eTransp> MatEvalType;

          constexpr bool bTransposed = eTransp == util::MatTranspEvalFlag::TRANSPOSED;

          static_assert(NumRowsA == (!bTransposed ? iNumRowsStatic : iNumColsStatic), "Number of rows does not match");
          static_assert(NumColsA == (!bTransposed ? iNumColsStatic : iNumRowsStatic), "Number of columns does not match");

          SP_GRAD_ASSERT(!bHaveRefTo(A));

          MatEvalType::ResizeReset(A, iNumRows, iNumCols, iMaxSize);

          SP_GRAD_ASSERT(A.iGetNumRows() == (!bTransposed ? iNumRows : iNumCols));
          SP_GRAD_ASSERT(A.iGetNumCols() == (!bTransposed ? iNumCols : iNumRows));

          for (index_type j = 1; j <= iNumCols; ++j) {
               for (index_type i = 1; i <= iNumRows; ++i) {
                    ValueTypeA& Ai = MatEvalType::GetElem(A, i, j);
                    SP_GRAD_ASSERT(SpGradientTraits<ValueTypeA>::dGetValue(Ai) == 0.);

                    SpGradientTraits<ValueTypeA>::ResizeReset(Ai, dGetValue(i, j), iGetSize(i, j));
                    InsertDeriv(Ai, 1., i, j);
               }
          }
     }

     template <typename ValueType, typename DERIVED>
     template <util::MatTranspEvalFlag eTransp, typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
     void SpMatElemExprBase<ValueType, DERIVED>::ElemEvalCompr(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
          const index_type iNumRows = iGetNumRows();
          const index_type iNumCols = iGetNumCols();

          typedef util::MatEvalHelperTransp<eTransp> MatEvalType;
          constexpr bool bTransposed = eTransp == util::MatTranspEvalFlag::TRANSPOSED;

          static_assert(NumRowsA == (!bTransposed ? iNumRowsStatic : iNumColsStatic), "Number of rows does not match");
          static_assert(NumColsA == (!bTransposed ? iNumColsStatic : iNumRowsStatic), "Number of columns does not match");

          SP_GRAD_ASSERT(!bHaveRefTo(A));

          SpGradDofStat oDofStat;

          for (index_type j = 1; j <= iNumCols; ++j) {
               for (index_type i = 1; i <= iNumRows; ++i) {
                    GetDofStat(oDofStat, i, j);
               }
          }

          SpGradExpDofMap oDofMap(oDofStat);

          for (index_type j = 1; j <= iNumCols; ++j) {
               for (index_type i = 1; i <= iNumRows; ++i) {
                    InsertDof(oDofMap, i, j);
               }
          }

          oDofMap.InsertDone();

          MatEvalType::ResizeReset(A, iNumRows, iNumCols, oDofStat.iNumNz);

          SP_GRAD_ASSERT(A.iGetNumRows() == (!bTransposed ? iNumRows : iNumCols));
          SP_GRAD_ASSERT(A.iGetNumCols() == (!bTransposed ? iNumCols : iNumRows));

          for (index_type j = 1; j <= iNumCols; ++j) {
               for (index_type i = 1; i <= iNumRows; ++i) {
                    ValueTypeA& Ai = MatEvalType::GetElem(A, i, j);

                    SP_GRAD_ASSERT(SpGradientTraits<ValueTypeA>::iGetSize(Ai) == 0);
                    SP_GRAD_ASSERT(SpGradientTraits<ValueTypeA>::dGetValue(Ai) == 0.);

                    Ai.ResizeReset(dGetValue(i, j), oDofStat.iNumNz);
                    Ai.InitDeriv(oDofMap);
                    AddDeriv(Ai, 1., oDofMap, i, j);

                    SP_GRAD_ASSERT(SpGradientTraits<ValueTypeA>::bIsUnique(Ai));
               }
          }
     }

     template <typename ValueType, typename DERIVED>
     template <util::MatTranspEvalFlag eTransp, index_type NumRowsA, index_type NumColsA>
     void SpMatElemExprBase<ValueType, DERIVED>::ElemEvalCompr(SpMatrixBase<SpGradient, NumRowsA, NumColsA>& A, const SpGradExpDofMapHelper<SpGradient>& oDofMap) const {
          const index_type iNumRows = iGetNumRows();
          const index_type iNumCols = iGetNumCols();

          typedef util::MatEvalHelperTransp<eTransp> MatEvalType;
          constexpr bool bTransposed = eTransp == util::MatTranspEvalFlag::TRANSPOSED;

          static_assert(NumRowsA == (!bTransposed ? iNumRowsStatic : iNumColsStatic), "Number of rows does not match");
          static_assert(NumColsA == (!bTransposed ? iNumColsStatic : iNumRowsStatic), "Number of columns does not match");

          SP_GRAD_ASSERT(!bHaveRefTo(A));

          MatEvalType::ResizeReset(A, iNumRows, iNumCols, oDofMap.GetDofStat().iNumNz);

          SP_GRAD_ASSERT(A.iGetNumRows() == (!bTransposed ? iNumRows : iNumCols));
          SP_GRAD_ASSERT(A.iGetNumCols() == (!bTransposed ? iNumCols : iNumRows));

          for (index_type j = 1; j <= iNumCols; ++j) {
               for (index_type i = 1; i <= iNumRows; ++i) {
                    SpGradient& Ai = MatEvalType::GetElem(A, i, j);

                    SP_GRAD_ASSERT(SpGradientTraits<SpGradient>::iGetSize(Ai) == 0);
                    SP_GRAD_ASSERT(SpGradientTraits<SpGradient>::dGetValue(Ai) == 0.);

                    Ai.ResizeReset(dGetValue(i, j), oDofMap.GetDofStat().iNumNz);
                    Ai.InitDeriv(oDofMap.GetDofMap());
                    AddDeriv(Ai, 1., oDofMap.GetDofMap(), i, j);

                    SP_GRAD_ASSERT(SpGradientTraits<SpGradient>::bIsUnique(Ai));
               }
          }
     }

     template <typename ValueType, typename DERIVED>
     template <util::MatTranspEvalFlag eTransp, typename Func, typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
     void SpMatElemExprBase<ValueType, DERIVED>::ElemAssignCompr(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
          util::ElemAssignHelper<ValueTypeA, ValueType>::template ElemAssignCompr<eTransp, Func, DERIVED, NumRowsA, NumColsA>(A, *this);
     }

     namespace util {
          struct MatMulExprHelper {
               SpGradDofStat oDofStat;
               SpGradExpDofMap oDofMap;

               template <typename TA, typename TB>
               inline void InnerProduct(typename ResultType<TA, TB>::Type& Aij,
                                        const TA* pAFirst,
                                        const TA* pALast,
                                        index_type iAOffset,
                                        const TB* pBFirst,
                                        const TB* pBLast,
                                        index_type iBOffset) const {
                    util::MapInnerProduct(Aij,
                                          pAFirst,
                                          pALast,
                                          iAOffset,
                                          pBFirst,
                                          pBLast,
                                          iBOffset,
                                          oDofMap);
               }

               inline void ResetDofStat() {
                    oDofStat = SpGradDofStat{};
               }

               inline void ResetDofMap() {
                    oDofMap.Reset(oDofStat);
               }

               inline void InsertDone() {
                    oDofMap.InsertDone();
               }

               template <typename T>
               inline void GetDofStat(const T* pFirst, const T* const pLast, const index_type iOffset) noexcept {
                    while (pFirst < pLast) {
                         SpGradientTraits<T>::GetDofStat(*pFirst, oDofStat);
                         pFirst += iOffset;
                    }
               }

               template <typename T>
               inline void InsertDof(const T* pFirst, const T* const pLast, const index_type iOffset) noexcept {
                    while (pFirst < pLast) {
                         SpGradientTraits<T>::InsertDof(*pFirst, oDofMap);
                         pFirst += iOffset;
                    }
               }
          };


          template <util::MatTranspEvalFlag eTransp, bool bIsGradientLhs, bool bIsGradientRhs, bool bIsSparse, bool bOneDofMap>
          struct MatMulExprLoop;

          template <util::MatTranspEvalFlag eTransp>
          struct MatMulExprLoop2 {
               template <typename MatA, typename MatU, typename MatV>
               static inline void InnerProduct(MatA& Atmp,
                                               const MatU& utmp,
                                               const MatV& vtmp,
                                               const index_type iNumRows,
                                               const index_type iNumCols,
                                               const index_type iRowSizeU,
                                               const index_type iRowOffsetU,
                                               const index_type iColOffsetU,
                                               const index_type iRowOffsetV,
                                               const index_type iColOffsetV,
                                               const index_type iColSizeV,
                                               const SpGradExpDofMapHelper<typename MatA::ValueType>& oDofMap) {
                    typedef MatEvalHelperTransp<eTransp> MatEvalType;
                    typedef InnerProductHelper<typename std::iterator_traits<decltype(utmp.begin())>::value_type,
                                               typename std::iterator_traits<decltype(vtmp.begin())>::value_type> IPH;

                    for (index_type j = 1; j <= iNumCols; ++j) {
                         const auto* const pvtmp_j = vtmp.begin() + (j - 1) * iColOffsetV;

                         for (index_type i = 1; i <= iNumRows; ++i) {
                              const auto* const putmp_i = utmp.begin() + (i - 1) * iRowOffsetU;

                              IPH::MapEval(MatEvalType::GetElem(Atmp, i, j),
                                           putmp_i,
                                           putmp_i + iRowSizeU,
                                           iColOffsetU,
                                           pvtmp_j,
                                           pvtmp_j + iColSizeV,
                                           iRowOffsetV,
                                           oDofMap);
                         }
                    }
               }
          };

          template <util::MatTranspEvalFlag eTransp>
          struct MatMulExprLoop<eTransp, true, true, true, false> {
               template <typename MatA, typename MatU, typename MatV>
               static inline void InnerProduct(MatA& Atmp,
                                               const MatU& utmp,
                                               const MatV& vtmp,
                                               const index_type iNumRows,
                                               const index_type iNumCols,
                                               const index_type iRowSizeU,
                                               const index_type iRowOffsetU,
                                               const index_type iColOffsetU,
                                               const index_type iRowOffsetV,
                                               const index_type iColOffsetV,
                                               const index_type iColSizeV) {
                    MatMulExprHelper oMatMulHelper;
                    typedef MatEvalHelperTransp<eTransp> MatEvalType;

                    for (index_type j = 1; j <= iNumCols; ++j) {
                         const SpGradient* const pvtmp_j = vtmp.begin() + (j - 1) * iColOffsetV;

                         for (index_type i = 1; i <= iNumRows; ++i) {
                              const SpGradient* const putmp_i = utmp.begin() + (i - 1) * iRowOffsetU;

                              // Generic method: rebuild dof map for every row and every column
                              oMatMulHelper.ResetDofStat();
                              oMatMulHelper.GetDofStat(putmp_i, putmp_i + iRowSizeU, iColOffsetU);
                              oMatMulHelper.GetDofStat(pvtmp_j, pvtmp_j + iColSizeV, iRowOffsetV);
                              oMatMulHelper.ResetDofMap();
                              oMatMulHelper.InsertDof(putmp_i, putmp_i + iRowSizeU, iColOffsetU);
                              oMatMulHelper.InsertDof(pvtmp_j, pvtmp_j + iColSizeV, iRowOffsetV);
                              oMatMulHelper.InsertDone();

                              oMatMulHelper.InnerProduct(MatEvalType::GetElem(Atmp, i, j),
                                                         putmp_i,
                                                         putmp_i + iRowSizeU,
                                                         iColOffsetU,
                                                         pvtmp_j,
                                                         pvtmp_j + iColSizeV,
                                                         iRowOffsetV);
                         }
                    }
               }
          };

          template <util::MatTranspEvalFlag eTransp>
          struct MatMulExprLoop<eTransp, true, true, true, true> {
               template <typename MatA, typename MatU, typename MatV>
               static inline void InnerProduct(MatA& Atmp,
                                               const MatU& utmp,
                                               const MatV& vtmp,
                                               const index_type iNumRows,
                                               const index_type iNumCols,
                                               const index_type iRowSizeU,
                                               const index_type iRowOffsetU,
                                               const index_type iColOffsetU,
                                               const index_type iRowOffsetV,
                                               const index_type iColOffsetV,
                                               const index_type iColSizeV) {
                    MatMulExprHelper oMatMulHelper;
                    typedef MatEvalHelperTransp<eTransp> MatEvalType;

                    // Generic method using a single dof map
                    // will be efficient if all elements of utmp and vtmp will depend on the same dof's

                    oMatMulHelper.ResetDofStat();

                    for (const SpGradient* putmp = utmp.begin(); putmp < utmp.begin() + iRowOffsetU * iNumRows; putmp += iRowOffsetU) {
                         oMatMulHelper.GetDofStat(putmp, putmp + iRowSizeU, iColOffsetU);
                    }

                    for (const SpGradient* pvtmp = vtmp.begin(); pvtmp < vtmp.begin() + iColOffsetV * iNumCols; pvtmp += iColOffsetV) {
                         oMatMulHelper.GetDofStat(pvtmp, pvtmp + iColSizeV, iRowOffsetV);
                    }

                    oMatMulHelper.ResetDofMap();

                    for (const SpGradient* putmp = utmp.begin(); putmp < utmp.begin() + iRowOffsetU * iNumRows; putmp += iRowOffsetU) {
                         oMatMulHelper.InsertDof(putmp, putmp + iRowSizeU, iColOffsetU);
                    }

                    for (const SpGradient* pvtmp = vtmp.begin(); pvtmp < vtmp.begin() + iColOffsetV * iNumCols; pvtmp += iColOffsetV) {
                         oMatMulHelper.InsertDof(pvtmp, pvtmp + iColSizeV, iRowOffsetV);
                    }

                    oMatMulHelper.InsertDone();

                    for (index_type j = 1; j <= iNumCols; ++j) {
                         const SpGradient* const pvtmp_j = vtmp.begin() + (j - 1) * iColOffsetV;

                         for (index_type i = 1; i <= iNumRows; ++i) {
                              const SpGradient* const putmp_i = utmp.begin() + (i - 1) * iRowOffsetU;

                              oMatMulHelper.InnerProduct(MatEvalType::GetElem(Atmp, i, j),
                                                         putmp_i,
                                                         putmp_i + iRowSizeU,
                                                         iColOffsetU,
                                                         pvtmp_j,
                                                         pvtmp_j + iColSizeV,
                                                         iRowOffsetV);
                         }
                    }
               }
          };

          template <util::MatTranspEvalFlag eTransp>
          struct MatMulExprLoop<eTransp, true, false, true, true> {
               template <typename MatA, typename MatU, typename MatV>
               static inline void InnerProduct(MatA& Atmp,
                                               const MatU& utmp,
                                               const MatV& vtmp,
                                               const index_type iNumRows,
                                               const index_type iNumCols,
                                               const index_type iRowSizeU,
                                               const index_type iRowOffsetU,
                                               const index_type iColOffsetU,
                                               const index_type iRowOffsetV,
                                               const index_type iColOffsetV,
                                               const index_type iColSizeV) {
                    MatMulExprHelper oMatMulHelper;
                    typedef MatEvalHelperTransp<eTransp> MatEvalType;

                    // Generic method using a single dof map
                    // will be efficient if all elements of utmp will depend on the same dof's

                    oMatMulHelper.ResetDofStat();

                    for (const SpGradient* putmp = utmp.begin(); putmp < utmp.begin() + iRowOffsetU * iNumRows; putmp += iRowOffsetU) {
                         oMatMulHelper.GetDofStat(putmp, putmp + iRowSizeU, iColOffsetU);
                    }

                    oMatMulHelper.ResetDofMap();

                    for (const SpGradient* putmp = utmp.begin(); putmp < utmp.begin() + iRowOffsetU * iNumRows; putmp += iRowOffsetU) {
                         oMatMulHelper.InsertDof(putmp, putmp + iRowSizeU, iColOffsetU);
                    }

                    oMatMulHelper.InsertDone();

                    for (index_type i = 1; i <= iNumRows; ++i) {
                         const SpGradient* const putmp_i = utmp.begin() + (i - 1) * iRowOffsetU;

                         for (index_type j = 1; j <= iNumCols; ++j) {
                              const doublereal* const pvtmp_j = vtmp.begin() + (j - 1) * iColOffsetV;

                              oMatMulHelper.InnerProduct(MatEvalType::GetElem(Atmp, i, j),
                                                         putmp_i,
                                                         putmp_i + iRowSizeU,
                                                         iColOffsetU,
                                                         pvtmp_j,
                                                         pvtmp_j + iColSizeV,
                                                         iRowOffsetV);
                         }
                    }

               }
          };

          template <util::MatTranspEvalFlag eTransp>
          struct MatMulExprLoop<eTransp, true, false, true, false> {
               template <typename MatA, typename MatU, typename MatV>
               static inline void InnerProduct(MatA& Atmp,
                                               const MatU& utmp,
                                               const MatV& vtmp,
                                               const index_type iNumRows,
                                               const index_type iNumCols,
                                               const index_type iRowSizeU,
                                               const index_type iRowOffsetU,
                                               const index_type iColOffsetU,
                                               const index_type iRowOffsetV,
                                               const index_type iColOffsetV,
                                               const index_type iColSizeV) {
                    MatMulExprHelper oMatMulHelper;
                    typedef MatEvalHelperTransp<eTransp> MatEvalType;

                    for (index_type i = 1; i <= iNumRows; ++i) {
                         const SpGradient* const putmp_i = utmp.begin() + (i - 1) * iRowOffsetU;

                         // Build dof map once for each row and reuse it for all columns
                         oMatMulHelper.ResetDofStat();
                         oMatMulHelper.GetDofStat(putmp_i, putmp_i + iRowSizeU, iColOffsetU);
                         oMatMulHelper.ResetDofMap();
                         oMatMulHelper.InsertDof(putmp_i, putmp_i + iRowSizeU, iColOffsetU);
                         oMatMulHelper.InsertDone();

                         for (index_type j = 1; j <= iNumCols; ++j) {
                              const doublereal* const pvtmp_j = vtmp.begin() + (j - 1) * iColOffsetV;

                              oMatMulHelper.InnerProduct(MatEvalType::GetElem(Atmp, i, j),
                                                         putmp_i,
                                                         putmp_i + iRowSizeU,
                                                         iColOffsetU,
                                                         pvtmp_j,
                                                         pvtmp_j + iColSizeV,
                                                         iRowOffsetV);
                         }
                    }

               }
          };

          template <util::MatTranspEvalFlag eTransp>
          struct MatMulExprLoop<eTransp, false, true, true, true> {
               template <typename MatA, typename MatU, typename MatV>
               static inline void InnerProduct(MatA& Atmp,
                                               const MatU& utmp,
                                               const MatV& vtmp,
                                               const index_type iNumRows,
                                               const index_type iNumCols,
                                               const index_type iRowSizeU,
                                               const index_type iRowOffsetU,
                                               const index_type iColOffsetU,
                                               const index_type iRowOffsetV,
                                               const index_type iColOffsetV,
                                               const index_type iColSizeV) {
                    MatMulExprHelper oMatMulHelper;
                    typedef MatEvalHelperTransp<eTransp> MatEvalType;

                    // Generic method using a single dof map
                    // will be efficient if all elements of vtmp will depend on the same dof's

                    oMatMulHelper.ResetDofStat();

                    for (const SpGradient* pvtmp = vtmp.begin(); pvtmp < vtmp.begin() + iColOffsetV * iNumCols; pvtmp += iColOffsetV) {
                         oMatMulHelper.GetDofStat(pvtmp, pvtmp + iColSizeV, iRowOffsetV);
                    }

                    oMatMulHelper.ResetDofMap();

                    for (const SpGradient* pvtmp = vtmp.begin(); pvtmp < vtmp.begin() + iColOffsetV * iNumCols; pvtmp += iColOffsetV) {
                         oMatMulHelper.InsertDof(pvtmp, pvtmp + iColSizeV, iRowOffsetV);
                    }

                    oMatMulHelper.InsertDone();

                    for (index_type j = 1; j <= iNumCols; ++j) {
                         const SpGradient* const pvtmp_j = vtmp.begin() + (j - 1) * iColOffsetV;

                         for (index_type i = 1; i <= iNumRows; ++i) {
                              const doublereal* const putmp_i = utmp.begin() + (i - 1) * iRowOffsetU;

                              oMatMulHelper.InnerProduct(MatEvalType::GetElem(Atmp, i, j),
                                                         putmp_i,
                                                         putmp_i + iRowSizeU,
                                                         iColOffsetU,
                                                         pvtmp_j,
                                                         pvtmp_j + iColSizeV,
                                                         iRowOffsetV);
                         }
                    }
               }
          };

          template <util::MatTranspEvalFlag eTransp>
          struct MatMulExprLoop<eTransp, false, true, true, false> {
               template <typename MatA, typename MatU, typename MatV>
               static inline void InnerProduct(MatA& Atmp,
                                               const MatU& utmp,
                                               const MatV& vtmp,
                                               const index_type iNumRows,
                                               const index_type iNumCols,
                                               const index_type iRowSizeU,
                                               const index_type iRowOffsetU,
                                               const index_type iColOffsetU,
                                               const index_type iRowOffsetV,
                                               const index_type iColOffsetV,
                                               const index_type iColSizeV) {
                    MatMulExprHelper oMatMulHelper;
                    typedef MatEvalHelperTransp<eTransp> MatEvalType;

                    for (index_type j = 1; j <= iNumCols; ++j) {
                         const SpGradient* const pvtmp_j = vtmp.begin() + (j - 1) * iColOffsetV;

                         // Build dof map for each column and reuse it for all rows
                         oMatMulHelper.ResetDofStat();
                         oMatMulHelper.GetDofStat(pvtmp_j, pvtmp_j + iColSizeV, iRowOffsetV);
                         oMatMulHelper.ResetDofMap();
                         oMatMulHelper.InsertDof(pvtmp_j, pvtmp_j + iColSizeV, iRowOffsetV);
                         oMatMulHelper.InsertDone();

                         for (index_type i = 1; i <= iNumRows; ++i) {
                              const doublereal* const putmp_i = utmp.begin() + (i - 1) * iRowOffsetU;

                              oMatMulHelper.InnerProduct(MatEvalType::GetElem(Atmp, i, j),
                                                         putmp_i,
                                                         putmp_i + iRowSizeU,
                                                         iColOffsetU,
                                                         pvtmp_j,
                                                         pvtmp_j + iColSizeV,
                                                         iRowOffsetV);
                         }
                    }
               }
          };

          template <util::MatTranspEvalFlag eTransp>
          struct MatMulExprLoop<eTransp, false, false, true, false> {
               template <typename MatA, typename MatU, typename MatV>
               static inline void InnerProduct(MatA& Atmp,
                                               const MatU& utmp,
                                               const MatV& vtmp,
                                               const index_type iNumRows,
                                               const index_type iNumCols,
                                               const index_type iRowSizeU,
                                               const index_type iRowOffsetU,
                                               const index_type iColOffsetU,
                                               const index_type iRowOffsetV,
                                               const index_type iColOffsetV,
                                               const index_type iColSizeV) noexcept {

                    typedef util::MatEvalHelperTransp<eTransp> MatEvalType;

                    for (index_type j = 1; j <= iNumCols; ++j) {
                         const doublereal* const pvtmp_j = vtmp.begin() + (j - 1) * iColOffsetV;

                         for (index_type i = 1; i <= iNumRows; ++i) {
                              const doublereal* const putmp_i = utmp.begin() + (i - 1) * iRowOffsetU;

                              util::InnerProduct(MatEvalType::GetElem(Atmp, i, j),
                                                 putmp_i,
                                                 putmp_i + iRowSizeU,
                                                 iColOffsetU,
                                                 pvtmp_j,
                                                 pvtmp_j + iColSizeV,
                                                 iRowOffsetV);
                         }
                    }
               }
          };

          template <util::MatTranspEvalFlag eTransp, bool bIsGradientLhs, bool bIsGradientRhs>
          struct MatMulExprLoop<eTransp, bIsGradientLhs, bIsGradientRhs, false, false> {
               template <typename MatA, typename MatU, typename MatV>
               static inline void InnerProduct(MatA& Atmp,
                                               const MatU& utmp,
                                               const MatV& vtmp,
                                               const index_type iNumRows,
                                               const index_type iNumCols,
                                               const index_type iRowSizeU,
                                               const index_type iRowOffsetU,
                                               const index_type iColOffsetU,
                                               const index_type iRowOffsetV,
                                               const index_type iColOffsetV,
                                               const index_type iColSizeV) noexcept {

                    typedef util::MatEvalHelperTransp<eTransp> MatEvalType;

                    for (index_type j = 1; j <= iNumCols; ++j) {
                         const auto* const pvtmp_j = vtmp.begin() + (j - 1) * iColOffsetV;

                         for (index_type i = 1; i <= iNumRows; ++i) {
                              const auto* const putmp_i = utmp.begin() + (i - 1) * iRowOffsetU;

                              util::InnerProduct(MatEvalType::GetElem(Atmp, i, j),
                                                 putmp_i,
                                                 putmp_i + iRowSizeU,
                                                 iColOffsetU,
                                                 pvtmp_j,
                                                 pvtmp_j + iColSizeV,
                                                 iRowOffsetV);
                         }
                    }
               }
          };
     }

     template <typename LhsValue, typename RhsValue, typename LhsExpr, typename RhsExpr>
     template <util::MatTranspEvalFlag eTransp, SpGradCommon::ExprEvalFlags eCompr, typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
     void SpMatMulExpr<LhsValue, RhsValue, LhsExpr, RhsExpr>::Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
          typedef util::MatEvalHelperTransp<eTransp> MatEvalType;

          constexpr bool bTransposedEval = eTransp == util::MatTranspEvalFlag::TRANSPOSED;

          static_assert(NumRowsA == (!bTransposedEval ? iNumRowsStatic : iNumColsStatic), "Number of rows do not match");
          static_assert(NumColsA == (!bTransposedEval ? iNumColsStatic : iNumRowsStatic), "Number of columns do not match");

          SP_GRAD_ASSERT(!bHaveRefTo(A));

          MatEvalType::ResizeReset(A, iGetNumRows(), iGetNumCols(), 0);

          typedef typename util::remove_all<LhsExpr>::type LhsExprType;
          typedef typename util::remove_all<RhsExpr>::type RhsExprType;

          constexpr bool bLhsUsesIterators = (LhsExprType::uMatAccess & util::MatAccessFlag::ITERATORS) != 0;
          constexpr bool bRhsUsesIterators = (RhsExprType::uMatAccess & util::MatAccessFlag::ITERATORS) != 0;

          constexpr bool bUseTmpExprLhs = !(LhsExprType::iNumElemOps == 0 && bLhsUsesIterators);
          constexpr bool bUseTmpExprRhs = !(RhsExprType::iNumElemOps == 0 && bRhsUsesIterators);

          typedef util::TempExprHelper<LhsExpr, bUseTmpExprLhs> UTmpType;
          typedef util::TempExprHelper<RhsExpr, bUseTmpExprRhs> VTmpType;

          typename UTmpType::Type utmp{UTmpType::EvalUnique(u)};
          typename VTmpType::Type vtmp{VTmpType::EvalUnique(v)};

          static_assert(utmp.iNumElemOps == 0, "invalid temporary expression");
          static_assert(vtmp.iNumElemOps == 0, "invalid temporary expression");
          static_assert((utmp.uMatAccess & util::MatAccessFlag::ITERATORS) != 0, "iterators required for temporary expression");
          static_assert((vtmp.uMatAccess & util::MatAccessFlag::ITERATORS) != 0, "iterators required for temporary expression");

          const index_type iRowOffsetU = utmp.iGetRowOffset();
          const index_type iColOffsetU = utmp.iGetColOffset();
          const index_type iRowSizeU = utmp.iGetColOffset() * utmp.iGetNumCols();
          const index_type iRowOffsetV = vtmp.iGetRowOffset();
          const index_type iColOffsetV = vtmp.iGetColOffset();
          const index_type iColSizeV = vtmp.iGetRowOffset() * vtmp.iGetNumRows();

          constexpr bool bIsGradientLhs = std::is_same<SpGradient, LhsValue>::value;
          constexpr bool bIsGradientRhs = std::is_same<SpGradient, RhsValue>::value;
          constexpr bool bIsGradProdLhs = std::is_same<GpGradProd, LhsValue>::value;
          constexpr bool bIsGradProdRhs = std::is_same<GpGradProd, RhsValue>::value;
          constexpr bool bIsGradOrGradProdLhs = bIsGradientLhs || bIsGradProdLhs;
          constexpr bool bIsGradOrGradProdRhs = bIsGradientRhs || bIsGradProdRhs;
          constexpr bool bIsSparseRep = !(bIsGradProdLhs || bIsGradProdRhs);
          constexpr bool bSingleDofMap = bIsSparseRep && (bIsGradientLhs && bIsGradientRhs);

          static_assert(!(bIsGradientRhs && bIsGradProdLhs), "cannot mix forward mode and sparse mode in the same expression");
          static_assert(!(bIsGradProdRhs && bIsGradientLhs), "cannot mix forward mode and sparse mode in the same expression");

          typedef util::MatMulExprLoop<eTransp, bIsGradOrGradProdLhs, bIsGradOrGradProdRhs, bIsSparseRep, bSingleDofMap> MatMulExprLoop;

          MatMulExprLoop::InnerProduct(A,
                                       utmp,
                                       vtmp,
                                       iGetNumRows(),
                                       iGetNumCols(),
                                       iRowSizeU,
                                       iRowOffsetU,
                                       iColOffsetU,
                                       iRowOffsetV,
                                       iColOffsetV,
                                       iColSizeV);
     }

     template <typename LhsValue, typename RhsValue, typename LhsExpr, typename RhsExpr>
     template <util::MatTranspEvalFlag eTransp, SpGradCommon::ExprEvalFlags eCompr, typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
     void SpMatMulExpr<LhsValue, RhsValue, LhsExpr, RhsExpr>::Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A, const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
          constexpr bool bTransposedEval = eTransp == util::MatTranspEvalFlag::TRANSPOSED;

          typedef util::MatEvalHelperTransp<eTransp> MatEvalType;

          static_assert(NumRowsA == (!bTransposedEval ? iNumRowsStatic : iNumColsStatic), "Number of rows do not match");
          static_assert(NumColsA == (!bTransposedEval ? iNumColsStatic : iNumRowsStatic), "Number of columns do not match");

          SP_GRAD_ASSERT(!bHaveRefTo(A));

          MatEvalType::ResizeReset(A, iGetNumRows(), iGetNumCols(), 0);

          typedef typename util::remove_all<LhsExpr>::type LhsExprType;
          typedef typename util::remove_all<RhsExpr>::type RhsExprType;

          constexpr bool bLhsUsesIterators = (LhsExprType::uMatAccess & util::MatAccessFlag::ITERATORS) != 0;
          constexpr bool bRhsUsesIterators = (RhsExprType::uMatAccess & util::MatAccessFlag::ITERATORS) != 0;

          constexpr bool bUseTmpExprLhs = !(LhsExprType::iNumElemOps == 0 && bLhsUsesIterators);
          constexpr bool bUseTmpExprRhs = !(RhsExprType::iNumElemOps == 0 && bRhsUsesIterators);

          typedef util::TempExprHelper<LhsExpr, bUseTmpExprLhs> UTmpType;
          typedef util::TempExprHelper<RhsExpr, bUseTmpExprRhs> VTmpType;

          typename UTmpType::Type utmp{u, oDofMap}; // FIXME: should use oDofMap
          typename VTmpType::Type vtmp{v, oDofMap};

          static_assert(utmp.iNumElemOps == 0, "temporary expression must not involve additional operations");
          static_assert(vtmp.iNumElemOps == 0, "temporary expression must not involve additional operations");
          static_assert((utmp.uMatAccess & util::MatAccessFlag::ITERATORS) != 0, "iterators are required for temporary operations");
          static_assert((vtmp.uMatAccess & util::MatAccessFlag::ITERATORS) != 0, "iterators are required for temporary operations");

          const index_type iRowOffsetU = utmp.iGetRowOffset();
          const index_type iColOffsetU = utmp.iGetColOffset();
          const index_type iRowSizeU = utmp.iGetColOffset() * utmp.iGetNumCols();
          const index_type iRowOffsetV = vtmp.iGetRowOffset();
          const index_type iColOffsetV = vtmp.iGetColOffset();
          const index_type iColSizeV = vtmp.iGetRowOffset() * vtmp.iGetNumRows();

          constexpr bool bIsGradientLhs = std::is_same<SpGradient, LhsValue>::value;
          constexpr bool bIsGradientRhs = std::is_same<SpGradient, RhsValue>::value;
          constexpr bool bIsGradProdLhs = std::is_same<GpGradProd, LhsValue>::value;
          constexpr bool bIsGradProdRhs = std::is_same<GpGradProd, RhsValue>::value;

          static_assert(!(bIsGradientRhs && bIsGradProdLhs), "cannot mix forward mode and sparse mode in a single expression");
          static_assert(!(bIsGradProdRhs && bIsGradientLhs), "cannot mix forward mode and sparse mode in a single expression");

          typedef util::MatMulExprLoop2<eTransp> MatMulExprLoop2;

          MatMulExprLoop2::InnerProduct(A,
                                        utmp,
                                        vtmp,
                                        iGetNumRows(),
                                        iGetNumCols(),
                                        iRowSizeU,
                                        iRowOffsetU,
                                        iColOffsetU,
                                        iRowOffsetV,
                                        iColOffsetV,
                                        iColSizeV,
                                        oDofMap);
     }

     template <typename ValueType, typename ScalarExpr, index_type NumRows, index_type NumCols>
     void SpMatElemScalarExpr<ValueType, ScalarExpr, NumRows, NumCols>::InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
          SpGradientTraits<ValueType>::InsertDof(u, oExpDofMap);
     }

     template <typename ValueType, typename ScalarExpr, index_type NumRows, index_type NumCols>
     template <typename ValueTypeB>
     void SpMatElemScalarExpr<ValueType, ScalarExpr, NumRows, NumCols>::AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
          SpGradientTraits<ValueType>::AddDeriv(u, g, dCoef, oExpDofMap);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixBase<ValueType, NumRows, NumCols>::SpMatrixBase() {
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixBase<ValueType, NumRows, NumCols>::SpMatrixBase(index_type iNumRows, index_type iNumCols, index_type iNumDeriv) {
          ResizeReset(iNumRows, iNumCols, iNumDeriv);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixBase<ValueType, NumRows, NumCols>::SpMatrixBase(const SpMatrixBase& oMat) {
          *this = oMat;	// Enable inexpensive shadow copies of SpGradient
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename ValueTypeExpr, typename Expr>
     SpMatrixBase<ValueType, NumRows, NumCols>::SpMatrixBase(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr) {
          constexpr bool bThisIsGradient = std::is_same<ValueType, SpGradient>::value;
          constexpr bool bThisIsGradProd = std::is_same<ValueType, GpGradProd>::value;
          constexpr bool bThisIsDouble = std::is_same<ValueType, doublereal>::value;
          constexpr bool bExprIsGradient = std::is_same<ValueTypeExpr, SpGradient>::value;
          constexpr bool bExprIsGradProd = std::is_same<ValueTypeExpr, GpGradProd>::value;
          constexpr bool bExprIsDouble = std::is_same<ValueTypeExpr, doublereal>::value;
          static_assert(bThisIsGradient || bThisIsGradProd || bThisIsDouble, "data type not supported");
          static_assert(bExprIsGradient || bExprIsGradProd || bExprIsDouble, "data type not supported");
          static_assert(!((bThisIsGradient && bExprIsGradProd) || (bThisIsGradProd && bExprIsGradient)), "cannot mix forward mode and sparse mode in one expression");
          static_assert((bThisIsGradient || bThisIsGradProd) || !bExprIsGradient, "Cannot convert SpGradient to doublereal");

          oExpr.template Eval<util::MatTranspEvalFlag::DIRECT, Expr::eExprEvalFlags>(*this);

#ifdef SP_GRAD_DEBUG
          if (Expr::eExprEvalFlags == SpGradCommon::ExprEvalUnique) {
               for (const auto& a: *this) {
                    SP_GRAD_ASSERT(SpGradientTraits<ValueType>::bIsUnique(a));
               }
          }
#endif
          SP_GRAD_ASSERT(bValid());
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename ValueTypeExpr, typename Expr>
     SpMatrixBase<ValueType, NumRows, NumCols>::SpMatrixBase(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr,
                                                             const SpGradExpDofMapHelper<ValueType>& oDofMap) {
          constexpr bool bThisIsGradient = std::is_same<ValueType, SpGradient>::value;
          constexpr bool bThisIsGradProd = std::is_same<ValueType, GpGradProd>::value;
          constexpr bool bThisIsDouble = std::is_same<ValueType, doublereal>::value;
          constexpr bool bExprIsGradient = std::is_same<ValueTypeExpr, SpGradient>::value;
          constexpr bool bExprIsGradProd = std::is_same<ValueTypeExpr, GpGradProd>::value;
          constexpr bool bExprIsDouble = std::is_same<ValueTypeExpr, doublereal>::value;
          constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = bThisIsGradient ? SpGradCommon::ExprEvalUnique : SpGradCommon::ExprEvalDuplicate;
          static_assert(bThisIsGradient || bThisIsGradProd || bThisIsDouble, "invalid data type");
          static_assert(bExprIsGradient || bExprIsGradProd || bExprIsDouble, "invalid data type");
          static_assert(!((bThisIsGradient && bExprIsGradProd) || (bThisIsGradProd && bExprIsGradient)), "cannot mix forward mode and sparse mode in the expression");
          static_assert((bThisIsGradient || bThisIsGradProd) || !bExprIsGradient, "Cannot convert SpGradient to doublereal");

          oExpr.template Eval<util::MatTranspEvalFlag::DIRECT, eExprEvalFlags>(*this, oDofMap);

#ifdef SP_GRAD_DEBUG
          if (Expr::eExprEvalFlags == SpGradCommon::ExprEvalUnique) {
               for (const auto& a: *this) {
                    SP_GRAD_ASSERT(SpGradientTraits<ValueType>::bIsUnique(a));
               }
          }
#endif
          SP_GRAD_ASSERT(bValid());
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixBase<ValueType, NumRows, NumCols>::SpMatrixBase(index_type iNumRows, index_type iNumCols, const SpGradExpDofMapHelper<ValueType>& oDofMap)
          :SpMatrixBase(iNumRows, iNumCols, oDofMap.iGetLocalSize()) { // FIXME: check if it is more efficient without preallocation

          for (ValueType& a: *this) {
               oDofMap.InitDofMap(a);
          }

          SP_GRAD_ASSERT(bValid());
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename ValueTypeExpr, typename Expr>
     inline SpMatrixBase<ValueType, NumRows, NumCols>&
     SpMatrixBase<ValueType, NumRows, NumCols>::MapAssign(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr,
                                                          const SpGradExpDofMapHelper<ValueType>& oDofMap)
     {
          constexpr bool bThisIsGradient = std::is_same<ValueType, SpGradient>::value;
          constexpr bool bThisIsGradProd = std::is_same<ValueType, GpGradProd>::value;
          constexpr bool bThisIsDouble = std::is_same<ValueType, doublereal>::value;
          constexpr bool bExprIsGradient = std::is_same<ValueTypeExpr, SpGradient>::value;
          constexpr bool bExprIsGradProd = std::is_same<ValueTypeExpr, GpGradProd>::value;
          constexpr bool bExprIsDouble = std::is_same<ValueTypeExpr, doublereal>::value;
          constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = bThisIsGradient ? SpGradCommon::ExprEvalUnique : SpGradCommon::ExprEvalDuplicate;
          static_assert(bThisIsGradient || bThisIsGradProd || bThisIsDouble, "invalid data type");
          static_assert(bExprIsGradient || bExprIsGradProd || bExprIsDouble, "invalid data type");
          static_assert(!((bThisIsGradient && bExprIsGradProd) || (bThisIsGradProd && bExprIsGradient)), "cannot mix forward mode and sparse mode in the same expression");
          static_assert((bThisIsGradient || bThisIsGradProd) || !bExprIsGradient, "Cannot convert SpGradient to doublereal");

          oExpr.template Eval<util::MatTranspEvalFlag::DIRECT, eExprEvalFlags>(*this, oDofMap);

#ifdef SP_GRAD_DEBUG
          if (Expr::eExprEvalFlags == SpGradCommon::ExprEvalUnique) {
               for (const auto& a: *this) {
                    SP_GRAD_ASSERT(SpGradientTraits<ValueType>::bIsUnique(a));
               }
          }
#endif
          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixBase<ValueType, NumRows, NumCols>::SpMatrixBase(const std::initializer_list<ValueType>& rgValues)
          :SpMatrixBase(NumRows, NumCols, 0)
     {
          static_assert(NumRows != SpMatrixSize::DYNAMIC, "matrix size must be known at compile time");
          static_assert(NumCols != SpMatrixSize::DYNAMIC, "matrix size must be known at compile time");

          SP_GRAD_ASSERT(rgValues.size() == static_cast<size_t>(NumRows * NumCols));

          index_type iRow = 1, iCol = 1;

          for (const ValueType& aij: rgValues) {
               GetElem(iRow, iCol) = aij;

               // Insert values column wise like Mat3x3(a11, a21, a31, a12, a22, a32, ...)
               if (++iRow > NumRows) {
                    iRow = 1;
                    ++iCol;
               }
          }

          SP_GRAD_ASSERT(bValid());
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     void SpMatrixBase<ValueType, NumRows, NumCols>::ResizeReset(index_type iNumRows, index_type iNumCols, index_type iNumDeriv) {
          SP_GRAD_ASSERT(bValid());
          SP_GRAD_ASSERT(iNumRows == NumRows || NumRows == SpMatrixSize::DYNAMIC);
          SP_GRAD_ASSERT(iNumCols == NumCols || NumCols == SpMatrixSize::DYNAMIC);

          if (pData->bCheckSize(iNumRows, iNumCols, iNumDeriv)) {
               for (ValueType& g: *pData) {
                    SpGradientTraits<ValueType>::ResizeReset(g, 0, iNumDeriv);
               }
          } else {
               pData.Allocate(iNumRows, iNumCols, iNumDeriv);
          }

          SP_GRAD_ASSERT(bValid());
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     void SpMatrixBase<ValueType, NumRows, NumCols>::ResizeReset(index_type iNumRows, index_type iNumCols, const SpGradExpDofMapHelper<ValueType>& oDofMap) {
          SP_GRAD_ASSERT(bValid());

          ResizeReset(iNumRows, iNumCols, oDofMap.iGetLocalSize()); // FIXME: Check if it is more efficient without pre-allocation

          for (ValueType& g: *this) {
               oDofMap.InitDofMap(g);
          }

          SP_GRAD_ASSERT(bValid());
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixBase<ValueType, NumRows, NumCols>::SpMatrixBase(SpMatrixBase&& oMat) {
#ifdef SP_GRAD_DEBUG
          if (!(NumRows == SpMatrixSize::DYNAMIC || NumCols == SpMatrixSize::DYNAMIC)) {
               DEBUGCERR("Warning: move constructors with static memory allocation will not be most efficient!\n");
          }
#endif
          *this = std::move(oMat);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixBase<ValueType, NumRows, NumCols>& SpMatrixBase<ValueType, NumRows, NumCols>::operator=(const SpMatrixBase& oMat) {
          SP_GRAD_ASSERT(bValid());

          if (&oMat == this) {
               return *this;
          }

          ResizeReset(oMat.iGetNumRows(), oMat.iGetNumCols(), 0);

          const index_type iNumElem = pData->iGetNumElem();

          for (index_type i = 1; i <= iNumElem; ++i) {
               GetElem(i) = oMat.GetElem(i); // Enable inexpensive shadow copies of SpGradient
          }

          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixBase<ValueType, NumRows, NumCols>& SpMatrixBase<ValueType, NumRows, NumCols>::operator=(SpMatrixBase&& oMat) {
          SP_GRAD_ASSERT(bValid());
          SP_GRAD_ASSERT(oMat.bValid());

          pData = std::move(oMat.pData);

          SP_GRAD_ASSERT(oMat.bValid());
          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename ValueTypeExpr, typename Expr>
     SpMatrixBase<ValueType, NumRows, NumCols>& SpMatrixBase<ValueType, NumRows, NumCols>::operator=(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr) {
          SP_GRAD_ASSERT(bValid());

          constexpr bool bThisIsGradient = std::is_same<ValueType, SpGradient>::value;
          constexpr bool bThisIsGradProd = std::is_same<ValueType, GpGradProd>::value;
          constexpr bool bThisIsDouble = std::is_same<ValueType, doublereal>::value;
          constexpr bool bExprIsGradient = std::is_same<ValueTypeExpr, SpGradient>::value;
          constexpr bool bExprIsGradProd = std::is_same<ValueTypeExpr, GpGradProd>::value;
          constexpr bool bExprIsDouble = std::is_same<ValueTypeExpr, doublereal>::value;
          static_assert(bThisIsGradient || bThisIsGradProd || bThisIsDouble, "data type not supported");
          static_assert(bExprIsGradient || bExprIsGradProd || bExprIsDouble, "data type not supported");
          static_assert((bThisIsGradient || bThisIsGradProd) || !(bExprIsGradient || bExprIsGradProd), "Cannot convert SpGradient to doublereal in assignment");
          static_assert(!(bThisIsGradient && bExprIsGradProd), "cannot mix forward mode and sparse mode within the same expression");
          static_assert(!(bThisIsGradProd && bExprIsGradient), "cannot mix forward mode and sparse mode within the same expression");

          oExpr.template Eval<util::MatTranspEvalFlag::DIRECT, Expr::eExprEvalFlags>(*this);

#ifdef SP_GRAD_DEBUG
          if (Expr::eExprEvalFlags == SpGradCommon::ExprEvalUnique) {
               for (const auto& a: *this) {
                    SP_GRAD_ASSERT(SpGradientTraits<ValueType>::bIsUnique(a));
               }
          }
#endif
          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename Expr>
     SpMatrixBase<ValueType, NumRows, NumCols>& SpMatrixBase<ValueType, NumRows, NumCols>::operator*=(const SpGradBase<Expr>& b) {
          SP_GRAD_ASSERT(bValid());

          constexpr SpGradCommon::ExprEvalFlags eEvalFlags = util::remove_all<Expr>::type::eExprEvalFlags;
          constexpr bool bThisIsGradient = std::is_same<ValueType, SpGradient>::value;

          static_assert(bThisIsGradient, "Cannot convert SpGradient to doublereal");
          static_assert(!std::is_same<typename util::remove_all<Expr>::type, SpGradient>::value, "invalid data type");

          const SpMatElemScalarExpr<SpGradient, const SpGradient, NumRows, NumCols> btmp{b};

          btmp.template AssignEval<util::MatTranspEvalFlag::DIRECT, eEvalFlags, SpGradBinMult>(*this);

          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixBase<ValueType, NumRows, NumCols>& SpMatrixBase<ValueType, NumRows, NumCols>::operator*=(const SpGradient& b) {
          SP_GRAD_ASSERT(bValid());

          constexpr SpGradCommon::ExprEvalFlags eEvalFlags = SpGradient::eExprEvalFlags;
          constexpr bool bThisIsGradient = std::is_same<ValueType, SpGradient>::value;

          static_assert(bThisIsGradient, "Cannot convert SpGradient to doublereal");

          const SpMatElemScalarExpr<SpGradient, const SpGradient&, NumRows, NumCols> btmp{b};

          btmp.template AssignEval<util::MatTranspEvalFlag::DIRECT, eEvalFlags, SpGradBinMult>(*this);

          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixBase<ValueType, NumRows, NumCols>& SpMatrixBase<ValueType, NumRows, NumCols>::operator*=(const GpGradProd& b) {
          SP_GRAD_ASSERT(bValid());

          constexpr SpGradCommon::ExprEvalFlags eEvalFlags = GpGradProd::eExprEvalFlags;
          constexpr bool bThisIsGradient = std::is_same<ValueType, GpGradProd>::value;

          static_assert(bThisIsGradient, "Cannot convert GpGradProd to doublereal");

          const SpMatElemScalarExpr<GpGradProd, const GpGradProd&, NumRows, NumCols> btmp{b};

          btmp.template AssignEval<util::MatTranspEvalFlag::DIRECT, eEvalFlags, SpGradBinMult>(*this);

          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixBase<ValueType, NumRows, NumCols>& SpMatrixBase<ValueType, NumRows, NumCols>::operator*=(doublereal b) {
          SP_GRAD_ASSERT(bValid());

          for (auto& ai: *this) {
               ai *= b;
          }

          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename Expr>
     SpMatrixBase<ValueType, NumRows, NumCols>& SpMatrixBase<ValueType, NumRows, NumCols>::operator/=(const SpGradBase<Expr>& b) {
          SP_GRAD_ASSERT(bValid());

          constexpr SpGradCommon::ExprEvalFlags eEvalFlags = util::remove_all<Expr>::type::eExprEvalFlags;
          constexpr bool bThisIsGradient = std::is_same<ValueType, SpGradient>::value;

          static_assert(bThisIsGradient, "Cannot convert SpGradient to doublereal");
          static_assert(!std::is_same<typename util::remove_all<Expr>::type, SpGradient>::value, "wrong data type for this function");

          const SpMatElemScalarExpr<SpGradient, const SpGradient, NumRows, NumCols> btmp{b};

          btmp.template AssignEval<util::MatTranspEvalFlag::DIRECT, eEvalFlags, SpGradBinDiv>(*this);

          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixBase<ValueType, NumRows, NumCols>& SpMatrixBase<ValueType, NumRows, NumCols>::operator/=(const SpGradient& b) {
          SP_GRAD_ASSERT(bValid());

          constexpr SpGradCommon::ExprEvalFlags eEvalFlags = SpGradient::eExprEvalFlags;
          constexpr bool bThisIsGradient = std::is_same<ValueType, SpGradient>::value;
          static_assert(bThisIsGradient, "Cannot convert SpGradient to doublereal");

          const SpMatElemScalarExpr<SpGradient, const SpGradient&, NumRows, NumCols> btmp{b};

          btmp.template AssignEval<util::MatTranspEvalFlag::DIRECT, eEvalFlags, SpGradBinDiv>(*this);

          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixBase<ValueType, NumRows, NumCols>& SpMatrixBase<ValueType, NumRows, NumCols>::operator/=(const GpGradProd& b) {
          SP_GRAD_ASSERT(bValid());

          constexpr SpGradCommon::ExprEvalFlags eEvalFlags = GpGradProd::eExprEvalFlags;
          constexpr bool bThisIsGradient = std::is_same<ValueType, GpGradProd>::value;
          static_assert(bThisIsGradient, "Cannot convert GpGradProd to doublereal");

          const SpMatElemScalarExpr<GpGradProd, const GpGradProd&, NumRows, NumCols> btmp{b};

          btmp.template AssignEval<util::MatTranspEvalFlag::DIRECT, eEvalFlags, SpGradBinDiv>(*this);

          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixBase<ValueType, NumRows, NumCols>& SpMatrixBase<ValueType, NumRows, NumCols>::operator/=(doublereal b) {
          SP_GRAD_ASSERT(bValid());

          for (auto& ai: *this) {
               ai /= b;
          }

          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename ValueTypeExpr, typename Expr>
     SpMatrixBase<ValueType, NumRows, NumCols>& SpMatrixBase<ValueType, NumRows, NumCols>::operator+=(const SpMatElemExprBase<ValueTypeExpr, Expr>& b)
     {
          SP_GRAD_ASSERT(bValid());

          constexpr SpGradCommon::ExprEvalFlags eEvalFlags = util::remove_all<Expr>::type::eExprEvalFlags;
          constexpr bool bThisIsGradient = std::is_same<ValueType, SpGradient>::value;
          constexpr bool bThisIsGradProd = std::is_same<ValueType, GpGradProd>::value;
          constexpr bool bThisIsDouble = std::is_same<ValueType, doublereal>::value;
          constexpr bool bExprIsGradient = std::is_same<ValueTypeExpr, SpGradient>::value;
          constexpr bool bExprIsGradProd = std::is_same<ValueTypeExpr, GpGradProd>::value;
          constexpr bool bExprIsDouble = std::is_same<ValueTypeExpr, doublereal>::value;

          static_assert(!(bThisIsGradProd && bExprIsGradient), "cannot mix forward mode and sparse mode within the same expression");
          static_assert(!(bThisIsGradient && bExprIsGradProd), "cannot mix forward mode and sparse mode within the same expression");
          static_assert(bThisIsGradient || bThisIsGradProd || bThisIsDouble, "data type is not supported");
          static_assert(bExprIsGradient || bExprIsGradProd || bExprIsDouble, "data type is not supported");
          static_assert((bThisIsGradient || bThisIsGradProd) || !(bExprIsGradient || bExprIsGradProd), "Cannot convert SpGradient to doublereal");

          typedef typename util::remove_all<Expr>::type ExprType;
          static constexpr bool bUseTempExpr = !(ExprType::uMatAccess & util::MatAccessFlag::ELEMENT_WISE);
          typedef typename util::TempExprHelper<decltype(b), bUseTempExpr>::Type TempExpr;

          const TempExpr btmp{b};

          btmp.template AssignEval<util::MatTranspEvalFlag::DIRECT, eEvalFlags, SpGradBinPlus>(*this);

          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename ValueTypeExpr, typename Expr>
     SpMatrixBase<ValueType, NumRows, NumCols>& SpMatrixBase<ValueType, NumRows, NumCols>::Add(const SpMatElemExprBase<ValueTypeExpr, Expr>& b, const SpGradExpDofMapHelper<ValueType>& oDofMap)
     {
          SP_GRAD_ASSERT(bValid());

          constexpr bool bThisIsGradient = std::is_same<ValueType, SpGradient>::value;
          constexpr bool bThisIsGradProd = std::is_same<ValueType, GpGradProd>::value;
          constexpr bool bThisIsDouble = std::is_same<ValueType, doublereal>::value;
          constexpr bool bExprIsGradient = std::is_same<ValueTypeExpr, SpGradient>::value;
          constexpr bool bExprIsGradProd = std::is_same<ValueTypeExpr, GpGradProd>::value;
          constexpr bool bExprIsDouble = std::is_same<ValueTypeExpr, doublereal>::value;

          static_assert(!(bThisIsGradProd && bExprIsGradient), "cannot mix forward mode and sparse mode within the same expression");
          static_assert(!(bThisIsGradient && bExprIsGradProd), "cannot mix forward mode and sparse mode within the same expression");
          static_assert(bThisIsGradient || bThisIsGradProd || bThisIsDouble, "unsupported data type");
          static_assert(bExprIsGradient || bExprIsGradProd || bExprIsDouble, "unsupported data type");
          static_assert((bThisIsGradient || bThisIsGradProd) || !(bExprIsGradient || bExprIsGradProd), "Cannot convert SpGradient to doublereal");

          typedef typename util::remove_all<Expr>::type ExprType;
          static constexpr bool bUseTempExpr = !(ExprType::uMatAccess & util::MatAccessFlag::ELEMENT_WISE);
          typedef typename util::TempExprHelper<decltype(b), bUseTempExpr>::Type TempExpr;

          const TempExpr btmp{b, oDofMap};

          btmp.template AssignEval<util::MatTranspEvalFlag::DIRECT, SpGradCommon::ExprEvalFlags::ExprEvalUnique, SpGradBinPlus>(*this, oDofMap);

          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename ValueTypeExpr, typename Expr>
     SpMatrixBase<ValueType, NumRows, NumCols>& SpMatrixBase<ValueType, NumRows, NumCols>::Sub(const SpMatElemExprBase<ValueTypeExpr, Expr>& b, const SpGradExpDofMapHelper<ValueType>& oDofMap)
     {
          SP_GRAD_ASSERT(bValid());

          constexpr bool bThisIsGradient = std::is_same<ValueType, SpGradient>::value;
          constexpr bool bThisIsGradProd = std::is_same<ValueType, GpGradProd>::value;
          constexpr bool bThisIsDouble = std::is_same<ValueType, doublereal>::value;
          constexpr bool bExprIsGradient = std::is_same<ValueTypeExpr, SpGradient>::value;
          constexpr bool bExprIsGradProd = std::is_same<ValueTypeExpr, GpGradProd>::value;
          constexpr bool bExprIsDouble = std::is_same<ValueTypeExpr, doublereal>::value;

          static_assert(!(bThisIsGradProd && bExprIsGradient), "cannot mix forward mode and sparse mode");
          static_assert(!(bThisIsGradient && bExprIsGradProd), "cannot mix forward mode and sparse mode");
          static_assert(bThisIsGradient || bThisIsGradProd || bThisIsDouble, "data type not supported");
          static_assert(bExprIsGradient || bExprIsGradProd || bExprIsDouble, "data type not supported");
          static_assert((bThisIsGradient || bThisIsGradProd) || !(bExprIsGradient || bExprIsGradProd), "Cannot convert SpGradient to doublereal");

          typedef typename util::remove_all<Expr>::type ExprType;
          static constexpr bool bUseTempExpr = !(ExprType::uMatAccess & util::MatAccessFlag::ELEMENT_WISE);
          typedef typename util::TempExprHelper<decltype(b), bUseTempExpr>::Type TempExpr;

          const TempExpr btmp{b, oDofMap};

          btmp.template AssignEval<util::MatTranspEvalFlag::DIRECT, SpGradCommon::ExprEvalFlags::ExprEvalUnique, SpGradBinMinus>(*this, oDofMap);

          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename ValueTypeExpr, typename Expr>
     SpMatrixBase<ValueType, NumRows, NumCols>& SpMatrixBase<ValueType, NumRows, NumCols>::operator-=(const SpMatElemExprBase<ValueTypeExpr, Expr>& b)
     {
          SP_GRAD_ASSERT(bValid());

          constexpr SpGradCommon::ExprEvalFlags eEvalFlags = util::remove_all<Expr>::type::eExprEvalFlags;
          constexpr bool bThisIsGradient = std::is_same<ValueType, SpGradient>::value;
          constexpr bool bThisIsGradProd = std::is_same<ValueType, GpGradProd>::value;
          constexpr bool bThisIsDouble = std::is_same<ValueType, doublereal>::value;
          constexpr bool bExprIsGradient = std::is_same<ValueTypeExpr, SpGradient>::value;
          constexpr bool bExprIsGradProd = std::is_same<ValueTypeExpr, GpGradProd>::value;
          constexpr bool bExprIsDouble = std::is_same<ValueTypeExpr, doublereal>::value;

          static_assert(!(bThisIsGradProd && bExprIsGradient), "cannot mix forward mode and sparse mode in the same expression");
          static_assert(!(bThisIsGradient && bExprIsGradProd), "cannot mix forward mode and sparse mode in the same expression");
          static_assert(bThisIsGradient || bThisIsGradProd || bThisIsDouble, "data type is not supported");
          static_assert(bExprIsGradient || bExprIsGradProd || bExprIsDouble, "data type is not supported");
          static_assert((bThisIsGradient || bThisIsGradProd) || !(bExprIsGradient || bExprIsGradProd), "Cannot convert SpGradient to doublereal");

          typedef typename util::remove_all<Expr>::type ExprType;
          static constexpr bool bUseTempExpr = !(ExprType::uMatAccess & util::MatAccessFlag::ELEMENT_WISE);
          typedef typename util::TempExprHelper<decltype(b), bUseTempExpr>::Type TempExpr;

          const TempExpr btmp{b};

          btmp.template AssignEval<util::MatTranspEvalFlag::DIRECT, eEvalFlags, SpGradBinMinus>(*this);

          SP_GRAD_ASSERT(bValid());

          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrixBase<ValueType, NumRows, NumCols>::~SpMatrixBase() {
          SP_GRAD_ASSERT(bValid());
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     index_type SpMatrixBase<ValueType, NumRows, NumCols>::iGetNumRows() const {
          ASSERT(NumRows == SpMatrixSize::DYNAMIC || NumRows == pData->iGetNumRows());

          return pData->iGetNumRows();
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     index_type SpMatrixBase<ValueType, NumRows, NumCols>::iGetNumCols() const {
          ASSERT(NumCols == SpMatrixSize::DYNAMIC || NumCols == pData->iGetNumCols());

          return pData->iGetNumCols();
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     index_type SpMatrixBase<ValueType, NumRows, NumCols>::iGetSize(index_type i, index_type j) const {
          return SpGradientTraits<ValueType>::iGetSize(GetElem(i, j));
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     doublereal SpMatrixBase<ValueType, NumRows, NumCols>::dGetValue(index_type i, index_type j) const {
          return SpGradientTraits<ValueType>::dGetValue(GetElem(i, j));
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename ValueTypeB>
     void SpMatrixBase<ValueType, NumRows, NumCols>::InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type i, index_type j) const {
          SpGradientTraits<ValueTypeB>::InsertDeriv(GetElem(i, j), g, dCoef);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     void SpMatrixBase<ValueType, NumRows, NumCols>::GetDofStat(SpGradDofStat& s, index_type i, index_type j) const {
          SpGradientTraits<ValueType>::GetDofStat(GetElem(i, j), s);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     void SpMatrixBase<ValueType, NumRows, NumCols>::InsertDof(SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
          SpGradientTraits<ValueType>::InsertDof(GetElem(i, j), oExpDofMap);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename ValueTypeB>
     void SpMatrixBase<ValueType, NumRows, NumCols>::AddDeriv(ValueTypeB& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap, index_type i, index_type j) const {
          SpGradientTraits<ValueType>::AddDeriv(GetElem(i, j), g, dCoef, oExpDofMap);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     void SpMatrixBase<ValueType, NumRows, NumCols>::MakeUnique() {
          *this = EvalUnique(*this);
     }

#ifdef SP_GRAD_DEBUG
     template <typename ValueType, index_type NumRows, index_type NumCols>
     bool SpMatrixBase<ValueType, NumRows, NumCols>::bValid() const {
          const SpMatrixData<ValueType>& oData = *pData; // Do not account for fixed size matrices

          SP_GRAD_ASSERT(oData.iGetRefCnt() >= 1);
          SP_GRAD_ASSERT(oData.iGetNumRows() >= 0);
          SP_GRAD_ASSERT(oData.iGetNumCols() >= 0);
          SP_GRAD_ASSERT(oData.iGetMaxDeriv() >= 0);

          if (&oData != pGetNullData()) {
               SP_GRAD_ASSERT(NumRows == SpMatrixSize::DYNAMIC || NumRows == oData.iGetNumRows());
               SP_GRAD_ASSERT(NumCols == SpMatrixSize::DYNAMIC || NumCols == oData.iGetNumCols());
          } else {
               SP_GRAD_ASSERT(oData.iGetNumRows() == 0);
               SP_GRAD_ASSERT(oData.iGetNumCols() == 0);
               SP_GRAD_ASSERT(oData.iGetMaxDeriv() == 0);
          }

          for (const auto& ai: oData) {
               SP_GRAD_ASSERT(bValid(ai));
          }

          return true;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     bool SpMatrixBase<ValueType, NumRows, NumCols>::bValid(const SpGradient& g) {
          return g.bValid();
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     bool SpMatrixBase<ValueType, NumRows, NumCols>::bValid(const GpGradProd& g) {
          return g.bValid();
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     bool SpMatrixBase<ValueType, NumRows, NumCols>::bValid(doublereal d) {
          return std::isfinite(d);
     }
#endif

#ifdef SP_GRAD_DEBUG
     namespace util {
          template <typename ValueType, typename ValueTypeExpr, typename Expr, index_type NumRows, index_type NumCols>
          struct MatrixBaseRefHelper {
               static constexpr bool bHaveRefTo(const SpMatrixBase<ValueType, NumRows, NumCols>& A, const SpMatElemExprBase<ValueTypeExpr, Expr>& B) {
                    return false;
               }
          };

          template <typename ValueType, index_type NumRows, index_type NumCols>
          struct MatrixBaseRefHelper<ValueType, ValueType, SpMatrixBase<ValueType, NumRows, NumCols>, NumRows, NumCols> {
               static constexpr bool bHaveRefTo(const SpMatrixBase<ValueType, NumRows, NumCols>& A, const SpMatElemExprBase<ValueType, SpMatrixBase<ValueType, NumRows, NumCols> >& B) {
                    return A.bHaveSameRep(*B.pGetRep());
               }
          };
     }
#endif

     template <typename ValueType, typename ScalarExpr, index_type NumRows, index_type NumCols>
     constexpr SpMatElemScalarExpr<ValueType, ScalarExpr, NumRows, NumCols>::SpMatElemScalarExpr(const ScalarExpr& u) noexcept
          :u(u) {

     }

     template <typename ValueType, typename ScalarExpr, index_type NumRows, index_type NumCols>
     constexpr doublereal SpMatElemScalarExpr<ValueType, ScalarExpr, NumRows, NumCols>::dGetValue(index_type, index_type) const noexcept {
          return SpGradientTraits<ScalarExprType>::dGetValue(u);
     }

     template <typename ValueType, typename ScalarExpr, index_type NumRows, index_type NumCols>
     constexpr index_type SpMatElemScalarExpr<ValueType, ScalarExpr, NumRows, NumCols>::iGetSize(index_type, index_type) const noexcept {
          return SpGradientTraits<ScalarExprType>::iGetSize(u);
     }

     template <typename ValueType, typename ScalarExpr, index_type NumRows, index_type NumCols>
     constexpr index_type SpMatElemScalarExpr<ValueType, ScalarExpr, NumRows, NumCols>::iGetNumRows() noexcept {
          return iNumRowsStatic;
     }

     template <typename ValueType, typename ScalarExpr, index_type NumRows, index_type NumCols>
     constexpr index_type SpMatElemScalarExpr<ValueType, ScalarExpr, NumRows, NumCols>::iGetNumCols() noexcept {
          return iNumColsStatic;
     }

     template <typename ValueType, typename ScalarExpr, index_type NumRows, index_type NumCols>
     constexpr index_type SpMatElemScalarExpr<ValueType, ScalarExpr, NumRows, NumCols>::iGetMaxSize() const noexcept {
          return SpGradientTraits<ValueType>::iGetSize(u);
     }

     template <typename ValueType, typename ScalarExpr, index_type NumRows, index_type NumCols>
     template <typename ValueTypeB>
     void SpMatElemScalarExpr<ValueType, ScalarExpr, NumRows, NumCols>::InsertDeriv(ValueTypeB& g, doublereal dCoef, index_type, index_type) const {
          SpGradientTraits<ValueTypeB>::InsertDeriv(u, g, dCoef);
     }

     template <typename ValueType, typename ScalarExpr, index_type NumRows, index_type NumCols>
     void SpMatElemScalarExpr<ValueType, ScalarExpr, NumRows, NumCols>::GetDofStat(SpGradDofStat& s, index_type, index_type) const noexcept {
          SpGradientTraits<ScalarExprType>::GetDofStat(u, s);
     }

#ifdef SP_GRAD_DEBUG
     template <typename ValueType, typename ScalarExpr, index_type NumRows, index_type NumCols>
     template <typename ExprTypeB, typename ExprB>
     constexpr bool SpMatElemScalarExpr<ValueType, ScalarExpr, NumRows, NumCols>::bHaveRefTo(const SpMatElemExprBase<ExprTypeB, ExprB>& A) const noexcept {
          return SpGradientTraits<ValueType>::bHaveRefTo(u, *A.pGetRep());
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename ExprType, typename Expr>
     constexpr bool SpMatrixBase<ValueType, NumRows, NumCols>::bHaveRefTo(const SpMatElemExprBase<ExprType, Expr>& A) const noexcept {
          return util::MatrixBaseRefHelper<ValueType, ExprType, Expr, NumRows, NumCols>::bHaveRefTo(*this, A);
     }
#endif

     template <typename ValueType, index_type NumRows, index_type NumCols>
     ValueType* SpMatrixBase<ValueType, NumRows, NumCols>::begin() {
          return pData->begin();
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     ValueType* SpMatrixBase<ValueType, NumRows, NumCols>::end() {
          return pData->end();
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     const ValueType* SpMatrixBase<ValueType, NumRows, NumCols>::begin() const {
          return pData->begin();
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     const ValueType* SpMatrixBase<ValueType, NumRows, NumCols>::end() const {
          return pData->end();
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     bool SpMatrixBase<ValueType, NumRows, NumCols>::bHaveSameRep(const SpMatrixBase<ValueType, NumRows, NumCols>& A) const noexcept {
          return pData == A.pData && pData != pGetNullData();
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     bool SpMatrixBase<ValueType, NumRows, NumCols>::bIsOwnerOf(const SpMatrixData<ValueType>* pDataB) const noexcept {
          return pData == pDataB;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     const ValueType& SpMatrixBase<ValueType, NumRows, NumCols>::GetElem(index_type i) const {
          SP_GRAD_ASSERT(pData != pGetNullData());

          return *pData->pGetData(i);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     ValueType& SpMatrixBase<ValueType, NumRows, NumCols>::GetElem(index_type i) {
          SP_GRAD_ASSERT(pData != pGetNullData());

          return *pData->pGetData(i);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     const ValueType& SpMatrixBase<ValueType, NumRows, NumCols>::GetElem(index_type i, index_type j) const {
          SP_GRAD_ASSERT(pData != pGetNullData());

          return *pData->pGetData(i, j);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     ValueType& SpMatrixBase<ValueType, NumRows, NumCols>::GetElem(index_type i, index_type j) {
          SP_GRAD_ASSERT(pData != pGetNullData());

          return *pData->pGetData(i, j);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     typename SpMatrixBase<ValueType, NumRows, NumCols>::SpMatrixDataType*
     SpMatrixBase<ValueType, NumRows, NumCols>::pGetNullData() {
          return static_cast<SpMatrixDataType*>(SpMatrixData<ValueType>::pGetNullData());
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <util::MatTranspEvalFlag eTransp, SpGradCommon::ExprEvalFlags eCompr, typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
     void SpMatrixBase<ValueType, NumRows, NumCols>::Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
          this->template ElemEval<eTransp, eCompr>(A);
     }


     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <util::MatTranspEvalFlag eTransp, SpGradCommon::ExprEvalFlags eCompr, typename ValueTypeA, index_type NumRowsA, index_type NumColsA>
     void SpMatrixBase<ValueType, NumRows, NumCols>::Eval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A, const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
          this->template ElemEval<eTransp, eCompr>(A, oDofMap);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <util::MatTranspEvalFlag eTransp,
               SpGradCommon::ExprEvalFlags eCompr,
               typename Func,
               typename ValueTypeA,
               index_type NumRowsA, index_type NumColsA>
     void SpMatrixBase<ValueType, NumRows, NumCols>::AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A) const {
          SP_GRAD_ASSERT(bValid());
          SP_GRAD_ASSERT(A.bValid());

          this->template ElemAssign<eTransp, eCompr, Func>(A);

          SP_GRAD_ASSERT(bValid());
          SP_GRAD_ASSERT(A.bValid());
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <util::MatTranspEvalFlag eTransp,
               SpGradCommon::ExprEvalFlags eCompr,
               typename Func,
               typename ValueTypeA,
               index_type NumRowsA, index_type NumColsA>
     void SpMatrixBase<ValueType, NumRows, NumCols>::AssignEval(SpMatrixBase<ValueTypeA, NumRowsA, NumColsA>& A,
                                                                const SpGradExpDofMapHelper<ValueTypeA>& oDofMap) const {
          SP_GRAD_ASSERT(bValid());
          SP_GRAD_ASSERT(A.bValid());

          this->template ElemAssign<eTransp, eCompr, Func>(A, oDofMap);

          SP_GRAD_ASSERT(bValid());
          SP_GRAD_ASSERT(A.bValid());
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     index_type SpMatrixBase<ValueType, NumRows, NumCols>::iGetMaxSize() const {
          return this->iGetMaxSizeElem();
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrix<ValueType, NumRows, NumCols>::SpMatrix(index_type iNumRows, index_type iNumCols, index_type iNumDeriv)
          :SpMatrixBase<ValueType, NumRows, NumCols>(iNumRows, iNumCols, iNumDeriv) {
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     ValueType& SpMatrix<ValueType, NumRows, NumCols>::operator()(index_type iRow, index_type iCol) {
          return this->GetElem(iRow, iCol);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     inline const ValueType& SpMatrix<ValueType, NumRows, NumCols>::operator() (index_type iRow, index_type iCol) const {
          return this->GetElem(iRow, iCol);
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename ValueTypeExpr, typename Expr>
     SpMatrix<ValueType, NumRows, NumCols>::SpMatrix(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr)
          :SpMatrixBase<ValueType, NumRows, NumCols>(oExpr) {
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename ValueTypeExpr, typename Expr>
     SpMatrix<ValueType, NumRows, NumCols>::SpMatrix(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr, const SpGradExpDofMapHelper<ValueType>& oDofMap)
          :SpMatrixBase<ValueType, NumRows, NumCols>(oExpr, oDofMap) {
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrix<ValueType, NumRows, NumCols>::SpMatrix(const std::initializer_list<ValueType>& rgValues)
          :SpMatrixBase<ValueType, NumRows, NumCols>(rgValues) {
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     template <typename ValueTypeExpr, typename Expr>
     SpMatrix<ValueType, NumRows, NumCols>& SpMatrix<ValueType, NumRows, NumCols>::operator=(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr) {
          SpMatrixBase<ValueType, NumRows, NumCols>::operator=(oExpr);
          return *this;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     SpMatrix<ValueType, NumRows, NumCols>& SpMatrix<ValueType, NumRows, NumCols>::operator=(const Mat3x3& oMat) {
          SpMatrixBase<ValueType, NumRows, NumCols>::operator=(oMat);
          return *this;
     }

     template <typename ValueType, index_type NumRows>
     SpColVector<ValueType, NumRows>::SpColVector(index_type iNumRows, index_type iNumDeriv)
          :SpMatrixBase<ValueType, NumRows, 1>(iNumRows, 1, iNumDeriv) {
     }

     template <typename ValueType, index_type NumRows>
     template <typename ValueTypeExpr, typename Expr>
     SpColVector<ValueType, NumRows>::SpColVector(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr)
          :SpMatrixBase<ValueType, NumRows, 1>(oExpr) {
     }

     template <typename ValueType, index_type NumRows>
     template <typename ValueTypeExpr, typename Expr>
     SpColVector<ValueType, NumRows>::SpColVector(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr, const SpGradExpDofMapHelper<ValueType>& oDofMap)
          :SpMatrixBase<ValueType, NumRows, 1>(oExpr, oDofMap) {
     }

     template <typename ValueType, index_type NumRows>
     SpColVector<ValueType, NumRows>::SpColVector(const std::initializer_list<ValueType>& rgValues)
          :SpMatrixBase<ValueType, NumRows, 1>(rgValues) {
     }

     template <typename ValueType, index_type NumRows>
     void SpColVector<ValueType, NumRows>::ResizeReset(index_type iNumRows, index_type iNumDeriv) {
          SpMatrixBase<ValueType, NumRows, 1>::ResizeReset(iNumRows, 1, iNumDeriv);
     }

     template <typename ValueType, index_type NumRows>
     void SpColVector<ValueType, NumRows>::ResizeReset(index_type iNumRows, const SpGradExpDofMapHelper<ValueType>& oDofMap) {
          SpMatrixBase<ValueType, NumRows, 1>::ResizeReset(iNumRows, 1, oDofMap);
     }

     template <typename ValueType, index_type NumRows>
     ValueType& SpColVector<ValueType, NumRows>::operator()(index_type iRow) {
          return this->GetElem(iRow);
     }

     template <typename ValueType, index_type NumRows>
     const ValueType& SpColVector<ValueType, NumRows>::operator() (index_type iRow) const {
          return this->GetElem(iRow);
     }

     template <typename ValueType, index_type NumRows>
     template <typename ValueTypeExpr, typename Expr>
     SpColVector<ValueType, NumRows>& SpColVector<ValueType, NumRows>::operator=(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr) {
          SpMatrixBase<ValueType, NumRows, 1>::operator=(oExpr);
          return *this;
     }

     template <typename ValueType, index_type NumRows>
     SpColVector<ValueType, NumRows>& SpColVector<ValueType, NumRows>::operator=(const Vec3& oVec) {
          SpMatrixBase<ValueType, NumRows, 1>::operator=(oVec);
          return *this;
     }

     template <typename ValueType, index_type NumCols>
     SpRowVector<ValueType, NumCols>::SpRowVector(index_type iNumCols, index_type iNumDeriv)
          :SpMatrixBase<ValueType, 1, NumCols>(1, iNumCols, iNumDeriv) {
     }

     template <typename ValueType, index_type NumCols>
     template <typename ValueTypeExpr, typename Expr>
     SpRowVector<ValueType, NumCols>::SpRowVector(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr)
          :SpMatrixBase<ValueType, 1, NumCols>(oExpr) {
     }

     template <typename ValueType, index_type NumCols>
     template <typename ValueTypeExpr, typename Expr>
     SpRowVector<ValueType, NumCols>::SpRowVector(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr, const SpGradExpDofMapHelper<ValueType>& oDofMap)
          :SpMatrixBase<ValueType, 1, NumCols>(oExpr, oDofMap) {
     }

     template <typename ValueType, index_type NumCols>
     SpRowVector<ValueType, NumCols>::SpRowVector(const std::initializer_list<ValueType>& rgValues)
          :SpMatrixBase<ValueType, 1, NumCols>(rgValues) {
     }

     template <typename ValueType, index_type NumCols>
     void SpRowVector<ValueType, NumCols>::ResizeReset(index_type iNumCols, index_type iNumDeriv) {
          SpMatrixBase<ValueType, 1, NumCols>::ResizeReset(1, iNumCols, iNumDeriv);
     }

     template <typename ValueType, index_type NumCols>
     void SpRowVector<ValueType, NumCols>::ResizeReset(index_type iNumCols, const SpGradExpDofMapHelper<ValueType>& oDofMap) {
          SpMatrixBase<ValueType, 1, NumCols>::ResizeReset(1, iNumCols, oDofMap);
     }

     template <typename ValueType, index_type NumCols>
     ValueType& SpRowVector<ValueType, NumCols>::operator()(index_type iCol) {
          return this->GetElem(iCol);
     }

     template <typename ValueType, index_type NumCols>
     const ValueType& SpRowVector<ValueType, NumCols>::operator() (index_type iCol) const {
          return this->GetElem(iCol);
     }

     template <typename ValueType, index_type NumCols>
     template <typename ValueTypeExpr, typename Expr>
     SpRowVector<ValueType, NumCols>& SpRowVector<ValueType, NumCols>::operator=(const SpMatElemExprBase<ValueTypeExpr, Expr>& oExpr) {
          SpMatrixBase<ValueType, 1, NumCols>::operator=(oExpr);
          return *this;
     }

     template <typename LhsValue, typename RhsValue, typename LhsExpr, typename RhsExpr>
     inline constexpr
     SpMatElemBinExpr<typename util::ResultType<LhsValue, RhsValue>::Type,
                      SpGradBinPlus,
                      const SpMatElemExprBase<LhsValue, LhsExpr>&,
                      const SpMatElemExprBase<RhsValue, RhsExpr>&>
     operator+(const SpMatElemExprBase<LhsValue, LhsExpr>& A,
               const SpMatElemExprBase<RhsValue, RhsExpr>& B) noexcept {
          return decltype(operator+(A, B))(A, B);
     }

     template <typename LhsValue, typename RhsValue, typename LhsExpr, typename RhsExpr>
     inline constexpr
     SpMatElemBinExpr<typename util::ResultType<LhsValue, RhsValue>::Type,
                      SpGradBinMinus,
                      const SpMatElemExprBase<LhsValue, LhsExpr>&,
                      const SpMatElemExprBase<RhsValue, RhsExpr>&>
     operator-(const SpMatElemExprBase<LhsValue, LhsExpr>& A,
               const SpMatElemExprBase<RhsValue, RhsExpr>& B) noexcept {
          return decltype(operator-(A, B))(A, B);
     }

     template <typename Value, typename Expr>
     inline constexpr
     SpMatElemUnaryExpr<Value,
                        SpGradUnaryMinus,
                        const SpMatElemExprBase<Value, Expr>& >
     operator-(const SpMatElemExprBase<Value, Expr>& A) noexcept {
          return decltype(operator-(A))(A);
     }

     template <typename LhsValue, typename RhsValue, typename LhsExpr, typename RhsExpr>
     inline constexpr
     SpMatCrossExpr<typename util::ResultType<LhsValue, RhsValue>::Type,
                    const SpMatElemExprBase<LhsValue, LhsExpr>&,
                    const SpMatElemExprBase<RhsValue, RhsExpr>&>
     Cross(const SpMatElemExprBase<LhsValue, LhsExpr>& A,
           const SpMatElemExprBase<RhsValue, RhsExpr>& B) noexcept {
          return decltype(Cross(A, B))(A, B);
     }

     template <typename LhsValue, typename RhsValue, typename LhsExpr, typename RhsExpr>
     inline constexpr
     SpMatCrossExpr<typename util::ResultType<LhsValue, RhsValue>::Type,
                    const SpMatElemExprBase<LhsValue, LhsExpr>&,
                    const SpMatElemExprBase<RhsValue, RhsExpr>&>
     Cross(const SpMatElemExprBase<LhsValue, LhsExpr>& A,
           const SpMatElemExprBase<RhsValue, RhsExpr>& B,
           const SpGradExpDofMapHelper<typename util::ResultType<LhsValue, RhsValue>::Type>& oDofMap) noexcept {
          return decltype(Cross(A, B))(A, B, oDofMap);
     }

     template <typename Value, typename Expr>
     inline constexpr
     SpMatElemTranspExpr<Value, const SpMatElemExprBase<Value, Expr>&>
     Transpose(const SpMatElemExprBase<Value, Expr>& A) noexcept {
          return decltype(Transpose(A)){A};
     }

     template <typename Value, typename Expr>
     inline constexpr
     const SpMatElemExprBase<Value, Expr>&
     Transpose(const SpMatElemTranspExpr<Value, const SpMatElemExprBase<Value, Expr>&>& A) noexcept {
          return decltype(Transpose(A))(A.Transpose());
     }

     template <typename Value, typename Expr>
     inline constexpr
     SpMatElemUniqueExpr<Value, const SpMatElemExprBase<Value, Expr>&>
     EvalUnique(const SpMatElemExprBase<Value, Expr>& A) noexcept {
          return decltype(EvalUnique(A)){A};
     }

     template <typename LhsValue, typename LhsExpr>
     inline constexpr
     SpMatElemBinExpr<typename util::ResultType<LhsValue, SpGradient>::Type,
                      SpGradBinMult,
                      const SpMatElemExprBase<LhsValue, LhsExpr>&,
                      const SpMatElemScalarExpr<SpGradient, const SpGradient&> >
     operator*(const SpMatElemExprBase<LhsValue, LhsExpr>& A,
               const SpGradient& b) noexcept {
          return decltype(operator*(A, b)){A, SpMatElemScalarExpr<SpGradient, const SpGradient&>{b}};
     }

     template <typename LhsValue, typename LhsExpr>
     inline constexpr
     SpMatElemBinExpr<typename util::ResultType<LhsValue, GpGradProd>::Type,
                      SpGradBinMult,
                      const SpMatElemExprBase<LhsValue, LhsExpr>&,
                      const SpMatElemScalarExpr<GpGradProd, const GpGradProd&> >
     operator*(const SpMatElemExprBase<LhsValue, LhsExpr>& A,
               const GpGradProd& b) noexcept {
          return decltype(operator*(A, b)){A, SpMatElemScalarExpr<GpGradProd, const GpGradProd&>{b}};
     }

     template <typename LhsValue, typename LhsExpr, typename RhsExpr>
     inline constexpr
     SpMatElemBinExpr<typename util::ResultType<LhsValue, SpGradient>::Type,
                      SpGradBinMult,
                      const SpMatElemExprBase<LhsValue, LhsExpr>&,
                      const SpMatElemScalarExpr<SpGradient, SpGradient> >
     operator*(const SpMatElemExprBase<LhsValue, LhsExpr>& A,
               const SpGradBase<RhsExpr>& b) noexcept {
          static_assert(!std::is_same<typename util::remove_all<RhsExpr>::type, SpGradient>::value, "not the right operator for this data type");
          return decltype(operator*(A, b)){A, SpMatElemScalarExpr<SpGradient, SpGradient>{SpGradient{EvalUnique(b)}}}; // Avoid multiple evaluations of b!
     }

     template <typename LhsValue, typename LhsExpr>
     inline constexpr
     SpMatElemBinExpr<typename util::ResultType<LhsValue, doublereal>::Type,
                      SpGradBinMult,
                      const SpMatElemExprBase<LhsValue, LhsExpr>&,
                      const SpMatElemScalarExpr<doublereal, doublereal> >
     operator*(const SpMatElemExprBase<LhsValue, LhsExpr>& A,
               const doublereal b) noexcept {
          return decltype(operator*(A, b)){A, SpMatElemScalarExpr<doublereal, doublereal>{b}};
     }

     template <typename RhsValue, typename RhsExpr>
     inline constexpr
     SpMatElemBinExpr<typename util::ResultType<SpGradient, RhsValue>::Type,
                      SpGradBinMult,
                      const SpMatElemScalarExpr<SpGradient, const SpGradient&>,
                      const SpMatElemExprBase<RhsValue, RhsExpr>&>
     operator*(const SpGradient& a,
               const SpMatElemExprBase<RhsValue, RhsExpr>& B) noexcept {
          return decltype(operator*(a, B)){SpMatElemScalarExpr<SpGradient, const SpGradient&>{a}, B};
     }

     template <typename LhsExpr, typename RhsValue, typename RhsExpr>
     inline constexpr
     SpMatElemBinExpr<typename util::ResultType<SpGradient, RhsValue>::Type,
                      SpGradBinMult,
                      const SpMatElemScalarExpr<SpGradient, SpGradient>,
                      const SpMatElemExprBase<RhsValue, RhsExpr>&>
     operator*(const SpGradBase<LhsExpr>& a,
               const SpMatElemExprBase<RhsValue, RhsExpr>& B) noexcept {
          static_assert(!std::is_same<typename util::remove_all<LhsExpr>::type, SpGradient>::value, "not the right operator for this data type");
          return decltype(operator*(a, B)){SpMatElemScalarExpr<SpGradient, SpGradient>{SpGradient{EvalUnique(a)}}, B}; // Avoid multiple evaluations of a!
     }

     template <typename RhsValue, typename RhsExpr>
     inline constexpr
     SpMatElemBinExpr<typename util::ResultType<doublereal, RhsValue>::Type,
                      SpGradBinMult,
                      const SpMatElemScalarExpr<doublereal, doublereal>,
                      const SpMatElemExprBase<RhsValue, RhsExpr>&>
     operator*(doublereal a,
               const SpMatElemExprBase<RhsValue, RhsExpr>& B) noexcept {
          return decltype(operator*(a, B)){SpMatElemScalarExpr<doublereal, doublereal>{a}, B};
     }

     template <typename LhsValue, typename LhsExpr>
     inline constexpr
     SpMatElemBinExpr<typename util::ResultType<LhsValue, SpGradient>::Type,
                      SpGradBinDiv,
                      const SpMatElemExprBase<LhsValue, LhsExpr>&,
                      const SpMatElemScalarExpr<SpGradient, const SpGradient&> >
     operator/(const SpMatElemExprBase<LhsValue, LhsExpr>& A,
               const SpGradient& b) noexcept {
          return decltype(operator/(A, b)){A, SpMatElemScalarExpr<SpGradient, const SpGradient&>{b}};
     }

     template <typename LhsValue, typename LhsExpr>
     inline constexpr
     SpMatElemBinExpr<typename util::ResultType<LhsValue, GpGradProd>::Type,
                      SpGradBinDiv,
                      const SpMatElemExprBase<LhsValue, LhsExpr>&,
                      const SpMatElemScalarExpr<GpGradProd, const GpGradProd&> >
     operator/(const SpMatElemExprBase<LhsValue, LhsExpr>& A,
               const GpGradProd& b) noexcept {
          return decltype(operator/(A, b)){A, SpMatElemScalarExpr<GpGradProd, const GpGradProd&>{b}};
     }

     template <typename LhsValue, typename LhsExpr, typename RhsExpr>
     inline constexpr
     SpMatElemBinExpr<typename util::ResultType<LhsValue, SpGradient>::Type,
                      SpGradBinDiv,
                      const SpMatElemExprBase<LhsValue, LhsExpr>&,
                      const SpMatElemScalarExpr<SpGradient, SpGradient> >
     operator/(const SpMatElemExprBase<LhsValue, LhsExpr>& A,
               const SpGradBase<RhsExpr>& b) noexcept {
          return decltype(operator/(A, b)){A, SpMatElemScalarExpr<SpGradient, SpGradient>{SpGradient{EvalUnique(b)}}}; // Avoid multiple evaluations of b!
     }

     template <typename LhsValue, typename LhsExpr>
     inline constexpr
     SpMatElemBinExpr<typename util::ResultType<LhsValue, doublereal>::Type,
                      SpGradBinDiv,
                      const SpMatElemExprBase<LhsValue, LhsExpr>&,
                      const SpMatElemScalarExpr<doublereal, doublereal> >
     operator/(const SpMatElemExprBase<LhsValue, LhsExpr>& A,
               const doublereal b) noexcept {
          return decltype(operator/(A, b)){A, SpMatElemScalarExpr<doublereal, doublereal>{b}};
     }

     template <typename LhsValue, typename RhsValue, typename LhsExpr, typename RhsExpr>
     inline constexpr
     SpMatMulExpr<LhsValue,
                  RhsValue,
                  const SpMatElemExprBase<LhsValue, LhsExpr>&,
                  const SpMatElemExprBase<RhsValue, RhsExpr>&>
     operator*(const SpMatElemExprBase<LhsValue, LhsExpr>& A,
               const SpMatElemExprBase<RhsValue, RhsExpr>& B) noexcept {
          return decltype(operator*(A, B)){A, B};
     }

     template <typename LhsValue, typename RhsValue, typename LhsExpr, typename RhsExpr>
     inline
     typename util::ResultType<LhsValue, RhsValue>::Type
     Dot(const SpMatElemExprBase<LhsValue, LhsExpr>& u, const SpMatElemExprBase<RhsValue, RhsExpr>& v) {
          typedef SpMatElemExprBase<LhsValue, LhsExpr> uType;
          typedef SpMatElemExprBase<RhsValue, RhsExpr> vType;
          static_assert(uType::iNumRowsStatic == vType::iNumRowsStatic, "number of rows does not match");
          static_assert(uType::iNumColsStatic == 1, "u must be a column vector");
          static_assert(vType::iNumColsStatic == 1, "v must be a column vector");
          SP_GRAD_ASSERT(u.iGetNumRows() == v.iGetNumRows());

          typedef const SpMatElemExprBase<LhsValue, LhsExpr>& LhsTmpExpr;
          typedef const SpMatElemExprBase<RhsValue, RhsExpr>& RhsTmpExpr;
          typedef typename util::remove_all<LhsTmpExpr>::type LhsExprType;
          typedef typename util::remove_all<RhsTmpExpr>::type RhsExprType;

          constexpr bool bLhsUsesIterators = (LhsExprType::uMatAccess & util::MatAccessFlag::ITERATORS) != 0;
          constexpr bool bRhsUsesIterators = (RhsExprType::uMatAccess & util::MatAccessFlag::ITERATORS) != 0;

          constexpr bool bUseTmpExprLhs = !(LhsExprType::iNumElemOps == 0 && bLhsUsesIterators);
          constexpr bool bUseTmpExprRhs = !(RhsExprType::iNumElemOps == 0 && bRhsUsesIterators);

          typedef util::TempExprHelper<LhsTmpExpr, bUseTmpExprLhs> UTmpType;
          typedef util::TempExprHelper<RhsTmpExpr, bUseTmpExprRhs> VTmpType;

          typename UTmpType::Type utmp{UTmpType::EvalUnique(u)};
          typename VTmpType::Type vtmp{VTmpType::EvalUnique(v)};

          static_assert(utmp.iNumElemOps == 0, "temporary expression must not involve additional operations");
          static_assert(vtmp.iNumElemOps == 0, "temporary expression must not involve additional operations");
          static_assert((utmp.uMatAccess & util::MatAccessFlag::ITERATORS) != 0, "support for iterators is required");
          static_assert((vtmp.uMatAccess & util::MatAccessFlag::ITERATORS) != 0, "support for iterators is required");

          const index_type iRowOffsetU = utmp.iGetRowOffset();
          const index_type iColSizeU = utmp.iGetRowOffset() * utmp.iGetNumRows();
          const index_type iRowOffsetV = vtmp.iGetRowOffset();
          const index_type iColSizeV = vtmp.iGetNumRows() * iRowOffsetV;

          typename util::ResultType<LhsValue, RhsValue>::Type a;

          util::MapInnerProduct(a,
                                utmp.begin(),
                                utmp.begin() + iColSizeU,
                                iRowOffsetU,
                                vtmp.begin(),
                                vtmp.begin() + iColSizeV,
                                iRowOffsetV);

          return a;
     }

     template <typename LhsValue, typename RhsValue, typename LhsExpr, typename RhsExpr>
     inline
     typename util::ResultType<LhsValue, RhsValue>::Type
     Dot(const SpMatElemExprBase<LhsValue, LhsExpr>& u, const SpMatElemExprBase<RhsValue, RhsExpr>& v, const SpGradExpDofMapHelper<typename util::ResultType<LhsValue, RhsValue>::Type>& oDofMap) {
          typedef SpMatElemExprBase<LhsValue, LhsExpr> uType;
          typedef SpMatElemExprBase<RhsValue, RhsExpr> vType;
          static_assert(uType::iNumRowsStatic == vType::iNumRowsStatic, "number of rows does not match");
          static_assert(uType::iNumColsStatic == 1, "u must be a column vector");
          static_assert(vType::iNumColsStatic == 1, "v must be a column vector");
          SP_GRAD_ASSERT(u.iGetNumRows() == v.iGetNumRows());

          typedef const SpMatElemExprBase<LhsValue, LhsExpr>& LhsTmpExpr;
          typedef const SpMatElemExprBase<RhsValue, RhsExpr>& RhsTmpExpr;
          typedef typename util::remove_all<LhsTmpExpr>::type LhsExprType;
          typedef typename util::remove_all<RhsTmpExpr>::type RhsExprType;

          constexpr bool bLhsUsesIterators = (LhsExprType::uMatAccess & util::MatAccessFlag::ITERATORS) != 0;
          constexpr bool bRhsUsesIterators = (RhsExprType::uMatAccess & util::MatAccessFlag::ITERATORS) != 0;

          constexpr bool bUseTmpExprLhs = !(LhsExprType::iNumElemOps == 0 && bLhsUsesIterators);
          constexpr bool bUseTmpExprRhs = !(RhsExprType::iNumElemOps == 0 && bRhsUsesIterators);

          typedef util::TempExprHelper<LhsTmpExpr, bUseTmpExprLhs> UTmpType;
          typedef util::TempExprHelper<RhsTmpExpr, bUseTmpExprRhs> VTmpType;

          typename UTmpType::Type utmp{UTmpType::EvalUnique(u), oDofMap};
          typename VTmpType::Type vtmp{VTmpType::EvalUnique(v), oDofMap};

          static_assert(utmp.iNumElemOps == 0, "temporary expression must not involve additional operations");
          static_assert(vtmp.iNumElemOps == 0, "temporary expression must not involve additional operations");
          static_assert((utmp.uMatAccess & util::MatAccessFlag::ITERATORS) != 0, "iterators are required for temporary expressions");
          static_assert((vtmp.uMatAccess & util::MatAccessFlag::ITERATORS) != 0, "iterators are required for temporary expressions");

          const index_type iRowOffsetU = utmp.iGetRowOffset();
          const index_type iColSizeU = utmp.iGetRowOffset() * utmp.iGetNumRows();
          const index_type iRowOffsetV = vtmp.iGetRowOffset();
          const index_type iColSizeV = vtmp.iGetNumRows() * iRowOffsetV;

          typename util::ResultType<LhsValue, RhsValue>::Type a;
          typedef util::InnerProductHelper<LhsValue, RhsValue> IPH;

          IPH::MapEval(a,
                       utmp.begin(),
                       utmp.begin() + iColSizeU,
                       iRowOffsetU,
                       vtmp.begin(),
                       vtmp.begin() + iColSizeV,
                       iRowOffsetV,
                       oDofMap);

          return a;
     }

     template <typename Value, typename Expr>
     inline constexpr Value Norm(const SpMatElemExprBase<Value, Expr>& u) {
          using std::sqrt;
          return sqrt(Dot(u, u));
     }

     template <typename Value, typename Expr>
     inline constexpr Value Norm(const SpMatElemExprBase<Value, Expr>& u, const SpGradExpDofMapHelper<Value>& oDofMap) {
          using std::sqrt;
          return oDofMap.MapEval(sqrt(Dot(u, u, oDofMap)));
     }

     template <typename ValueType, typename Expr>
     inline constexpr SpSubMatDynExpr<ValueType, const SpMatElemExprBase<ValueType, Expr>&>
     SubMatrix(const SpMatElemExprBase<ValueType, Expr>& A, index_type iRowStart, index_type iRowStep, index_type iNumRows, index_type iColStart, index_type iColStep, index_type iNumCols) {
          return decltype(SubMatrix(A, iRowStart, iRowStep, iNumRows, iColStart, iColStep, iNumCols))(A, iRowStart, iRowStep, iNumRows, iColStart, iColStep, iNumCols);
     }

     template <index_type iRowStart, index_type iRowStep, index_type iNumRows, index_type iColStart, index_type iColStep, index_type iNumCols, typename ValueType, typename Expr>
     inline constexpr SpSubMatStatExpr<ValueType, const SpMatElemExprBase<ValueType, Expr>&, iRowStart, iRowStep, iNumRows, iColStart, iColStep, iNumCols>
     SubMatrix(const SpMatElemExprBase<ValueType, Expr>& A) {
          return decltype(SubMatrix<iRowStart, iRowStep, iNumRows, iColStart, iColStep, iNumCols>(A))(A);
     }

     template <index_type iNumRows, index_type iNumCols, typename ValueType, typename Expr>
     inline constexpr SpSubMatStatResExpr<ValueType, const SpMatElemExprBase<ValueType, Expr>&, iNumRows, iNumCols>
     SubMatrix(const SpMatElemExprBase<ValueType, Expr>& A, index_type iRowStart, index_type iRowStep, index_type iColStart, index_type iColStep) {
          return decltype(SubMatrix<iNumRows, iNumCols>(A, iRowStart, iRowStep, iColStart, iColStep))(A, iRowStart, iRowStep, iColStart, iColStep);
     }

     template <index_type iRowStart, index_type iRowStep, index_type iNumRows, typename ValueType, typename Expr>
     inline constexpr SpSubMatStatRowExpr<ValueType, const SpMatElemExprBase<ValueType, Expr>&, iRowStart, iRowStep, iNumRows>
     SubMatrix(const SpMatElemExprBase<ValueType, Expr>& A, index_type iColStart, index_type iColStep, index_type iNumCols) {
          return decltype(SubMatrix<iRowStart, iRowStep, iNumRows>(A, iColStart, iColStep, iNumCols))(A, iColStart, iColStep, iNumCols);
     }

     template <index_type iRowStart, index_type iRowStep, index_type iNumRows, typename ValueType, typename Expr>
     inline constexpr SpSubMatStatExpr<ValueType, const SpMatElemExprBase<ValueType, Expr>&, iRowStart, iRowStep, iNumRows, 1, 1, 1>
     SubColVector(const SpMatElemExprBase<ValueType, Expr>& A) {
          static_assert(SpMatElemExprBase<ValueType, Expr>::iNumColsStatic == 1, "A must be a column vector");
          return decltype(SubColVector<iRowStart, iRowStep, iNumRows>(A))(A);
     }

     template <index_type iColStart, index_type iColStep, index_type iNumCols, typename ValueType, typename Expr>
     inline constexpr SpSubMatStatExpr<ValueType, const SpMatElemExprBase<ValueType, Expr>&, 1, 1, 1, iColStart, iColStep, iNumCols>
     SubRowVector(const SpMatElemExprBase<ValueType, Expr>& A) {
          static_assert(SpMatElemExprBase<ValueType, Expr>::iNumRowsStatic == 1, "A must be a row vector");
          return decltype(SubRowVector<iColStart, iColStep, iNumCols>(A))(A);
     }

     template <typename ValueType>
     void MatGVec(const SpColVector<ValueType, 3>& g, SpMatrix<ValueType, 3, 3>& G, const SpGradExpDofMapHelper<ValueType>& oDofMap) {
          ValueType d;

          oDofMap.MapAssign(d, (4./(4.+Dot(g, g))));

          G(1, 1) = d;
          oDofMap.MapAssign(G(2, 1), g(3) * d / 2.);
          oDofMap.MapAssign(G(3, 1), -g(2) * d / 2.);
          oDofMap.MapAssign(G(1, 2), -g(3) * d / 2.);
          G(2, 2) = d;
          oDofMap.MapAssign(G(3, 2), g(1) * d / 2.);
          oDofMap.MapAssign(G(1, 3), g(2) * d / 2.);
          oDofMap.MapAssign(G(2, 3), -g(1) * d / 2.);
          G(3, 3) = d;
     }

     template <typename ValueType>
     void MatGVec(const SpColVector<ValueType, 3>& g, SpMatrix<ValueType, 3, 3>& G) {
          SpGradExpDofMapHelper<ValueType> oDofMap;

          oDofMap.GetDofStat(g);
          oDofMap.Reset();
          oDofMap.InsertDof(g);
          oDofMap.InsertDone();

          MatGVec(g, G, oDofMap);
     }

     template <typename ValueType>
     SpMatrix<ValueType, 3, 3> MatGVec(const SpColVector<ValueType, 3>& g) {
          SpMatrix<ValueType, 3, 3> G(3, 3, 0);

          MatGVec(g, G);

          return G;
     }

     template <typename ValueType>
     SpMatrix<ValueType, 3, 3> MatGVec(const SpColVector<ValueType, 3>& g, const SpGradExpDofMapHelper<ValueType>& oDofMap) {
          SpMatrix<ValueType, 3, 3> G(3, 3, 0);

          MatGVec(g, G, oDofMap);

          return G;
     }

     template <typename ValueType>
     inline void
     MatRVec(const SpColVector<ValueType, 3>& g, SpMatrix<ValueType, 3, 3>& RDelta, const SpGradExpDofMapHelper<ValueType>& oDofMap) {
          ValueType d, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6;

          oDofMap.MapAssign(d, 4. / (4. + g(1) * g(1) + g(2) * g(2) + g(3) * g(3)));
          oDofMap.MapAssign(tmp1, -g(3) * g(3));
          oDofMap.MapAssign(tmp2, -g(2) * g(2));
          oDofMap.MapAssign(tmp3, -g(1) * g(1));
          oDofMap.MapAssign(tmp4, g(1) * g(2) * 0.5);
          oDofMap.MapAssign(tmp5, g(2) * g(3) * 0.5);
          oDofMap.MapAssign(tmp6, g(1) * g(3) * 0.5);

          oDofMap.MapAssign(RDelta(1,1), (tmp1 + tmp2) * d * 0.5 + 1);
          oDofMap.MapAssign(RDelta(1,2), (tmp4 - g(3)) * d);
          oDofMap.MapAssign(RDelta(1,3), (tmp6 + g(2)) * d);
          oDofMap.MapAssign(RDelta(2,1), (g(3) + tmp4) * d);
          oDofMap.MapAssign(RDelta(2,2), (tmp1 + tmp3) * d * 0.5 + 1.);
          oDofMap.MapAssign(RDelta(2,3), (tmp5 - g(1)) * d);
          oDofMap.MapAssign(RDelta(3,1), (tmp6 - g(2)) * d);
          oDofMap.MapAssign(RDelta(3,2), (tmp5 + g(1)) * d);
          oDofMap.MapAssign(RDelta(3,3), (tmp2 + tmp3) * d * 0.5 + 1.);
     }

     template <typename ValueType>
     inline void
     MatRVec(const SpColVector<ValueType, 3>& g, SpMatrix<ValueType, 3, 3>& RDelta) {
          SpGradExpDofMapHelper<ValueType> oDofMap;

          oDofMap.GetDofStat(g);
          oDofMap.Reset();
          oDofMap.InsertDof(g);
          oDofMap.InsertDone();

          MatRVec(g, RDelta, oDofMap);
     }

     template <typename ValueType>
     inline SpMatrix<ValueType, 3, 3>
     MatRVec(const SpColVector<ValueType, 3>& g) {
          SpMatrix<ValueType, 3, 3> RDelta(3, 3, 0);

          MatRVec(g, RDelta);

          return RDelta;
     }

     template <typename ValueType>
     inline SpMatrix<ValueType, 3, 3> MatRotVec(const SpColVector<ValueType, 3>& p) {
          constexpr index_type cid = RotCoeff::COEFF_B;
          using std::sqrt;
          using std::sin;
          using std::cos;

          ValueType phip[10];
          ValueType phi2(Dot(p, p));
          ValueType pd(sqrt(phi2));
          ValueType cf[RotCoeff::COEFF_B];
          index_type k, j;

          if (pd < RotCoeff::SerThrsh[cid-1]) {
               SpGradientTraits<ValueType>::ResizeReset(phip[0], 1., SpGradientTraits<ValueType>::iGetSize(phi2));
               for (j = 1; j <= 9; j++) {
                    phip[j] = phip[j-1]*phi2;
               }
               for (k = 0; k < cid; k++) {
                    SpGradientTraits<ValueType>::ResizeReset(cf[k], 0., 0);
                    for (j = 0; j < RotCoeff::SerTrunc[k]; j++) {
                         cf[k] += phip[j]/RotCoeff::SerCoeff[k][j];
                    }
               }
          } else {
               cf[0] = sin(pd) / pd;                 // a = sin(phi)/phi
               cf[1]=(1. - cos(pd)) / phi2;           // b = (1.-cos(phi))/phi2
          }

          SpMatrix<ValueType, 3, 3> R(3, 3, p.iGetMaxSize());

          R(1,1) = cf[1]*((-p(3)*p(3))-p(2)*p(2))+1.;
          R(1,2) = cf[1]*p(1)*p(2)-cf[0]*p(3);
          R(1,3) = cf[1]*p(1)*p(3)+cf[0]*p(2);
          R(2,1) = cf[0]*p(3)+cf[1]*p(1)*p(2);
          R(2,2) = cf[1]*((-p(3)*p(3))-p(1)*p(1))+1.;
          R(2,3) = cf[1]*p(2)*p(3)-cf[0]*p(1);
          R(3,1) = cf[1]*p(1)*p(3)-cf[0]*p(2);
          R(3,2) = cf[1]*p(2)*p(3)+cf[0]*p(1);
          R(3,3) = cf[1]*((-p(2)*p(2))-p(1)*p(1))+1.;

          return R;
     }

     template <typename ValueType>
     inline ValueType RotCo(const ValueType& phi, const SpGradExpDofMapHelper<ValueType>& oDofMap) {
          // This algorithm is a simplified version of RotCo in RotCoeff.hc
          // from Marco Morandini  <morandini@aero.polimi.it>
          // and Teodoro Merlini  <merlini@aero.polimi.it>
          using std::sin;
          using std::cos;
          using std::sqrt;
          using std::fabs;

          constexpr index_type N = 10;

          if (fabs(phi) < RotCoeff::SerThrsh[0]) {
               ValueType phi2;

               oDofMap.MapAssign(phi2, phi * phi);

               std::array<ValueType, N> phip;

               SpGradientTraits<ValueType>::ResizeReset(phip[0], 1., 0);

               index_type iSize_phip = 0;

               for (index_type j = 1; j <= N - 1; j++) {
                    oDofMap.MapAssign(phip[j], phip[j - 1] * phi2);
                    iSize_phip += SpGradientTraits<ValueType>::iGetSize(phip[j]);
               }

               ValueType cf;

               SpGradientTraits<ValueType>::ResizeReset(cf, 0., iSize_phip);

               for (index_type j = 0; j < RotCoeff::SerTrunc[0]; j++) {
                    cf += phip[j] / RotCoeff::SerCoeff[0][j];
               }

               oDofMap.MapAssign(cf, cf);

               return cf;
          }

          ValueType pd;

          oDofMap.MapAssign(pd, sin(phi) / phi);

          return pd;
     }

     template <typename ValueType>
     inline SpColVector<ValueType, 3>
     VecRotMat(const SpMatrix<ValueType, 3, 3>& R) {
          SpGradExpDofMapHelper<ValueType> oDofMap;

          oDofMap.GetDofStat(R);
          oDofMap.Reset();
          oDofMap.InsertDof(R);
          oDofMap.InsertDone();

          // Modified from Appendix 2.4 of
          //
          // author = {Marco Borri and Lorenzo Trainelli and Carlo L. Bottasso},
          // title = {On Representations and Parameterizations of Motion},
          // journal = {Multibody System Dynamics},
          // volume = {4},
          // pages = {129--193},
          // year = {2000}

          SpColVector<ValueType, 3> unit(3, 0);

          using std::atan2;
          using std::sqrt;

          ValueType cosphi;

          oDofMap.MapAssign(cosphi, 0.5 * (R(1, 1) + R(2, 2) + R(3, 3) - 1.));

          if (cosphi > 0.) {
               oDofMap.MapAssign(unit(1), 0.5*(R(3, 2) - R(2, 3)));
               oDofMap.MapAssign(unit(2), 0.5*(R(1, 3) - R(3, 1)));
               oDofMap.MapAssign(unit(3), 0.5*(R(2, 1) - R(1, 2)));

               ValueType sinphi2;

               oDofMap.MapAssign(sinphi2, unit(1) * unit(1) + unit(2) * unit(2) + unit(3) * unit(3));

               ValueType sinphi;

               if (sinphi2 != 0) {
                    oDofMap.MapAssign(sinphi, sqrt(sinphi2));
               } else {
                    sinphi = unit(1);
               }

               ValueType phi;

               oDofMap.MapAssign(phi, atan2(sinphi, cosphi));

               const ValueType RotCoPhi = RotCo(phi, oDofMap);

               for (index_type i = 1; i <= 3; ++i) {
                    oDofMap.MapAssign(unit(i), unit(i) / RotCoPhi);
               }
          } else {
               // -1 <= cosphi <= 0
               SpMatrix<ValueType, 3, 3> eet = (R + Transpose(R)) * 0.5;
               oDofMap.MapAssign(eet(1, 1), eet(1, 1) - cosphi);
               oDofMap.MapAssign(eet(2, 2), eet(2, 2) - cosphi);
               oDofMap.MapAssign(eet(3, 3), eet(3, 3) - cosphi);
               // largest (abs) component of unit vector phi/|phi|
               index_type maxcol = 1;
               if (eet(2, 2) > eet(1, 1)) {
                    maxcol = 2;
               }
               if (eet(3, 3) > eet(maxcol, maxcol)) {
                    maxcol = 3;
               }

               ValueType d;

               oDofMap.MapAssign(d, sqrt(eet(maxcol, maxcol) * (1. - cosphi)));

               for (index_type i = 1; i <= 3; ++i) {
                    oDofMap.MapAssign(unit(i), eet(i, maxcol) / d);
               }

               ValueType sinphi;

               SpGradientTraits<ValueType>::ResizeReset(sinphi, 0., 9 * (R.iGetMaxSize() + unit.iGetMaxSize()));

               for (index_type i = 1; i <= 3; ++i) {
                    SpColVector<ValueType, 1> xi = SubMatrix<1, 1>(Cross(unit, R.GetCol(i)), i, 1, 1, 1);
                    sinphi -= xi(1) * 0.5;
               }

               ValueType phi;
               oDofMap.MapAssign(phi, atan2(sinphi, cosphi));

               for (index_type i = 1; i <= 3; ++i) {
                    oDofMap.MapAssign(unit(i), unit(i) * phi);
               }
          }

          return unit;
     }

     template <typename ValueType>
     inline SpMatrix<ValueType, 3, 3>
     MatCrossVec(const SpColVector<ValueType, 3>& v, doublereal d) {
          SpMatrix<ValueType, 3, 3> A(3, 3, v.iGetMaxSize());

          SpGradientTraits<ValueType>::ResizeReset(A(1, 1), d, 0);
          SpGradientTraits<ValueType>::ResizeReset(A(2, 2), d, 0);
          SpGradientTraits<ValueType>::ResizeReset(A(3, 3), d, 0);

          A(1, 2) = -v(3);
          A(1, 3) =  v(2);
          A(2, 1) =  v(3);
          A(2, 3) = -v(1);
          A(3, 1) = -v(2);
          A(3, 2) =  v(1);

          return A;
     }

     template <typename T>
     inline T Det(const SpMatrix<T, 2, 2>& A) {
          return A(1, 1) * A(2, 2) - A(1, 2) * A(2, 1);
     }

     template <typename T>
     inline void Det(const SpMatrix<T, 2, 2>& A, T& detA, const SpGradExpDofMapHelper<T>& oDofMap) {
          oDofMap.MapAssign(detA, A(1, 1) * A(2, 2) - A(1, 2) * A(2, 1));
     }

     template <typename T>
     inline void Inv(const SpMatrix<T, 2, 2>& A, SpMatrix<T, 2, 2>& invA, T& detA) {
          SpGradExpDofMapHelper<T> oDofMap;
          oDofMap.GetDofStat(A);
          oDofMap.Reset();
          oDofMap.InsertDof(A);
          oDofMap.InsertDone();

          Inv(A, invA, detA, oDofMap);
     }

     template <typename T>
     inline void Inv(const SpMatrix<T, 2, 2>& A, SpMatrix<T, 2, 2>& invA, T& detA, const SpGradExpDofMapHelper<T>& oDofMap) {
          Det(A, detA, oDofMap);

          oDofMap.MapAssign(invA(1, 1), A(2, 2) / detA);
          oDofMap.MapAssign(invA(2, 1), -A(2, 1) / detA);
          oDofMap.MapAssign(invA(1, 2), -A(1, 2) / detA);
          oDofMap.MapAssign(invA(2, 2), A(1, 1) / detA);
     }

     template <typename T>
     inline void InvSymm(const SpMatrix<T, 2, 2>& A, SpMatrix<T, 2, 2>& invA, T& detA) {
          SpGradExpDofMapHelper<T> oDofMap;
          oDofMap.GetDofStat(A);
          oDofMap.Reset();
          oDofMap.InsertDof(A);
          oDofMap.InsertDone();

          InvSymm(A, invA, detA, oDofMap);
     }

     template <typename T>
     inline void InvSymm(const SpMatrix<T, 2, 2>& A, SpMatrix<T, 2, 2>& invA, T& detA, const SpGradExpDofMapHelper<T>& oDofMap) {
          Det(A, detA, oDofMap);

          oDofMap.MapAssign(invA(1, 1), A(2, 2) / detA);
          oDofMap.MapAssign(invA(2, 1), -A(2, 1) / detA);
          oDofMap.MapAssign(invA(2, 2), A(1, 1) / detA);
          invA(1, 2) = invA(2, 1);
     }

     template <typename T>
     inline SpMatrix<T, 2, 2> Inv(const SpMatrix<T, 2, 2>& A) {
          const T detA = Det(A);

          return SpMatrix<T, 2, 2>{A(2, 2) / detA,
                    -A(2, 1) / detA,
                    -A(1, 2) / detA,
                    A(1, 1) / detA};
     }

     template <typename T>
     inline T Det(const SpMatrix<T, 3, 3>& A) {
          return A(1,1)*(A(2,2)*A(3,3)-A(2,3)*A(3,2))-A(1,2)*(A(2,1)*A(3,3)-A(2,3)*A(3,1))+A(1,3)*(A(2,1)*A(3,2)-A(2,2)*A(3,1));
     }

     template <typename T>
     inline void Det(const SpMatrix<T, 3, 3>& A, T& detA, const SpGradExpDofMapHelper<T>& oDofMap) {
          oDofMap.MapAssign(detA, A(1,1)*(A(2,2)*A(3,3)-A(2,3)*A(3,2))-A(1,2)*(A(2,1)*A(3,3)-A(2,3)*A(3,1))+A(1,3)*(A(2,1)*A(3,2)-A(2,2)*A(3,1)));
     }

     template <typename T>
     inline void Inv(const SpMatrix<T, 3, 3>& A, SpMatrix<T, 3, 3>& invA, T& detA) {
          SpGradExpDofMapHelper<T> oDofMap;

          oDofMap.GetDofStat(A);
          oDofMap.Reset();
          oDofMap.InsertDof(A);
          oDofMap.InsertDone();

          Inv(A, invA, detA, oDofMap);
     }

     template <typename T>
     inline void Inv(const SpMatrix<T, 3, 3>& A, SpMatrix<T, 3, 3>& invA, T& detA, const SpGradExpDofMapHelper<T>& oDofMap) {
          Det(A, detA, oDofMap);

          oDofMap.MapAssign(invA(1, 1),  (A(2, 2) * A(3, 3) - A(2, 3) * A(3, 2)) / detA);
          oDofMap.MapAssign(invA(1, 2), -(A(1, 2) * A(3, 3) - A(1, 3) * A(3, 2)) / detA);
          oDofMap.MapAssign(invA(1, 3),  (A(1, 2) * A(2, 3) - A(1, 3) * A(2, 2)) / detA);
          oDofMap.MapAssign(invA(2, 1), -(A(2, 1) * A(3, 3) - A(2, 3) * A(3, 1)) / detA);
          oDofMap.MapAssign(invA(2, 2),  (A(1, 1) * A(3, 3) - A(1, 3) * A(3, 1)) / detA);
          oDofMap.MapAssign(invA(2, 3), -(A(1, 1) * A(2, 3) - A(1, 3) * A(2, 1)) / detA);
          oDofMap.MapAssign(invA(3, 1),  (A(2, 1) * A(3, 2) - A(2, 2) * A(3, 1)) / detA);
          oDofMap.MapAssign(invA(3, 2), -(A(1, 1) * A(3, 2) - A(1, 2) * A(3, 1)) / detA);
          oDofMap.MapAssign(invA(3, 3),  (A(1, 1) * A(2, 2) - A(1, 2) * A(2, 1)) / detA);
     }

     template <typename T>
     inline void InvSymm(const SpMatrix<T, 3, 3>& A, SpMatrix<T, 3, 3>& invA, T& detA) {
          SpGradExpDofMapHelper<T> oDofMap;

          oDofMap.GetDofStat(A);
          oDofMap.Reset();
          oDofMap.InsertDof(A);
          oDofMap.InsertDone();

          InvSymm(A, invA, detA, oDofMap);
     }

     template <typename T>
     inline void InvSymm(const SpMatrix<T, 3, 3>& A, SpMatrix<T, 3, 3>& invA, T& detA, const SpGradExpDofMapHelper<T>& oDofMap) {

          T a11, a12, a13, a22, a23, a33;

          oDofMap.MapAssign(a11, A(3, 3) * A(2, 2) - A(2, 3) * A(2, 3));
          oDofMap.MapAssign(a12, A(1, 3) * A(2, 3) - A(3, 3) * A(1, 2));
          oDofMap.MapAssign(a13, A(1, 2) * A(2, 3) - A(1, 3) * A(2, 2));
          oDofMap.MapAssign(a22, A(3, 3) * A(1, 1) - A(1, 3) * A(1, 3));
          oDofMap.MapAssign(a23, A(1, 2) * A(1, 3) - A(1, 1) * A(2, 3));
          oDofMap.MapAssign(a33, A(1, 1) * A(2, 2) - A(1, 2) * A(1, 2));

          oDofMap.MapAssign(detA, (A(1, 1) * a11) + (A(1, 2) * a12) + (A(1, 3) * a13));

          oDofMap.MapAssign(invA(1, 1), a11 / detA);
          oDofMap.MapAssign(invA(1, 2), a12 / detA);
          oDofMap.MapAssign(invA(1, 3), a13 / detA);
          oDofMap.MapAssign(invA(2, 2), a22 / detA);
          oDofMap.MapAssign(invA(2, 3), a23 / detA);
          oDofMap.MapAssign(invA(3, 3), a33 / detA);

          invA(2, 1) = invA(1, 2);
          invA(3, 1) = invA(1, 3);
          invA(3, 2) = invA(2, 3);
     }

     template <typename T>
     inline SpMatrix<T, 3, 3> Inv(const SpMatrix<T, 3, 3>& A) {
          SpMatrix<T, 3, 3> invA(3, 3, 0);

          T detA;

          Inv(A, invA, detA);

          return invA;
     }

     template <typename ValueType, index_type NumRows, index_type NumCols>
     inline std::ostream& operator<<(std::ostream& os, const SpMatrixBase<ValueType, NumRows, NumCols>& A) {
          for (index_type i = 1; i <= A.iGetNumRows(); ++i) {
               for (index_type j = 1; j <= A.iGetNumCols(); ++j) {
                    os << A.GetElem(i, j) << ' ';
               }
          }

          return os;
     }
}

template <typename ValueType, typename DerivedType>
std::ostream&
Write(std::ostream& out, const sp_grad::SpMatElemExprBase<ValueType, DerivedType>& m, const char* sFill = " ", const char* sFill2 = nullptr)
{
     using namespace sp_grad;

     if (!sFill2) {
          sFill2 = sFill;
     }

     for (index_type i = 1; i <= m.iGetNumRows(); ++i) {
          const bool bLastRow = (i == m.iGetNumRows());

          for (index_type j = 1; j <= m.iGetNumCols(); ++j) {
               out << m.dGetValue(i, j);

               const bool bLastCol = (j == m.iGetNumCols());

               if (bLastCol && !bLastRow) {
                    out << sFill2;
               } else if (!bLastCol) {
                    out << sFill;
               }
          }
     }

     return out;
}

namespace BinaryConversion {
     template <typename ValueType, sp_grad::index_type NumRows, sp_grad::index_type NumCols>
     std::istream& ReadBinary(std::istream& is, sp_grad::SpMatrixBase<ValueType, NumRows, NumCols>& A) {
          using namespace sp_grad;
          
          index_type iNumRows = 0, iNumCols = 0;
          
          ReadBinary(is, iNumRows);
          ReadBinary(is, iNumCols);
          
          A.ResizeReset(iNumRows, iNumCols, 0);

          for (index_type i = 1; i <= iNumRows; ++i) {
               for (index_type j = 1; j <= iNumCols; ++j) {
                    ReadBinary(is, A.GetElem(i, j));
               }
          }
               
          return is;
     }

     template <typename ValueType, sp_grad::index_type NumRows, sp_grad::index_type NumCols>
     std::ostream& WriteBinary(std::ostream& os, const sp_grad::SpMatrixBase<ValueType, NumRows, NumCols>& A) {
          using namespace sp_grad;

          WriteBinary(os, A.iGetNumRows());
          WriteBinary(os, A.iGetNumCols());

          for (index_type i = 1; i <= A.iGetNumRows(); ++i) {
               for (index_type j = 1; j <= A.iGetNumCols(); ++j) {
                    WriteBinary(os, A.GetElem(i, j));
               }
          }

          return os;
     }
}
#endif
