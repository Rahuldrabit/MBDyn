/* 
 * MBDyn (C) is a multibody analysis code. 
 * http://www.mbdyn.org
 *
 * Copyright (C) 1996-2017
 *
 * Pierangelo Masarati	<masarati@aero.polimi.it>
 * Paolo Mantegazza	<mantegazza@aero.polimi.it>
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
  * With the contribution of Runsen Zhang <runsen.zhang@polimi.it>
  * during Google Summer of Code 2020
 */



/* name rules for MBDyn-chrono::engine:
functions/variables: MBDyn_CE_XxxYyy (eg. MBDyn_CE_NodesNum)
pointer: pMBDyn_CE_XxxYyy
structures/class: MBDYN_CE_XXXYYY (eg. MBDYN_CE_POINTDATA)
temporary variables: mbdynce_xxx (eg. mbdynce_point)
functions/variables for C::E Model:MBDyn_CE_CEModel_XxxYyy
*/

#ifndef MBDYN_CE_H
#define MBDYN_CE_H

#include"chrono/ChConfig.h"
#include <vector>

extern "C" {

    //- IDs of coupling bodies and motors in C::E
    //- a set of IDs
    //- Body_Label: body label in C::E(/Chrono);
    //- Motor_Label: motor label in C::E;
    //- bool output: output the body motion in C::E;
    //- constraint (MotionImposed) in Chrono:
    //- Node in MBDyn  >>>>> Body Ground (Marker Ground);
    //- Body in Chrono >>>>> Body i (Marker i);
    //- constraint connects Marker i and Marker Ground;
    //- constraints are described in Marker i local ref. 
struct MBDYN_CE_CEMODELDATA
{
    unsigned MBDyn_CE_CEBody_Label;
    unsigned MBDyn_CE_CEMotor_Label;
    bool bMBDyn_CE_CEBody_Output;
    bool bMBDyn_CE_Constraint[6];              //- position and rotation constraints, expresed in node local ref.
    double MBDyn_CE_CEBody_Offset[3];          //- relative position between constraints (Marker i) and C::E body, expressed in C::E body ref.
    double MBDyn_CE_CEBody_Rh[9]; //- relative rotation between constraints and MBDyn Node, expressed in node ref.
};

    //- 0: loose interface
    //- 1: tight coupling
    //- >1: exchange every iCoupling iterations // TO DO
enum MBDyn_CE_COUPLING
{
    COUPLING_NONE = -2,
    COUPLING_STSTAGGERED = -1, // not implenmented, only for consistent with strext
    COUPLING_LOOSE = 0,        // loose
    COUPLING_TIGHT = 1,
    //COUPLING_MULTIRATE >1 // TO DO
};

enum MBDyn_CE_COUPLING_LOOSE
{
    TIGHT = 0,           //- meaningless
    LOOSE_EMBEDDED = 1,  //- Embedded function method
    LOOSE_JACOBIAN = 2,  //- Jacobian/parallel type
    LOOSE_GAUSS = 3,     //- Guass/serial type
};

enum MBDyn_CE_CEMOTORTYPE
{
    POSITION = 0,     //- uses x_{k+1,P}
    VELOCITY = 1,     //- uses x_{k+1,P}/time_step + \beta*v_{k+1,P}
    ACCELERATION = 2, //- uses \alpha*x_{k+1,P} + \beta*v_{k+1,P} + \gamma*a_{k}
    ORIGIN=3,         //- uses x_k and v_k+1
};

enum MBDyn_CE_CEFORCETYPE
{
    REACTION_FORCE = 0,  //- using reaction forces as coupling forces (coupled by constraints)
    CONTACT_FORCE = 1,   //- using contact forces as coupling forces (coupled by anytype, but using coupling forces acting on the coupling bodies)
};

enum MBDYN_CE_OUTPUTTYPE
{
    MBDYN_CHRONO_OUTPUT_ALLBODIES = 0,              //- outputing kinematic data of all bodies in chrono
    MBDYN_CHRONO_OUTPUT_SELECTEDCOUPLINGBODIES = 1, //- outputing kinematic data of selected coupling bodies in chrono
};

//- opaque pointer to C::E system
typedef  void* pMBDyn_CE_CEModel_t;

//- create the pointer
pMBDyn_CE_CEModel_t MBDyn_CE_CEModel_Init(std::vector<double> & MBDyn_CE_CEModel_DataSave, //- space for data save
                                          const int MBDyn_CE_CEMotorType, //- motor type
                                          const double* pMBDyn_CE_CEFrame, const double* MBDyn_CE_CEScale, //- coupling information: ground ref and scale for units
                                          std::vector<MBDYN_CE_CEMODELDATA> & MBDyn_CE_CEModel_Label, //- coupling information: coupling bodies
                                          const int & MBDyn_CE_CouplingType); //- coupling information: coupling type

//- destroy the pointer
void
MBDyn_CE_CEModel_Destroy(pMBDyn_CE_CEModel_t &pMBDyn_CE_CEModel);

//- check the consistency of the initial condition settings between MBDyn and C::E model
//- run after C::E receiving data from MBDyn
bool 
MBDyn_CE_CEModel_InitCheck(pMBDyn_CE_CEModel_t pMBDyn_CE_CEModel,
                                  const std::vector<double> &MBDyn_CE_CouplingKinematic,
                                  const unsigned &MBDyn_CE_NodesNum,
                                  const std::vector<MBDYN_CE_CEMODELDATA> &MBDyn_CE_CEModel_Label,
                                  const double * pMBDyn_CE_Gravity);

                                  
//- C::E models receive coupling motion from the buffer
int 
MBDyn_CE_CEModel_RecvFromBuf(pMBDyn_CE_CEModel_t pMBDyn_CE_CEModel,
                                  const std::vector<double> &MBDyn_CE_CouplingKinematic,
                                  const unsigned &MBDyn_CE_NodesNum,
                                  const std::vector<MBDYN_CE_CEMODELDATA> &MBDyn_CE_CEModel_Label,
                                  const int MBDyn_CE_CEMotorType,
                                  double* MBDyn_CE_CEMotorType_coeff,
                                  double time_step,
                                  bool bMBDyn_CE_Verbose);


//- C::E models send coupling forces to the buffer
int 
MBDyn_CE_CEModel_SendToBuf(pMBDyn_CE_CEModel_t pMBDyn_CE_CEModel, std::vector<double> &MBDyn_CE_CouplingDynamic, 
                                double* pMBDyn_CE_CEFrame,
                                const unsigned& MBDyn_CE_NodesNum,
                                const double* MBDyn_CE_CEScale,
                                const std::vector<MBDYN_CE_CEMODELDATA> & MBDyn_CE_CEModel_Label,
                                const int MBDyn_CE_CEForceType, 
                                bool bMBDyn_CE_Verbose);

//- update CEModel, and do time integration.
int
MBDyn_CE_CEModel_DoStepDynamics(pMBDyn_CE_CEModel_t pMBDyn_CE_CEModel, double time_step, bool bMBDyn_CE_Verbose);

//- save CEModel at current step for reloading it in the tight coupling scheme
//- (before advance())
int
MBDyn_CE_CEModel_DataSave(pMBDyn_CE_CEModel_t pMBDyn_CE_CEModel, 
                        std::vector<double> & MBDyn_CE_CEModel_Data);

//- reload data in the tight coupling scheme at each iteration
int
MBDyn_CE_CEModel_DataReload(pMBDyn_CE_CEModel_t pMBDyn_CE_CEModel, 
                        std::vector<double> & MBDyn_CE_CEModel_Data);

//- write data to files. 
int 
MBDyn_CE_CEModel_WriteToFiles(pMBDyn_CE_CEModel_t pMBDyn_CE_CEModel, 
                    const std::vector<MBDYN_CE_CEMODELDATA> & MBDyn_CE_CEModel_Label,
                    double *pMBDyn_CE_CEFrame,  const double* MBDyn_CE_CEScale,
                    std::ostream & out,
                    int MBDyn_CE_OutputType);
}
#endif //- MBDYN_CE_H
