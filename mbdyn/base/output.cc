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

/* gestore dell'output */

#include "mbconfig.h"           /* This goes first in every *.c,*.cc file */

#include <sstream>
#include <list>

#include "output.h"
#include "mbpar.h"
#include "dataman.h"

/* OutputHandler - begin */

#ifndef OUTPUT_PRECISION
#define OUTPUT_PRECISION 6
#endif /* OUTPUT_PRECISION */

const int iDefaultWidth = OUTPUT_PRECISION + 6;
const int iDefaultPrecision = OUTPUT_PRECISION;

const char* psExt[] = {
	".out",		//  0
	".mov",
	".ele",
	".abs",
	".ine",
	".jnt",		//  5
	".frc",
	".act",
	".rot",
	".rst",
	".rst.X",	// 10
	".aer",
	".hyd",
	".prs",
	".usr",
	".gen",		// 15
	".par",
	".amd",
	".rfm",
	".log",
	".air",		// 20
	".prm",
	".ext",
	".mod",
	".nc",
	".thn",		// 25
	".the",
	".pla",
	".grv",
	".dof",
	".drv",		// 30
	".trc",
        ".sol",
        ".prl",
        ".inl",
	".m",		// NOTE: ALWAYS LAST!
	NULL		// 35
};

const std::unordered_map<const OutputHandler::Dimensions, const std::string> DimensionNames ({
	{ OutputHandler::Dimensions::Dimensionless , std::string("Dimensionless") },
	{ OutputHandler::Dimensions::Boolean , std::string("Boolean") },
	{ OutputHandler::Dimensions::Length , std::string("Length") },
	{ OutputHandler::Dimensions::Mass , std::string("Mass") },
	{ OutputHandler::Dimensions::Time , std::string("Time") },
	{ OutputHandler::Dimensions::Current , std::string("Current") },
	{ OutputHandler::Dimensions::Temperature , std::string("Temperature") },
	{ OutputHandler::Dimensions::Angle , std::string("Angle") },
	{ OutputHandler::Dimensions::Area , std::string("Area") },
	{ OutputHandler::Dimensions::Force , std::string("Force") },
	{ OutputHandler::Dimensions::Velocity , std::string("Velocity") },
	{ OutputHandler::Dimensions::Acceleration , std::string("Acceleration") },
	{ OutputHandler::Dimensions::AngularVelocity , std::string("Angular velocity") },
	{ OutputHandler::Dimensions::AngularAcceleration , std::string("Angular acceleration") },

	{ OutputHandler::Dimensions::Momentum , std::string("Momentum") },
	{ OutputHandler::Dimensions::MomentaMoment , std::string("Momenta moment") },
	{ OutputHandler::Dimensions::MomentumDerivative , std::string("Momentum derivative") },
	{ OutputHandler::Dimensions::MomentaMomentDerivative , std::string("Momenta moment derivative") },

	{ OutputHandler::Dimensions::LinearStrain , std::string("Linear strain") },
	{ OutputHandler::Dimensions::AngularStrain , std::string("Angular strain") },
	{ OutputHandler::Dimensions::LinearStrainRate , std::string("Linear strain rate") },
	{ OutputHandler::Dimensions::AngularStrainRate , std::string("Angular strain rate") },

	{ OutputHandler::Dimensions::StaticMoment , std::string("Static moment") },
	{ OutputHandler::Dimensions::MomentOfInertia , std::string("Moment of inertia") },

	{ OutputHandler::Dimensions::ForceUnitSpan , std::string("Force per unit span") },

	{ OutputHandler::Dimensions::Work , std::string("Work") },
	{ OutputHandler::Dimensions::Power , std::string("Power") },
	{ OutputHandler::Dimensions::Pressure , std::string("Pressure") },
	{ OutputHandler::Dimensions::Moment , std::string("Moment") },
	{ OutputHandler::Dimensions::Voltage , std::string("Voltage") },
	{ OutputHandler::Dimensions::Charge , std::string("Charge") },
	{ OutputHandler::Dimensions::Resistance , std::string("Resistance") },
	{ OutputHandler::Dimensions::Capacitance, std::string("Capacitance") },
	{ OutputHandler::Dimensions::Inductance, std::string("Inductance") },
	{ OutputHandler::Dimensions::Frequency , std::string("Frequency") },
	{ OutputHandler::Dimensions::deg , std::string("deg") },
	{ OutputHandler::Dimensions::rad , std::string("rad") },

	/* added later for GetEquationDimension method of DofOwnerOwner class */

	{ OutputHandler::Dimensions::MassFlow, std::string("Mass flow")},
	{ OutputHandler::Dimensions::Jerk , std::string("Jerk") },
	{ OutputHandler::Dimensions::VoltageDerivative , std::string("Voltage derivative") },
	{ OutputHandler::Dimensions::TemperatureDerivative , std::string("Temperature derivative") },
	{ OutputHandler::Dimensions::UnknownDimension , std::string("Unknown dimension") }
});

/* Costruttore senza inizializzazione */
OutputHandler::OutputHandler(void)
: FileName(NULL),
#ifdef USE_NETCDF
m_pBinFile(0),
#endif /* USE_NETCDF */
iCurrWidth(iDefaultWidth),
iCurrPrecision(iDefaultPrecision),
nCurrRestartFile(0)
#ifdef USE_NETCDF
,
ncStart1(1,0),  // must initialize vectors otherwise can't assign
ncCount1(1,1),
ncStart1x3(2,0),
ncCount1x3(2,1),
ncStart1x3x3(3,0),
ncCount1x3x3(3,1)
#endif  /* USE_NETCDF */
{
	OutputHandler_int();
}

/* Costruttore con inizializzazione */
OutputHandler::OutputHandler(const char* sFName, int iExtNum)
: FileName(sFName, iExtNum),
#ifdef USE_NETCDF
m_pBinFile(0),
#endif /* USE_NETCDF */
iCurrWidth(iDefaultWidth),
iCurrPrecision(iDefaultPrecision),
nCurrRestartFile(0)
#ifdef USE_NETCDF
,ncStart1(1,0),  // must initialize vectors otherwise can't assign
ncCount1(1,1),
ncStart1x3(2,0),
ncCount1x3(2,1),
ncStart1x3x3(3,0),
ncCount1x3x3(3,1)
#endif  /* USE_NETCDF */
{
	OutputHandler_int();
	Init(sFName, iExtNum);
	SetUnspecifiedUnits(Units);
}

void OutputHandler::ReadOutputUnits(MBDynParser& HP) {
	if (HP.IsKeyWord("MKS")) {
		SetMKSUnits(Units);
		Log() << "Unit for the whole model: MKS" << std::endl;
	} else if (HP.IsKeyWord("CGS")) {
		SetCGSUnits(Units);
		Log() << "Unit for the whole model: CGS" << std::endl;
	} else if (HP.IsKeyWord("MMTMS")) {
		SetMMTMSUnits(Units);
		Log() << "Unit for the whole model: MMTMS" << std::endl;
	} else if (HP.IsKeyWord("MMKGMS")) {
		SetMMKGMSUnits(Units);
		Log() << "Unit for the whole model: MMKGMS" << std::endl;
	} else if (HP.IsKeyWord("Custom")) {
		Log() << "Unit for the whole model: Custom" << std::endl;
		const std::list<Dimensions> BaseUnits ({
			Dimensions::Length,
			Dimensions::Mass,
			Dimensions::Time,
			Dimensions::Current,
			Dimensions::Temperature		 
		});
		for (auto i = BaseUnits.begin(); i != BaseUnits.end(); i++) {
			if (HP.IsKeyWord(DimensionNames.find(*i)->second.c_str())) {
				Units[*i] = HP.GetStringWithDelims();
			} else {
				silent_cerr("Error while reading Custom unit system  at line"
						<< HP.GetLineData()
						<< "\nExpecting the definition of "
						<< DimensionNames.find(*i)->second
						<< " units."
						<< std::endl);
				throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
		}
		SetDerivedUnits(Units);
	} else {
		silent_cerr("Error while reading the model Units at line"
						<< HP.GetLineData()
						<< std::endl);
		throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
	for (auto i = DimensionNames.begin(); i != DimensionNames.end(); i++) {
		Log() << "Unit for " << i->second << ": " << Units[i->first] << std::endl;
	}
}

void OutputHandler::SetDerivedUnits(std::unordered_map<Dimensions, std::string>& Units ) {
	Units[Dimensions::Angle] = "rad";
	Units[Dimensions::Area] = Units[Dimensions::Length] + "^2";
	Units[Dimensions::Force] = Units[Dimensions::Mass] + " " +
		Units[Dimensions::Length] + " " +
		Units[Dimensions::Time] + "^-2";
	Units[Dimensions::Velocity] = Units[Dimensions::Length] + " " +
		Units[Dimensions::Time] + "^-1";
	Units[Dimensions::Acceleration] = Units[Dimensions::Length] + " " +
		Units[Dimensions::Time] + "^-2";
	Units[Dimensions::AngularVelocity] = Units[Dimensions::Angle] + " " +
		Units[Dimensions::Time] + "^-1";
	Units[Dimensions::AngularAcceleration] = Units[Dimensions::Angle] + " " +
		Units[Dimensions::Time] + "^-2";

	Units[Dimensions::Momentum] = Units[Dimensions::Mass] + " " +
		Units[Dimensions::Velocity];
	Units[Dimensions::MomentaMoment] = Units[Dimensions::Mass] + " " +
		Units[Dimensions::Length] + "^2 " + 
		Units[Dimensions::Time] + "^-1";
	Units[Dimensions::MomentumDerivative] = Units[Dimensions::Mass] + " " +
		Units[Dimensions::Acceleration];
	Units[Dimensions::MomentaMomentDerivative] = Units[Dimensions::Mass] + " " +
		Units[Dimensions::Length] + "^2 " + 
		Units[Dimensions::Time] + "^-2";

	Units[Dimensions::LinearStrain] = Units[Dimensions::Dimensionless];
	Units[Dimensions::AngularStrain] = Units[Dimensions::Angle] + " " +
		Units[Dimensions::Length] + "^-1";
	Units[Dimensions::LinearStrainRate] = Units[Dimensions::Time] + "^-1";
	Units[Dimensions::AngularStrainRate] = Units[Dimensions::Angle] + " " +
		Units[Dimensions::Length] + "^-1 " + 
		Units[Dimensions::Time] + "^-1";

	Units[Dimensions::StaticMoment] = Units[Dimensions::Mass] + " " +
		Units[Dimensions::Length];
	Units[Dimensions::MomentOfInertia] = Units[Dimensions::Mass] + " " +
		Units[Dimensions::Length] + "^2";

	Units[Dimensions::ForceUnitSpan] = Units[Dimensions::Mass] + " " +
		Units[Dimensions::Time] + "^-2";

	Units[Dimensions::Work] = Units[Dimensions::Force] + " " +
		Units[Dimensions::Length];
	Units[Dimensions::Power] = Units[Dimensions::Force] + " " +
		Units[Dimensions::Velocity];
	Units[Dimensions::Pressure] = Units[Dimensions::Force] + " " +
		Units[Dimensions::Length] + "^-2";
	Units[Dimensions::Moment] = Units[Dimensions::Force] + " " +
		Units[Dimensions::Length];
	Units[Dimensions::Voltage] = Units[Dimensions::Length] + "^2 " +
		Units[Dimensions::Mass] + " " +
		Units[Dimensions::Time] + "^-3 " +
		Units[Dimensions::Current] + "^-1";
	Units[Dimensions::Resistance] = Units[Dimensions::Length] + "^2 " + 
		Units[Dimensions::Mass] + " " + 
		Units[Dimensions::Time] + "^-3 " +
		Units[Dimensions::Current] + "^-2";
	Units[Dimensions::Capacitance] = Units[Dimensions::Length] + "^-2 " + 
		Units[Dimensions::Mass] + "^-1 " + 
		Units[Dimensions::Time] + "^4 " +
		Units[Dimensions::Current] + "^2";
	Units[Dimensions::Inductance] = Units[Dimensions::Length] + "^2 " + 
		Units[Dimensions::Mass] + " " + 
		Units[Dimensions::Time] + "^-2 " +
		Units[Dimensions::Current] + "^-2";
	Units[Dimensions::Frequency] = Units[Dimensions::Time] + "^-1";
	Units[Dimensions::Charge] = Units[Dimensions::Time] + " " +
		Units[Dimensions::Current];
	Units[Dimensions::deg] = "deg";
	Units[Dimensions::rad] = "rad";
	Units[Dimensions::MassFlow] = Units[Dimensions::Mass] + " " + 
		Units[Dimensions::Time] + "^-1";
	Units[Dimensions::Jerk] = Units[Dimensions::Mass] + " " +
        Units[Dimensions::Time] + "^-3";
	Units[Dimensions::VoltageDerivative] = Units[Dimensions::Voltage] + " " +
        Units[Dimensions::Time] + "^-1";
	Units[Dimensions::UnknownDimension] = "UnknownDimension";
};

void OutputHandler::SetUnspecifiedUnits(std::unordered_map<Dimensions, std::string>& Units) {
	for (auto i = DimensionNames.begin(); i != DimensionNames.end(); i++) {
		Units[i->first] = i->second;
	}
}

void OutputHandler::SetMKSUnits(std::unordered_map<Dimensions, std::string>& Units) {
	Units[Dimensions::Length] = "m";
	Units[Dimensions::Mass] = "kg";
	Units[Dimensions::Time] = "s";
	Units[Dimensions::Current] = "A";
	Units[Dimensions::Temperature] = "K";
	SetDerivedUnits(Units);
	Units[Dimensions::Force] = "N";
	Units[Dimensions::Moment] = "N m";
	Units[Dimensions::Work] = "J";
	Units[Dimensions::Power] = "W";
	Units[Dimensions::Pressure] = "Pa";
	Units[Dimensions::Voltage] = "V";
	Units[Dimensions::Charge] = "C";
	Units[Dimensions::Frequency] = "Hz";
};

void OutputHandler::SetCGSUnits(std::unordered_map<Dimensions, std::string>& Units) {
	Units[Dimensions::Length] = "cm";
	Units[Dimensions::Mass] = "kg";
	Units[Dimensions::Time] = "s";
	Units[Dimensions::Current] = "A";
	Units[Dimensions::Temperature] = "K";
	SetDerivedUnits(Units);
	Units[Dimensions::Force] = "dyn";
	Units[Dimensions::Pressure] = "dyn cm^-2";
	Units[Dimensions::Moment] = "dyn cm";
	Units[Dimensions::Work] = "erg";
	Units[Dimensions::Power] = "erg s^-1";
	Units[Dimensions::Frequency] = "Hz";
	Units[Dimensions::Charge] = "C";
}

void OutputHandler::SetMMTMSUnits(std::unordered_map<Dimensions, std::string>& Units) {
	Units[Dimensions::Length] = "mm";
	Units[Dimensions::Mass] = "ton";
	Units[Dimensions::Time] = "ms";
	Units[Dimensions::Current] = "A";
	Units[Dimensions::Temperature] = "K";
	SetDerivedUnits(Units);
	Units[Dimensions::Force] = "N";
	Units[Dimensions::Moment] = "N mm";
	Units[Dimensions::Work] = "N mm";
	Units[Dimensions::Power] = "N mm s^-1";
	Units[Dimensions::Pressure] = "MPa";
	Units[Dimensions::Frequency] = "kHz";
	Units[Dimensions::Charge] = "mC";
}

void OutputHandler::SetMMKGMSUnits(std::unordered_map<Dimensions, std::string>& Units) {
	Units[Dimensions::Length] = "mm";
	Units[Dimensions::Mass] = "kg";
	Units[Dimensions::Time] = "ms";
	Units[Dimensions::Current] = "A";
	Units[Dimensions::Temperature] = "K";
	SetDerivedUnits(Units);
	Units[Dimensions::Force] = "kN";
	Units[Dimensions::Moment] = "N m";
	Units[Dimensions::Work] = "N m";
	Units[Dimensions::Power] = "N m ms^-1";
	Units[Dimensions::Pressure] = "GPa";
	Units[Dimensions::Work] = "kN mm";
	Units[Dimensions::Frequency] = "kHz";
	Units[Dimensions::Charge] = "mC";
}


// Pesudo-constructor
void
OutputHandler::OutputHandler_int(void)
{
	OutData[OUTPUT].flags = OUTPUT_USE_DEFAULT_PRECISION
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[OUTPUT].pof = &ofOutput;

	OutData[STRNODES].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT
		| OUTPUT_MAY_USE_NETCDF;
	OutData[STRNODES].pof = &ofStrNodes;

	OutData[ELECTRIC].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT
		| OUTPUT_MAY_USE_NETCDF;
	OutData[ELECTRIC].pof= &ofElectric;

	OutData[THERMALNODES].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT
		| OUTPUT_MAY_USE_NETCDF;
	OutData[THERMALNODES].pof= &ofThermalNodes;

	OutData[THERMALELEMENTS].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[THERMALELEMENTS].pof= &ofThermalElements;

	OutData[ABSTRACT].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT
		| OUTPUT_MAY_USE_NETCDF;
	OutData[ABSTRACT].pof = &ofAbstract;

	OutData[INERTIA].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT
		| OUTPUT_MAY_USE_NETCDF;
	OutData[INERTIA].pof = &ofInertia;

	OutData[JOINTS].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT
		| OUTPUT_MAY_USE_NETCDF;
	OutData[JOINTS].pof = &ofJoints;

	OutData[FORCES].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT
		| OUTPUT_MAY_USE_NETCDF;
	OutData[FORCES].pof = &ofForces;

	OutData[BEAMS].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT
		| OUTPUT_MAY_USE_NETCDF;
	OutData[BEAMS].pof = &ofBeams;

	OutData[ROTORS].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT
		| OUTPUT_MAY_USE_NETCDF;
	OutData[ROTORS].pof = &ofRotors;

	OutData[RESTART].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[RESTART].pof = &ofRestart;

	OutData[RESTARTXSOL].flags = 0
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[RESTARTXSOL].pof = &ofRestartXSol;

	OutData[AERODYNAMIC].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT
		| OUTPUT_MAY_USE_NETCDF;
	OutData[AERODYNAMIC].pof = &ofAerodynamic;

	OutData[HYDRAULIC].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[HYDRAULIC].pof = &ofHydraulic;

	OutData[PRESNODES].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT
		| OUTPUT_MAY_USE_NETCDF;
	OutData[PRESNODES].pof = &ofPresNodes;

	OutData[LOADABLE].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT
		| OUTPUT_MAY_USE_NETCDF;
	OutData[LOADABLE].pof = &ofLoadable;

	OutData[GENELS].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[GENELS].pof = &ofGenels;

	OutData[PARTITION].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[PARTITION].pof = &ofPartition;

	OutData[AEROMODALS].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[AEROMODALS].pof = &ofAeroModals;

	OutData[REFERENCEFRAMES].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[REFERENCEFRAMES].pof = &ofReferenceFrames;

	OutData[LOG].flags = 0
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[LOG].pof = &ofLog;

	OutData[AIRPROPS].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[AIRPROPS].pof = &ofAirProps;

	OutData[PARAMETERS].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[PARAMETERS].pof = &ofParameters;

	OutData[EXTERNALS].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[EXTERNALS].pof = &ofExternals;

	OutData[MODAL].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT | OUTPUT_MAY_USE_NETCDF;
	OutData[MODAL].pof = &ofModal;

	OutData[PLATES].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT
		| OUTPUT_MAY_USE_NETCDF;
	OutData[PLATES].pof = &ofPlates;

	OutData[GRAVITY].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT | OUTPUT_MAY_USE_NETCDF;
	OutData[GRAVITY].pof = &ofGravity;

	OutData[DOFSTATS].flags = OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[DOFSTATS].pof = &ofDofStats;

	OutData[DRIVECALLERS].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
			| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[DRIVECALLERS].pof = &ofDriveCallers;

	OutData[TRACES].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
			| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[TRACES].pof = &ofTraces;

	OutData[SOLIDS].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
		| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;

        OutData[SOLIDS].pof = &ofSolids;

        OutData[SURFACE_LOADS].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
             | OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;

        OutData[SURFACE_LOADS].pof = &ofSurfaceLoads;

        OutData[INERTIA_ELEMENTS].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
             | OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT
             | OUTPUT_MAY_USE_NETCDF;
        OutData[INERTIA_ELEMENTS].pof = &ofInertiaElements;
        
	OutData[EIGENANALYSIS].flags = OUTPUT_USE_DEFAULT_PRECISION | OUTPUT_USE_SCIENTIFIC
			| OUTPUT_MAY_USE_TEXT | OUTPUT_USE_TEXT;
	OutData[EIGENANALYSIS].pof = &ofEigenanalysis;

	OutData[NETCDF].flags = 0
		| OUTPUT_MAY_USE_NETCDF;
	OutData[NETCDF].pof = 0;

	currentStep = 0;
#if defined(USE_NETCDF)
	ncCount1x3[1] = ncCount1x3x3[1] = 3;
	ncCount1x3x3[2] = 3;
#endif  /* USE_NETCDF */
}

/* Inizializzazione */
void
OutputHandler::Init(const char* sFName, int iExtNum)
{
	FileName::iInit(sFName, iExtNum);

	OutputOpen();
	LogOpen();
}

/* Distruttore */
OutputHandler::~OutputHandler(void)
{
	for (int iCnt = 0; iCnt < LASTFILE; iCnt++) {
		if (IsOpen(iCnt)) {
#ifdef USE_NETCDF
			if (iCnt == NETCDF) {
				if (m_pBinFile != 0) {
					delete m_pBinFile;
				}

			} else
#endif /* USE_NETCDF */
			{
				OutData[iCnt].pof->exceptions(std::ios::iostate(0));
				OutData[iCnt].pof->close();
			}
		}
	}
}

/* Aggiungere qui le funzioni che aprono i singoli stream */
void
OutputHandler::Open(const OutputHandler::OutFiles out)
{
#ifdef USE_NETCDF
	if (out == NETCDF && !IsOpen(out)) {
		// FIXME: we should use the default format, or any selected by the user;
		// but wait a minute: can this actually happen?
		// return NetCDFOpen(out, netCDF::NcFile::nc4);
		NetCDFOpen(out, netCDF::NcFile::classic);

	}
#endif /* USE_NETCDF */
	if (UseText(out) && !IsOpen(out)) {
		const char *fname = _sPutExt(psExt[out]);

		// Open stream
		OutData[out].pof->open(fname);

		if (!(*OutData[out].pof)) {
			int save_errno = errno;
			silent_cerr("Unable to open file "
				"\"" << fname << "\" (" << save_errno << ": " << strerror(save_errno) << ");" << std::endl);
			throw ErrFile(MBDYN_EXCEPT_ARGS);
		}

		// Set precision
		if (UseDefaultPrecision(out)) {
			OutData[out].pof->precision(iCurrPrecision);
		}

		// Set notation
		if (UseScientific(out)) {
			OutData[out].pof->setf(std::ios::scientific);
		}
	}

	return;
}

#ifdef USE_NETCDF
void
OutputHandler::NetCDFOpen(const OutputHandler::OutFiles out, const netCDF::NcFile::FileFormat NetCDF_Format)
{
	if (!IsOpen(out)) {
		m_pBinFile = new netCDF::NcFile(_sPutExt((char*)(psExt[NETCDF])), netCDF::NcFile::replace, NetCDF_Format); // using the default (nc4) mode was seen to drasticly reduce the writing speed, thus using classic format
		//~ NC_FILL only applies top variables, not files or groups in netcdf-cxx4
		// also: error messages (throw) are part of the netcdf-cxx4 interface by default...

		// Let's define some dimensions which could be useful
		m_DimTime = CreateDim("time");
		m_DimV1 = CreateDim("Vec1", 1);
		m_DimV3 = CreateDim("Vec3", 3);
	}

	return;
}
#endif /* USE_NETCDF */

void
OutputHandler::Open(const int out, const std::string& postfix)
{
	if (UseText(out) && !IsOpen(out)) {
		std::string sCurrFileName = sGet();
		std::stringstream fname_ss;

		auto uExtIdx = sCurrFileName.find_last_of(EXT_SEP);

		if (uExtIdx != std::string::npos) {
			fname_ss << sCurrFileName.substr(0, uExtIdx);

		} else {
			fname_ss << sCurrFileName;
		}
		
		fname_ss << postfix << psExt[out] << std::ends;

		const std::string fname = fname_ss.str();

		// Opens the stream
		OutData[out].pof->open(fname.c_str());

		if (!(*OutData[out].pof)) {
			int save_errno = errno;
			silent_cerr("Unable to open file "
				"\"" << fname << "\" (" << save_errno << ": " << strerror(save_errno) << ");" << std::endl);
			throw ErrFile(MBDYN_EXCEPT_ARGS);
		}

		// Sets the field format
		if (UseDefaultPrecision(out)) {
			OutData[out].pof->precision(iCurrPrecision);
		}

		// Sets the notation
		if (UseScientific(out)) {
			OutData[out].pof->setf(std::ios::scientific);
		}

		return;
	}

	return;
}

bool
OutputHandler::IsOpen(int out) const
{
	ASSERT(out > OutputHandler::UNKNOWN);
	ASSERT(out < OutputHandler::LASTFILE);

	return IsOpen(OutputHandler::OutFiles(out));
}

bool
OutputHandler::IsOpen(const OutputHandler::OutFiles out) const
{
#ifdef USE_NETCDF
	if (out == NETCDF) {
		return m_pBinFile == 0 ? false : !m_pBinFile->isNull();
	}
#endif /* USE_NETCDF */

	return OutData[out].pof == 0 ? false : OutData[out].pof->is_open();
}

bool
OutputHandler::UseScientific(int out) const
{
	ASSERT(out > OutputHandler::UNKNOWN);
	ASSERT(out < OutputHandler::LASTFILE);

	return UseScientific(OutputHandler::OutFiles(out));
}

bool
OutputHandler::UseScientific(const OutputHandler::OutFiles out) const
{
	return (OutData[out].flags & OUTPUT_USE_SCIENTIFIC);
}

bool
OutputHandler::UseDefaultPrecision(int out) const
{
	ASSERT(out > OutputHandler::UNKNOWN);
	ASSERT(out < OutputHandler::LASTFILE);

	return UseDefaultPrecision(OutputHandler::OutFiles(out));
}

bool
OutputHandler::UseDefaultPrecision(const OutputHandler::OutFiles out) const
{
	return (OutData[out].flags & OUTPUT_USE_DEFAULT_PRECISION);
}

bool
OutputHandler::UseText(int out) const
{
	ASSERT(out > OutputHandler::UNKNOWN);
	ASSERT(out < OutputHandler::LASTFILE);

	return UseText(OutputHandler::OutFiles(out));
}

void
OutputHandler::SetText(const OutputHandler::OutFiles out)
{
	if (!(OutData[out].flags & OUTPUT_MAY_USE_TEXT)) {
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	OutData[out].flags |= OUTPUT_USE_TEXT;
}

void
OutputHandler::ClearText(void)
{
	for (int out = FIRSTFILE; out < LASTFILE; out++) {
		if (OutData[out].flags & OUTPUT_MAY_USE_TEXT) {
			OutData[out].flags &= ~OUTPUT_USE_TEXT;
		}
	}
}

void
OutputHandler::ClearText(const OutputHandler::OutFiles out)
{
	if (!(OutData[out].flags & OUTPUT_MAY_USE_TEXT)) {
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	OutData[out].flags &= ~OUTPUT_USE_TEXT;
}

bool
OutputHandler::UseText(const OutputHandler::OutFiles out) const
{
	return (OutData[out].flags & OUTPUT_USE_TEXT);
}

bool
OutputHandler::UseNetCDF(int out) const
{
	ASSERT(out > OutputHandler::UNKNOWN);
	ASSERT(out < OutputHandler::LASTFILE);

	return UseNetCDF(OutputHandler::OutFiles(out));
}

void
OutputHandler::SetNetCDF(const OutputHandler::OutFiles out)
{
	if (!(OutData[out].flags & OUTPUT_MAY_USE_NETCDF)) {
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	OutData[out].flags |= OUTPUT_USE_NETCDF;
}

void
OutputHandler::ClearNetCDF(void)
{
	for (int out = FIRSTFILE; out < LASTFILE; out++) {
		if (OutData[out].flags & OUTPUT_MAY_USE_NETCDF) {
			OutData[out].flags &= ~OUTPUT_USE_NETCDF;
		}
	}
}

void
OutputHandler::ClearNetCDF(const OutputHandler::OutFiles out)
{
	if (!(OutData[out].flags & OUTPUT_MAY_USE_NETCDF)) {
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	OutData[out].flags &= ~OUTPUT_USE_NETCDF;
}

bool
OutputHandler::UseNetCDF(const OutputHandler::OutFiles out) const
{
	return (OutData[out].flags & OUTPUT_USE_NETCDF);
}

bool
OutputHandler::Close(const OutputHandler::OutFiles out)
{
	if (!IsOpen(out)) {
		return false;
	}

#ifdef USE_NETCDF
	if (out == NETCDF) {
		m_pBinFile->close();

	} else
#endif /* USE_NETCDF */
	{
		// Chiude lo stream
		OutData[out].pof->close();
	}

	return true;
}

void
OutputHandler::OutputOpen(void)
{
	return Open(OUTPUT);
}

bool
OutputHandler::RestartOpen(bool openResXSol)
{
	if (!IsOpen(RESTART)) {
		char *resExt = NULL;
		int n = nCurrRestartFile > 0 ?
			(int)log10(nCurrRestartFile) + 1 : 1;
		int lenExt = STRLENOF(".")
			+ n
			+ STRLENOF(".rst")
			+ 1;

		SAFENEWARR(resExt, char, lenExt);
		snprintf(resExt, lenExt, ".%.*d.rst", n, nCurrRestartFile);
		/* Apre lo stream */
	      	OutData[RESTART].pof->open(_sPutExt(resExt));

	      	if(!(*OutData[RESTART].pof)) {
			int save_errno = errno;
		 	std::cerr << "Unable to open file \"" << _sPutExt(resExt)
		   		<< "\" (" << save_errno << ": " << strerror(save_errno) << ");" << std::endl;
			throw ErrFile(MBDYN_EXCEPT_ARGS);
		}
		SAFEDELETEARR(resExt);

		/* Setta la formattazione dei campi */
		if(UseDefaultPrecision(RESTART)) {
			OutData[RESTART].pof->precision(iCurrPrecision);
		}

		/* Setta la notazione */
		if(UseScientific(RESTART)) {
			OutData[RESTART].pof->setf(std::ios::scientific);
		}

		if (openResXSol) {
			ASSERT(!IsOpen(RESTARTXSOL));

			char *resXSolExt = NULL;
			int n = nCurrRestartFile > 0 ?
				(int)log10(nCurrRestartFile) + 1 : 1;
			int lenXSolExt = STRLENOF(".")
				+ n
				+ STRLENOF(".rst.X")
				+ 1;

			SAFENEWARR(resXSolExt, char, lenXSolExt);
			snprintf(resXSolExt, lenXSolExt, ".%.*d.rst.X", n, nCurrRestartFile);
			/* Apre lo stream */
		      	OutData[RESTARTXSOL].pof->open(_sPutExt(resXSolExt));
		      	if(!(*OutData[RESTARTXSOL].pof)) {
				int save_errno = errno;
			 	std::cerr << "Unable to open file \"" << _sPutExt(resExt)
			   		<< "\" (" << save_errno << ": " << strerror(save_errno) << ");" << std::endl;
				throw ErrFile(MBDYN_EXCEPT_ARGS);
			}
			SAFEDELETEARR(resXSolExt);
			/* non occorre settare la precisione e il formato
			perche' il file e' binario */
		}

		nCurrRestartFile++;

 	     	return false;
	}

	return true;
}

void
OutputHandler::PartitionOpen(void)
{
	ASSERT(!IsOpen(PARTITION));
	return Open(PARTITION);
}

void
OutputHandler::LogOpen(void)
{
	ASSERT(!IsOpen(LOG));
	return Open(LOG);
}

/* Setta precisione e dimensioni campo */
const int iWidth = 7; /* Caratteri richiesti dalla notazione esponenziale */

void
OutputHandler::SetWidth(int iNewWidth)
{
	ASSERT(iNewWidth > iWidth);
	if (iNewWidth > iWidth) {
		iCurrWidth = iNewWidth;
		iCurrPrecision = iCurrWidth-iWidth;
		for (int iCnt = 0; iCnt < LASTFILE; iCnt++) {
			if (UseDefaultPrecision(iCnt) && IsOpen(iCnt)) {
				OutData[iCnt].pof->width(iCurrWidth);
				OutData[iCnt].pof->precision(iCurrPrecision);
			}
		}
	}
}

void
OutputHandler::SetPrecision(int iNewPrecision)
{
	ASSERT(iNewPrecision > 0);
	if (iNewPrecision > 0) {
		iCurrPrecision = iNewPrecision;
		iCurrWidth = iNewPrecision + iWidth;
		for (int iCnt = 0; iCnt < LASTFILE; iCnt++) {
			if (UseDefaultPrecision(iCnt) && IsOpen(iCnt)) {
				OutData[iCnt].pof->width(iCurrWidth);
				OutData[iCnt].pof->precision(iCurrPrecision);
			}
		}
	}
}

void OutputHandler::SetExceptions(std::ios::iostate flags)
{
	for (int iCnt = 0; iCnt < LASTFILE; iCnt++) {
		if(OutData[iCnt].pof) {
			OutData[iCnt].pof->exceptions(flags);
		}
	}
}

#ifdef USE_NETCDF
MBDynNcDim 
OutputHandler::CreateDim(const std::string& name, integer size)
{
	ASSERT(m_pBinFile != 0);

	MBDynNcDim dim;
	if (size == -1) {
		dim = m_pBinFile->addDim(name);  // .c_str is useless here
	} else {
		dim = m_pBinFile->addDim(name, size);
	}

	return dim;
}

MBDynNcDim 
OutputHandler::GetDim(const std::string& name) const
{
	ASSERT(m_pBinFile != 0);

	return m_pBinFile->getDim(name);
}


/// the following overloaded functions allow to use a uniform function call
/// regardless of whether the variable has one, three, or nine dimensions
/// and regardless of its type, and this without requiring a if condition
/// or further testing of the NcVar, which if done at every timestep
/// would slow down the execution
void
OutputHandler::WriteNcVar(const MBDynNcVar& Var_Var, const Mat3x3& pGetVar) {
	Var_Var.putVar(ncStart1x3x3, ncCount1x3x3, pGetVar.pGetMat());
}
void
OutputHandler::WriteNcVar(const MBDynNcVar& Var_Var, const Mat3x3& pGetVar,
		const size_t& ncStart) 
{
	std::vector<size_t> ncStart1x3x3Tmp = ncStart1x3x3;
	ncStart1x3x3Tmp[0] = ncStart;
	Var_Var.putVar(ncStart1x3x3Tmp, ncCount1x3x3, pGetVar.pGetMat());
}
void
OutputHandler::WriteNcVar(const MBDynNcVar& Var_Var, const Vec3& pGetVar) {
	Var_Var.putVar(ncStart1x3, ncCount1x3, pGetVar.pGetVec());
}
void
OutputHandler::WriteNcVar(const MBDynNcVar& Var_Var, const Vec3& pGetVar,
		const size_t& ncStart) 
{
	std::vector<size_t> ncStart1x3Tmp = ncStart1x3;
	ncStart1x3Tmp[0] = ncStart;
	Var_Var.putVar(ncStart1x3Tmp, ncCount1x3, pGetVar.pGetVec());
}
template <class Tvar>
void
OutputHandler::WriteNcVar(const MBDynNcVar& Var_Var, const Tvar& pGetVar) {
	Var_Var.putVar(ncStart1, ncCount1, &pGetVar);
}
template <class Tvar, class Tstart>
void
OutputHandler::WriteNcVar(const MBDynNcVar& Var_Var, const Tvar& pGetVar, 
		const Tstart& ncStart) 
{
	Var_Var.putVar(std::vector<size_t>(1,ncStart), ncCount1, &pGetVar);
}
template <class Tvar, class Tstart>
void
OutputHandler::WriteNcVar(const MBDynNcVar& Var_Var, const Tvar& pGetVar, 
		const std::vector<Tstart>& ncStart,
		const std::vector<size_t>& count) 
{
	Var_Var.putVar(ncStart, count, &pGetVar);
}

template void OutputHandler::WriteNcVar(const MBDynNcVar&, const doublereal&);
template void OutputHandler::WriteNcVar(const MBDynNcVar&, const long&);
template void OutputHandler::WriteNcVar(const MBDynNcVar&, const int&);
template void OutputHandler::WriteNcVar(const MBDynNcVar&, const doublereal&, const size_t&);
template void OutputHandler::WriteNcVar(const MBDynNcVar&, const doublereal&, const unsigned int&);
template void OutputHandler::WriteNcVar(const MBDynNcVar&, const int&, const unsigned int&);
template void OutputHandler::WriteNcVar(const MBDynNcVar&, const unsigned int&, const unsigned int&);
template void OutputHandler::WriteNcVar(const MBDynNcVar&, const int&, const size_t&);
template void OutputHandler::WriteNcVar(const MBDynNcVar&, const long&, const size_t&);
template void OutputHandler::WriteNcVar(const MBDynNcVar&, const long&, const unsigned int&);
template void OutputHandler::WriteNcVar(const MBDynNcVar&, const doublereal&,
		const std::vector<size_t>&, const std::vector<size_t>&);
template void OutputHandler::WriteNcVar(const MBDynNcVar&, const int&,
		const std::vector<size_t>&, const std::vector<size_t>&);

MBDynNcVar 
OutputHandler::CreateVar(const std::string& name, const MBDynNcType& type,
	const AttrValVec& attrs, const NcDimVec& dims)
{
	MBDynNcVar var;

	var = m_pBinFile->addVar(name, type, dims);
	for (AttrValVec::const_iterator i = attrs.begin(); i != attrs.end(); ++i) {
		var.putAtt(i->attr, i->val);
	}

	return var;
}

MBDynNcVar 
OutputHandler::CreateVar(const std::string& name, const std::string& type)
{
	AttrValVec attrs(1);
	attrs[0] = AttrVal("type", type);

	NcDimVec dims(1);
	dims[0] = DimV1();
	return CreateVar(name, MbNcChar, attrs, dims);
}

MBDynNcVar 
OutputHandler::CreateRotationVar(const std::string& name_prefix,
	const std::string& name_postfix,
	OrientationDescription od,
	const std::string& description)
{
	NcDimVec dim;
	AttrValVec attrs;
	std::string name(name_prefix + ".");

	switch (od) {
	case ORIENTATION_MATRIX:
		dim.resize(3);
		dim[0] = DimTime();
		dim[1] = DimV3();
		dim[2] = DimV3();

		attrs.resize(3);
		attrs[0] = AttrVal("units", "-");
		attrs[1] = AttrVal("type", "Mat3x3");
		attrs[2] = AttrVal("description",
			description + " orientation matrix "
			"(R11, R21, R31, R12, R22, R32, R13, R23, R33)");

		name += "R";
		break;

	case ORIENTATION_VECTOR:
	case UNKNOWN_ORIENTATION_DESCRIPTION:  // only relevant to displacement nodes
		dim.resize(2);
		dim[0] = DimTime();
		dim[1] = DimV3();

		attrs.resize(3);
		attrs[0] = AttrVal("units", "radian");
		attrs[1] = AttrVal("type", "Vec3");
		attrs[2] = AttrVal("description",
			description + " orientation vector "
			"(Phi_X, Phi_Y, Phi_Z)");

		name += "Phi";
		break;

	case EULER_123:
	case EULER_313:
	case EULER_321:
		{
		dim.resize(2);
		dim[0] = DimTime();
		dim[1] = DimV3();

		attrs.resize(3);
		attrs[0] = AttrVal("units", "deg");
		attrs[1] = AttrVal("type", "Vec3");

		std::string etype;
		switch (od) {
		case EULER_123:
			etype = "123";
			break;

		case EULER_313:
			etype = "313";
			break;

		case EULER_321:
			etype = "321";
			break;

		default:
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		attrs[2] = AttrVal("description",
			description + " orientation Euler angles (" + etype + ") "
			"(E_X, E_Y, E_Z)");

		name += "E";
		} break;

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	name += name_postfix;
	return CreateVar(name, MbNcDouble, attrs, dim);
}
#endif // USE_NETCDF

/* OutputHandler - end */


/* ToBeOutput - begin */

ToBeOutput::ToBeOutput(flag fOut)
: fOutput(fOut)
{
	NO_OP;
}

ToBeOutput::~ToBeOutput(void)
{
	NO_OP;
}

void
ToBeOutput::OutputPrepare(OutputHandler &OH)
{
	NO_OP;
}

/* Regular output */
void
ToBeOutput::Output(OutputHandler& OH) const
{
	NO_OP;
}

/* Output of perturbed solution (modes ...) */
void
ToBeOutput::Output(OutputHandler& OH,
		const VectorHandler& X, const VectorHandler& XP) const
{
	NO_OP;
}

flag
ToBeOutput::fToBeOutput(void) const
{
  	return fOutput;
}

bool
ToBeOutput::bToBeOutput(void) const
{
	return fOutput & flag(1);
}

void
ToBeOutput::SetOutputFlag(flag f)
{
	ASSERT(f ? f & flag(1) : 1);

  	fOutput = f;
}

const std::string&
ToBeOutput::sGetOutputNameBase(void) const
{
	return m_sOutputNameBase;
}

Traceable::Traceable(flag fTrace)
:fTrace(fTrace)
{

}

Traceable::~Traceable(void)
{

}

flag Traceable::fToBeTraced(void) const
{
	return fTrace;
}

void Traceable::SetTraceFlag(flag f)
{
	ASSERT(f ? f & flag(1) : 1);

	fTrace = f;
}

/* ToBeOutput - end */
