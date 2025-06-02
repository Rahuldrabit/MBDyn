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
  Copyright (C) 2021(-2023) all rights reserved.

  The copyright of this code is transferred
  to Pierangelo Masarati and Paolo Mantegazza
  for use in the software MBDyn as described
  in the GNU Public License version 2.1
*/

#ifndef __PARDISO_SOLUTION_MANAGER_H__INCLUDED__
#define __PARDISO_SOLUTION_MANAGER_H__INCLUDED__

#ifdef USE_PARDISO

#include <cstdint>
#include <iostream>
#include <vector>
#include <limits>

#include "myassert.h"
#include "mynewmem.h"
#include "ls.h"
#include "solman.h"
#include "spmapmh.h"
#include "sp_gradient_spmh.h"
#include <mkl_pardiso.h>

template <typename MKL_INT_TYPE>
struct PardisoSolverTraits;

template <>
struct PardisoSolverTraits<MKL_INT> {
     typedef std::int32_t MH_INT_TYPE;
     static constexpr auto pardiso = ::pardiso;
};

template <>
struct PardisoSolverTraits<long long> {
     typedef std::int64_t MH_INT_TYPE;
     static constexpr auto pardiso = ::pardiso_64;
};

template <typename MKL_INT_TYPE>
class PardisoSolver: public LinearSolver {
private:
     typedef PardisoSolverTraits<MKL_INT_TYPE> SolverType;
     typedef typename SolverType::MH_INT_TYPE MH_INT_TYPE;
     
     static_assert(sizeof(MH_INT_TYPE) == sizeof(MKL_INT_TYPE), "data type does not match");
     static_assert(std::numeric_limits<MH_INT_TYPE>::is_integer, "invalid data type");
     static_assert(std::numeric_limits<MKL_INT_TYPE>::is_integer, "invalid data type");
     
     mutable _MKL_DSS_HANDLE_t pt[64];
     mutable MKL_INT_TYPE iparm[64];
     mutable doublereal* pAx;
     mutable MKL_INT_TYPE* pAi;
     mutable MKL_INT_TYPE* pAp;
     mutable MKL_INT_TYPE iNumNz;
     mutable MKL_INT_TYPE phase, maxfct, mnum, mtype, n, nrhs, msglvl;
     mutable MyVectorHandler AX;
public:
     explicit PardisoSolver(SolutionManager* pSM,
                            integer iDim,
                            doublereal dPivot,
                            integer iNumThreads,
                            integer iNumIter,
                            integer iVerbose);
     ~PardisoSolver();

     virtual void Solve() const override;

     MKL_INT_TYPE PardisoMakeCompactForm(SparseMatrixHandler& mh,
                                         std::vector<doublereal>& Ax,
                                         std::vector<MH_INT_TYPE>& Ai,
                                         std::vector<MH_INT_TYPE>& Ap) const;
};

template <typename MatrixHandlerType, typename MKL_INT_TYPE>
class PardisoSolutionManager: public SolutionManager {
private:
     typedef typename PardisoSolverTraits<MKL_INT_TYPE>::MH_INT_TYPE MH_INT_TYPE;
     mutable MyVectorHandler x, b;
     std::vector<MH_INT_TYPE> Ai, Ap;
     std::vector<doublereal> Ax;

protected:
     mutable MatrixHandlerType A;

     PardisoSolver<MKL_INT_TYPE>* pGetSolver() { return static_cast<PardisoSolver<MKL_INT_TYPE>*>(pLS); }

public:
     PardisoSolutionManager(integer iDim,
                            doublereal dPivot,
                            integer iNumThreads,
                            integer iNumIter,
                            integer iVerbose = 0);
     virtual ~PardisoSolutionManager(void);

#ifdef DEBUG
     virtual void IsValid() const override;
#endif

     virtual void MatrReset() override;
     virtual void MatrInitialize() override;
     virtual void Solve() override;
     void MakeCompressedRowForm();
     virtual MatrixHandler* pMatHdl() const override;
     virtual VectorHandler* pResHdl() const override;
     virtual VectorHandler* pSolHdl() const override;
};

#endif
#endif
