/* $Header$ */
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

#include "mbconfig.h"           /* This goes first in every *.c,*.cc file */

#include "ac/sys_sysinfo.h"
#include "dataman.h"
#include "readlinsol.h"

void
ReadLinSol(LinSol& cs, HighParser &HP, bool bAllowEmpty)
{
   	/* parole chiave */
   	const char* sKeyWords[] = { 
		::solver[LinSol::EMPTY_SOLVER].s_name,
		::solver[LinSol::HARWELL_SOLVER].s_name,
		::solver[LinSol::LAPACK_SOLVER].s_name,
		::solver[LinSol::NAIVE_SOLVER].s_name,
		::solver[LinSol::SUPERLU_SOLVER].s_name,
		::solver[LinSol::TAUCS_SOLVER].s_name,
		::solver[LinSol::UMFPACK_SOLVER].s_name,
		::solver[LinSol::UMFPACK_SOLVER].s_alias,
		::solver[LinSol::KLU_SOLVER].s_name,
		::solver[LinSol::Y12_SOLVER].s_name,
                ::solver[LinSol::PARDISO_SOLVER].s_name,
                ::solver[LinSol::PARDISO_64_SOLVER].s_name,
                ::solver[LinSol::PASTIX_SOLVER].s_name,
                ::solver[LinSol::QR_SOLVER].s_name,
                ::solver[LinSol::SPQR_SOLVER].s_name,
		::solver[LinSol::STRUMPACK_SOLVER].s_name,
		::solver[LinSol::WATSON_SOLVER].s_name,
                ::solver[LinSol::AZTECOO_SOLVER].s_name,
                ::solver[LinSol::AMESOS_SOLVER].s_name,
                ::solver[LinSol::SICONOS_SPARSE_SOLVER].s_name,
                ::solver[LinSol::SICONOS_DENSE_SOLVER].s_name,
		NULL
	};

	enum KeyWords {
		EMPTY,
		HARWELL,
		LAPACK,
		NAIVE,
		SUPERLU,
		TAUCS,
		UMFPACK,
		UMFPACK3,
		KLU,
		Y12,
                PARDISO,
                PARDISO_64,
                PASTIX,
                QR,
                SPQR,
		STRUMPACK,
		WATSON,
		AZTECOO,
                AMESOS,
                SICONOS_SPARSE,
                SICONOS_DENSE,
		LASTKEYWORD
	};

   	/* tabella delle parole chiave */
   	KeyTable K(HP, sKeyWords);

	bool bGotIt = false;	
	switch (KeyWords(HP.GetWord())) {
	case EMPTY:
		if (!bAllowEmpty) {
			silent_cerr("empty solver not allowed at line "
				<< HP.GetLineData() << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		cs.SetSolver(LinSol::EMPTY_SOLVER);
		DEBUGLCOUT(MYDEBUG_INPUT,
				"No LU solver" << std::endl);
		bGotIt = true;
		break;

	case HARWELL:
		cs.SetSolver(LinSol::HARWELL_SOLVER);
		DEBUGLCOUT(MYDEBUG_INPUT,
				"Using harwell sparse LU solver" << std::endl);
#ifdef USE_HARWELL
		bGotIt = true;
#endif /* USE_HARWELL */
		break;

	case LAPACK:
		cs.SetSolver(LinSol::LAPACK_SOLVER);
		DEBUGLCOUT(MYDEBUG_INPUT,
				"Using Lapack dense LU solver" << std::endl);
#ifdef USE_LAPACK
		bGotIt = true;
#endif /* USE_LAPACK */
		break;

	case NAIVE:
		cs.SetSolver(LinSol::NAIVE_SOLVER);
		bGotIt = true;
		break;

	case SUPERLU:
		/*
		 * FIXME: use CC as default???
		 */
		cs.SetSolver(LinSol::SUPERLU_SOLVER);
		DEBUGLCOUT(MYDEBUG_INPUT,
				"Using SuperLU sparse LU solver" << std::endl);
#ifdef USE_SUPERLU
		bGotIt = true;
#endif /* USE_SUPERLU */
		break;

	case TAUCS:
		/*
		 * FIXME: use CC as default???
		 */
		cs.SetSolver(LinSol::TAUCS_SOLVER);
		DEBUGLCOUT(MYDEBUG_INPUT,
				"Using Taucs sparse solver" << std::endl);
#ifdef USE_TAUCS
		bGotIt = true;
#endif /* USE_TAUCS */
		break;

	case UMFPACK3:
		pedantic_cerr("\"umfpack3\" is deprecated; "
				"use \"umfpack\" instead" << std::endl);
	case UMFPACK:
		/*
		 * FIXME: use CC as default???
		 */
		cs.SetSolver(LinSol::UMFPACK_SOLVER);
		DEBUGLCOUT(MYDEBUG_INPUT,
				"Using umfpack sparse LU solver" << std::endl);
#ifdef USE_UMFPACK
		bGotIt = true;
#endif /* USE_UMFPACK */
		break;

	case KLU:
		/*
		 * FIXME: use CC as default???
		 */
		cs.SetSolver(LinSol::KLU_SOLVER);
		DEBUGLCOUT(MYDEBUG_INPUT,
				"Using KLU sparse LU solver" << std::endl);
#ifdef USE_KLU
		bGotIt = true;
#endif /* USE_KLU */
		break;

	case Y12:
		/*
		 * FIXME: use CC as default???
		 */
		cs.SetSolver(LinSol::Y12_SOLVER);
		DEBUGLCOUT(MYDEBUG_INPUT,
				"Using y12 sparse LU solver" << std::endl);
#ifdef USE_Y12
		bGotIt = true;
#endif /* USE_Y12 */
		break;
        case PARDISO:
                cs.SetSolver(LinSol::PARDISO_SOLVER);

                DEBUGLCOUT(MYDEBUG_INPUT,
                           "Using pardiso sparse LU solver" << std::endl);
#ifdef USE_PARDISO
                bGotIt = true;
#endif /* USE_PARDISO */
                break;

        case PARDISO_64:
                cs.SetSolver(LinSol::PARDISO_64_SOLVER);

                DEBUGLCOUT(MYDEBUG_INPUT,
                           "Using pardiso_64 sparse LU solver" << std::endl);
#ifdef USE_PARDISO
                bGotIt = true;
#endif /* USE_PARDISO */
                break;

                
	case PASTIX:
		cs.SetSolver(LinSol::PASTIX_SOLVER);
		DEBUGLCOUT(MYDEBUG_INPUT,
				"Using pastix sparse LU solver" << std::endl);
#ifdef USE_PASTIX
		bGotIt = true;
#endif /* USE_PASTIX */
                break;

        case QR:
                cs.SetSolver(LinSol::QR_SOLVER);
                DEBUGLCOUT(MYDEBUG_INPUT,
                           "Using dense QR solver" << std::endl);
                bGotIt = true;
                break;                
        case SPQR:
                cs.SetSolver(LinSol::SPQR_SOLVER);
                DEBUGLCOUT(MYDEBUG_INPUT,
                           "Using sparse QR solver" << std::endl);
#ifdef USE_SUITESPARSE_QR
                bGotIt = true;
#endif
		break;

	case STRUMPACK:
	     cs.SetSolver(LinSol::STRUMPACK_SOLVER);
	     DEBUGLCOUT(MYDEBUG_INPUT, "Using STRUMPACK solver\n");
#ifdef USE_STRUMPACK
	     bGotIt = true;
#endif
	     break;
	case WATSON:
	     cs.SetSolver(LinSol::WATSON_SOLVER);
	     DEBUGLCOUT(MYDEBUG_INPUT, "Using Watson solver\n");
#ifdef USE_WSMP
	     bGotIt = true;
#endif
	     break;
        case AZTECOO:
             cs.SetSolver(LinSol::AZTECOO_SOLVER);
             DEBUGLCOUT(MYDEBUG_INPUT, "Using AztecOO solver\n");
#ifdef USE_TRILINOS
             bGotIt = true;
#endif
             break;
        case AMESOS:
             cs.SetSolver(LinSol::AMESOS_SOLVER);
             DEBUGLCOUT(MYDEBUG_INPUT, "Using Amesos solver\n");
#ifdef USE_TRILINOS
             bGotIt = true;
#endif
             break;
        case SICONOS_SPARSE:
             cs.SetSolver(LinSol::SICONOS_SPARSE_SOLVER);
             DEBUGLCOUT(MYDEBUG_INPUT, "Using Siconos sparse solver\n");
#ifdef USE_SICONOS
             bGotIt = true;
#endif
             break;
        case SICONOS_DENSE:
             cs.SetSolver(LinSol::SICONOS_DENSE_SOLVER);
             DEBUGLCOUT(MYDEBUG_INPUT, "Using Siconos dense solver\n");
#ifdef USE_SICONOS
             bGotIt = true;
#endif
             break;
	default:
		silent_cerr("unknown solver" << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	const LinSol::solver_t	currSolver = ::solver[cs.GetSolver()];

	if (!bGotIt) {
		silent_cerr(currSolver.s_name << " solver "
			"not available; requested at line " << HP.GetLineData()
			<< std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	cs.SetSolverFlags(currSolver.s_default_flags);

	/* map? */
	if (HP.IsKeyWord("map")) {
		if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_MAP) {
			cs.AddSolverFlags(LinSol::SOLVER_FLAGS_TYPE_MASK, LinSol::SOLVER_FLAGS_ALLOWS_MAP);
			pedantic_cout("using map matrix handling for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		} else {
			pedantic_cerr("map is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		}

	/* CC? */
	} else if (HP.IsKeyWord("column" "compressed") || HP.IsKeyWord("cc")) {
		if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_CC) {
			cs.AddSolverFlags(LinSol::SOLVER_FLAGS_TYPE_MASK, LinSol::SOLVER_FLAGS_ALLOWS_CC);
			pedantic_cout("using column compressed matrix handling for "
					<< currSolver.s_name
					<< " solver" << std::endl);

		} else {
			pedantic_cerr("column compressed is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		}
	} else if (HP.IsKeyWord("sparse" "gradient") || HP.IsKeyWord("grad")) {
		if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_GRAD) {
			cs.AddSolverFlags(LinSol::SOLVER_FLAGS_TYPE_MASK, LinSol::SOLVER_FLAGS_ALLOWS_GRAD);
			pedantic_cout("using sparse gradient handling for "
					<< currSolver.s_name
					<< " solver" << std::endl);

		} else {
			pedantic_cerr("sparse gradient is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		}
	/* direct? */
	} else if (HP.IsKeyWord("direct" "access") || HP.IsKeyWord("dir")) {
		if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_DIR) {
			cs.AddSolverFlags(LinSol::SOLVER_FLAGS_TYPE_MASK, LinSol::SOLVER_FLAGS_ALLOWS_DIR);
			pedantic_cout("using direct access matrix handling for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		} else {
			pedantic_cerr("direct is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		}
	}

	/* colamd? */
	if (HP.IsKeyWord("colamd")) {
		if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_COLAMD) {
		        cs.AddSolverFlags(LinSol::SOLVER_FLAGS_PERM_MASK, LinSol::SOLVER_FLAGS_ALLOWS_COLAMD);
			pedantic_cout("using colamd symmetric preordering for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		} else {
			pedantic_cerr("colamd preordering is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		}
	/* amd ata? */
	} else if (HP.IsKeyWord("mmdata")) {
		silent_cerr("approximate minimum degree solver support is still TODO"
			"task: detect (or import) the MD library;" 
			"uncomment the relevant bits in naivewrap;"
			"remove this check (readlinsol.cc)."
			"Patches welcome"
			<< std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_MMDATA) {
		        cs.AddSolverFlags(LinSol::SOLVER_FLAGS_PERM_MASK, LinSol::SOLVER_FLAGS_ALLOWS_MMDATA);
			pedantic_cout("using mmd symmetric preordering for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		} else {
			pedantic_cerr("mmdata preordering is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		}
	/* minimum degree ?*/
	} else if (HP.IsKeyWord("minimum" "degree")) {
		if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_MDAPLUSAT) {
		        cs.AddSolverFlags(LinSol::SOLVER_FLAGS_PERM_MASK, LinSol::SOLVER_FLAGS_ALLOWS_MDAPLUSAT);
			pedantic_cout("using minimum degree symmetric preordering of A+A^T for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		} else {
			pedantic_cerr("md preordering is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		}
	/* Reverse Kuthill McKee? */
	} else if (HP.IsKeyWord("rcmk")) {
		if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_REVERSE_CUTHILL_MC_KEE) {
		        cs.AddSolverFlags(LinSol::SOLVER_FLAGS_PERM_MASK, LinSol::SOLVER_FLAGS_ALLOWS_REVERSE_CUTHILL_MC_KEE);
			pedantic_cout("using rcmk symmetric preordering for "
					<< currSolver.s_name
					<< " solver" << std::endl);

		} else {
			pedantic_cerr("rcmk preordering is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		}
	/* king ?*/
	} else if (HP.IsKeyWord("king")) {
		if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_KING) {
		        cs.AddSolverFlags(LinSol::SOLVER_FLAGS_PERM_MASK, LinSol::SOLVER_FLAGS_ALLOWS_KING);
			pedantic_cout("using king symmetric preordering for "
					<< currSolver.s_name
					<< " solver" << std::endl);

		} else {
			pedantic_cerr("king preordering is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		}
	/* sloan ? */
	} else if (HP.IsKeyWord("sloan")) {
		if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_KING) {
		        cs.AddSolverFlags(LinSol::SOLVER_FLAGS_PERM_MASK, LinSol::SOLVER_FLAGS_ALLOWS_KING);
			pedantic_cout("using sloan symmetric preordering for "
					<< currSolver.s_name
					<< " solver" << std::endl);

		} else {
			pedantic_cerr("sloan preordering is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		}
	/* nested dissection ? */
	} else if (HP.IsKeyWord("nested" "dissection")) {
#ifdef USE_METIS
		if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_NESTED_DISSECTION) {
		        cs.AddSolverFlags(LinSol::SOLVER_FLAGS_PERM_MASK, LinSol::SOLVER_FLAGS_ALLOWS_NESTED_DISSECTION);
			pedantic_cout("using nested dissection symmetric preordering for "
					<< currSolver.s_name
					<< " solver" << std::endl);

		} else {
			pedantic_cerr("nested dissection preordering is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		}
#else //!USE_METIS
		silent_cerr("nested dissection permutation not built in;"
			"please configure --with-metis to get it"
			<< std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
#endif //USE_METIS
        } else if (HP.IsKeyWord("amd")) {
                if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_AMD) {
		        cs.AddSolverFlags(LinSol::SOLVER_FLAGS_PERM_MASK, LinSol::SOLVER_FLAGS_ALLOWS_AMD);
                        pedantic_cout("using amd preordering for "
                                        << currSolver.s_name
                                        << " solver" << std::endl);

                } else {
                        pedantic_cerr("amd preordering is meaningless for "
                                        << currSolver.s_name
                                        << " solver" << std::endl);
                }                
        } else if (HP.IsKeyWord("given")) {
                if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_GIVEN) {
		        cs.AddSolverFlags(LinSol::SOLVER_FLAGS_PERM_MASK, LinSol::SOLVER_FLAGS_ALLOWS_GIVEN);
                        pedantic_cout("using givens preordering for "
                                        << currSolver.s_name
                                        << " solver" << std::endl);

                } else {
                        pedantic_cerr("givens preordering is meaningless for "
                                        << currSolver.s_name
                                        << " solver" << std::endl);
                }                
        } else if (HP.IsKeyWord("metis")) {
                if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_METIS) {
		        cs.AddSolverFlags(LinSol::SOLVER_FLAGS_PERM_MASK, LinSol::SOLVER_FLAGS_ALLOWS_METIS);
                        pedantic_cout("using metis preordering for "
                                        << currSolver.s_name
                                        << " solver" << std::endl);

                } else {
                        pedantic_cerr("metis preordering is meaningless for "
                                        << currSolver.s_name
                                        << " solver" << std::endl);
                }                
	} else if (HP.IsKeyWord("scotch")) {
                if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_SCOTCH) {
		        cs.AddSolverFlags(LinSol::SOLVER_FLAGS_PERM_MASK, LinSol::SOLVER_FLAGS_ALLOWS_SCOTCH);
                        pedantic_cout("using scotch preordering for "
                                        << currSolver.s_name
                                        << " solver" << std::endl);

                } else {
                        pedantic_cerr("scotch preordering is meaningless for "
                                        << currSolver.s_name
                                        << " solver" << std::endl);
                }                
	}

	/* multithread? */
	if (HP.IsKeyWord("multi" "thread") || HP.IsKeyWord("mt")) {
		int nThreads = HP.GetInt();

		if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_MT_FCT) {
		        cs.AddSolverFlags(LinSol::SOLVER_FLAGS_ALLOWS_MT_FCT, LinSol::SOLVER_FLAGS_ALLOWS_MT_FCT);
			if (nThreads < 1) {
				silent_cerr("illegal thread number, using 1" << std::endl);
				nThreads = 1;
			}
			cs.SetNumThreads(nThreads);

		} else if (nThreads != 1) {
			pedantic_cerr("multithread is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		}


	} else {
		if (currSolver.s_flags & LinSol::SOLVER_FLAGS_ALLOWS_MT_FCT) {
			int nThreads = get_nprocs();

			if (nThreads > 1) {
				silent_cout("no multithread requested "
						"with a potential "
						"of " << nThreads << " CPUs"
						<< std::endl);
			}

			cs.SetNumThreads(nThreads);
		}
	}

	if (HP.IsKeyWord("workspace" "size")) {
		integer iWorkSpaceSize = HP.GetInt();
		if (iWorkSpaceSize < 0) {
			iWorkSpaceSize = 0;
		}

		if (!cs.SetWorkSpaceSize(iWorkSpaceSize)) {
			pedantic_cerr("workspace size is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);
		}
	}

	if (HP.IsKeyWord("pivot" "factor")) {
		doublereal dPivotFactor = HP.GetReal();

		if (currSolver.s_pivot_factor == -1.) {
			pedantic_cerr("pivot factor is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);

		} else {
			if (dPivotFactor <= 0. || dPivotFactor > 1.) {
				silent_cerr("pivot factor " << dPivotFactor
						<< " is out of bounds; "
						"using default "
						"(" << currSolver.s_pivot_factor << ")"
						<< std::endl);
				dPivotFactor = currSolver.s_pivot_factor;
			}
			cs.SetPivotFactor(dPivotFactor);
		}

	} else {
		if (currSolver.s_pivot_factor != -1.) {
			cs.SetPivotFactor(currSolver.s_pivot_factor);
		}
	}

	if (HP.IsKeyWord("drop" "tolerance")) {
		doublereal dDropTolerance = HP.GetReal();

		if (currSolver.s_drop_tolerance == -1.) {
			pedantic_cerr("\"drop tolerance\" is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);

		} else {
			if (dDropTolerance < 0.) {
				silent_cerr("drop tolerance " << dDropTolerance
						<< " is out of bounds; "
						"using default "
						"(" << currSolver.s_drop_tolerance << ")"
						<< std::endl);
				dDropTolerance = currSolver.s_drop_tolerance;
			}
			cs.SetDropTolerance(dDropTolerance);
		}

	} else {
		if (currSolver.s_drop_tolerance != -1.) {
			cs.SetDropTolerance(currSolver.s_drop_tolerance);
		}
	}

	if (HP.IsKeyWord("low" "rank" "compression")) {
	     const char* sKeyWords[] = {
		  "hss",
		  "blr",
		  "hodlr",
		  "svd",
		  "pqrcp",
		  "rqrcp",
		  "tqrcp",
		  "rqrrt",
		  NULL
	     };

	     enum KeyWords {
		  HSS,
		  BLR,
		  HODLR,
		  SVD,
		  PQRCP,
		  RQRCP,
		  TQRCP,
		  RQRRT,		
		  LASTKEYWORD
	     };

	     KeyTable K(HP, sKeyWords);
	     unsigned uKeyWord = HP.GetWord();

	     if (!(uKeyWord >= HSS && uKeyWord < LASTKEYWORD)) {
		  silent_cerr("keywords {hss|blr|hodlr|svd|pqrcp|rqrcp|tqrcp|rqrrt} expected at line " << HP.GetLineData() << std::endl);
		  throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	     }
	     
	     unsigned uCompression = LinSol::SOLVER_FLAGS_ALLOWS_COMPRESSION_HSS << uKeyWord;	    
	     	     
	     if (currSolver.s_flags & uCompression) {
		  cs.AddSolverFlags(LinSol::SOLVER_FLAGS_COMPRESSION_MASK, uCompression);
		  pedantic_cout("using compression " << sKeyWords[uKeyWord] << " for "
				<< currSolver.s_name
				<< " solver" << std::endl);
	     } else {
		  pedantic_cerr("compression " << sKeyWords[uKeyWord] << " is meaningless for "
				<< currSolver.s_name
				<< " solver" << std::endl);
	     }

	     if (HP.IsKeyWord("low" "rank" "tolerance")) {
		  if (!cs.SetLowRankCompressTol(HP.GetReal())) {
		       pedantic_cerr("low rank tolerance is meaningless for "
				     << currSolver.s_name
				     << " solver" << std::endl);		       
		  }
	     }

	     if (HP.IsKeyWord("low" "rank" "min" "ratio")) {
		  if (!cs.SetLowRankCompressMinRatio(HP.GetReal())) {
		       pedantic_cerr("low rank min ratio is meaningless for "
				     << currSolver.s_name
				     << " solver" << std::endl);
		  }
	     }
	}

	if (HP.IsKeyWord("block" "size")) {
		integer blockSize = HP.GetInt();
		if (blockSize < 1) {
			silent_cerr("illegal negative block size; "
					"using default" << std::endl);
			blockSize = 0;
		}

		switch (cs.GetSolver()) {
		case LinSol::UMFPACK_SOLVER:
			cs.SetBlockSize(blockSize);
			break;

		default:
			pedantic_cerr("block size is meaningless for "
					<< currSolver.s_name
					<< " solver" << std::endl);
			break;
		}
	}

	if (HP.IsKeyWord("scale")) {
			SolutionManager::ScaleOpt scale;

			if (HP.IsKeyWord("no")) {
				scale.algorithm = SolutionManager::SCALEA_NONE;

			} else if (HP.IsKeyWord("max") || HP.IsKeyWord("row" "max")) {
				scale.algorithm = SolutionManager::SCALEA_ROW_MAX;

			} else if (HP.IsKeyWord("sum") || HP.IsKeyWord("row" "sum")) {
				scale.algorithm = SolutionManager::SCALEA_ROW_SUM;
			} else if (HP.IsKeyWord("column" "max")) {
				scale.algorithm = SolutionManager::SCALEA_COL_MAX;
			} else if (HP.IsKeyWord("column" "sum")) {
				scale.algorithm = SolutionManager::SCALEA_COL_SUM;
			} else if (HP.IsKeyWord("lapack")) {
				scale.algorithm = SolutionManager::SCALEA_LAPACK;
			} else if (HP.IsKeyWord("iterative")) {
				scale.algorithm = SolutionManager::SCALEA_ITERATIVE;
			} else if (HP.IsKeyWord("row" "max" "column" "max") || HP.IsKeyWord("sinkhorn" "knopp")) {
				scale.algorithm = SolutionManager::SCALEA_ROW_MAX_COL_MAX;
			}

			if (HP.IsKeyWord("scale" "tolerance")) {
				scale.dTol = HP.GetReal();
			}

			if (HP.IsKeyWord("scale" "iterations")) {
				scale.iMaxIter = HP.GetInt();
			}

			if (HP.IsKeyWord("once")) {
				scale.when = SolutionManager::SCALEW_ONCE;

			} else if (HP.IsKeyWord("always")) {
				scale.when = SolutionManager::SCALEW_ALWAYS;

			} else if (HP.IsKeyWord("never")) {
				scale.when = SolutionManager::SCALEW_NEVER;
			}

			switch (scale.when) {
			case SolutionManager::SCALEW_ONCE:
			case SolutionManager::SCALEW_ALWAYS:
				switch (scale.algorithm) {
				case SolutionManager::SCALEA_UNDEF:
					scale.algorithm = SolutionManager::SCALEA_LAPACK; // Restore the original behavior for Naive
					break;

				default:
					// Use the value provided by the input file
					;
				}
				break;

			case SolutionManager::SCALEW_NEVER:
				scale.algorithm = SolutionManager::SCALEA_NONE;
				break;
			}

			if (HP.IsKeyWord("verbose") && HP.GetYesNoOrBool()) {
				scale.uFlags |= SolutionManager::SCALEF_VERBOSE;
			}

			if (HP.IsKeyWord("warnings") && HP.GetYesNoOrBool()) {
				scale.uFlags |= SolutionManager::SCALEF_WARN;
			}

			unsigned uCondFlag = 0;

			if (HP.IsKeyWord("print" "condition" "number")) {
				if (HP.IsKeyWord("norm")) {
					if (HP.IsKeyWord("inf")) {
						uCondFlag = SolutionManager::SCALEF_COND_NUM_INF;
					} else {
						const doublereal dNorm = HP.GetReal();

						if (dNorm != 1.) {
							silent_cerr("Only one norm or infinity norm are supported for condition numbers at line " << HP.GetLineData() << std::endl);
							throw ErrGeneric(MBDYN_EXCEPT_ARGS);
						}

						uCondFlag = SolutionManager::SCALEF_COND_NUM_1;
					}
				} else {
					uCondFlag = SolutionManager::SCALEF_COND_NUM_1;
				}
			}

			if (uCondFlag != 0 && HP.GetYesNoOrBool()) {
				scale.uFlags |= uCondFlag;
			}

			if (!cs.SetScale(scale)) {
				silent_cerr("Warning: Scale options are not available for "
						    << cs.GetSolverName()
						    << " at line "
						    << HP.GetLineData() << std::endl);
			}
	}

        if (HP.IsKeyWord("tolerance")) {
             if (!cs.SetTolerance(HP.GetReal())) {
                  silent_cerr("Warning: refinement tolerance is not supported by " << cs.GetSolverName() << " at line " << HP.GetLineData() << "\n");
             }
        }
        
	if (HP.IsKeyWord("max" "iterations")) {
		if (!cs.SetMaxIterations(HP.GetInt())) {
			silent_cerr("Warning: iterative refinement is not supported by " << cs.GetSolverName() << " at line " << HP.GetLineData() << std::endl);
		}
	}

        if (HP.IsKeyWord("preconditioner") || HP.IsKeyWord("solver")) {
             unsigned uPrecondFlag = 0u;
             
             if (HP.IsKeyWord("umfpack")) {
                  uPrecondFlag = LinSol::SOLVER_FLAGS_ALLOWS_PRECOND_UMFPACK;
             } else if (HP.IsKeyWord("klu")) {
                  uPrecondFlag = LinSol::SOLVER_FLAGS_ALLOWS_PRECOND_KLU;
             } else if (HP.IsKeyWord("lapack")) {
                  uPrecondFlag = LinSol::SOLVER_FLAGS_ALLOWS_PRECOND_LAPACK;
             } else if (HP.IsKeyWord("ilut")) {
                  uPrecondFlag = LinSol::SOLVER_FLAGS_ALLOWS_PRECOND_ILUT;
             } else if (HP.IsKeyWord("superlu")) {
                  uPrecondFlag = LinSol::SOLVER_FLAGS_ALLOWS_PRECOND_SUPERLU;
             } else if (HP.IsKeyWord("mumps")) {
                  uPrecondFlag = LinSol::SOLVER_FLAGS_ALLOWS_PRECOND_MUMPS;
             } else if (HP.IsKeyWord("scalapack")) {
                  uPrecondFlag = LinSol::SOLVER_FLAGS_ALLOWS_PRECOND_SCALAPACK;
             } else if (HP.IsKeyWord("dscpack")) {
                  uPrecondFlag = LinSol::SOLVER_FLAGS_ALLOWS_PRECOND_DSCPACK;
             } else if (HP.IsKeyWord("pardiso")) {
                  uPrecondFlag = LinSol::SOLVER_FLAGS_ALLOWS_PRECOND_PARDISO;
             } else if (HP.IsKeyWord("paraklete")) {
                  uPrecondFlag = LinSol::SOLVER_FLAGS_ALLOWS_PRECOND_PARAKLETE;
             } else if (HP.IsKeyWord("taucs")) {
                  uPrecondFlag = LinSol::SOLVER_FLAGS_ALLOWS_PRECOND_TAUCS;
             } else if (HP.IsKeyWord("csparse")) {
                  uPrecondFlag = LinSol::SOLVER_FLAGS_ALLOWS_PRECOND_CSPARSE;
             } else {
                  silent_cerr("Keywords {umfpack, klu, lapack, ilut, superlu, mumps, scalapack, dscpack, pardiso, paraklete, taucs, csparse} expected at line " << HP.GetLineData() << std::endl);
                  throw ErrGeneric(MBDYN_EXCEPT_ARGS);
             }

             if (!cs.AddSolverFlags(LinSol::SOLVER_FLAGS_PRECOND_MASK, uPrecondFlag)) {
                  silent_cerr("Warning: preconditioner flag is not supported by " << cs.GetSolverName() << " at line " << HP.GetLineData() << std::endl);
             }
        }
        
	if (HP.IsKeyWord("verbose")) {
	     if (!cs.SetVerbose(HP.GetInt())) {
		  silent_cerr("Warning: verbose flag is not supported by " << cs.GetSolverName() << " at line " << HP.GetLineData() << std::endl);
	     }
	}

	switch (cs.GetSolver()) {
	case LinSol::NAIVE_SOLVER:
		if (!(cs.GetSolverFlags() & LinSol::SOLVER_FLAGS_ALLOWS_COLAMD)) {
			silent_cout("warning: \"naive\" solver should be used with \"colamd\"" << std::endl);
		}
		break;

	// add more warnings...

	default:
		break;
	}
}

std::ostream & RestartLinSol(std::ostream& out, const LinSol& cs)
{
	out << cs.GetSolverName();
	
	const LinSol::solver_t	currSolver = ::solver[cs.GetSolver()];
	
	if (cs.GetSolverFlags() != currSolver.s_default_flags) {
		unsigned f = cs.GetSolverFlags();
		if((f & LinSol::SOLVER_FLAGS_ALLOWS_MAP) == 
			LinSol::SOLVER_FLAGS_ALLOWS_MAP) {
			/*Map*/
			out << ", map ";
		}
		if((f & LinSol::SOLVER_FLAGS_ALLOWS_CC) == 
			LinSol::SOLVER_FLAGS_ALLOWS_CC) {
			/*column compressed*/
			out << ", cc ";
		}
		if((f & LinSol::SOLVER_FLAGS_ALLOWS_DIR) == 
			LinSol::SOLVER_FLAGS_ALLOWS_DIR) {
			/*direct access*/
			out << ", dir ";
		}
		if((f & LinSol::SOLVER_FLAGS_ALLOWS_COLAMD) == 
			LinSol::SOLVER_FLAGS_ALLOWS_COLAMD) {
			/*colamd*/
			out << ", colamd ";
		}
		unsigned nt = cs.GetNumThreads();
		if(((f & LinSol::SOLVER_FLAGS_ALLOWS_MT_FCT) == 
			 LinSol::SOLVER_FLAGS_ALLOWS_MT_FCT) && 
			(nt != 1)) {
			/*multi thread*/
			out << ", mt , " << nt;
		}
	}
	integer ws = cs.iGetWorkSpaceSize();
	if(ws > 0) {
		/*workspace size*/
		out << ", workspace size, " << ws;
	}
	doublereal pf = cs.dGetPivotFactor();
	if(pf != -1.) {
		/*pivot factor*/
		out << ", pivot factor, " << pf;
	}
	unsigned bs = cs.GetBlockSize();
	if(bs != 0) {
		/*block size*/
		out << ", block size, " << bs ;
	}
	out << ";" << std::endl;
	return out;
}
