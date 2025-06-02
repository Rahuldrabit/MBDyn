/* $Header: /var/cvs/mbdyn/mbdyn/mbdyn-1.0/tests/rtai/pendulum-control/pendulum_control_rtai/pendulum_control.h,v 1.3 2007/02/26 00:34:50 masarati Exp $ */
/*
 * pendulum_control.h
 *
 * Real-Time Workshop code generation for Simulink model "pendulum_control.mdl".
 *
 * Model Version                        : 1.83
 * Real-Time Workshop file version      : 5.0 $Date: 2007/02/26 00:34:50 $
 * Real-Time Workshop file generated on : Thu Nov 27 14:52:23 2003
 * TLC version                          : 5.0 (Jun 18 2002)
 * C source code generated on           : Thu Nov 27 14:52:23 2003
 */

#ifndef _RTW_HEADER_pendulum_control_h_
# define _RTW_HEADER_pendulum_control_h_

#include <limits.h>
#include "tmwtypes.h"
#ifndef _pendulum_control_COMMON_INCLUDES_
# define _pendulum_control_COMMON_INCLUDES_
#include <math.h>
#include <string.h>

#include "simstruc.h"
#include "simstruc_types.h"
#include "rt_logging.h"
#include "rtlibsrc.h"

#endif                                  /* _pendulum_control_COMMON_INCLUDES_ */

#include "pendulum_control_types.h"

#define MODEL_NAME                      pendulum_control
#define NSAMPLE_TIMES                   (1)                      /* Number of sample times */
#define NINPUTS                         (0)                      /* Number of model inputs */
#define NOUTPUTS                        (0)                      /* Number of model outputs */
#define NBLOCKIO                        (6)                      /* Number of data output port signals */
#define NUM_ZC_EVENTS                   (0)                      /* Number of zero-crossing events */

#ifndef NCSTATES
# define NCSTATES (0)                   /* Number of continuous states */
#elif NCSTATES != 0
# error Invalid specification of NCSTATES defined in compiler command
#endif

/* Intrinsic types */
#ifndef POINTER_T
# define POINTER_T
typedef void * pointer_T;
#endif

/* Block signals (auto storage) */
typedef struct _BlockIO {
  real_T S_Function1;                   /* '<Root>/S-Function1' */
  real_T S_Function2_o1;                /* '<Root>/S-Function2' */
  real_T S_Function2_o2;                /* '<Root>/S-Function2' */
  real_T Sum2;                          /* '<Root>/Sum2' */
  real_T Sum;                           /* '<Root>/Sum' */
  real_T Sum1;                          /* '<Root>/Sum1' */
} BlockIO;

/* Block states (auto storage) for system: '<Root>' */
typedef struct D_Work_tag {
  void *S_Function1_PWORK[2];           /* '<Root>/S-Function1' */
  void *S_Function2_PWORK;              /* '<Root>/S-Function2' */
  void *C_PWORK;                        /* '<Root>/C' */
  void *err_PWORK;                      /* '<Root>/err' */
  void *omega_PWORK;                    /* '<Root>/omega' */
  void *theta_PWORK;                    /* '<Root>/theta' */
  int_T S_Function1_IWORK[2];           /* '<Root>/S-Function1' */
  int_T S_Function2_IWORK;              /* '<Root>/S-Function2' */
} D_Work;

/* Parameters (auto storage) */
struct _Parameters {
  real_T S_Function1_P1_Size[2];        /* Computed Parameter: P1Size
                                         * '<Root>/S-Function1'
                                         */
  real_T S_Function1_P1[9];             /* Expression: '127.0.0.1'
                                         * '<Root>/S-Function1'
                                         */
  real_T S_Function1_P2_Size[2];        /* Computed Parameter: P2Size
                                         * '<Root>/S-Function1'
                                         */
  real_T S_Function1_P2[6];             /* Expression: 'mbdtsk'
                                         * '<Root>/S-Function1'
                                         */
  real_T S_Function1_P3_Size[2];        /* Computed Parameter: P3Size
                                         * '<Root>/S-Function1'
                                         */
  real_T S_Function1_P3[6];             /* Expression: 'MBXOUT'
                                         * '<Root>/S-Function1'
                                         */
  real_T S_Function1_P4_Size[2];        /* Computed Parameter: P4Size
                                         * '<Root>/S-Function1'
                                         */
  real_T S_Function1_P4;                /* Expression: 1
                                         * '<Root>/S-Function1'
                                         */
  real_T S_Function1_P5_Size[2];        /* Computed Parameter: P5Size
                                         * '<Root>/S-Function1'
                                         */
  real_T S_Function1_P5;                /* Expression: 0.001
                                         * '<Root>/S-Function1'
                                         */
  real_T S_Function2_P1_Size[2];        /* Computed Parameter: P1Size
                                         * '<Root>/S-Function2'
                                         */
  real_T S_Function2_P1[9];             /* Expression: '127.0.0.1'
                                         * '<Root>/S-Function2'
                                         */
  real_T S_Function2_P2_Size[2];        /* Computed Parameter: P2Size
                                         * '<Root>/S-Function2'
                                         */
  real_T S_Function2_P2[6];             /* Expression: 'mbdtsk'
                                         * '<Root>/S-Function2'
                                         */
  real_T S_Function2_P3_Size[2];        /* Computed Parameter: P3Size
                                         * '<Root>/S-Function2'
                                         */
  real_T S_Function2_P3[6];             /* Expression: 'MBXINN'
                                         * '<Root>/S-Function2'
                                         */
  real_T S_Function2_P4_Size[2];        /* Computed Parameter: P4Size
                                         * '<Root>/S-Function2'
                                         */
  real_T S_Function2_P4;                /* Expression: 2
                                         * '<Root>/S-Function2'
                                         */
  real_T S_Function2_P5_Size[2];        /* Computed Parameter: P5Size
                                         * '<Root>/S-Function2'
                                         */
  real_T S_Function2_P5;                /* Expression: 0.001
                                         * '<Root>/S-Function2'
                                         */
  real_T Constant1_Value;               /* Expression: pi
                                         * '<Root>/Constant1'
                                         */
  real_T Gain_Gain;                     /* Expression: m*g*l*10
                                         * '<Root>/Gain'
                                         */
  real_T Gain1_Gain;                    /* Expression: 20
                                         * '<Root>/Gain1'
                                         */
  real_T C_P1_Size[2];                  /* Computed Parameter: P1Size
                                         * '<Root>/C'
                                         */
  real_T C_P1;                          /* Expression: numch
                                         * '<Root>/C'
                                         */
  real_T C_P2_Size[2];                  /* Computed Parameter: P2Size
                                         * '<Root>/C'
                                         */
  real_T C_P2;                          /* Expression: ts
                                         * '<Root>/C'
                                         */
  real_T err_P1_Size[2];                /* Computed Parameter: P1Size
                                         * '<Root>/err'
                                         */
  real_T err_P1;                        /* Expression: numch
                                         * '<Root>/err'
                                         */
  real_T err_P2_Size[2];                /* Computed Parameter: P2Size
                                         * '<Root>/err'
                                         */
  real_T err_P2;                        /* Expression: ts
                                         * '<Root>/err'
                                         */
  real_T omega_P1_Size[2];              /* Computed Parameter: P1Size
                                         * '<Root>/omega'
                                         */
  real_T omega_P1;                      /* Expression: numch
                                         * '<Root>/omega'
                                         */
  real_T omega_P2_Size[2];              /* Computed Parameter: P2Size
                                         * '<Root>/omega'
                                         */
  real_T omega_P2;                      /* Expression: ts
                                         * '<Root>/omega'
                                         */
  real_T theta_P1_Size[2];              /* Computed Parameter: P1Size
                                         * '<Root>/theta'
                                         */
  real_T theta_P1;                      /* Expression: numch
                                         * '<Root>/theta'
                                         */
  real_T theta_P2_Size[2];              /* Computed Parameter: P2Size
                                         * '<Root>/theta'
                                         */
  real_T theta_P2;                      /* Expression: ts
                                         * '<Root>/theta'
                                         */
  real_T Constant_Value;                /* Expression: 0
                                         * '<Root>/Constant'
                                         */
};

/* Real-time Model Data Structure */
struct _rtModel_pendulum_control_Tag {
  const char *path;
  const char *modelName;
  struct SimStruct_tag * *childSfunctions;
  const char *errorStatus;
  SS_SimMode simMode;
  RTWLogInfo *rtwLogInfo;
  RTWExtModeInfo *extModeInfo;
  RTWSolverInfo *solverInfo;
  void *sfcnInfo;

  /*
   * ModelData:
   * The following substructure contains information regarding
   * the data used in the model.
   */
  struct {
    void *blockIO;
    const void *constBlockIO;
    real_T *defaultParam;
    ZCSigState *prevZCSigState;
    real_T *contStates;
    real_T *discStates;
    real_T *derivs;
    real_T *nonsampledZCs;
    void *inputs;
    void *outputs;
    boolean_T contStateDisabled;
    boolean_T zCCacheNeedsReset;
    boolean_T derivCacheNeedsReset;
    boolean_T blkStateChange;
  } ModelData;

  /*
   * Sizes:
   * The following substructure contains sizes information
   * for many of the model attributes such as inputs, outputs,
   * dwork, sample times, etc.
   */
  struct {
    uint32_T checksums[4];
    uint32_T options;
    int_T numContStates;
    int_T numU;
    int_T numY;
    int_T numSampTimes;
    int_T numBlocks;
    int_T numBlockIO;
    int_T numBlockPrms;
    int_T numDwork;
    int_T numSFcnPrms;
    int_T numSFcns;
    int_T numIports;
    int_T numOports;
    int_T numNonSampZCs;
    int_T sysDirFeedThru;
    int_T rtwGenSfcn;
  } Sizes;

  /*
   * SpecialInfo:
   * The following substructure contains special information
   * related to other components that are dependent on RTW.
   */
  struct {
    const void *mappingInfo;
    void *xpcData;
  } SpecialInfo;

  /*
   * Timing:
   * The following substructure contains information regarding
   * the timing information for the model.
   */
  struct {
    time_T *t;
    time_T tStart;
    time_T tFinal;
    time_T stepSize;
    time_T timeOfLastOutput;
    void *timingData;
    real_T *varNextHitTimesList;
    SimTimeStep simTimeStep;
    int_T *sampleHits;
    int_T *perTaskSampleHits;
    boolean_T stopRequestedFlag;
    time_T *sampleTimes;
    time_T *offsetTimes;
    int_T *sampleTimeTaskIDPtr;
  } Timing;

  /*
   * Work:
   * The following substructure contains information regarding
   * the work vectors in the model.
   */
  struct {
    struct _ssDWorkRecord *dwork;
  } Work;
};

/* Simulation structure */
extern rtModel_pendulum_control *const rtM_pendulum_control;

/* Macros for accessing real-time model data structure  */
#ifndef rtmGetBlkStateChangeFlag
# define rtmGetBlkStateChangeFlag(rtm) ((rtm)->ModelData.blkStateChange)
#endif

#ifndef rtmSetBlkStateChangeFlag
# define rtmSetBlkStateChangeFlag(rtm, val) ((rtm)->ModelData.blkStateChange = (val))
#endif

#ifndef rtmGetBlockIO
# define rtmGetBlockIO(rtm) ((rtm)->ModelData.blockIO)
#endif

#ifndef rtmSetBlockIO
# define rtmSetBlockIO(rtm, val) ((rtm)->ModelData.blockIO = (val))
#endif

#ifndef rtmGetCTaskTicks
# define rtmGetCTaskTicks(rtm) ((rtm)->Timing.cTaskTicks)
#endif

#ifndef rtmSetCTaskTicks
# define rtmSetCTaskTicks(rtm, val) ((rtm)->Timing.cTaskTicks = (val))
#endif

#ifndef rtmGetChecksums
# define rtmGetChecksums(rtm) ((rtm)->Sizes.checksums)
#endif

#ifndef rtmSetChecksums
# define rtmSetChecksums(rtm, val) ((rtm)->Sizes.checksums = (val))
#endif

#ifndef rtmGetClockTick
# define rtmGetClockTick(rtm) ((rtm)->Timing.clockTick)
#endif

#ifndef rtmSetClockTick
# define rtmSetClockTick(rtm, val) ((rtm)->Timing.clockTick = (val))
#endif

#ifndef rtmGetConstBlockIO
# define rtmGetConstBlockIO(rtm) ((rtm)->ModelData.constBlockIO)
#endif

#ifndef rtmSetConstBlockIO
# define rtmSetConstBlockIO(rtm, val) ((rtm)->ModelData.constBlockIO = (val))
#endif

#ifndef rtmGetContStateDisabled
# define rtmGetContStateDisabled(rtm) ((rtm)->ModelData.contStateDisabled)
#endif

#ifndef rtmSetContStateDisabled
# define rtmSetContStateDisabled(rtm, val) ((rtm)->ModelData.contStateDisabled = (val))
#endif

#ifndef rtmGetContStates
# define rtmGetContStates(rtm) ((rtm)->ModelData.contStates)
#endif

#ifndef rtmSetContStates
# define rtmSetContStates(rtm, val) ((rtm)->ModelData.contStates = (val))
#endif

#ifndef rtmGetDefaultParam
# define rtmGetDefaultParam(rtm) ((rtm)->ModelData.defaultParam)
#endif

#ifndef rtmSetDefaultParam
# define rtmSetDefaultParam(rtm, val) ((rtm)->ModelData.defaultParam = (val))
#endif

#ifndef rtmGetDerivCacheNeedsReset
# define rtmGetDerivCacheNeedsReset(rtm) ((rtm)->ModelData.derivCacheNeedsReset)
#endif

#ifndef rtmSetDerivCacheNeedsReset
# define rtmSetDerivCacheNeedsReset(rtm, val) ((rtm)->ModelData.derivCacheNeedsReset = (val))
#endif

#ifndef rtmGetDirectFeedThrough
# define rtmGetDirectFeedThrough(rtm) ((rtm)->Sizes.sysDirFeedThru)
#endif

#ifndef rtmSetDirectFeedThrough
# define rtmSetDirectFeedThrough(rtm, val) ((rtm)->Sizes.sysDirFeedThru = (val))
#endif

#ifndef rtmGetDiscStates
# define rtmGetDiscStates(rtm) ((rtm)->ModelData.discStates)
#endif

#ifndef rtmSetDiscStates
# define rtmSetDiscStates(rtm, val) ((rtm)->ModelData.discStates = (val))
#endif

#ifndef rtmGetErrorStatusFlag
# define rtmGetErrorStatusFlag(rtm) ((rtm)->errorStatus)
#endif

#ifndef rtmSetErrorStatusFlag
# define rtmSetErrorStatusFlag(rtm, val) ((rtm)->errorStatus = (val))
#endif

#ifndef rtmGetFirstInitCondFlag
# define rtmGetFirstInitCondFlag(rtm) ((rtm)->Timing.firstInitCondFlag)
#endif

#ifndef rtmSetFirstInitCondFlag
# define rtmSetFirstInitCondFlag(rtm, val) ((rtm)->Timing.firstInitCondFlag = (val))
#endif

#ifndef rtmGetModelMappingInfo
# define rtmGetModelMappingInfo(rtm) ((rtm)->SpecialInfo.mappingInfo)
#endif

#ifndef rtmSetModelMappingInfo
# define rtmSetModelMappingInfo(rtm, val) ((rtm)->SpecialInfo.mappingInfo = (val))
#endif

#ifndef rtmGetModelName
# define rtmGetModelName(rtm) ((rtm)->modelName)
#endif

#ifndef rtmSetModelName
# define rtmSetModelName(rtm, val) ((rtm)->modelName = (val))
#endif

#ifndef rtmGetNTaskTicks
# define rtmGetNTaskTicks(rtm) ((rtm)->Timing.nTaskTicks)
#endif

#ifndef rtmSetNTaskTicks
# define rtmSetNTaskTicks(rtm, val) ((rtm)->Timing.nTaskTicks = (val))
#endif

#ifndef rtmGetNonsampledZCs
# define rtmGetNonsampledZCs(rtm) ((rtm)->ModelData.nonsampledZCs)
#endif

#ifndef rtmSetNonsampledZCs
# define rtmSetNonsampledZCs(rtm, val) ((rtm)->ModelData.nonsampledZCs = (val))
#endif

#ifndef rtmGetNumBlockIO
# define rtmGetNumBlockIO(rtm) ((rtm)->Sizes.numBlockIO)
#endif

#ifndef rtmSetNumBlockIO
# define rtmSetNumBlockIO(rtm, val) ((rtm)->Sizes.numBlockIO = (val))
#endif

#ifndef rtmGetNumBlockParams
# define rtmGetNumBlockParams(rtm) ((rtm)->Sizes.numBlockPrms)
#endif

#ifndef rtmSetNumBlockParams
# define rtmSetNumBlockParams(rtm, val) ((rtm)->Sizes.numBlockPrms = (val))
#endif

#ifndef rtmGetNumBlocks
# define rtmGetNumBlocks(rtm) ((rtm)->Sizes.numBlocks)
#endif

#ifndef rtmSetNumBlocks
# define rtmSetNumBlocks(rtm, val) ((rtm)->Sizes.numBlocks = (val))
#endif

#ifndef rtmGetNumContStates
# define rtmGetNumContStates(rtm) ((rtm)->Sizes.numContStates)
#endif

#ifndef rtmSetNumContStates
# define rtmSetNumContStates(rtm, val) ((rtm)->Sizes.numContStates = (val))
#endif

#ifndef rtmGetNumDWork
# define rtmGetNumDWork(rtm) ((rtm)->Sizes.numDwork)
#endif

#ifndef rtmSetNumDWork
# define rtmSetNumDWork(rtm, val) ((rtm)->Sizes.numDwork = (val))
#endif

#ifndef rtmGetNumInputPorts
# define rtmGetNumInputPorts(rtm) ((rtm)->Sizes.numIports)
#endif

#ifndef rtmSetNumInputPorts
# define rtmSetNumInputPorts(rtm, val) ((rtm)->Sizes.numIports = (val))
#endif

#ifndef rtmGetNumNonSampledZCs
# define rtmGetNumNonSampledZCs(rtm) ((rtm)->Sizes.numNonSampZCs)
#endif

#ifndef rtmSetNumNonSampledZCs
# define rtmSetNumNonSampledZCs(rtm, val) ((rtm)->Sizes.numNonSampZCs = (val))
#endif

#ifndef rtmGetNumOutputPorts
# define rtmGetNumOutputPorts(rtm) ((rtm)->Sizes.numOports)
#endif

#ifndef rtmSetNumOutputPorts
# define rtmSetNumOutputPorts(rtm, val) ((rtm)->Sizes.numOports = (val))
#endif

#ifndef rtmGetNumSFcnParams
# define rtmGetNumSFcnParams(rtm) ((rtm)->Sizes.numSFcnPrms)
#endif

#ifndef rtmSetNumSFcnParams
# define rtmSetNumSFcnParams(rtm, val) ((rtm)->Sizes.numSFcnPrms = (val))
#endif

#ifndef rtmGetNumSFunctions
# define rtmGetNumSFunctions(rtm) ((rtm)->Sizes.numSFcns)
#endif

#ifndef rtmSetNumSFunctions
# define rtmSetNumSFunctions(rtm, val) ((rtm)->Sizes.numSFcns = (val))
#endif

#ifndef rtmGetNumSampleTimes
# define rtmGetNumSampleTimes(rtm) ((rtm)->Sizes.numSampTimes)
#endif

#ifndef rtmSetNumSampleTimes
# define rtmSetNumSampleTimes(rtm, val) ((rtm)->Sizes.numSampTimes = (val))
#endif

#ifndef rtmGetNumU
# define rtmGetNumU(rtm) ((rtm)->Sizes.numU)
#endif

#ifndef rtmSetNumU
# define rtmSetNumU(rtm, val) ((rtm)->Sizes.numU = (val))
#endif

#ifndef rtmGetNumY
# define rtmGetNumY(rtm) ((rtm)->Sizes.numY)
#endif

#ifndef rtmSetNumY
# define rtmSetNumY(rtm, val) ((rtm)->Sizes.numY = (val))
#endif

#ifndef rtmGetOffsetTimePtr
# define rtmGetOffsetTimePtr(rtm) ((rtm)->Timing.offsetTimes)
#endif

#ifndef rtmSetOffsetTimePtr
# define rtmSetOffsetTimePtr(rtm, val) ((rtm)->Timing.offsetTimes = (val))
#endif

#ifndef rtmGetOptions
# define rtmGetOptions(rtm) ((rtm)->Sizes.options)
#endif

#ifndef rtmSetOptions
# define rtmSetOptions(rtm, val) ((rtm)->Sizes.options = (val))
#endif

#ifndef rtmGetPath
# define rtmGetPath(rtm) ((rtm)->path)
#endif

#ifndef rtmSetPath
# define rtmSetPath(rtm, val) ((rtm)->path = (val))
#endif

#ifndef rtmGetPerTaskSampleHits
# define rtmGetPerTaskSampleHits(rtm) ((rtm)->Timing.perTaskSampleHits)
#endif

#ifndef rtmSetPerTaskSampleHits
# define rtmSetPerTaskSampleHits(rtm, val) ((rtm)->Timing.perTaskSampleHits = (val))
#endif

#ifndef rtmGetPerTaskSampleHitsPtr
# define rtmGetPerTaskSampleHitsPtr(rtm) ((rtm)->Timing.perTaskSampleHits)
#endif

#ifndef rtmSetPerTaskSampleHitsPtr
# define rtmSetPerTaskSampleHitsPtr(rtm, val) ((rtm)->Timing.perTaskSampleHits = (val))
#endif

#ifndef rtmGetPrevZCSigState
# define rtmGetPrevZCSigState(rtm) ((rtm)->ModelData.prevZCSigState)
#endif

#ifndef rtmSetPrevZCSigState
# define rtmSetPrevZCSigState(rtm, val) ((rtm)->ModelData.prevZCSigState = (val))
#endif

#ifndef rtmGetRTWExtModeInfo
# define rtmGetRTWExtModeInfo(rtm) ((rtm)->extModeInfo)
#endif

#ifndef rtmSetRTWExtModeInfo
# define rtmSetRTWExtModeInfo(rtm, val) ((rtm)->extModeInfo = (val))
#endif

#ifndef rtmGetRTWGeneratedSFcn
# define rtmGetRTWGeneratedSFcn(rtm) ((rtm)->Sizes.rtwGenSfcn)
#endif

#ifndef rtmSetRTWGeneratedSFcn
# define rtmSetRTWGeneratedSFcn(rtm, val) ((rtm)->Sizes.rtwGenSfcn = (val))
#endif

#ifndef rtmGetRTWLogInfo
# define rtmGetRTWLogInfo(rtm) ((rtm)->rtwLogInfo)
#endif

#ifndef rtmSetRTWLogInfo
# define rtmSetRTWLogInfo(rtm, val) ((rtm)->rtwLogInfo = (val))
#endif

#ifndef rtmGetRTWRTModelMethodsInfo
# define rtmGetRTWRTModelMethodsInfo(rtm) ((rtm)->modelMethodsInfo)
#endif

#ifndef rtmSetRTWRTModelMethodsInfo
# define rtmSetRTWRTModelMethodsInfo(rtm, val) ((rtm)->modelMethodsInfo = (val))
#endif

#ifndef rtmGetRTWSfcnInfo
# define rtmGetRTWSfcnInfo(rtm) ((rtm)->sfcnInfo)
#endif

#ifndef rtmSetRTWSfcnInfo
# define rtmSetRTWSfcnInfo(rtm, val) ((rtm)->sfcnInfo = (val))
#endif

#ifndef rtmGetRTWSolverInfo
# define rtmGetRTWSolverInfo(rtm) ((rtm)->solverInfo)
#endif

#ifndef rtmSetRTWSolverInfo
# define rtmSetRTWSolverInfo(rtm, val) ((rtm)->solverInfo = (val))
#endif

#ifndef rtmGetReservedForXPC
# define rtmGetReservedForXPC(rtm) ((rtm)->SpecialInfo.xpcData)
#endif

#ifndef rtmSetReservedForXPC
# define rtmSetReservedForXPC(rtm, val) ((rtm)->SpecialInfo.xpcData = (val))
#endif

#ifndef rtmGetRootDWork
# define rtmGetRootDWork(rtm) ((rtm)->Work.dwork)
#endif

#ifndef rtmSetRootDWork
# define rtmSetRootDWork(rtm, val) ((rtm)->Work.dwork = (val))
#endif

#ifndef rtmGetSFunctions
# define rtmGetSFunctions(rtm) ((rtm)->childSfunctions)
#endif

#ifndef rtmSetSFunctions
# define rtmSetSFunctions(rtm, val) ((rtm)->childSfunctions = (val))
#endif

#ifndef rtmGetSampleHitPtr
# define rtmGetSampleHitPtr(rtm) ((rtm)->Timing.sampleHits)
#endif

#ifndef rtmSetSampleHitPtr
# define rtmSetSampleHitPtr(rtm, val) ((rtm)->Timing.sampleHits = (val))
#endif

#ifndef rtmGetSampleTimePtr
# define rtmGetSampleTimePtr(rtm) ((rtm)->Timing.sampleTimes)
#endif

#ifndef rtmSetSampleTimePtr
# define rtmSetSampleTimePtr(rtm, val) ((rtm)->Timing.sampleTimes = (val))
#endif

#ifndef rtmGetSampleTimeTaskIDPtr
# define rtmGetSampleTimeTaskIDPtr(rtm) ((rtm)->Timing.sampleTimeTaskIDPtr)
#endif

#ifndef rtmSetSampleTimeTaskIDPtr
# define rtmSetSampleTimeTaskIDPtr(rtm, val) ((rtm)->Timing.sampleTimeTaskIDPtr = (val))
#endif

#ifndef rtmGetSimMode
# define rtmGetSimMode(rtm) ((rtm)->simMode)
#endif

#ifndef rtmSetSimMode
# define rtmSetSimMode(rtm, val) ((rtm)->simMode = (val))
#endif

#ifndef rtmGetSimTimeStep
# define rtmGetSimTimeStep(rtm) ((rtm)->Timing.simTimeStep)
#endif

#ifndef rtmSetSimTimeStep
# define rtmSetSimTimeStep(rtm, val) ((rtm)->Timing.simTimeStep = (val))
#endif

#ifndef rtmGetStartTime
# define rtmGetStartTime(rtm) ((rtm)->Timing.tStart)
#endif

#ifndef rtmSetStartTime
# define rtmSetStartTime(rtm, val) ((rtm)->Timing.tStart = (val))
#endif

#ifndef rtmGetStepSize
# define rtmGetStepSize(rtm) ((rtm)->Timing.stepSize)
#endif

#ifndef rtmSetStepSize
# define rtmSetStepSize(rtm, val) ((rtm)->Timing.stepSize = (val))
#endif

#ifndef rtmGetStopRequestedFlag
# define rtmGetStopRequestedFlag(rtm) ((rtm)->Timing.stopRequestedFlag)
#endif

#ifndef rtmSetStopRequestedFlag
# define rtmSetStopRequestedFlag(rtm, val) ((rtm)->Timing.stopRequestedFlag = (val))
#endif

#ifndef rtmGetTFinal
# define rtmGetTFinal(rtm) ((rtm)->Timing.tFinal)
#endif

#ifndef rtmSetTFinal
# define rtmSetTFinal(rtm, val) ((rtm)->Timing.tFinal = (val))
#endif

#ifndef rtmGetTaskTimes
# define rtmGetTaskTimes(rtm) ((rtm)->Timing.taskTimes)
#endif

#ifndef rtmSetTaskTimes
# define rtmSetTaskTimes(rtm, val) ((rtm)->Timing.taskTimes = (val))
#endif

#ifndef rtmGetTimeOfLastOutput
# define rtmGetTimeOfLastOutput(rtm) ((rtm)->Timing.timeOfLastOutput)
#endif

#ifndef rtmSetTimeOfLastOutput
# define rtmSetTimeOfLastOutput(rtm, val) ((rtm)->Timing.timeOfLastOutput = (val))
#endif

#ifndef rtmGetTimePtr
# define rtmGetTimePtr(rtm) ((rtm)->Timing.t)
#endif

#ifndef rtmSetTimePtr
# define rtmSetTimePtr(rtm, val) ((rtm)->Timing.t = (val))
#endif

#ifndef rtmGetTimingData
# define rtmGetTimingData(rtm) ((rtm)->Timing.timingData)
#endif

#ifndef rtmSetTimingData
# define rtmSetTimingData(rtm, val) ((rtm)->Timing.timingData = (val))
#endif

#ifndef rtmGetU
# define rtmGetU(rtm) ((rtm)->ModelData.inputs)
#endif

#ifndef rtmSetU
# define rtmSetU(rtm, val) ((rtm)->ModelData.inputs = (val))
#endif

#ifndef rtmGetVarNextHitTimesListPtr
# define rtmGetVarNextHitTimesListPtr(rtm) ((rtm)->Timing.varNextHitTimesList)
#endif

#ifndef rtmSetVarNextHitTimesListPtr
# define rtmSetVarNextHitTimesListPtr(rtm, val) ((rtm)->Timing.varNextHitTimesList = (val))
#endif

#ifndef rtmGetY
# define rtmGetY(rtm) ((rtm)->ModelData.outputs)
#endif

#ifndef rtmSetY
# define rtmSetY(rtm, val) ((rtm)->ModelData.outputs = (val))
#endif

#ifndef rtmGetZCCacheNeedsReset
# define rtmGetZCCacheNeedsReset(rtm) ((rtm)->ModelData.zCCacheNeedsReset)
#endif

#ifndef rtmSetZCCacheNeedsReset
# define rtmSetZCCacheNeedsReset(rtm, val) ((rtm)->ModelData.zCCacheNeedsReset = (val))
#endif

#ifndef rtmGetdX
# define rtmGetdX(rtm) ((rtm)->ModelData.derivs)
#endif

#ifndef rtmSetdX
# define rtmSetdX(rtm, val) ((rtm)->ModelData.derivs = (val))
#endif

#ifndef rtmGetChecksumVal
# define rtmGetChecksumVal(rtm, idx) ((rtm)->Sizes.checksums[idx])
#endif

#ifndef rtmSetChecksumVal
# define rtmSetChecksumVal(rtm, idx, val) ((rtm)->Sizes.checksums[idx] = (val))
#endif

#ifndef rtmGetDWork
# define rtmGetDWork(rtm, idx) ((rtm)->Work.dwork[idx])
#endif

#ifndef rtmSetDWork
# define rtmSetDWork(rtm, idx, val) ((rtm)->Work.dwork[idx] = (val))
#endif

#ifndef rtmGetOffsetTime
# define rtmGetOffsetTime(rtm, idx) ((rtm)->Timing.offsetTimes[idx])
#endif

#ifndef rtmSetOffsetTime
# define rtmSetOffsetTime(rtm, idx, val) ((rtm)->Timing.offsetTimes[idx] = (val))
#endif

#ifndef rtmGetSFunction
# define rtmGetSFunction(rtm, idx) ((rtm)->childSfunctions[idx])
#endif

#ifndef rtmSetSFunction
# define rtmSetSFunction(rtm, idx, val) ((rtm)->childSfunctions[idx] = (val))
#endif

#ifndef rtmGetSampleTime
# define rtmGetSampleTime(rtm, idx) ((rtm)->Timing.sampleTimes[idx])
#endif

#ifndef rtmSetSampleTime
# define rtmSetSampleTime(rtm, idx, val) ((rtm)->Timing.sampleTimes[idx] = (val))
#endif

#ifndef rtmGetSampleTimeTaskID
# define rtmGetSampleTimeTaskID(rtm, idx) ((rtm)->Timing.sampleTimeTaskIDPtr[idx])
#endif

#ifndef rtmSetSampleTimeTaskID
# define rtmSetSampleTimeTaskID(rtm, idx, val) ((rtm)->Timing.sampleTimeTaskIDPtr[idx] = (val))
#endif

#ifndef rtmGetVarNextHitTime
# define rtmGetVarNextHitTime(rtm, idx) ((rtm)->Timing.varNextHitTimesList[idx])
#endif

#ifndef rtmSetVarNextHitTime
# define rtmSetVarNextHitTime(rtm, idx, val) ((rtm)->Timing.varNextHitTimesList[idx] = (val))
#endif

#ifndef rtmIsContinuousTask
# define rtmIsContinuousTask(rtm, tid) (tid) == 0
#endif

#ifndef rtmGetErrorStatus
# define rtmGetErrorStatus(rtm) (rtm)->errorStatus
#endif

#ifndef rtmSetErrorStatus
# define rtmSetErrorStatus(rtm, val) (rtm)->errorStatus = ((val))
#endif

#ifndef rtmIsFirstInitCond
# define rtmIsFirstInitCond(rtm) rtmGetT((rtm)) == (rtmGetTStart((rtm)))
#endif

#ifndef rtmIsMajorTimeStep
# define rtmIsMajorTimeStep(rtm) ((rtm)->Timing.simTimeStep) == MAJOR_TIME_STEP
#endif

#ifndef rtmIsMinorTimeStep
# define rtmIsMinorTimeStep(rtm) ((rtm)->Timing.simTimeStep) == MINOR_TIME_STEP
#endif

#ifndef rtmIsSampleHit
# define rtmIsSampleHit(rtm, sti, tid) (rtmIsMajorTimeStep((rtm)) && (rtm)->Timing.sampleHits[(rtm)->Timing.sampleTimeTaskIDPtr[sti]])
#endif

#ifndef rtmIsSpecialSampleHit
# define rtmIsSpecialSampleHit(rtm, sti, prom_sti, tid) (rtmIsMajorTimeStep((rtm)) && (rtm)->Timing.sampleHits[(rtm)->Timing.sampleTimeTaskIDPtr[sti]])
#endif

#ifndef rtmGetStopRequested
# define rtmGetStopRequested(rtm) (rtm)->Timing.stopRequestedFlag
#endif

#ifndef rtmSetStopRequested
# define rtmSetStopRequested(rtm, val) (rtm)->Timing.stopRequestedFlag = ((val))
#endif

#ifndef rtmGetT
# define rtmGetT(rtm) rtmGetTPtr((rtm))[0]
#endif

#ifndef rtmSetT
# define rtmSetT(rtm, val) rtmGetTPtr((rtm))[0] = ((val))
#endif

#ifndef rtmGetTPtr
# define rtmGetTPtr(rtm) (rtm)->Timing.t
#endif

#ifndef rtmSetTPtr
# define rtmSetTPtr(rtm, val) (rtm)->Timing.t = ((val))
#endif

#ifndef rtmGetTStart
# define rtmGetTStart(rtm) (rtm)->Timing.tStart
#endif

#ifndef rtmSetTStart
# define rtmSetTStart(rtm, val) (rtm)->Timing.tStart = ((val))
#endif

#ifndef rtmGetTaskTime
# define rtmGetTaskTime(rtm, sti) rtmGetTPtr((rtm))[(rtm)->Timing.sampleTimeTaskIDPtr[sti]]
#endif

#ifndef rtmSetTaskTime
# define rtmSetTaskTime(rtm, sti, val) rtmGetTPtr((rtm))[sti] = ((val))
#endif

/* Definition for use in the target main file */
#define pendulum_control_rtModel        rtModel_pendulum_control

extern Parameters rtP;                  /* parameters */

/* External data declarations for dependent source files */

extern BlockIO rtB;                     /* block i/o */
extern D_Work rtDWork;                  /* data type work */

/* 
 * The generated code includes comments that allow you to trace directly 
 * back to the appropriate location in the model.  The basic format
 * is <system>/block_name, where system is the system number (uniquely
 * assigned by Simulink) and block_name is the name of the block.
 *
 * Use the MATLAB hilite_system command to trace the generated code back
 * to the model.  For example,
 *
 * hilite_system('<S3>')    - opens system 3
 * hilite_system('<S3>/Kp') - opens and selects block Kp which resides in S3
 *
 * Here is the system hierarchy for this model
 *
 * '<Root>' : pendulum_control
 */

#endif                                  /* _RTW_HEADER_pendulum_control_h_ */
