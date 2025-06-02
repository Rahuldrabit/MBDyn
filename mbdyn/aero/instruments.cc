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

/* Aircraft Instruments */

#include "dataman.h"
#include "instruments.h"
#include <stdlib.h>
#include <cmath>

AircraftInstruments::AircraftInstruments(unsigned int uLabel, const DofOwner* pDO,
	const StructNode* pN, const Mat3x3 &R, flag fOut, doublereal initLong, doublereal initLat, doublereal earth_radius)
: Elem(uLabel, fOut), AerodynamicElem(pDO),
pNode(pN),
Rh(R),
earth_radius(earth_radius),
dAttitudePrev(0.),
dBankPrev(0.)
//,dHeadingPrev(0.)
{
	memset(&dMeasure[0], 0, sizeof(dMeasure));
	/*Luca Conti edits--------------------------------------------------------*/
	dMeasure[INIT_LATITUDE] = initLat;
	dMeasure[INIT_LONGITUDE] = initLong;
	normalizeGeoCoordinates();
	const Vec3& X(pNode->GetXCurr());
	dMeasure[INIT_X2] = X(2);
	dMeasure[INIT_X1] = X(1);
	/*------------------------------------------------------------------------*/
	/*Matteo Daniele edits--------------------------------------------------------*/
	const Vec3& LINACC(pNode->GetXPPCurr());
	const Vec3& ANGACC(pNode->GetWPCurr());
	dMeasure[NODE_BODY_ACC_X] = LINACC(1);
	dMeasure[NODE_BODY_ACC_Y] = LINACC(2);
	dMeasure[NODE_BODY_ACC_Z] = LINACC(3);
	dMeasure[NODE_BODY_ACC_P] = ANGACC(1);
	dMeasure[NODE_BODY_ACC_Q] = ANGACC(2);
	dMeasure[NODE_BODY_ACC_R] = ANGACC(3);
	/*------------------------------------------------------------------------*/

	
}

AircraftInstruments::~AircraftInstruments(void)
{
	NO_OP;
}

Elem::Type
AircraftInstruments::GetElemType(void) const
{
	return Elem::AERODYNAMIC;
}

void
AircraftInstruments::Update(void)
{
	const Vec3& X(pNode->GetXCurr());
	const Mat3x3 R(pNode->GetRCurr()*Rh);
	const Vec3& V(pNode->GetVCurr());
	const Vec3& Omega(pNode->GetWCurr());
	Vec3 VV = V;

	const float toll = 1e-6;			//Di Lallo Luigi edits

	// body axes expressed in "world" reference frame
	const Vec3 e1(R.GetVec(1));
	const Vec3 e2(R.GetVec(2));
	const Vec3 e3(R.GetVec(3));

	/*
	 * Assumptions:
	 * - world is "flat"
	 * - world "z" is positive upward
	 * (flight mechanics conventions?)
	 * - aircraft "x" is positive forward
	 * - aircraft "z" is positive downward
	 * - aircraft "y" is positive towards the right of the pilot
	 * - pitch is positive nose up
	 * - roll is positive right wing down
	 * - yaw is positive right wing backward
	 */

	// add wind velocity (wind evaluated at aircraft node, not at pitot's location!)
	Vec3 VecTmp(Zero3);
	if (fGetAirVelocity(VecTmp, X)) {
		VV -= VecTmp;
	}

	// physical parameters
	doublereal rho, c, p, T;
	GetAirProps(X, rho, c, p, T);
	dMeasure[DENSITY] = rho;
	dMeasure[SOUND_CELERITY] = c;
	dMeasure[STATIC_PRESSURE] = p;
	dMeasure[TEMPERATURE] = T;

	// airspeed (norm of aircraft + wind velocity)
	dMeasure[AIRSPEED] = VV.Norm(); //m/s

	// groundspeed (norm of horizontal component of velocity at aircraft node)
	VecTmp = V;
	VecTmp(3) = 0.;
	dMeasure[GROUNDSPEED] = VecTmp.Norm();  //m/s

	// altitude (vertical component of node position)
	dMeasure[ALTITUDE] = X(3); 

	/* attitude */

	/*NB: maybe this new version should be fine for every possible spatial orientation of the body frame.
	Previous calculation of ATTITUDE did not take into account when x axis of the body frame is aligned
	with world "y".
	In that case atan2 generates a domain error (both arguments zero)*/

	// x axis of the body frame aligned with "world" y: attitude is 0
	doublereal dAttitudeCurr = std::asin(e1(3));
	if (std::abs(dAttitudeCurr - dAttitudePrev) >= (M_PI_2 - toll)) {
		dMeasure[ATTITUDE] = dAttitudeCurr + sign(dAttitudePrev)*M_PI_2;
	} else {
		dMeasure[ATTITUDE] = dAttitudeCurr;
	}
	dAttitudePrev = dAttitudeCurr;
	
	/* bank */

	/* NB: maybe this new version should be fine for every possible spatial orientation of the body frame.
	Previous calculation of BANK did not take into account when y axis of the body frame is aligned with "world" x.
	In that case atan2 generates a domain error (both arguments zero) */
	
	// y axis of the body frame aligned with "world" x: bank is 0
	doublereal dBankCurr = std::asin(-e2(3)); 
	if (std::abs(dBankCurr - dBankPrev) >= (M_PI_2 - toll)) {
		dMeasure[BANK] = dBankCurr + sign(dBankPrev)*M_PI_2;
	} else {
		dMeasure[BANK] = dBankCurr;
	}
	dBankPrev = dBankCurr;
	
	// turn (vertical component of angular velocity)
	/* Luca Conti edits */
	dMeasure[TURN] = Omega(3)*60; /* turn rate in rad/min */

	// slip
	// TODO: should be in body frame, right?!?
#if 0
	/*Luca Conti edits-----------------*/
	// velocity aligned with z body frame! aircraft sliding vertically... strange, but might occur
	if (std::abs(VV(2)) <= toll && std::abs(VV(1)) <= toll) {		//Di Lallo Luigi edits
		dMeasure[SLIP] = 0;

	} else {
		dMeasure[SLIP] = std::atan2(VV(2), VV(1));
	}
	/* SLIP is positive if the aircraft nose is tilted leftward wrt velocity vector, spans -PI;+PI */
	/*---------------------------------*/
#endif

	// vertical speed as z component of velocity vector in absolute frame
	dMeasure[VERTICALSPEED] = VV(3);

	// velocity vector in body frame
	VecTmp = R.MulTV(VV);

	// angle of attack
	if (dMeasure[AIRSPEED] < toll) {
		dMeasure[AOA] = 0.;

	} else {
		dMeasure[AOA] = -std::atan2(VecTmp(3), VecTmp(1));
	}

	// sideslip
	if (dMeasure[AIRSPEED] < toll) {
		dMeasure[SLIP] = 0.;

	} else {
		dMeasure[SLIP] = -std::atan2(VecTmp(2), VecTmp(1));
	}

	// node velocity components in body frame
	dMeasure[NODE_BODY_VX] = VecTmp(1);
	dMeasure[NODE_BODY_VY] = VecTmp(2);
	dMeasure[NODE_BODY_VZ] = VecTmp(3);

	// heading

	/* x axis body frame aligned with vertical direction! aircraft nose pointing upward or downward */
	if (std::abs(e1(2)) <= toll &&  std::abs(e1(1)) <= toll) {
		dMeasure[HEADING] = 0;

	} else {
		dMeasure[HEADING] = -std::atan2(e1(2), e1(1));
	}
	/* in order to have HEADING between 0° and 360° */
	while (dMeasure[HEADING] < 0) {
		dMeasure[HEADING] += 2*M_PI;
	}


	// longitude
	/* Luca Conti edits-------------------------------------------------- */
	/* NB: "world" y axis points towards W, LONGITUDE grows towards E */
	dMeasure[LONGITUDE] = dMeasure[INIT_LONGITUDE] + (-X(2) - (-dMeasure[INIT_X2]))/earth_radius;
	/* ------------------------------------------------------------------ */

	// latitude
	/* Luca Conti edits-------------------------------------------------- */
	dMeasure[LATITUDE] = dMeasure[INIT_LATITUDE] + (X(1) - dMeasure[INIT_X1])/earth_radius;
	/* ------------------------------------------------------------------ */

	/* Luca Conti edits */
	/* checks if LONGITUDE and LATITUDE exceed their standard value: in that case, the proper value is recalculated */
	normalizeGeoCoordinates();

	VecTmp = R.MulTV(Omega);
	dMeasure[ROLLRATE] = VecTmp(1);
	dMeasure[PITCHRATE] = VecTmp(2);
	dMeasure[YAWRATE] = VecTmp(3);

	const DynamicStructNode* pDSN = dynamic_cast<const DynamicStructNode *>(pNode);
	if (pDSN) {
		/* Matteo Daniele edits */
		const Vec3& LINACC(pNode->GetXPPCurr());
		const Vec3& ANGACC(pNode->GetWPCurr());
		dMeasure[NODE_BODY_ACC_X] = LINACC(1);
		dMeasure[NODE_BODY_ACC_Y] = LINACC(2);
		dMeasure[NODE_BODY_ACC_Z] = LINACC(3);
		dMeasure[NODE_BODY_ACC_P] = ANGACC(1);
		dMeasure[NODE_BODY_ACC_Q] = ANGACC(2);
		dMeasure[NODE_BODY_ACC_R] = ANGACC(3);
	}
	/* ------------------------------------------------------------------ */

}

void
AircraftInstruments::normalizeGeoCoordinates(void)
{
	// checks if LONGITUDE exceeds -180/180: in that case, the proper value is recalculated
	if (dMeasure[LONGITUDE] > M_PI || dMeasure[LONGITUDE] < -M_PI) {
		int n = (int)dMeasure[LONGITUDE]/M_PI;
		if (n % 2 != 0) {
			dMeasure[LONGITUDE] = dMeasure[LONGITUDE] - (n + std::abs(n)/n)*M_PI;
		} else {
			dMeasure[LONGITUDE] = dMeasure[LONGITUDE] - n*M_PI;
		}
	}

	// checks if LATITUDE exceeds -90/90: in that case, the proper value is recalculated
	if (dMeasure[LATITUDE] > M_PI/2 || dMeasure[LATITUDE] < -M_PI/2) {
		int n = (int) dMeasure[LATITUDE]/M_PI;
		if (n % 2 != 0) {
			dMeasure[LATITUDE] = -dMeasure[LATITUDE] + n*M_PI;
			if (std::abs(dMeasure[LATITUDE]) > M_PI/2) {
				dMeasure[LATITUDE] = -dMeasure[LATITUDE] - sign(n)*M_PI;
			}
		} else {
			dMeasure[LATITUDE] = dMeasure[LATITUDE] - n*M_PI;
			if (std::abs(dMeasure[LATITUDE]) > M_PI/2) {
				dMeasure[LATITUDE] = -dMeasure[LATITUDE] + sign(n)*M_PI;
			}
		}
	}
}

/* Scrive il contributo dell'elemento al file di restart */
std::ostream&
AircraftInstruments::Restart(std::ostream& out) const
{
	out << "aircraft instruments: " << GetLabel()
		<< ", " << pNode->GetLabel()
		<< ", orientation, ", Rh.Write(out)
		<< ";" << std::endl;

	return out;
}

void
AircraftInstruments::Output(OutputHandler& OH) const
{
	if (bToBeOutput() && OH.UseText(OutputHandler::AERODYNAMIC)) {
		std::ostream& out = OH.Aerodynamic()
			<< std::setw(8) << GetLabel();

		for (int iCnt = 1; iCnt < LASTMEASURE; iCnt++) {
			out << " " << dMeasure[iCnt];
		}

		out << std::endl;
	}

	// TODO: NetCDF output...
}

/* Dimensioni del workspace */
void
AircraftInstruments::WorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
	*piNumRows = 0;
	*piNumCols = 0;
}

/* assemblaggio jacobiano */
VariableSubMatrixHandler&
AircraftInstruments::AssJac(VariableSubMatrixHandler& WorkMat,
	doublereal /* dCoef */ ,
	const VectorHandler& /* XCurr */ ,
	const VectorHandler& /* XPrimeCurr */ )
{
	DEBUGCOUTFNAME("AircraftInstruments::AssJac");

	WorkMat.SetNullMatrix();

	return WorkMat;
}

/* assemblaggio residuo */
SubVectorHandler&
AircraftInstruments::AssRes(SubVectorHandler& WorkVec,
	doublereal dCoef,
	const VectorHandler& XCurr,
	const VectorHandler& XPrimeCurr)
{
	WorkVec.Resize(0);

	Update();

	return WorkVec;
}

/* Dati privati */
unsigned int
AircraftInstruments::iGetNumPrivData(void) const
{
	return LASTMEASURE-1;
}

unsigned int
AircraftInstruments::iGetPrivDataIdx(const char *s) const
{
	ASSERT(s != NULL);

	struct {
		const char *s;
		int i;
		bool bDynOnly;
	} s2i[] = {
		{ "airspeed", AIRSPEED, false },
		{ "ground" "speed", GROUNDSPEED, false },
		{ "altitude", ALTITUDE, false },
		{ "attitude", ATTITUDE, false },
		{ "bank", BANK, false },
		{ "turn", TURN, false },
		{ "slip", SLIP, false },
		{ "sideslip", SLIP, false },
		{ "vertical" "speed", VERTICALSPEED, false },
		{ "angle" "of" "attack", AOA, false },
		{ "aoa", AOA, false },
		{ "heading", HEADING, false },
		{ "init_x1", INIT_X1, false },/*Luca Conti edits*/
		{ "init_x2", INIT_X2, false },/*Luca Conti edits*/
		{ "init_longitude", INIT_LONGITUDE, false },/*Luca Conti edits: to be specified by the user in the element, optional*/
		{ "longitude", LONGITUDE, false },
		{ "init_latitude", INIT_LATITUDE, false },/*Luca Conti edits: to be specified by the user in the element, optional*/
		{ "latitude", LATITUDE, false },
		{ "rollrate", ROLLRATE, false },
		{ "pitchrate", PITCHRATE, false },
		{ "yawrate", YAWRATE, false },
		{ "body_axb", NODE_BODY_ACC_X, true },	// Matteo Daniele edits
		{ "body_ayb", NODE_BODY_ACC_Y, true },	// Matteo Daniele edits
		{ "body_azb", NODE_BODY_ACC_Z, true },	// Matteo Daniele edits
		{ "body_pd", NODE_BODY_ACC_P, true },		// Matteo Daniele edits
		{ "body_qd", NODE_BODY_ACC_Q, true },		// Matteo Daniele edits
		{ "body_rd", NODE_BODY_ACC_R, true },		// Matteo Daniele edits
		// PM Feb 2024
		{ "body_vx", NODE_BODY_VX, false },
		{ "body_vy", NODE_BODY_VY, false },
		{ "body_vz", NODE_BODY_VZ, false },
		{ "density", DENSITY, false },
		{ "sound" "celerity", SOUND_CELERITY, false },
		{ "static" "pressure", STATIC_PRESSURE, false },
		{ "temperature", TEMPERATURE, false },

		{ 0 }
	};

	for (int i = 0; s2i[i].s != 0; i++) {
		if (strcasecmp(s, s2i[i].s) == 0) {
			if (s2i[i].bDynOnly && dynamic_cast<const DynamicStructNode *>(pNode) == 0) {
				silent_cerr("AircraftInstruments(" << GetLabel() << "): "
					"measure \"" << s2i[i].s << "\" only available when attached to a dynamic node, not with Node(" << pNode->GetLabel() << ")" << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			return s2i[i].i;
		}
	}

	return 0;
}

doublereal
AircraftInstruments::dGetPrivData(unsigned int i) const
{
	if (i <= 0 || i >= LASTMEASURE) {
		silent_cerr("AircraftInstruments(" << GetLabel() << "): "
			"illegal measure " << i << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	return dMeasure[i];
}

void
AircraftInstruments::GetConnectedNodes(std::vector<const Node *>& connectedNodes) const
{
	connectedNodes.resize(1);
	connectedNodes[0] = pNode;
}

const OutputHandler::Dimensions 
AircraftInstruments::GetEquationDimension(integer index) const {
	// DOF is unknown
	return OutputHandler::Dimensions::UnknownDimension;
}

std::ostream&
AircraftInstruments::DescribeEq(std::ostream& out, const char *prefix, bool bInitial) const
{

	out
		<< "It does not have any DOF" << std::endl;

	return out;
}

Elem *
ReadAircraftInstruments(DataManager* pDM, MBDynParser& HP,
	const DofOwner *pDO, unsigned int uLabel)
{
	Elem *pEl = NULL;

	const StructNode* pNode = pDM->ReadNode<const StructNode, Node::STRUCTURAL>(HP);
	Mat3x3 R = Eye3;
	if (HP.IsKeyWord("orientation")) {
		if (HP.IsKeyWord("flight" "mechanics")) {
			R = ::Eye3;

		} else if (HP.IsKeyWord("aeroelasticity")) {
			R = Mat3x3(-1., 0., 0., 0., 1., 0., 0., 0., -1.);
		
		} else {
			R = HP.GetRotRel(ReferenceFrame(pNode));
		}
	}

	/* Luca Conti edits ------------------------------------------------- */
	doublereal initLat = 0.;
	doublereal initLong = 0.;
	// default value in meters
	doublereal earth_radius = 6371005.;

	if (HP.IsKeyWord("initial" "latitude")){
		initLat = HP.GetReal();
	}

	if (HP.IsKeyWord("initial" "longitude")){
		initLong = HP.GetReal();
	}

	if (HP.IsKeyWord("earth" "radius")){
		earth_radius = HP.GetReal();
	}
	/* ------------------------------------------------------------------ */

	flag fOut = pDM->fReadOutput(HP, Elem::AERODYNAMIC);

	SAFENEWWITHCONSTRUCTOR(pEl, AircraftInstruments,
		AircraftInstruments(uLabel, pDO, pNode, R, fOut, initLong, initLat, earth_radius));

	return pEl;
}
