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
 * Author: Andrea Zanoni <andrea.zanoni@polimi.it>
 *         Pierangelo Masarati <pierangelo.masarati@polimi.it>
 */

#include "mbconfig.h"           /* This goes first in every *.c,*.cc file */

#include <cmath>
#include <cfloat>

#include "dataman.h"
#include "constltp_impl.h"

#include "module-muscles.h"

// FIXME: should not be needed (should be pure virtual), but ltdl wants it for some
// reasons!
void
MuscleCL::Update(const doublereal& Eps, const doublereal& EpsPrime)
{
	NO_OP;
};

std::ostream&
MuscleCL::Restart(std::ostream& out) const
{
	// FIXME: this is supposed to be a pure virtual method, so this (dummy)
	// implementation should not be needed. But ltdl complains if it isn't there
	return out;
};

std::ostream& 
MuscleCL::OutputAppend(std::ostream& out) const 
{
	return out 
		<< " " << a 
		<< " " << aReq
		<< " " << f1
		<< " " << f2
		<< " " << f3
		;
};

void 
MuscleCL::NetCDFOutputAppend(OutputHandler& OH) const 
{
#ifdef USE_NETCDF
	OH.WriteNcVar(Var_dAct, a);
	OH.WriteNcVar(Var_dActReq, aReq);
	OH.WriteNcVar(Var_f1, f1);
	OH.WriteNcVar(Var_f2, f2);
	OH.WriteNcVar(Var_f3, f3);
#endif // USE_NETCDF
};


void 
MuscleCL::OutputAppendPrepare(OutputHandler& OH, const std::string& name)
{
#ifdef USE_NETCDF
	ASSERT(OH.IsOpen(OutputHandler::NETCDF));
	if (OH.UseNetCDF(OutputHandler::LOADABLE)) 
	{
		Var_dAct = OH.CreateVar<doublereal>(name + ".a", 
				OutputHandler::Dimensions::Dimensionless, 
				"Muscular activation (effective value)");
		Var_dActReq = OH.CreateVar<doublereal>(name + ".aReq",  
				OutputHandler::Dimensions::Dimensionless,
				"Requested muscular activation");
		Var_f1 = OH.CreateVar<doublereal>(name + ".f1",
				OutputHandler::Dimensions::Dimensionless,
				"Active force-length relationship f1(x)");
		Var_f2 = OH.CreateVar<doublereal>(name + ".f2",
				OutputHandler::Dimensions::Dimensionless,
				"Active force-velocity relationship f2(v)");
		Var_f3 = OH.CreateVar<doublereal>(name + ".f3",
				OutputHandler::Dimensions::Dimensionless,
				"Passive force-length relationship f3(v)");
	}
#endif // USE_NETCDF
};


void 
MuscleErfCL::Update(const doublereal& Eps, const doublereal& EpsPrime) 
{
	ConstitutiveLaw<doublereal, doublereal>::Epsilon = Eps - ElasticConstitutiveLaw<doublereal, doublereal>::Get();
	ConstitutiveLaw<doublereal, doublereal>::EpsilonPrime = EpsPrime;

	aReq = Activation.dGet();
	a = aReq;
	if (a < 0.) {
		if (bActivationOverflowWarn) {
			silent_cerr("MuscleErfCL: activation underflow (aReq=" << aReq << ")" << std::endl);
		}
		if (bActivationOverflow) {
			a = 0.;
		}

	} else if (a > 1.) {
		if (bActivationOverflowWarn) {
			silent_cerr("MuscleErfCL: activation overflow (aReq=" << aReq << ")" << std::endl);
		}
		if (bActivationOverflow) {
			a = 1.;
		}
	}

	doublereal dxdEps = Li/L0;
	doublereal dvdEpsPrime = Li/V0;
	doublereal x = (1. + ConstitutiveLaw<doublereal, doublereal>::Epsilon)*dxdEps;
	doublereal v = ConstitutiveLaw<doublereal, doublereal>::EpsilonPrime*dvdEpsPrime;

	f1 = 0.4*std::erf(10*(x - 0.55)) + 0.2*std::erf(5*(x - 0.85)) - 0.6*std::erf(2.4*(x - 1.35));
	f2 = 1.6 - 1.6*std::exp(0.1/std::pow(v - 1., 2) - 1.1/std::pow(v - 1., 4));
	f3 = 1.3*std::atan(0.1*std::pow(x - 0.22, 10));

	df1dx = 2/M_PI*(4*std::exp(-10*std::pow(-100*(x - 0.55), 2)) + std::exp(-25*std::pow(x - 0.85, 2)) - 1.44*std::exp(-5.76*std::pow(x - 1.35, 2)));
	df2dv = 1.6*std::exp(0.1/std::pow(v - 1., 2) - 1.1/std::pow(v - 1, 4))*(2*0.1/std::pow(v - 1., 3) - 4*1.1/std::pow(v - 1., 5));
	df3dx = 1.3*std::pow(x - 0.22, 9)/(0.01*std::pow(x - 0.22, 20) + 1);

	ConstitutiveLaw<doublereal, doublereal>::F = PreStress + F0*(f1*f2*a + f3);
	ConstitutiveLaw<doublereal, doublereal>::FDE = F0*(df1dx*f2*a + df3dx)*dxdEps;
	ConstitutiveLaw<doublereal, doublereal>::FDEPrime = F0*f1*df2dv*a*dvdEpsPrime;
};

std::ostream& 
MuscleErfCL::Restart(std::ostream& out) const 
{
	out << "muscle"
		", initial length, " << Li
		<< ", reference length, " << L0
		<< ", reference velocity, " << V0
		<< ", reference force, " << F0
		<< ", activation, ", Activation.pGetDriveCaller()->Restart(out)
		<< ", activation check, " << bActivationOverflow
		<< ", warn, " << bActivationOverflowWarn
		<< ", model, erf, "; 
	Restart_int(out)
		<< ", ", ElasticConstitutiveLaw<doublereal, doublereal>::Restart_int(out);
	return out;
};

std::ostream&
MuscleErfCL::OutputAppend(std::ostream& out) const
{
	// FIXME: SHOULD NOT BE NEEDED! (OutputAppend is virtual, with base implementation
	// in MuscleReflexiveCL)
        return	MuscleCL::OutputAppend(out);
};

void MuscleErfCL::NetCDFOutputAppend(OutputHandler& OH) const
{
	// FIXME: SHOULD NOT BE NEEDED! (NetCDFOutputAppend is virtual, with base implementation
	// in MuscleReflexiveCL)
        MuscleCL::NetCDFOutputAppend(OH);
};

void MuscleErfCL::OutputAppendPrepare(OutputHandler& OH, const std::string& name)
{
	// FIXME: SHOULD NOT BE NEEDED (OutputAppendPrepare is virtual, with base
	// implementation in MuscleReflexiveCL)
	MuscleCL::OutputAppendPrepare(OH, name);
};


// FIXME: should not be needed (should be pure virtual), but ltdl wants it for some
// reasons!
void
MuscleReflexiveCL::Update(const doublereal& Eps, const doublereal& EpsPrime)
{
	NO_OP;
};

std::ostream& 
MuscleReflexiveCL::Restart_int(std::ostream& out) const 
{
	out
		<< ", reflexive"
		<< ", proportional gain, ", Kp.pGetDriveCaller()->Restart(out)
		<< ", derivative gain, ", Kd.pGetDriveCaller()->Restart(out)
		<< ", reference length, ", ReferenceLength.pGetDriveCaller()->Restart(out);
	return out;
};

void 
MusclePennestriReflexiveCL::Update(const doublereal& Eps, const doublereal& EpsPrime) 
{
	ConstitutiveLaw<doublereal, doublereal>::Epsilon = Eps - ElasticConstitutiveLaw<doublereal, doublereal>::Get();
	ConstitutiveLaw<doublereal, doublereal>::EpsilonPrime = EpsPrime;

	doublereal dxdEps = Li/L0;
	doublereal dvdEpsPrime = Li/V0;
	doublereal x = (1. + ConstitutiveLaw<doublereal, doublereal>::Epsilon)*dxdEps;
	doublereal v = ConstitutiveLaw<doublereal, doublereal>::EpsilonPrime*dvdEpsPrime;

	doublereal dxRef = ReferenceLength.dGet()/L0;

	doublereal aRef = Activation.dGet();
	aReq = aRef + Kp.dGet()*(x - dxRef) + Kd.dGet()*v;
	a = aReq;

	if (aReq < 0.) {
		if (bActivationOverflowWarn) {
			silent_cerr("MusclePennestriReflexiveCL: activation underflow (aReq=" << aReq << ")" << std::endl);
		}
		if (bActivationOverflow) {
			a = 0.;
		}

	} else if (aReq > 1.) {
		if (bActivationOverflowWarn) {
			silent_cerr("MusclePennestriReflexiveCL: activation overflow (aReq=" << aReq << ")" << std::endl);
		}
		if (bActivationOverflow) {
			a = 1.;
		}
	}

	f1 = std::exp(std::pow(x - 0.95, 2) - 40*std::pow(x - 0.95, 4));
	f2 = 1.6 - 1.6*std::exp(0.1/std::pow(v - 1., 2) - 1.1/std::pow(v - 1., 4));
	f3 = 1.3*std::atan(0.1*std::pow(x - 0.22, 10));

	df1dx = f1*(2*(x - 0.95) - 4*40.*std::pow(x - 0.95, 3));
	df2dv = 1.6*std::exp(0.1/std::pow(v - 1., 2) - 1.1/std::pow(v - 1, 4))*(2*0.1/std::pow(v - 1., 3) - 4*1.1/std::pow(v - 1., 5));
	df3dx = 1.3*std::pow(x - 0.22, 9)/(0.01*std::pow(x - 0.22, 20) + 1);

	ConstitutiveLaw<doublereal, doublereal>::F = PreStress + F0*(f1*f2*a + f3);
	ConstitutiveLaw<doublereal, doublereal>::FDE = F0*((df1dx*aRef + f1*Kp.dGet())*f2 + df3dx)*dxdEps;
	ConstitutiveLaw<doublereal, doublereal>::FDEPrime = F0*f1*(df2dv*aRef + f2*Kd.dGet())*dvdEpsPrime;
};

std::ostream&
MusclePennestriReflexiveCL::Restart_int(std::ostream& out) const
{
	MuscleReflexiveCL::Restart_int(out);
	out
		<< "model, pennestri";
		return out;
};

std::ostream&
MusclePennestriReflexiveCL::OutputAppend(std::ostream& out) const
{
	// FIXME: SHOULD NOT BE NEEDED! (OutputAppend is virtual, with base implementation
	// in MuscleReflexiveCL)
        return	MuscleReflexiveCL::OutputAppend(out);
};

void MusclePennestriReflexiveCL::NetCDFOutputAppend(OutputHandler& OH) const
{
	// FIXME: SHOULD NOT BE NEEDED! (NetCDFOutputAppend is virtual, with base implementation
	// in MuscleReflexiveCL)
        MuscleReflexiveCL::NetCDFOutputAppend(OH);
};

void MusclePennestriReflexiveCL::OutputAppendPrepare(OutputHandler& OH, const std::string& name)
{
	// FIXME: SHOULD NOT BE NEEDED (OutputAppendPrepare is virtual, with base
	// implementation in MuscleReflexiveCL)
	MuscleReflexiveCL::OutputAppendPrepare(OH, name);
};

void 
MuscleErfReflexiveCL::Update(const doublereal& Eps, const doublereal& EpsPrime) 
{
	ConstitutiveLaw<doublereal, doublereal>::Epsilon = Eps - ElasticConstitutiveLaw<doublereal, doublereal>::Get();
	ConstitutiveLaw<doublereal, doublereal>::EpsilonPrime = EpsPrime;

	doublereal dxdEps = Li/L0;
	doublereal dvdEpsPrime = Li/V0;
	doublereal x = (1. + ConstitutiveLaw<doublereal, doublereal>::Epsilon)*dxdEps;
	doublereal v = ConstitutiveLaw<doublereal, doublereal>::EpsilonPrime*dvdEpsPrime;

	doublereal dxRef = ReferenceLength.dGet()/L0;

	doublereal aRef = Activation.dGet();
	aReq = aRef + Kp.dGet()*(x - dxRef) + Kd.dGet()*v;
	a = aReq;

	if (aReq < 0.) {
		if (bActivationOverflowWarn) {
			silent_cerr("MuscleErfReflexiveCL: activation underflow (aReq=" << aReq << ")" << std::endl);
		}
		if (bActivationOverflow) {
			a = 0.;
		}

	} else if (aReq > 1.) {
		if (bActivationOverflowWarn) {
			silent_cerr("MuscleErfReflexiveCL: activation overflow (aReq=" << aReq << ")" << std::endl);
		}
		if (bActivationOverflow) {
			a = 1.;
		}
	}

	f1 = 0.4*std::erf(10*(x - 0.55)) + 0.2*std::erf(5*(x - 0.85)) - 0.6*std::erf(2.4*(x - 1.35));
	f2 = 1.6 - 1.6*std::exp(0.1/std::pow(v - 1., 2) - 1.1/std::pow(v - 1., 4));
	f3 = 1.3*std::atan(0.1*std::pow(x - 0.22, 10));

	df1dx = 2/M_PI*(4*std::exp(-10*std::pow(-100*(x - 0.55), 2)) + std::exp(-25*std::pow(x - 0.85, 2)) - 1.44*std::exp(-5.76*std::pow(x - 1.35, 2)));
	df2dv = 1.6*std::exp(0.1/std::pow(v - 1., 2) - 1.1/std::pow(v - 1, 4))*(2*0.1/std::pow(v - 1., 3) - 4*1.1/std::pow(v - 1., 5));
	df3dx = 1.3*std::pow(x - 0.22, 9)/(0.01*std::pow(x - 0.22, 20) + 1);

	ConstitutiveLaw<doublereal, doublereal>::F = PreStress + F0*(f1*f2*a + f3);
	ConstitutiveLaw<doublereal, doublereal>::FDE = F0*((df1dx*aRef + f1*Kp.dGet())*f2 + df3dx)*dxdEps;
	ConstitutiveLaw<doublereal, doublereal>::FDEPrime = F0*f1*(df2dv*aRef + f2*Kd.dGet())*dvdEpsPrime;
};

std::ostream& 
MuscleErfReflexiveCL::Restart_int(std::ostream& out) const 
{
	MuscleReflexiveCL::Restart_int(out);
	out
		<< "model, erf";
		return out;
};

std::ostream&
MuscleErfReflexiveCL::OutputAppend(std::ostream& out) const
{
	// FIXME: SHOULD NOT BE NEEDED! (OutputAppend is virtual, with base implementation
	// in MuscleReflexiveCL)
        return	MuscleReflexiveCL::OutputAppend(out);
};

void MuscleErfReflexiveCL::NetCDFOutputAppend(OutputHandler& OH) const
{
	// FIXME: SHOULD NOT BE NEEDED! (NetCDFOutputAppend is virtual, with base implementation
	// in MuscleReflexiveCL)
        MuscleReflexiveCL::NetCDFOutputAppend(OH);
};

void MuscleErfReflexiveCL::OutputAppendPrepare(OutputHandler& OH, const std::string& name)
{
	// FIXME: SHOULD NOT BE NEEDED (OutputAppendPrepare is virtual, with base
	// implementation in MuscleReflexiveCL)
	MuscleReflexiveCL::OutputAppendPrepare(OH, name);
};

void 
MusclePennestriCL::Update(const doublereal& Eps, const doublereal& EpsPrime) 
{
	ConstitutiveLaw<doublereal, doublereal>::Epsilon = Eps - ElasticConstitutiveLaw<doublereal, doublereal>::Get();
	ConstitutiveLaw<doublereal, doublereal>::EpsilonPrime = EpsPrime;

	aReq = Activation.dGet();
	a = aReq;
	if (a < 0.) {
		if (bActivationOverflowWarn) {
			silent_cerr("MusclePennestriCL: activation underflow (aReq=" << aReq << ")" << std::endl);
		}
		if (bActivationOverflow) {
			a = 0.;
		}

	} else if (a > 1.) {
		if (bActivationOverflowWarn) {
			silent_cerr("MusclePennestriCL: activation overflow (aReq=" << aReq << ")" << std::endl);
		}
		if (bActivationOverflow) {
			a = 1.;
		}
	}

	doublereal dxdEps = Li/L0;
	doublereal dvdEpsPrime = Li/V0;
	doublereal x = (1. + ConstitutiveLaw<doublereal, doublereal>::Epsilon)*dxdEps;
	doublereal v = ConstitutiveLaw<doublereal, doublereal>::EpsilonPrime*dvdEpsPrime;

	f1 = std::exp(std::pow(x - 0.95, 2) - 40*std::pow(x - 0.95, 4));
	f2 = 1.6 - 1.6*std::exp(0.1/std::pow(v - 1., 2) - 1.1/std::pow(v - 1., 4));
	f3 = 1.3*std::atan(0.1*std::pow(x - 0.22, 10));

	df1dx = f1*(2*(x - 0.95) - 4*40.*std::pow(x - 0.95, 3));
	df2dv = 1.6*std::exp(0.1/std::pow(v - 1., 2) - 1.1/std::pow(v - 1, 4))*(2*0.1/std::pow(v - 1., 3) - 4*1.1/std::pow(v - 1., 5));
	df3dx = 1.3*std::pow(x - 0.22, 9)/(0.01*std::pow(x - 0.22, 20) + 1);

	ConstitutiveLaw<doublereal, doublereal>::F = PreStress + F0*(f1*f2*a + f3);
	ConstitutiveLaw<doublereal, doublereal>::FDE = F0*(df1dx*f2*a + df3dx)*dxdEps;
	ConstitutiveLaw<doublereal, doublereal>::FDEPrime = F0*f1*df2dv*a*dvdEpsPrime;
};

std::ostream& 
MusclePennestriCL::Restart(std::ostream& out) const 
{
	out << "muscle"
		", initial length, " << Li
		<< ", reference length, " << L0
		<< ", reference velocity, " << V0
		<< ", reference force, " << F0
		<< ", activation, ", Activation.pGetDriveCaller()->Restart(out)
		<< ", activation check, " << bActivationOverflow
		<< ", warn, " << bActivationOverflowWarn
		<< ", model, pennestri, "; 
	Restart_int(out)
		<< ", ", ElasticConstitutiveLaw<doublereal, doublereal>::Restart_int(out);
	return out;
};

std::ostream&
MusclePennestriCL::OutputAppend(std::ostream& out) const
{
	// FIXME: SHOULD NOT BE NEEDED! (OutputAppend is virtual, with base implementation
	// in MuscleReflexiveCL)
        return	MuscleCL::OutputAppend(out);
};

void MusclePennestriCL::NetCDFOutputAppend(OutputHandler& OH) const
{
	// FIXME: SHOULD NOT BE NEEDED! (NetCDFOutputAppend is virtual, with base implementation
	// in MuscleReflexiveCL)
        MuscleCL::NetCDFOutputAppend(OH);
};

void MusclePennestriCL::OutputAppendPrepare(OutputHandler& OH, const std::string& name)
{
	// FIXME: SHOULD NOT BE NEEDED (OutputAppendPrepare is virtual, with base
	// implementation in MuscleReflexiveCL)
	MuscleCL::OutputAppendPrepare(OH, name);
};

std::ostream& 
MuscleReflexiveCL::OutputAppend(std::ostream& out) const 
{
	return out 
		<< " " << a 
		<< " " << aReq 
		<< " " << f1
		<< " " << f2
		<< " " << f3
		<< " " << df1dx
		<< " " << df2dv
		<< " " << df3dx
		<< " " << Kp.dGet()
		<< " " << Kd.dGet()
		<< " " << ReferenceLength.dGet()
		;
}

void 
MuscleReflexiveCL::NetCDFOutputAppend(OutputHandler& OH) const 
{
#ifdef USE_NETCDF
	OH.WriteNcVar(Var_dAct, a);
	OH.WriteNcVar(Var_dActReq, aReq);
	OH.WriteNcVar(Var_dAref, Activation.dGet()); 
	OH.WriteNcVar(Var_f1, f1);
	OH.WriteNcVar(Var_f2, f2);
	OH.WriteNcVar(Var_f3, f3);
	OH.WriteNcVar(Var_df1dx, df1dx);
	OH.WriteNcVar(Var_df2dv, df2dv);
	OH.WriteNcVar(Var_df3dx, df3dx);
	OH.WriteNcVar(Var_dKp, Kp.dGet());
	OH.WriteNcVar(Var_dKd, Kd.dGet());
	OH.WriteNcVar(Var_dReferenceLength, ReferenceLength.dGet());
#endif // USE_NETCDF
}

void 
MuscleReflexiveCL::OutputAppendPrepare(OutputHandler& OH, const std::string& name)
{
#ifdef USE_NETCDF
	ASSERT(OH.IsOpen(OutputHandler::NETCDF));
	if (OH.UseNetCDF(OutputHandler::LOADABLE)) 
	{
		Var_dAct = OH.CreateVar<doublereal>(name + ".a", 
				OutputHandler::Dimensions::Dimensionless, 
				"Muscular activation (effective value)");
		Var_dActReq = OH.CreateVar<doublereal>(name + ".aReq",  
				OutputHandler::Dimensions::Dimensionless,
				"Requested muscular activation");
		Var_dAref = OH.CreateVar<doublereal>(name + ".aRef",
				OutputHandler::Dimensions::Dimensionless,
				"Reference muscular activation");
		Var_f1 = OH.CreateVar<doublereal>(name + ".f1",
				OutputHandler::Dimensions::Dimensionless,
				"Active force-length relationship f1(x)");
		Var_f2 = OH.CreateVar<doublereal>(name + ".f2",
				OutputHandler::Dimensions::Dimensionless,
				"Active force-velocity relationship f2(v)");
		Var_f3 = OH.CreateVar<doublereal>(name + ".f3",
				OutputHandler::Dimensions::Dimensionless,
				"Passive force-length relationship f3(v)");
		Var_df1dx = OH.CreateVar<doublereal>(name + ".df1dx",
				OutputHandler::Dimensions::Dimensionless,
				"Active force-length relationship derivative df1(x)/dx");
		Var_df2dv = OH.CreateVar<doublereal>(name + ".df2dv",
				OutputHandler::Dimensions::Dimensionless,
				"Active force-velocity relationship derivative df2(v)/dv");
		Var_df3dx = OH.CreateVar<doublereal>(name + ".df3dx",
				OutputHandler::Dimensions::Dimensionless,
				"Passive force-length relationship derivative df3(x)/dx");
		Var_dKp = OH.CreateVar<doublereal>(name + ".Kp",
				OutputHandler::Dimensions::Dimensionless,
				"Proportional gain of reflexive activation");
		Var_dKd = OH.CreateVar<doublereal>(name + ".Kd",
				OutputHandler::Dimensions::Dimensionless,
				"Derivative gain of reflexive activation");
		Var_dReferenceLength = OH.CreateVar<doublereal>(name + ".Lref",
				OutputHandler::Dimensions::Length,
				"Reference length of reflexive activation model");
	}
#endif // USE_NETCDF
};

void 
MusclePennestriReflexiveCLWithSRS::Update(const doublereal& Eps, const doublereal& EpsPrime) 
{
	ConstitutiveLaw<doublereal, doublereal>::Epsilon = Eps - ElasticConstitutiveLaw<doublereal, doublereal>::Get();
	ConstitutiveLaw<doublereal, doublereal>::EpsilonPrime = EpsPrime;

	doublereal dxdEps = Li/L0;
	doublereal dvdEpsPrime = Li/V0;
	doublereal x = (1. + ConstitutiveLaw<doublereal, doublereal>::Epsilon)*dxdEps;
	doublereal v = ConstitutiveLaw<doublereal, doublereal>::EpsilonPrime*dvdEpsPrime;

	doublereal dxRef = ReferenceLength.dGet()/L0;

	doublereal aRef = Activation.dGet();
	aReq = aRef + Kp.dGet()*(x - dxRef) + Kd.dGet()*v;
	a = aReq;

	if (aReq < 0.) {
		if (bActivationOverflowWarn) {
			silent_cerr("MusclePennestriCL: activation underflow (aReq=" << aReq << ")" << std::endl);
		}
		if (bActivationOverflow) {
			a = 0.;
		}

	} else if (aReq > 1.) {
		if (bActivationOverflowWarn) {
			silent_cerr("MusclePennestriCL: activation overflow (aReq=" << aReq << ")" << std::endl);
		}
		if (bActivationOverflow) {
			a = 1.;
		}
	}

	f1 = std::exp(std::pow(x - 0.95, 2) - 40*std::pow(x - 0.95, 4));
	f2 = 1.6 - 1.6*std::exp(0.1/std::pow(v - 1., 2) - 1.1/std::pow(v - 1., 4));
	f3 = 1.3*std::atan(0.1*std::pow(x - 0.22, 10));

	df1dx = f1*(2*(x - 0.95) - 4*40.*std::pow(x - 0.95, 3));
	df2dv = 1.6*std::exp(0.1/std::pow(v - 1., 2) - 1.1/std::pow(v - 1, 4))*(2*0.1/std::pow(v - 1., 3) - 4*1.1/std::pow(v - 1., 5));
	df3dx = 1.3*std::pow(x - 0.22, 9)/(0.01*std::pow(x - 0.22, 20) + 1);

	if ( (x - dxRef) >= 0 ) {
		if (m_SRSModel == SRS_LINEAR) {
			if ( (x - dxRef) < SRSDelta ) {
				SRSf = SRSGamma*aRef*(x - dxRef);
				SRSdfdx = SRSGamma*aRef;
			} else { // (x - dxRef >= Delta)
				SRSf = SRSGamma*aRef*SRSDelta;
				SRSdfdx = 0.;
			}
		} else { // m_SRSModel == SRS_EXPONENTIAL
			SRSf = SRSGamma*aRef*SRSDelta*(1 - std::exp(1 - std::exp((x - dxRef)/SRSDelta)));
			SRSdfdx = SRSGamma*aRef*std::exp((x - dxRef)/SRSDelta - std::exp((x - dxRef)/SRSDelta) + 1);
		}
	} else { // (x - xRef) < 0
		SRSf = 0.;
		SRSdfdx = 0.;
	}

	ConstitutiveLaw<doublereal, doublereal>::F = PreStress + F0*(f1*(f2*a + SRSf) + f3 );
	ConstitutiveLaw<doublereal, doublereal>::FDE = 
		F0*(df1dx*(f2*aRef + SRSf) + f1*(SRSdfdx + Kp.dGet()*f2) + df3dx)*dxdEps;
};

std::ostream&
MusclePennestriReflexiveCLWithSRS::Restart_int(std::ostream& out) const
{
	MuscleReflexiveCL::Restart_int(out);
	out
		<< "model, pennestri";
	return out;
};

std::ostream& 
MusclePennestriReflexiveCLWithSRS::OutputAppend(std::ostream& out) const 
{
	return out 
		<< " " << a 
		<< " " << aReq 
		<< " " << f1
		<< " " << f2
		<< " " << f3
		<< " " << df1dx
		<< " " << df2dv
		<< " " << df3dx
		<< " " << Kp.dGet()
		<< " " << Kd.dGet()
		<< " " << ReferenceLength.dGet()
		<< " " << F0*f1*SRSf
		<< " " << F0*(df1dx*SRSf + f1*SRSdfdx)
		;
}

void 
MusclePennestriReflexiveCLWithSRS::NetCDFOutputAppend(OutputHandler& OH) const 
{
#ifdef USE_NETCDF
	OH.WriteNcVar(Var_dAct, a);
	OH.WriteNcVar(Var_dActReq, aReq);
	OH.WriteNcVar(Var_dAref, Activation.dGet()); 
	OH.WriteNcVar(Var_f1, f1);
	OH.WriteNcVar(Var_f2, f2);
	OH.WriteNcVar(Var_f3, f3);
	OH.WriteNcVar(Var_df1dx, df1dx);
	OH.WriteNcVar(Var_df2dv, df2dv);
	OH.WriteNcVar(Var_df3dx, df3dx);
	OH.WriteNcVar(Var_dKp, Kp.dGet());
	OH.WriteNcVar(Var_dKd, Kd.dGet());
	OH.WriteNcVar(Var_dReferenceLength, ReferenceLength.dGet());
	OH.WriteNcVar(Var_dSRSf, F0*f1*SRSf);
	OH.WriteNcVar(Var_dSRSdfdx, F0*(df1dx*SRSf + f1*SRSdfdx));
#endif // USE_NETCDF
}

void 
MusclePennestriReflexiveCLWithSRS::OutputAppendPrepare(OutputHandler& OH, const std::string& name)
{
#ifdef USE_NETCDF
	ASSERT(OH.IsOpen(OutputHandler::NETCDF));

	Var_dAct = OH.CreateVar<doublereal>(name + ".a", 
		OutputHandler::Dimensions::Dimensionless, 
		"Muscular activation (effective value)");
	Var_dActReq = OH.CreateVar<doublereal>(name + ".aReq",  
		OutputHandler::Dimensions::Dimensionless,
		"Requested muscular activation");
	Var_dAref = OH.CreateVar<doublereal>(name + ".aRef",
		OutputHandler::Dimensions::Dimensionless,
		"Reference muscular activation");
	Var_f1 = OH.CreateVar<doublereal>(name + ".f1",
		OutputHandler::Dimensions::Dimensionless,
		"Active force-length relationship f1(x)");
	Var_f2 = OH.CreateVar<doublereal>(name + ".f2",
		OutputHandler::Dimensions::Dimensionless,
		"Active force-velocity relationship f2(v)");
	Var_f3 = OH.CreateVar<doublereal>(name + ".f3",
		OutputHandler::Dimensions::Dimensionless,
		"Passive force-length relationship f3(v)");
	Var_df1dx = OH.CreateVar<doublereal>(name + ".df1dx",
		OutputHandler::Dimensions::Dimensionless,
		"Active force-length relationship derivative df1(x)/dx");
	Var_df2dv = OH.CreateVar<doublereal>(name + ".df2dv",
		OutputHandler::Dimensions::Dimensionless,
		"Active force-velocity relationship derivative df2(v)/dv");
	Var_df3dx = OH.CreateVar<doublereal>(name + ".df3dx",
		OutputHandler::Dimensions::Dimensionless,
		"Passive force-length relationship derivative df3(x)/dx");
	Var_dKp = OH.CreateVar<doublereal>(name + ".Kp",
		OutputHandler::Dimensions::Dimensionless,
		"Proportional gain of reflexive activation");
	Var_dKd = OH.CreateVar<doublereal>(name + ".Kd",
		OutputHandler::Dimensions::Dimensionless,
		"Derivative gain of reflexive activation");
	Var_dReferenceLength = OH.CreateVar<doublereal>(name + ".Lref",
		OutputHandler::Dimensions::Length,
		"Reference length of reflexive activation model");
	Var_dSRSf = OH.CreateVar<doublereal>(name + ".SRSf",
		OutputHandler::Dimensions::Dimensionless,
		"Short-range stiffness force");
	Var_dSRSdfdx = OH.CreateVar<doublereal>(name + ".SRSdfdx",
		OutputHandler::Dimensions::Dimensionless,
		"Short-range stiffness force gradient with respect to dimensionless muscle length");
#endif // USE_NETCDF
}

/* specific functional object(s) */
struct MuscleCLR : public ConstitutiveLawRead<doublereal, doublereal> {
	virtual ConstitutiveLaw<doublereal, doublereal> *
	Read(const DataManager* pDM, MBDynParser& HP, ConstLawType::Type& CLType) {
		ConstitutiveLaw<doublereal, doublereal>* pCL = 0;

		CLType = ConstLawType::VISCOELASTIC;

		if (HP.IsKeyWord("help")) {
			silent_cerr("MuscleCL:\n"
				"        muscle,\n"
				"                [ initial length , <Li> , ]\n"
				"                reference length , <L0> ,\n"
				"                [ reference velocity , <V0> , ]\n"
				"                reference force , <F0> ,\n"
				"                activation , (DriveCaller) <activation>\n"
				"                [ , activation check , (bool)<activation_check> ]\n"
				"		 [ , warn , (bool)<warn_user> ]\n"
				"                [ , ergonomy , { yes | no } , ]\n"
				"                [ , reflexive , # only when ergonomy == no\n"
				"                        proportional gain , (DriveCaller) <kp> ,\n"
				"                        derivative gain , (DriveCaller) <kd> ,\n"
				"                        reference length, (DriveCaller) <lref> ]\n"
				" 			 model, { pennestri | erf }\n"
				"		  	[ , short range stiffness ]\n"
				"			[ 	, model, { exponential | linear }]\n"
				" 		  	[ 	, gamma, (real) <gamma>]\n"
				"		   	[ 	, delta, (real) <delta>]\n"
				"                [ , prestress, <prestress> ]\n"
				"                [ , prestrain, (DriveCaller) <prestrain> ]\n"
				<< std::endl);

			if (!HP.IsArg()) {
				throw NoErr(MBDYN_EXCEPT_ARGS);
			}
		}

		bool bErgo(false);
		bool bGotErgo(false);
		if (HP.IsKeyWord("ergonomy")) {
			silent_cerr("MuscleCL: deprecated, \"ergonomy\" "
					"at line " << HP.GetLineData()
					<< " should be at end of definition" << std::endl);
			bErgo = HP.GetYesNoOrBool(bErgo);
			bGotErgo = true;
		}

		// Models
		enum MuscleModel {
			PENNESTRI,
			ERF
		} m_MuscleModel;
		
		// Pennestrì is the default model
		m_MuscleModel = PENNESTRI;
		
		if (HP.IsKeyWord("model")) {
			if (HP.IsKeyWord("pennestri")) {
				NO_OP;	// default model
			} else if (HP.IsKeyWord("erf")) {
				m_MuscleModel = ERF;
			} else {
				silent_cerr("MuscleCL: unrecognised muscle model at line "
						<< HP.GetLineData() << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
		}

		doublereal Li = -1.;
		if (HP.IsKeyWord("initial" "length")) {
			Li = HP.GetReal();
			if (Li <= 0.) {
				silent_cerr("MuscleCL: null or negative initial length "
					"at line " << HP.GetLineData() << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
		}

		if (!HP.IsKeyWord("reference" "length")) {
			silent_cerr("MuscleCL: \"reference length\" expected "
				"at line " << HP.GetLineData() << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
		doublereal L0 = HP.GetReal();
		if (L0 <= 0.) {
			silent_cerr("MuscleCL: null or negative reference length "
				"at line " << HP.GetLineData() << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		// default [m/s]
		doublereal V0 = 4*L0;
		if (HP.IsKeyWord("reference" "velocity")) {
			V0 = HP.GetReal();
			if (V0 <= 0.) {
				silent_cerr("MuscleCL: null or negative reference velocity "
					"at line " << HP.GetLineData() << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
		}

		if (!HP.IsKeyWord("reference" "force")) {
			silent_cerr("MuscleCL: \"reference force\" expected "
				"at line " << HP.GetLineData() << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
		doublereal F0 = HP.GetReal();
		if (F0 <= 0.) {
			silent_cerr("MuscleCL: null or negative reference force "
				"at line " << HP.GetLineData() << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		if (!HP.IsKeyWord("activation")) {
			silent_cerr("MuscleCL: \"activation\" expected "
				"at line " << HP.GetLineData() << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
		const DriveCaller *pAct = HP.GetDriveCaller();

		bool bActivationOverflow(false);
		if (HP.IsKeyWord("activation" "check")) {
			bActivationOverflow = HP.GetYesNoOrBool(bActivationOverflow);
		}

		bool bActivationOverflowWarn(false);
		if (HP.IsKeyWord("warn")) {
			bActivationOverflow = HP.GetYesNoOrBool(bActivationOverflowWarn);
		}

		// FIXME: "ergonomy" must be here
		if (!bGotErgo && HP.IsKeyWord("ergonomy")) {
			bErgo = HP.GetYesNoOrBool(bErgo);
		}

		bool bReflexive(false);
		DriveCaller* pKp(NULL);
		DriveCaller* pKd(NULL);
		const DriveCaller *pRefLen(0);
	
		// Short Range Stiffness
		bool bSRS(false);
		
		// Default values, from De Groote et Al, JBiomech 55 (2017)
		doublereal dSRSGamma = 280.; 
		doublereal dSRSDelta = 5.7e-3;
		
		// Default model: double linear.
		MusclePennestriReflexiveCLWithSRS::SRSModel m_SRSModel = MusclePennestriReflexiveCLWithSRS::SRSModel::SRS_LINEAR;
		if (HP.IsKeyWord("reflexive")) {
			if (bErgo) {
				silent_cerr("MuscleCL: "
					"\"reflexive\" and \"ergonomy\" incompatible "
					"at line " << HP.GetLineData() << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			bReflexive = true;

			if (!HP.IsKeyWord("proportional" "gain")) {
				silent_cerr("MuscleCL: \"proportional gain\" expected "
					"at line " << HP.GetLineData() << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			pKp = HP.GetDriveCaller();

			if (!HP.IsKeyWord("derivative" "gain")) {
				silent_cerr("MuscleCL: \"derivative gain\" expected "
					"at line " << HP.GetLineData() << std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			pKd = HP.GetDriveCaller();

			if (!HP.IsKeyWord("reference" "length")) {
				silent_cerr("MuscleCL: \"reference length\" expected "
					"at line " << HP.GetLineData() << std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			pRefLen = HP.GetDriveCaller();
		
			if (HP.IsKeyWord("short" "range" "stiffness")) {
				bSRS = true;
				m_MuscleModel = PENNESTRI;
				silent_cout("MuscleCL: short range stiffness requested, selecting Pennestri model" << std::endl);

				if (HP.IsKeyWord("model")) {
					if (HP.IsKeyWord("exponential")) {
						m_SRSModel = MusclePennestriReflexiveCLWithSRS::SRSModel::SRS_EXPONENTIAL;
					} else if (HP.IsKeyWord("linear")) {
						NO_OP; // model is unchanged
					} else {
						silent_cerr("MuscleCL: unrecognised Short-Range Stiffness model "
								"at line " << HP.GetLineData() << std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}
				}

				if (HP.IsKeyWord("gamma")) {
					dSRSGamma = HP.GetReal();
				}

				if (HP.IsKeyWord("delta")) {
					dSRSDelta = HP.GetReal();
				}
			}
		}

		/* Prestress and prestrain */
		doublereal PreStress(0.);
		GetPreStress(HP, PreStress);
		TplDriveCaller<doublereal> *pTplDC = GetPreStrain<doublereal>(pDM, HP);

		if (Li == -1.) {
			Li = L0;
		}

		if (bErgo) {
			if (m_MuscleModel == PENNESTRI) {
				SAFENEWWITHCONSTRUCTOR(pCL, MusclePennestriErgoCL,
						MusclePennestriErgoCL(pTplDC, PreStress, 
							Li, L0, V0, F0, pAct, 
							bActivationOverflow, bActivationOverflowWarn));
			} else if (m_MuscleModel == ERF) {
				SAFENEWWITHCONSTRUCTOR(pCL, MuscleErfErgoCL,
						MuscleErfErgoCL(pTplDC, PreStress,
							Li, L0, V0, F0, pAct,
							bActivationOverflow, bActivationOverflowWarn));
			}
		} else if (bReflexive) {
			if (bSRS) {
				SAFENEWWITHCONSTRUCTOR(pCL, MusclePennestriReflexiveCLWithSRS,
					MusclePennestriReflexiveCLWithSRS(pTplDC, PreStress,
						Li, L0, V0, F0, pAct,
						bActivationOverflow,
						bActivationOverflowWarn,
						pKp, pKd, pRefLen, dSRSGamma, dSRSDelta, m_SRSModel));
			} else {
				if (m_MuscleModel == PENNESTRI) {
					SAFENEWWITHCONSTRUCTOR(pCL, MusclePennestriReflexiveCL,
						MusclePennestriReflexiveCL(pTplDC, PreStress,
							Li, L0, V0, F0, pAct,
							bActivationOverflow,
							bActivationOverflowWarn,
							pKp, pKd, pRefLen));
				} else if (m_MuscleModel == ERF) {
					SAFENEWWITHCONSTRUCTOR(pCL, MuscleErfReflexiveCL,
							MuscleErfReflexiveCL(pTplDC, PreStress,
							Li, L0, V0, F0, pAct,
							bActivationOverflow,
							bActivationOverflowWarn,
							pKp, pKd, pRefLen));
				}
			}
		} else {
			if (m_MuscleModel == PENNESTRI) {
				SAFENEWWITHCONSTRUCTOR(pCL, MusclePennestriCL,
						MusclePennestriCL(pTplDC, PreStress,
							Li, L0, V0, F0, pAct,
							bActivationOverflow, 
							bActivationOverflowWarn));
			} else if (m_MuscleModel == ERF) {
				SAFENEWWITHCONSTRUCTOR(pCL, MuscleErfCL,
						MuscleErfCL(pTplDC, PreStress,
							Li, L0, V0, F0, pAct,
							bActivationOverflow, 
							bActivationOverflowWarn));
			}
		}

		return pCL;
	};
};

extern "C" int
module_init(const char *module_name, void *pdm, void *php)
{
	ConstitutiveLawRead<doublereal, doublereal> *rf1D = new MuscleCLR;
	if (!SetCL1D("muscle", rf1D)) {
		delete rf1D;

		silent_cerr("MuscleCL: "
			"module_init(" << module_name << ") "
			"failed" << std::endl);

		return -1;
	}

	return 0;
}

