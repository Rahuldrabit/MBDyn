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

#ifndef SPMH_H
#define SPMH_H

#include <stdint.h>
#include <vector>

#include "myassert.h"
#include "solman.h"

/* Sparse Matrix */
class SparseMatrixHandler : public MatrixHandler {
protected:
	integer NRows;
	integer NCols;

#ifdef DEBUG
	void IsValid(void) const {
		NO_OP;
	};
#endif /* DEBUG */
                
        template <typename idx_type>
        idx_type MakeCompressedRowFormTpl(doublereal *const Ax,
                                          idx_type *const Ai,
                                          idx_type *const Ap,
                                          int offset = 0) const;
public:

	struct SparseMatrixElement_base {
		integer iRow;
		integer iCol;

		SparseMatrixElement_base(void)
			: iRow(0), iCol(0) { NO_OP; };
		SparseMatrixElement_base(integer iRow, integer iCol)
			: iRow(iRow), iCol(iCol) { NO_OP; };
		virtual ~SparseMatrixElement_base(void) { NO_OP; };
		bool operator == (const SparseMatrixElement_base& op) const
			{ return iRow == op.iRow && iCol == op.iCol; };
		bool operator != (const SparseMatrixElement_base& op) const
			{ return iRow != op.iRow || iCol != op.iCol; };
	};

	// copy
	struct SparseMatrixElement : public SparseMatrixElement_base {
		doublereal dCoef;

		SparseMatrixElement(void)
			: dCoef(0.) {};
		SparseMatrixElement(integer iRow, integer iCol, const doublereal& dCoef)
			: SparseMatrixElement_base(iRow, iCol), dCoef(dCoef) { NO_OP; };
	};

	// reference
	struct SparseMatrixElementRef : public SparseMatrixElement_base {
		doublereal& dCoef;

		SparseMatrixElementRef(integer iRow, integer iCol, doublereal& dCoef)
			: SparseMatrixElement_base(iRow, iCol), dCoef(dCoef) { NO_OP; };
private:
		// do not use
		SparseMatrixElementRef(void);
	};

	// const reference
	struct SparseMatrixElementConstRef : public SparseMatrixElement_base {
		const doublereal& dCoef;

		SparseMatrixElementConstRef(void)
			: dCoef(::Zero1) {};
		SparseMatrixElementConstRef(integer iRow, integer iCol, const doublereal& dCoef)
			: SparseMatrixElement_base(iRow, iCol), dCoef(dCoef) { NO_OP; };
	};

public:
        virtual integer Nz() const=0;

	/* FIXME: always square? */
	SparseMatrixHandler(const integer &n, const integer &nn = 0);

	virtual ~SparseMatrixHandler(void);

	integer iGetNumRows(void) const {
		return NRows;
	};

	integer iGetNumCols(void) const {
		return NCols;
	};

	virtual
	int32_t MakeCompressedColumnForm(doublereal *const Ax,
					 int32_t *const Ai,
					 int32_t *const Ap,
					 int offset = 0) const;

	virtual
	int32_t MakeCompressedColumnForm(std::vector<doublereal>& Ax,
					 std::vector<int32_t>& Ai,
					 std::vector<int32_t>& Ap,
					 int offset = 0) const;

	virtual
	int64_t MakeCompressedColumnForm(doublereal *const Ax,
					 int64_t *const Ai,
					 int64_t *const Ap,
					 int offset = 0) const;

	virtual
	int64_t MakeCompressedColumnForm(std::vector<doublereal>& Ax,
					 std::vector<int64_t>& Ai,
					 std::vector<int64_t>& Ap,
					 int offset = 0) const;

     virtual
     int32_t MakeCompressedRowForm(doublereal *const Ax,
				   int32_t *const Ai,
				   int32_t *const Ap,
				   int offset = 0) const;

     virtual
     int32_t MakeCompressedRowForm(std::vector<doublereal>& Ax,
				   std::vector<int32_t>& Ai,
				   std::vector<int32_t>& Ap,
				   int offset = 0) const;

     virtual
     int64_t MakeCompressedRowForm(doublereal *const Ax,
				   int64_t *const Ai,
				   int64_t *const Ap,
				   int offset = 0) const;

     virtual
     int64_t MakeCompressedRowForm(std::vector<doublereal>& Ax,
				   std::vector<int64_t>& Ai,
				   std::vector<int64_t>& Ap,
				   int offset = 0) const;

     
	virtual
	int32_t MakeIndexForm(doublereal *const Ax,
			      int32_t *const Arow, int32_t *const Acol,
			      int32_t *const AcolSt,
			      int offset = 0) const;

	virtual
	int32_t MakeIndexForm(std::vector<doublereal>& Ax,
			      std::vector<int32_t>& Arow, std::vector<int32_t>& Acol,
			      std::vector<int32_t>& AcolSt,
			      int offset = 0) const;

	virtual
	int64_t MakeIndexForm(doublereal *const Ax,
			      int64_t *const Arow, int64_t *const Acol,
			      int64_t *const AcolSt,
			      int offset = 0) const;

	virtual
	int64_t MakeIndexForm(std::vector<doublereal>& Ax,
			      std::vector<int64_t>& Arow, std::vector<int64_t>& Acol,
			      std::vector<int64_t>& AcolSt,
			      int offset = 0) const;
     
	/* Estrae una colonna da una matrice */
	virtual VectorHandler& GetCol(integer icol,
				      VectorHandler& out) const = 0;
	using MatrixHandler::operator=;
};

/* Sparse Matrix in compact form */
class CompactSparseMatrixHandler : public SparseMatrixHandler {
#ifdef DEBUG
	void IsValid(void) const {
		NO_OP;
	};
#endif /* DEBUG */

public:
	CompactSparseMatrixHandler(const integer &n, const integer &nn);

	virtual ~CompactSparseMatrixHandler();

	/* used by MultiThreadDataManager to duplicate the storage array
	 * while preserving the CC indices */
	virtual CompactSparseMatrixHandler *Copy(void) const = 0;

	/* used to sum CC matrices with identical indices */
        virtual void AddUnchecked(const CompactSparseMatrixHandler& m) = 0;

	using MatrixHandler::operator=;
};

/* Sparse Matrix in compact form */
template <int off, typename idx_type>
class CompactSparseMatrixHandler_tpl : public CompactSparseMatrixHandler {
        integer NZ;
protected:
	bool bMatDuplicate;
	std::vector<doublereal>& Ax;
	const std::vector<idx_type>& Ai;
	const std::vector<idx_type>& Ap;     
public:
	CompactSparseMatrixHandler_tpl(const integer &n, const integer &nn,
				       std::vector<doublereal>& x,
				       const std::vector<idx_type>& i,
				       const std::vector<idx_type>& p);
	virtual ~CompactSparseMatrixHandler_tpl(void);
	using MatrixHandler::operator=;

	class const_iterator {
	private:
	        const CompactSparseMatrixHandler_tpl<off, idx_type>& m;
		mutable idx_type i_idx;
		mutable SparseMatrixHandler::SparseMatrixElement elem;

	protected:
		void reset(bool is_end = false);

	public:
	     const_iterator(const CompactSparseMatrixHandler_tpl<off, idx_type>& m, bool is_end = false);
		~const_iterator(void);
	        const CompactSparseMatrixHandler_tpl<off, idx_type>::const_iterator& operator ++ (void) const;
		const SparseMatrixHandler::SparseMatrixElement* operator -> (void);
		const SparseMatrixHandler::SparseMatrixElement& operator * (void);
	        bool operator == (const CompactSparseMatrixHandler_tpl<off, idx_type>::const_iterator& op) const;
	        bool operator != (const CompactSparseMatrixHandler_tpl<off, idx_type>::const_iterator& op) const;
	};

protected:
	/* Matrix Matrix product */
	MatrixHandler&
	MatMatMul_base(void (MatrixHandler::*op)(integer iRow, integer iCol,
				const doublereal& dCoef),
			MatrixHandler& out, const MatrixHandler& in) const override;
	MatrixHandler&
	MatTMatMul_base(void (MatrixHandler::*op)(integer iRow, integer iCol,
				const doublereal& dCoef),
			MatrixHandler& out, const MatrixHandler& in) const override;

	/* Matrix Vector product */
	virtual VectorHandler&
	MatVecMul_base(void (VectorHandler::*op)(integer iRow,
				const doublereal& dCoef),
			VectorHandler& out, const VectorHandler& in) const override;
	virtual VectorHandler&
	MatTVecMul_base(void (VectorHandler::*op)(integer iRow,
				const doublereal& dCoef),
			VectorHandler& out, const VectorHandler& in) const override;

protected:
        CompactSparseMatrixHandler_tpl<off, idx_type>::const_iterator m_end;

public:
        CompactSparseMatrixHandler_tpl<off, idx_type>::const_iterator begin(void) const {
	     return CompactSparseMatrixHandler_tpl<off, idx_type>::const_iterator(*this);
	};

        const CompactSparseMatrixHandler_tpl<off, idx_type>::const_iterator& end(void) const {
		return m_end;
	};

	/* Restituisce un puntatore all'array di reali della matrice */
        virtual const doublereal* pdGetMat(void) const override;
        virtual doublereal* pdGetMat(void) override;

     	virtual void Reset(void) override;

        virtual void AddUnchecked(const CompactSparseMatrixHandler& m) override;
     
        virtual void Scale(const std::vector<doublereal>& oRowScale, const std::vector<doublereal>& oColScale) override;

        virtual integer Nz() const override;

        virtual void EnumerateNz(const std::function<EnumerateNzCallback>& func) const override;
};

#endif /* SPMH_H */
