/* $Header$ */
/*
 * MBDyn (C) is a multibody analysis code.
 * http://www.mbdyn.org
 *
 * Copyright (C) 1996-2017
 *
 * Pierangelo Masarati  <masarati@aero.polimi.it>
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
/* module-rotor_disc
 * Author: Matteo Daniele
 *
 * Copyright (C) 2008-2021
 *
 * Matteo Daniele <matteo.daniele@polimi.it>
 *
 * Dipartimento di Ingegneria Aerospaziale - Politecnico di Milano
 * via La Masa, 34 - 20156 Milano, Italy
 * http://www.aero.polimi.it
 *
 */
// Author: Matteo Daniele <matteo.daniele@polimi.it>
// tail rotor module for mbdyn

// model as follower force composed of a part depending on xapedal and a part depending on twist and inflow
// T = T0*(T_theta+T_lambda_twist)
#include "mbconfig.h" 		/* This goes first in every *.c,*.cc file */

#include <iostream>
#include <cmath>
#include <fstream>

#include "module-rotor_disc.h"


RotorDisc::RotorDisc( unsigned int uLabel, const DofOwner *pDO,
                                DataManager* pDM, MBDynParser& HP)
//: AerodynamicElem(uLabel, pDO, flag(0))
//: Elem(uLabel, flag(0)), AerodynamicElem(uLabel, pDO, flag(0)), UserDefinedElem(uLabel, pDO)
: UserDefinedElem(uLabel, pDO)
{
    if (HP.IsKeyWord("help")) {
        silent_cout("\nModule: rotor disc\n"
        "Author: Matteo Daniele <matteo.daniele@polimi.it>\n"
        "Organization:	Dipartimento di Ingegneria Aerospaziale			\n"
        "		Politecnico di Milano					\n"
        "		http://www.aero.polimi.it				\n"
        "									\n"
        "	All rights reserved						\n"
        "Implementation of a disc rotor model with closed form inflow calculation\n"
        "Input:\n"
        "   hub structural node label,\n"
        "   rotor relative position wrt structural node,\n"
        "   (the rotor force is always oriented as the Z of the reference frame considered),\n"
        "   control input driver (collective input [rad]),\n"
        "   rotor angular velocity driver [rad/s],\n"
        "   rotor radius [m],\n"
        "   rotor solidity [-],\n"
        "   blade Cl0 [1/rad],\n"
        "   blade ClAlpha [1/rad],\n"
        "   [drag coefficient,\n"
        "       cd0 [-] ,cd1 [1/rad],cd2 [1/rad^2]],\n"
        "   blade twist [rad],\n"
        "   alpha stall min (stall) [rad],\n"
        "   alpha stall max (stall) [rad],\n"
        "   control input minvalue (saturation) [rad],\n"
        "   control input maxvalue (saturation) [rad],\n"
        "   [main rotor data | MTOW]\n"
        "   |____ main rotor data,\n"
        "   |       hubs distance,\n"
        "   |       main rotor nominal power,\n"
        "   |       main rotor angular speed,\n"
        "   |____ MTOW\n"
        "\n"
        "Output:\n"
        "1):       label               [-]\n"
        "2-4):     force               [N]\n"
        "5-7):     moment              [Nm]\n"
        "8):       thetaColl           [rad]\n"
        "9):       Thrust              [N]\n"
        "10):      CoupleInduced       [Nm]\n"
        "11):      CoupleProfile       [Nm]\n"
        "12):      Couple              [Nm]\n"
        "13):      PowerInduced        [W]\n"
        "14):      PowerProfile        [W]\n"
        "15):      Power               [W]\n"
        "16):      PowerInducedHover   [W]\n"
        "17):      PowerProfileHover   [W]\n"
        "18):      PowerHover          [W]\n"
        "19):      alphaTPP            [rad]\n"
        "20):      rho                 [kg/m3]\n"
        "21):      lambda              [-]\n"
        "22):      V1                  [m/s]\n"
        "23):      v1h                 [m/s]\n"
        "24-26):   VTrHub              [m/s]\n"
        "27):      CpInduced           [-]\n"
        "28):      CpProfile           [-]\n"
        "29):      Cp                  [-]\n"
        "30):      CpSigma             [-]\n"
        "31):      Ct                  [-]\n"
        "32):      CtSigma             [-]\n"
        "33):      FOM                 [-]\n"
        << std::endl);

        if (!HP.IsArg()){
            throw NoErr(MBDYN_EXCEPT_ARGS);
        }
    }

    // read rotor hub node
    pHubNode = pDM->ReadNode<StructNode, Node::STRUCTURAL>(HP);
    ReferenceFrame rf;
    if (pHubNode)
    {
        rf = ReferenceFrame(pHubNode);
    }

    // distanza dal nodo (vettore di 3 elementi)
    // if wrong reference node label
    if (pHubNode == 0)
    {
        silent_cerr("rotordisc(" << uLabel << "): "
				"invalid node type at line " << HP.GetLineData() << std::endl);
			    throw ErrGeneric(MBDYN_EXCEPT_ARGS);
    }
    // when keyword "position" is indicated, the next 3 elements are the
    // relative position components of the point of application of the force
    // wrt the node
    /*force: label, type,
        node_label,
        position, [vec3 indicating relative position | reference, smt, vec3 of relative pos wrt smt]
        [vec3 of relative direction + driver force | vec3 of drive callers in the rf of the node]
    */
    DEBUGCOUT( "reading rotor disc element..."<<std::endl);
    if (HP.IsKeyWord("position")){
        // obtain relative arm wrt node
        HubNodeArm = HP.GetPosRel(rf);
    }
    DEBUGCOUT( "rotordisc("<< uLabel << "): thrust relative position wrt node "<< pHubNode->GetLabel() <<" : "<< HubNodeArm <<std::endl);

    if (HP.IsKeyWord("orientation")){
        // obtain relative orientation wrt node
        RThrustOrientation = HP.GetRotRel(rf);
    }
    DEBUGCOUT("rotordisc("<< uLabel << "): thrust relative orientation "<<std::endl);
    DEBUGCOUT("wrt node "<< pHubNode->GetLabel() <<" : " <<std::endl);
    DEBUGCOUT( RThrustOrientation.GetRow(1) <<std::endl);
    DEBUGCOUT( RThrustOrientation.GetRow(2) <<std::endl);
    DEBUGCOUT( RThrustOrientation.GetRow(3) <<std::endl);
    // direction of thrust in the reference frame of hub node
    // the magnitude of the drivers is not important since it only
    // indicates the direction of the force wrt hub reference frame
    // pDCForceDir = ReadDCVecRel(pDM, HP, rf);
    // initialize the drive owner of the force direction
    // DOForceDir.Set(pDCForceDir);
    // drive di input (collective pitch)
    if (HP.IsKeyWord("collective" "input")){
        pXColl      = HP.GetDriveCaller(); // da adoperare assieme a pXColl->dGet()
        thetaColl   = pXColl->dGet();
    }
    // drive di input air density
    // pRho    = HP.GetDriveCaller();

    // air density taken by exploiting class inheritance on aerodynamic element
    // const Vec3& XPosAbs(pHubNode->GetXCurr());
    DEBUGCOUT("rotordisc("<< uLabel << "): rho = "<< rho <<std::endl);
    rho = 1.225 ; // dGetAirDensity(pHubNode->GetXCurr());
    DEBUGCOUT("rotordisc("<< uLabel << "): rho = "<< rho <<std::endl);

    bool bGotOmega      = false;
    bool bGotChord      = false;
    bool bGotNBlades    = false;
    // bool bGotSigma      = false;
    // drive di input rotor angular speed;
    if (HP.IsKeyWord("angular" "velocity")){
        pOmega  = HP.GetDriveCaller();
        bGotOmega = true;
        RotorOmega = pOmega->dGet();
    }
    // dati di progetto del rotore
    if (HP.IsKeyWord("radius")){
        RotorRadius             = HP.GetReal();
    }
    // TODO: AGGIUNGERE METODO CHE DECIDA COME SI VOGLIONO CALCOLARE I DATI
    if (HP.IsKeyWord("chord")){
        Chord = HP.GetReal();
        bGotChord = true;
    }
    if (HP.IsKeyWord("blade" "number")){
        NBlades = HP.GetInt();
        bGotNBlades = true;
    }
    // if i have both then useless to look for solidity in the data
    if (bGotChord & bGotNBlades){
        RotorSolidity = doublereal(NBlades)*Chord/(M_PI*RotorRadius);
    }
    else if (HP.IsKeyWord("sigma")){
        RotorSolidity = HP.GetReal();
    }

    if (HP.IsKeyWord("cl0")){
        Cl0 = HP.GetReal();
    }
    else { 
        //std::cout << "rotordisc(" << uLabel << "): Cl0 not provided, assuming null Cl0"<< std::endl;
        Cl0 = 0.0;
    }

    if (HP.IsKeyWord("clalpha")){
        ClAlpha = HP.GetReal();
    }

    if (HP.IsKeyWord("drag" "coefficient")){
        if(HP.IsKeyWord("cd0")){Cd0=HP.GetReal();}
        if(HP.IsKeyWord("cd1")){Cd1=HP.GetReal();}
        if(HP.IsKeyWord("cd2")){Cd2=HP.GetReal();}
        bWithDrag = true;
    }
    else {
        //std::cout << "rotordisc(" << uLabel << "): drag coefficient not provided, assuming null rotor drag"<< std::endl;
        bWithDrag = false;
    }

    if (HP.IsKeyWord("twist")){
        BladeTwist = HP.GetReal();
    }
    else {
        //std::cout << "rotordisc(" << uLabel << "): twist not provided, assuming null twist"<< std::endl;
        BladeTwist = 0.0;
    }

    if (HP.IsKeyWord("stall" "limits")){
        AOAStallMin             = HP.GetReal();
        AOAStallMax             = HP.GetReal();
        bWithStallLimits = true;
    }
    else {
        //std::cout << "rotordisc(" << uLabel << "): stall limits not provided, stall will not be included in the model"<< std::endl;
        bWithStallLimits = false;
    }

    if (HP.IsKeyWord("control" "limits")){
        thetaCollMin            = HP.GetReal();
        thetaCollMax            = HP.GetReal();
        bWithCollSaturation = true;
    }
    else {
        //std::cout << "rotordisc(" << uLabel << "): control limits not provided, saturation not included"<< std::endl;
        bWithCollSaturation = false;
    }

    // disc area
    DiscArea = M_PI*pow(RotorRadius, 2.0);

    // initialize the stall effects on the rotor
    computeCLInit();

    bool bGotTailRotor = false;
    bool bGotMainRotor = false;

    // reference values if we are dealing with a tail rotor
    if (HP.IsKeyWord("main" "rotor" "data")){
        if (HP.IsKeyWord("hubs" "distance")){
            hubs_distance = HP.GetReal();
            // pay attention to possible negative values
            hubs_distance = std::abs(hubs_distance);
        }
        if (HP.IsKeyWord("main" "rotor" "nominal" "power")){
            mr_nominal_power_shp    = HP.GetReal();
        }
        if (HP.IsKeyWord("main" "rotor" "nominal" "angular" "speed")){
            mr_nominal_omega        = HP.GetReal();
            if (bGotOmega == false){
                //std::cout << "rotordisc(" << uLabel << "): tail rotor angular speed not provided: "<< std::endl;
                //std::cout << "using as default value: tail_rotor_omega = 4*main_rotor_omega"<< std::endl;
                RotorOmega = 4.0*mr_nominal_omega;
            }
        }


        // compute nominal main rotor torque in hover
        doublereal mr_nominal_power_w   = mr_nominal_power_shp*hp2W;
        doublereal mr_nominal_torque_Nm = mr_nominal_power_w/mr_nominal_omega;
        ThrustHover                     = mr_nominal_torque_Nm/hubs_distance;
        bGotTailRotor = true;

    }
    // second case: we are dealing with a disc rotor, we need to compute the thrust in hover
    // at sea level to find the induced velocity in hover
    else if (HP.IsKeyWord("MTOW")){
        
        MTOW = HP.GetReal();
        ThrustHover = MTOW*g;

        bGotMainRotor = true;
    }

    // v1h depending on costant rotor parameters
    v1hPart             = sqrt(ThrustHover/(2.0*DiscArea));
    doublereal v1hInit  = v1hPart/sqrt(1.225);
    // induced power in hover
    PowerInducedHover = ThrustHover*v1hInit;
    // profile power in hover
    if (bWithDrag){
        Cd = Cd0+Cd1*thetaColl+Cd2*pow(thetaColl,2.0);
        Cl = Cl0+ClAlpha*thetaColl;
        doublereal E = abs(Cl/Cd);
        PowerProfile = 0.75*ThrustHover*RotorRadius*RotorOmega/E;
    }
    else{
        PowerProfile = 0.0;
    }
    PowerHover = PowerInducedHover+PowerProfileHover;

    if (bGotTailRotor){
        //std::cout <<"ref distance [m]: " << hubs_distance << std::endl;
        //std::cout <<"ref nominal power [hp]: " << mr_nominal_power_shp << std::endl;
        //std::cout <<"ref omega [rad/s]: " << mr_nominal_omega << std::endl;
    }
    else if (bGotMainRotor){
        //std::cout <<"MTOW [kg]: " << MTOW << std::endl;
    }

    doublereal t0,t1,t2,t3, CtHover;
    CtHover = ThrustHover/(rho*pow(RotorOmega*RotorRadius,2.0)*DiscArea);
    t0 = 12.0/(RotorSolidity*ClAlpha);
    t1 = CtHover/2.0;
    t2 = RotorSolidity*ClAlpha/8.0*sqrt(t1);
    t3 = -0.75*BladeTwist;

    doublereal thetaCollHover = t0*(t1+t2)-t3; (void)thetaCollHover; // silence unused warning

    //std::cout <<"rotordisc(" << uLabel << "): Initial Properties"<< std::endl;
    //std::cout <<"###########################" << std::endl;
    //std::cout <<"Induced velocity in Hover [m/s]: "     << v1hInit                << std::endl;
    //std::cout <<"Thrust required for Hover [N]: "       << ThrustHover            << std::endl;
    //std::cout <<"Power required for Hover [hp]: "       << PowerHover*W2hp        << std::endl;
    //std::cout <<"Collective required for Hover [deg]: " << thetaCollHover*rad2deg        << std::endl;
    //std::cout <<"###########################" << std::endl;


    SetOutputFlag(pDM->fReadOutput(HP, Elem::LOADABLE));
    std::ostream& out = pDM->GetLogFile();
    if (bGotTailRotor){
        out << "rotordisc: " << uLabel
            << " " << pHubNode->GetLabel() // node label
            << " " << HubNodeArm
            << " " << RotorRadius
            << " " << DiscArea
            << " " << RotorSolidity
            << " " << Cl0
            << " " << ClAlpha
            << " " << BladeTwist
            << " " << AOAStallMin
            << " " << AOAStallMax
            << " " << thetaCollMin
            << " " << thetaCollMax
            << " " << hubs_distance
            << " " << mr_nominal_power_shp
            << " " << mr_nominal_omega
            << std::endl;
    }
    else if (bGotMainRotor){
        out << "rotordisc: " << uLabel
            << " " << pHubNode->GetLabel() // node label
            << " " << HubNodeArm
            << " " << RotorRadius
            << " " << DiscArea
            << " " << RotorSolidity
            << " " << Cl0
            << " " << ClAlpha
            << " " << BladeTwist
            << " " << AOAStallMin
            << " " << AOAStallMax
            << " " << thetaCollMin
            << " " << thetaCollMax
            << " " << MTOW
            << std::endl;
    }
}

RotorDisc::~RotorDisc()
{
    NO_OP;
}

void RotorDisc::Output(OutputHandler& OH) const
{
    if (bToBeOutput() && OH.UseText(OutputHandler::LOADABLE))
    {
        std::ostream& out = OH.Loadable();
        out << std::setw(8) 
            << GetLabel()                   // 1:       label               [-]
            << " " << F            	        // 2-4:     force               [N]
            << " " << M            	        // 5-7:     moment              [Nm]
            << " " << thetaColl             // 8:       thetaColl           [rad]
            << " " << RotorOmega            // 9:       RotorOmega           [rad/s]
            << " " << Thrust                // 10:       Thrust              [N]
            << " " << CoupleInduced         // 11:       CoupleInduced       [Nm]
            << " " << CoupleProfile         // 12:      CoupleProfile       [Nm]
            << " " << Couple                // 13:      Couple              [Nm]
            << " " << PowerInduced          // 14:      PowerInduced        [W]
            << " " << PowerProfile          // 15:      PowerProfile        [W]
            << " " << Power                 // 16:      Power               [W]
            << " " << PowerInducedHover     // 17:      PowerInducedHover   [W]
            << " " << PowerProfileHover     // 18:      PowerProfileHover   [W]
            << " " << PowerHover            // 19:      PowerHover          [W]
            << " " << alphaTPP              // 20:      alphaTPP            [rad]
            << " " << rho                   // 21:      rho                 [kg/m3]
            << " " << lambda                // 22:      lambda              [-]
            << " " << V1                    // 23:      V1                  [m/s]
            << " " << v1h                   // 24:      v1h                 [m/s]
            << " " << VTrHub                // 25-27:   VTrHub              [m/s]
            << " " << CpInduced             // 28:      CpInduced           [-]
            << " " << CpProfile             // 29:      CpProfile           [-]
            << " " << Cp                    // 30:      Cp                  [-]
            << " " << CpSigma               // 31:      CpSigma             [-]
            << " " << Ct                    // 32:      Ct                  [-]
            << " " << CtSigma               // 33:      CtSigma             [-]
            << " " << FOM                   // 34:      FOM                 [-]
            << std::endl;
    }

    // TODO: NetCDF output
}


// partial derivatives for Jacobian assembly
void RotorDisc::dMuCalc()
{
    // Vtip: rotor tip speed
    // V: airspeed vector (u, v, w)
    // partial derivatives wrt u,v,w,omega,rho
    dMu[0] = u/(Vtip*Vtot);    // = dMudU;
    dMu[1] = v/(Vtip*Vtot);    // = dMudV;
    dMu[2] = w/(Vtip*Vtot);    // = dMudW;
    dMu[3] = -(Vtot*RotorRadius)/(Vtip2);  // = dMudOmega;
    dMu[4] = 0.0;                          // = dMudRho;
}

void RotorDisc::dLambdaCalc()
{
    doublereal v1h4 = pow(v1h, 4.0);
    doublereal dLambdaRhoDen = 2.0*Vtip*V1*a_v1*rho;

    dLambda[0]  = (0.5*u/(Vtip*V1))*(1.0-Vtot2/(2.0*a_v1));  // = dLambdadU;
    dLambda[1]  = (0.5*v/(Vtip*V1))*(1.0-Vtot2/(2.0*a_v1));  // = dLambdadV;
    dLambda[2]  = (0.5*w/(Vtip*V1))*(1.0-Vtot2/(2.0*a_v1));  // = dLambdadW;
    dLambda[3]  = (V1*RotorRadius)/Vtip2;                                                      // = dLambdadOmega;
    dLambda[4]  = v1h4/dLambdaRhoDen;                                                                                     // = dLambdadRho;
}

void RotorDisc::dT0Calc()
{
    // advance ratio
    doublereal c0 = 0.5*rho*Vtip2*DiscArea*RotorSolidity*Cl;
    doublereal c1Num = -(3.0*mu);
    doublereal c1Den = pow((1.0+1.5*mu2),2.0);
    doublereal c1 = c1Num/c1Den;
    doublereal c2 = c0*c1;

    // for dT0dOmega and dT0dRho
    doublereal c3 = 0.5*DiscArea*RotorSolidity*Cl;
    doublereal c4 = 1.0+1.5*mu2;
    doublereal c5 = c4*2.0*Vtip*RotorRadius;
    doublereal c6 = -3.0*Vtip2*mu;

    doublereal dT0dOmegaNum = rho*c3*(c5+c6*dMu[3]);
    doublereal dT0dOmegaDen = (pow(c4,2.0));

    dT0[0] = c2*dMu[0];             // = dT0dU;
    dT0[1] = c2*dMu[1];             // = dT0dV;
    dT0[2] = c2*dMu[2];             // = dT0dW;
    dT0[3] = dT0dOmegaNum/dT0dOmegaDen;   // = dT0dOmega;
    dT0[4] = c3*Vtip2/(c4);  // = dT0dRho;
}

void RotorDisc::dTThetaCalc()
{
    doublereal p0 = thetaColl*(6*mu2-4/3)*mu;

    dTTheta[0] = p0*dMu[0]; // = dTThetadU;
    dTTheta[1] = p0*dMu[1]; // = dTThetadV;
    dTTheta[2] = p0*dMu[2]; // = dTThetadW;
    dTTheta[3] = p0*dMu[3]; // = dTThetadOmega;
    dTTheta[4] = 0.0;       // = dTThetadRho;
}

void RotorDisc::dTLambdaCalc()
{
    doublereal l0 = 1.0-0.5*mu2;
    doublereal l1 = 1.5*(2.0*mu2-1.0)*mu*BladeTwist;
    doublereal l2 = l1-lambda*mu;

    dTLambda[0] = l0*dLambda[0] + l2*dMu[0];// = dTLambdadU
    dTLambda[1] = l0*dLambda[1] + l2*dMu[1];// = dTLambdadV
    dTLambda[2] = l0*dLambda[2] + l2*dMu[2];// = dTLambdadW
    dTLambda[3] = l0*dLambda[3] + l2*dMu[3];// = dTLambdadOmega
    dTLambda[4] = l0*dLambda[4];            // = dTLambdadRho
}

void RotorDisc::T0Calc()
{
    doublereal t0 = 0.5*rho*Vtip2*DiscArea*RotorSolidity*Cl;

    doublereal t1 = 1.0 + 1.5*mu2;
    // update T0
    T0 = t0/t1;

}

void RotorDisc::TThetaCalc()
{
    // update TTheta
    TTheta = (2.0/3.0 - 2.0/3.0*mu2 + 1.5*mu4)*thetaColl;
}

void RotorDisc::TLambdaCalc()
{
    doublereal i0 = (1.0-0.5*mu2);
    doublereal i1 = 0.5*(1.0-1.5*mu2 + 1.5*mu4);
    // update TLambda
    TLambda = i0*lambda + i1*BladeTwist;
}

void RotorDisc::ThrustCalc()
{
    //T = T0*(TTheta+TLambda);
    Thrust = (T0)*(TTheta+TLambda);
    OutputThrust[0] = 0.0;
    OutputThrust[1] = 0.0;
    // WARNING: THRUST IS APPLIED AS A FOLLOWER FORCE APPLIED ON THE LOCAL Z AXIS,
    // THE SIGN HAS YO BE CHANGED TO TAKE INTO ACCOUNT THE CORRECT DIRECTION OF APPLICATION
    OutputThrust[2] = Thrust;
    PowerCalc();
}

void RotorDisc::PowerCalc(){
    // disc actuator + blade element theory for required power and couple
    doublereal qDyn = rho*DiscArea*Vtip2;
    doublereal E;
    Ct = Thrust/qDyn;
    if (bWithDrag){
        Cd = Cd0+Cd1*thetaColl+Cd2*pow(thetaColl,2.0);
        E = Cl/Cd;        
        if (E==0.0){
            CoupleProfile   = 0.0;
            PowerProfile    = 0.0;
            CpProfile       = 0.0;
        }
        else{
            CoupleProfile   = 0.75*Thrust*RotorRadius/E;
            PowerProfile    = 0.75*Thrust*Vtip/E;
            CpProfile       = 0.75*Ct/E;
        }
    }
    else{

        CoupleProfile   = 0.0;
        PowerProfile    = 0.0;
        CpProfile       = 0.0;
    
    }
    // from L'Elicottero, M. Arra, pp. 50-60
    computeLambdaNewman();
    lambda = lambdaNewman;

    CoupleInduced   = Thrust*V1/RotorOmega;
    PowerInduced    = pow(Thrust,1.5)/sqrt(2.0*rho*DiscArea);
     
    CpInduced       = Ct*lambda;

    Couple = CoupleInduced + CoupleProfile;
    Power  = PowerInduced + PowerProfile;

    Cp = CpInduced+CpProfile;

    Cp == 0.0 ? FOM = 0.0 : FOM = pow(Ct,1.5)/(sqrt(2.0)*Cp);

    CtSigma = Ct/RotorSolidity;
    CpSigma = Cp/RotorSolidity;

    OutputCouple[0] = 0.0;
    OutputCouple[1] = 0.0;
    OutputCouple[2] = Couple;

}

void RotorDisc::dTCalc()
{
    // parameters constant for each partial derivative
    doublereal c0 = (TTheta + TLambda);
    doublereal c1 = T0;

    // update thrust partial derivatives
    int n = *(&Thrust + 1) - Thrust;
    for (int i=0; i<n; i++)
    {
        dThrust[i] = c0*dT0[i] + c1*(dTTheta[i] + dTLambda[i]);
    }
}

void RotorDisc::updateStatesDeps()
{
    u = VTrHub[0];
    v = VTrHub[1];
    w = VTrHub[2];

    Vtot    = sqrt(pow(u, 2.0) + pow(v, 2.0) + pow(w, 2.0));
    Vtot2   = pow(Vtot, 2.0);
    // Vtip
    Vtip    = RotorOmega*RotorRadius;
    Vtip2   = pow(Vtip,2.0);
    // adv ratio
    mu      = Vtot/Vtip;
    mu2     = pow(mu,2.0);
    mu4     = pow(mu,4.0);
    // v1h: induced velocity in hover = sqrt(Th/2A)*sqrt(1/rho)
    v1h = v1hPart*sqrt(1.0/rho);
    // CONSTANT MOMENTUM INDUCED VELOCITY
    a_v1 = sqrt(pow(0.5*Vtot2,2.0) + pow(v1h, 4.0));
    V1 = sqrt(-0.5*Vtot2 + a_v1);
    // alpha tip path plane
    alphaTPP = atan2(w,u);
    // inflow ratio
    lambda = V1/Vtip;

}

void RotorDisc::computeLambdaNewman(int itMax, doublereal tollMax){

    
    doublereal vsx,vsz; // components of the inflow along x,z
    doublereal dfdl, f, lambdaIt, lambdaItPrev; // function used to find lamba in newton iterations
    
    doublereal ct2 = 0.5*Ct;
    // initial guess is lambda just found out
    lambdaItPrev = isnan(lambda) ? 0.0 : lambda;
    // lambdaItPrev = lambda;
    //lambdaIt     = lambda;
    vsx = mu*cos(alphaTPP);

    //doublereal b1,c1;
    //b1 = 0.5*RotorSolidity*Cl/8.0;
    //c1 = -RotorSolidity*Cl/12.0*(thetaColl+0.75*BladeTwist);
    //lambdaIt = -b1+sqrt(pow(b1,2.0)-c1);

    doublereal tollLn = 10.0;
    int it=0;
    do {
        vsz = mu*sin(alphaTPP)+lambdaItPrev;
        f = lambdaItPrev-ct2*pow((pow(vsx,2.0)+pow(vsz,2.0)),-0.5);
        dfdl = 1+ct2*vsz*pow((pow(vsx,2.0)+pow(vsz,2.0)),-1.5);
        lambdaIt = lambdaItPrev - f/dfdl;
        tollLn = abs(lambdaIt-lambdaItPrev);
        lambdaItPrev = lambdaIt;
        it++;
    } while ((it<=itMax) && (tollLn>=tollMax));

    lambdaNewman = lambdaIt;

}

// compute cl with stall effects
void RotorDisc::computeCLInit(doublereal RDecayIn)
{
    Cl = ClAlpha*thetaColl+Cl0;
    if (bWithStallLimits){

        // stall effect: circumference tangent to linear cl in clmax point
        clMax = ClAlpha*AOAStallMax+Cl0;
        clMin = ClAlpha*AOAStallMin+Cl0;

        xpMin = AOAStallMin;
        ypMin = clMin;
        xpMax = AOAStallMax;
        ypMax = clMax;

        // stall decay + "delay" modeled by circumference of radius R (assigned here)
        RDecay=RDecayIn*acos(-1.0)/180.0;
        // TODO: perfezionare stall decay+delay
        a1=atan(-1/ClAlpha);
        // circ center (max alpha)
        x0Min = xpMin-RDecay*cos(a1);
        y0Min = ypMin-RDecay*sin(a1);

        x0Max = xpMax+RDecay*cos(a1);
        y0Max = ypMax+RDecay*sin(a1);

        xbMin = xpMin-2*RDecay*cos(a1);
        ybMin = ypMin;

        xbMax = xpMax+2*RDecay*cos(a1);
        ybMax = ypMax;

        mb = 1/tan(a1);
        qbMin = ybMin-mb*xbMin;
        qbMax = ybMax-mb*xbMax;

        AOAAfterDecayMin = xbMin;
        AOAAfterDecayMax = xbMax;
    }
    
}

void RotorDisc::computeCL(){
    if (bWithStallLimits){
        // for simplicity, the input is thetacoll
        if (thetaColl < AOAAfterDecayMin){
            // after decay min phase
            Cl=mb*thetaColl+qbMin;
        }
        else if ((AOAAfterDecayMin <= thetaColl) and (thetaColl < AOAStallMin)){
            // decay min phase
            Cl=y0Min-sqrt(pow(RDecay,2.0)-pow((thetaColl-x0Min),2.0));
        }
        else if (thetaColl > AOAAfterDecayMax){
            // after decay max phase
            Cl=mb*thetaColl+qbMax;
        }
        else if ((AOAStallMax < thetaColl) and (thetaColl <= AOAAfterDecayMax)){
            // decay max phase
            Cl=y0Max+sqrt(pow(RDecay,2.0)-pow((thetaColl-x0Max),2.0));
        }
        else {
            // linear field
            Cl=ClAlpha*thetaColl+Cl0;
        }
    }
    else{
        Cl=ClAlpha*thetaColl+Cl0;
    }
}

void RotorDisc::inputSaturation()
{
    // check if collective input saturation    
    if (bWithCollSaturation){
        // the minimum value between input and saturation upper limit
        thetaColl = std::min(thetaColl, thetaCollMax);
        // the maximum value between input and saturation lower limit
        thetaColl = std::max(thetaColl, thetaCollMin);
    }
}

void RotorDisc::assemblyJacobian()
{
    // compute partial derivatives of adv ratio
    dMuCalc();
    // compute partial derivatives of inflow ratio
    dLambdaCalc();
    // compute partial derivatives components of the jacobian
    dT0Calc();
    dTThetaCalc();
    dTLambdaCalc();
    // compute the components of thrust (and thrust itself)
    computeRotorThrust(); // gives also the value of induced power and drag
    // and finally compute Jacobian of tail rotor thrust
    dTCalc();
}


void RotorDisc::computeRotorThrust()
{
    // compute the components of thrust (and thrust itself)
    T0Calc();
    TThetaCalc();
    TLambdaCalc();
    // compute thrust
    ThrustCalc();
}

////////////////////////////////////////////////////////////////////////////////////
void RotorDisc::WorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
	*piNumRows = 6;
	*piNumCols = 3;
}

void RotorDisc::InitialWorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
	*piNumRows = 12;
	*piNumCols = 6;
}

//contributo al file di restart
std::ostream& RotorDisc::Restart(std::ostream& out) const
{
    return out << "# not implemented yet" << std::endl;
}

VariableSubMatrixHandler&
RotorDisc::AssJac(VariableSubMatrixHandler& Workmat,
                        doublereal dCoef,
                        const VectorHandler& XCurr,
                        const VectorHandler& XPrimeCurr)
{
    // sicuri sicuri che qua non va niente?
    Workmat.SetNullMatrix();
    return Workmat;
    /*
    FullSubMatrixHandler& WM = Workmat.SetFull();
    // reset and workmatrix dimension
    integer iNumRows = 0;
    integer iNumCols = 0;
    WorkSpaceDim(&iNumRows, &iNumCols);
    WM.ResizeReset(iNumRows, iNumCols);

    integer iFirstRotationIndex = pHubNode->iGetFirstPositionIndex() + 3;
    integer iFirstMomentumIndex = pHubNode->iGetFirstMomentumIndex();
    for (integer iCnt = 1; iCnt <= 3; iCnt++)
    {
        WM.PutRowIndex(iCnt, iFirstMomentumIndex + iCnt); // force
        WM.PutRowIndex(3 + iCnt, iFirstMomentumIndex + 3 + iCnt); // couple
        WM.PutColIndex(iCnt, iFirstRotationIndex + iCnt); // rotation
    }



    // data
    const Mat3x3& R(pHubNode->GetRRef());
    Vec3 TmpDir(R*(f.Get()*dCoef));
    Vec3 TmpArm(R*HubNodeArm);
    /// |    F/\   |           |   F  |
	 // |          | Delta_g = |      |
	 // | (d/\F)/\ |           | d/\F |
	 ///
    WM.Add(1, 1, Mat3x3(MatCross, TmpDir));
    WM.Add(4, 1, Mat3x3(MatCross, TmpArm.Cross(TmpDir)));

    return Workmat;
    */

}

unsigned int RotorDisc::iGetNumPrivData(void) const
{

    // thetaColl           [rad]
    // rotorOmega          [rad/s]
    // Thrust              [N]
    // CoupleInduced       [Nm]
    // CoupleProfile       [Nm]
    // Couple              [Nm]
    // PowerInduced        [W]
    // PowerProfile        [W]
    // Power               [W]
    // PowerInducedHover   [W]
    // PowerProfileHover   [W]
    // PowerHover          [W]
    // alphaTPP            [rad]
    // rho                 [kg/m3]
    // lambda              [-]
    // V1                  [m/s]
    // v1h                 [m/s]
    // VTrHubX             [m/s]
    // VTrHubY             [m/s]
    // VTrHubZ             [m/s]
    // CpInduced           [-]
    // CpProfile           [-]
    // Cp                  [-]
    // CpSigma             [-]
    // Ct                  [-]
    // CtSigma             [-]
    // FOM                 [-]

    // number of private data that can be extracted from the module
    return 27;
}

unsigned int RotorDisc::iGetPrivDataIdx(const char* s) const
{

    ASSERT(s != NULL);

    struct
    {
        const char* s;
        int i;
    } sPrivData[] = {

        {"theta",             PRIV_THETACOLL},
        {"omega",             PRIV_OMEGA},
        {"thrust",            PRIV_THRUST},
        {"coupleinduced",     PRIV_COUPLEINDUCED},
        {"coupleprofile",     PRIV_COUPLEPROFILE},
        {"couple",            PRIV_COUPLE},
        {"powerinduced",      PRIV_POWERINDUCED},
        {"powerprofile",      PRIV_POWERPROFILE},
        {"power",             PRIV_POWER},
        {"powerinducedhover", PRIV_POWERINDUCEDHOVER},
        {"powerprofilehover", PRIV_POWERPROFILEHOVER},
        {"powerhover",        PRIV_POWERHOVER},
        {"alphatpp",          PRIV_ALPHATPP},
        {"rho",               PRIV_RHO},
        {"lambda",            PRIV_LAMBDA},
        {"v1",                PRIV_V1},
        {"v1h",               PRIV_V1H},
        {"vhubx",             PRIV_VHUBX},
        {"vhuby",             PRIV_VHUBY},
        {"vhubz",             PRIV_VHUBZ},
        {"cpinduced",         PRIV_CPINDUCED},
        {"cpprofile",         PRIV_CPPROFILE},
        {"cp",                PRIV_CP},
        {"cpsigma",           PRIV_CPSIGMA},
        {"ct",                PRIV_CT},
        {"ctsigma",           PRIV_CTSIGMA},
        {"fom",               PRIV_FOM},
        {0}

    };

    for (int i = 0; sPrivData[i].s != 0; i++)
    {
        if (strcasecmp(s, sPrivData[i].s) == 0)
        {
            return sPrivData[i].i;
        }
    }

    return 0;

}

doublereal RotorDisc::dGetPrivData(unsigned int i) const
{
    if (i <= 0 || i >= LASTPRIVDATA)
    {
        silent_cerr("rotordisc("<<GetLabel()<<"): "
        "private data "<< i << "not available" << std::endl);
        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
    }
    else
    {
        switch (i)
        {
            case PRIV_THETACOLL:            return thetaColl;
            case PRIV_OMEGA:                return RotorOmega;
            case PRIV_THRUST:               return Thrust;
            case PRIV_COUPLEINDUCED:        return CoupleInduced;
            case PRIV_COUPLEPROFILE:        return CoupleProfile;
            case PRIV_COUPLE:               return Couple;
            case PRIV_POWERINDUCED:         return PowerInduced;
            case PRIV_POWERPROFILE:         return PowerProfile;
            case PRIV_POWER:                return Power;
            case PRIV_POWERINDUCEDHOVER:    return PowerInducedHover;
            case PRIV_POWERPROFILEHOVER:    return PowerProfileHover;
            case PRIV_POWERHOVER:           return PowerHover;
            case PRIV_ALPHATPP:             return alphaTPP;
            case PRIV_RHO:                  return rho;
            case PRIV_LAMBDA:               return lambda;
            case PRIV_V1:                   return V1;
            case PRIV_V1H:                  return v1h;
            case PRIV_VHUBX:                return VTrHub[0];
            case PRIV_VHUBY:                return VTrHub[1];
            case PRIV_VHUBZ:                return VTrHub[2];
            case PRIV_CPINDUCED:            return CpInduced;
            case PRIV_CPPROFILE:            return CpProfile;
            case PRIV_CP:                   return Cp;
            case PRIV_CPSIGMA:              return CpSigma;
            case PRIV_CT:                   return Ct;
            case PRIV_CTSIGMA:              return CtSigma;
            case PRIV_FOM:                  return FOM;

        }
    }

    return 0.;
}

int RotorDisc::iGetNumConnectedNodes(void) const
{
    // hub node
    return 1;
}

void RotorDisc::GetConnectedNodes(std::vector<const Node *>& connectedNodes) const
{
    // hub node
    connectedNodes.resize(1);
    connectedNodes[0] = pHubNode;
}

void RotorDisc::SetValue(DataManager *pDM,
	VectorHandler& X, VectorHandler& XP,
	SimulationEntity::Hints *ph)
{
   	NO_OP;
}

unsigned RotorDisc::iGetInitialNumDof(void) const
{
    return 0;
}

SubVectorHandler&
RotorDisc::AssRes(SubVectorHandler& WorkVec,
                        doublereal dCoef,
                        const VectorHandler& XCurr,
                        const VectorHandler& XPrimeCurr)
{
    integer iNumRows;
    integer iNumCols;
    WorkSpaceDim(&iNumRows, &iNumCols);
    WorkVec.ResizeReset(iNumRows);
    // indices of node unkowns
    integer iFirstMomentumIndex = pHubNode->iGetFirstMomentumIndex();
    for (integer iCnt = 1; iCnt <= 6; iCnt++){
         WorkVec.PutRowIndex(iCnt, iFirstMomentumIndex + iCnt);
    }

    // get velocity of the hub point in the reference frame of the node
    Vec3 VTrHubTemp = pHubNode->GetVCurr() + pHubNode->GetWCurr().Cross(HubNodeArm);
    // rotate the velocity in the reference frame of the rotor disc
    VTrHub = RThrustOrientation*VTrHubTemp;
    // the force is oriented along the ideal z of node reference frame
    // air density and rotor angular velocity
    RotorOmega = pOmega->dGet();
    // air density taken by exploiting class inheritance on aerodynamic element
    rho = dGetAirDensity(pHubNode->GetXCurr());
    // rotor collective pitch input
    thetaColl   = pXColl->dGet();
    // check for saturation
    inputSaturation();
    // compute CL with stall effects
    computeCL();
    // update state-dependent parameters (lambda, mu, ecc)
    updateStatesDeps();
    // compute rotor thrust (direction wrt to node hub given by constructor)
    computeRotorThrust();
    // OutputThrust = Zero3;
    const Mat3x3& R(pHubNode->GetRCurr());
    // bring back rotor disc forces to node reference frame
    OutputThrust = RThrustOrientation.MulTV(OutputThrust);
    // from node reference frame to global reference frame
    F = -R*OutputThrust;
    // M = thrust effect cross arm + couple 
    M = -R*(HubNodeArm.Cross(OutputThrust)+OutputCouple);

    WorkVec.Add(1, F);
    WorkVec.Add(4, M);

    return WorkVec;
}

VariableSubMatrixHandler&
RotorDisc::InitialAssJac(VariableSubMatrixHandler& WorkMat,
                                const VectorHandler& XCurr)
{
    WorkMat.SetNullMatrix();
    return WorkMat;
    /*
    FullSubMatrixHandler& WM = WorkMat.SetFull();
    // workmatrix reset
    WM.ResizeReset(12, 6);

    integer iFirstPositionIndex = pHubNode->iGetFirstPositionIndex();
    integer iFirstVelocityIndex = iFirstPositionIndex + 6;
    for (integer iCnt = 1; iCnt <= 3; iCnt)
    {
        WM.PutRowIndex(iCnt, iFirstPositionIndex + iCnt);
		WM.PutRowIndex(3 + iCnt, iFirstPositionIndex + 3 + iCnt);
		WM.PutRowIndex(6 + iCnt, iFirstVelocityIndex + iCnt);
		WM.PutRowIndex(9 + iCnt, iFirstVelocityIndex + 3 + iCnt);
		WM.PutColIndex(iCnt, iFirstPositionIndex + 3 + iCnt);
		WM.PutColIndex(3 + iCnt, iFirstVelocityIndex + 3 + iCnt);
    }

    // data
	const Mat3x3& R(pHubNode->GetRRef());
	Vec3 TmpArm(R*HubNodeArm);
	Vec3 TmpDir = R*f.Get();
	const Vec3& Omega(pHubNode->GetWRef());

	/// |    F/\   |           |   F  |
	 // |          | Delta_g = |      |
	 // | (d/\F)/\ |           | d/\F |
	 ///

	WM.Add(1, 1, Mat3x3(MatCross, TmpDir));
	WM.Add(4, 1, Mat3x3(MatCross, TmpArm.Cross(TmpDir)));
	WM.Add(7, 1, Mat3x3(MatCrossCross, Omega, TmpDir));
	WM.Add(7, 4, Mat3x3(MatCross, TmpDir));
	WM.Add(10, 1, Mat3x3(MatCrossCross, Omega, TmpArm.Cross(TmpDir)));
	WM.Add(10, 4, Mat3x3(MatCross, TmpArm.Cross(TmpDir)));

	return WorkMat;
    */
}

SubVectorHandler&
RotorDisc::InitialAssRes(SubVectorHandler& WorkVec, const VectorHandler& XCurr)
{
    WorkVec.Resize(0);
    return WorkVec;
    /*
    integer iNumRows;
	integer iNumCols;
	InitialWorkSpaceDim(&iNumRows, &iNumCols);
	WorkVec.ResizeReset(iNumRows);

	// Indici delle incognite del nodo
	integer iFirstPositionIndex = pHubNode->iGetFirstPositionIndex();
	integer iFirstVelocityIndex = iFirstPositionIndex + 6;
	for (integer iCnt = 1; iCnt <= 6; iCnt++) {
		WorkVec.PutRowIndex(iCnt, iFirstPositionIndex + iCnt);
		WorkVec.PutRowIndex(6 + iCnt, iFirstVelocityIndex + iCnt);
	}

	//Dati
	const Mat3x3& R(pHubNode->GetRCurr());
	Vec3 TmpDir(R*f.Get());
	Vec3 TmpArm(R*HubNodeArm);
	const Vec3& Omega(pHubNode->GetWCurr());

	WorkVec.Add(1, TmpDir);
	WorkVec.Add(4, TmpArm.Cross(TmpDir));
	WorkVec.Add(7, Omega.Cross(TmpDir));
	WorkVec.Add(10, (Omega.Cross(TmpArm)).Cross(TmpDir)
		+ TmpArm.Cross(Omega.Cross(TmpDir)));

	return WorkVec;
    */
}

bool RotorDisc_set(void)
{
    UserDefinedElemRead *rf = new UDERead<RotorDisc>;

    if (!SetUDE("rotordisc", rf))
    {
        delete rf;
        return false;
    }

    return true;
}

//#ifndef STATIC_MODULES
extern "C" int
module_init(const char *module_name, void *pdm, void *php)
{
    if (!RotorDisc_set())
    {
        silent_cerr("rotordisc: "
                    "module_init("<< module_name << ") "
                    "failed" << std::endl);
        return -1;
    }

    return 0;

}
//#endif // ! STATIC_MODULES
