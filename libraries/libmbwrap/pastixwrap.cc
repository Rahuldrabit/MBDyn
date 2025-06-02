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
  Copyright (C) 2018(-2019) all rights reserved.

  The copyright of this code is transferred
  to Pierangelo Masarati and Paolo Mantegazza
  for use in the software MBDyn as described
  in the GNU Public License version 2.1
*/

#include "mbconfig.h"

#ifdef USE_PASTIX

#include "task2cpu.h"

#include <algorithm>
#include "dgeequ.h"
#include "linsol.h"
#include "pastixwrap.h"
#include "cscmhtpl.h"

#if PASTIX_VERSION_MAJOR >= 6 && PASTIX_VERSION_MINOR >= 3 || PASTIX_VERSION_MAJOR > 6
constexpr int PastixArrayBase = 0;
#else
constexpr int PastixArrayBase = 1;
#endif

PastixSolver::SpMatrix::SpMatrix()
     :iNumNonZeros(-1) {
     spmInit(this);
     mtxtype = SpmGeneral;
     flttype = SpmDouble;
     fmttype = SpmCSC;
     dof = 1;
}

PastixSolver::SpMatrix::~SpMatrix() {
     spmExit(this);
}

template <typename T>
T* PastixSolver::SpMatrix::pAllocate(T* pMem, size_t nSize)
{
     pMem = reinterpret_cast<T*>(realloc(pMem, sizeof(T) * nSize));

     if (!pMem) {
          throw std::bad_alloc();
     }

     return pMem;
}

void PastixSolver::SpMatrix::Allocate(size_t iNumNZ, size_t iMatSize)
{
     values = pAllocate(pAx(), iNumNZ);
     rowptr = pAllocate(rowptr, iNumNZ);
     colptr = pAllocate(colptr, iMatSize + 1);
}

bool PastixSolver::SpMatrix::MakeCompactForm(const SparseMatrixHandler& mh)
{
     bool bNewPattern = false;

     if (iNumNonZeros != mh.Nz()) {
          // Force a new ordering step (e.g. needed if we are using a SpMapMatrixHandler)
          Allocate(mh.Nz(), mh.iGetNumCols());
          iNumNonZeros = mh.Nz();
          bNewPattern = true;
     }

     nnz = mh.Nz();
     n = mh.iGetNumCols();
     mh.MakeCompressedColumnForm(pAx(), pAi(), pAp(), PastixArrayBase);

     spmUpdateComputedFields(this);

     spmSymmetrize(this);

     return bNewPattern;
}

PastixSolver::PastixSolver(SolutionManager* pSM, integer iDim, integer iNumIter, doublereal dTolRefine, integer iNumThreads, unsigned uSolverFlags, doublereal dCompressTol, doublereal dMinRatio, integer iVerbose)
    :LinearSolver(pSM),
     pastix_data(nullptr),
     bDoOrdering(true)
{
    pastixInitParam(iparm, dparm);

    iparm[IPARM_VERBOSE] = iVerbose;

    iparm[IPARM_FACTORIZATION] = PastixFactLU;
    iparm[IPARM_THREAD_NBR] = iNumThreads;
    iparm[IPARM_ITERMAX] = iNumIter;

    if (uSolverFlags & LinSol::SOLVER_FLAGS_ALLOWS_SCOTCH) {
         iparm[IPARM_ORDERING] = PastixOrderScotch;
    } else if (uSolverFlags & LinSol::SOLVER_FLAGS_ALLOWS_METIS) {
         iparm[IPARM_ORDERING] = PastixOrderMetis;
    }

    const unsigned uCompressionFlag = uSolverFlags & LinSol::SOLVER_FLAGS_COMPRESSION_MASK;

    if (uCompressionFlag) {
         switch (uCompressionFlag) {
         case LinSol::SOLVER_FLAGS_ALLOWS_COMPRESSION_SVD:
              iparm[IPARM_COMPRESS_METHOD] = PastixCompressMethodSVD;
              break;
         case LinSol::SOLVER_FLAGS_ALLOWS_COMPRESSION_PQRCP:
              iparm[IPARM_COMPRESS_METHOD] = PastixCompressMethodPQRCP;
              break;
         case LinSol::SOLVER_FLAGS_ALLOWS_COMPRESSION_RQRCP:
              iparm[IPARM_COMPRESS_METHOD] = PastixCompressMethodRQRCP;
              break;
         case LinSol::SOLVER_FLAGS_ALLOWS_COMPRESSION_TQRCP:
              iparm[IPARM_COMPRESS_METHOD] = PastixCompressMethodTQRCP;
              break;
         case LinSol::SOLVER_FLAGS_ALLOWS_COMPRESSION_RQRRT:
              iparm[IPARM_COMPRESS_METHOD] = PastixCompressMethodRQRRT;
              break;
         }

         iparm[IPARM_COMPRESS_WHEN] = PastixCompressWhenEnd;
         dparm[DPARM_COMPRESS_TOLERANCE] = dCompressTol;
         dparm[DPARM_COMPRESS_MIN_RATIO] = dMinRatio;
    }

    dparm[DPARM_EPSILON_REFINEMENT] = dTolRefine;

    const Task2CPU& oCPUStateGlobal = Task2CPU::GetGlobalState();

    if (oCPUStateGlobal.iGetCount() >= iNumThreads) {
         std::vector<int> rgCPUSet(iNumThreads, -1);

         int iCPU = oCPUStateGlobal.iGetFirstCPU();

         for (integer i = 0; i < iNumThreads; ++i) {
              rgCPUSet[i] = iCPU;
              iCPU = oCPUStateGlobal.iGetNextCPU(iCPU);
         }

         pastixInitWithAffinity(&pastix_data, MPI_COMM_WORLD, iparm, dparm, &rgCPUSet.front());
    } else {
         pastixInit(&pastix_data, MPI_COMM_WORLD, iparm, dparm);
    }
}

PastixSolver::~PastixSolver()
{
     if (pastix_data) {
          pastixFinalize(&pastix_data);
     }
}

#ifdef DEBUG
void PastixSolver::IsValid(void) const
{
}
#endif /* DEBUG */

void PastixSolver::Solve(void) const
{
    int rc;

    if (bDoOrdering) {
         rc = pastix_task_analyze(pastix_data, &spm);

         if (PASTIX_SUCCESS != rc) {
              throw ErrGeneric(MBDYN_EXCEPT_ARGS);
         }

         bDoOrdering = false;
    }

    if (bHasBeenReset) {
         rc = pastix_task_numfact(pastix_data, &spm);

         if (PASTIX_SUCCESS != rc) {
              throw ErrFactor(-1, MBDYN_EXCEPT_ARGS);
         }

         bHasBeenReset = false;
    }

    // Right hand side will be overwritten by the solution by pastix
    std::copy(pdRhs, pdRhs + spm.n, pdSol);

#if PASTIX_VERSION_MAJOR >= 6 && PASTIX_VERSION_MINOR >= 3 || PASTIX_VERSION_MAJOR > 6
    rc = pastix_task_solve(pastix_data,
                           spm.n,
                           1,
                           pdSol,
                           spm.n);
#else
    rc = pastix_task_solve(pastix_data,
                           1,
                           pdSol,
                           spm.n);
#endif

    if (PASTIX_SUCCESS != rc) {
         throw ErrGeneric(MBDYN_EXCEPT_ARGS);
    }

    if (iparm[IPARM_ITERMAX] > 0) {
         bool bZeroVector = true;

         for (pastix_int_t i = 0; i < spm.n; ++i) {
              if (pdSol[i]) {
                   bZeroVector = false;
                   break;
              }
         }

         if (!bZeroVector) {
              // Avoid division zero by zero in PaStiX
              rc = pastix_task_refine(pastix_data,
                                      spm.n,
                                      1,
                                      pdRhs,
                                      spm.n,
                                      pdSol,
                                      spm.n);

              if (PASTIX_SUCCESS != rc) {
                   throw ErrGeneric(MBDYN_EXCEPT_ARGS);
              }
         }
    }
}

PastixSolver::SpMatrix& PastixSolver::PastixMakeCompactForm(SparseMatrixHandler& mh)
{
    ASSERT(mh.iGetNumRows() == mh.iGetNumCols());

    if (!bHasBeenReset) {
         return spm;
    }

    bDoOrdering = spm.MakeCompactForm(mh);

    return spm;
}

template <typename MatrixHandlerType>
PastixSolutionManager<MatrixHandlerType>::PastixSolutionManager(integer iDim, integer iNumThreads, integer iNumIter, doublereal dTolRefine, const ScaleOpt& scale, unsigned uSolverFlags, doublereal dCompressTol, doublereal dMinRatio, integer iVerbose)
    :x(iDim),
     b(iDim),
     xVH(iDim, &x[0]),
     bVH(iDim, &b[0]),
     scale(scale),
     pMatScale(nullptr),
     A(iDim, iDim)
{
    SAFENEWWITHCONSTRUCTOR(pLS,
                           PastixSolver,
                           PastixSolver(this, iDim, iNumIter, dTolRefine, iNumThreads, uSolverFlags, dCompressTol, dMinRatio, iVerbose));

    pLS->pdSetResVec(&b[0]);
    pLS->pdSetSolVec(&x[0]);
}

template <typename MatrixHandlerType>
PastixSolutionManager<MatrixHandlerType>::~PastixSolutionManager(void)
{
    if (pMatScale) {
        SAFEDELETE(pMatScale);
        pMatScale = nullptr;
    }
}

#ifdef DEBUG
template <typename MatrixHandlerType>
void PastixSolutionManager<MatrixHandlerType>::IsValid(void) const
{
    ASSERT(b.size() == x.size());
    ASSERT(A.iGetNumRows() == A.iGetNumCols());
    ASSERT(b.size() == static_cast<size_t>(A.iGetNumRows()));
    ASSERT(x.size() == static_cast<size_t>(A.iGetNumCols()));

    pLS->IsValid();
}
#endif /* DEBUG */

template <typename MatrixHandlerType>
void PastixSolutionManager<MatrixHandlerType>::MatrReset(void)
{
    pLS->Reset();
}

template <typename MatrixHandlerType>
void PastixSolutionManager<MatrixHandlerType>::MatrInitialize(void)
{
    MatrReset();
}

template <typename MatrixHandlerType>
void PastixSolutionManager<MatrixHandlerType>::MakeCompressedColumnForm(void)
{
     auto& spm = pGetSolver()->PastixMakeCompactForm(A);

     // Attention: Do not use spm.Nz() because we are calling spmSymmetrize!
     CSCMatrixHandlerTpl<doublereal, pastix_int_t, PastixArrayBase> Acsc(spm.pAx(), spm.pAi(), spm.pAp(), A.iGetNumCols(), spm.nnz);

     ScaleMatrixAndRightHandSide(Acsc);
}

template <typename MatrixHandlerType>
template <typename MH>
void PastixSolutionManager<MatrixHandlerType>::ScaleMatrixAndRightHandSide(MH& mh)
{
    if (scale.when != SCALEW_NEVER) {
        MatrixScale<MH>& rMatScale = GetMatrixScale<MH>();

        if (pLS->bReset()) {
            if (!rMatScale.bGetInitialized()
                || scale.when == SolutionManager::SCALEW_ALWAYS) {
                // (re)compute
                rMatScale.ComputeScaleFactors(mh);
            }
            // in any case scale matrix and right-hand-side
            rMatScale.ScaleMatrix(mh);

            if (silent_err) {
                rMatScale.Report(std::cerr);
            }
        }

        rMatScale.ScaleRightHandSide(bVH);
    }
}

template <typename MatrixHandlerType>
template <typename MH>
MatrixScale<MH>& PastixSolutionManager<MatrixHandlerType>::GetMatrixScale()
{
    if (pMatScale == nullptr) {
        pMatScale = MatrixScale<MH>::Allocate(scale);
    }

    // Will throw std::bad_cast if the type does not match
    return dynamic_cast<MatrixScale<MH>&>(*pMatScale);
}

template <typename MatrixHandlerType>
void PastixSolutionManager<MatrixHandlerType>::ScaleSolution(void)
{
    if (scale.when != SCALEW_NEVER) {
        ASSERT(pMatScale != nullptr);

        pMatScale->ScaleSolution(xVH);
    }
}

template <typename MatrixHandlerType>
void PastixSolutionManager<MatrixHandlerType>::Solve(void)
{
    MakeCompressedColumnForm();

    pLS->Solve();

    ScaleSolution();
}

template <typename MatrixHandlerType>
MatrixHandler* PastixSolutionManager<MatrixHandlerType>::pMatHdl(void) const
{
    return &A;
}

template <typename MatrixHandlerType>
VectorHandler* PastixSolutionManager<MatrixHandlerType>::pResHdl(void) const
{
    return &bVH;
}

template <typename MatrixHandlerType>
VectorHandler* PastixSolutionManager<MatrixHandlerType>::pSolHdl(void) const
{
    return &xVH;
}

template class PastixSolutionManager<SpMapMatrixHandler>;
template class PastixSolutionManager<SpGradientSparseMatrixHandler>;
#endif
