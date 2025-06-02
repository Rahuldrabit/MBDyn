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
        Copyright (C) 2022(-2023) all rights reserved.

        The copyright of this code is transferred
        to Pierangelo Masarati and Paolo Mantegazza
        for use in the software MBDyn as described
        in the GNU Public License version 2.1
  */

#ifndef __SICONOS_MCP_SOLVER_H__INCLUDED__
#define __SICONOS_MCP_SOLVER_H__INCLUDED__

// This code provides interfaces to INRIA's Siconos library
// https://nonsmooth.gricad-pages.univ-grenoble-alpes.fr/siconos/index.html
// https://github.com/siconos/siconos

// It is a reference implementation for the solution of Mixed (nonlinear) Complementarity Problems (MCP).
// See https://nonsmooth.gricad-pages.univ-grenoble-alpes.fr/siconos/users_guide/problems_and_solvers/mcp.html#mixed-non-linear-complementarity-problem-mcp
// Right now only dense matrices are supported for MCP problems by the Siconos library.

#ifdef USE_SICONOS
#include "nonlin.h"
#include "linesearch.h"
#include "siconosmh.h"

class SiconosMCPSolver: public NonlinearSolver, protected LineSearchParameters
{
public:
     explicit SiconosMCPSolver(const NonlinearSolverTestOptions& options, const LineSearchParameters& oLineSearch, integer iSolverId);
     virtual ~SiconosMCPSolver();

     virtual void Solve(const NonlinearProblem *pNLP,
                        Solver *pS,
                        const integer iMaxIter,
                        const doublereal& Tol,
                        integer& iIterCnt,
                        doublereal& dErr,
                        const doublereal& SolTol,
                        doublereal& dSolErr) override;
private:
     void Cleanup();
     void Attach(const NonlinearProblem* pNLP, Solver* pS);
     static void compute_Fmcp(void *env, int n, doublereal *z, doublereal *F);
     static void compute_nabla_Fmcp(void *env, int n, doublereal *z, struct NumericsMatrix *F);
     static void collectStatsIteration(void *env, int size, double *reaction, double *velocity, double error, void *extra_data);

     SiconosIndexMap* pIndexMap;
     SiconosVectorHandler* pRes;
     SiconosVectorHandler* pSol;
     SiconosVectorHandler W;
     SiconosVectorHandler zPrev, zDelta;
     SiconosMatrixHandler* pJac;
     const NonlinearProblem* pNLP;
     Solver* pSolver;
     struct MixedComplementarityProblem* pMCP;
     struct SolverOptions* pOptions;
     integer iJacPrev, iIterCurr;
     const integer iSolverId;
};

class SiconosMCPNewton: public SiconosMCPSolver {
public:
     explicit SiconosMCPNewton(const NonlinearSolverTestOptions& options, const LineSearchParameters& oLineSearch);
     virtual ~SiconosMCPNewton();
};

class SiconosMCPNewtonMin: public SiconosMCPSolver {
public:
     explicit SiconosMCPNewtonMin(const NonlinearSolverTestOptions& options, const LineSearchParameters& oLineSearch);
     virtual ~SiconosMCPNewtonMin();
};

#endif
#endif
