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

#ifndef __SP_GRADIENT_BASE_H__INCLUDED__
#define __SP_GRADIENT_BASE_H__INCLUDED__

#include <cstddef>
#include <iostream>
#include <limits>
#include <type_traits>

#include "mbconfig.h"
#include "ac/f2c.h"
#include "myassert.h"

#ifdef USE_MULTITHREAD
#include <atomic>
#endif

#ifdef DEBUG
#define SP_GRAD_DEBUG DEBUG
#endif
#define SP_GRAD_ASSERT(expr) ASSERT(expr)
#define SP_GRAD_TRACE(expr) DEBUGCERR(expr)
#define SP_GRAD_TRACE_VAR(var) SP_GRAD_TRACE(#var << "=" << var << std::endl)

#ifdef __GNUC__
#define SP_GRAD_ALIGNMENT(SIZE) __attribute__((aligned (SIZE)))
#else
#define SP_GRAD_ALIGNMENT(SIZE)
#warning SP_GRAD_ALIGNMENT is not defined for this compliler
#endif

#ifdef USE_MULTITHREAD
#define SP_GRAD_THREAD_SAFE
#endif

#if defined(SP_GRAD_THREAD_SAFE)
#define SP_GRAD_THREAD_LOCAL thread_local
#else
#define SP_GRAD_THREAD_LOCAL
#endif

namespace sp_grad {
     typedef integer index_type;

     enum SpFunctionCall: index_type {
          // FIXME: There should be a flag for the initial derivatives phase
          //              However this information is not available for elements at the moment
          //		  The prototype for Element::AssRes and Element::AssJac should be changed like
          //		  AssRes(..., SpFunctionCall func);
          //		  AssJac(..., SpFunctionCall func);
          STATE_MASK                    = 0x0F,
          FUNCTION_MASK                 = 0xF0,
          INITIAL_ASS_FLAG      = 0x01,
          INITIAL_DER_FLAG      = 0x02,
          REGULAR_FLAG          = 0x04,
          RESIDUAL_FLAG                 = 0x10,
          JACOBIAN_FLAG                 = 0x20,
          UNKNOWN_FUNC  = 0x0,
          INITIAL_ASS_RES = INITIAL_ASS_FLAG | RESIDUAL_FLAG,
          INITIAL_ASS_JAC = INITIAL_ASS_FLAG | JACOBIAN_FLAG,
          INITIAL_DER_RES = INITIAL_DER_FLAG | RESIDUAL_FLAG,
          INITIAL_DER_JAC = INITIAL_DER_FLAG | JACOBIAN_FLAG,
          REGULAR_RES   = REGULAR_FLAG     | RESIDUAL_FLAG,
          REGULAR_JAC   = REGULAR_FLAG	   | JACOBIAN_FLAG
     };

     class SpDerivData;
     class SpGradExpDofMap;
     class SpGradient;
     class GpGradProd;

     template <typename ValueType>
     class SpMatrixData;

     template <typename ValueType>
     class SpGradExpDofMapHelper;

     namespace util {
          template <typename T1, typename T2>
          struct ResultType;

          template <>
          struct ResultType<doublereal, doublereal> {
               typedef doublereal Type;
          };

          template <>
          struct ResultType<SpGradient, SpGradient> {
               typedef SpGradient Type;
          };

          template <>
          struct ResultType<GpGradProd, GpGradProd> {
               typedef GpGradProd Type;
          };

          template <>
          struct ResultType<SpGradient, doublereal>: ResultType<SpGradient, SpGradient> {};

          template <>
          struct ResultType<doublereal, SpGradient>: ResultType<SpGradient, SpGradient> {};

          template <>
          struct ResultType<GpGradProd, doublereal>: ResultType<GpGradProd, GpGradProd> {};

          template <>
          struct ResultType<doublereal, GpGradProd>: ResultType<GpGradProd, GpGradProd> {};
     }

     struct SpDerivRec {
          SpDerivRec(index_type iDof, doublereal dDer) noexcept
               :iDof(iDof), dDer(dDer) {
          }

          bool operator<(const SpDerivRec& oRec) const {
               return iDof < oRec.iDof;
          }

          index_type iDof;
          doublereal dDer;
     } SP_GRAD_ALIGNMENT(16);

     class SpDerivData {
     public:
          friend class SpGradient;
          SpDerivData(doublereal dVal,
                      index_type iSizeRes,
                      index_type iSizeCurr,
                      unsigned uFlags,
                      index_type iRefCnt,
                      SpMatrixData<SpGradient>* pOwner) noexcept
               :dVal(dVal),
                iSizeRes(iSizeRes),
                iSizeCurr(iSizeCurr),
                uFlags(uFlags),
                iRefCnt(iRefCnt),
                pOwner(pOwner) {
          }

          bool bHaveRefTo(const SpMatrixData<SpGradient>* pMatData) const {
               return pOwner == pMatData;
          }

          enum Flags: unsigned {
               DER_GENERAL = 0x0u,
               DER_SORTED = 0x1u,
               DER_UNIQUE = 0x2u
          };
     private:
          doublereal dVal;
          index_type iSizeRes;
          index_type iSizeCurr;
          unsigned uFlags;
#ifdef USE_MULTITHREAD
          std::atomic<index_type> iRefCnt;
#else
          index_type iRefCnt;
#endif
          SpMatrixData<SpGradient>* pOwner;
          SpDerivRec rgDer[0];
     } SP_GRAD_ALIGNMENT(alignof(SpDerivRec));

     struct SpGradDofStat {
          SpGradDofStat() noexcept
          :iMinDof(std::numeric_limits<index_type>::max()),
               iMaxDof(std::numeric_limits<index_type>::min()),
               iNumNz(0) {
          }

          index_type iMinDof;
          index_type iMaxDof;
          index_type iNumNz;
     };

     struct SpGradCommon {
          enum SpecialDofs: index_type {
               iInvalidDof = -1,
               iDeletedDof = -2
          };

          enum ExprEvalFlags {
               ExprEvalUnique,
               ExprEvalDuplicate
          };
     };


     template <typename DERIVED>
     class SpGradBase: public SpGradCommon {
     protected:
          constexpr SpGradBase() noexcept {}
          ~SpGradBase() noexcept {}

     public:
          static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = std::remove_reference<DERIVED>::type::eExprEvalFlags;

          constexpr doublereal dGetValue() const {
               return pGetRep()->dGetValue();
          }

          constexpr index_type iGetSize() const {
               return pGetRep()->iGetSize();
          }

          void InsertDeriv(SpGradient& g, doublereal dCoef) const {
               pGetRep()->InsertDeriv(g, dCoef);
          }

          void GetDofStat(SpGradDofStat& s) const {
               pGetRep()->GetDofStat(s);
          }

          template <typename Expr>
          constexpr bool bHaveRefTo(const SpGradBase<Expr>& g) const {
               return pGetRep()->bHaveRefTo(g);
          }

          void InsertDof(SpGradExpDofMap& oExpDofMap) const {
               pGetRep()->InsertDof(oExpDofMap);
          }

          void AddDeriv(SpGradient& g, doublereal dCoef, const SpGradExpDofMap& oExpDofMap) const {
               pGetRep()->AddDeriv(g, dCoef, oExpDofMap);
          }

          constexpr const DERIVED* pGetRep() const {
               return static_cast<const DERIVED*>(this);
          }

#ifdef SP_GRAD_DEBUG
          void PrintValue(std::ostream& os) const {
               pGetRep()->PrintValue(os);
          }

          void PrintDeriv(std::ostream& os, doublereal dCoef) const {
               pGetRep()->PrintDeriv(os, dCoef);
          }
#endif
     };

     namespace util {
          template <SpGradCommon::ExprEvalFlags EXPR_EVAL_FLAGS_A, SpGradCommon::ExprEvalFlags EXPR_EVAL_FLAGS_B>
          struct ExprEvalFlagsHelper {
          };

          template <>
          struct ExprEvalFlagsHelper<SpGradCommon::ExprEvalUnique, SpGradCommon::ExprEvalUnique> {
               static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = SpGradCommon::ExprEvalUnique;
          };

          template <>
          struct ExprEvalFlagsHelper<SpGradCommon::ExprEvalDuplicate, SpGradCommon::ExprEvalUnique> {
               static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = SpGradCommon::ExprEvalUnique;
          };

          template <>
          struct ExprEvalFlagsHelper<SpGradCommon::ExprEvalUnique, SpGradCommon::ExprEvalDuplicate> {
               static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = SpGradCommon::ExprEvalUnique;
          };

          template <>
          struct ExprEvalFlagsHelper<SpGradCommon::ExprEvalDuplicate, SpGradCommon::ExprEvalDuplicate> {
               static constexpr SpGradCommon::ExprEvalFlags eExprEvalFlags = SpGradCommon::ExprEvalDuplicate;
          };

          template <SpGradCommon::ExprEvalFlags EXPR_EVAL_FLAGS>
          struct ExprEvalHelper;

          template <>
          struct ExprEvalHelper<SpGradCommon::ExprEvalDuplicate> {
               template <typename Expr>
               static inline void Eval(SpGradient& g, const SpGradBase<Expr>& f);
               template <typename Func, typename Expr>
               static inline void AssignOper(SpGradient& g, const SpGradBase<Expr>& f);
               template <typename Expr>
               static inline void Eval(SpGradient& g,
                                       const SpGradBase<Expr>& f,
                                       const SpGradExpDofMapHelper<SpGradient>& oDofMap);
               template <typename Func, typename Expr>
               static inline void AssignOper(SpGradient& g,
                                             const SpGradBase<Expr>& f,
                                             const SpGradExpDofMapHelper<SpGradient>& oDofMap);
          };

          template <>
          struct ExprEvalHelper<SpGradCommon::ExprEvalUnique> {
               template <typename Expr>
               static inline void Eval(SpGradient& g, const SpGradBase<Expr>& f);
               template <typename Func, typename Expr>
               static inline void AssignOper(SpGradient& g, const SpGradBase<Expr>& f);
               template <typename Expr>
               static inline void Eval(SpGradient& g,
                                       const SpGradBase<Expr>& f,
                                       const SpGradExpDofMapHelper<SpGradient>& oDofMap);
               template <typename Func, typename Expr>
               static inline void AssignOper(SpGradient& g,
                                             const SpGradBase<Expr>& f,
                                             const SpGradExpDofMapHelper<SpGradient>& oDofMap);
          };
     }

     class GpGradProd;
}

#endif
