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

#ifndef DirCColMatrixHandler_hh
#define DirCColMatrixHandler_hh

#include <vector>

#include "myassert.h"
#include "solman.h"
#include "spmh.h"

/* Sparse Matrix in columns form */
template <int off, typename idx_type = integer>
class DirCColMatrixHandler : public CompactSparseMatrixHandler_tpl<off, idx_type> {
private:
#ifdef DEBUG
	void IsValid(void) const {
		NO_OP;
	};
#endif /* DEBUG */
	std::vector<idx_type *> pindices;
	std::vector<idx_type> indices;

	// don't allow copy constructor!
	DirCColMatrixHandler(const DirCColMatrixHandler&);

public:
	DirCColMatrixHandler(std::vector<doublereal>& x,
			const std::vector<idx_type>& i,
			const std::vector<idx_type>& p);

	virtual ~DirCColMatrixHandler();

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

		auto idx = pindices[i_col][i_row];
		if (idx == -1) {
			/* matrix must be rebuilt */
			throw MatrixHandler::ErrRebuildMatrix(MBDYN_EXCEPT_ARGS);
		}

		return this->Ax[idx];
	};

	const doublereal& operator () (integer i_row, integer i_col) const override {
		ASSERTMSGBREAK(i_row > 0 && i_row <= this->iGetNumRows(),
				"Error in CColMatrixHandler::operator(), "
				"row index out of range");
		ASSERTMSGBREAK(i_col > 0 && i_col <= this->iGetNumCols(),
				"Error in CColMatrixHandler::operator(), "
				"col index out of range");

		auto idx = pindices[i_col][i_row];
		if (idx == -1) {
			/* matrix must be rebuilt */
			return ::Zero1;

		}

		return this->Ax[idx];
	};

	void Resize(integer n, integer nn) override;

	/* Estrae una colonna da una matrice */
	VectorHandler& GetCol(integer icol, VectorHandler& out) const override;

        /* Moltiplica per uno scalare e somma a una matrice */
	MatrixHandler& MulAndSumWithShift(MatrixHandler& out,
			doublereal s = 1.,
			integer drow = 0, integer dcol = 0) const;
	
};


#endif /* DirCColMatrixHandler_hh */

