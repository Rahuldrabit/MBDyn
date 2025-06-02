/*
 * MBDyn (C) is a multibody analysis code.
 * http://www.mbdyn.org
 *
 * Copyright (C) 2003-2023
 *
 * This code is a partial merge of HmFe and MBDyn.
 *
 * Pierangelo Masarati  <pierangelo.masarati@polimi.it>
 * Paolo Mantegazza     <paolo.mantegazza@polimi.it>
 * Marco Morandini  <morandini@aero.polimi.it>
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

#ifndef __COMPRESSED_SPARSE_COLUMN_MATRIX_HANDLER_TPL__
#define __COMPRESSED_SPARSE_COLUMN_MATRIX_HANDLER_TPL__

#include <algorithm>
#include <vector>

#include "myassert.h"
#include "spmh.h"

template <typename value_type = doublereal, typename idx_type = integer, int offset = 0>
class CSCMatrixHandlerTpl: public MatrixHandler {
public:
     // This matrix handler is used mainly for scaling matrices after they are converted to compressed sparse column form.
     // It does not own the memory and will not free it!
     // So, it can be used with in combination with any CSC matrix handler (e.g. Umfpack, KLU, Pastix)
     CSCMatrixHandlerTpl()
          :NCols(0), NZ(0), pAx(nullptr), pAi(nullptr), pAp(nullptr) {
     }
     
     CSCMatrixHandlerTpl(value_type* pAx, idx_type* pAi, idx_type* pAp, idx_type NCols, idx_type NZ)
	  :NCols(NCols), NZ(NZ), pAx(pAx), pAi(pAi), pAp(pAp) {

	  ASSERT(NCols >= 0);
	  ASSERT(NZ == 0 || pAp[0] == offset);
	  ASSERT(NZ == 0 || pAp[NCols] - pAp[0] == NZ);

#ifdef DEBUG
	  for (idx_type iCol = 0; iCol <= NCols; ++iCol) {
	       ASSERT(pAp[iCol] - offset >= 0);
	       ASSERT(pAp[iCol] - offset <= NZ);
	  }

	  for (idx_type iCol = 0; iCol < NCols; ++iCol) {
	       for (idx_type iIdx = pAp[iCol] - offset; iIdx < pAp[iCol + 1] - offset; ++iIdx) {
		    ASSERT(pAi[iIdx] - offset >= 0);
		    ASSERT(pAi[iIdx] - offset < NCols);
	       }
	  }
#endif
     }

     using MatrixHandler::operator=;

     virtual integer iGetNumRows() const override { return NCols; }
     virtual integer iGetNumCols() const override { return NCols; }

     virtual const doublereal&
     operator () (const integer iRow, const integer iCol) const override {
          return *pFindItem(iRow, iCol);
     }

     virtual doublereal&
     operator () (const integer iRow, const integer iCol) override {
          return *pFindItem(iRow, iCol);
     }

     virtual void Resize(integer, integer) override {
	  throw ErrNotImplementedYet(MBDYN_EXCEPT_ARGS);
     }

     virtual void Reset() override {
	  std::fill(pAx, pAx + NZ, 0.);
     }

#ifdef DEBUG
     virtual void IsValid() const override {
	  ASSERT(NZ == 0 || pAp[0] == offset);
	  ASSERT(NZ == 0 || pAp[NCols] - pAp[0] == NZ);
     }
#endif

     virtual void Scale(const std::vector<doublereal>& oRowScale, const std::vector<doublereal>& oColScale) override {
	  if (oRowScale.size() && oColScale.size()) {
	       ASSERT(oRowScale.size() == static_cast<size_t>(NCols));
	       ASSERT(oColScale.size() == static_cast<size_t>(NCols));

	       for (idx_type iCol = 0; iCol < NCols; ++iCol) {
		    for (idx_type iIdx = pAp[iCol] - offset; iIdx < pAp[iCol + 1] - offset; ++iIdx) {
			 pAx[iIdx] *= oRowScale[pAi[iIdx] - offset] * oColScale[iCol];
		    }
	       }
	  } else if (oRowScale.size()) {
	       ASSERT(oRowScale.size() == static_cast<size_t>(NCols));

	       for (idx_type iCol = 0; iCol < NCols; ++iCol) {
		    for (idx_type iIdx = pAp[iCol] - offset; iIdx < pAp[iCol + 1] - offset; ++iIdx) {
			 pAx[iIdx] *= oRowScale[pAi[iIdx] - offset];
		    }
	       }
	  } else if (oColScale.size()) {
	       ASSERT(oColScale.size() == static_cast<size_t>(NCols));

	       for (idx_type iCol = 0; iCol < NCols; ++iCol) {
		    for (idx_type iIdx = pAp[iCol] - offset; iIdx < pAp[iCol + 1] - offset; ++iIdx) {
			 pAx[iIdx] *= oColScale[iCol];
		    }
	       }
	  }
     }

     virtual void EnumerateNz(const std::function<EnumerateNzCallback>& func) const override {
          for (const auto& d: *this) {
               func(d.iRow + 1, d.iCol + 1, d.dCoef);
          }
     }
     
     class const_iterator {
	  friend class CSCMatrixHandlerTpl;
	  const CSCMatrixHandlerTpl& A;
	  idx_type iIdx;
	  SparseMatrixHandler::SparseMatrixElement elem;

#ifdef DEBUG
	  bool bInvariant() const {
	       ASSERT(iIdx >= 0);
	       ASSERT(iIdx <= A.NZ);
	       ASSERT(iIdx == A.NZ || (elem.iCol >= 0 && elem.iCol < A.NCols));
	       ASSERT(iIdx == A.NZ || A.pAp[elem.iCol] - offset <= iIdx);
	       ASSERT(iIdx == A.NZ || A.pAp[elem.iCol + 1] - offset > iIdx);
	       return true;
	  }
#endif
	  void UpdateElem() {
	       if (iIdx < A.NZ) {
		    elem.iRow = A.pAi[iIdx] - offset;
		    elem.dCoef = A.pAx[iIdx];

		    if (iIdx >= A.pAp[elem.iCol + 1] - offset) {
			 ++elem.iCol;
		    }
	       } else {
		    ASSERT(iIdx == A.NZ);
#ifdef DEBUG
		    elem.iRow = std::numeric_limits<decltype(elem.iRow)>::min();
		    elem.iCol = std::numeric_limits<decltype(elem.iCol)>::min();
		    elem.dCoef = -std::numeric_limits<decltype(elem.dCoef)>::max();
#endif
	       }
	  }

	  const_iterator(const CSCMatrixHandlerTpl& A, idx_type iIdx, idx_type iCol)
	       :A(A), iIdx(iIdx) {

	       elem.iCol = iCol;

	       UpdateElem();

	       ASSERT(bInvariant());
	  }

     public:
	  ~const_iterator() {
	       ASSERT(bInvariant());
	  }

	  const const_iterator& operator++() {
	       ASSERT(bInvariant());

	       ++iIdx;

	       UpdateElem();

	       ASSERT(bInvariant());

	       return *this;
	  }

	  const SparseMatrixHandler::SparseMatrixElement* operator->() const {
	       ASSERT(bInvariant());

	       return &elem;
	  }

	  const SparseMatrixHandler::SparseMatrixElement& operator*() const {
	       ASSERT(bInvariant());

	       return elem;
	  }

	  bool operator == (const const_iterator& op) const {
	       ASSERT(bInvariant());
	       ASSERT(&A == &op.A);

	       return iIdx == op.iIdx;
	  }

	  bool operator != (const const_iterator& op) const {
	       ASSERT(bInvariant());
	       ASSERT(&A == &op.A);

	       return iIdx != op.iIdx;
	  }
     };

     const_iterator begin() const {
	  return const_iterator(*this, 0, 0);
     }

     const_iterator end() const {
	  return const_iterator(*this, NZ, NCols - 1);
     }

     virtual CSCMatrixHandlerTpl* Copy() const override {
          throw ErrNotImplementedYet(MBDYN_EXCEPT_ARGS);
     }     

     virtual VectorHandler&
     MatVecMul_base(void (VectorHandler::*op)(integer iRow,
					      const doublereal& dCoef),
		    VectorHandler& out, const VectorHandler& in) const override {
	  ASSERT(in.iGetSize() == iGetNumCols());
	  ASSERT(out.iGetSize() == iGetNumRows());

	  // NOTE: out must be zeroed by caller

	  for (integer col_idx = 1; col_idx <= NCols; col_idx++) {
	       idx_type re = pAp[col_idx] - offset;
	       idx_type ri = pAp[col_idx - 1] - offset;
	       for ( ; ri < re; ri++) {
		    idx_type row_idx = pAi[ri] - offset + 1;
		    (out.*op)(row_idx, pAx[ri] * in(col_idx));
	       }
	  }

	  return out;
     }

     virtual VectorHandler&
     MatTVecMul_base(void (VectorHandler::*op)(integer iRow,
					       const doublereal& dCoef),
		     VectorHandler& out, const VectorHandler& in) const override {
	  ASSERT(in.iGetSize() == iGetNumRows());
	  ASSERT(out.iGetSize() == iGetNumCols());

	  // NOTE: out must be zeroed by caller

	  for (integer col_idx = 1; col_idx <= NCols; col_idx++) {
	       idx_type re = pAp[col_idx] - offset;
	       idx_type ri = pAp[col_idx - 1] - offset;
	       for ( ; ri < re; ri++) {
		    idx_type row_idx = pAi[ri] - offset + 1;
		    (out.*op)(col_idx, pAx[ri] * in(row_idx));
	       }
	  }

	  return out;
     }

     CSCMatrixHandlerTpl& operator=(const CSCMatrixHandlerTpl& oMH) {
#ifdef DEBUG
          oMH.IsValid();
          IsValid();
#endif
          NCols = oMH.NCols;
          NZ = oMH.NZ;
          pAx = oMH.pAx;
          pAi = oMH.pAi;
          pAp = oMH.pAp;

#ifdef DEBUG
          IsValid();
#endif
          return *this;
     }
     
private:
     doublereal* pFindItem(const integer iRow, const integer iCol) const {
#ifdef DEBUG
          IsValid();
#endif
          ASSERT(iRow >= 1);
          ASSERT(iRow <= iGetNumRows());
          ASSERT(iCol >= 1);
          ASSERT(iCol <= iGetNumCols());
     
          const idx_type iColStart = pAp[iCol - 1] - offset;
          const idx_type iColEnd = pAp[iCol] - offset;

          ASSERT(iColStart >= 0);
          ASSERT(iColStart < NZ);
          ASSERT(iColEnd >= iColStart);
          ASSERT(iColEnd <= NZ);
     
          const idx_type* const pRowStart = pAi + iColStart;
          const idx_type* const pRowEnd = pAi + iColEnd;
     
          const auto it = std::lower_bound(pRowStart, pRowEnd, iRow + offset - 1);

          ASSERT(it >= pRowStart);
          ASSERT(it <= pRowEnd);
     
          if (it == pRowEnd || *it != iRow + offset - 1) {
               static constexpr doublereal dZero = 0.;
               // Return address of write protected memory
               // Depending on the platform it may cause a SIGSEGV
               // if we are writing to this address
               ASSERT(dZero == 0.);
               return const_cast<doublereal*>(&dZero);
          }

          const idx_type iIndex = it - pRowStart + iColStart;

          ASSERT(iIndex >= 0);
          ASSERT(iIndex < NZ);
     
          return pAx + iIndex;
     }
     
     idx_type NCols;
     idx_type NZ;
     value_type* pAx;
     idx_type* pAi;
     idx_type* pAp;
};

#endif
