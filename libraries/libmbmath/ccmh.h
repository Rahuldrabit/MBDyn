/* $Header$ */
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

#ifndef CColMatrixHandler_hh
#define CColMatrixHandler_hh

#include <vector>

#include "myassert.h"
#include "solman.h"
#include "spmh.h"

/* Sparse Matrix in columns form */
template <int off, typename idx_type = integer>
class CColMatrixHandler : public CompactSparseMatrixHandler_tpl<off, idx_type> {
private:
#ifdef DEBUG
	void IsValid(void) const {
		NO_OP;
	};
#endif /* DEBUG */

public:
	CColMatrixHandler(std::vector<doublereal>& x,
			  const std::vector<idx_type>& i,
			  const std::vector<idx_type>& p);

	virtual ~CColMatrixHandler();

	using MatrixHandler::operator=;

	/* used by MultiThreadDataManager to duplicate the storage array
	 * while preserving the CC indices */
        virtual CompactSparseMatrixHandler *Copy(void) const override;

public:
	doublereal & operator()(integer i_row, integer i_col) override {
		ASSERTMSGBREAK(i_row > 0 && i_row <= this->iGetNumRows(),
				"Error in CColMatrixHandler::operator(), "
				"row index out of range");
		ASSERTMSGBREAK(i_col > 0 && i_col <= this->iGetNumCols(),
				"Error in CColMatrixHandler::operator(), "
				"col index out of range");
		i_row--;
		idx_type row_begin = this->Ap[i_col - 1] - off;
		idx_type row_end = this->Ap[i_col] - off - 1;
		idx_type idx;
		idx_type row;

		if (row_begin == (this->Ap[i_col] - off)
				|| (this->Ai[row_begin] - off) > i_row
				|| (this->Ai[row_end] - off) < i_row)
		{
			/* matrix must be rebuilt */
			throw MatrixHandler::ErrRebuildMatrix(MBDYN_EXCEPT_ARGS);
		}

		while (row_end >= row_begin) {
			idx = (row_begin + row_end)/2;
			row = this->Ai[idx] - off;
			if (i_row < row) {
				row_end = idx - 1;
			} else if (i_row > row) {
				row_begin = idx + 1;
			} else {
				return this->Ax[idx];
			}
		}

		/* matrix must be rebuilt */
		throw MatrixHandler::ErrRebuildMatrix(MBDYN_EXCEPT_ARGS);
	};

	const doublereal& operator () (integer i_row, integer i_col) const override {
		ASSERTMSGBREAK(i_row > 0 && i_row <= this->iGetNumRows(),
				"Error in CColMatrixHandler::operator(), "
				"row index out of range");
		ASSERTMSGBREAK(i_col > 0 && i_col <= this->iGetNumCols(),
				"Error in CColMatrixHandler::operator(), "
				"col index out of range");
		i_row--;
		idx_type row_begin = this->Ap[i_col - 1] - off;
		idx_type row_end = this->Ap[i_col] - off - 1;
		idx_type idx;
		idx_type row;

		if (row_begin == this->Ap[i_col] - off
				|| this->Ai[row_begin] - off > i_row
				|| this->Ai[row_end] - off < i_row)
		{
			return ::Zero1;
		}

		while (row_end >= row_begin) {
			idx = (row_begin + row_end)/2;
			row = this->Ai[idx] - off;
			if (i_row < row) {
				row_end = idx - 1;
			} else if (i_row > row) {
				row_begin = idx + 1;
			} else {
				return this->Ax[idx];
			}
		}

		return ::Zero1;
	};

	void Resize(integer ir, integer ic) override;

	/* Estrae una colonna da una matrice */
	VectorHandler& GetCol(integer icol, VectorHandler& out) const override;

        /* Moltiplica per uno scalare e somma a una matrice */
	MatrixHandler& MulAndSumWithShift(MatrixHandler& out,
			doublereal s = 1.,
			integer drow = 0, integer dcol = 0) const;
	
};

#endif /* CColMatrixHandler_hh */

