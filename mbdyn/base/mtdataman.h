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

/* gestore dei dati */

#ifndef MTDATAMAN_H
#define MTDATAMAN_H

#ifdef USE_MULTITHREAD

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <semaphore>
#include <thread>

#include "dataman.h"
#include "spmh.h"

#ifdef USE_NAIVE_MULTITHREAD
#include <atomic_ops.h>
#include "naivemh.h"
#endif

#include "sp_gradient_spmh.h"

class Solver;

/* MultiThreadDataManager - begin */

class MultiThreadDataManager : public DataManager {
protected:
        // nThreads is now in DataManager
        enum {
                ASS_UNKNOWN = -1,

                ASS_CC,			/* use native column-compressed form */
#ifdef USE_NAIVE_MULTITHREAD
                ASS_NAIVE,		/* use native H-P sparse solver */
#endif
                ASS_GRAD,
                ASS_GRAD_PROD,

                ASS_DEFAULT,
                ASS_LAST
        } AssMode;

        /* steps of CC computation */
        enum {
                CC_NO,
                CC_FIRST,
                CC_YES
        } CCReady;

        /* per-thread specific data */
        struct ThreadData {
                MultiThreadDataManager *pDM;
                integer threadNumber;
                integer iCPUIndex;
                std::thread thread;
                std::counting_semaphore<> sem{0};
                std::exception_ptr except;
                mutable MT_VecIter<Elem *> ElemIter;

                VariableSubMatrixHandler *pWorkMatA;	/* Working SubMatrix */
                VariableSubMatrixHandler *pWorkMatB;
                VariableSubMatrixHandler *pWorkMat;	/* same as pWorkMatA */
#ifdef MBDYN_X_MT_ASSRES
                MySubVectorHandler *pWorkVec;
#endif
                /* for CC assembly */
                CompactSparseMatrixHandler* pJacHdl;
#ifdef USE_NAIVE_MULTITHREAD
                /* for Naive assembly */
                NaiveMatrixHandler** ppNaiveJacHdl;
#endif
                SpGradientSparseMatrixWrapper oGradJacHdl;
                const VectorHandler* pY;
                VectorHandler* pJacProd;
#ifdef USE_NAIVE_MULTITHREAD
                AO_TS_t* lock;
#endif
#ifdef MBDYN_X_MT_ASSRES
                VectorHandler* pResHdl;
                VectorHandler* pAbsResHdl;
#endif
                MatrixHandler* pMatA;
                MatrixHandler* pMatB;
                doublereal dCoef;
        } *thread_data;

        enum DataManagerOp {
                OP_UNKNOWN = -1,

                OP_ASSJAC_CC,
#ifdef USE_NAIVE_MULTITHREAD
                OP_ASSJAC_NAIVE,
                OP_SUM_NAIVE,
#endif
                OP_ASSJAC_GRAD,
                OP_ASSJAC_PROD,

                /* used only #ifdef MBDYN_X_MT_ASSRES */
                OP_ASSRES,

                /* not used yet */
                OP_ASSMATS,
                OP_BEFOREPREDICT,
                OP_AFTERPREDICT,
                OP_AFTERCONVERGENCE,
                /* end of not used yet */

                OP_EXIT,

                LAST_OP
        } op;

        /* will be replaced by barriers ... */
        unsigned thread_count;

        /* this can be replaced by a barrier ... */
        std::mutex thread_mutex;
        std::condition_variable	thread_cond;

        /* this is used to propagate ErrMatrixRebuild ... */
        std::atomic<bool>	propagate_ErrMatrixRebuild;

        void EndOfOp(void);

        /* thread function */
        static void thread(ThreadData *arg);
        static void thread_cleanup(ThreadData *arg);

        /* starts the helper threads */
        void ThreadSpawn(void);
        void ThreadDestroy(void);

        /* specialized assembly */
        virtual void CCAssJac(MatrixHandler& JacHdl, doublereal dCoef);
#ifdef USE_NAIVE_MULTITHREAD
        virtual void NaiveAssJac(NaiveMatrixHandler& JacHdl, doublereal dCoef);
        virtual void NaiveAssJacInit(NaiveMatrixHandler& JacHdl, doublereal dCoef);
#endif
        void GradAssJac(SpGradientSparseMatrixHandler& JacHdl, doublereal dCoef);
        void GradAssJacProd(VectorHandler& JacY, const VectorHandler& Y, doublereal dCoef);
        virtual void AssJac(VectorHandler& JacY, const VectorHandler& Y, doublereal dCoef) override;

        static void SetAffinity(const ThreadData& oThread);
public:
        /* costruttore - legge i dati e costruisce le relative strutture */
        MultiThreadDataManager(MBDynParser& HP,
                        unsigned OF,
                        Solver* pS,
                        doublereal dInitialTime,
                        const char* sOutputFileName,
                        const char* sInputFileName,
                        bool bAbortAfterInput,
                        unsigned nt);

        /* distruttore */
        virtual ~MultiThreadDataManager(void);

        /* Assembla lo jacobiano */
        virtual void AssJac(MatrixHandler& JacHdl, doublereal dCoef) override;

#ifdef MBDYN_X_MT_ASSRES
        /* Assembla il residuo */
        virtual void AssRes(VectorHandler &ResHdl, doublereal dCoef, VectorHandler*const pAbsResHdl = 0)
                /*throw(ChangedEquationStructure)*/;
#endif /* MBDYN_X_MT_ASSRES */
};

/* MultiThreadDataManager - end */

#endif /* USE_MULTITHREAD */

#endif /* MTDATAMAN_H */
