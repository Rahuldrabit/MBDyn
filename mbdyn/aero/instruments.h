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

#ifndef INSTRUMENTS_H
#define INSTRUMENTS_H

// strumenti di volo

#include "aerodyn.h"

class AircraftInstruments : public Elem, public AerodynamicElem {
public:
	enum Measure {
		AIRSPEED = 1,
		GROUNDSPEED,
		ALTITUDE,
		ATTITUDE,
		BANK,
		TURN,
		SLIP,
		VERTICALSPEED,
		AOA,
		HEADING,
		INIT_X1, // Luca Conti edits - initial x coordinate
		INIT_X2, // Luca Conti edits - initial y coordinate
		INIT_LONGITUDE, // Luca Conti edits: to be specified by the user in the element, optional
		LONGITUDE,
		INIT_LATITUDE, // Luca Conti edits: to be specified by the user in the element, optional
		LATITUDE,

		ROLLRATE,
		PITCHRATE,
		YAWRATE,

		NODE_BODY_ACC_X,	// Matteo Daniele edits - node linear accelerations (body accelerations)
		NODE_BODY_ACC_Y,	// Matteo Daniele edits - node linear accelerations (body accelerations)
		NODE_BODY_ACC_Z,	// Matteo Daniele edits - node linear accelerations (body accelerations)
		
		NODE_BODY_ACC_P,	// Matteo Daniele edits - node angular rate variations (body accelerations)
		NODE_BODY_ACC_Q,	// Matteo Daniele edits - node angular rate variations (body accelerations)
		NODE_BODY_ACC_R,	// Matteo Daniele edits - node angular rate variations (body accelerations)

		NODE_BODY_VX,		// node velocity components in body frame
		NODE_BODY_VY,		// node velocity components in body frame
		NODE_BODY_VZ,		// node velocity components in body frame

		DENSITY,
		SOUND_CELERITY,
		STATIC_PRESSURE,
		TEMPERATURE,

		LASTMEASURE
	};

protected:
	const StructNode* pNode;
	Mat3x3 Rh;

	doublereal dMeasure[LASTMEASURE];
	/* Luca Conti edits */
	// optionally chosen by the user in order to let him use any unit of measurement (set by default in meters if not differently specified)
	doublereal earth_radius;
	doublereal dAttitudePrev;
	doublereal dBankPrev;
	//doublereal dHeadingPrev;

	using SimulationEntity::Update;
	void Update(void);

	void normalizeGeoCoordinates(void);

public:
	AircraftInstruments(unsigned int uLabel,
		const DofOwner* pDO,
		const StructNode* pN,
		const Mat3x3 &R, flag fOut, doublereal initLong, doublereal initLat, doublereal earth_radius);
	virtual ~AircraftInstruments(void);

	/* Scrive il contributo dell'elemento al file di restart */
	virtual std::ostream& Restart(std::ostream& out) const;

	/* Tipo dell'elemento (usato per debug ecc.) */
	virtual Elem::Type GetElemType(void) const;

	/* funzioni proprie */

	/*
	 * output; si assume che ogni tipo di elemento sappia, attraverso
	 * l'OutputHandler, dove scrivere il proprio output
	 */
	virtual void Output(OutputHandler& OH) const;

	/* Tipo di elemento aerodinamico */
	virtual AerodynamicElemBase::Type GetAerodynamicElemType(void) const {
		return AerodynamicElemBase::AIRCRAFTINSTRUMENTS;
	};

	/* Dimensioni del workspace */
	virtual void
	WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;

	/* assemblaggio jacobiano */
	virtual VariableSubMatrixHandler&
	AssJac(VariableSubMatrixHandler& WorkMat,
	       doublereal /* dCoef */ ,
	       const VectorHandler& /* XCurr */ ,
	       const VectorHandler& /* XPrimeCurr */ );

	/* assemblaggio residuo */
	virtual SubVectorHandler&
	AssRes(SubVectorHandler& WorkVec,
	       doublereal dCoef,
	       const VectorHandler& XCurr,
	       const VectorHandler& XPrimeCurr);

	/* Dati privati */
	virtual unsigned int iGetNumPrivData(void) const;
	virtual unsigned int iGetPrivDataIdx(const char *s) const;
	virtual doublereal dGetPrivData(unsigned int i) const;

	/* *******PER IL SOLUTORE PARALLELO******** */
	/*
	 * Fornisce il tipo e la label dei nodi che sono connessi all'elemento
	 * utile per l'assemblaggio della matrice di connessione fra i dofs
	 */
	virtual void
	GetConnectedNodes(std::vector<const Node *>& connectedNodes) const;

	/* ************************************************ */

	/* returns the dimension of the component */
	const virtual OutputHandler::Dimensions GetEquationDimension(integer index) const;

	/* describes the dimension of components of equation */
    virtual std::ostream& DescribeEq(std::ostream& out,
		  const char *prefix = "",
		  bool bInitial = false) const;
};

class DataManager;
class MBDynParser;

extern Elem *
ReadAircraftInstruments(DataManager* pDM, MBDynParser& HP,
	const DofOwner *pDO, unsigned int uLabel);

#endif // INSTRUMENTS_H
