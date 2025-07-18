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

/*
 * Copyright 1999-2023 Giuseppe Quaranta <quaranta@aero.polimi.it>
 * Dipartimento di Ingegneria Aerospaziale - Politecnico di Milano
 *
 * This copyright statement applies to the MPI related code, which was
 * merged from files schur.h/schur.cc
 */

/*
 *
 * Copyright (C) 2003-2023
 * Giuseppe Quaranta	<quaranta@aero.polimi.it>
 *
 */

/* metodo per la soluzione del modello */

#include "mbconfig.h"           /* This goes first in every *.c,*.cc file */

/* required for configure time macros with paths */
#include "mbdefs.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <ac/unistd.h>
#include <cerrno>
#include <csignal>
#include <cfloat>
#include <cmath>
#include <vector>
#include <algorithm>

#ifdef HAVE_ALLOCA
#include <alloca.h>
#endif

#include "ac/sys_sysinfo.h"

#include "solver.h"
#include "dataman.h"
#include "mtdataman.h"
#include "stepsol_impl.h"
#include "ms34stepsol.h"
#include "multistagestepsol_impl.h"
#include "singlestepsol_impl.h"
#include "nr.h"
#include "linesearch.h"
#ifdef USE_TRILINOS
#include "noxsolver.h"
#endif
#ifdef USE_SICONOS
#include "siconosmcp.h"
#endif
#include "bicg.h"
#include "gmres.h"
#include "solman.h"
#include "readlinsol.h"
#include "ls.h"
#include "naivemh.h"
#include "Rot.hh"
#include "cleanup.h"
#include "drive_.h"
#include "TimeStepControl.h"
#include "solver_impl.h"

#include "ac/lapack.h"
#include "ac/arpack.h"
#include "eigjdqz.h"


#ifdef HAVE_SIGNAL
/*
 * MBDyn starts with mbdyn_keep_going == MBDYN_KEEP_GOING.
 *
 * A single CTRL^C sets it to MBDYN_STOP_AT_END_OF_TIME_STEP,
 * which results in exiting at the end of the time step,
 * after the output in case of success.
 *
 * A second CTRL^C sets it to MBDYN_STOP_AT_END_OF_ITERATION,
 * which results in exiting at the end of the current iteration,
 * after printing debug output if required.
 *
 * A further CTRL^C sets it to MBDYN_STOP_NOW and in throwing
 * an exception.
 */
enum {
	MBDYN_KEEP_GOING = 0,
	MBDYN_STOP_AT_END_OF_TIME_STEP = 1,
	MBDYN_STOP_AT_END_OF_ITERATION = 2,
	MBDYN_STOP_NOW = 3

};

volatile sig_atomic_t mbdyn_keep_going = MBDYN_KEEP_GOING;
#if defined(__FreeBSD__)
__sighandler_t *mbdyn_sh_term = SIG_DFL;
__sighandler_t *mbdyn_sh_int = SIG_DFL;
__sighandler_t *mbdyn_sh_hup = SIG_DFL;
__sighandler_t *mbdyn_sh_pipe = SIG_DFL;
#else // !defined(__FreeBSD__)
__sighandler_t mbdyn_sh_term = SIG_DFL;
__sighandler_t mbdyn_sh_int = SIG_DFL;
__sighandler_t mbdyn_sh_hup = SIG_DFL;
__sighandler_t mbdyn_sh_pipe = SIG_DFL;
#endif // !defined(__FreeBSD__)

extern "C" void
mbdyn_really_exit_handler(int signum)
{
	::mbdyn_keep_going = MBDYN_STOP_NOW;
	switch (signum) {
	case SIGTERM:
		signal(signum, ::mbdyn_sh_term);
		break;

	case SIGINT:
		signal(signum, ::mbdyn_sh_int);
		break;

#ifdef SIGHUP
	case SIGHUP:
		signal(signum, ::mbdyn_sh_hup);
		break;
#endif // SIGHUP

#ifdef SIGPIPE
	case SIGPIPE:
		signal(signum, ::mbdyn_sh_pipe);
		break;
#endif // SIGPIPE
	}

	mbdyn_cleanup();

	throw ErrInterrupted(MBDYN_EXCEPT_ARGS);
}

extern "C" void
mbdyn_modify_last_iteration_handler(int signum)
{
	::mbdyn_keep_going = MBDYN_STOP_AT_END_OF_ITERATION;
#ifdef USE_MULTITHREAD
     	signal(signum, mbdyn_modify_last_iteration_handler);
#else
	signal(signum, mbdyn_really_exit_handler);
#endif
}

extern "C" void
mbdyn_modify_final_time_handler(int signum)
{
	::mbdyn_keep_going = MBDYN_STOP_AT_END_OF_TIME_STEP;
	signal(signum, mbdyn_modify_last_iteration_handler);
}
#endif /* HAVE_SIGNAL */

extern "C" int
mbdyn_stop_at_end_of_iteration(void)
{
#ifdef HAVE_SIGNAL
	return ::mbdyn_keep_going >= MBDYN_STOP_AT_END_OF_ITERATION;
#else // ! HAVE_SIGNAL
	return 0;
#endif // ! HAVE_SIGNAL
}

extern "C" int
mbdyn_stop_at_end_of_time_step(void)
{
#ifdef HAVE_SIGNAL
	return ::mbdyn_keep_going >= MBDYN_STOP_AT_END_OF_TIME_STEP;
#else // ! HAVE_SIGNAL
	return 0;
#endif // ! HAVE_SIGNAL
}

extern "C" void
mbdyn_set_stop_at_end_of_iteration(void)
{
#ifdef HAVE_SIGNAL
	::mbdyn_keep_going = MBDYN_STOP_AT_END_OF_ITERATION;
#endif // HAVE_SIGNAL
}

extern "C" void
mbdyn_set_stop_at_end_of_time_step(void)
{
#ifdef HAVE_SIGNAL
	::mbdyn_keep_going = MBDYN_STOP_AT_END_OF_TIME_STEP;
#endif // HAVE_SIGNAL
}

extern "C" void
mbdyn_signal_init(int pre)
{
#ifdef HAVE_SIGNAL
#if defined(__FreeBSD__)
	__sighandler_t *hdl;
#else // ! defined(__FreeBSD__)
 	__sighandler_t hdl;
#endif // ! defined(__FreeBSD__)

	if (pre) {
		hdl = mbdyn_really_exit_handler;

	} else {
		hdl = mbdyn_modify_final_time_handler;
	}
	/*
	 * FIXME: don't do this if compiling with USE_RTAI
	 * Re FIXME: use sigaction() ...
	 */
	::mbdyn_sh_term = signal(SIGTERM, hdl);
	if (::mbdyn_sh_term == SIG_IGN) {
		signal(SIGTERM, SIG_IGN);
	}

	::mbdyn_sh_int = signal(SIGINT, hdl);
	if (::mbdyn_sh_int == SIG_IGN) {
		signal(SIGINT, SIG_IGN);
	}

#ifdef SIGHUP
	::mbdyn_sh_hup = signal(SIGHUP, hdl);
	if (::mbdyn_sh_hup == SIG_IGN) {
		signal(SIGHUP, SIG_IGN);
	}
#endif // SIGHUP

#ifdef SIGPIPE
	::mbdyn_sh_pipe = signal(SIGPIPE, hdl);
	if (::mbdyn_sh_pipe == SIG_IGN) {
		signal(SIGPIPE, SIG_IGN);
	}
#endif // SIGPIPE
#endif /* HAVE_SIGNAL */
}

int
mbdyn_reserve_stack(unsigned long size)
{
#ifdef HAVE_ALLOCA
       int* buf = (int*)alloca(sizeof(int) * size);
#else
       int buf[size];
#endif
       
#ifdef HAVE_MEMSET
	memset(buf, 0, size*sizeof(int));
#else /* !HAVE_MEMSET */
	for (unsigned long i = 0; i < size; i++) {
		buf[i] = 0;
	}
#endif /* !HAVE_MEMSET */

#ifdef HAVE_MLOCKALL
	return mlockall(MCL_CURRENT | MCL_FUTURE);
#else /* !HAVE_MLOCKALL */
	return 0;
#endif /* !HAVE_MLOCKALL */
}

Solver::FakeStepIntegrator::FakeStepIntegrator(doublereal dCoef)
     :StepIntegrator(-1, dCoef, -1., 1, 1),
      dCoef(dCoef)
{
}

doublereal Solver::FakeStepIntegrator::dGetCoef(unsigned int iDof) const
{
     return dCoef;
}

doublereal
Solver::FakeStepIntegrator::Advance(Solver* pS,
                                    const doublereal TStep,
                                    const doublereal dAlph,
                                    const StepChange StType,
                                    std::deque<VectorHandler*>& qX,
                                    std::deque<VectorHandler*>& qXPrime,
                                    MyVectorHandler*const pX,
                                    MyVectorHandler*const pXPrime,
                                    integer& EffIter,
                                    doublereal& Err,
                                    doublereal& SolErr)
{
     ASSERT(0);

     throw ErrGeneric(MBDYN_EXCEPT_ARGS);
}

/* Costruttore: esegue la simulazione */
Solver::Solver(MBDynParser& HPar,
		const std::string& sInFName,
		const std::string& sOutFName,
		unsigned int nThreads,
		bool bPar)
:
#ifdef USE_MULTITHREAD
nThreads(nThreads),
#endif /* USE_MULTITHREAD */
pTSC(0),
dCurrTimeStep(0.),
iStIter(0),
dTime(0.),
MaxTimeStep(new ConstDriveCaller(::dDefaultMaxTimeStep)),
dMinTimeStep(::dDefaultMinTimeStep),
CurrStep(StepIntegrator::NEWSTEP),
sInputFileName(sInFName),
sOutputFileName(sOutFName),
HP(HPar),
iMaxIterations(::iDefaultMaxIterations),
EigAn(),
pRTSolver(0),
iNumPreviousVectors(2),
iUnkStates(1),
pdWorkSpace(0),
qX(),
qXPrime(),
pX(0),
pXPrime(0),
dInitialTime(0.),
dFinalTime(0.),
dRefTimeStep(0.),
dInitialTimeStep(1.),
dMaxResidual(std::numeric_limits<doublereal>::max()),
dMaxResidualDiff(std::numeric_limits<doublereal>::max()),
eTimeStepLimit(TS_SOFT_LIMIT),
iDummyStepsNumber(::iDefaultDummyStepsNumber),
dDummyStepsRatio(::dDefaultDummyStepsRatio),
eAbortAfter(AFTER_UNKNOWN),
oFakeStepIntegrator(1.), // dCoef == 1 needed for initial assembly
RegularType(INT_UNKNOWN),
DummyType(INT_UNKNOWN),
pDerivativeSteps(0),
pFirstDummyStep(0),
pSecondDummyStep(0),
pThirdDummyStep(0),
pDummySteps(0),
pFirstRegularStep(0),
pSecondRegularStep(0),
pThirdRegularStep(0),
pRegularSteps(0),
pCurrStepIntegrator(&oFakeStepIntegrator), // Used for initial assembly
pRhoRegular(0),
pRhoAlgebraicRegular(0),
//pFirstRhoRegular(0),
//pFirstRhoAlgebraicRegular(0),
pSecondRhoRegular(0),
pSecondRhoAlgebraicRegular(0),
pThirdRhoRegular(0),
pThirdRhoAlgebraicRegular(0),
pRhoDummy(0),
pRhoAlgebraicDummy(0),
pSecondRhoDummy(0),
pSecondRhoAlgebraicDummy(0),
pThirdRhoDummy(0),
pThirdRhoAlgebraicDummy(0),
dDerivativesCoef(::dDefaultDerivativesCoefficient),
CurrLinearSolver(),
ResTest(NonlinearSolverTest::NORM),
SolTest(NonlinearSolverTest::NONE),
bScale(false),
bTrueNewtonRaphson(true),
NonlinearSolverType(NonlinearSolver::UNKNOWN),
/* for matrix-free solvers */
MFSolverType(MatrixFreeSolver::UNKNOWN),
dIterTol(::dDefaultTol),
PcType(Preconditioner::FULLJACOBIANMATRIX),
iPrecondSteps(::iDefaultPreconditionerSteps),
iIterativeMaxSteps(::iDefaultPreconditionerSteps),
dIterertiveEtaMax(defaultIterativeEtaMax),
dIterertiveTau(defaultIterativeTau),
/* end of matrix-free solvers */
/* for line search solver */
oLineSearchParam(),
#ifdef USE_TRILINOS
oNoxSolverParam(),
#endif
/* end of line search solver */
/* for parallel solvers */
bParallel(bPar),
pSDM(0),
iNumLocDofs(0),
iNumIntDofs(0),
pLocDofs(0),
pIntDofs(0),
pDofs(0),
pLocalSM(0),
/* end of parallel solvers */
pDM(0),
iNumDofs(0),
pSM(0),
pNLS(0),
eStatus(SOLVER_STATUS_UNINITIALIZED),
bOutputCounter(false),
outputCounterPrefix(),
outputCounterPostfix(),
iTotIter(0),
dTotErr(0.),
dTest(std::numeric_limits<double>::max()),
dSolTest(std::numeric_limits<double>::max()),
bSolConv(false),
bOut(false),
lStep(0)
{
	DEBUGCOUTFNAME("Solver::Solver");
	::InitTimeStepData();
	ASSERT(!sInFName.empty());
}

#ifdef USE_MULTITHREAD
void
Solver::ThreadPrepare(void)
{
	/* check for thread potential */
	if (nThreads == 0) {
		int n = get_nprocs();

		if (n > 1) {
			silent_cout("no multithread requested "
					"with a potential of " << n
					<< " CPUs" << std::endl);
			nThreads = n;

		} else {
			nThreads = 1;
		}
	}
}
#endif /* USE_MULTITHREAD */

bool
Solver::Prepare(void)
{
	DEBUGCOUTFNAME("Solver::Prepare");

	// consistency check
	if (eStatus != SOLVER_STATUS_UNINITIALIZED) {
		silent_cerr("Prepare() must be called first" << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	mbdyn_signal_init(1);

	/* Legge i dati relativi al metodo di integrazione */
	ReadData(HP);

#ifdef USE_MULTITHREAD
	ThreadPrepare();
#endif /* USE_MULTITHREAD */

	if (pRTSolver) {
		pRTSolver->Setup();
	}

#ifdef USE_SCHUR
	if (bParallel) {
		DEBUGLCOUT(MYDEBUG_MEM, "creating parallel SchurDataManager"
				<< std::endl);

		SAFENEWWITHCONSTRUCTOR(pSDM,
			SchurDataManager,
			SchurDataManager(HP,
				OutputFlags,
				this,
				dInitialTime,
				sOutputFileName.c_str(),
				sInputFileName.c_str(),
				eAbortAfter == AFTER_INPUT));

		pDM = pSDM;

	} else
#endif // USE_SCHUR
	{
		/* chiama il gestore dei dati generali della simulazione */
#ifdef USE_MULTITHREAD
		if (nThreads > 1) {
			if (!(CurrLinearSolver.GetSolverFlags() & LinSol::SOLVER_FLAGS_ALLOWS_MT_ASS)) {
				/* conservative: dir may use too much memory */
			     if (!CurrLinearSolver.AddSolverFlags(LinSol::SOLVER_FLAGS_ALLOWS_MT_ASS, LinSol::SOLVER_FLAGS_ALLOWS_MT_ASS)) {
					bool b;

#if defined(USE_UMFPACK)
					b = CurrLinearSolver.SetSolver(LinSol::UMFPACK_SOLVER,
							LinSol::SOLVER_FLAGS_ALLOWS_MT_ASS);
#elif defined(USE_Y12)
					b = CurrLinearSolver.SetSolver(LinSol::Y12_SOLVER,
							LinSol::SOLVER_FLAGS_ALLOWS_MT_ASS);
#else
					b = false;
#endif
					if (!b) {
						silent_cerr("unable to select a CC-capable solver"
									<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}
				}
			}

			silent_cout("Creating multithread solver "
					"with " << nThreads << " threads "
					"and "
					<< CurrLinearSolver.GetSolverName()
					<< " linear solver"
					<< std::endl);

			SAFENEWWITHCONSTRUCTOR(pDM,
					MultiThreadDataManager,
					MultiThreadDataManager(HP,
						OutputFlags,
						this,
						dInitialTime,
						sOutputFileName.c_str(),
						sInputFileName.c_str(),
						eAbortAfter == AFTER_INPUT,
						nThreads));

		} else
#endif /* USE_MULTITHREAD */
		{
			DEBUGLCOUT(MYDEBUG_MEM, "creating DataManager"
					<< std::endl);

			silent_cout("Creating scalar solver "
					"with "
					<< CurrLinearSolver.GetSolverName()
					<< " linear solver"
					<< std::endl);

			SAFENEWWITHCONSTRUCTOR(pDM,
					DataManager,
					DataManager(HP,
						OutputFlags,
						this,
						dInitialTime,
						sOutputFileName.c_str(),
						sInputFileName.c_str(),
						eAbortAfter == AFTER_INPUT));
		}
	}

        const unsigned uSolverFlags = CurrLinearSolver.GetSolverFlags();

        if (!pDM->bUseAutoDiff() && (uSolverFlags & LinSol::SOLVER_FLAGS_ALLOWS_GRAD) != 0u) {
             silent_cerr("warning: sparse matrix handler \"grad\" requires support for automatic differentiation which was not enabled!\n"
                         "warning: add the statement \"use automatic differentiation;\" inside the control data section to enable it!\n");
             CurrLinearSolver.SetSolverFlags(uSolverFlags & ~LinSol::SOLVER_FLAGS_ALLOWS_GRAD);
        }

	// log symbol table
	std::ostream& log = pDM->GetLogFile();
	log << "Symbol table:" << std::endl;
	log << HP.GetMathParser().GetSymbolTable();

#ifdef HAVE_ENVIRON
	// log environment
	log << "Environment:" << std::endl;
	for (int i = 0; environ[i] != NULL; i++) {
		log << "  " << environ[i] << std::endl;
	}
#endif // HAVE_ENVIRON

	// close input stream
	HP.Close();

	/* Si fa dare il DriveHandler e linka i drivers di rho ecc. */
	const DriveHandler* pDH = pDM->pGetDrvHdl();
	SetOutputDriveHandler(pDH);

	bOutputCounter = outputCounter() && isatty(fileno(stderr));
	outputCounterPrefix = bOutputCounter ? "\n" : "";
	outputCounterPostfix = outputStep() ? "\n" : "\r";

	/* Si fa dare l'std::ostream al file di output per il log */
	std::ostream& Out = pDM->GetOutFile();

	/*
	 * Need to call OutputPrepare before calling Output!
	 */
	pDM->OutputPrepare();
        
	if (eAbortAfter == AFTER_INPUT) {
		/* Esce */
		pDM->Output(0, dTime, 0., 0, 0., 0., false, true);
		Out << "End of Input; no simulation or assembly is required."
			<< std::endl;
		return false;

	} else if (eAbortAfter == AFTER_ASSEMBLY) {
		/* Fa l'output dell'assemblaggio iniziale e poi esce */
		pDM->Output(0, dTime, 0., 0, 0., 0., false, true);
		Out << "End of Initial Assembly; no simulation is required."
			<< std::endl;
		return false;
	}

#ifdef USE_SCHUR
	/* Qui crea le partizioni: principale fra i processi, se parallelo  */
	if (bParallel) {
		pSDM->CreatePartition();
	}
#endif // USE_SCHUR

	pRegularSteps->SetDriveHandler(pDH);
	if (iDummyStepsNumber) {
		pDummySteps->SetDriveHandler(pDH);
	}

	/* Costruisce i vettori della soluzione ai vari passi */
	DEBUGLCOUT(MYDEBUG_MEM, "creating solution vectors" << std::endl);

#ifdef USE_SCHUR
	if (bParallel) {
		iNumDofs = pSDM->HowManyDofs(SchurDataManager::TOTAL);
		pDofs = pSDM->pGetDofsList();

		iNumLocDofs = pSDM->HowManyDofs(SchurDataManager::LOCAL);
		pLocDofs = pSDM->GetDofsList(SchurDataManager::LOCAL);

		iNumIntDofs = pSDM->HowManyDofs(SchurDataManager::INTERNAL);
		pIntDofs = pSDM->GetDofsList(SchurDataManager::INTERNAL);

	} else
#endif // USE_SCHUR
	{
		iNumDofs = pDM->iGetNumDofs();
	}

	/* relink those known drive callers that might need
	 * the data manager, but were verated ahead of it */
	pTSC->SetDriveHandler(pDM->pGetDrvHdl());

	ASSERT(iNumDofs > 0);

	integer iRSteps = pRegularSteps->GetIntegratorNumPreviousStates();
	integer iFSteps = 0;
	if (iDummyStepsNumber) {
		iFSteps = pDummySteps->GetIntegratorNumPreviousStates();
	}
	iNumPreviousVectors = (iRSteps < iFSteps) ? iFSteps : iRSteps;

	integer iRUnkStates = pRegularSteps->GetIntegratorNumUnknownStates();
	integer iFUnkStates = 0;
	if (iDummyStepsNumber) {
		iFUnkStates = pDummySteps->GetIntegratorNumUnknownStates();
	}
	iUnkStates = (iRUnkStates < iFUnkStates) ? iFUnkStates : iRUnkStates;

	/* allocate workspace for previous time steps */
	SAFENEWARR(pdWorkSpace, doublereal,
		2*(iNumPreviousVectors)*iNumDofs);
	/* allocate MyVectorHandlers for previous time steps: use workspace */
	for (int ivec = 0; ivec < iNumPreviousVectors; ivec++) {
		SAFENEWWITHCONSTRUCTOR(pX,
			MyVectorHandler,
			MyVectorHandler(iNumDofs, pdWorkSpace+ivec*iNumDofs));
		qX.push_back(pX);
		SAFENEWWITHCONSTRUCTOR(pXPrime,
			MyVectorHandler,
			MyVectorHandler(iNumDofs,
				pdWorkSpace+((iNumPreviousVectors)+ivec)*iNumDofs));
		qXPrime.push_back(pXPrime);
		pX = 0;
		pXPrime = 0;
	}
	/* allocate MyVectorHandlers for unknown time step(s): own memory */
	SAFENEWWITHCONSTRUCTOR(pX,
		MyVectorHandler,
		MyVectorHandler(iUnkStates*iNumDofs));
	SAFENEWWITHCONSTRUCTOR(pXPrime,
		MyVectorHandler,
		MyVectorHandler(iUnkStates*iNumDofs));


	/* Resetta i vettori */
	for (int ivec = 0; ivec < iNumPreviousVectors; ivec++) {
		qX[ivec]->Reset();
		qXPrime[ivec]->Reset();
	}
	pX->Reset();
	pXPrime->Reset();

	/*
	 * Immediately link DataManager to current solution
	 *
	 * this should work as long as the last unknown time step is put
	 * at the beginning of pX, pXPrime
	 */
	pDM->LinkToSolution(*pX, *pXPrime);

	/* a questo punto si costruisce il nonlinear solver */
	pNLS = AllocateNonlinearSolver();

	Scale.ResizeReset(iNumDofs);
	if (bScale) {
		/* collects scale factors from data manager */
		pDM->SetScale(Scale);
	}

	/*
	 * prepare tests for nonlinear solver;
	 *
	 * test on residual may allow pre-scaling;
	 * test on solution (difference between two iterations) does not
	 */
	NonlinearSolverTest *pResTest = 0;
	if (bScale) {
		NonlinearSolverTestScale *pResTestScale = 0;

		switch (ResTest) {
		case NonlinearSolverTest::NORM:
			SAFENEW(pResTestScale, NonlinearSolverTestScaleNorm);
			break;

		case NonlinearSolverTest::MINMAX:
			SAFENEW(pResTestScale, NonlinearSolverTestScaleMinMax);
			break;

		case NonlinearSolverTest::RELNORM:
			SAFENEW(pResTestScale, NonlinearSolverTestScaleRelNorm);
			break;

		case NonlinearSolverTest::SEPNORM:
			SAFENEW(pResTestScale, NonlinearSolverTestScaleSepNorm);
			break;

		default:
			ASSERT(0);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* registers scale factors at nonlinear solver */
		pResTestScale->SetScale(&Scale);

		pResTest = dynamic_cast<NonlinearSolverTest*>(pResTestScale);


	} else {
		switch (ResTest) {
		case NonlinearSolverTest::NONE:
			SAFENEW(pResTest, NonlinearSolverTestNone);
			break;

		case NonlinearSolverTest::NORM:
			SAFENEW(pResTest, NonlinearSolverTestNorm);
			break;

		case NonlinearSolverTest::MINMAX:
			SAFENEW(pResTest, NonlinearSolverTestMinMax);
			break;

		case NonlinearSolverTest::RELNORM:
			SAFENEW(pResTest, NonlinearSolverTestRelNorm);
			break;

		case NonlinearSolverTest::SEPNORM:
			SAFENEW(pResTest, NonlinearSolverTestSepNorm);
			break;

		default:
			ASSERT(0);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
	}

	NonlinearSolverTest *pSolTest = 0;
	switch (SolTest) {
	case NonlinearSolverTest::NONE:
		SAFENEW(pSolTest, NonlinearSolverTestNone);
		break;

	case NonlinearSolverTest::NORM:
		SAFENEW(pSolTest, NonlinearSolverTestNorm);
		break;

	case NonlinearSolverTest::MINMAX:
		SAFENEW(pSolTest, NonlinearSolverTestMinMax);
		break;

	default:
		ASSERT(0);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	/* registers tests in nonlinear solver */
	pNLS->SetTest(pResTest, pSolTest);

	/* set the dimension and indices map */
	if (pNLS->pGetResTest()->GetDimMap() != 0) {
		pDM->SetElemDimensionIndices(pNLS->pGetResTest()->GetDimMap());
		pDM->SetNodeDimensionIndices(pNLS->pGetResTest()->GetDimMap());
	}

	/*
	 * Dell'assemblaggio iniziale dei vincoli se ne occupa il DataManager
	 * in quanto e' lui il responsabile dei dati della simulazione,
	 * e quindi anche della loro coerenza. Inoltre e' lui a sapere
	 * quali equazioni sono di vincolo o meno.
	 */

	pDM->SetValue(*pX, *pXPrime);

	/*
	 * If eigenanalysis is requested, prepare output for it
	 */
	if (EigAn.bAnalysis) {
                pDM->OutputEigPrepare(EigAn.Analyses.size(), iNumDofs, EigAn.uFlags);
		if (EigAn.iFNameWidth == EigenAnalysis::EIGAN_WIDTH_COMPUTE) {
			EigAn.iFNameWidth = int(std::log10(double(EigAn.Analyses.size()))) + 1;
		}
	}

	/*
	 * Dialoga con il DataManager per dargli il tempo iniziale
	 * e per farsi inizializzare i vettori di soluzione e derivata
	 */
	/* FIXME: the time is already set by DataManager, immediately before InitialAssembly,
	 * but dInitialTimeStep was not set
	 */
	dTime = dInitialTime;
	pDM->SetTime(dTime, dInitialTimeStep, 0, false);

	/*
	 * If first eigenanalysis requested at time *before* initial time,
	 * perform it at initial time but *before* derivatives
	 *
	 *         | t_i
	 * --------+---------------------->
	 *
	 *     o         # if t < t_i, analysis at t = t_i *before derivatives*
	 *         o     # if t == t_i, analysis at t = t_i *after derivatives*
	 */
        EigAll(Solver::EigAnTimeBeforeNow);

	/* calcolo delle derivate */
	DEBUGLCOUT(MYDEBUG_DERIVATIVES, "derivatives solution step"
			<< std::endl);

	mbdyn_signal_init(0);

	/* settaggio degli output Types */
	unsigned OF = OutputFlags;
	if ( DEBUG_LEVEL_MATCH(MYDEBUG_RESIDUAL) ) {
		OF |= OUTPUT_RES;
	}
	if ( DEBUG_LEVEL_MATCH(MYDEBUG_JAC) ) {
		OF |= OUTPUT_JAC;
	}
	if ( DEBUG_LEVEL_MATCH(MYDEBUG_SOL) ) {
		OF |= OUTPUT_SOL;
	}
	pNLS->SetOutputFlags(OF);
	if (pOutputMeter) {
		pOutputMeter->SetDrvHdl(pDM->pGetDrvHdl());
		pNLS->SetOutputMeter(pOutputMeter->pCopy());
	}

	pDerivativeSteps->SetDataManager(pDM);
	pDerivativeSteps->OutputTypes(DEBUG_LEVEL_MATCH(MYDEBUG_PRED));
	if (iDummyStepsNumber) {
		if (DummyType == StepIntegratorType::INT_MS2 || DummyType == StepIntegratorType::INT_HOPE || DummyType == StepIntegratorType::INT_MS3 || DummyType == StepIntegratorType::INT_MS4)
		{
		pFirstDummyStep->SetDataManager(pDM);
		pFirstDummyStep->OutputTypes(DEBUG_LEVEL_MATCH(MYDEBUG_PRED));
		}
		if (DummyType == StepIntegratorType::INT_MS3 || DummyType == StepIntegratorType::INT_MS4)
		{
			pSecondDummyStep->SetDataManager(pDM);
			pSecondDummyStep->OutputTypes(DEBUG_LEVEL_MATCH(MYDEBUG_PRED));
		}
		if (DummyType == StepIntegratorType::INT_MS4)
		{
			pThirdDummyStep->SetDataManager(pDM);
			pThirdDummyStep->OutputTypes(DEBUG_LEVEL_MATCH(MYDEBUG_PRED));
		}
		pDummySteps->SetDataManager(pDM);
		pDummySteps->OutputTypes(DEBUG_LEVEL_MATCH(MYDEBUG_PRED));
	}
	if (RegularType == StepIntegratorType::INT_MS2 || RegularType == StepIntegratorType::INT_HOPE || RegularType == StepIntegratorType::INT_MS3 || RegularType == StepIntegratorType::INT_MS4 || RegularType == StepIntegratorType::INT_HYBRID)
	{
	pFirstRegularStep->SetDataManager(pDM);
	pFirstRegularStep->OutputTypes(DEBUG_LEVEL_MATCH(MYDEBUG_PRED));
	}
	if (RegularType == StepIntegratorType::INT_MS3 || RegularType == StepIntegratorType::INT_MS4)
	{
		pSecondRegularStep->SetDataManager(pDM);
		pSecondRegularStep->OutputTypes(DEBUG_LEVEL_MATCH(MYDEBUG_PRED));
	}
	if (RegularType == StepIntegratorType::INT_MS4)
	{
		pThirdRegularStep->SetDataManager(pDM);
		pThirdRegularStep->OutputTypes(DEBUG_LEVEL_MATCH(MYDEBUG_PRED));
	}
	pRegularSteps->SetDataManager(pDM);
	pRegularSteps->OutputTypes(DEBUG_LEVEL_MATCH(MYDEBUG_PRED));

#ifdef USE_EXTERNAL
	pNLS->SetExternal(External::EMPTY);
#endif /* USE_EXTERNAL */
	/* Setup SolutionManager(s) */
	SetupSolmans(pDerivativeSteps->GetIntegratorNumUnknownStates());

	if (pNLS->pGetResTest()->GetAbsRes() != 0) {
		pNLS->pGetResTest()->GetAbsRes()->Resize(pSM->pResHdl()->iGetSize());
	}

	/* set the dimension and indices map */
	if (pNLS->pGetResTest()->GetDimMap() != 0) {
		pDM->SetElemDimensionIndices(pNLS->pGetResTest()->GetDimMap());
		pDM->SetNodeDimensionIndices(pNLS->pGetResTest()->GetDimMap());
	}
	
	/* Derivative steps */
	pCurrStepIntegrator = pDerivativeSteps;
	try {
		if (outputStep()) {
			if (outputCounter()) {
				silent_cout(std::endl);
			}
 			silent_cout("Derivatives t=" << dTime << " coef=" << dDerivativesCoef << std::endl);
		}
		dTest = pDerivativeSteps->Advance(this,
			dInitialTimeStep, 1., StepIntegrator::NEWSTEP,
			qX, qXPrime, pX, pXPrime,
			iStIter, dTest, dSolTest);
	}
	catch (NonlinearSolver::NoConvergence& e) {
		silent_cerr("Initial derivatives calculation " << iStIter
			<< " does not converge; aborting..." << std::endl
			<< "(hint: try playing with the \"derivatives coefficient\" value)" << std::endl);
		pDM->Output(0, dTime, 0., iStIter, dTest, dSolTest, false, true);
		throw ErrMaxIterations(MBDYN_EXCEPT_ARGS);
	}
	catch (NonlinearSolver::ErrSimulationDiverged& e) {
		/*
		 * Mettere qui eventuali azioni speciali
		 * da intraprendere in caso di errore ...
		 */
		throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
	}
	catch (LinearSolver::ErrFactor& err) {
		/*
		 * Mettere qui eventuali azioni speciali
		 * da intraprendere in caso di errore ...
		 */
		silent_cerr("Initial derivatives failed because no pivot element "
			"could be found for column " << err.iCol
			<< " (" << pDM->GetDofDescription(err.iCol) << "); "
			"aborting..." << std::endl);
		throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
	}
	catch (NonlinearSolver::ConvergenceOnSolution& e) {
		bSolConv = true;
	}
	catch (EndOfSimulation& eos) {
		silent_cerr("Simulation ended during the derivatives steps:\n" << eos.what() << "\n");
		return false;
	}

	SAFEDELETE(pDerivativeSteps);
	pDerivativeSteps = 0;

#if 0
	/* don't sum up the derivatives error */
	dTotErr  += dTest;
#endif
	iTotIter += iStIter;

	if (outputMsg()) {
		Out << "# Derivatives solution step at time " << dInitialTime
			<< " performed in " << iStIter
			<< " iterations with " << dTest
			<< " error" << std::endl;
	}

	DEBUGCOUT("Derivatives solution step has been performed successfully"
		" in " << iStIter << " iterations" << std::endl);

#ifdef USE_EXTERNAL
	/* comunica che gli ultimi dati inviati sono la condizione iniziale */
	if (iDummyStepsNumber == 0) {
		External::SendInitial();
	}
#endif /* USE_EXTERNAL */

	if (eAbortAfter == AFTER_DERIVATIVES) {
		/*
		 * Fa l'output della soluzione delle derivate iniziali ed esce
		 */


		pDM->Output(0, dTime, 0., iStIter, dTest, dSolTest, false, true);
		Out << "End of derivatives; no simulation is required."
			<< std::endl;
		return false;

	} else if (mbdyn_stop_at_end_of_time_step()) {
		/*
		 * Fa l'output della soluzione delle derivate iniziali ed esce
		 */
		pDM->Output(0, dTime, 0., iStIter, dTest, dSolTest, false, true);
		Out << "Interrupted during derivatives computation." << std::endl;
		throw ErrInterrupted(MBDYN_EXCEPT_ARGS);
	}

	// if eigenanalysis is requested at initial time,
	// perform *after* derivatives
        EigAll(Solver::EigAnTimeExactlyNow);

	/* Dati comuni a passi fittizi e normali */
	lStep = 1;

	if (iDummyStepsNumber > 0) {
		/* passi fittizi */
		int iSubStep = 0;
		dRefTimeStep = dInitialTimeStep * dDummyStepsRatio;
		dCurrTimeStep = dRefTimeStep;
		/*
		 * inizio integrazione: primo passo a predizione lineare
		 * con sottopassi di correzione delle accelerazioni
		 * e delle reazioni vincolari
		 */
		if (DummyType == StepIntegratorType::INT_MS2  || DummyType == StepIntegratorType::INT_HOPE || DummyType == StepIntegratorType::INT_MS3 || DummyType == StepIntegratorType::INT_MS4)
		{
			/* Setup SolutionManager(s) */
			SetupSolmans(pFirstDummyStep->GetIntegratorNumUnknownStates());
			iSubStep++;

			pDM->BeforePredict(*pX, *pXPrime, qX, qXPrime);
		Flip();

		dRefTimeStep = dInitialTimeStep*dDummyStepsRatio;
		dCurrTimeStep = dRefTimeStep;
		/* FIXME: do we need to serve pending drives in dummy steps? */
		pDM->SetTime(dTime + dCurrTimeStep, dCurrTimeStep, 0);

			DEBUGLCOUT(MYDEBUG_FSTEPS, "Dummy step "
				<< iSubStep
				<< "; current time step: "
			<< dCurrTimeStep << std::endl);

			if (outputStep())
			{
				silent_cout("Dummy Step(" << iSubStep << ") t=" << dTime + dCurrTimeStep << " dt=" << dCurrTimeStep << std::endl);
                }
                
		ASSERT(pFirstDummyStep != 0);

		/* Setup SolutionManager(s) */
		SetupSolmans(pFirstDummyStep->GetIntegratorNumUnknownStates());
		/* pFirstDummyStep */
		pCurrStepIntegrator = pFirstDummyStep;
		try {
			dTest = pFirstDummyStep->Advance(this,
				dRefTimeStep, 1.,
				StepIntegrator::NEWSTEP,
				qX, qXPrime, pX, pXPrime,
				iStIter, dTest, dSolTest);
		}
		catch (NonlinearSolver::NoConvergence& e) {
				silent_cerr("Dummy step" << iSubStep << "does not converge; "
				"TimeStep=" << dCurrTimeStep
				<< " cannot be reduced further; "
				"aborting..." << std::endl);
			pDM->Output(0, dTime, dCurrTimeStep, iStIter, dTest, dSolTest, false, true);
			throw ErrMaxIterations(MBDYN_EXCEPT_ARGS);
		}
		catch (NonlinearSolver::ErrSimulationDiverged& e) {
			/*
			 * Mettere qui eventuali azioni speciali
			 * da intraprendere in caso di errore ...
			 */
			throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
		}
		catch (LinearSolver::ErrFactor& err) {
			/*
			 * Mettere qui eventuali azioni speciali
			 * da intraprendere in caso di errore ...
			 */
				silent_cerr("Dummy step " << iSubStep
				<< " failed because no pivot element "
				"could be found for column " << err.iCol
				<< " (" << pDM->GetDofDescription(err.iCol) << "); "
				"aborting..." << std::endl);
			throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
		}
		catch (NonlinearSolver::ConvergenceOnSolution& e) {
			bSolConv = true;
		}
		catch (EndOfSimulation& eos) {
				silent_cerr("Simulation ended during the dummy step " << iSubStep << " :\n"
				<< eos.what() << "\n");
			return false;
		}

		SAFEDELETE(pFirstDummyStep);
		pFirstDummyStep = 0;

		dRefTimeStep = dCurrTimeStep;
		dTime += dRefTimeStep;

#if 0
		/* don't sum up the derivatives error */
		dTotErr += dTest;
#endif
		iTotIter += iStIter;

		if (mbdyn_stop_at_end_of_time_step()) {
			/*
			 * Fa l'output della soluzione delle derivate iniziali
			 * ed esce
			 */
#ifdef DEBUG_FICTITIOUS
			pDM->Output(0, dTime, dCurrTimeStep, iStIter, dTest, dSolTest, false, true);
#endif /* DEBUG_FICTITIOUS */
				Out << "Interrupted during dummy step" << iSubStep << " ." << std::endl;
			throw ErrInterrupted(MBDYN_EXCEPT_ARGS);
		}

#ifdef DEBUG_FICTITIOUS
		pDM->Output(0, dTime, dCurrTimeStep, iStIter, dTest, dSolTest, false, true);
#endif /* DEBUG_FICTITIOUS */
		}

		if (DummyType == StepIntegratorType::INT_MS3 || DummyType == StepIntegratorType::INT_MS4)
		{
			/* Setup SolutionManager(s) */
			SetupSolmans(pSecondDummyStep->GetIntegratorNumUnknownStates());
			iSubStep++;

			pDM->BeforePredict(*pX, *pXPrime, qX, qXPrime);
			Flip();

			/* FIXME: do we need to serve pending drives in dummy steps? */
			pDM->SetTime(dTime + dCurrTimeStep, dCurrTimeStep, 0);

			DEBUGLCOUT(MYDEBUG_FSTEPS, "Dummy step "
				<< iSubStep
				<< "; current time step: "
				<< dCurrTimeStep << std::endl);

			if (outputStep())
			{
				silent_cout("Dummy Step(" << iSubStep << ") t=" << dTime + dCurrTimeStep << " dt=" << dCurrTimeStep << std::endl);
            }
                
			ASSERT(pSecondDummyStep != 0);

			/* pSecondDummyStep */
			pCurrStepIntegrator = pSecondDummyStep;
			try {
				dTest = pSecondDummyStep->Advance(this,
					dRefTimeStep, dCurrTimeStep/dRefTimeStep,
					StepIntegrator::NEWSTEP,
					qX, qXPrime, pX, pXPrime,
					iStIter, dTest, dSolTest);
			}
			catch (NonlinearSolver::NoConvergence& e) {
				silent_cerr("Dummy step" << iSubStep << "does not converge; "
					"TimeStep=" << dCurrTimeStep
					<< " cannot be reduced further; "
					"aborting..." << std::endl);
				pDM->Output(0, dTime, dCurrTimeStep, iStIter, dTest, dSolTest, false, true);
				throw ErrMaxIterations(MBDYN_EXCEPT_ARGS);
			}
			catch (NonlinearSolver::ErrSimulationDiverged& e) {
				/*
			 	* Mettere qui eventuali azioni speciali
			 	* da intraprendere in caso di errore ...
			 	*/
				throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
			}
			catch (LinearSolver::ErrFactor& err) {
				/*
			 	* Mettere qui eventuali azioni speciali
			 	* da intraprendere in caso di errore ...
			 	*/
				silent_cerr("Dummy step " << iSubStep
				<< " failed because no pivot element "
				"could be found for column " << err.iCol
				<< " (" << pDM->GetDofDescription(err.iCol) << "); "
				"aborting..." << std::endl);
				throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
			}
			catch (NonlinearSolver::ConvergenceOnSolution& e) {
				bSolConv = true;
			}
			catch (EndOfSimulation& eos) {
				silent_cerr("Simulation ended during dummy step " << iSubStep << " :\n"
					<< eos.what() << "\n");
				return false;
			}

			SAFEDELETE(pSecondDummyStep);
			pSecondDummyStep = 0;

			dRefTimeStep = dCurrTimeStep;
			dTime += dRefTimeStep;

#if 0
		/* don't sum up the derivatives error */
		dTotErr += dTest;
#endif
			iTotIter += iStIter;

			if (mbdyn_stop_at_end_of_time_step()) {
			/*
			 * Fa l'output della soluzione delle derivate iniziali
			 * ed esce
			 */
#ifdef DEBUG_FICTITIOUS
			pDM->Output(0, dTime, dCurrTimeStep, iStIter, dTest, dSolTest, false, true);
#endif /* DEBUG_FICTITIOUS */
				Out << "Interrupted during dummy step" << iSubStep << " ." << std::endl;
				throw ErrInterrupted(MBDYN_EXCEPT_ARGS);
			}

#ifdef DEBUG_FICTITIOUS
		pDM->Output(0, dTime, dCurrTimeStep, iStIter, dTest, dSolTest, false, true);
#endif /* DEBUG_FICTITIOUS */
		}

		if (DummyType == StepIntegratorType::INT_MS4)
		{
			/* Setup SolutionManager(s) */
			SetupSolmans(pThirdDummyStep->GetIntegratorNumUnknownStates());
			iSubStep++;

			pDM->BeforePredict(*pX, *pXPrime, qX, qXPrime);
			Flip();

			/* FIXME: do we need to serve pending drives in dummy steps? */
			pDM->SetTime(dTime + dCurrTimeStep, dCurrTimeStep, 0);

			DEBUGLCOUT(MYDEBUG_FSTEPS, "Dummy step "
				<< iSubStep
				<< "; current time step: "
				<< dCurrTimeStep << std::endl);

			if (outputStep())
			{
				silent_cout("Dummy Step(" << iSubStep << ") t=" << dTime + dCurrTimeStep << " dt=" << dCurrTimeStep << std::endl);
            }
                
			ASSERT(pThirdDummyStep != 0);

			/* pThirdDummyStep */
			pCurrStepIntegrator = pThirdDummyStep;
			try {
				dTest = pThirdDummyStep->Advance(this,
					dRefTimeStep, dCurrTimeStep/dRefTimeStep,
					StepIntegrator::NEWSTEP,
					qX, qXPrime, pX, pXPrime,
					iStIter, dTest, dSolTest);
			}
			catch (NonlinearSolver::NoConvergence& e) {
				silent_cerr("Dummy step" << iSubStep << "does not converge; "
					"TimeStep=" << dCurrTimeStep
					<< " cannot be reduced further; "
					"aborting..." << std::endl);
				pDM->Output(0, dTime, dCurrTimeStep, iStIter, dTest, dSolTest, false, true);
				throw ErrMaxIterations(MBDYN_EXCEPT_ARGS);
			}
			catch (NonlinearSolver::ErrSimulationDiverged& e) {
				/*
			 	* Mettere qui eventuali azioni speciali
			 	* da intraprendere in caso di errore ...
			 	*/
				throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
			}
			catch (LinearSolver::ErrFactor& err) {
				/*
			 	* Mettere qui eventuali azioni speciali
			 	* da intraprendere in caso di errore ...
			 	*/
				silent_cerr("Dummy step " << iSubStep
				<< " failed because no pivot element "
				"could be found for column " << err.iCol
				<< " (" << pDM->GetDofDescription(err.iCol) << "); "
				"aborting..." << std::endl);
				throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
			}
			catch (NonlinearSolver::ConvergenceOnSolution& e) {
				bSolConv = true;
			}
			catch (EndOfSimulation& eos) {
				silent_cerr("Simulation ended during dummy step " << iSubStep << " :\n"
					<< eos.what() << "\n");
				return false;
			}

			SAFEDELETE(pThirdDummyStep);
			pThirdDummyStep = 0;

			dRefTimeStep = dCurrTimeStep;
			dTime += dRefTimeStep;

#if 0
		/* don't sum up the derivatives error */
		dTotErr += dTest;
#endif
			iTotIter += iStIter;

			if (mbdyn_stop_at_end_of_time_step()) {
			/*
			 * Fa l'output della soluzione delle derivate iniziali
			 * ed esce
			 */
#ifdef DEBUG_FICTITIOUS
			pDM->Output(0, dTime, dCurrTimeStep, iStIter, dTest, dSolTest, false, true);
#endif /* DEBUG_FICTITIOUS */
				Out << "Interrupted during dummy step" << iSubStep << " ." << std::endl;
				throw ErrInterrupted(MBDYN_EXCEPT_ARGS);
			}

#ifdef DEBUG_FICTITIOUS
		pDM->Output(0, dTime, dCurrTimeStep, iStIter, dTest, dSolTest, false, true);
#endif /* DEBUG_FICTITIOUS */
		}

		/* Passi fittizi successivi */
		if (iDummyStepsNumber > iSubStep) {
			/* Setup SolutionManager(s) */
			SetupSolmans(pDummySteps->GetIntegratorNumUnknownStates());
		}

		for (iSubStep += 1;
			iSubStep <= iDummyStepsNumber;
			iSubStep++)
		{
			pDM->BeforePredict(*pX, *pXPrime,
				qX, qXPrime);
			Flip();

			pDM->SetTime(dTime + dCurrTimeStep, dCurrTimeStep, 0);


			DEBUGLCOUT(MYDEBUG_FSTEPS, "Dummy step "
				<< iSubStep
				<< "; current time step: " << dCurrTimeStep
				<< std::endl);

                        if (outputStep()) {
                            silent_cout("Dummy Step(" << iSubStep << ") t=" << dTime + dCurrTimeStep << " dt=" << dCurrTimeStep << std::endl);
                        }
                        
			pCurrStepIntegrator = pDummySteps;
			ASSERT(pDummySteps!= 0);
			try {
				pDM->SetTime(dTime + dCurrTimeStep, dCurrTimeStep, 0);
				dTest = pDummySteps->Advance(this,
					dRefTimeStep,
					dCurrTimeStep/dRefTimeStep,
					StepIntegrator::NEWSTEP,
					qX, qXPrime, pX, pXPrime,
					iStIter, dTest, dSolTest);
			}
			catch (NonlinearSolver::NoConvergence& e) {
				silent_cerr("Dummy step " << iSubStep
					<< " does not converge; "
					"TimeStep=" << dCurrTimeStep
					<< " cannot be reduced further; "
					"aborting..." << std::endl);
				pDM->Output(0, dTime, dCurrTimeStep, iStIter, dTest, dSolTest, false, true);
				throw ErrMaxIterations(MBDYN_EXCEPT_ARGS);
			}

			catch (NonlinearSolver::ErrSimulationDiverged& e) {
				/*
				 * Mettere qui eventuali azioni speciali
				 * da intraprendere in caso di errore ...
		 		 */
				throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
			}
			catch (LinearSolver::ErrFactor& err) {
				/*
				 * Mettere qui eventuali azioni speciali
				 * da intraprendere in caso di errore ...
				 */
				silent_cerr("Dummy step " << iSubStep
					<< " failed because no pivot element "
					"could be found for column " << err.iCol
					<< " (" << pDM->GetDofDescription(err.iCol) << "); "
					"aborting..." << std::endl);
				throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
			}
			catch (NonlinearSolver::ConvergenceOnSolution& e) {
				bSolConv = true;
			}
			catch (EndOfSimulation& eos) {
				silent_cerr("Simulation ended during the dummy step " << iSubStep << " :\n"
					<< eos.what() << "\n");
				return false;
			}

#if 0
			/* don't sum up the derivatives error */
			dTotErr += dTest;
#endif
			iTotIter += iStIter;

#ifdef DEBUG
			if (DEBUG_LEVEL(MYDEBUG_FSTEPS)) {
				Out << "Step " << lStep
					<< " time " << dTime + dCurrTimeStep
					<< " step " << dCurrTimeStep
					<< " iterations " << iStIter
					<< " error " << dTest << std::endl;
			}
#endif /* DEBUG */

			DEBUGLCOUT(MYDEBUG_FSTEPS, "Substep " << iSubStep
				<< " of step " << lStep
				<< " has been successfully completed "
				"in " << iStIter << " iterations"
				<< std::endl);

			if (mbdyn_stop_at_end_of_time_step()) {
				/* */
#ifdef DEBUG_FICTITIOUS
				pDM->Output(0, dTime, dCurrTimeStep, iStIter, dTest, dSolTest, false);
#endif /* DEBUG_FICTITIOUS */
				Out << "Interrupted during dummy step"  << iSubStep << " ."
					<< std::endl;
				throw ErrInterrupted(MBDYN_EXCEPT_ARGS);
			}

			dTime += dRefTimeStep;
		}
		if (outputMsg()) {
			Out << "# Initial solution after dummy steps "
				"at time " << dTime
				<< " performed in " << iStIter
				<< " iterations with " << dTest
				<< " error" << std::endl;
		}

		DEBUGLCOUT(MYDEBUG_FSTEPS,
			"Dummy steps have been successfully completed "
			"in " << iStIter << " iterations" << std::endl);
#ifdef USE_EXTERNAL
	/* comunica che gli ultimi dati inviati sono la condizione iniziale */
		External::SendInitial();
#endif /* USE_EXTERNAL */
	} /* Fine dei passi fittizi */

	/* Output delle "condizioni iniziali" */
	bOut = pDM->Output(0, dTime, dCurrTimeStep, iStIter, dTest, dSolTest, bSolConv);

	if (outputMsg()) {
		Out
			<< "# Key for lines starting with \"Step\":"
				<< std::endl
			<< "# Step Time TStep NIter ResErr SolErr SolConv Out"
				<< std::endl
			<< "Step " << 0
			<< " " << dTime + dCurrTimeStep
			<< " " << dCurrTimeStep
			<< " " << iStIter
			<< " " << dTest
			<< " " << dSolTest
			<< " " << bSolConv
			<< " " << bOut
			<< std::endl;
	}

        if (pDummySteps) {
             SAFEDELETE(pDummySteps);
             pDummySteps = 0;
        }


	if (eAbortAfter == AFTER_DUMMY_STEPS) {
		Out << "End of dummy steps; no simulation is required."
			<< std::endl;
		return false;

	} else if (mbdyn_stop_at_end_of_time_step()) {
		/* Fa l'output della soluzione ed esce */
		Out << "Interrupted during dummy steps." << std::endl;
		throw ErrInterrupted(MBDYN_EXCEPT_ARGS);
	}

	eStatus = SOLVER_STATUS_PREPARED;

	return true;
}

bool
Solver::Start(void)
{
	DEBUGCOUTFNAME("Solver::Start");

#ifdef USE_MPI
        int mpi_finalize = 0;
#endif

	// consistency check
	if (eStatus != SOLVER_STATUS_PREPARED) {
		silent_cerr("Start() must be called after Prepare()" << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	/* primo passo regolare */

#ifdef USE_EXTERNAL
	/* il prossimo passo e' un regular */
	pNLS->SetExternal(External::REGULAR);
#endif /* USE_EXTERNAL */

	lStep = 0; /* Resetto di nuovo lStep */
	dRefTimeStep = dInitialTimeStep;
	dCurrTimeStep = dRefTimeStep;
	pTSC->Init(iMaxIterations, dMinTimeStep, MaxTimeStep, dInitialTimeStep);

	//DEBUGCOUT("Step " << lStep << " has been successfully completed "
	//		"in " << iStIter << " iterations" << std::endl);


	//DEBUGCOUT("Current time step: " << dCurrTimeStep << std::endl);
	//First start-up step for MS2, HOPE, MS3 and MS4
	if (RegularType == StepIntegratorType::INT_MS2  || RegularType == StepIntegratorType::INT_HOPE || RegularType == StepIntegratorType::INT_MS3 || RegularType == StepIntegratorType::INT_MS4 || RegularType == StepIntegratorType::INT_HYBRID)
	{
		lStep++;
		pDM->BeforePredict(*pX, *pXPrime, qX, qXPrime);

	Flip();
	dRefTimeStep = dInitialTimeStep;
	dCurrTimeStep = dRefTimeStep;

	CurrStep = StepIntegrator::NEWSTEP;
		//pTSC->Init(iMaxIterations, dMinTimeStep, MaxTimeStep, dInitialTimeStep);

	/* Setup SolutionManager(s) */
	ASSERT(pFirstRegularStep!= 0);
	SetupSolmans(pFirstRegularStep->GetIntegratorNumUnknownStates(), true);
	pCurrStepIntegrator = pFirstRegularStep;
        int retries = -1;
IfFirstStepIsToBeRepeated:
	try {
                retries++;
		pDM->SetTime(dTime + dCurrTimeStep, dCurrTimeStep, 1);
		if (outputStep()) {
			if (outputCounter()) {
				silent_cout(std::endl);
			}
 			silent_cout("Step(" << lStep << ':' << retries << ") t=" << dTime + dCurrTimeStep << " dt=" << dCurrTimeStep << std::endl);
		}
		dTest = pFirstRegularStep->Advance(this, dRefTimeStep,
				dCurrTimeStep/dRefTimeStep, CurrStep,
				qX, qXPrime, pX, pXPrime,
				iStIter, dTest, dSolTest);
	}
	catch (NonlinearSolver::NoConvergence& e) {
		if (dCurrTimeStep > dMinTimeStep) {
			/* Riduce il passo */
			CurrStep = StepIntegrator::REPEATSTEP;
			doublereal dOldCurrTimeStep = dCurrTimeStep;
			dCurrTimeStep = pTSC->dGetNewStepTime(CurrStep, iStIter);
			if (dCurrTimeStep < dOldCurrTimeStep) {
				DEBUGCOUT("Changing time step"
					" from " << dOldCurrTimeStep
					<< " to " << dCurrTimeStep
					<< " during first step after "
					<< iStIter << " iterations"
					<< std::endl);
				goto IfFirstStepIsToBeRepeated;
			}
		}

		silent_cerr("Max iterations number "
			<< std::abs(pFirstRegularStep->GetIntegratorMaxIters())
			<< " has been reached during "
				"first step, Time=" << dTime + dCurrTimeStep << "; "
			<< "TimeStep=" << dCurrTimeStep
			<< " cannot be reduced further; "
			"aborting..." << std::endl);
		pDM->Output(0, dTime, dCurrTimeStep, iStIter, dTest, dSolTest, bSolConv, true);

		throw Solver::ErrMaxIterations(MBDYN_EXCEPT_ARGS);
	}
	catch (NonlinearSolver::ErrSimulationDiverged& e) {
		/*
		 * Mettere qui eventuali azioni speciali
		 * da intraprendere in caso di errore ...
		 */

		throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
	}
	catch (LinearSolver::ErrFactor& err) {
		/*
		 * Mettere qui eventuali azioni speciali
		 * da intraprendere in caso di errore ...
		 */
		silent_cerr("First step failed because no pivot element "
			"could be found for column " << err.iCol
			<< " (" << pDM->GetDofDescription(err.iCol) << "); "
			"aborting..." << std::endl);
		throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
	}
	catch (NonlinearSolver::ConvergenceOnSolution& e) {
		bSolConv = true;
	}
	catch (EndOfSimulation& eos) {
		silent_cerr("Simulation ended during the first regular step:\n"
			<< eos.what() << "\n");
		return false;
	}

		DEBUGCOUT("Step " << lStep << " has been successfully completed "
				"in " << iStIter << " iterations" << std::endl);


	SAFEDELETE(pFirstRegularStep);
	pFirstRegularStep = 0;

	bOut = pDM->Output(lStep, dTime + dCurrTimeStep, dCurrTimeStep, iStIter, dTest, dSolTest, bSolConv);

	/* Si fa dare l'std::ostream al file di output per il log */
	std::ostream& Out = pDM->GetOutFile();

	if (mbdyn_stop_at_end_of_time_step()) {
		/* Fa l'output della soluzione al primo passo ed esce */
		Out << "Interrupted during first step." << std::endl;
		throw ErrInterrupted(MBDYN_EXCEPT_ARGS);
	}

	if (outputMsg()) {
		Out
			<< "Step " << lStep
			<< " " << dTime + dCurrTimeStep
			<< " " << dCurrTimeStep
			<< " " << iStIter
			<< " " << dTest
			<< " " << dSolTest
			<< " " << bSolConv
			<< " " << bOut
			<< std::endl;
	}

	bSolConv = false;

	dRefTimeStep = dCurrTimeStep;
	dTime += dRefTimeStep;

	dTotErr += dTest;
	iTotIter += iStIter;

        EigAll(Solver::EigAnTimeUntilNow);

	if (pRTSolver) {
		pRTSolver->Init();
	}

		dCurrTimeStep = pTSC->dGetNewStepTime(CurrStep, iStIter);
		DEBUGCOUT("Current time step: " << dCurrTimeStep << std::endl);
	}
	//First start-up step for MS2, HOPE, MS3 and MS4

	//Second start-up step for MS3 and MS4
	if (RegularType == StepIntegratorType::INT_MS3 || RegularType == StepIntegratorType::INT_MS4)
	{	
		ASSERT(pSecondRegularStep!= 0);
		SetupSolmans(pSecondRegularStep->GetIntegratorNumUnknownStates(), true);
		pCurrStepIntegrator = pSecondRegularStep;	
		CurrStep = StepIntegrator::NEWSTEP;
		
		if (pDM->EndOfSimulation() || dTime >= dFinalTime) {
			if (pRTSolver) {
				pRTSolver->StopCommanded();
			}
			silent_cout(outputCounterPrefix
				<< "End of simulation at time "
				<< dTime << " after "
				<< lStep << " steps;" << std::endl
				<< "output in file \"" << sOutputFileName << "\"" << std::endl
				<< "total iterations: " << iTotIter << std::endl
				<< "total Jacobian matrices: " << pNLS->TotalAssembledJacobian() << std::endl
				<< "total error: " << dTotErr << std::endl);

			if (pRTSolver) {
				pRTSolver->Log();
			}

			return false;

		} else if (pRTSolver && pRTSolver->IsStopCommanded()) {
			silent_cout(outputCounterPrefix
				<< "Simulation is stopped by RTAI task" << std::endl
				<< "Simulation ended at time "
				<< dTime << " after "
				<< lStep << " steps;" << std::endl
				<< "total iterations: " << iTotIter << std::endl
				<< "total Jacobian matrices: " << pNLS->TotalAssembledJacobian() << std::endl
				<< "total error: " << dTotErr << std::endl);
			pRTSolver->Log();
			return false;

		} else if (mbdyn_stop_at_end_of_time_step()
	#ifdef USE_MPI
			|| (MPI_Finalized(&mpi_finalize), mpi_finalize)
	#endif /* USE_MPI */
				)
		{
			if (pRTSolver) {
				pRTSolver->StopCommanded();
			}

			silent_cout(outputCounterPrefix
				<< "Interrupted!" << std::endl
				<< "Simulation ended at time "
				<< dTime << " after "
				<< lStep << " steps;" << std::endl
				<< "total iterations: " << iTotIter << std::endl
				<< "total Jacobian matrices: " << pNLS->TotalAssembledJacobian() << std::endl
				<< "total error: " << dTotErr << std::endl);

			if (pRTSolver) {
				pRTSolver->Log();
			}

			throw ErrInterrupted(MBDYN_EXCEPT_ARGS);
		}
		
		lStep++;
		pDM->BeforePredict(*pX, *pXPrime, qX, qXPrime);
		Flip();
		if (pRTSolver) {
			pRTSolver->Wait();
		}

		int retries = -1;
	IfSecondStepIsToBeRepeated:
		try {
			retries++;
			pDM->SetTime(dTime + dCurrTimeStep, dCurrTimeStep, lStep);
			if (outputStep()) {
				if (outputCounter()) {
					silent_cout(std::endl);
				}
 				silent_cout("Step(" << lStep << ':' << retries << ") t=" << dTime + dCurrTimeStep << " dt=" << dCurrTimeStep << std::endl);
			}
			dTest = pSecondRegularStep->Advance(this, dRefTimeStep,
					dCurrTimeStep/dRefTimeStep, CurrStep,
					qX, qXPrime, pX, pXPrime, iStIter,
					dTest, dSolTest);
		}
		catch (NonlinearSolver::NoConvergence& e) {
			if (dCurrTimeStep > dMinTimeStep) {
				/* Riduce il passo */
				CurrStep = StepIntegrator::REPEATSTEP;
				doublereal dOldCurrTimeStep = dCurrTimeStep;
				dCurrTimeStep = pTSC->dGetNewStepTime(CurrStep, iStIter);
				if (dCurrTimeStep < dOldCurrTimeStep) {
					DEBUGCOUT("Changing time step"
						" from " << dOldCurrTimeStep
						<< " to " << dCurrTimeStep
						<< " during step "
						<< lStep << " after "
						<< iStIter << " iterations"
						<< std::endl);
					goto IfSecondStepIsToBeRepeated;
				}
			}

			silent_cerr(outputCounterPrefix
				<< "Max iterations number "
				<< std::abs(pSecondRegularStep->GetIntegratorMaxIters())
				<< " has been reached during "
				"Step=" << lStep << ", "
				"Time=" << dTime + dCurrTimeStep << "; "
				"TimeStep=" << dCurrTimeStep
				<< " cannot be reduced further; "
				"aborting..." << std::endl);
			throw ErrMaxIterations(MBDYN_EXCEPT_ARGS);
		}
		catch (NonlinearSolver::ErrSimulationDiverged& e) {
			if (dCurrTimeStep > dMinTimeStep) {
				/* Riduce il passo */
				CurrStep = StepIntegrator::REPEATSTEP;
				doublereal dOldCurrTimeStep = dCurrTimeStep;
				dCurrTimeStep = pTSC->dGetNewStepTime(CurrStep, iStIter);
				if (dCurrTimeStep < dOldCurrTimeStep) {
					DEBUGCOUT("Changing time step"
						" from " << dOldCurrTimeStep
						<< " to " << dCurrTimeStep
						<< " during step "
						<< lStep << " after "
						<< iStIter << " iterations"
						<< std::endl);
					goto IfSecondStepIsToBeRepeated;
				}
			}

			silent_cerr(outputCounterPrefix
				<< "Simulation diverged after "
				<< iStIter << " iterations, before "
				"reaching max iteration number "
				<< std::abs(pSecondRegularStep->GetIntegratorMaxIters())
				<< " during Step=" << lStep << ", "
				"Time=" << dTime + dCurrTimeStep << "; "
				"TimeStep=" << dCurrTimeStep
				<< " cannot be reduced further; "
				"aborting..." << std::endl);
			throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
		}
		catch (LinearSolver::ErrFactor& err) {
		/*
		 * Mettere qui eventuali azioni speciali
		 * da intraprendere in caso di errore ...
		 */
			silent_cerr(outputCounterPrefix
				<< "Simulation failed because no pivot element "
				"could be found for column " << err.iCol
				<< " (" << pDM->GetDofDescription(err.iCol) << ") "
				"after " << iStIter << " iterations "
				"during step " << lStep << "; "
				"aborting..." << std::endl);
			throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
		}
		catch (NonlinearSolver::ConvergenceOnSolution& e) {
			bSolConv = true;
		}
		catch (EndOfSimulation& eos) {
			silent_cerr(outputCounterPrefix
				<< "Simulation ended during a regular step:\n"
				<< eos.what() << "\n");
	#ifdef USE_MPI
			MBDynComm.Abort(0);
	#endif /* USE_MPI */
			if (pRTSolver) {
				pRTSolver->StopCommanded();
			}

			silent_cout("Simulation ended at time "
				<< dTime << " after "
				<< lStep << " steps;" << std::endl
				<< "total iterations: " << iTotIter << std::endl
				<< "total Jacobian matrices: " << pNLS->TotalAssembledJacobian() << std::endl
				<< "total error: " << dTotErr << std::endl);

			if (pRTSolver) {
				pRTSolver->Log();
			}

			return false;
		}

		dTotErr += dTest;
		iTotIter += iStIter;

		bOut = pDM->Output(lStep, dTime + dCurrTimeStep, dCurrTimeStep, iStIter, dTest, dSolTest, bSolConv);
		/* Si fa dare l'std::ostream al file di output per il log */
		std::ostream& Out = pDM->GetOutFile();
		if (outputMsg()) {
			Out << "Step " << lStep
                        	<< " " << std::setprecision(16) << dTime + dCurrTimeStep
				<< " " << dCurrTimeStep
				<< " " << iStIter
				<< " " << dTest
				<< " " << dSolTest
				<< " " << bSolConv
				<< " " << bOut
				<< std::endl;
		}

		if (bOutputCounter) {
			silent_cout("Step " << std::setw(5) << lStep
				<< " " << std::setw(13) << dTime + dCurrTimeStep
				<< " " << std::setw(13) << dCurrTimeStep
				<< " " << std::setw(4) << iStIter
				<< " " << std::setw(13) << dTest
				<< " " << std::setw(13) << dSolTest
				<< " " << bSolConv
				<< " " << bOut
				<< outputCounterPostfix);
		}

		DEBUGCOUT("Step " << lStep
			<< " has been successfully completed "
			"in " << iStIter << " iterations" << std::endl);

		dRefTimeStep = dCurrTimeStep;
		dTime += dRefTimeStep;

		bSolConv = false;

                EigAll(Solver::EigAnTimeUntilNow, bOutputCounter);

		/* Calcola il nuovo timestep */
		dCurrTimeStep = pTSC->dGetNewStepTime(CurrStep, iStIter);
		DEBUGCOUT("Current time step: " << dCurrTimeStep << std::endl);
		
		SAFEDELETE(pSecondRegularStep);
		pSecondRegularStep = 0;
	}
	// Second start-up step for MS3 and MS4;	
	// Third start-up step for MS4;
	if (RegularType == StepIntegratorType::INT_MS4)
	{	
		ASSERT(pThirdRegularStep!= 0);
		SetupSolmans(pThirdRegularStep->GetIntegratorNumUnknownStates(), true);
		pCurrStepIntegrator = pThirdRegularStep;	
		CurrStep = StepIntegrator::NEWSTEP;	

		if (pDM->EndOfSimulation() || dTime >= dFinalTime) {
			if (pRTSolver) {
				pRTSolver->StopCommanded();
			}
			silent_cout(outputCounterPrefix
				<< "End of simulation at time "
				<< dTime << " after "
				<< lStep << " steps;" << std::endl
				<< "output in file \"" << sOutputFileName << "\"" << std::endl
				<< "total iterations: " << iTotIter << std::endl
				<< "total Jacobian matrices: " << pNLS->TotalAssembledJacobian() << std::endl
				<< "total error: " << dTotErr << std::endl);

			if (pRTSolver) {
				pRTSolver->Log();
			}

			return false;

			} else if (pRTSolver && pRTSolver->IsStopCommanded()) {
			silent_cout(outputCounterPrefix
				<< "Simulation is stopped by RTAI task" << std::endl
				<< "Simulation ended at time "
				<< dTime << " after "
				<< lStep << " steps;" << std::endl
				<< "total iterations: " << iTotIter << std::endl
				<< "total Jacobian matrices: " << pNLS->TotalAssembledJacobian() << std::endl
				<< "total error: " << dTotErr << std::endl);
			pRTSolver->Log();
			return false;

		} else if (mbdyn_stop_at_end_of_time_step()
	#ifdef USE_MPI
			|| (MPI_Finalized(&mpi_finalize), mpi_finalize)
	#endif /* USE_MPI */
				)
		{
			if (pRTSolver) {
				pRTSolver->StopCommanded();
			}

			silent_cout(outputCounterPrefix
				<< "Interrupted!" << std::endl
				<< "Simulation ended at time "
				<< dTime << " after "
				<< lStep << " steps;" << std::endl
				<< "total iterations: " << iTotIter << std::endl
				<< "total Jacobian matrices: " << pNLS->TotalAssembledJacobian() << std::endl
				<< "total error: " << dTotErr << std::endl);

			if (pRTSolver) {
				pRTSolver->Log();
			}

			throw ErrInterrupted(MBDYN_EXCEPT_ARGS);
		}

	
		lStep++;
		pDM->BeforePredict(*pX, *pXPrime, qX, qXPrime);

		Flip();

		if (pRTSolver) {
			pRTSolver->Wait();
		}

		int retries = -1;
	IfThirdStepIsToBeRepeated:
		try {
			retries++;
			pDM->SetTime(dTime + dCurrTimeStep, dCurrTimeStep, lStep);
			if (outputStep()) {
				if (outputCounter()) {
					silent_cout(std::endl);
				}
 				silent_cout("Step(" << lStep << ':' << retries << ") t=" << dTime + dCurrTimeStep << " dt=" << dCurrTimeStep << std::endl);
			}
			dTest = pThirdRegularStep->Advance(this, dRefTimeStep,
					dCurrTimeStep/dRefTimeStep, CurrStep,
					qX, qXPrime, pX, pXPrime, iStIter,
					dTest, dSolTest);
		}
		catch (NonlinearSolver::NoConvergence& e) {
			if (dCurrTimeStep > dMinTimeStep) {
				/* Riduce il passo */
				CurrStep = StepIntegrator::REPEATSTEP;
				doublereal dOldCurrTimeStep = dCurrTimeStep;
				dCurrTimeStep = pTSC->dGetNewStepTime(CurrStep, iStIter);
				if (dCurrTimeStep < dOldCurrTimeStep) {
					DEBUGCOUT("Changing time step"
						" from " << dOldCurrTimeStep
						<< " to " << dCurrTimeStep
						<< " during step "
						<< lStep << " after "
						<< iStIter << " iterations"
						<< std::endl);
					goto IfThirdStepIsToBeRepeated;
				}
			}

			silent_cerr(outputCounterPrefix
				<< "Max iterations number "
				<< std::abs(pThirdRegularStep->GetIntegratorMaxIters())
				<< " has been reached during "
				"Step=" << lStep << ", "
				"Time=" << dTime + dCurrTimeStep << "; "
				"TimeStep=" << dCurrTimeStep
				<< " cannot be reduced further; "
				"aborting..." << std::endl);
			throw ErrMaxIterations(MBDYN_EXCEPT_ARGS);
		}
		catch (NonlinearSolver::ErrSimulationDiverged& e) {
			if (dCurrTimeStep > dMinTimeStep) {
				/* Riduce il passo */
				CurrStep = StepIntegrator::REPEATSTEP;
				doublereal dOldCurrTimeStep = dCurrTimeStep;
				dCurrTimeStep = pTSC->dGetNewStepTime(CurrStep, iStIter);
				if (dCurrTimeStep < dOldCurrTimeStep) {
					DEBUGCOUT("Changing time step"
						" from " << dOldCurrTimeStep
						<< " to " << dCurrTimeStep
						<< " during step "
						<< lStep << " after "
						<< iStIter << " iterations"
						<< std::endl);
					goto IfThirdStepIsToBeRepeated;
				}
			}

			silent_cerr(outputCounterPrefix
				<< "Simulation diverged after "
				<< iStIter << " iterations, before "
				"reaching max iteration number "
				<< std::abs(pThirdRegularStep->GetIntegratorMaxIters())
				<< " during Step=" << lStep << ", "
				"Time=" << dTime + dCurrTimeStep << "; "
				"TimeStep=" << dCurrTimeStep
				<< " cannot be reduced further; "
				"aborting..." << std::endl);
			throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
		}
		catch (LinearSolver::ErrFactor& err) {
		/*
		 * Mettere qui eventuali azioni speciali
		 * da intraprendere in caso di errore ...
		 */
			silent_cerr(outputCounterPrefix
				<< "Simulation failed because no pivot element "
				"could be found for column " << err.iCol
				<< " (" << pDM->GetDofDescription(err.iCol) << ") "
				"after " << iStIter << " iterations "
				"during step " << lStep << "; "
				"aborting..." << std::endl);
			throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
		}
		catch (NonlinearSolver::ConvergenceOnSolution& e) {
			bSolConv = true;
		}
		catch (EndOfSimulation& eos) {
			silent_cerr(outputCounterPrefix
				<< "Simulation ended during a regular step:\n"
				<< eos.what() << "\n");
	#ifdef USE_MPI
			MBDynComm.Abort(0);
	#endif /* USE_MPI */
			if (pRTSolver) {
				pRTSolver->StopCommanded();
			}

			silent_cout("Simulation ended at time "
				<< dTime << " after "
				<< lStep << " steps;" << std::endl
				<< "total iterations: " << iTotIter << std::endl
				<< "total Jacobian matrices: " << pNLS->TotalAssembledJacobian() << std::endl
				<< "total error: " << dTotErr << std::endl);

			if (pRTSolver) {
				pRTSolver->Log();
			}

			return false;
		}

		dTotErr += dTest;
		iTotIter += iStIter;

		bOut = pDM->Output(lStep, dTime + dCurrTimeStep, dCurrTimeStep, iStIter, dTest, dSolTest, bSolConv);

		/* Si fa dare l'std::ostream al file di output per il log */
		std::ostream& Out = pDM->GetOutFile();

		if (outputMsg()) {
			Out << "Step " << lStep
                        	<< " " << std::setprecision(16) << dTime + dCurrTimeStep
				<< " " << dCurrTimeStep
				<< " " << iStIter
				<< " " << dTest
				<< " " << dSolTest
				<< " " << bSolConv
				<< " " << bOut
				<< std::endl;
		}

		if (bOutputCounter) {
			silent_cout("Step " << std::setw(5) << lStep
				<< " " << std::setw(13) << dTime + dCurrTimeStep
				<< " " << std::setw(13) << dCurrTimeStep
				<< " " << std::setw(4) << iStIter
				<< " " << std::setw(13) << dTest
				<< " " << std::setw(13) << dSolTest
				<< " " << bSolConv
				<< " " << bOut
				<< outputCounterPostfix);
		}

		DEBUGCOUT("Step " << lStep
			<< " has been successfully completed "
			"in " << iStIter << " iterations" << std::endl);

		dRefTimeStep = dCurrTimeStep;
		dTime += dRefTimeStep;

		bSolConv = false;

                EigAll(Solver::EigAnTimeUntilNow, bOutputCounter);

		/* Calcola il nuovo timestep */
		dCurrTimeStep = pTSC->dGetNewStepTime(CurrStep, iStIter);
		DEBUGCOUT("Current time step: " << dCurrTimeStep << std::endl);
		
		SAFEDELETE(pThirdRegularStep);
		pThirdRegularStep = 0;
	}
	//Third start-up step for MS4
	/* Altri passi regolari */
	ASSERT(pRegularSteps != 0);

	/* Setup SolutionManager(s) */
	SetupSolmans(pRegularSteps->GetIntegratorNumUnknownStates(), true);
	pCurrStepIntegrator = pRegularSteps;

	eStatus = SOLVER_STATUS_STARTED;

	return true;
}

bool
Solver::Advance(void)
{
	DEBUGCOUTFNAME("Solver::Advance");
        
#ifdef USE_MPI
	int mpi_finalize = 0;
#endif /* USE_MPI */

	// consistency check
	if (eStatus != SOLVER_STATUS_STARTED) {
		silent_cerr("Started() must be called first" << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	CurrStep = StepIntegrator::NEWSTEP;

        if (eAbortAfter >= AFTER_REGULAR_STEP_0 && lStep >= eAbortAfter - AFTER_REGULAR_STEP_0) {
                silent_cout("End of step " << lStep << "; no simulation is required.\n");
                dFinalTime = dTime;
        }
        
	if (pDM->EndOfSimulation() || dTime >= dFinalTime) {
                // Make sure to execute all eigenanalyses after the end of the simulation
                DEBUGCERR("Executing all remaining eigenanalyses\n");
                EigAll(Solver::EigAnTimeAllRemaining, bOutputCounter);

		if (pRTSolver) {
			pRTSolver->StopCommanded();
		}
		silent_cout(outputCounterPrefix
			<< "End of simulation at time "
			<< dTime << " after "
			<< lStep << " steps;" << std::endl
			<< "output in file \"" << sOutputFileName << "\"" << std::endl
			<< "total iterations: " << iTotIter << std::endl
			<< "total Jacobian matrices: " << pNLS->TotalAssembledJacobian() << std::endl
			<< "total error: " << dTotErr << std::endl);

		if (pRTSolver) {
			pRTSolver->Log();
		}

		return false;

	} else if (pRTSolver && pRTSolver->IsStopCommanded()) {
		silent_cout(outputCounterPrefix
			<< "Simulation is stopped by RTAI task" << std::endl
			<< "Simulation ended at time "
			<< dTime << " after "
			<< lStep << " steps;" << std::endl
			<< "total iterations: " << iTotIter << std::endl
			<< "total Jacobian matrices: " << pNLS->TotalAssembledJacobian() << std::endl
			<< "total error: " << dTotErr << std::endl);
		pRTSolver->Log();
		return false;

	} else if (mbdyn_stop_at_end_of_time_step()
#ifdef USE_MPI
		|| (MPI_Finalized(&mpi_finalize), mpi_finalize)
#endif /* USE_MPI */
			)
	{
		if (pRTSolver) {
			pRTSolver->StopCommanded();
		}

		silent_cout(outputCounterPrefix
			<< "Interrupted!" << std::endl
			<< "Simulation ended at time "
			<< dTime << " after "
			<< lStep << " steps;" << std::endl
			<< "total iterations: " << iTotIter << std::endl
			<< "total Jacobian matrices: " << pNLS->TotalAssembledJacobian() << std::endl
			<< "total error: " << dTotErr << std::endl);

		if (pRTSolver) {
			pRTSolver->Log();
		}

		throw ErrInterrupted(MBDYN_EXCEPT_ARGS);
	}

	lStep++;
	pDM->BeforePredict(*pX, *pXPrime, qX, qXPrime);

	Flip();

	if (pRTSolver) {
		pRTSolver->Wait();
	}

	int retries = -1;
IfStepIsToBeRepeated:
	try {
		retries++;
		pDM->SetTime(dTime + dCurrTimeStep, dCurrTimeStep, lStep);
		if (outputStep()) {
			if (outputCounter()) {
				silent_cout(std::endl);
			}
 			silent_cout("Step(" << lStep << ':' << retries << ") t=" << dTime + dCurrTimeStep << " dt=" << dCurrTimeStep << std::endl);
		}
		dTest = pRegularSteps->Advance(this, dRefTimeStep,
				dCurrTimeStep/dRefTimeStep, CurrStep,
				qX, qXPrime, pX, pXPrime, iStIter,
				dTest, dSolTest);
	}
	catch (NonlinearSolver::NoConvergence& e) {
		if (dCurrTimeStep > dMinTimeStep) {
			/* Riduce il passo */
			CurrStep = StepIntegrator::REPEATSTEP;
			doublereal dOldCurrTimeStep = dCurrTimeStep;
			dCurrTimeStep = pTSC->dGetNewStepTime(CurrStep, iStIter);
			if (dCurrTimeStep < dOldCurrTimeStep) {
				DEBUGCOUT("Changing time step"
					" from " << dOldCurrTimeStep
					<< " to " << dCurrTimeStep
					<< " during step "
					<< lStep << " after "
					<< iStIter << " iterations"
					<< std::endl);
				goto IfStepIsToBeRepeated;
			}
		}

		silent_cerr(outputCounterPrefix
			<< "Max iterations number "
			<< std::abs(pRegularSteps->GetIntegratorMaxIters())
			<< " has been reached during "
			"Step=" << lStep << ", "
			"Time=" << dTime + dCurrTimeStep << "; "
			"TimeStep=" << dCurrTimeStep
			<< " cannot be reduced further; "
			"aborting..." << std::endl);
		throw ErrMaxIterations(MBDYN_EXCEPT_ARGS);
	}
	catch (NonlinearSolver::ErrSimulationDiverged& e) {
		if (dCurrTimeStep > dMinTimeStep) {
			/* Riduce il passo */
			CurrStep = StepIntegrator::REPEATSTEP;
			doublereal dOldCurrTimeStep = dCurrTimeStep;
			dCurrTimeStep = pTSC->dGetNewStepTime(CurrStep, iStIter);
			if (dCurrTimeStep < dOldCurrTimeStep) {
				DEBUGCOUT("Changing time step"
					" from " << dOldCurrTimeStep
					<< " to " << dCurrTimeStep
					<< " during step "
					<< lStep << " after "
					<< iStIter << " iterations"
					<< std::endl);
				goto IfStepIsToBeRepeated;
			}
		}

		silent_cerr(outputCounterPrefix
			<< "Simulation diverged after "
			<< iStIter << " iterations, before "
			"reaching max iteration number "
			<< std::abs(pRegularSteps->GetIntegratorMaxIters())
			<< " during Step=" << lStep << ", "
			"Time=" << dTime + dCurrTimeStep << "; "
			"TimeStep=" << dCurrTimeStep
			<< " cannot be reduced further; "
			"aborting..." << std::endl);
		throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
	}
	catch (LinearSolver::ErrFactor& err) {
		/*
		 * Mettere qui eventuali azioni speciali
		 * da intraprendere in caso di errore ...
		 */
		silent_cerr(outputCounterPrefix
			<< "Simulation failed because no pivot element "
			"could be found for column " << err.iCol
			<< " (" << pDM->GetDofDescription(err.iCol) << ") "
			"after " << iStIter << " iterations "
			"during step " << lStep << "; "
			"aborting..." << std::endl);
		throw SimulationDiverged(MBDYN_EXCEPT_ARGS);
	}
	catch (NonlinearSolver::ConvergenceOnSolution& e) {
		bSolConv = true;
	}
	catch (EndOfSimulation& eos) {
		silent_cerr(outputCounterPrefix
			<< "Simulation ended during a regular step:\n"
			<< eos.what() << "\n");
#ifdef USE_MPI
		MBDynComm.Abort(0);
#endif /* USE_MPI */
		if (pRTSolver) {
			pRTSolver->StopCommanded();
		}

		silent_cout("Simulation ended at time "
			<< dTime << " after "
			<< lStep << " steps;" << std::endl
			<< "total iterations: " << iTotIter << std::endl
			<< "total Jacobian matrices: " << pNLS->TotalAssembledJacobian() << std::endl
			<< "total error: " << dTotErr << std::endl);

		if (pRTSolver) {
			pRTSolver->Log();
		}

		return false;
	}

	dTotErr += dTest;
	iTotIter += iStIter;

	bOut = pDM->Output(lStep, dTime + dCurrTimeStep, dCurrTimeStep, iStIter, dTest, dSolTest, bSolConv);

	/* Si fa dare l'std::ostream al file di output per il log */
	std::ostream& Out = pDM->GetOutFile();

	if (outputMsg()) {
		Out << "Step " << lStep
                        << " " << std::setprecision(16) << dTime + dCurrTimeStep
			<< " " << dCurrTimeStep
			<< " " << iStIter
			<< " " << dTest
			<< " " << dSolTest
			<< " " << bSolConv
			<< " " << bOut
			<< std::endl;
	}

	if (bOutputCounter) {
		silent_cout("Step " << std::setw(5) << lStep
			<< " " << std::setw(13) << dTime + dCurrTimeStep
			<< " " << std::setw(13) << dCurrTimeStep
			<< " " << std::setw(4) << iStIter
			<< " " << std::setw(13) << dTest
			<< " " << std::setw(13) << dSolTest
			<< " " << bSolConv
			<< " " << bOut
			<< outputCounterPostfix);
	}

	DEBUGCOUT("Step " << lStep
		<< " has been successfully completed "
		"in " << iStIter << " iterations" << std::endl);

	dRefTimeStep = dCurrTimeStep;
	dTime += dRefTimeStep;

	bSolConv = false;

        EigAll(Solver::EigAnTimeUntilNow, bOutputCounter);

	/* Calcola il nuovo timestep */
	dCurrTimeStep = pTSC->dGetNewStepTime(CurrStep, iStIter);
	DEBUGCOUT("Current time step: " << dCurrTimeStep << std::endl);
       
	return true;
}

void
Solver::Run(void)
{
	DEBUGCOUTFNAME("Solver::Run");

	if (Prepare()) {
		if (Start()) {
			while (Advance()) {
				NO_OP;
			}
		}
	}
}

/* Distruttore */
Solver::~Solver(void)
{
	DEBUGCOUTFNAME("Solver::~Solver");

	if (!qX.empty()) {
		integer temp_qx_size = qX.size();
		for (int ivec = 0; ivec < temp_qx_size; ivec++)
		{
			if (qX[ivec] != 0) {
				SAFEDELETE(qX[ivec]);
			}
		}
	}

	if (!qXPrime.empty()) {
		integer temp_qxprime_size = qXPrime.size();
		for (int ivec = 0; ivec < temp_qxprime_size; ivec++)
		{
			if (qXPrime[ivec] != 0)
			{
				SAFEDELETE(qXPrime[ivec]);
			}
		}
	}

	if (pX) {
		SAFEDELETE(pX);
	}

	if (pXPrime) {
		SAFEDELETE(pXPrime);
	}

	if (pdWorkSpace) {
		SAFEDELETEARR(pdWorkSpace);
	}

	if (pDM) {
		SAFEDELETE(pDM);
	}

	if (pRTSolver) {
		SAFEDELETE(pRTSolver);
	}

	if (pDerivativeSteps) {
		SAFEDELETE(pDerivativeSteps);
	}

	if (pFirstDummyStep) {
		SAFEDELETE(pFirstDummyStep);
	}

	if (pSecondDummyStep) {
		SAFEDELETE(pSecondDummyStep);
	}

	if (pThirdDummyStep) {
		SAFEDELETE(pThirdDummyStep);
	}

	if (pDummySteps) {
		SAFEDELETE(pDummySteps);
	}

	if (pFirstRegularStep) {
		SAFEDELETE(pFirstRegularStep);
	}

	if (pSecondRegularStep) {
		SAFEDELETE(pSecondRegularStep);
	}

	if (pThirdRegularStep) {
		SAFEDELETE(pThirdRegularStep);
	}

	if (pRegularSteps) {
		SAFEDELETE(pRegularSteps);
	}

	if (pSM) {
		SAFEDELETE(pSM);
	}

	if (pNLS) {
		SAFEDELETE(pNLS);
	}

	if (pTSC) {
		delete pTSC;
	}

	DestroyTimeStepData();
}

/*scrive il contributo al file di restart*/
std::ostream &
Solver::Restart(std::ostream& out,DataManager::eRestartWhen type) const
{

	out << "begin: initial value;" << std::endl;
	switch(type) {
	case DataManager::ATEND:
		out << "  #  initial time: " << pDM->dGetTime() << ";"
			<< std::endl
			<< "  #  final time: " << dFinalTime << ";"
			<< std::endl
			<< "  #  time step: " << dInitialTimeStep << ";"
			<< std::endl;
		break;
	case DataManager::ITERATIONS:
	case DataManager::TIME:
	case DataManager::TIMES:
		out << "  initial time: " << pDM->dGetTime()<< ";" << std::endl
			<< "  final time: " << dFinalTime << ";" << std::endl
			<< "  time step: " << dInitialTimeStep << ";"
			<< std::endl;
		break;
	default:
		ASSERT(0);
	}

	out << "  method: ";
	switch(RegularType) {
	case INT_CRANKNICOLSON:
		out << "Crank Nicolson; " << std::endl;
		break;
	case INT_MS2:
		out << "ms, ";
		pRhoRegular->Restart(out) << ", ";
		pRhoAlgebraicRegular->Restart(out) << ";" << std::endl;
		break;
	case INT_MS3:
		out << "ms3, ";
		pRhoRegular->Restart(out) << ", ";
		pRhoAlgebraicRegular->Restart(out) << ";" << std::endl;
		break;
	case INT_MS4:
		out << "ms4, ";
		pRhoRegular->Restart(out) << ", ";
		pRhoAlgebraicRegular->Restart(out) << ";" << std::endl;
		break;
	case INT_SS2:
		out << "ss2, ";
		pRhoRegular->Restart(out) << ", ";
		pRhoAlgebraicRegular->Restart(out) << ";" << std::endl;
		break;
	case INT_SS3:
		out << "ss3, ";
		pRhoRegular->Restart(out) << ", ";
		pRhoAlgebraicRegular->Restart(out) << ";" << std::endl;
		break;
	case INT_SS4:
		out << "ss4, ";
		pRhoRegular->Restart(out) << ", ";
		pRhoAlgebraicRegular->Restart(out) << ";" << std::endl;
		break;
	case INT_HOPE:
		out << "hope, ";
		pRhoRegular->Restart(out) << ", ";
		pRhoAlgebraicRegular->Restart(out) << ";" << std::endl;
		break;
	case INT_BATHE:
		out << "Bathe, ";
		pRhoRegular->Restart(out) << ", ";
		pRhoAlgebraicRegular->Restart(out) << ";" << std::endl;
		break;
	case INT_MSSTC3:
		out << "msstc3, ";
		pRhoRegular->Restart(out) << ", ";
		pRhoAlgebraicRegular->Restart(out) << ";" << std::endl;
		break;
	case INT_MSSTH3:
		out << "mssth3, ";
		pRhoRegular->Restart(out) << ", ";
		pRhoAlgebraicRegular->Restart(out) << ";" << std::endl;
		break;
	case INT_MSSTC4:
		out << "msstc4, ";
		pRhoRegular->Restart(out) << ", ";
		pRhoAlgebraicRegular->Restart(out) << ";" << std::endl;
		break;
	case INT_MSSTH4:
		out << "mssth4, ";
		pRhoRegular->Restart(out) << ", ";
		pRhoAlgebraicRegular->Restart(out) << ";" << std::endl;
		break;
	case INT_MSSTC5:
		out << "msstc5, ";
		pRhoRegular->Restart(out) << ", ";
		pRhoAlgebraicRegular->Restart(out) << ";" << std::endl;
		break;
	case INT_MSSTH5:
		out << "mssth5, ";
		pRhoRegular->Restart(out) << ", ";
		pRhoAlgebraicRegular->Restart(out) << ";" << std::endl;
		break;
	case INT_DIRK33:
		out << "DIRK33; " << std::endl;
		break;
	case INT_DIRK43:
		out << "DIRK43; " << std::endl;
		break;
	case INT_DIRK54:
		out << "DIRK54; " << std::endl;
		break;
	case INT_IMPLICITEULER:
		out << "implicit euler;" << std::endl;
		break;
	default:
		ASSERT(0);
	}

	integer iMI = pRegularSteps->GetIntegratorMaxIters();
	out << "  max iterations: " << std::abs(iMI);
	if (iMI < 0) out << ", at most";
	out
		<< ";" << std::endl
		<< "  tolerance: " << pRegularSteps->GetIntegratorDTol();
	switch(ResTest) {
	case NonlinearSolverTest::NORM:
		out << ", test, norm" ;
		break;
	case NonlinearSolverTest::MINMAX:
		out << ", test, minmax" ;
		break;
	case NonlinearSolverTest::NONE:
		NO_OP;
	default:
		silent_cerr("unhandled nonlinear solver test type" << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	if (bScale) {
		out << ", scale"
			<< ", " << pRegularSteps->GetIntegratorDSolTol();
	}

	switch (SolTest) {
	case NonlinearSolverTest::NORM:
		out << ", test, norm" ;
		break;
	case NonlinearSolverTest::MINMAX:
		out << ", test, minmax" ;
		break;
	case NonlinearSolverTest::NONE:
                break;
	default:
		ASSERT(0);
	}
	out
		<< ";" << std::endl;

	if ( pDerivativeSteps != 0 ) {
		out
			<< "  derivatives max iterations: " << pDerivativeSteps->GetIntegratorMaxIters() << ";" << std::endl
			<< "  derivatives tolerance: " << pDerivativeSteps->GetIntegratorDTol() << ";" << std::endl
			<< "  derivatives coefficient: " << dDerivativesCoef << ";" << std::endl;
	}

	if (iDummyStepsNumber) {
		out
			<< "  dummy steps max iterations: " << pDummySteps->GetIntegratorMaxIters() << ";" << std::endl
			<< "  dummy steps tolerance: " << pDummySteps->GetIntegratorDTol() << ";" << std::endl;
	}
	out
		<< "  dummy steps number: " << iDummyStepsNumber << ";" << std::endl
		<< "  dummy steps ratio: " << dDummyStepsRatio << ";" << std::endl;
	switch (NonlinearSolverType) {
	case NonlinearSolver::MATRIXFREE:
		out << "  #  nonlinear solver: matrix free;" << std::endl;
		break;
	case NonlinearSolver::NEWTONRAPHSON:
	default :
		out << "  nonlinear solver: newton raphson";
		if (!bTrueNewtonRaphson) {
                        out << ", modified, " << oLineSearchParam.iIterationsBeforeAssembly;
                        if (oLineSearchParam.bKeepJacAcrossSteps) {
				out << ", keep jacobian matrix";
			}
			if (bHonorJacRequest) {
				out << ", honor element requests";
			}
		}
		out << ";" << std::endl;
	}
	out << "  solver: ";
	RestartLinSol(out, CurrLinearSolver);
	out << "end: initial value;" << std::endl << std::endl;
	return out;
}

void Solver::Restart(RestartData& oData, RestartData::RestartAction eAction)
{
     // FIXME: During the execution of the constructor of DataManager, pDM will be a null pointer
     double dRestartTime = pDM ? pDM->dGetTime() : dTime; 

     oData.Sync(RestartData::INITVAL_TIME, 0, "dTime", dRestartTime, eAction);

     if (eAction == RestartData::RESTART_RESTORE) {
          dTime = dInitialTime = dRestartTime;
     }
}
/* Dati dell'integratore */
void
Solver::ReadData(MBDynParser& HP)
{
	DEBUGCOUTFNAME("MultiStepIntegrator::ReadData");

	/* parole chiave */
	static const char*const sKeyWords[] = {
		"begin",
		"initial" "value",
		"multistep",		/* deprecated */
		"end",

		"initial" "time",
		"final" "time",
		"time" "step",
		"min" "time" "step",
		"max" "time" "step",
		"tolerance",
		"max" "residual",
		"max" "iterations",
		"modify" "residual" "test",
		"enforce" "constraint" "equations",
		/* DEPRECATED */
		"fictitious" "steps" "number",
		"fictitious" "steps" "ratio",
		"fictitious" "steps" "tolerance",
		"fictitious" "steps" "max" "iterations",
		/* END OF DEPRECATED */

		"dummy" "steps" "number",
		"dummy" "steps" "ratio",
		"dummy" "steps" "tolerance",
		"dummy" "steps" "max" "iterations",

		"abort" "after",
			"input",
			"assembly",
			"derivatives",

			/* DEPRECATED */ "fictitious" "steps" /* END OF DEPRECATED */ ,
			"dummy" "steps",
                        "regular" "step",
		"output",
			"none",
			"iterations",
			"residual",
			"solution",
			/* DEPRECATED */ "jacobian" /* END OF DEPRECATED */ ,
			"jacobian" "matrix",
			"bailout",
			"messages",
			"counter",
			"matrix" "condition" "number",
			"solver" "condition" "number",
			"cpu" "time",
		"output" "meter",

		"method",
		/* DEPRECATED */ "fictitious" "steps" "method" /* END OF DEPRECATED */ ,
		"dummy" "steps" "method",

		"Crank" "Nicolson",
		/* DEPRECATED */ "Crank" "Nicholson" /* END OF DEPRECATED */ ,
			/* DEPRECATED */ "nostro" /* END OF DEPRECATED */ ,
			"ms",
			"ms2",
			"ms3",
			"ms4",
			"ss2",
			"ss3",
			"ss4",
			"hope",
			"Bathe",
			"msstc3",
			"mssth3",
			"msstc4",
			"mssth4",
			"msstc5",
			"mssth5",
			"DIRK33",
			"DIRK43",
			"DIRK54",
			"bdf",
			"implicit" "euler",
                        "hybrid",

		"derivatives" "coefficient",
		"derivatives" "tolerance",
		"derivatives" "max" "iterations",

		/* DEPRECATED */
		"true",
		"modified",
		/* END OF DEPRECATED */

		"strategy",
			"factor",
			"no" "change",
			"change",

		"pod",
		"eigen" "analysis",

		/* DEPRECATED */
		"solver",
		"interface" "solver",
		/* END OF DEPRECATED */
		"linear" "solver",
		"interface" "linear" "solver",

		/* DEPRECATED */
		"preconditioner",
		/* END OF DEPRECATED */

		"nonlinear" "solver",
			"default",
			"newton" "raphson",
			"line" "search",
                        "bfgs",
                        "mcp" "newton" "fb",
                        "mcp" "newton" "min" "fb",
                        "siconos" "mcp" "newton" "fb",
                        "siconos" "mcp" "newton" "min" "fb",
                        "nox",
			"matrix" "free",
				"bicgstab",
				"gmres",
					/* DEPRECATED */ "full" "jacobian" /* END OF DEPRECATED */ ,
					"full" "jacobian" "matrix",

		/* RTAI stuff */
		"real" "time",

		/* multithread stuff */
		"threads",

		NULL
	};

	/* enum delle parole chiave */
	enum KeyWords {
		UNKNOWN = -1,
		BEGIN = 0,
		INITIAL_VALUE,
		MULTISTEP,
		END,

		INITIALTIME,
		FINALTIME,
		TIMESTEP,
		MINTIMESTEP,
		MAXTIMESTEP,
		TOLERANCE,
		MAXRESIDUAL,
		MAXITERATIONS,
		MODIFY_RES_TEST,
		ENFORCE_CONSTRAINT_EQUATIONS,
		FICTITIOUSSTEPSNUMBER,
		FICTITIOUSSTEPSRATIO,
		FICTITIOUSSTEPSTOLERANCE,
		FICTITIOUSSTEPSMAXITERATIONS,

		DUMMYSTEPSNUMBER,
		DUMMYSTEPSRATIO,
		DUMMYSTEPSTOLERANCE,
		DUMMYSTEPSMAXITERATIONS,

		ABORTAFTER,
			INPUT,
			ASSEMBLY,
			DERIVATIVES,
			FICTITIOUSSTEPS,
			DUMMYSTEPS,
                        REGULARSTEP,
		OUTPUT,
			NONE,
			ITERATIONS,
			RESIDUAL,
			SOLUTION,
			JACOBIAN,
			JACOBIANMATRIX,
			BAILOUT,
			MESSAGES,
			COUNTER,
			MATRIX_COND_NUM,
			SOLVER_COND_NUM,
			CPU_TIME,
		OUTPUTMETER,

		METHOD,
		FICTITIOUSSTEPSMETHOD,
		DUMMYSTEPSMETHOD,
		CRANKNICOLSON,
		CRANKNICHOLSON,
		NOSTRO,
		MS,
		MS2,
		MS3,
		MS4,
		SS2,
		SS3,
		SS4,
		HOPE,
		BATHE,
		MSSTC3,
		MSSTH3,
		MSSTC4,
		MSSTH4,
		MSSTC5,
		MSSTH5,
		DIRK33,
		DIRK43,
		DIRK54,
		BDF,
		IMPLICITEULER,
                HYBRID,

		DERIVATIVESCOEFFICIENT,
		DERIVATIVESTOLERANCE,
		DERIVATIVESMAXITERATIONS,

		/* DEPRECATED */
		NR_TRUE,
		MODIFIED,
		/* END OF DEPRECATED */

		STRATEGY,
		STRATEGYFACTOR,
		STRATEGYNOCHANGE,
		STRATEGYCHANGE,

		POD,
		EIGENANALYSIS,

		/* DEPRECATED */
		SOLVER,
		INTERFACESOLVER,
		/* END OF DEPRECATED */
		LINEARSOLVER,
		INTERFACELINEARSOLVER,

		/* DEPRECATED */
		PRECONDITIONER,
		/* END OF DEPRECATED */

		NONLINEARSOLVER,
			DEFAULT,
			NEWTONRAPHSON,
			LINESEARCH,
                        BFGS,
                        MCP_NEWTON_FB,
                        MCP_NEWTON_MIN_FB,
                        SICONOS_MCP_NEWTON_FB,
                        SICONOS_MCP_NEWTON_MIN_FB,
                        NOX,
			MATRIXFREE,
				BICGSTAB,
				GMRES,
					FULLJACOBIAN,
					FULLJACOBIANMATRIX,

		/* RTAI stuff */
		REALTIME,

		THREADS,

		LASTKEYWORD
	};

	/* tabella delle parole chiave */
	KeyTable K(HP, sKeyWords);

	/* legge i dati della simulazione */
	if (KeyWords(HP.GetDescription()) != BEGIN) {
		silent_cerr("Error: <begin> expected at line "
			<< HP.GetLineData() << "; aborting..." << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	switch (KeyWords(HP.GetWord())) {
	case MULTISTEP:
		pedantic_cout("warning: \"begin: multistep\" is deprecated; "
			"use \"begin: initial value;\" instead." << std::endl);
	case INITIAL_VALUE:
		break;

	default:
		silent_cerr("Error: \"begin: initial value;\" expected at line "
			<< HP.GetLineData() << "; aborting..." << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	bool bMethod(false);
	bool bDummyStepsMethod(false);
	StepIntegratorType eHybridDefaultIntRegular(INT_MS2);

	/* dati letti qui ma da passare alle classi
	 *	StepIntegration e NonlinearSolver
	 */

	doublereal dTol = ::dDefaultTol;
	doublereal dSolutionTol = 0.;
	iMaxIterations = ::iDefaultMaxIterations;
	bool bModResTest = false;
	bool bSetScaleAlgebraic = false;

	/* Dati dei passi fittizi di trimmaggio iniziale */
	doublereal dDummyStepsTolerance = ::dDefaultDummyStepsTolerance;
	integer iDummyStepsMaxIterations = ::iDefaultMaxIterations;

	/* Dati del passo iniziale di calcolo delle derivate */

	doublereal dDerivativesTol = ::dDefaultTol;
        doublereal dDerivativesSolTol = -1.;
	integer iDerivativesMaxIterations = ::iDefaultMaxIterations;
	integer iDerivativesCoefMaxIter = 0;
	doublereal dDerivativesCoefFactor = 10.;

#ifdef USE_MULTITHREAD
	bool bSolverThreads(false);
	unsigned nSolverThreads = 0;
#endif /* USE_MULTITHREAD */

        bool bEigAnUserDefinedParam = false;
        bool bEigAnUserDefinedLowerFreq = false;
        bool bEigAnUserDefinedUpperFreq = false;
        bool bEigAnUserDefinedWhich = false;

        /* Ciclo infinito */
	while (true) {
		KeyWords CurrKeyWord = KeyWords(HP.GetDescription());

		switch (CurrKeyWord) {
		case INITIALTIME:
			dInitialTime = HP.GetReal();
			DEBUGLCOUT(MYDEBUG_INPUT, "Initial time is "
				<< dInitialTime << std::endl);
			break;

		case FINALTIME:
			if (HP.IsKeyWord("forever")) {
				dFinalTime = std::numeric_limits<doublereal>::max();
			} else {
				dFinalTime = HP.GetReal();
			}
			DEBUGLCOUT(MYDEBUG_INPUT, "Final time is "
				<< dFinalTime << std::endl);

			if(dFinalTime <= dInitialTime) {
				silent_cerr("warning: final time " << dFinalTime
					<< " is less than initial time "
					<< dInitialTime << ';' << std::endl
					<< "this will cause the simulation"
					" to abort" << std::endl);
			}
			break;

		case TIMESTEP:
			dInitialTimeStep = HP.GetReal();
			DEBUGLCOUT(MYDEBUG_INPUT, "Initial time step is "
				<< dInitialTimeStep << std::endl);

			if (dInitialTimeStep == 0.) {
				silent_cerr("warning, null initial time step"
					" is not allowed" << std::endl);
			} else if (dInitialTimeStep < 0.) {
				dInitialTimeStep = -dInitialTimeStep;
				silent_cerr("warning, negative initial time step"
					" is not allowed;" << std::endl
					<< "its modulus " << dInitialTimeStep
					<< " will be considered" << std::endl);
			}
			break;

		case MINTIMESTEP:
			try {
				dMinTimeStep = HP.GetReal(std::numeric_limits<doublereal>::min(), HighParser::range_gt<doublereal>(0.));

			} catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
				silent_cerr("invalid min time step " << e.Get() << " (must be positive) [" << e.what() << "] at line " << HP.GetLineData() << std::endl);
				throw e;
			}
			break;

		case MAXTIMESTEP:
			if (HP.IsKeyWord("unlimited")) {
				DriveCaller *pDC = 0;
				SAFENEWWITHCONSTRUCTOR(pDC, ConstDriveCaller, ConstDriveCaller(0.));
				MaxTimeStep.Set(pDC);

			} else {
				MaxTimeStep.Set(HP.GetDriveCaller());
			}

#ifdef DEBUG
			if (typeid(*MaxTimeStep.pGetDriveCaller()) == typeid(PostponedDriveCaller)) {
				DEBUGLCOUT(MYDEBUG_INPUT, "Max time step is postponed" << std::endl);

			} else {
				DEBUGLCOUT(MYDEBUG_INPUT, "Max time step is " << MaxTimeStep.dGet() << std::endl);
			}
#endif // DEBUG

			eTimeStepLimit = (HP.IsKeyWord("hard" "limit")
								&& HP.GetYesNoOrBool())
							? TS_HARD_LIMIT
							: TS_SOFT_LIMIT;

			if (dGetInitialMaxTimeStep() == 0.) {
				silent_cout("no max time step limit will be"
					" considered" << std::endl);

			} else if (dGetInitialMaxTimeStep() < 0.) {
				silent_cerr("negative max time step"
					" is not allowed" << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			break;

		case FICTITIOUSSTEPSNUMBER:
		case DUMMYSTEPSNUMBER:
			iDummyStepsNumber = HP.GetInt();
			if (iDummyStepsNumber < 0) {
				iDummyStepsNumber =
					::iDefaultDummyStepsNumber;
				silent_cerr("negative dummy steps number"
					" is illegal" << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);

			} else if (iDummyStepsNumber == 1) {
				silent_cerr("warning, a single dummy step"
					" may be useless" << std::endl);
			}

			DEBUGLCOUT(MYDEBUG_INPUT, "Dummy steps number: "
				<< iDummyStepsNumber << std::endl);
			break;

		case FICTITIOUSSTEPSRATIO:
		case DUMMYSTEPSRATIO:
			dDummyStepsRatio = HP.GetReal();
			if (dDummyStepsRatio < 0.) {
				silent_cerr("negative dummy steps ratio"
					" is illegal" << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}

			if (dDummyStepsRatio > 1.) {
				silent_cerr("warning, dummy steps ratio"
					" is larger than one." << std::endl
					<< "Something like 1.e-3 should"
					" be safer ..." << std::endl);
			}

			DEBUGLCOUT(MYDEBUG_INPUT, "Dummy steps ratio: "
				<< dDummyStepsRatio << std::endl);
			break;

		case FICTITIOUSSTEPSTOLERANCE:
		case DUMMYSTEPSTOLERANCE:
			dDummyStepsTolerance = HP.GetReal();
			if (dDummyStepsTolerance <= 0.) {
				silent_cerr("negative dummy steps"
					" tolerance is illegal" << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			DEBUGLCOUT(MYDEBUG_INPUT,
				"Dummy steps tolerance: "
				<< dDummyStepsTolerance << std::endl);
			break;

		case ABORTAFTER: {
			KeyWords WhenToAbort(KeyWords(HP.GetWord()));
			switch (WhenToAbort) {
			case INPUT:
				eAbortAfter = AFTER_INPUT;
				DEBUGLCOUT(MYDEBUG_INPUT,
					"Simulation will abort after"
					" data input" << std::endl);
				break;

			case ASSEMBLY:
				eAbortAfter = AFTER_ASSEMBLY;
				DEBUGLCOUT(MYDEBUG_INPUT,
					"Simulation will abort after"
					" initial assembly" << std::endl);
				break;

			case DERIVATIVES:
				eAbortAfter = AFTER_DERIVATIVES;
				DEBUGLCOUT(MYDEBUG_INPUT,
					"Simulation will abort after"
					" derivatives solution" << std::endl);
				break;

			case FICTITIOUSSTEPS:
			case DUMMYSTEPS:
				eAbortAfter = AFTER_DUMMY_STEPS;
				DEBUGLCOUT(MYDEBUG_INPUT,
					"Simulation will abort after"
					" dummy steps solution" << std::endl);
				break;
                        case REGULARSTEP: {
                                const long lAbortAfterStep = HP.GetInt();
                                if (lAbortAfterStep < 0) {
                                        silent_cerr("Step must be greater than or equal to zero at line " << HP.GetLineData() << "\n");
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }
                                eAbortAfter = static_cast<AbortAfter>(lAbortAfterStep + AFTER_REGULAR_STEP_0);
				DEBUGLCOUT(MYDEBUG_INPUT,
					"Simulation will abort after"
                                           " step " << lAbortAfterStep << "\n");
                        } break;
			default:
				silent_cerr("Don't know when to abort,"
					" so I'm going to abort now" << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
		} break;

		case OUTPUT: {
			unsigned OF = OUTPUT_DEFAULT;
			bool setOutput = false;

			while (HP.IsArg()) {
				KeyWords OutputFlag(KeyWords(HP.GetWord()));
				switch (OutputFlag) {
				case NONE:
					OF = OUTPUT_NONE;
					setOutput = true;
					break;

				case ITERATIONS:
					OF |= OUTPUT_ITERS;
					break;

				case RESIDUAL:
					OF |= OUTPUT_RES;
					break;

				case SOLUTION:
					OF |= OUTPUT_SOL;
					break;

				case JACOBIAN:
				case JACOBIANMATRIX:
					OF |= OUTPUT_JAC;
					break;

				case BAILOUT:
					OF |= OUTPUT_BAILOUT;
					break;

				case MESSAGES:
					OF |= OUTPUT_MSG;
					break;

				case COUNTER:
					OF |= OUTPUT_COUNTER;
					break;

				case MATRIX_COND_NUM:
					DelOutputFlags(OUTPUT_MAT_COND_NUM);
					if (HP.IsKeyWord("norm")) {
						if (HP.IsKeyWord("inf")) {
							OF |= OUTPUT_MAT_COND_NUM_INF;
						} else {
							const double P = HP.GetReal();
							if (P != 1.) {
								silent_cerr("Only inf or 1 norm are supported for condition numbers at line " << HP.GetLineData() << std::endl);
								throw ErrGeneric(MBDYN_EXCEPT_ARGS);
							}
							OF |= OUTPUT_MAT_COND_NUM_1;
						}
					} else {
						OF |= OUTPUT_MAT_COND_NUM_1;
					}
					break;

				case SOLVER_COND_NUM:
					OF |= OUTPUT_SOLVER_COND_NUM;
					if (HP.IsKeyWord("stat")) {
						if (HP.GetYesNoOrBool()) {
							OF |= OUTPUT_SOLVER_COND_STAT;
						} else {
							DelOutputFlags(OUTPUT_SOLVER_COND_STAT);
						}
					}
					break;
				case CPU_TIME:
					OF |= OUTPUT_CPU_TIME;
					break;

				default:
					silent_cerr("Warning, unknown output flag "
						"at line " << HP.GetLineData()
						<< "; ignored" << std::endl);
					break;
				}
			}

			if (setOutput) {
				SetOutputFlags(OF);

			} else {
				AddOutputFlags(OF);
			}
		} break;

		case OUTPUTMETER:
			SetOutputMeter(HP.GetDriveCaller(true));
			break;

		case METHOD: {
			if (bMethod) {
				silent_cerr("error: multiple definition"
					" of integration method at line "
					<< HP.GetLineData() << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			bMethod = true;

			KeyWords KMethod = KeyWords(HP.GetWord());
			switch (KMethod) {
			case CRANKNICHOLSON:
				silent_cout("warning: \"crank nicolson\" is the correct spelling "
					"at line " << HP.GetLineData() << std::endl);
			case CRANKNICOLSON:
				RegularType = INT_CRANKNICOLSON;
				break;

			case DIRK33:
				RegularType = INT_DIRK33;
				break;

			case DIRK43:
				RegularType = INT_DIRK43;
				break;

			case DIRK54:
				RegularType = INT_DIRK54;
				break;

			case BDF:
				/* default (order 2) */
				RegularType = INT_MS2;

				if (HP.IsKeyWord("order")) {
					int iOrder = HP.GetInt();

					switch (iOrder) {
					case 1:
						RegularType = INT_IMPLICITEULER;
						break;

					case 2:
						break;

					default:
						silent_cerr("unhandled BDF order " << iOrder << std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}
				}

				if (RegularType == INT_MS2) {
					SAFENEW(pRhoRegular, NullDriveCaller);
					SAFENEW(pRhoAlgebraicRegular, NullDriveCaller);
				}
				break;

			case NOSTRO:
				silent_cerr("integration method \"nostro\" "
					"is deprecated; use \"ms\" "
					"instead at line "
					<< HP.GetLineData()
					<< std::endl);
			case MS:
			case MS2:
			case MS3:
			case MS4:
			case SS2:
			case SS3:
			case SS4:
			case BATHE:
			case MSSTC3:
			case MSSTH3:
			case MSSTC4:
			case MSSTH4:
			case MSSTC5:
			case MSSTH5:
			case HOPE:
				pRhoRegular = HP.GetDriveCaller(true);

				pRhoAlgebraicRegular = 0;
				if (HP.IsArg()) {
					pRhoAlgebraicRegular = HP.GetDriveCaller(true);
				} else {
					pRhoAlgebraicRegular = pRhoRegular->pCopy();
				}

				switch (KMethod) {
				case NOSTRO:
				case MS:
				case MS2:
					RegularType = INT_MS2;
					break;

				case MS3:
					RegularType = INT_MS3;
					pSecondRhoRegular=pRhoRegular->pCopy();
					pSecondRhoAlgebraicRegular=pRhoRegular->pCopy();
					break;

				case MS4:
					RegularType = INT_MS4;
					pSecondRhoRegular=pRhoRegular->pCopy();
					pSecondRhoAlgebraicRegular=pRhoRegular->pCopy();
					pThirdRhoRegular=pRhoRegular->pCopy();
					pThirdRhoAlgebraicRegular=pRhoRegular->pCopy();
					break;

				case SS2:
					RegularType = INT_SS2;
					break;

				case SS3:
					RegularType = INT_SS3;
					break;

				case SS4:
					RegularType = INT_SS4;
					break;

				case BATHE:
					RegularType = INT_BATHE;
					break;

				case MSSTC3:
					RegularType = INT_MSSTC3;
					break;

				case MSSTH3:
					RegularType = INT_MSSTH3;
					break;

				case MSSTC4:
					RegularType = INT_MSSTC4;
					break;

				case MSSTH4:
					RegularType = INT_MSSTH4;
					break;

				case MSSTC5:
					RegularType = INT_MSSTC5;
					break;

				case MSSTH5:
					RegularType = INT_MSSTH5;
					break;

				case HOPE:
					RegularType = INT_HOPE;
					break;

				default:
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
				}
				break;

			case IMPLICITEULER:
				RegularType = INT_IMPLICITEULER;
				break;
			case HYBRID: {
				const KeyWords KMethod = KeyWords(HP.GetWord());

				switch (KMethod) {
				case IMPLICITEULER:
					eHybridDefaultIntRegular = INT_IMPLICITEULER;
					break;

				case CRANKNICOLSON:
					eHybridDefaultIntRegular = INT_CRANKNICOLSON;
					break;

				case MS:
				case MS2:
					eHybridDefaultIntRegular = INT_MS2;
					break;

				case HOPE:
					eHybridDefaultIntRegular = INT_HOPE;
					break;

				default:
					silent_cerr("Default method \"" << K.pGetDescription(KMethod)
						    << "\" not implemented for hybrid integrator at line "
						    << HP.GetLineData() << std::endl);
					throw ErrNotImplementedYet(MBDYN_EXCEPT_ARGS);
				}

				if (HP.IsArg()) {
					pRhoRegular = HP.GetDriveCaller(true);
				} else {
					SAFENEW(pRhoRegular, NullDriveCaller);
				}

				if (HP.IsArg()) {
					pRhoAlgebraicRegular = HP.GetDriveCaller(true);
				} else {
					pRhoAlgebraicRegular = pRhoRegular->pCopy();
				}

				RegularType = INT_HYBRID;
			} break;

			default:
				silent_cerr("Unknown integration method at line "
					<< HP.GetLineData() << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			break;
		}

		case FICTITIOUSSTEPSMETHOD:
		case DUMMYSTEPSMETHOD: {
			if (bDummyStepsMethod) {
				silent_cerr("error: multiple definition "
					"of dummy steps integration method "
					"at line " << HP.GetLineData()
					<< std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			bDummyStepsMethod = true;

			KeyWords KMethod = KeyWords(HP.GetWord());
			switch (KMethod) {
			case CRANKNICHOLSON:
				silent_cout("warning: \"crank nicolson\" is the correct spelling" << std::endl);
			case CRANKNICOLSON:
				DummyType = INT_CRANKNICOLSON;
				break;

			case DIRK33:
				DummyType = INT_DIRK33;
				break;

			case DIRK43:
				DummyType = INT_DIRK43;
				break;

			case DIRK54:
				DummyType = INT_DIRK54;
				break;

			case BDF:
				/* default (order 2) */
				DummyType = INT_MS2;

				if (HP.IsKeyWord("order")) {
					int iOrder = HP.GetInt();

					switch (iOrder) {
					case 1:
						DummyType = INT_IMPLICITEULER;
						break;

					case 2:
						break;

					default:
						silent_cerr("unhandled BDF order " << iOrder << std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}
				}

				if (DummyType == INT_MS2) {
					SAFENEW(pRhoDummy, NullDriveCaller);
					SAFENEW(pRhoAlgebraicDummy, NullDriveCaller);
				}
				break;

			case NOSTRO:
			case MS:
			case MS2:
			case MS3:
			case MS4:
			case SS2:
			case SS3:
			case SS4:
			case BATHE:
			case MSSTC3:
			case MSSTH3:
			case MSSTC4:
			case MSSTH4:
			case MSSTC5:
			case MSSTH5:
			case HOPE:
				pRhoDummy = HP.GetDriveCaller(true);

				if (HP.IsArg()) {
					pRhoAlgebraicDummy = HP.GetDriveCaller(true);
				} else {
					pRhoAlgebraicDummy = pRhoDummy->pCopy();
				}

				switch (KMethod) {
				case NOSTRO:
				case MS:
				case MS2:
					DummyType = INT_MS2;
					break;

				case MS3:
					DummyType = INT_MS3;
					pSecondRhoDummy = pRhoDummy->pCopy();
					pSecondRhoAlgebraicDummy = pRhoAlgebraicDummy->pCopy();
					break;

				case MS4:
					DummyType = INT_MS4;
					pSecondRhoDummy = pRhoDummy->pCopy();
					pSecondRhoAlgebraicDummy = pRhoAlgebraicDummy->pCopy();
					pThirdRhoDummy = pRhoDummy->pCopy();
					pThirdRhoAlgebraicDummy = pRhoAlgebraicDummy->pCopy();
					break;

				case SS2:
					DummyType = INT_SS2;
					break;

				case SS3:
					DummyType = INT_SS3;
					break;

				case SS4:
					DummyType = INT_SS4;
					break;

				case BATHE:
					DummyType = INT_BATHE;
					break;

				case MSSTC3:
					DummyType = INT_MSSTC3;
					break;

				case MSSTH3:
					DummyType = INT_MSSTH3;
					break;

				case MSSTC4:
					DummyType = INT_MSSTC4;
					break;

				case MSSTH4:
					DummyType = INT_MSSTH4;
					break;

				case MSSTC5:
					DummyType = INT_MSSTC5;
					break;

				case MSSTH5:
					DummyType = INT_MSSTH5;
					break;

				case HOPE:
					DummyType = INT_HOPE;
					break;

				default:
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
				}
				break;

			case IMPLICITEULER:
				DummyType = INT_IMPLICITEULER;
				break;

			default:
				silent_cerr("Unknown integration method at line " << HP.GetLineData() << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			break;
		}

		case TOLERANCE: {
			/*
			 * residual tolerance; can be the keyword "null",
			 * which means that the convergence test
			 * will be computed on the solution, or a number
			 */
			if (HP.IsKeyWord("null")) {
				dTol = 0.;

			} else {
				dTol = HP.GetReal();
				if (dTol < 0.) {
					dTol = ::dDefaultTol;
					silent_cerr("warning, residual tolerance "
						"< 0. is illegal; "
						"using default value " << dTol
						<< std::endl);
				}
			}

			/* safe default */
			if (dTol == 0.) {
				ResTest = NonlinearSolverTest::NONE;
			}

			if (HP.IsArg()) {
				if (HP.IsKeyWord("test")) {
					if (HP.IsKeyWord("norm")) {
						ResTest = NonlinearSolverTest::NORM;
					} else if (HP.IsKeyWord("minmax")) {
						ResTest = NonlinearSolverTest::MINMAX;
					} else if (HP.IsKeyWord("relnorm")) {
						ResTest = NonlinearSolverTest::RELNORM;
					} else if (HP.IsKeyWord("sepnorm")) {
						ResTest = NonlinearSolverTest::SEPNORM;
					} else if (HP.IsKeyWord("none")) {
						ResTest = NonlinearSolverTest::NONE;
					} else {
						silent_cerr("unknown test "
							"method at line "
							<< HP.GetLineData()
							<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}

					if (HP.IsKeyWord("scale")) {
						if (ResTest == NonlinearSolverTest::NONE) {
							silent_cerr("it's a nonsense "
								"to scale a disabled test; "
								"\"scale\" ignored"
								<< std::endl);
							bScale = false;
						} else {
							bScale = true;
						}
					}
				}
			}

			if (HP.IsArg()) {
				if (!HP.IsKeyWord("null")) {
					dSolutionTol = HP.GetReal();
				}

				/* safe default */
				if (dSolutionTol != 0.) {
					SolTest = NonlinearSolverTest::NORM;
				}

				if (HP.IsArg()) {
					if (HP.IsKeyWord("test")) {
						if (HP.IsKeyWord("norm")) {
							SolTest = NonlinearSolverTest::NORM;
						} else if (HP.IsKeyWord("minmax")) {
							SolTest = NonlinearSolverTest::MINMAX;
						} else if (HP.IsKeyWord("none")) {
							SolTest = NonlinearSolverTest::NONE;
						} else {
							silent_cerr("unknown test "
								"method at line "
								<< HP.GetLineData()
								<< std::endl);
							throw ErrGeneric(MBDYN_EXCEPT_ARGS);
						}
					}
				}

			} else if (dTol == 0.) {
				silent_cerr("need solution tolerance "
					"with null residual tolerance"
					<< std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}

			if (dSolutionTol < 0.) {
				dSolutionTol = 0.;
				silent_cerr("warning, solution tolerance "
					"< 0. is illegal; "
					"solution test is disabled"
					<< std::endl);
			}

			if (dTol == 0. && dSolutionTol == 0.) {
				silent_cerr("both residual and solution "
					"tolerances are zero" << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}

			DEBUGLCOUT(MYDEBUG_INPUT, "tolerance = " << dTol
					<< ", " << dSolutionTol << std::endl);
			break;
		}

		case MAXRESIDUAL: {
			if (HP.IsKeyWord("differential" "equations")) {
				dMaxResidualDiff = HP.GetReal();

				if (dMaxResidualDiff <= 0.) {
					silent_cerr("error: max residual for differential equations "
								"must be greater than zero at line "
							<< HP.GetLineData() << std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
				}
			}

			if (HP.IsKeyWord("all" "equations") || HP.IsArg()) {
				dMaxResidual = HP.GetReal();

				if (dMaxResidual <= 0.) {
					silent_cerr("error: max residual must be greater than zero at line "
							<< HP.GetLineData() << std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
				}
			}
			break;
		}

		case DERIVATIVESTOLERANCE: {
			dDerivativesTol = HP.GetReal();
			if (dDerivativesTol <= 0.) {
				dDerivativesTol = ::dDefaultTol;
				silent_cerr("warning, derivatives "
					"tolerance <= 0.0 is illegal; "
					"using default value "
					<< dDerivativesTol
					<< std::endl);
			}
			DEBUGLCOUT(MYDEBUG_INPUT,
					"Derivatives tolerance = "
					<< dDerivativesTol
					<< std::endl);
                        
                        if (HP.IsArg()) {
                                dDerivativesSolTol = HP.GetReal();
                        }
                        DEBUGLCOUT(MYDEBUG_INPUT, "Derivatives solution tolerance = "
                                   << dDerivativesSolTol << "\n");
			break;
		}

		case MAXITERATIONS: {
			iMaxIterations = HP.GetInt();
			if (iMaxIterations < 1) {
				iMaxIterations = ::iDefaultMaxIterations;
				silent_cerr("warning, max iterations "
					"< 1 is illegal; using default value "
					<< iMaxIterations
					<< std::endl);
			}
			DEBUGLCOUT(MYDEBUG_INPUT,
					"Max iterations = "
					<< iMaxIterations << std::endl);
			if (HP.IsKeyWord("at" "most")) {
				iMaxIterations = -iMaxIterations;
			}
			break;
		}

		case MODIFY_RES_TEST:
			if (bParallel) {
				silent_cerr("\"modify residual test\" "
					"not supported by schur data manager "
					"at line " << HP.GetLineData()
					<< "; ignored" << std::endl);
			} else {
				bModResTest = true;
				DEBUGLCOUT(MYDEBUG_INPUT,
					"Modify residual test" << std::endl);
			}
			break;

		case ENFORCE_CONSTRAINT_EQUATIONS:
			if (HP.IsKeyWord("constraint" "violations")) {
				eScaleFlags = SCALE_ALGEBRAIC_EQUATIONS_YES;

				bSetScaleAlgebraic = !HP.IsKeyWord("scale" "factor");

				if (!bSetScaleAlgebraic) {
					dScaleAlgebraic = HP.GetReal();
				}
			} else if (HP.IsKeyWord("constraint" "violation" "rates")) {
				eScaleFlags = SCALE_ALGEBRAIC_EQUATIONS_NO;
			} else {
				silent_cerr("Keyword \"constraint violations\" or "
						    "\"constraint violation rates\" expected at line "
							<< HP.GetLineData() << std::endl);

				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			break;

		case DERIVATIVESMAXITERATIONS: {
			iDerivativesMaxIterations = HP.GetInt();
			if (iDerivativesMaxIterations < 1) {
				iDerivativesMaxIterations = ::iDefaultMaxIterations;
				silent_cerr("warning, derivatives "
					"max iterations < 1 is illegal; "
					"using default value "
					<< iDerivativesMaxIterations
					<< std::endl);
			}
			DEBUGLCOUT(MYDEBUG_INPUT, "Derivatives "
					"max iterations = "
					<< iDerivativesMaxIterations
					<< std::endl);
			break;
		}

		case FICTITIOUSSTEPSMAXITERATIONS:
		case DUMMYSTEPSMAXITERATIONS: {
			iDummyStepsMaxIterations = HP.GetInt();
			if (iDummyStepsMaxIterations < 1) {
				iDummyStepsMaxIterations = ::iDefaultMaxIterations;
				silent_cerr("warning, dummy steps "
					"max iterations < 1 is illegal; "
					"using default value "
					<< iDummyStepsMaxIterations
					<< std::endl);
			}
			DEBUGLCOUT(MYDEBUG_INPUT, "Dummy steps "
					"max iterations = "
					<< iDummyStepsMaxIterations
					<< std::endl);
			break;
		}

		case DERIVATIVESCOEFFICIENT: {
			const bool bAutoDerivCoef = HP.IsKeyWord("auto");

			if (!bAutoDerivCoef) {
				dDerivativesCoef = HP.GetReal();
				if (dDerivativesCoef <= 0.) {
					dDerivativesCoef = ::dDefaultDerivativesCoefficient;
					silent_cerr("warning, derivatives "
						"coefficient <= 0. is illegal; "
						"using default value "
						<< dDerivativesCoef
						<< std::endl);
				}
				DEBUGLCOUT(MYDEBUG_INPUT, "Derivatives coefficient = "
					<< dDerivativesCoef << std::endl);
			}

			if (bAutoDerivCoef || HP.IsKeyWord("auto")) {
				if (HP.IsKeyWord("max" "iterations")) {
					iDerivativesCoefMaxIter = HP.GetInt();
                                } else {
                                        iDerivativesCoefMaxIter = 6;
                                }
                            
					if (iDerivativesCoefMaxIter < 0) {
						silent_cerr("number of iterations must be greater than or equal to zero at line " << HP.GetLineData() << std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}

				if (HP.IsKeyWord("factor")) {
					dDerivativesCoefFactor = HP.GetReal();

					if (dDerivativesCoefFactor <= 1.) {
						silent_cerr("factor must be greater than one at line " << HP.GetLineData() << std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}
				}
			}

			break;
		}

		case NEWTONRAPHSON: {
			pedantic_cout("Newton Raphson is deprecated; use "
					"\"nonlinear solver: newton raphson "
					"[ , modified, <steps> ]\" instead"
					<< std::endl);
			KeyWords NewRaph(KeyWords(HP.GetWord()));
			switch(NewRaph) {
			case MODIFIED:
				bTrueNewtonRaphson = 0;
				if (HP.IsArg()) {
                                        oLineSearchParam.iIterationsBeforeAssembly = HP.GetInt();
				} else {
                                        oLineSearchParam.iIterationsBeforeAssembly = ::iDefaultIterationsBeforeAssembly;
				}
				DEBUGLCOUT(MYDEBUG_INPUT, "Modified "
						"Newton-Raphson will be used; "
						"matrix will be assembled "
						"at most after "
                                                << oLineSearchParam.iIterationsBeforeAssembly
						<< " iterations" << std::endl);
				break;

			default:
				silent_cerr("warning: unknown case; "
					"using default" << std::endl);

			/* no break: fall-thru to next case */
			case NR_TRUE:
				bTrueNewtonRaphson = 1;
                                oLineSearchParam.iIterationsBeforeAssembly = 0;
				break;
			}
			break;
		}

		case END:
			switch (KeyWords(HP.GetWord())) {
			case MULTISTEP:
				pedantic_cout("\"end: multistep;\" is deprecated; "
					"use \"end: initial value;\" instead." << std::endl);
			case INITIAL_VALUE:
				break;

			default:
				silent_cerr("\"end: initial value;\" expected "
					"at line " << HP.GetLineData()
					<< "; aborting..." << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			goto EndOfCycle;

		case STRATEGY:
			pTSC = ReadTimeStepData(this, HP);
			break;

		case POD:
			silent_cerr("line " << HP.GetLineData()
				<< ": POD analysis not supported (ignored)"
				<< std::endl);
			for (; HP.IsArg();) {
				(void)HP.GetReal();
			}
			break;

		case EIGENANALYSIS: {
                        // initialize output precision: 0 means use default precision
                        EigAn.iMatrixPrecision = 0;
                        EigAn.iResultsPrecision = 0;

                        // read eigenanalysis time (to be changed)

                        const integer iNumTimes = HP.IsKeyWord("list") ? HP.GetInt() : 1;

                        if (iNumTimes <= 0) {
                             silent_cerr("invalid number of eigenanalysis times "
                                         "at line " << HP.GetLineData() << "\n");
                             throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                        EigAn.Analyses.clear();
                        EigAn.Analyses.reserve(iNumTimes);

                        for (integer i = 0; i < iNumTimes; ++i)
                        {
                             const doublereal dTimeEig = HP.GetReal();

                             if (i > 0 && dTimeEig <= EigAn.Analyses.back()) {
                                  silent_cerr("eigenanalysis times must be in strict ascending order "
                                              "at line " << HP.GetLineData() << "\n");
                                  throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                             }

                             EigAn.Analyses.push_back(dTimeEig); // If dTimeEig is after the end of the simulation it will be executed as well.
                        }

                        ASSERT(EigAn.Analyses.size() > 0);
                        // initialize EigAn
                        EigAn.currAnalysis = EigAn.Analyses.begin();
                        EigAn.currAnalysisIndex = 0u;
                        EigAn.bAnalysis = true;

                        // permute is the default; use "balance, no" to disable
                        EigAn.uFlags = EigenAnalysis::EIG_PERMUTE;

                        while (HP.IsArg()) {
				if (HP.IsKeyWord("parameter")) {
					EigAn.dParam = HP.GetReal();
                                        bEigAnUserDefinedParam = true;
                                } else if (HP.IsKeyWord("mode")) {
                                        bEigAnUserDefinedWhich = true;
                                     
                                        if (HP.IsKeyWord("largest" "magnitude")) {
                                                EigAn.eWhichEigVal = EigenAnalysis::LM;
                                        } else if (HP.IsKeyWord("smallest" "magnitude")) {
                                                EigAn.eWhichEigVal = EigenAnalysis::SM;
                                        } else if (HP.IsKeyWord("largest" "real" "part")) {
                                                EigAn.eWhichEigVal = EigenAnalysis::LR;
                                        } else if (HP.IsKeyWord("smallest" "real" "part")) {
                                                EigAn.eWhichEigVal = EigenAnalysis::SR;
                                        } else if (HP.IsKeyWord("largest" "imaginary" "part")) {
                                                EigAn.eWhichEigVal = EigenAnalysis::LI;
                                        } else if (HP.IsKeyWord("smallest" "imaginary" "part")) {
                                                EigAn.eWhichEigVal = EigenAnalysis::SI;
                                        } else {
                                                silent_cerr("keywords \"largest magnitude\", \"smallest magnitude\", "
                                                            "\"largest real part\", \"smallest real part\", "
                                                            "\"largest imaginary part\" or \"smallest imaginary part\""
                                                            " expected at line " << HP.GetLineData() << "\n");
                                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                        }
                                } else if (HP.IsKeyWord("output" "matrices")) {
					EigAn.uFlags |= EigenAnalysis::EIG_OUTPUT_MATRICES;
				} else if (HP.IsKeyWord("output" "full" "matrices")) {
#ifndef USE_EIG
					silent_cerr("\"output full matrices\" needs eigenanalysis support" << std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
#endif // !USE_EIG
					EigAn.uFlags |= EigenAnalysis::EIG_OUTPUT_FULL_MATRICES;

				} else if (HP.IsKeyWord("output" "sparse" "matrices")) {
					EigAn.uFlags |= EigenAnalysis::EIG_OUTPUT_SPARSE_MATRICES;

				} else if (HP.IsKeyWord("output" "eigenvectors")) {
#ifndef USE_EIG
					silent_cerr("\"output eigenvectors\" needs eigenanalysis support" << std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
#endif // !USE_EIG
					EigAn.uFlags |= EigenAnalysis::EIG_OUTPUT_EIGENVECTORS;

				} else if (HP.IsKeyWord("output" "geometry")) {
					EigAn.uFlags |= EigenAnalysis::EIG_OUTPUT_GEOMETRY;

				} else if (HP.IsKeyWord("matrix" "output" "precision")) {
					EigAn.iMatrixPrecision = HP.GetInt();
					if (EigAn.iMatrixPrecision <= 0) {
						silent_cerr("negative or null \"matrix output precision\" "
							"parameter at line " << HP.GetLineData()
							<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}

				} else if (HP.IsKeyWord("results" "output" "precision")) {
					EigAn.iResultsPrecision = HP.GetInt();
					if (EigAn.iResultsPrecision <= 0) {
						silent_cerr("negative or null \"results output precision\" "
							"parameter at line " << HP.GetLineData()
							<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}

				} else if (HP.IsKeyWord("upper" "frequency" "limit")) {
					EigAn.dUpperFreq = HP.GetReal();
					if (EigAn.dUpperFreq < 0.) {
						silent_cerr("invalid \"upper frequency limit\" "
							"at line " << HP.GetLineData()
							<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}
                                        bEigAnUserDefinedUpperFreq = true;
				} else if (HP.IsKeyWord("lower" "frequency" "limit")) {
					EigAn.dLowerFreq = HP.GetReal();
					if (EigAn.dLowerFreq < 0.) {
						silent_cerr("invalid \"lower frequency limit\" "
							"at line " << HP.GetLineData()
							<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}
                                        bEigAnUserDefinedLowerFreq = true;
				} else if (HP.IsKeyWord("use" "lapack")) {
					if (EigAn.uFlags & EigenAnalysis::EIG_USE_MASK) {
						silent_cerr("eigenanalysis routine already selected "
							"at line " << HP.GetLineData()
							<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}
#ifdef USE_LAPACK
					EigAn.uFlags |= EigenAnalysis::EIG_USE_LAPACK;
#else // !USE_LAPACK
					silent_cerr("\"use lapack\" "
						"needs to configure --with-lapack "
						"at line " << HP.GetLineData()
						<< std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
#endif // !USE_LAPACK

				} else if (HP.IsKeyWord("use" "arpack")) {
					if (EigAn.uFlags & EigenAnalysis::EIG_USE_MASK) {
						silent_cerr("eigenanalysis routine already selected "
							"at line " << HP.GetLineData()
							<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}
#ifdef USE_ARPACK
					EigAn.uFlags |= EigenAnalysis::EIG_USE_ARPACK;

					EigAn.arpack.iNEV = HP.GetInt();
					if (EigAn.arpack.iNEV <= 0) {
						silent_cerr("invalid number of eigenvalues "
							"at line " << HP.GetLineData()
							<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}

					EigAn.arpack.iNCV = HP.GetInt();
					if (EigAn.arpack.iNCV <= 0
						|| EigAn.arpack.iNCV <= EigAn.arpack.iNEV + 2)
					{
						silent_cerr("invalid number of Arnoldi vectors "
							"(must be > NEV+2) "
							"at line " << HP.GetLineData()
							<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}

					if (EigAn.arpack.iNCV <= 2*EigAn.arpack.iNEV) {
						silent_cerr("warning, possibly incorrect number of Arnoldi vectors "
							"(should be > 2*NEV) "
							"at line " << HP.GetLineData()
							<< std::endl);
					}

					EigAn.arpack.dTOL = HP.GetReal();
					if (EigAn.arpack.dTOL < 0.) {
						silent_cerr("tolerance must be non-negative "
							"at line " << HP.GetLineData()
							<< std::endl);
						EigAn.arpack.dTOL = 0.;
					}

                                        if (HP.IsKeyWord("max" "iterations")) {
                                             EigAn.arpack.iMaxIterations = HP.GetInt();

                                             if (EigAn.arpack.iMaxIterations < 1) {
                                                  silent_cerr("max iterations must be greater than zero at line " << HP.GetLineData() << "\n");
                                                  throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                             }
                                        }
#else // !USE_ARPACK
					silent_cerr("\"use arpack\" "
						"needs to configure --with-arpack "
						"at line " << HP.GetLineData()
						<< std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
#endif // !USE_ARPACK

				} else if (HP.IsKeyWord("use" "jdqz")) {
					if (EigAn.uFlags & EigenAnalysis::EIG_USE_MASK) {
						silent_cerr("eigenanalysis routine already selected "
							"at line " << HP.GetLineData()
							<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}
#ifdef USE_JDQZ
					EigAn.uFlags |= EigenAnalysis::EIG_USE_JDQZ;

					EigAn.jdqz.kmax = HP.GetInt();
					if (EigAn.jdqz.kmax <= 0) {
						silent_cerr("invalid number of eigenvalues "
							"at line " << HP.GetLineData()
							<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}

					EigAn.jdqz.jmax = HP.GetInt();
					if (EigAn.jdqz.jmax < 20
						|| EigAn.jdqz.jmax < 2*EigAn.jdqz.kmax)
					{
						silent_cerr("invalid size of the search space "
							"(must be >= 20 && >= 2*kmax) "
							"at line " << HP.GetLineData()
							<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}

					EigAn.jdqz.jmin = 2*EigAn.jdqz.kmax;

					EigAn.jdqz.eps = HP.GetReal();
					if (EigAn.jdqz.eps <= 0.) {
						silent_cerr("tolerance must be non-negative "
							"at line " << HP.GetLineData()
							<< std::endl);
						EigAn.jdqz.eps = std::numeric_limits<doublereal>::epsilon();
					}
#else // !USE_JDQZ
					silent_cerr("\"use jdqz\" "
						"needs to configure --with-jdqz "
						"at line " << HP.GetLineData()
						<< std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
#endif // !USE_JDQZ
                                } else if (HP.IsKeyWord("use" "external")) {
                                        EigAn.uFlags |= EigenAnalysis::EIG_USE_EXTERNAL;
				} else if (HP.IsKeyWord("balance")) {
					if (HP.IsKeyWord("no")) {
						EigAn.uFlags &= ~EigenAnalysis::EIG_BALANCE;

					} else if (HP.IsKeyWord("permute")) {
						EigAn.uFlags |= EigenAnalysis::EIG_PERMUTE;

					} else if (HP.IsKeyWord("scale")) {
						EigAn.uFlags |= EigenAnalysis::EIG_SCALE;

					} else if (HP.IsKeyWord("all")) {
						EigAn.uFlags |= EigenAnalysis::EIG_BALANCE;

					} else {
						silent_cerr("unknown balance option "
							"at line " << HP.GetLineData()
							<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}

				} else if (HP.IsKeyWord("suffix" "width")) {
					if (HP.IsKeyWord("compute")) {
						EigAn.iFNameWidth = EigenAnalysis::EIGAN_WIDTH_COMPUTE;

					} else {
						EigAn.iFNameWidth = HP.GetInt(0, HighParser::range_ge<integer>(0));
					}

				} else if (HP.IsKeyWord("suffix" "format")) {
					EigAn.iFNameFormat = HP.GetStringWithDelims();
					// check?

				} else {
					silent_cerr("unknown option "
						"at line " << HP.GetLineData()
						<< std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
				}
			}

                        if (!bEigAnUserDefinedParam && (bEigAnUserDefinedLowerFreq || bEigAnUserDefinedUpperFreq)) {
                             DEBUGCERR("Parameter for eigenanalysis has not been defined in the input file.\n");
                             DEBUGCERR("Try to estimate the optimum settings for ARPACK based on input file.\n");

                             if (!bEigAnUserDefinedWhich) {
                                  DEBUGCERR("It was not defined in the input file which eigenvalues to compute\n");
                                  if (EigAn.uFlags & EigenAnalysis::EIG_USE_ARPACK) {
                                       if (bEigAnUserDefinedLowerFreq && EigAn.dLowerFreq > 0.) {
                                            // This is the preferred option for ARPACK
                                            EigAn.eWhichEigVal = EigenAnalysis::LI;
                                       } else if (bEigAnUserDefinedUpperFreq && EigAn.dUpperFreq > 0.) {
                                            EigAn.eWhichEigVal = EigenAnalysis::SM;
                                       }
                                  } else {
                                       if (bEigAnUserDefinedUpperFreq && EigAn.dUpperFreq > 0.) {
                                            // This is the preferred option for LAPACK
                                            // JDQZ was not tested with this option so far
                                            EigAn.eWhichEigVal = EigenAnalysis::SM;
                                       } else if (bEigAnUserDefinedLowerFreq && EigAn.dLowerFreq > 0.) {
                                            EigAn.eWhichEigVal = EigenAnalysis::LI;
                                       }
                                  }
                             }

                             static constexpr doublereal dSafetyFactor = 5.;
                             static constexpr doublereal dEigAnMinParam = 1e-6;
                             static constexpr doublereal dEigAnMaxParam = 1. / dEigAnMinParam;

                             bool bEigAnParamSet = false;

                             switch (EigAn.eWhichEigVal) {
                             case EigenAnalysis::SM:
                                  if (bEigAnUserDefinedUpperFreq && EigAn.dUpperFreq > 0.) {
                                       EigAn.dParam = 1. / (2. * M_PI * EigAn.dUpperFreq * dSafetyFactor);
                                       bEigAnParamSet = true;
                                  }
                                  break;
                             case EigenAnalysis::LI:
                                  if (bEigAnUserDefinedLowerFreq && EigAn.dLowerFreq > 0.) {
                                       EigAn.dParam = dSafetyFactor / (2. * M_PI * EigAn.dLowerFreq);
                                       bEigAnParamSet = true;
                                  }
                                  break;
                             default:
                                  break;
                             }

                             if (bEigAnParamSet) {
                                  EigAn.dParam = std::min(dEigAnMaxParam, std::max(dEigAnMinParam, EigAn.dParam));
                                  silent_cerr("info: estimated parameter for eigenanalysis " << EigAn.dParam << "\n");
                             } else {
                                  silent_cerr("warning: estimation of parameter for eigenanalysis not available; using the default: " << EigAn.dParam << "\n");
                             }
                        }

                        // lower must be less than upper
			if (EigAn.dLowerFreq > EigAn.dUpperFreq) {
				silent_cerr("upper frequency limit " << EigAn.dUpperFreq
					<< " less than lower frequency limit " << EigAn.dLowerFreq
					<< std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}

			// if only upper is defined, make lower equal to -upper
			if (!bEigAnUserDefinedLowerFreq) {
				EigAn.dLowerFreq = -EigAn.dUpperFreq;
			}

			switch (EigAn.uFlags & EigenAnalysis::EIG_USE_MASK) {
			case EigenAnalysis::EIG_USE_LAPACK:
				if (EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_SPARSE_MATRICES) {
					silent_cerr("sparse matrices output "
						"incompatible with lapack "
						"at line " << HP.GetLineData()
						<< std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
				}
				if (EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_MATRICES) {
					EigAn.uFlags |= EigenAnalysis::EIG_OUTPUT_FULL_MATRICES;
				}
				break;

			case EigenAnalysis::EIG_USE_ARPACK:
				if (EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_FULL_MATRICES) {
					silent_cerr("full matrices output "
						"incompatible with arpack "
						"at line " << HP.GetLineData()
						<< std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
				}
				if (EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_MATRICES) {
					EigAn.uFlags |= EigenAnalysis::EIG_OUTPUT_SPARSE_MATRICES;
				}
				break;

			case EigenAnalysis::EIG_USE_JDQZ:
				if (EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_FULL_MATRICES) {
					silent_cerr("full matrices output "
						"incompatible with jdqz "
						"at line " << HP.GetLineData()
						<< std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
				}
				if (EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_MATRICES) {
					EigAn.uFlags |= EigenAnalysis::EIG_OUTPUT_SPARSE_MATRICES;
				}
				break;
                        case EigenAnalysis::EIG_USE_EXTERNAL:
                                if (!(EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_MATRICES_MASK)) {
                                        silent_cerr("external eigenanalysis is not possible without output of matrices" << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if ((EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_MATRICES) && !(EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_FULL_MATRICES)) {
                                        EigAn.uFlags |= EigenAnalysis::EIG_OUTPUT_SPARSE_MATRICES;
                                }
                                break;
			default:
				break;
			}

#ifdef USE_EIG
			// if an eigenanalysis routine is selected
			// or sparse matrix output is not requested,
			// force direct eigensolution
			if ((EigAn.uFlags & EigenAnalysis::EIG_USE_MASK)
				|| !(EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_SPARSE_MATRICES))
			{
				EigAn.uFlags |= EigenAnalysis::EIG_SOLVE;
			}

			// if no eigenanalysis routine is selected,
			// force the use of LAPACK's
			if ((EigAn.uFlags & EigenAnalysis::EIG_SOLVE)
				&& !(EigAn.uFlags & EigenAnalysis::EIG_USE_MASK))
			{
				EigAn.uFlags |= EigenAnalysis::EIG_USE_LAPACK;
				if (EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_MATRICES) {
					EigAn.uFlags |= EigenAnalysis::EIG_OUTPUT_FULL_MATRICES;
				}
			}
#else // !USE_EIG
			if (EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_MATRICES) {
				EigAn.uFlags |= EigenAnalysis::EIG_OUTPUT_SPARSE_MATRICES;
			}
			silent_cerr("warning: \"eigenanalysis\" not supported; ignored" << std::endl);
#endif // !USE_EIG
                } break;

		case SOLVER:
			silent_cerr("\"solver\" keyword at line "
					<< HP.GetLineData()
					<< " is deprecated; "
					"use \"linear solver\" instead"
					<< std::endl);
		case LINEARSOLVER:
			ReadLinSol(CurrLinearSolver, HP);
			break;

		case INTERFACESOLVER:
			silent_cerr("\"interface solver\" keyword at line "
					<< HP.GetLineData()
					<< " is deprecated; "
					"use \"interface linear solver\" "
					"instead" << std::endl);
		case INTERFACELINEARSOLVER:
			ReadLinSol(CurrIntSolver, HP, true);

#ifndef USE_MPI
			silent_cerr("Interface solver only allowed "
				"when compiled with MPI support" << std::endl);
#endif /* ! USE_MPI */
			break;

		case NONLINEARSOLVER:
			switch (KeyWords(HP.GetWord())) {
			case DEFAULT:
				NonlinearSolverType = NonlinearSolver::DEFAULT;
				break;

			case NEWTONRAPHSON:
				NonlinearSolverType = NonlinearSolver::NEWTONRAPHSON;
				break;

			case LINESEARCH:
				NonlinearSolverType = NonlinearSolver::LINESEARCH;
				break;

                        case BFGS:
                                NonlinearSolverType = NonlinearSolver::BFGS;
                                break;

                        case MCP_NEWTON_FB:
                                NonlinearSolverType = NonlinearSolver::MCP_NEWTON_FB;
                                break;

                        case MCP_NEWTON_MIN_FB:
                                NonlinearSolverType = NonlinearSolver::MCP_NEWTON_MIN_FB;
                                break;
                                
                        case NOX:
                                NonlinearSolverType = NonlinearSolver::NOX;
                                break;

                        case SICONOS_MCP_NEWTON_FB:
                                NonlinearSolverType = NonlinearSolver::SICONOS_MCP_NEWTON_FB;
                                break;
                                
                        case SICONOS_MCP_NEWTON_MIN_FB:
                                NonlinearSolverType = NonlinearSolver::SICONOS_MCP_NEWTON_MIN_FB;
                                break;

			case MATRIXFREE:
				NonlinearSolverType = NonlinearSolver::MATRIXFREE;
				break;

			default:
				silent_cerr("unknown nonlinear solver "
					"at line " << HP.GetLineData()
					<< std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}

			switch (NonlinearSolverType) {
                        case NonlinearSolver::SICONOS_MCP_NEWTON_FB:
                        case NonlinearSolver::SICONOS_MCP_NEWTON_MIN_FB:
#ifndef USE_SICONOS
                                silent_cerr("siconos solver is not available!\n"
                                            "configure with --with-siconos in order to use siconos!\n");
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
#endif
			case NonlinearSolver::NEWTONRAPHSON:
			case NonlinearSolver::LINESEARCH:
                        case NonlinearSolver::BFGS:
                        case NonlinearSolver::MCP_NEWTON_FB:
                        case NonlinearSolver::MCP_NEWTON_MIN_FB:
				bTrueNewtonRaphson = true;
                                oLineSearchParam.bKeepJacAcrossSteps = false;
                                oLineSearchParam.iIterationsBeforeAssembly = 0;

				if (NonlinearSolverType == NonlinearSolver::NEWTONRAPHSON && HP.IsKeyWord("true")) {
					break;
				}

				if (HP.IsKeyWord("modified")) {
					bTrueNewtonRaphson = false;
                                        oLineSearchParam.iIterationsBeforeAssembly = HP.GetInt();

					if (HP.IsKeyWord("keep" "jacobian")) {
						pedantic_cout("Use of deprecated \"keep jacobian\" "
							"at line " << HP.GetLineData() << std::endl);
                                                oLineSearchParam.bKeepJacAcrossSteps = true;

					} else if (HP.IsKeyWord("keep" "jacobian" "matrix")) {
                                                oLineSearchParam.bKeepJacAcrossSteps = true;
					}

					DEBUGLCOUT(MYDEBUG_INPUT, "modified "
							"Newton-Raphson "
							"will be used; "
							"matrix will be "
							"assembled at most "
							"after "
                                                        << oLineSearchParam.iIterationsBeforeAssembly
							<< " iterations"
							<< std::endl);
					if (HP.IsKeyWord("honor" "element" "requests")) {
						bHonorJacRequest = true;
						DEBUGLCOUT(MYDEBUG_INPUT,
								"honor elements' "
								"request to update "
								"the preconditioner"
								<< std::endl);
					}
				}

                                if (NonlinearSolverType == NonlinearSolver::BFGS) {
                                        oLineSearchParam.dAlphaModified = oLineSearchParam.dAlphaFull; // Lower number of jacobians achieved with this settings
                                }

					while (HP.IsArg()) {
                                                if (HP.IsKeyWord("default" "solver" "options")) {
                                                oLineSearchParam.uFlags &= ~LineSearchParameters::ABORT_AT_LAMBDA_MIN;
                                                oLineSearchParam.uFlags |= LineSearchParameters::NON_NEGATIVE_SLOPE_CONTINUE;
                                                oLineSearchParam.uFlags |= LineSearchParameters::ZERO_GRADIENT_CONTINUE;
                                                oLineSearchParam.uFlags |= LineSearchParameters::DIVERGENCE_CHECK;
                                                oLineSearchParam.uFlags &= ~LineSearchParameters::SCALE_NEWTON_STEP;
                                                
                                                        if (HP.IsKeyWord("moderate" "nonlinear")) {
                                                        oLineSearchParam.dLambdaMin = 0.1;
                                                        oLineSearchParam.dDivergenceCheck = 10;
                                                        } else if (HP.IsKeyWord("heavy" "nonlinear")) {
                                                        oLineSearchParam.dLambdaMin = 1e-4;
                                                        oLineSearchParam.dDivergenceCheck = 1.5;
                                                        } else {
                                                                silent_cerr("keyword \"moderate nonlinear\" or \"heavy nonlinear\" expected at line " << HP.GetLineData() << std::endl);
                                                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                        }
                                                } else if (HP.IsKeyWord("tolerance" "x")) {
                                                oLineSearchParam.dTolX = HP.GetReal();
                                                if (oLineSearchParam.dTolX < 0.) {
								silent_cerr("tolerance x must be greater than or equal to zero at line " << HP.GetLineData() << std::endl);
								throw ErrGeneric(MBDYN_EXCEPT_ARGS);
							}
						} else if (HP.IsKeyWord("tolerance" "min")) {
                                                oLineSearchParam.dTolMin = HP.GetReal();
                                                if (oLineSearchParam.dTolMin < 0.) {
								silent_cerr("tolerance min must be greater than or equal to zero at line " << HP.GetLineData() << std::endl);
								throw ErrGeneric(MBDYN_EXCEPT_ARGS);
							}
						} else if (HP.IsKeyWord("max" "iterations")) {
                                                oLineSearchParam.iMaxIterations = HP.GetInt();
                                                if (oLineSearchParam.iMaxIterations < 0) {
								silent_cerr("max iterations must be greater than or equal to zero at line " << HP.GetLineData() << std::endl);
								throw ErrGeneric(MBDYN_EXCEPT_ARGS);
							}
						} else if (HP.IsKeyWord("alpha") || HP.IsKeyWord("alpha" "full")) {
                                                oLineSearchParam.dAlphaFull = HP.GetReal();
                                                if (oLineSearchParam.dAlphaFull < 0.) {
								silent_cerr("alpha full must be greater than or equal to zero at line " << HP.GetLineData() << std::endl);
								throw ErrGeneric(MBDYN_EXCEPT_ARGS);
							}
                                                } else if (HP.IsKeyWord("alpha" "modified")) {
                                                oLineSearchParam.dAlphaModified = HP.GetReal();
                                                if (oLineSearchParam.dAlphaModified < 0.) {
								silent_cerr("alpha modified must be greater than or equal to zero at line " << HP.GetLineData() << std::endl);
								throw ErrGeneric(MBDYN_EXCEPT_ARGS);
							}
						} else if (HP.IsKeyWord("lambda" "min")) {
                                                oLineSearchParam.dLambdaMin = HP.GetReal();
                                                if (oLineSearchParam.dLambdaMin < 0.) {
								silent_cerr("lambda min must be greater than or equal to zero at line " << HP.GetLineData() << std::endl);
								throw ErrGeneric(MBDYN_EXCEPT_ARGS);
							}
							if (HP.IsKeyWord("relative")) {
								if (HP.GetYesNoOrBool()) {
                                                                oLineSearchParam.uFlags |= LineSearchParameters::RELATIVE_LAMBDA_MIN;
								} else {
                                                                oLineSearchParam.uFlags &= ~LineSearchParameters::RELATIVE_LAMBDA_MIN;
								}
							}
						} else if (HP.IsKeyWord("lambda" "factor" "min")) {
                                                oLineSearchParam.dLambdaFactMin = HP.GetReal();
                                                if (oLineSearchParam.dLambdaFactMin <= 0. || oLineSearchParam.dLambdaFactMin >= 1.) {
								silent_cerr("lambda factor min must be in between zero and one at line" << HP.GetLineData() << std::endl);
								throw ErrGeneric(MBDYN_EXCEPT_ARGS);
							}
						} else if (HP.IsKeyWord("max" "step")) {
                                                oLineSearchParam.dMaxStep = HP.GetReal();
                                                if (oLineSearchParam.dMaxStep <= 0.) {
								silent_cerr("max step must be greater than zero at line " << HP.GetLineData() << std::endl);
								throw ErrGeneric(MBDYN_EXCEPT_ARGS);
							}
						} else if (HP.IsKeyWord("zero" "gradient")) {
							if (HP.IsKeyWord("continue")) {
								if (HP.GetYesNoOrBool()) {
                                                                oLineSearchParam.uFlags |= LineSearchParameters::ZERO_GRADIENT_CONTINUE;
								} else {
                                                                oLineSearchParam.uFlags &= ~LineSearchParameters::ZERO_GRADIENT_CONTINUE;
								}
							}
							else {
								silent_cerr("Keyword \"continue\" expected at line " << HP.GetLineData() << std::endl);
								throw ErrGeneric(MBDYN_EXCEPT_ARGS);
							}
						} else if (HP.IsKeyWord("divergence" "check")) {
							if (HP.GetYesNoOrBool()) {
                                                        oLineSearchParam.uFlags |= LineSearchParameters::DIVERGENCE_CHECK;
							} else {
                                                        oLineSearchParam.uFlags &= ~LineSearchParameters::DIVERGENCE_CHECK;
							}
							if (HP.IsKeyWord("factor")) {
                                                        oLineSearchParam.dDivergenceCheck = HP.GetReal();
                                                        if (oLineSearchParam.dDivergenceCheck < 1.) {
									silent_cerr("divergence check factor must be greater than one at line "
                                                                                    << HP.GetLineData() << std::endl);
									throw ErrGeneric(MBDYN_EXCEPT_ARGS);
								}
							}
						} else if (HP.IsKeyWord("algorithm")) {
                                                oLineSearchParam.uFlags &= ~LineSearchParameters::ALGORITHM;
							if (HP.IsKeyWord("cubic")) {
                                                        oLineSearchParam.uFlags |= LineSearchParameters::ALGORITHM_CUBIC;
							} else if (HP.IsKeyWord("factor")) {
                                                        oLineSearchParam.uFlags |= LineSearchParameters::ALGORITHM_FACTOR;
							} else {
								silent_cerr("Keyword \"cubic\" or \"factor\" expected at line " << HP.GetLineData() << std::endl);
								throw ErrGeneric(MBDYN_EXCEPT_ARGS);
							}
						} else if (HP.IsKeyWord("scale" "newton" "step")) {
							if (HP.GetYesNoOrBool()) {
                                                        oLineSearchParam.uFlags |= LineSearchParameters::SCALE_NEWTON_STEP;
							} else {
                                                        oLineSearchParam.uFlags &= ~LineSearchParameters::SCALE_NEWTON_STEP;
							}
							if (HP.IsKeyWord("min" "scale")) {
                                                        oLineSearchParam.dMinStepScale = HP.GetReal();
                                                        if (oLineSearchParam.dMinStepScale < 0. || oLineSearchParam.dMinStepScale > 1.) {
									silent_cerr("min scale must be in range [0-1] at line " << HP.GetLineData() << std::endl);
									throw ErrGeneric(MBDYN_EXCEPT_ARGS);
								}
							}
						} else if (HP.IsKeyWord("print" "convergence" "info")) {
							if (HP.GetYesNoOrBool()) {
                                                        oLineSearchParam.uFlags |= LineSearchParameters::PRINT_CONVERGENCE_INFO;
							} else {
                                                        oLineSearchParam.uFlags &= ~LineSearchParameters::PRINT_CONVERGENCE_INFO;
							}
						} else if (HP.IsKeyWord("verbose")) {
							if (HP.GetYesNoOrBool()) {
                                                        oLineSearchParam.uFlags |= LineSearchParameters::VERBOSE_MODE;
							} else {
                                                        oLineSearchParam.uFlags &= ~LineSearchParameters::VERBOSE_MODE;
							}
						} else if (HP.IsKeyWord("abort" "at" "lambda" "min")) {
							if (HP.GetYesNoOrBool()) {
                                                        oLineSearchParam.uFlags |= LineSearchParameters::ABORT_AT_LAMBDA_MIN;
							} else {
                                                        oLineSearchParam.uFlags &= ~LineSearchParameters::ABORT_AT_LAMBDA_MIN;
							}
						} else if (HP.IsKeyWord("non" "negative" "slope" "continue")) {
							if (HP.GetYesNoOrBool()) {
                                                        oLineSearchParam.uFlags |= LineSearchParameters::NON_NEGATIVE_SLOPE_CONTINUE;
							} else {
                                                        oLineSearchParam.uFlags &= ~LineSearchParameters::NON_NEGATIVE_SLOPE_CONTINUE;
							}
                                                } else if (HP.IsKeyWord("time" "step" "tolerance")) {
                                                oLineSearchParam.dTimeStepTol = HP.GetReal();

                                                if (oLineSearchParam.dTimeStepTol < 0) {
                                                        silent_cerr("time step tolerance must not be smaller than zero at line "
                                                                    << HP.GetLineData()
                                                                    << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                    }
                                                } else if (HP.IsKeyWord("update" "ratio")) {
                                                oLineSearchParam.dUpdateRatio = HP.GetReal();

                                                if (oLineSearchParam.dUpdateRatio <= 0) {
                                                        silent_cerr("update ratio must be a value greater than zero at line "
                                                                    << HP.GetLineData()
                                                                    << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("mcp" "tolerance")) {
                                                oLineSearchParam.dMcpTol = HP.GetReal();

                                                if (oLineSearchParam.dMcpTol <= 0.) {
                                                        silent_cerr("mcp tolerance must be greater than zero at line "
                                                                    << HP.GetLineData() << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("mcp" "sigma")) {
                                                oLineSearchParam.dMcpSigma = HP.GetReal();

                                                if (!(oLineSearchParam.dMcpSigma <= 1. && oLineSearchParam.dMcpSigma >= 0.)) {
                                                        silent_cerr("mcp sigma must be between zero and one at line "
                                                                    << HP.GetLineData() << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("mcp" "rho")) {
                                                oLineSearchParam.dMcpRho = HP.GetReal();

                                                if (!(oLineSearchParam.dMcpRho <= 1. && oLineSearchParam.dMcpRho >= 0.)) {
                                                        silent_cerr("mcp rho must be between zero and one at line "
                                                                    << HP.GetLineData() << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("mcp" "p")) {
                                                oLineSearchParam.dMcpP = HP.GetReal();

                                                if (!(oLineSearchParam.dMcpP > 0.)) {
                                                        silent_cerr("mcp p must be greater than zero at line "
                                                                    << HP.GetLineData() << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                    }
						} else {
							silent_cerr("Keyword \"default solver options\", \"tolerance x\", "
								"\"tolerance min\", \"max iterations\", \"alpha\", "
								"\"lambda min\" \"lambda factor min\", \"max step\" "
								"\"divergence check\", \"algorithm\", \"scale newton step\" "
								"\"print convergence info\", \"verbose\", "
                                                            "\"abort at lambda min\", \"time step tolerance\", \"update ratio\", "
                                                            "\"zero gradient\", \"mcp tolerance\", \"mcp sigma\", \"mcp rho\" or \"mcp p\" expected at line " << HP.GetLineData() << std::endl);
							throw ErrGeneric(MBDYN_EXCEPT_ARGS);
						}
					}
                                        break;

                        case NonlinearSolver::NOX:
#ifdef USE_TRILINOS
                                bTrueNewtonRaphson = true;
                                oNoxSolverParam.bKeepJacAcrossSteps = false;
                                oNoxSolverParam.iIterationsBeforeAssembly = 0;

                                if (HP.IsKeyWord("modified")) {
                                        bTrueNewtonRaphson = false;
                                        oNoxSolverParam.iIterationsBeforeAssembly = HP.GetInt();

                                        if (HP.IsKeyWord("keep" "jacobian" "matrix") || HP.IsKeyWord("keep" "jacobian")) {
                                                oNoxSolverParam.bKeepJacAcrossSteps = true;
				}

                                        if (HP.IsKeyWord("honor" "element" "requests")) {
                                                bHonorJacRequest = true;
                                        }
                                }

                                while (HP.IsArg()) {
                                        if (HP.IsKeyWord("print" "convergence" "info")) {
                                                if (HP.GetYesNoOrBool()) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::PRINT_CONVERGENCE_INFO;
                                                } else {
                                                        oNoxSolverParam.uFlags &= ~NoxSolverParameters::PRINT_CONVERGENCE_INFO;
                                                }
                                        } else if (HP.IsKeyWord("verbose")) {
                                                if (HP.GetYesNoOrBool()) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::VERBOSE_MODE;
                                                } else {
                                                        oNoxSolverParam.uFlags &= ~NoxSolverParameters::VERBOSE_MODE;
                                                }
                                        } else if (HP.IsKeyWord("weighted" "rms" "relative" "tolerance")) {
                                                oNoxSolverParam.dWrmsRelTol = HP.GetReal();
                                        } else if (HP.IsKeyWord("weighted" "rms" "absolute" "tolerance")) {
                                                oNoxSolverParam.dWrmsAbsTol = HP.GetReal();
                                        } else if (HP.IsKeyWord("linear" "solver")) {
                                                oNoxSolverParam.uFlags &= ~NoxSolverParameters::LINEAR_SOLVER_MASK;

                                                if (HP.IsKeyWord("gmres")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::LINEAR_SOLVER_GMRES;
                                                } else if (HP.IsKeyWord("cg")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::LINEAR_SOLVER_CG;
                                                } else if (HP.IsKeyWord("cgs")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::LINEAR_SOLVER_CGS;
                                                } else if (HP.IsKeyWord("tfqmr")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::LINEAR_SOLVER_TFQMR;
                                                } else if (HP.IsKeyWord("bicgstab")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::LINEAR_SOLVER_BICGSTAB;
                                                } else {
                                                        silent_cerr("keywords \"gmres\", \"cg\", \"cgs\", \"tfqmr\" or \"bicgstab\" expected "
                                                                    << HP.GetLineData()
                                                                    << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("linear" "solver" "tolerance")) {
                                                oNoxSolverParam.dTolLinSol = HP.GetReal();
                                        } else if (HP.IsKeyWord("linear" "solver" "max" "iterations")) {
                                                oNoxSolverParam.iMaxIterLinSol = HP.GetInt();
                                        } else if (HP.IsKeyWord("krylov" "subspace" "size")) {
                                                oNoxSolverParam.iKrylovSubSpaceSize = HP.GetInt();

                                                if (oNoxSolverParam.iKrylovSubSpaceSize < 1) {
                                                        silent_cerr("krylov subspace size must be greater than zero at line "
                                                                    << HP.GetLineData() << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("use" "preconditioner" "as" "solver")) {
                                                if (HP.GetYesNoOrBool()) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::USE_PRECOND_AS_SOLVER;
                                                } else {
                                                        oNoxSolverParam.uFlags &= ~NoxSolverParameters::USE_PRECOND_AS_SOLVER;
                                                }
                                        } else if (HP.IsKeyWord("jacobian" "operator")) {
                                                oNoxSolverParam.uFlags &= ~NoxSolverParameters::JACOBIAN_OPERATOR_MASK;
                                                if (HP.IsKeyWord("newton" "krylov")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::JACOBIAN_NEWTON_KRYLOV;
                                                } else if (HP.IsKeyWord("newton")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::JACOBIAN_NEWTON;
                                                } else {
                                                        silent_cerr("keywords \"newton krylov\" or \"newton\" expected "
                                                                    << HP.GetLineData()
                                                                    << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("solver")) {
                                                oNoxSolverParam.uFlags &= ~NoxSolverParameters::SOLVER_MASK;

                                                if (HP.IsKeyWord("line" "search" "based")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::SOLVER_LINESEARCH_BASED;
                                                } else if (HP.IsKeyWord("trust" "region" "based")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::SOLVER_TRUST_REGION_BASED;
                                                } else if (HP.IsKeyWord("inexact" "trust" "region" "based")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::SOLVER_INEXACT_TRUST_REGION_BASED;
                                                } else if (HP.IsKeyWord("tensor" "based")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::SOLVER_TENSOR_BASED;
                                                } else {
                                                        silent_cerr("keywords \"line search based\", \"trust region based\", \"inexact trust region based\" or \"tensor based\" expected "
                                                                    << HP.GetLineData()
                                                                    << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("minimum" "step")) {
                                                oNoxSolverParam.dMinStep = HP.GetReal();

                                                if (oNoxSolverParam.dMinStep <= 0.) {
                                                        silent_cerr("minimum step must be greater than zero at line "
                                                                    << HP.GetLineData() << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("recovery" "step")) {
                                                oNoxSolverParam.dRecoveryStep = HP.GetReal();

                                                if (oNoxSolverParam.dRecoveryStep <= 0.) {
                                                        silent_cerr("\"recovery step\" must be greater than zero at line "
                                                                    << HP.GetLineData() << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("recovery" "step" "type")) {
                                                oNoxSolverParam.uFlags &= ~NoxSolverParameters::RECOVERY_STEP_TYPE_MASK;

                                                if (HP.IsKeyWord("constant")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::RECOVERY_STEP_TYPE_CONST;
                                                } else if (HP.IsKeyWord("last" "computed" "step")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::RECOVERY_STEP_TYPE_LAST_STEP;
                                                } else {
                                                        silent_cerr("keywords \"constant\" or \"last computed step\" expected at line "
                                                                    << HP.GetLineData() << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("direction")) {
                                                oNoxSolverParam.uFlags &= ~NoxSolverParameters::DIRECTION_MASK;

                                                if (HP.IsKeyWord("newton")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::DIRECTION_NEWTON;
                                                } else if (HP.IsKeyWord("steepest" "descent")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::DIRECTION_STEEPEST_DESCENT;
                                                } else if (HP.IsKeyWord("nonlinear" "cg")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::DIRECTION_NONLINEAR_CG;
                                                } else if (HP.IsKeyWord("broyden")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::DIRECTION_BROYDEN;
                                                } else {
                                                        silent_cerr("keywords \"newton\", \"steepest descent\", "
                                                                    "\"nonlinear cg\" or \"broyden\" expected at line "
                                                                    << HP.GetLineData() << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("forcing" "term")) {
                                                oNoxSolverParam.uFlags &= ~NoxSolverParameters::FORCING_TERM_MASK;

                                                if (HP.IsKeyWord("constant")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::FORCING_TERM_CONSTANT;
                                                } else if (HP.IsKeyWord("type" "1")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::FORCING_TERM_TYPE1;
                                                } else if (HP.IsKeyWord("type" "2")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::FORCING_TERM_TYPE2;
                                                } else {
                                                        silent_cerr("keywords \"constant\", \"type 1\" or \"type 2\" expected at line "
                                                                    << HP.GetLineData() << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("forcing" "term" "min" "tolerance")) {
                                                oNoxSolverParam.dForcingTermMinTol = HP.GetReal();

                                                if (oNoxSolverParam.dForcingTermMinTol <= 0.) {
                                                        silent_cerr("forcing term min tolerance "
                                                                    "must be greater than zero at line "
                                                                    << HP.GetLineData() << "\n");
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("forcing" "term" "max" "tolerance")) {
                                                oNoxSolverParam.dForcingTermMaxTol = HP.GetReal();

                                                if (oNoxSolverParam.dForcingTermMaxTol <= 0.) {
                                                        silent_cerr("forcing term max tolerance "
                                                                    "must be greater than zero at line "
                                                                    << HP.GetLineData() << "\n");
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("forcing" "term" "alpha")) {
                                                oNoxSolverParam.dForcingTermAlpha = HP.GetReal();

                                                if (oNoxSolverParam.dForcingTermAlpha <= 0.) {
                                                        silent_cerr("forcing term alpha "
                                                                    "must be greater than zero at line "
                                                                    << HP.GetLineData() << "\n");
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("forcing" "term" "gamma")) {
                                                oNoxSolverParam.dForcingTermGamma = HP.GetReal();

                                                if (oNoxSolverParam.dForcingTermGamma <= 0.) {
                                                        silent_cerr("forcing term gamma "
                                                                    "must be greater than zero at line "
                                                                    << HP.GetLineData() << "\n");
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("line" "search" "method")) {
                                                oNoxSolverParam.uFlags &= ~NoxSolverParameters::LINESEARCH_MASK;

                                                if (HP.IsKeyWord("backtrack")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::LINESEARCH_BACKTRACK;
                                                } else if (HP.IsKeyWord("polynomial")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::LINESEARCH_POLYNOMIAL;
                                                } else if (HP.IsKeyWord("more" "thuente")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::LINESEARCH_MORE_THUENTE;
                                                } else {
                                                        silent_cerr("keywords \"backtrack\", \"polynomial\" or \"more thuente\" expected at line "
                                                                    << HP.GetLineData() << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("line" "search" "max" "iterations")) {
                                                oNoxSolverParam.iMaxIterLineSearch = HP.GetInt();

                                                if (oNoxSolverParam.iMaxIterLineSearch < 1) {
                                                        silent_cerr("\"line search max iterations\" must be greater than or equal to one at line "
                                                                    << HP.GetLineData() << std::endl);
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("sufficient" "decrease" "condition")) {
                                                oNoxSolverParam.uFlags &= ~NoxSolverParameters::SUFFICIENT_DEC_COND_MASK;

                                                if (HP.IsKeyWord("armijo" "goldstein")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::SUFFICIENT_DEC_COND_ARMIJO_GOLDSTEIN;
                                                } else if (HP.IsKeyWord("ared" "pred")) {
                                                        oNoxSolverParam.uFlags |= NoxSolverParameters::SUFFICIENT_DEC_COND_ARED_PRED;
                                                } else {
                                                        silent_cerr("Keywords \"armijo goldstein\" or \"ared pred\" expected at line " << HP.GetLineData() << "\n");
                                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                                }
                                        } else if (HP.IsKeyWord("inner" "iterations" "before" "assembly")) {
                                                oNoxSolverParam.iInnerIterBeforeAssembly = HP.GetInt();
                                        } else {
                                                silent_cerr("Keywords \"print convergence info\", "
                                                            "\"verbose\", "
                                                            "\"newton krylov perturbation\", "
                                                            "\"solver\", "
                                                            "\"jacobian operator\", "
                                                            "\"direction\", "
                                                            "\"linear solver\", "
                                                            "\"linear solver tolerance\", "
                                                            "\"linear solver max iterations\", "
                                                            "\"krylov subspace size\", "
                                                            "\"forcing term\", "
                                                            "\"line search method\", "
                                                            "\"line search max iterations\", "
                                                            "\"minimum step\", "
                                                            "\"recovery step\", "
                                                            "\"recovery step type\", "
                                                            "\"sufficient decrease condition\", "
                                                            "\"inner iterations before assembly\", "
                                                            "\"weighted rms relative tolerance\" or "
                                                            "\"weighted rms absolute tolerance\" expected at line "
                                                            << HP.GetLineData() << std::endl);
                                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                        }
                                }

                                if (oNoxSolverParam.dForcingTermMinTol >= oNoxSolverParam.dForcingTermMaxTol) {
                                        silent_cerr("\"forcing term min tolerance\" must be "
                                                    "less than \"forcing term max tolerance\" at line "
                                                    << HP.GetLineData() << "\n");
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if ((oNoxSolverParam.uFlags & NoxSolverParameters::SOLVER_LINESEARCH_BASED) == 0 &&
                                    (oNoxSolverParam.uFlags & NoxSolverParameters::JACOBIAN_NEWTON_KRYLOV) != 0) {
                                        silent_cerr("warning: jacobian operator \"newton krylov\" can be used only for the line search based solver\n");
                                        oNoxSolverParam.uFlags &= ~NoxSolverParameters::JACOBIAN_OPERATOR_MASK;
                                        oNoxSolverParam.uFlags |= NoxSolverParameters::JACOBIAN_NEWTON;
                                }

                                if ((oNoxSolverParam.uFlags & NoxSolverParameters::DIRECTION_STEEPEST_DESCENT) != 0 &&
                                    (oNoxSolverParam.uFlags & NoxSolverParameters::JACOBIAN_NEWTON_KRYLOV) != 0) {
                                        silent_cerr("warning: jacobian operator \"newton krylov\" cannot be used with direction \"steepest descent\"\n");
                                        oNoxSolverParam.uFlags &= ~NoxSolverParameters::JACOBIAN_OPERATOR_MASK;
                                        oNoxSolverParam.uFlags |= NoxSolverParameters::JACOBIAN_NEWTON;
                                }

                                if ((oNoxSolverParam.uFlags & (NoxSolverParameters::DIRECTION_STEEPEST_DESCENT | NoxSolverParameters::DIRECTION_NONLINEAR_CG)) != 0 &&
                                    (oNoxSolverParam.uFlags & NoxSolverParameters::SOLVER_TENSOR_BASED) != 0) {
                                        silent_cerr("warning: directions \"steepest descent\" and \"nonlinear cg\" are not valid for solver \"tensor based\"\n");
                                        oNoxSolverParam.uFlags &= ~NoxSolverParameters::DIRECTION_MASK;
                                        oNoxSolverParam.uFlags |= NoxSolverParameters::DIRECTION_NEWTON;
                                }

                                if ((oNoxSolverParam.uFlags & NoxSolverParameters::SOLVER_LINESEARCH_BASED) == 0 &&
                                    (oNoxSolverParam.uFlags & NoxSolverParameters::DIRECTION_BROYDEN) != 0) {
                                        silent_cerr("warning: direction \"broyden\" is valid for solver \"line search based\" only\n");
                                        oNoxSolverParam.uFlags &= ~NoxSolverParameters::SOLVER_MASK;
                                        oNoxSolverParam.uFlags |= NoxSolverParameters::SOLVER_LINESEARCH_BASED;
                                }

                                if ((oNoxSolverParam.uFlags & NoxSolverParameters::DIRECTION_BROYDEN) != 0 &&
                                    (oNoxSolverParam.uFlags & NoxSolverParameters::JACOBIAN_NEWTON_KRYLOV) != 0) {
                                        silent_cerr("warning: direction \"broyden\" cannot be used with jacobian operator \"newton krylov\"\n");
                                        oNoxSolverParam.uFlags &= ~NoxSolverParameters::JACOBIAN_OPERATOR_MASK;
                                        oNoxSolverParam.uFlags |= NoxSolverParameters::JACOBIAN_NEWTON;
                                }

                                if (oNoxSolverParam.uFlags & NoxSolverParameters::USE_PRECOND_AS_SOLVER) {
                                        if (oNoxSolverParam.iIterationsBeforeAssembly > 0) {
                                                silent_cerr("warning: cannot use option \"modified\" "
                                                            "if \"use preconditioner as solver\" is enabled\n");
                                        }

                                        oNoxSolverParam.bKeepJacAcrossSteps = false;
                                        oNoxSolverParam.iIterationsBeforeAssembly = 0;
                                }
#else
                                silent_cerr("nox solver is not available!\n"
                                            "configure with --with-trilinos in order to use nox!\n");
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
#endif
				break;

			case NonlinearSolver::MATRIXFREE:
				switch (KeyWords(HP.GetWord())) {
				case DEFAULT:
					MFSolverType = MatrixFreeSolver::DEFAULT;
					break;


				case BICGSTAB:
					MFSolverType = MatrixFreeSolver::BICGSTAB;
					break;

				case GMRES:
					MFSolverType = MatrixFreeSolver::GMRES;
					break;

				default:
					silent_cerr("unknown iterative "
						"solver at line "
						<< HP.GetLineData()
						<< std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
				}

				if (HP.IsKeyWord("tolerance")) {
					dIterTol = HP.GetReal();
					DEBUGLCOUT(MYDEBUG_INPUT,"inner "
							"iterative solver "
							"tolerance: "
							<< dIterTol
							<< std::endl);
				}

				if (HP.IsKeyWord("steps")) {
					iIterativeMaxSteps = HP.GetInt();
					DEBUGLCOUT(MYDEBUG_INPUT, "maximum "
							"number of inner "
							"steps for iterative "
							"solver: "
							<< iIterativeMaxSteps
							<< std::endl);
				}

				if (HP.IsKeyWord("tau")) {
					dIterertiveTau = HP.GetReal();
					DEBUGLCOUT(MYDEBUG_INPUT,
							"tau scaling "
							"coefficient "
							"for iterative "
							"solver: "
							<< dIterertiveTau
							<< std::endl);
				}

				if (HP.IsKeyWord("eta")) {
					dIterertiveEtaMax = HP.GetReal();
					DEBUGLCOUT(MYDEBUG_INPUT, "maximum "
							"eta coefficient "
							"for iterative "
							"solver: "
							<< dIterertiveEtaMax
							<< std::endl);
				}

				if (HP.IsKeyWord("preconditioner")) {
					KeyWords KPrecond = KeyWords(HP.GetWord());
					switch (KPrecond) {
					case FULLJACOBIAN:
					case FULLJACOBIANMATRIX:
						PcType = Preconditioner::FULLJACOBIANMATRIX;
						if (HP.IsKeyWord("steps")) {
							iPrecondSteps = HP.GetInt();
							DEBUGLCOUT(MYDEBUG_INPUT,
									"number of steps "
									"before recomputing "
									"the preconditioner: "
									<< iPrecondSteps
									<< std::endl);
						}
						if (HP.IsKeyWord("honor" "element" "requests")) {
							bHonorJacRequest = true;
							DEBUGLCOUT(MYDEBUG_INPUT,
									"honor elements' "
									"request to update "
									"the preconditioner"
									<< std::endl);
						}
						break;

						/* add other preconditioners
						 * here */

					default:
						silent_cerr("unknown "
							"preconditioner "
							"at line "
							<< HP.GetLineData()
							<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}
					break;
				}
				break;

			default:
				ASSERT(0);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			break;

		case REALTIME:
			pRTSolver = ReadRTSolver(this, HP);
			break;

		case THREADS:
			if (HP.IsKeyWord("auto")) {
#ifdef USE_MULTITHREAD
				int n = get_nprocs();
				/* sanity checks ... */
				if (n <= 0) {
					silent_cerr("got " << n << " CPUs "
							"at line "
							<< HP.GetLineData()
							<< std::endl);
					nThreads = 1;
				} else {
					nThreads = n;
				}
#else /* ! USE_MULTITHREAD */
				silent_cerr("configure with "
						"--enable-multithread "
						"for multithreaded assembly"
						<< std::endl);
#endif /* ! USE_MULTITHREAD */

			} else if (HP.IsKeyWord("disable")) {
#ifdef USE_MULTITHREAD
				nThreads = 1;
#endif /* USE_MULTITHREAD */

			} else {
#ifdef USE_MULTITHREAD
				bool bAssembly = false;
				bool bSolver = false;
				bool bAll = true;
				unsigned nt;
#endif // USE_MULTITHREAD

				if (HP.IsKeyWord("assembly")) {
#ifdef USE_MULTITHREAD
					bAll = false;
					bAssembly = true;
#endif // USE_MULTITHREAD

				} else if (HP.IsKeyWord("solver")) {
#ifdef USE_MULTITHREAD
					bAll = false;
					bSolver = true;
#endif // USE_MULTITHREAD
				}

#ifdef USE_MULTITHREAD
				nt =
#endif // USE_MULTITHREAD
				HP.GetInt();

#ifdef USE_MULTITHREAD
				if (bAll || bAssembly) {
					nThreads = nt;
				}

				if (bAll || bSolver) {
					bSolverThreads = true;
					nSolverThreads = nt;
				}
#else /* ! USE_MULTITHREAD */
				silent_cerr("configure with "
						"--enable-multithread "
						"for multithreaded assembly"
						<< std::endl);
#endif /* ! USE_MULTITHREAD */
			}
			break;


		default:
			silent_cerr("unknown description at line "
				<< HP.GetLineData() << "; aborting..."
				<< std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
	}

EndOfCycle: /* esce dal ciclo di lettura */

        switch (RegularType) {
        case StepIntegratorType::INT_IMPLICITEULER:
        case StepIntegratorType::INT_CRANKNICOLSON:
        case StepIntegratorType::INT_MS2:
        case StepIntegratorType::INT_HOPE:
        case StepIntegratorType::INT_HYBRID:
                // TODO: Add all solvers with variable time step capabilities here ...
                break;
        default:
                if (pTSC && !dynamic_cast<NoChange*>(pTSC)) {
                        silent_cerr("warning: time step control strategy is not applicable for the selected step integrator; defaulting to NoChange time step control\n" );

                        SAFEDELETE(pTSC);
                        SAFENEWWITHCONSTRUCTOR(pTSC, NoChange, NoChange(this));
                }
        }

        if (pTSC == 0) {
                pedantic_cout("No time step control strategy defined; defaulting to NoChange time step control\n" );
                SAFENEWWITHCONSTRUCTOR(pTSC, NoChange, NoChange(this));
        }

	if (dFinalTime < dInitialTime) {
		eAbortAfter = AFTER_ASSEMBLY;
	}

	if (dFinalTime == dInitialTime) {
		eAbortAfter = AFTER_DERIVATIVES;
	}

	/* Metodo di integrazione di default */
	if (!bMethod) {
		ASSERT(RegularType == INT_UNKNOWN);

		/* FIXME: maybe we should use a better value
		 * like 0.6; however, BDF should be conservative */
		SAFENEW(pRhoRegular, NullDriveCaller);

		/* DriveCaller per Rho asintotico per variabili algebriche */
		pRhoAlgebraicRegular = pRhoRegular->pCopy();

		RegularType = INT_MS2;
	}

	/* Metodo di integrazione di default */
	if (iDummyStepsNumber && !bDummyStepsMethod) {
		ASSERT(DummyType == INT_UNKNOWN);

		SAFENEW(pRhoDummy, NullDriveCaller);

		/* DriveCaller per Rho asintotico per variabili algebriche */
		pRhoAlgebraicDummy = pRhoDummy->pCopy();

		DummyType = INT_MS2;
	}

        if (dDerivativesSolTol < 0.) {
             dDerivativesSolTol = dSolutionTol;
        }

	/* costruzione dello step solver derivative */
	SAFENEWWITHCONSTRUCTOR(pDerivativeSteps,
			DerivativeSolver,
			DerivativeSolver(dDerivativesTol,
                                dDerivativesSolTol,
				dInitialTimeStep*dDerivativesCoef,
				iDerivativesMaxIterations,
				bModResTest,
				iDerivativesCoefMaxIter,
				dDerivativesCoefFactor));

	/* First step prediction must always be Crank-Nicolson for accuracy */
	if (iDummyStepsNumber) {

		/* costruzione dello step solver dummy */
		switch (DummyType) {
		case INT_CRANKNICOLSON:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					CrankNicolsonIntegrator,
					CrankNicolsonIntegrator(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						bModResTest));
			break;

		case INT_MS2:
		SAFENEWWITHCONSTRUCTOR(pFirstDummyStep,
				CrankNicolsonIntegrator,
				CrankNicolsonIntegrator(dDummyStepsTolerance,
					dSolutionTol,
					iDummyStepsMaxIterations,
					bModResTest));
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					Multistep2Solver,
					Multistep2Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pRhoDummy,
						pRhoAlgebraicDummy,
						bModResTest));
			break;

		case INT_MS3:
			SAFENEWWITHCONSTRUCTOR(pFirstDummyStep,
					CrankNicolsonIntegrator,
					CrankNicolsonIntegrator(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						bModResTest));
			SAFENEWWITHCONSTRUCTOR(pSecondDummyStep,
					Multistep2Solver,
					Multistep2Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pSecondRhoDummy,
						pSecondRhoAlgebraicDummy,
						bModResTest));
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					TunableStep3Solver,
					TunableStep3Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pRhoDummy,
						pRhoAlgebraicDummy,
						bModResTest));
			break;

		case INT_MS4:
			SAFENEWWITHCONSTRUCTOR(pFirstDummyStep,
					CrankNicolsonIntegrator,
					CrankNicolsonIntegrator(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						bModResTest));
			SAFENEWWITHCONSTRUCTOR(pSecondDummyStep,
					Multistep2Solver,
					Multistep2Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pSecondRhoDummy,
						pSecondRhoAlgebraicDummy,
						bModResTest));
			SAFENEWWITHCONSTRUCTOR(pThirdDummyStep,
					TunableStep3Solver,
					TunableStep3Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pThirdRhoDummy,
						pThirdRhoAlgebraicDummy,
						bModResTest));
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					TunableStep4Solver,
					TunableStep4Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pRhoDummy,
						pRhoAlgebraicDummy,
						bModResTest));
			break;

		case INT_SS2:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					SS2Solver,
					SS2Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pRhoDummy,
						pRhoAlgebraicDummy,
						bModResTest));
			break;

		case INT_SS3:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					SS3Solver,
					SS3Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pRhoDummy,
						pRhoAlgebraicDummy,
						bModResTest));
			break;

		case INT_SS4:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					SS4Solver,
					SS4Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pRhoDummy,
						pRhoAlgebraicDummy,
						bModResTest));
			break;

		case INT_BATHE:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					TunableBatheSolver,
					TunableBatheSolver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pRhoDummy,
						pRhoAlgebraicDummy,
						bModResTest));
			break;

		case INT_MSSTC3:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					Msstc3Solver,
					Msstc3Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pRhoDummy,
						pRhoAlgebraicDummy,
						bModResTest));
			break;

		case INT_MSSTH3:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					Mssth3Solver,
					Mssth3Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pRhoDummy,
						pRhoAlgebraicDummy,
						bModResTest));
			break;

		case INT_MSSTC4:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					Msstc4Solver,
					Msstc4Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pRhoDummy,
						pRhoAlgebraicDummy,
						bModResTest));
			break;

		case INT_MSSTH4:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					Mssth4Solver,
					Mssth4Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pRhoDummy,
						pRhoAlgebraicDummy,
						bModResTest));
			break;

		case INT_MSSTC5:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					Msstc5Solver,
					Msstc5Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pRhoDummy,
						pRhoAlgebraicDummy,
						bModResTest));
			break;

		case INT_MSSTH5:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					Mssth5Solver,
					Mssth5Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pRhoDummy,
						pRhoAlgebraicDummy,
						bModResTest));
			break;

		case INT_DIRK33:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					DIRK33Solver,
					DIRK33Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						bModResTest));
			break;

		case INT_DIRK43:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					DIRK43Solver,
					DIRK43Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						bModResTest));
			break;

		case INT_DIRK54:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					DIRK54Solver,
					DIRK54Solver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						bModResTest));
			break;

		case INT_HOPE:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					HopeSolver,
					HopeSolver(dDummyStepsTolerance,
						dSolutionTol,
						iDummyStepsMaxIterations,
						pRhoDummy,
						pRhoAlgebraicDummy,
						bModResTest));
			break;

		case INT_IMPLICITEULER:
			SAFENEWWITHCONSTRUCTOR(pDummySteps,
					ImplicitEulerIntegrator,
					ImplicitEulerIntegrator(dDummyStepsTolerance,
						dSolutionTol, iDummyStepsMaxIterations,
						bModResTest));
			break;

		default:
			silent_cerr("unknown dummy steps integration method"
				<< std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			break;
		}
	}
	/* constructor step solver for start steps*/
	switch (RegularType)
	{
		case INT_MS2:
		case INT_HOPE:
		case INT_HYBRID:
			SAFENEWWITHCONSTRUCTOR(pFirstRegularStep,
					CrankNicolsonIntegrator,
					CrankNicolsonIntegrator(dTol,
						dSolutionTol,
						iMaxIterations,
						bModResTest));
                        if (eAbortAfter >= AFTER_REGULAR_STEP_0) {
                                eAbortAfter = static_cast<AbortAfter>(eAbortAfter + 1);
                        }
				break;		

		case INT_MS3:
			SAFENEWWITHCONSTRUCTOR(pFirstRegularStep,
					CrankNicolsonIntegrator,
					CrankNicolsonIntegrator(dTol,
						dSolutionTol,
						iMaxIterations,
						bModResTest));
			SAFENEWWITHCONSTRUCTOR(pSecondRegularStep,
					Multistep2Solver,
					Multistep2Solver(dTol,
						dSolutionTol,
						iMaxIterations,
						pSecondRhoRegular,
						pSecondRhoAlgebraicRegular,
						bModResTest));
                        if (eAbortAfter >= AFTER_REGULAR_STEP_0) {
                                eAbortAfter = static_cast<AbortAfter>(eAbortAfter + 2);
                        }                        
				break;

		case INT_MS4:
			SAFENEWWITHCONSTRUCTOR(pFirstRegularStep,
					CrankNicolsonIntegrator,
					CrankNicolsonIntegrator(dTol,
						dSolutionTol,
						iMaxIterations,
						bModResTest));
			SAFENEWWITHCONSTRUCTOR(pSecondRegularStep,
					Multistep2Solver,
					Multistep2Solver(dTol,
						dSolutionTol,
						iMaxIterations,
						pSecondRhoRegular,
						pSecondRhoAlgebraicRegular,
						bModResTest));
			SAFENEWWITHCONSTRUCTOR(pThirdRegularStep,
					TunableStep3Solver,
					TunableStep3Solver(dTol,
						dSolutionTol,
						iMaxIterations,
						pThirdRhoRegular,
						pThirdRhoAlgebraicRegular,
						bModResTest));
                        if (eAbortAfter >= AFTER_REGULAR_STEP_0) {
                                eAbortAfter = static_cast<AbortAfter>(eAbortAfter + 3);
                        }                        
				break;
	
	default:
		break;
	}


	/* costruzione dello step solver per i passi normali */
	switch (RegularType) {
	case INT_CRANKNICOLSON:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
			CrankNicolsonIntegrator,
			CrankNicolsonIntegrator(dTol,
				dSolutionTol,
				iMaxIterations,
				bModResTest));
		break;

	case INT_MS2:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				Multistep2Solver,
				Multistep2Solver(dTol,
					dSolutionTol,
					iMaxIterations,
					pRhoRegular,
					pRhoAlgebraicRegular,
					bModResTest));
		break;

	case INT_MS3:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				TunableStep3Solver,
				TunableStep3Solver(dTol,
					dSolutionTol,
					iMaxIterations,
					pRhoRegular,
					pRhoAlgebraicRegular,
					bModResTest));
		break;

	case INT_MS4:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				TunableStep4Solver,
				TunableStep4Solver(dTol,
					dSolutionTol,
					iMaxIterations,
					pRhoRegular,
					pRhoAlgebraicRegular,
					bModResTest));
		break;

	case INT_SS2:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				SS2Solver,
				SS2Solver(dTol,
					dSolutionTol,
					iMaxIterations,
					pRhoRegular,
					pRhoAlgebraicRegular,
					bModResTest));
		break;

	case INT_SS3:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				SS3Solver,
				SS3Solver(dTol,
					dSolutionTol,
					iMaxIterations,
					pRhoRegular,
					pRhoAlgebraicRegular,
					bModResTest));
		break;

	case INT_SS4:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				SS4Solver,
				SS4Solver(dTol,
					dSolutionTol,
					iMaxIterations,
					pRhoRegular,
					pRhoAlgebraicRegular,
					bModResTest));
		break;

	case INT_BATHE:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				TunableBatheSolver,
				TunableBatheSolver(dTol,
					dSolutionTol,
					iMaxIterations,
					pRhoRegular,
					pRhoAlgebraicRegular,
					bModResTest));
		break;

	case INT_MSSTC3:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				Msstc3Solver,
				Msstc3Solver(dTol,
					dSolutionTol,
					iMaxIterations,
					pRhoRegular,
					pRhoAlgebraicRegular,
					bModResTest));
		break;

	case INT_MSSTH3:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				Mssth3Solver,
				Mssth3Solver(dTol,
					dSolutionTol,
					iMaxIterations,
					pRhoRegular,
					pRhoAlgebraicRegular,
					bModResTest));
		break;

	case INT_MSSTC4:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				Msstc4Solver,
				Msstc4Solver(dTol,
					dSolutionTol,
					iMaxIterations,
					pRhoRegular,
					pRhoAlgebraicRegular,
					bModResTest));
		break;

	case INT_MSSTH4:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				Mssth4Solver,
				Mssth4Solver(dTol,
					dSolutionTol,
					iMaxIterations,
					pRhoRegular,
					pRhoAlgebraicRegular,
					bModResTest));
		break;

	case INT_MSSTC5:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				Msstc5Solver,
				Msstc5Solver(dTol,
					dSolutionTol,
					iMaxIterations,
					pRhoRegular,
					pRhoAlgebraicRegular,
					bModResTest));
		break;

	case INT_MSSTH5:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				Mssth5Solver,
				Mssth5Solver(dTol,
					dSolutionTol,
					iMaxIterations,
					pRhoRegular,
					pRhoAlgebraicRegular,
					bModResTest));
		break;

	case INT_DIRK33:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				DIRK33Solver,
				DIRK33Solver(dTol,
					dSolutionTol,
					iMaxIterations,
					bModResTest));
		break;

	case INT_DIRK43:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				DIRK43Solver,
				DIRK43Solver(dTol,
					dSolutionTol,
					iMaxIterations,
					bModResTest));
		break;

	case INT_DIRK54:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				DIRK54Solver,
				DIRK54Solver(dTol,
					dSolutionTol,
					iMaxIterations,
					bModResTest));
		break;

	case INT_HOPE:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				HopeSolver,
				HopeSolver(dTol,
					dSolutionTol,
					iMaxIterations,
					pRhoRegular,
					pRhoAlgebraicRegular,
					bModResTest));
		break;

	case INT_IMPLICITEULER:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				ImplicitEulerIntegrator,
				ImplicitEulerIntegrator(dTol,
					dSolutionTol,
					iMaxIterations,
					bModResTest));
		break;
	case INT_HYBRID:
		SAFENEWWITHCONSTRUCTOR(pRegularSteps,
				       HybridStepIntegrator,
				       HybridStepIntegrator(eHybridDefaultIntRegular,
							    dTol,
							    dSolutionTol,
							    iMaxIterations,
							    pRhoRegular,
							    pRhoAlgebraicRegular,
							    bModResTest));
		break;
	default:
		silent_cerr("Unknown integration method" << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		break;
	}

	if (bSetScaleAlgebraic) {
		dScaleAlgebraic = 1. / dInitialTimeStep;
	}

#ifdef USE_MULTITHREAD
	if (bSolverThreads) {
		if (!CurrLinearSolver.SetNumThreads(nSolverThreads)) {
			silent_cerr("linear solver "
					<< CurrLinearSolver.GetSolverName()
					<< " does not support "
					"threaded solution" << std::endl);
		}
	}
#endif /* USE_MULTITHREAD */
}

#if defined USE_LAPACK || defined USE_ARPACK || defined USE_JDQZ
static int
do_eig(const doublereal& b, const doublereal& re,
	const doublereal& im, const doublereal& h,
	doublereal& sigma, doublereal& omega,
	doublereal& csi, doublereal& freq)
{
	// denominator
	doublereal d = re + b;
	d *= d;
	d += im*im;
	d *= h/2.;

	// real & imag
	sigma = (re*re - b*b + im*im)/d;
	omega = 2.*b*im/d;

	// frequency and damping factor
	if (im != 0.) {
		d = sigma*sigma + omega*omega;
		if (d > std::numeric_limits<doublereal>::epsilon()) {
			csi = -100*sigma/sqrt(d);

		} else {
			csi = 0.;
		}

		freq = omega/(2*M_PI);

	} else {
		if (std::abs(sigma) < std::numeric_limits<doublereal>::epsilon()) {
			csi = 0.;

		} else {
			csi = -100.*copysign(1, sigma);
		}

		freq = 0.;
	}

	return 0;
}

namespace {
     struct EigenValue {
          EigenValue(doublereal sigma, doublereal omega, doublereal csi, doublereal freq)
               :sigma(sigma), omega(omega), csi(csi), freq(freq) {
          }

          bool operator<(const EigenValue& rhs) const {
               return (freq == rhs.freq) ? sigma < rhs.sigma : freq < rhs.freq;
          }

          doublereal sigma, omega, csi, freq;
     };
};

// Writes eigenvalues to the .out file in human-readable form
static void
output_eigenvalues(const VectorHandler *pBeta,
	const VectorHandler& R, const VectorHandler& I,
	const doublereal& dShiftR,
	DataManager* pDM,
	const Solver::EigenAnalysis *pEA,
	integer iLow, integer iHigh,
	std::vector<bool>& vOut)
{
	std::ostream& Out = pDM->GetOutFile();

	/* Output? */
	Out << "Mode n. " "  " "    Real    " "   " "    Imag    " "  " "    " "   Damp %   " "  Freq Hz" << std::endl;

	integer iNVec = R.iGetSize();

        std::vector<EigenValue> rgEigenValues;

        rgEigenValues.reserve(iNVec);

	for (int iCnt = 1; iCnt <= iNVec; iCnt++) {
		doublereal b = pBeta ? (*pBeta)(iCnt) : 1.;
		doublereal re = R(iCnt) + dShiftR;
		doublereal im = I(iCnt);
		doublereal sigma;
		doublereal omega;
		doublereal csi;
		doublereal freq;

		if (iCnt < iLow || iCnt > iHigh) {
			vOut[iCnt - 1] = false;
			continue;
		}

		const doublereal& h = pEA->dParam;
		do_eig(b, re, im, h, sigma, omega, csi, freq);

		if (freq < pEA->dLowerFreq || freq > pEA->dUpperFreq) {
			vOut[iCnt - 1] = false;
			continue;
		}

		vOut[iCnt - 1] = true;

                rgEigenValues.emplace_back(sigma, omega, csi, freq);
	}

        std::sort(rgEigenValues.begin(), rgEigenValues.end());

        for (size_t iCnt = 0; iCnt < rgEigenValues.size(); ++iCnt) {
             Out << std::setw(8) << iCnt + 1 << ": "
                 << std::setw(12) << rgEigenValues[iCnt].sigma
                 << ((rgEigenValues[iCnt].omega >= 0.) ? " + " : " - ") << std::setw(12)
                 << std::fabs(rgEigenValues[iCnt].omega) << " j"
                 << "    " << std::setw(12) << rgEigenValues[iCnt].csi
                 << "    " << std::setw(12) << rgEigenValues[iCnt].freq << '\n';
        }
}
#endif // defined USE_LAPACK || defined USE_ARPACK || defined USE_JDQZ

#ifdef USE_LAPACK
// Computes eigenvalues and eigenvectors using LAPACK's
// generalized non-symmetric eigenanalysis
static void
eig_lapack(const MatrixHandler* pMatA, const MatrixHandler* pMatB,
	DataManager *pDM, Solver::EigenAnalysis *pEA,
	bool bNewLine, const unsigned uCurr)
{
	const FullMatrixHandler& MatA = dynamic_cast<const FullMatrixHandler &>(*pMatA);
	const FullMatrixHandler& MatB = dynamic_cast<const FullMatrixHandler &>(*pMatB);

	char sB[2] = "N";
	if ((pEA->uFlags & Solver::EigenAnalysis::EIG_BALANCE)
		== Solver::EigenAnalysis::EIG_BALANCE)
	{
		sB[0] = 'B';

	} else if (pEA->uFlags & Solver::EigenAnalysis::EIG_PERMUTE) {
		sB[0] = 'P';

	} else if (pEA->uFlags & Solver::EigenAnalysis::EIG_SCALE) {
		sB[0] = 'S';
	}

	char sL[2] = "V";
	char sR[2] = "V";
	char sS[2] = "N";

	// iNumDof is a member, set after dataman constr.
	integer iSize = MatA.iGetNumRows();

	// Minimum workspace size. To be improved.
	// NOTE: optimal iWorkSize is computed by dggev() and dggevx()
	// when called with iWorkSize = -1.
	// The computed value is stored in WorkVec[0].
	integer iWorkSize = -1;
	integer iMinWorkSize = -1;
	integer iInfo = 0;

	integer iILO = 1, iIHI = iSize;
	doublereal dABNRM = -1., dBBNRM = -1.;

	doublereal dDmy;
	integer	iDmy;
	logical lDmy;
	doublereal dWV;
	if (sB[0] == 'N') {
		iMinWorkSize = 8*iSize;

		__FC_DECL__(dggev)(
			sL,		// JOBVL
			sR,		// JOBVR
			&iSize,		// N
			&dDmy,		// A
			&iSize,		// LDA
			&dDmy,		// B
			&iSize,		// LDB
			&dDmy,		// ALPHAR
			&dDmy,		// ALPHAI
			&dDmy,		// BETA
			&dDmy,		// VL
			&iSize,		// LDVL
			&dDmy,		// VR
			&iSize,		// LDVR
			&dWV,		// WORK
			&iWorkSize,	// LWORK
			&iInfo);

	} else {
		iMinWorkSize = 6*iSize;

		__FC_DECL__(dggevx)(
			sB,		// BALANC
			sL,		// JOBVL
			sR,		// JOBVR
			sS,		// SENSE
			&iSize,		// N
			&dDmy,		// A
			&iSize,		// LDA
			&dDmy,		// B
			&iSize,		// LDB
			&dDmy,		// ALPHAR
			&dDmy,		// ALPHAI
			&dDmy,		// BETA
			&dDmy,		// VL
			&iSize,		// LDVL
			&dDmy,		// VR
			&iSize,		// LDVR
			&iILO,		// ILO
			&iIHI,		// IHI
			&dDmy,		// LSCALE
			&dDmy,		// RSCALE
			&dABNRM,	// ABNRM
			&dBBNRM,	// BBNRM
			&dDmy,		// RCONDE
			&dDmy,		// RCONDV
			&dWV,		// WORK
			&iWorkSize,	// LWORK
			&iDmy,		// IWORK
			&lDmy,		// BWORK
			&iInfo);
	}

	if (iInfo != 0) {
		if (bNewLine && silent_err) {
			silent_cerr(std::endl);
			bNewLine = false;
		}
		silent_cerr("dggev[x]() query for worksize failed "
			"INFO=" << iInfo << std::endl);
		iInfo = 0;
	}

	iWorkSize = (integer)dWV;
	if (iWorkSize < iMinWorkSize) {
		if (bNewLine && silent_err) {
			silent_cerr(std::endl);
			bNewLine = false;
		}
		silent_cerr("dggev[x]() asked for a worksize " << iWorkSize
			<< " less than the minimum, " << iMinWorkSize
			<< "; using the minimum" << std::endl);
		iWorkSize = iMinWorkSize;
	}

	// Workspaces
	// 	2 matrices iSize x iSize
	//	5 vectors iSize x 1
	//	1 vector iWorkSize x 1
	doublereal* pd = 0;
	int iTmpSize = 2*(iSize*iSize) + 3*iSize + iWorkSize;
	if (sB[0] != 'N') {
		iTmpSize += 2*iSize;
	}
	SAFENEWARR(pd, doublereal, iTmpSize);
#if defined HAVE_MEMSET
	memset(pd, 0, iTmpSize*sizeof(doublereal));
#else // !HAVE_MEMSET
	for (int iCnt = iTmpSize; iCnt-- > 0; ) {
		pd[iCnt] = 0.;
	}
#endif // !HAVE_MEMSET

	// 2 pointer arrays iSize x 1 for the matrices
	doublereal** ppd = 0;
	SAFENEWARR(ppd, doublereal*, 2*iSize);

	// Data Handlers
	doublereal* pdTmp = pd;
	doublereal** ppdTmp = ppd;

	FullMatrixHandler MatVL(pdTmp, ppdTmp, iSize*iSize, iSize, iSize);
	pdTmp += iSize*iSize;
	ppdTmp += iSize;

	FullMatrixHandler MatVR(pdTmp, ppdTmp, iSize*iSize, iSize, iSize);
	pdTmp += iSize*iSize;

	MyVectorHandler AlphaR(iSize, pdTmp);
	pdTmp += iSize;

	MyVectorHandler AlphaI(iSize, pdTmp);
	pdTmp += iSize;

	MyVectorHandler Beta(iSize, pdTmp);
	pdTmp += iSize;

	MyVectorHandler LScale;
	MyVectorHandler RScale;
	if (sB[0] != 'N') {
		LScale.Attach(iSize, pdTmp);
		pdTmp += iSize;

		RScale.Attach(iSize, pdTmp);
		pdTmp += iSize;
	}

	MyVectorHandler WorkVec(iWorkSize, pdTmp);

	// Eigenanalysis
	// NOTE: according to lapack's documentation, dgegv() is deprecated
	// in favour of dggev()... I find dgegv() a little bit faster (10%?)
	// for typical problems (N ~ 1000).
	if (sB[0] == 'N') {
		__FC_DECL__(dggev)(
			sL,			// JOBVL
			sR,			// JOBVR
			&iSize,			// N
			const_cast<doublereal *>(MatA.pdGetMat()),	// A; remove const'ness (hack)
			&iSize,			// LDA
			const_cast<doublereal *>(MatB.pdGetMat()),	// B; remove const'ness (hack)
			&iSize,			// LDB
			AlphaR.pdGetVec(),	// ALPHAR
			AlphaI.pdGetVec(),	// ALPHAI
			Beta.pdGetVec(),	// BETA
			MatVL.pdGetMat(),	// VL
			&iSize,			// LDVL
			MatVR.pdGetMat(),	// VR
			&iSize,			// LDVR
			WorkVec.pdGetVec(),	// WORK
			&iWorkSize,		// LWORK
			&iInfo);		// INFO

	} else {
		std::vector<integer> iIWORK(iSize + 6);

		__FC_DECL__(dggevx)(
			sB,			// BALANCE
			sL,			// JOBVL
			sR,			// JOBVR
			sS,			// SENSE
			&iSize,			// N
			const_cast<doublereal *>(MatA.pdGetMat()),	// A; remove const'ness (hack)
			&iSize,			// LDA
			const_cast<doublereal *>(MatB.pdGetMat()),	// B; remove const'ness (hack)
			&iSize,			// LDB
			AlphaR.pdGetVec(),	// ALPHAR
			AlphaI.pdGetVec(),	// ALPHAI
			Beta.pdGetVec(),	// BETA
			MatVL.pdGetMat(),	// VL
			&iSize,			// LDVL
			MatVR.pdGetMat(),	// VR
			&iSize,			// LDVR
			&iILO,			// ILO
			&iIHI,			// IHI
			LScale.pdGetVec(),	// LSCALE
			RScale.pdGetVec(),	// RSCALE
			&dABNRM,		// ABNRM
			&dBBNRM,		// BBNRM
			&dDmy,			// RCONDE
			&dDmy,			// RCONDV
			WorkVec.pdGetVec(),	// WORK
			&iWorkSize,		// LWORK
			&iIWORK[0],		// IWORK
			&lDmy,			// BWORK
			&iInfo);		// INFO
	}

	std::ostream& Out = pDM->GetOutFile();
	Out << "Info: " << iInfo << ", ";

	if (iInfo == 0) {
		// = 0:  successful exit
		Out << "success"
			<< " BALANC=\"" << sB << "\""
			<< " ILO=" << iILO
			<< " IHI=" << iIHI
			<< " ABNRM=" << dABNRM
			<< " BBNRM=" << dBBNRM
			<< std::endl;

	} else if (iInfo < 0) {
		const char *arg[] = {
			"JOBVL",
			"JOBVR",
			"N",
			"A",
			"LDA",
			"B",
			"LDB",
			"ALPHAR",
			"ALPHAI",
			"BETA",
			"VL",
			"LDVL",
			"VR",
			"LDVR",
			"WORK",
			"LWORK",
			"INFO",
			NULL
		};

		const char *argx[] = {
			"BALANCE",
			"JOBVL",
			"JOBVR",
			"SENSE",
			"N",
			"A",
			"LDA",
			"B",
			"LDB",
			"ALPHAR",
			"ALPHAI",
			"BETA",
			"VL",
			"LDVL",
			"VR",
			"LDVR",
			"ILO",
			"IHI",
			"LSCALE",
			"RSCALE",
			"ABNRM",
			"BBNRM",
			"RCONDE",
			"RCONDV",
			"WORK",
			"LWORK",
			"IWORK",
			"BWORK",
			"INFO",
			NULL
		};

		const char **argv = (sB[0] == 'N' ? arg : argx );

		// < 0:  if INFO = -i, the i-th argument had an illegal value.
		Out << "argument #" << -iInfo
			<< " (" << argv[-iInfo - 1] << ") "
			<< "was passed an illegal value" << std::endl;

	} else if (iInfo > 0 && iInfo <= iSize) {
		/* = 1,...,N:
		 * The QZ iteration failed.  No eigenvectors have been
		 * calculated, but ALPHAR(j), ALPHAI(j), and BETA(j)
		 * should be correct for j=INFO+1,...,N. */
		Out << "the QZ iteration failed, but eigenvalues "
			<< iInfo + 1 << "->" << iSize << " should be correct"
			<< std::endl;

	} else if (iInfo > iSize) {
		const char* const sErrs[] = {
			"DHGEQZ (other than QZ iteration failed in DHGEQZ)",
			"DTGEVC",
			NULL
		};

		Out << "error return from " << sErrs[iInfo - iSize - 1]
			<< std::endl;
	}

	std::vector<bool> vOut(iSize);
	output_eigenvalues(&Beta, AlphaR, AlphaI, 0., pDM, pEA, iILO, iIHI, vOut);

	if (pEA->uFlags & Solver::EigenAnalysis::EIG_OUTPUT_GEOMETRY) {
		pDM->OutputEigGeometry(uCurr, pEA->iResultsPrecision);
	}

	if (pEA->uFlags & Solver::EigenAnalysis::EIG_OUTPUT_EIGENVECTORS) {
		pDM->OutputEigenvectors(&Beta, AlphaR, AlphaI, 0.,
			&MatVL, MatVR, vOut, uCurr, pEA->iResultsPrecision);
	}

	SAFEDELETEARR(pd);
	SAFEDELETEARR(ppd);
}
#endif // USE_LAPACK

#ifdef USE_ARPACK
// Computes eigenvalues and eigenvectors using ARPACK's
// canonical non-symmetric eigenanalysis
static void
eig_arpack(const MatrixHandler* pMatA, SolutionManager* pSM,
	DataManager *pDM, Solver::EigenAnalysis *pEA,
	bool bNewLine, const unsigned uCurr)
{
	// shift
	doublereal SIGMAR = 0.;
	doublereal SIGMAI = 0.;

	// arpack-related vars
	integer IDO;		// 0 at first iteration; then set by dnaupd
	const char *BMAT;	// 'I' for standard problem
	integer N;		// size of problem
	const char *WHICH;	// "SM" to request smallest eigenvalues
	integer NEV;		// number of eigenvalues
	doublereal TOL;		// -1 to use machine precision
	std::vector<doublereal> RESID;	// residual vector (ignored if IDO==0)
	integer NCV;		// number of vectors in subspace
	std::vector<doublereal> V;	// Schur basis
	integer LDV;		// leading dimension of V (==N!)
	integer IPARAM[11] = { 0 };
	integer IPNTR[14] = { 0 };
	std::vector<doublereal> WORKD;
	std::vector<doublereal> WORKL;
	integer LWORKL;
	integer INFO;

        static constexpr char szWhich[][3] = {"LM", "SM", "LR", "SR", "LI", "SI"};
        IDO = 0;
        BMAT = "I";
        N = pMatA->iGetNumRows();
        WHICH = szWhich[pEA->eWhichEigVal];
        NEV = pEA->arpack.iNEV;
        if (NEV > N) {
                silent_cerr("eig_arpack: invalid NEV=" << NEV << " > size of problem (=" << N << ")" << std::endl);
                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
        }

	TOL = pEA->arpack.dTOL;
	RESID.resize(N, 0.);
	NCV = pEA->arpack.iNCV;
	if (NCV > N) {
		silent_cerr("eig_arpack: invalid NCV=" << NCV << " > size of problem (=" << N << ")" << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
	// NOTE: we accommodate NCV + 1 vectors to handle the case
	// of a complex last eigenvalue which requires a vector
	// for the real part and one for the imaginary part
	V.resize(N*(NCV + 1), 0.);
	LDV = N;
	IPARAM[0] = 1;
	IPARAM[2] = pEA->arpack.iMaxIterations;
	IPARAM[3] = 1;
	IPARAM[6] = 1;			// mode 1: canonical problem
	WORKD.resize(3*N, 0.);
	LWORKL = 3*NCV*NCV + 6*NCV;
	WORKL.resize(LWORKL, 0.);
	INFO = 0;

	int cnt = 0;

	const bool bOutputStatus = isatty(fileno(stderr));

	do {
		__FC_DECL__(dnaupd)(&IDO, &BMAT[0], &N, &WHICH[0], &NEV,
			&TOL, &RESID[0], &NCV, &V[0], &LDV, &IPARAM[0], &IPNTR[0],
			&WORKD[0], &WORKL[0], &LWORKL, &INFO);

#if 0
		std::cout << "cnt=" << cnt << ": IDO=" << IDO << ", INFO=" << INFO << std::endl;
#endif

		// compute Y = OP*X
		ASSERT(IPNTR[0] - 1 <= 2*N);
		ASSERT(IPNTR[1] - 1 <= 2*N);
		MyVectorHandler X(N, &WORKD[IPNTR[0] - 1]);
		MyVectorHandler Y(N, &WORKD[IPNTR[1] - 1]);

		/*
		 * NOTE: we are solving the problem

			MatB * X * Lambda = MatA * X

		 * and we want to focus on Ritz parameters Lambda
		 * as close as possible to (1., 0.), which maps
		 * to (0., 0.) in continuous time.
		 *
		 * We are casting the problem in the form

			X * Alpha = A * X

		 * by putting the problem in canonical form

			X * Lambda = MatB \ MatA * X

		 * and then subtracting a shift Sigma = (1., 0) after :

			X * Lambda - X = MatB \ MatA * X - X

			X * (Lambda - 1.) = (MatB \ MatA - I) * X

		 * so

			Alpha = Lambda - 1.

			A = MatB \ MatA - I

		 * and the sequence of operations for Y = A * X is

			X' = MatA * X
			X'' = MatB \ X'
			Y = X'' - X

		 * the eigenvalues need to be modified by adding 1.
		 */


		pMatA->MatVecMul(*pSM->pResHdl(), X);
		pSM->Solve();
		*pSM->pSolHdl() -= X;

		Y = *pSM->pSolHdl();

		static const int CNT = 100;
		cnt++;
		if (bOutputStatus && !(cnt % CNT)) {
			if (bNewLine && silent_err) {
				silent_cerr(std::endl);
				bNewLine = false;
			}
			silent_cerr("\r" "cnt=" << cnt);
		}

		if (mbdyn_stop_at_end_of_iteration()) {
			if (bNewLine && silent_err) {
				silent_cerr(std::endl);
				bNewLine = false;
			}
			silent_cerr((cnt >= CNT ? "\n" : "")
				<< "ARPACK: interrupted" << std::endl);
			return;
		}
	} while (IDO == 1 || IDO == -1);

	if (bOutputStatus) {
		if (bNewLine && silent_err) {
			silent_cerr(std::endl);
			bNewLine = false;
		}
		silent_cerr("\r" "cnt=" << cnt << std::endl);
	}

	// NOTE: improve diagnostics
	if (INFO < 0) {
		if (bNewLine && silent_err) {
			silent_cerr(std::endl);
			bNewLine = false;
		}
		silent_cerr("ARPACK error after " << cnt << " iterations; "
			"IDO=" << IDO << ", INFO=" << INFO << std::endl);
		return;
	}

	switch (INFO) {
	case 0:
		break;

	case 1:
		if (bNewLine && silent_err) {
			silent_cerr(std::endl);
			bNewLine = false;
		}
		silent_cerr("INFO=1: Maximum number of iterations taken. "
			"All possible eigenvalues of OP have been found. IPARAM(5) "
			"returns the number of wanted converged Ritz values "
			"(currently = " << IPARAM[4] << "; requested NEV = " << NEV << ")."
			<< std::endl);
		break;

	case 2:
		if (bNewLine && silent_err) {
			silent_cerr(std::endl);
			bNewLine = false;
		}
		silent_cerr("INFO=2: No longer an informational error. Deprecated starting "
			"with release 2 of ARPACK."
			<< std::endl);
		break;

	case 3:
		if (bNewLine && silent_err) {
			silent_cerr(std::endl);
			bNewLine = false;
		}
		silent_cerr("INFO=3: No shifts could be applied during a cycle of the "
			"implicitly restarted Arnoldi iteration. One possibility "
			"is to increase the size of NCV (currently = " << NCV << ") "
			"relative to NEV (currently = " << NEV << "). "
			"See remark 4 in dnaupd(3)."
			<< std::endl);
		break;

	default:
		if (bNewLine && silent_err) {
			silent_cerr(std::endl);
			bNewLine = false;
		}
		silent_cerr("INFO=" << INFO << ": undocumented value." << std::endl);
		break;
	}

	std::ostream& Out = pDM->GetOutFile();
	Out << "INFO: " << INFO << std::endl;

#if 0
	for (int ii = 0; ii < 11; ii++) {
		std::cerr << "IPARAM(" << ii + 1 << ")=" << IPARAM[ii] << std::endl;
	}
#endif

	logical RVEC = true;
	const char *HOWMNY = "A";
	std::vector<logical> SELECT(NCV);
	std::vector<doublereal> D(2*NCV);
	doublereal *DR = &D[0], *DI = &D[NCV];
	std::vector<doublereal> Z(N*(NCV + 1));
	integer LDZ = N;
	std::vector<doublereal> WORKEV(3*NCV);

#if 0
	std::cerr << "dneupd:" << std::endl
		<< "RVEC = " << RVEC << std::endl
		<< "HOWMNY = " << HOWMNY << std::endl
		<< "SELECT(" << NCV << ")" << std::endl
		<< "DR(" << NEV + 1 << ")" << std::endl
		<< "DI(" << NEV + 1 << ")" << std::endl
		<< "Z(" << N << ", " << NEV + 1 << ")" << std::endl
		<< "LDZ = " << LDZ << std::endl
		<< "SIGMAR = " << SIGMAR << std::endl
		<< "SIGMAI = " << SIGMAI << std::endl
		<< "WORKEV(" << 3*NCV << ")" << std::endl
		<< "BMAT = " << BMAT << std::endl
		<< "N = " << N << std::endl
		<< "WHICH = " << WHICH << std::endl
		<< "NEV = " << NEV << std::endl
		<< "TOL = " << TOL << std::endl
		<< "RESID(" << N << ")" << std::endl
		<< "NCV = " << NCV << std::endl
		<< "V(" << N << ", " << NCV << ")" << std::endl
		<< "LDV = " << LDV << std::endl
		<< "IPARAM(" << sizeof(IPARAM)/sizeof(IPARAM[0]) << ")" << std::endl
		<< "IPNTR(" << sizeof(IPNTR)/sizeof(IPNTR[0]) << ")" << std::endl
		<< "WORKD(" << 3*N << ")" << std::endl
		<< "WORKL(" << LWORKL << ")" << std::endl
		<< "LWORKL = " << LWORKL << std::endl
		<< "INFO = " << INFO << std::endl;
#endif

	__FC_DECL__(dneupd)(&RVEC, &HOWMNY[0], &SELECT[0], &DR[0], &DI[0],
		&Z[0], &LDZ, &SIGMAR, &SIGMAI, &WORKEV[0],
		&BMAT[0], &N, &WHICH[0], &NEV,
		&TOL, &RESID[0], &NCV, &V[0], &LDV, &IPARAM[0], &IPNTR[0],
		&WORKD[0], &WORKL[0], &LWORKL, &INFO);

	int nconv = IPARAM[4];
	if (nconv > 0) {
		ASSERT(nconv <= NCV);
		MyVectorHandler AlphaR(nconv, DR);
		MyVectorHandler AlphaI(nconv, DI);
		std::vector<bool> vOut(nconv);
		output_eigenvalues(0, AlphaR, AlphaI, 1., pDM, pEA, 1, nconv, vOut);

		if (pEA->uFlags & Solver::EigenAnalysis::EIG_OUTPUT_GEOMETRY) {
			pDM->OutputEigGeometry(uCurr, pEA->iResultsPrecision);
		}

		if (pEA->uFlags & Solver::EigenAnalysis::EIG_OUTPUT_EIGENVECTORS) {
			std::vector<doublereal *> ZC(nconv);
			FullMatrixHandler VR(&Z[0], &ZC[0], N*nconv, N, nconv);
			pDM->OutputEigenvectors(0, AlphaR, AlphaI, 1.,
				0, VR, vOut, uCurr, pEA->iResultsPrecision);
		}

	} else {
		if (bNewLine && silent_err) {
			silent_cerr(std::endl);
			bNewLine = false;
		}
		silent_cerr("no converged Ritz coefficients" << std::endl);
	}
}
#endif // USE_ARPACK

#ifdef USE_JDQZ
// Computes eigenvalues and eigenvectors using ARPACK's
// canonical non-symmetric eigenanalysis
static void
eig_jdqz(const MatrixHandler *pMatA, const MatrixHandler *pMatB,
	DataManager *pDM, Solver::EigenAnalysis *pEA,
	bool bNewLine, const unsigned uCurr)
{
	const NaiveMatrixHandler& MatA = dynamic_cast<const NaiveMatrixHandler &>(*pMatA);
	const NaiveMatrixHandler& MatB = dynamic_cast<const NaiveMatrixHandler &>(*pMatB);

	MBJDQZ mbjdqz(MatA, MatB);
	mbjdqzp = &mbjdqz;

	/*

alpha, beta Obvious from equation (1)
wanted      Compute the converged eigenvectors (if wanted =
            .true.)
eivec       Converged eigenvectors if wanted = .true., else con-
            verged Schur vectors
n           The size of the problem
target      The value near which the eigenvalues are sought
eps         Tolerance of the eigensolutions, Ax-Bx /|/| <
kmax        Number of wanted eigensolutions, on output: number of
            converged eigenpairs
jmax        Maximum size of the search space
jmin        Minimum size of the search space
method      Linear equation solver:
   1:       GMRESm , [2]
   2:       BiCGstab(), [3]
m           Maximum dimension of searchspace of GMRESm
l           Degree of GMRES-polynomial in Bi-CGstab()
mxmv        Maximum number of matrix-vector multiplications in
            GMRESm or BiCGstab()
maxstep     Maximum number of Jacobi-Davidson iterations
lock        Tracking parameter (section 2.5.1)
order       Selection criterion for Ritz values:
   0:       nearest to target
   -1:      smallest real part
   1:       largest real part
   -2:      smallest imaginary part
   2:       largest imaginary part
testspace   Determines how to expand the testspace W
   1:       w = "Standard Petrov" ×v (Section 3.1.1)
   2:       w = "Standard 'variable' Petrov" ×v (Section 3.1.2)
   3:       w = "Harmonic Petrov" ×v (Section 3.5.1)
zwork       Workspace
lwork       Size of workspace, >= 4+m+5jmax+3kmax if GMRESm
            is used, >= 10 + 6 + 5jmax + 3kmax if Bi-CGstab() is
            used.

*/

	std::vector<doublecomplex> alpha;
	std::vector<doublecomplex> beta;
	std::vector<doublecomplex> eivec;
	logical wanted = 1;
	integer n = pMatA->iGetNumRows();
	doublecomplex target = { 1., 0. };
	doublereal eps = pEA->jdqz.eps;
	integer kmax = pEA->jdqz.kmax;
	integer jmax = pEA->jdqz.jmax;
	integer jmin = pEA->jdqz.jmin;
	integer method = pEA->jdqz.method;
	integer m = pEA->jdqz.m;
	integer l = pEA->jdqz.l;
	integer mxmv = pEA->jdqz.mxmv;
	integer maxstep = pEA->jdqz.maxstep;
	doublereal lock = pEA->jdqz.lock;
	integer order = pEA->jdqz.order;
	integer testspace = pEA->jdqz.testspace;
	std::vector<doublecomplex> zwork;
	integer lwork;

	switch (method) {
	case Solver::EigenAnalysis::JDQZ::GMRES:
		lwork = 4 + m + 5*jmax + 3*kmax;
		break;

	case Solver::EigenAnalysis::JDQZ::BICGSTAB:
		lwork = 10 + 6*l + 5*jmax + 3*kmax;
		break;

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	alpha.resize(jmax);
	beta.resize(jmax);
	eivec.resize(n*kmax);
	zwork.resize(n*lwork);

	__FC_DECL__(jdqz)(
		&alpha[0],
		&beta[0],
		&eivec[0],
		&wanted,
		&n,
		&target,
		&eps,
		&kmax,
		&jmax,
		&jmin,
		&method,
		&m,
		&l,
		&mxmv,
		&maxstep,
		&lock,
		&order,
		&testspace,
		&zwork[0],
		&lwork);

	if (bNewLine && silent_err) {
		silent_cerr(std::endl);
		bNewLine = false;
	}
	silent_cerr("\r" "cnt=" << mbjdqz.Cnt() << std::endl);

	if (kmax > 0) {
		int nconv = kmax;
		MyVectorHandler AlphaR(nconv);
		MyVectorHandler AlphaI(nconv);
		MyVectorHandler Beta(nconv);
		for (integer c = 0; c < nconv; c++) {
			if (beta[c].i != 0.) {
				doublereal d = std::sqrt(beta[c].r*beta[c].r + beta[c].i*beta[c].i);
				Beta(c + 1) = d;
				AlphaR(c + 1) = (alpha[c].r*beta[c].r + alpha[c].i*beta[c].i)/d;
				AlphaI(c + 1) = (alpha[c].i*beta[c].r - alpha[c].r*beta[c].i)/d;

			} else {
				Beta(c + 1) = beta[c].r;
				AlphaR(c + 1) = alpha[c].r;
				AlphaI(c + 1) = alpha[c].i;
			}
		}
		std::vector<bool> vOut(nconv);
		output_eigenvalues(0, AlphaR, AlphaI, 1., pDM, pEA, 1, nconv, vOut);

		if (pEA->uFlags & Solver::EigenAnalysis::EIG_OUTPUT_GEOMETRY) {
			pDM->OutputEigGeometry(uCurr, pEA->iResultsPrecision);
		}

		if (pEA->uFlags & Solver::EigenAnalysis::EIG_OUTPUT_EIGENVECTORS) {
			FullMatrixHandler VR(n, nconv);
			doublecomplex *p = &eivec[0] - 1;
			for (integer c = 1; c <= nconv; c++) {

				if (AlphaI(c) == 0.) {
					for (integer r = 1; r <= n; r++) {
						VR(r, c) = p[r].r;
					}

				} else {
					for (integer r = 1; r <= n; r++) {
						VR(r, c) = p[r].r;
						VR(r, c + 1) = p[n + r].i;
					}

					p += n;
					c++;
				}

				p += n;
			}

			pDM->OutputEigenvectors(&Beta, AlphaR, AlphaI, 1.,
				0, VR, vOut, uCurr, pEA->iResultsPrecision);
		}

	} else {
		if (bNewLine && silent_err) {
			silent_cerr(std::endl);
			bNewLine = false;
		}
		silent_cerr("no converged eigenpairs" << std::endl);
	}
}
#endif // USE_JDQZ

bool Solver::EigNext(bool (*pfnConditionTime)(doublereal, doublereal), bool bNewLine)
{
     ASSERT(!EigAn.bAnalysis || (EigAn.currAnalysis >= EigAn.Analyses.begin() && EigAn.currAnalysis <= EigAn.Analyses.end()));
     ASSERT(EigAn.currAnalysisIndex <= EigAn.Analyses.size());
     
     if (!(EigAn.bAnalysis &&
           EigAn.currAnalysis != EigAn.Analyses.end() &&
           (*pfnConditionTime)(*EigAn.currAnalysis, dTime))) {
          return false;
     }

     ASSERT(EigAn.currAnalysis >= EigAn.Analyses.begin() && EigAn.currAnalysis < EigAn.Analyses.end());

     Eig(bNewLine);
     
     do {
          ++EigAn.currAnalysis;
          // Because it would be pointless to execute exactly the same eigenanalysis several times,
          // all the the remaining eigenanalyses will be ignored.
     } while (EigAn.currAnalysis != EigAn.Analyses.end() && (*pfnConditionTime)(*EigAn.currAnalysis, dTime));
     
     ++EigAn.currAnalysisIndex;
     
     ASSERT(EigAn.currAnalysis >= EigAn.Analyses.begin() && EigAn.currAnalysis <= EigAn.Analyses.end());
     ASSERT(EigAn.currAnalysisIndex <= EigAn.Analyses.size());
     
     return true;
}

int Solver::EigAll(bool (*pfnConditionTime)(doublereal, doublereal), bool bNewLine)
{
     int iCnt = 0;

     while (EigNext(pfnConditionTime, bNewLine)) {
          ++iCnt;
     }

     return iCnt;
}

// Driver for eigenanalysis
void
Solver::Eig(bool bNewLine)
{
	DEBUGCOUTFNAME("Solver::Eig");

	/*
	 * MatA, MatB: MatrixHandlers to eigenanalysis matrices
	 * MatVL, MatVR: MatrixHandlers to eigenvectors, if required
	 * AlphaR, AlphaI Beta: eigenvalues
	 * WorkVec:    Workspace
	 * iWorkSize:  Size of the workspace
	 */

	DEBUGCOUT("Solver::Eig(): performing eigenanalysis" << std::endl);

	integer iSize = iNumDofs;
	
        SolutionManager *pSM = nullptr;
	MatrixHandler *pMatA = nullptr;
	MatrixHandler *pMatB = nullptr;

	if (EigAn.uFlags & EigenAnalysis::EIG_USE_LAPACK) {
		SAFENEWWITHCONSTRUCTOR(pMatA, FullMatrixHandler,
			FullMatrixHandler(iSize));
		SAFENEWWITHCONSTRUCTOR(pMatB, FullMatrixHandler,
			FullMatrixHandler(iSize));

	} else if (EigAn.uFlags & EigenAnalysis::EIG_USE_ARPACK) {
                const integer iStates = pRegularSteps->GetIntegratorNumUnknownStates();
                const integer iWorkSpaceSize = CurrLinearSolver.iGetWorkSpaceSize();
                integer iLWS = iWorkSpaceSize;
                integer iNLD = iNumDofs*iStates;
                
                if (bParallel) {
                        iLWS = iWorkSpaceSize*iNumLocDofs/(iNumDofs*iNumDofs);
                        iNLD = iNumLocDofs*iStates;
                }

                pSM = AllocateSolman(iNLD, iLWS);                
		pMatB = pSM->pMatHdl();
                pMatA = pMatB->Copy();

	} else if (EigAn.uFlags & EigenAnalysis::EIG_USE_JDQZ) {
		SAFENEWWITHCONSTRUCTOR(pMatA, NaiveMatrixHandler,
			NaiveMatrixHandler(iSize));
		SAFENEWWITHCONSTRUCTOR(pMatB, NaiveMatrixHandler,
			NaiveMatrixHandler(iSize));

	} else if (EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_SPARSE_MATRICES) {
		SAFENEWWITHCONSTRUCTOR(pMatA, SpMapMatrixHandler,
			SpMapMatrixHandler(iSize));
		SAFENEWWITHCONSTRUCTOR(pMatB, SpMapMatrixHandler,
			SpMapMatrixHandler(iSize));

	} else if (EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_FULL_MATRICES) {
		SAFENEWWITHCONSTRUCTOR(pMatA, FullMatrixHandler,
			FullMatrixHandler(iSize));
		SAFENEWWITHCONSTRUCTOR(pMatB, FullMatrixHandler,
			FullMatrixHandler(iSize));
	}
        
	// Matrices assembly (see eig.ps)
	const doublereal h = EigAn.dParam;

        /*
         * Call AssRes before AssJac in order to be sure that all
         * the elements have updated the internal data
         */
        if (pMatA && pMatB) // Suppress null pointer warnings reported by clang analyzer
        {
             MyVectorHandler Res(iNumDofs);

             StepIntegratorGuard oRestoreCurrentStepIntegrator{*this};

             pCurrStepIntegrator = &oFakeStepIntegrator; // Needed for hybrid step integrator only

             pDM->Update();
             Res.Reset();
             oFakeStepIntegrator.SetCoef(-h/2.);
             pDM->AssRes(Res, -h/2.);
             pMatA->Reset();
             pDM->AssJac(*pMatA, -h/2.);
             pMatA->PacMat(); // Needed for Trilinos sparse matrix handler

             pDM->Update();
             Res.Reset();
             oFakeStepIntegrator.SetCoef(h/2.);
             pDM->AssRes(Res, h/2.);
             pMatB->Reset();
             pDM->AssJac(*pMatB, h/2.);
             pMatB->PacMat(); // Needed for Trilinos sparse matrix handler
        }

#ifdef DEBUG
	DEBUGCOUT(std::endl
		<< "Matrix A:" << std::endl << *pMatA << std::endl
		<< "Matrix B:" << std::endl << *pMatB << std::endl);
#endif /* DEBUG */

	unsigned uCurr = EigAn.currAnalysisIndex;
	if (EigAn.uFlags & EigenAnalysis::EIG_OUTPUT) {
		unsigned uSize = EigAn.Analyses.size();
		if (uSize >= 1) {
			std::stringstream postfix_ss;

			postfix_ss << '_';

			if (EigAn.iFNameWidth > 0) {
				postfix_ss << std::setw(EigAn.iFNameWidth) << std::setfill('0') << uCurr;

			} else if (!EigAn.iFNameFormat.empty()) {
				char buf[BUFSIZ];

				snprintf(buf, sizeof(buf), EigAn.iFNameFormat.c_str(), (int)uCurr);
				postfix_ss << buf;

			} else {
				postfix_ss << uCurr;
			}
			
			pDM->OutputEigOpen(postfix_ss.str());
			pDM->OutputEigParams(dTime, h/2., uCurr, EigAn.iResultsPrecision);
		}
	}
	if (EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_FULL_MATRICES) {
                pDM->OutputEigFullMatrices(pMatA, pMatB, uCurr, EigAn.iMatrixPrecision);
	}
	else if (EigAn.uFlags & EigenAnalysis::EIG_OUTPUT_SPARSE_MATRICES) {
                pDM->OutputEigSparseMatrices(pMatA, pMatB, uCurr, EigAn.iMatrixPrecision);
	}

	switch (EigAn.uFlags & EigenAnalysis::EIG_USE_MASK) {
#ifdef USE_LAPACK
	case EigenAnalysis::EIG_USE_LAPACK:
		eig_lapack(pMatA, pMatB, pDM, &EigAn, bNewLine, uCurr);
		break;
#endif // USE_LAPACK

#ifdef USE_ARPACK
	case EigenAnalysis::EIG_USE_ARPACK:
		eig_arpack(pMatA, pSM, pDM, &EigAn, bNewLine, uCurr);
		break;
#endif // USE_ARPACK

#ifdef USE_JDQZ
	case EigenAnalysis::EIG_USE_JDQZ:
		eig_jdqz(pMatA, pMatB, pDM, &EigAn, bNewLine, uCurr);
		break;
#endif // USE_JDQZ
        case EigenAnalysis::EIG_USE_EXTERNAL:
                if (EigAn.uFlags & Solver::EigenAnalysis::EIG_OUTPUT_GEOMETRY) {
                    pDM->OutputEigGeometry(uCurr, EigAn.iResultsPrecision);
                }
                break;
	default:
		// only output matrices, use external eigenanalysis
		break;
	}

	pDM->OutputEigClose();

	if (pSM) {
                pMatB = nullptr; // pMatB will be deleted when deleting pSM
		SAFEDELETE(pSM);
	}

	if (pMatA) {
		SAFEDELETE(pMatA);
	}

	if (pMatB) {
		SAFEDELETE(pMatB);
	}
}

doublereal Solver::dGetInitialMaxTimeStep() const
{
// 	if (typeid(*MaxTimeStep.pGetDriveCaller()) == typeid(PostponedDriveCaller)) {
// 		return ::dDefaultMaxTimeStep;
// 	}
	{
		auto ts_drv = MaxTimeStep.pGetDriveCaller();
		if (dynamic_cast<PostponedDriveCaller*>(ts_drv) != 0) {
			return ::dDefaultMaxTimeStep;
		}
	}

	// The same behavior like in previous releases
	return MaxTimeStep.dGet();
}

SolutionManager *const
Solver::AllocateSolman(integer iNLD, integer iLWS)
{
        SolutionManager *pCurrSM = CurrLinearSolver.GetSolutionManager(iNLD,
#ifdef USE_MPI
                                                                       MBDynComm,
#endif
                                                                       iLWS);

	/* special extra parameters if required */
	switch (CurrLinearSolver.GetSolver()) {
	case LinSol::UMFPACK_SOLVER:
#ifdef HAVE_UMFPACK_TIC_DISABLE
		if (pRTSolver) {
			/* disable profiling, to avoid times() system call
			 *
			 * This function has been introduced in Umfpack 4.1
			 * by our patch at
			 *
			 * http://mbdyn.aero.polimi.it/~masarati/Download/\
			 * 	mbdyn/umfpack-4.1-nosyscalls.patch
			 *
			 * but since Umfpack 4.3 is no longer required,
			 * provided the library is compiled with -DNO_TIMER
			 * to disable run-time syscalls to timing routines.
			 */
			umfpack_tic_disable();
		}
#endif // HAVE_UMFPACK_TIC_DISABLE
		break;

	default:
		break;
	}

	return pCurrSM;
};


SolutionManager *const
Solver::AllocateSchurSolman(integer iStates)
{
	SolutionManager *pSSM(0);

#ifdef USE_SCHUR
	switch (CurrIntSolver.GetSolver()) {
	case LinSol::LAPACK_SOLVER:
	case LinSol::NAIVE_SOLVER:
	case LinSol::UMFPACK_SOLVER:
	case LinSol::Y12_SOLVER:
		break;

	default:
		silent_cerr("apparently solver "
				<< CurrIntSolver.GetSolverName()
				<< " is not allowed as interface solver "
				"for SchurSolutionManager" << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	SAFENEWWITHCONSTRUCTOR(pSSM,
			SchurSolutionManager,
			SchurSolutionManager(iNumDofs, iStates, pLocDofs,
				iNumLocDofs,
				pIntDofs, iNumIntDofs,
				pLocalSM, CurrIntSolver));

#else /* !USE_SCHUR */
	silent_cerr("Configure --with-mpi to enable Schur solver" << std::endl);
	throw ErrGeneric(MBDYN_EXCEPT_ARGS);
#endif /* !USE_SCHUR */

	return pSSM;
};

NonlinearSolver *const
Solver::AllocateNonlinearSolver()
{
	NonlinearSolver *pNLS = 0;

	switch (NonlinearSolverType) {
	case NonlinearSolver::MATRIXFREE:
		switch (MFSolverType) {
		case MatrixFreeSolver::BICGSTAB:
			SAFENEWWITHCONSTRUCTOR(pNLS,
					BiCGStab,
					BiCGStab(PcType,
						iPrecondSteps,
						dIterTol,
						iIterativeMaxSteps,
						dIterertiveEtaMax,
						dIterertiveTau,
						*this));
			break;

		default:
			pedantic_cout("unknown matrix free solver type; "
					"using default" << std::endl);
			/* warning: should be unreachable */

		case MatrixFreeSolver::GMRES:
			SAFENEWWITHCONSTRUCTOR(pNLS,
					Gmres,
					Gmres(PcType,
						iPrecondSteps,
						dIterTol,
						iIterativeMaxSteps,
						dIterertiveEtaMax,
						dIterertiveTau,
						*this));
			break;
		}
		break;

	default:
		pedantic_cout("unknown nonlinear solver type; using default"
				<< std::endl);

	case NonlinearSolver::NEWTONRAPHSON:
		SAFENEWWITHCONSTRUCTOR(pNLS,
				NewtonRaphsonSolver,
				NewtonRaphsonSolver(bTrueNewtonRaphson,
                                        oLineSearchParam.bKeepJacAcrossSteps,
                                        oLineSearchParam.iIterationsBeforeAssembly,
					*this));
		break;
	case NonlinearSolver::LINESEARCH:
            switch (CurrLinearSolver.GetSolver()) {
            case LinSol::UMFPACK_SOLVER:
            case LinSol::KLU_SOLVER:
            case LinSol::PASTIX_SOLVER:
            case LinSol::PARDISO_SOLVER:
                break;
                
            default:
                if (!bTrueNewtonRaphson) {
                    pedantic_cout("warning: linear solver \""
                                  << CurrLinearSolver.GetSolverName()
                                  << "\" not supported by modified line search\n");
                }
                
                bTrueNewtonRaphson = true;
            }
            
            if (bTrueNewtonRaphson) {
		SAFENEWWITHCONSTRUCTOR(pNLS,
				LineSearchFull,
				LineSearchFull(pDM, 
                                 *this,
                                 oLineSearchParam));
            } else {                
                SAFENEWWITHCONSTRUCTOR(pNLS,
                                       LineSearchModified,
                                       LineSearchModified(pDM, 
                                                          *this,
                                                          oLineSearchParam));
            }
		break;
        case NonlinearSolver::BFGS:
                switch (CurrLinearSolver.GetSolver()) {
                case LinSol::QR_SOLVER:
                case LinSol::SPQR_SOLVER:
                        SAFENEWWITHCONSTRUCTOR(pNLS,
                                               LineSearchBFGS,
                                               LineSearchBFGS(pDM,
                                                              *this,
                                                              oLineSearchParam));
                        break;
                default:
                        silent_cerr("nonlinear solver \"bfgs\" can be used only with linear solver \"qr\" or \"spqr\"\n");
                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                }            
                break;
        case NonlinearSolver::MCP_NEWTON_FB:
                SAFENEWWITHCONSTRUCTOR(pNLS,
                                       MCPNewtonFB,
                                       MCPNewtonFB(pDM, *this, oLineSearchParam));
                break;
        case NonlinearSolver::MCP_NEWTON_MIN_FB:
                if (!pDM->bUseAutoDiff()) {
                        silent_cerr("nonlinear solver \"mcp newton min fb\" requires support for automatic differentiation which was not enabled!\n"
                                    "add the statement \"use automatic differentiation;\" inside the control data section to enable it!\n");
                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                }
                
                SAFENEWWITHCONSTRUCTOR(pNLS,
                                       MCPNewtonMinFB,
                                       MCPNewtonMinFB(pDM, *this, oLineSearchParam));
                break;
#ifdef USE_TRILINOS
        case NonlinearSolver::NOX: {
                // FIXME: Since access to the Jacobian matrix through the Trilinos library is not under our control,
                // FIXME: we have to ensure that the content of the matrix is not destroyed by the linear solver.
                constexpr unsigned uMaskCCDir = LinSol::SOLVER_FLAGS_ALLOWS_CC | LinSol::SOLVER_FLAGS_ALLOWS_DIR;
                unsigned uSolFlags = CurrLinearSolver.GetSolverFlags();
                const bool bCompactSpmh = (uSolFlags & uMaskCCDir) != 0u;
                const bool bJacOpNewton = (oNoxSolverParam.uFlags & NoxSolverParameters::JACOBIAN_NEWTON) != 0u;
                bool bAllowCompactSpmh = true;

                switch (CurrLinearSolver.GetSolver()) {
                case LinSol::UMFPACK_SOLVER:
                case LinSol::KLU_SOLVER:
                case LinSol::PARDISO_SOLVER:
                case LinSol::PARDISO_64_SOLVER:
                case LinSol::PASTIX_SOLVER:
                case LinSol::SPQR_SOLVER:
                case LinSol::STRUMPACK_SOLVER:
                case LinSol::AZTECOO_SOLVER:
                case LinSol::AMESOS_SOLVER:
                case LinSol::SICONOS_SPARSE_SOLVER:
                        // All linear solvers which do not destroy the Jacobian during factorization should be added here.
                        break;
                case LinSol::Y12_SOLVER:
                        // Y12 will destroy the Jacobian only if a "dir" or "cc" matrix handler is used.
                        if (bJacOpNewton) {
                                bAllowCompactSpmh = false;
                        }
                        break;
                default:
                        if (bJacOpNewton) {
                                silent_cerr("Nonlinear solver \"nox\" cannot "
                                            "be used with linear solver \""
                                            << CurrLinearSolver.GetSolverName()
                                            << "\" unless the \"newton krylov\" option is used!\n");
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }
                }

                const SolutionManager::ScaleOpt& oScale = CurrLinearSolver.GetScale();
                const bool bScaleEnabled = oScale.when != SolutionManager::SCALEW_NEVER && oScale.algorithm != SolutionManager::SCALEA_NONE;

                if (bScaleEnabled) {
                        if (bCompactSpmh) {
                                silent_cerr("Warning: Nonlinear solver \"nox\" cannot "
                                            "be used with compact sparse matrix handlers "
                                            "\"cc\" or \"dir\" and matrix scaling enabled at the same time!\n"
                                            "Warning: using matrix handler \"map\" instead\n");
                        }
                        bAllowCompactSpmh = false;
                }

                if (!bAllowCompactSpmh && bCompactSpmh) {
                        silent_cerr("Warning: Nonlinear solver \"nox\" cannot "
                                    "be used with linear solver \""
                                    << CurrLinearSolver.GetSolverName()
                                    << "\" and \"cc\" or \"dir\" matrix handlers enabled, "
                                    << "unless the \"newton krylov\" option is used!\n");
                        uSolFlags = (uSolFlags & ~uMaskCCDir) | LinSol::SOLVER_FLAGS_ALLOWS_MAP;
                }

                CurrLinearSolver.SetSolverFlags(uSolFlags);
                pNLS = pAllocateNoxNonlinearSolver(*this, oNoxSolverParam);
        } break;
#endif
#ifdef USE_SICONOS
        case NonlinearSolver::SICONOS_MCP_NEWTON_FB:
                SAFENEWWITHCONSTRUCTOR(pNLS,
                                       SiconosMCPNewton,
                                       SiconosMCPNewton(*this, oLineSearchParam));
                break;
        case NonlinearSolver::SICONOS_MCP_NEWTON_MIN_FB:
                SAFENEWWITHCONSTRUCTOR(pNLS,
                                       SiconosMCPNewtonMin,
                                       SiconosMCPNewtonMin(*this, oLineSearchParam));
                break;                
#endif
	}
	return pNLS;
}

void
Solver::SetupSolmans(integer iStates, bool bCanBeParallel)
{
	DEBUGLCOUT(MYDEBUG_MEM, "creating SolutionManager\n\tsize = "
		<< iNumDofs*iUnkStates <<
		"\n\tnumdofs = " << iNumDofs
		<< "\n\tnumstates = " << iStates << std::endl);

	/* delete previous solmans */
	if (pSM != 0) {
		SAFEDELETE(pSM);
		pSM = 0;
	}
	if (pLocalSM != 0) {
		SAFEDELETE(pLocalSM);
		pLocalSM = 0;
	}

	integer iWorkSpaceSize = CurrLinearSolver.iGetWorkSpaceSize();
	integer iLWS = iWorkSpaceSize;
	integer iNLD = iNumDofs*iStates;
	if (bCanBeParallel && bParallel) {
		/* FIXME BEPPE! */
		iLWS = iWorkSpaceSize*iNumLocDofs/(iNumDofs*iNumDofs);
		/* FIXME: GIUSTO QUESTO? */
		iNLD = iNumLocDofs*iStates;
	}

	SolutionManager *pCurrSM = AllocateSolman(iNLD, iLWS);

	/*
	 * This is the LOCAL solver if instantiating a parallel
	 * integrator; otherwise it is the MAIN solver
	 */
	if (bCanBeParallel && bParallel) {
		pLocalSM = pCurrSM;

		/* Crea il solutore di Schur globale */
		pSM = AllocateSchurSolman(iStates);

	} else {
		pSM = pCurrSM;
	}
	/*
	 * FIXME: at present there MUST be a pSM
	 * (even for matrix-free nonlinear solvers)
	 */
	if (pSM == 0) {
		silent_cerr("No linear solver defined" << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

clock_t
Solver::GetCPUTime(void) const
{
	return pDM->GetCPUTime();
}

void
Solver::PrintResidual(const VectorHandler& Res, integer iIterCnt) const
{
	pDM->PrintResidual(Res, iIterCnt);
}

void
Solver::PrintSolution(const VectorHandler& Sol, integer iIterCnt) const
{
	pDM->PrintSolution(Sol, iIterCnt);
}

void Solver::CheckTimeStepLimit(doublereal dErr, doublereal dErrDiff) const /*throw(NonlinearSolver::MaxResidualExceeded, NonlinearSolver::TimeStepLimitExceeded)*/
{
	if (pDerivativeSteps) {
		// Time step cannot be reduced
		return;
	}

	if (dErr > dMaxResidual) {
            if (dCurrTimeStep > 2 * dMinTimeStep) {
		if (outputIters()) {
#ifdef USE_MPI
			if (!bParallel || MBDynComm.Get_rank() == 0)
#endif /* USE_MPI */
			{
				silent_cerr("warning: current residual = " << dErr
						<< " > maximum residual = " << dMaxResidual
						<< std::endl);
			}
		}

		throw NonlinearSolver::MaxResidualExceeded(MBDYN_EXCEPT_ARGS);
	}
	}

	if (dErrDiff > dMaxResidualDiff) {
            if (dCurrTimeStep > 2 * dMinTimeStep) {
		if (outputIters()) {
#ifdef USE_MPI
			if (!bParallel || MBDynComm.Get_rank() == 0)
#endif /* USE_MPI */
			{
				silent_cerr("warning: current residual for differential equations = " << dErrDiff
						<< " > maximum residual = " << dMaxResidualDiff
						<< std::endl);
			}
		}

		throw NonlinearSolver::MaxResidualExceeded(MBDYN_EXCEPT_ARGS);
	}
	}

	switch (eTimeStepLimit) {
	case TS_SOFT_LIMIT:
		break;

	case TS_HARD_LIMIT: {
                const doublereal dMaxTS = MaxTimeStep.dGet() * (1 + sqrt(std::numeric_limits<doublereal>::epsilon()));

		if (dCurrTimeStep > dMaxTS && dCurrTimeStep > dMinTimeStep) {
			if (outputIters()) {
#ifdef USE_MPI
				if (!bParallel || MBDynComm.Get_rank() == 0)
#endif /* USE_MPI */
				{
					silent_cerr("warning: current time step = "
						<< dCurrTimeStep
						<< " > hard limit of the maximum time step = "
						<< dMaxTS << std::endl);
				}
			}
			throw NonlinearSolver::TimeStepLimitExceeded(MBDYN_EXCEPT_ARGS);
		}
		} break;

	default:
		ASSERT(0);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}
