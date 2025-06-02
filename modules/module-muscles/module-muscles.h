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

#ifndef ___MODULE_MUSCLES_H__INCLUDED___
#define ___MODULE_MUSCLES_H__INCLUDED___

#include "mbconfig.h"           /* This goes first in every *.c,*.cc file */

#include <cmath>
#include <cfloat>

#include "dataman.h"
#include "constltp_impl.h"


class MuscleCL
: public ElasticConstitutiveLaw<doublereal, doublereal> {
protected:
	doublereal Li;
	doublereal L0;
	doublereal V0;
	doublereal F0;
	
	doublereal f1;		// active force-length curve
	doublereal f2;		// force-velocity curve
	doublereal f3;		// passive force-length curve
	doublereal df1dx;
	doublereal df2dv;
	doublereal df3dx;
	
	DriveOwner Activation;
	bool bActivationOverflow;
	bool bActivationOverflowWarn;
	doublereal a;
	doublereal aReq;	// requested activation: for output only
	virtual std::ostream& Restart_int(std::ostream& out) const {
		return out;
	};
#ifdef USE_NETCDF
	MBDynNcVar Var_dAct;
	MBDynNcVar Var_dActReq;
	MBDynNcVar Var_dAref;
	MBDynNcVar Var_f1;
	MBDynNcVar Var_f2;
	MBDynNcVar Var_f3;
	MBDynNcVar Var_df1dx;
	MBDynNcVar Var_df2dv;
	MBDynNcVar Var_df3dx;
#endif // USE_NETCDF
public:
	MuscleCL(const TplDriveCaller<doublereal> *pTplDC, doublereal dPreStress,
		doublereal Li, doublereal L0, doublereal V0, doublereal F0,
		const DriveCaller *pAct, bool bActivationOverflow, bool bActivationOverflowWarn)
	: ElasticConstitutiveLaw<doublereal, doublereal>(pTplDC, dPreStress),
	Li(Li), L0(L0), V0(V0), F0(F0), f1(0), f2(0), f3(0), df1dx(0), df2dv(0), df3dx(0),
	Activation(pAct), bActivationOverflow(bActivationOverflow), bActivationOverflowWarn(bActivationOverflowWarn)
	{
		NO_OP;
	};

	virtual ~MuscleCL(void) {
		NO_OP;
	};

	virtual ConstLawType::Type GetConstLawType(void) const {
		return ConstLawType::VISCOELASTIC;
	};

	virtual ConstitutiveLaw<doublereal, doublereal>* pCopy(void) const {
		ConstitutiveLaw<doublereal, doublereal>* pCL = 0;

		// pass parameters to copy constructor
		SAFENEWWITHCONSTRUCTOR(pCL, MuscleCL,
			MuscleCL(pGetDriveCaller()->pCopy(),
				PreStress,
				Li, L0, V0, F0,
				Activation.pGetDriveCaller()->pCopy(),
				bActivationOverflow, 
				bActivationOverflowWarn));
		return pCL;
	};

	virtual std::ostream& Restart(std::ostream& out) const;
	virtual std::ostream& OutputAppend(std::ostream& out) const;
	virtual void NetCDFOutputAppend(OutputHandler& OH) const;
	virtual void OutputAppendPrepare(OutputHandler& OH, const std::string& name);

	virtual void Update(const doublereal& Eps, const doublereal& EpsPrime);
};

class MuscleErfCL
: public MuscleCL {
public:
	MuscleErfCL(const TplDriveCaller<doublereal> *pTplDC, doublereal dPreStress,
		doublereal Li, doublereal L0, doublereal V0, doublereal F0,
		const DriveCaller *pAct, bool bActivationOverflow, bool bActivationOverflowWarn)
	: MuscleCL(pTplDC, dPreStress, Li, L0, V0, F0, pAct, bActivationOverflow, bActivationOverflowWarn) 
	{
		NO_OP;
	};

	virtual ~MuscleErfCL(void) {
		NO_OP;
	};

	virtual ConstLawType::Type GetConstLawType(void) const {
		return ConstLawType::VISCOELASTIC;
	};

	virtual ConstitutiveLaw<doublereal, doublereal>* pCopy(void) const {
		ConstitutiveLaw<doublereal, doublereal>* pCL = 0;

		// pass parameters to copy constructor
		SAFENEWWITHCONSTRUCTOR(pCL, MuscleErfCL,
			MuscleCL(pGetDriveCaller()->pCopy(),
				PreStress,
				Li, L0, V0, F0,
				Activation.pGetDriveCaller()->pCopy(),
				bActivationOverflow, 
				bActivationOverflowWarn));
		return pCL;
	};

	virtual std::ostream& Restart(std::ostream& out) const;
	virtual std::ostream& OutputAppend(std::ostream& out) const;
	virtual void NetCDFOutputAppend(OutputHandler& OH) const;
	virtual void OutputAppendPrepare(OutputHandler& OH, const std::string& name);

	virtual void Update(const doublereal& Eps, const doublereal& EpsPrime);
};

class MuscleErfErgoCL
: public MuscleErfCL {
public:
	MuscleErfErgoCL(const TplDriveCaller<doublereal> *pTplDC, doublereal dPreStress,
		doublereal Li, doublereal L0, doublereal V0, doublereal F0,
		const DriveCaller *pAct, bool bActivationOverflow, bool bActivationOverflowWarn)
	: MuscleErfCL(pTplDC, dPreStress, Li, L0, V0, F0, pAct, bActivationOverflow, bActivationOverflowWarn)
	{
		NO_OP;
	};

	virtual ~MuscleErfErgoCL(void) {
		NO_OP;
	};

	virtual ConstLawType::Type GetConstLawType(void) const {
		return ConstLawType::ELASTIC;
	};

	virtual ConstitutiveLaw<doublereal, doublereal>* pCopy(void) const {
		ConstitutiveLaw<doublereal, doublereal>* pCL = 0;

		// pass parameters to copy constructor
		SAFENEWWITHCONSTRUCTOR(pCL, MuscleErfErgoCL,
			MuscleErfErgoCL(pGetDriveCaller()->pCopy(),
				PreStress,
				Li, L0, V0, F0,
				Activation.pGetDriveCaller()->pCopy(),
				bActivationOverflow, 
				bActivationOverflowWarn));
		return pCL;
	};

	virtual void Update(const doublereal& Eps, const doublereal& EpsPrime) {
		MuscleErfCL::Update(Eps, 0.);
	};

protected:
	virtual std::ostream& Restart_int(std::ostream& out) const {
		out << ", ergonomy, yes";
		return out;
	};
};

class MuscleReflexiveCL
: public MuscleCL {
protected:
	DriveOwner Kp;
	DriveOwner Kd;
	DriveOwner ReferenceLength;
#ifdef USE_NETCDF
	MBDynNcVar Var_dKp;
	MBDynNcVar Var_dKd;
	MBDynNcVar Var_dReferenceLength;
#endif // USE_NETCDF
public:
	MuscleReflexiveCL(const TplDriveCaller<doublereal> *pTplDC, doublereal dPreStress,
		doublereal Li, doublereal L0, doublereal V0, doublereal F0,
		const DriveCaller *pAct, bool bActivationOverflow, bool bActivationOverflowWarn,
		const DriveCaller *pKp, const DriveCaller *pKd, const DriveCaller *pReferenceLength)
	: MuscleCL(pTplDC, dPreStress, Li, L0, V0, F0, pAct, bActivationOverflow, bActivationOverflowWarn),
	Kp(pKp), Kd(pKd), ReferenceLength(pReferenceLength)
	{
		NO_OP;
	};

	virtual ~MuscleReflexiveCL(void) {
		NO_OP;
	};

	virtual ConstitutiveLaw<doublereal, doublereal>* pCopy(void) const {
		ConstitutiveLaw<doublereal, doublereal>* pCL = 0;

		// pass parameters to copy constructor
		SAFENEWWITHCONSTRUCTOR(pCL, MuscleReflexiveCL,
			MuscleReflexiveCL(pGetDriveCaller()->pCopy(),
				PreStress,
				Li, L0, V0, F0,
				Activation.pGetDriveCaller()->pCopy(),
				bActivationOverflow,
				bActivationOverflowWarn,
				Kp.pGetDriveCaller()->pCopy(), Kd.pGetDriveCaller()->pCopy(),
				ReferenceLength.pGetDriveCaller()->pCopy()));
		return pCL;
	};

	virtual void Update(const doublereal& Eps, const doublereal& EpsPrime);
	virtual std::ostream& OutputAppend(std::ostream& out) const;
	virtual void NetCDFOutputAppend(OutputHandler& OH) const;
	virtual void OutputAppendPrepare(OutputHandler& OH, const std::string& name);

protected:
	virtual std::ostream& Restart_int(std::ostream& out) const;
};

class MuscleErfReflexiveCL
: public MuscleReflexiveCL {
public:
	MuscleErfReflexiveCL(const TplDriveCaller<doublereal> *pTplDC, doublereal dPreStress,
		doublereal Li, doublereal L0, doublereal V0, doublereal F0,
		const DriveCaller *pAct, bool bActivationOverflow, bool bActivationOverflowWarn,
		const DriveCaller *pKp, const DriveCaller *pKd, const DriveCaller *pReferenceLength)
	: MuscleReflexiveCL(pTplDC, dPreStress, Li, L0, V0, F0, pAct, bActivationOverflow, bActivationOverflowWarn, pKp, pKd, pReferenceLength)
	{
		NO_OP;
	};

	virtual ~MuscleErfReflexiveCL(void) {
		NO_OP;
	};

	virtual ConstitutiveLaw<doublereal, doublereal>* pCopy(void) const {
		ConstitutiveLaw<doublereal, doublereal>* pCL = 0;

		// pass parameters to copy constructor
		SAFENEWWITHCONSTRUCTOR(pCL, MuscleErfReflexiveCL,
			MuscleErfReflexiveCL(pGetDriveCaller()->pCopy(),
				PreStress,
				Li, L0, V0, F0,
				Activation.pGetDriveCaller()->pCopy(),
				bActivationOverflow,
				bActivationOverflowWarn,
				Kp.pGetDriveCaller()->pCopy(), Kd.pGetDriveCaller()->pCopy(),
				ReferenceLength.pGetDriveCaller()->pCopy()));
		return pCL;
	};

	virtual void Update(const doublereal& Eps, const doublereal& EpsPrime);
	virtual std::ostream& OutputAppend(std::ostream& out) const;
	virtual void NetCDFOutputAppend(OutputHandler& OH) const;
	virtual void OutputAppendPrepare(OutputHandler& OH, const std::string& name);

protected:
	virtual std::ostream& Restart_int(std::ostream& out) const;
};

class MusclePennestriCL
: public MuscleCL {
public:
	MusclePennestriCL(const TplDriveCaller<doublereal> *pTplDC, doublereal dPreStress,
		doublereal Li, doublereal L0, doublereal V0, doublereal F0,
		const DriveCaller *pAct, bool bActivationOverflow, bool bActivationOverflowWarn)
	: MuscleCL(pTplDC, dPreStress, Li, L0, V0, F0, pAct, bActivationOverflow, bActivationOverflowWarn)
	{
		NO_OP;
	};

	virtual ~MusclePennestriCL(void) {
		NO_OP;
	};

	virtual ConstLawType::Type GetConstLawType(void) const {
		return ConstLawType::VISCOELASTIC;
	};

	virtual ConstitutiveLaw<doublereal, doublereal>* pCopy(void) const {
		ConstitutiveLaw<doublereal, doublereal>* pCL = 0;

		// pass parameters to copy constructor
		SAFENEWWITHCONSTRUCTOR(pCL, MusclePennestriCL,
			MusclePennestriCL(pGetDriveCaller()->pCopy(),
				PreStress,
				Li, L0, V0, F0,
				Activation.pGetDriveCaller()->pCopy(),
				bActivationOverflow, 
				bActivationOverflowWarn));
		return pCL;
	};

	virtual std::ostream& Restart(std::ostream& out) const;
	virtual std::ostream& OutputAppend(std::ostream& out) const;
	virtual void NetCDFOutputAppend(OutputHandler& OH) const;
	virtual void OutputAppendPrepare(OutputHandler& OH, const std::string& name);

	virtual void Update(const doublereal& Eps, const doublereal& EpsPrime);
};

class MusclePennestriErgoCL
: public MusclePennestriCL {
public:
	MusclePennestriErgoCL(const TplDriveCaller<doublereal> *pTplDC, doublereal dPreStress,
		doublereal Li, doublereal L0, doublereal V0, doublereal F0,
		const DriveCaller *pAct, bool bActivationOverflow, bool bActivationOverflowWarn)
	: MusclePennestriCL(pTplDC, dPreStress, Li, L0, V0, F0, pAct, bActivationOverflow, bActivationOverflowWarn)
	{
		NO_OP;
	};

	virtual ~MusclePennestriErgoCL(void) {
		NO_OP;
	};

	virtual ConstLawType::Type GetConstLawType(void) const {
		return ConstLawType::ELASTIC;
	};

	virtual ConstitutiveLaw<doublereal, doublereal>* pCopy(void) const {
		ConstitutiveLaw<doublereal, doublereal>* pCL = 0;

		// pass parameters to copy constructor
		SAFENEWWITHCONSTRUCTOR(pCL, MusclePennestriErgoCL,
			MusclePennestriErgoCL(pGetDriveCaller()->pCopy(),
				PreStress,
				Li, L0, V0, F0,
				Activation.pGetDriveCaller()->pCopy(),
				bActivationOverflow, 
				bActivationOverflowWarn));
		return pCL;
	};

	virtual void Update(const doublereal& Eps, const doublereal& EpsPrime) {
		MusclePennestriCL::Update(Eps, 0.);
	};

protected:
	virtual std::ostream& Restart_int(std::ostream& out) const {
		out << ", ergonomy, yes";
		return out;
	};
};

class MusclePennestriReflexiveCL
: public MuscleReflexiveCL {
public:
	MusclePennestriReflexiveCL(const TplDriveCaller<doublereal> *pTplDC, doublereal dPreStress,
		doublereal Li, doublereal L0, doublereal V0, doublereal F0,
		const DriveCaller *pAct, bool bActivationOverflow, bool bActivationOverflowWarn,
		const DriveCaller *pKp, const DriveCaller *pKd, const DriveCaller *pReferenceLength)
	: MuscleReflexiveCL(pTplDC, dPreStress, Li, L0, V0, F0, pAct, bActivationOverflow, bActivationOverflowWarn, pKp, pKd, pReferenceLength)
	{
		NO_OP;
	};

	virtual ~MusclePennestriReflexiveCL(void) {
		NO_OP;
	};

	virtual ConstitutiveLaw<doublereal, doublereal>* pCopy(void) const {
		ConstitutiveLaw<doublereal, doublereal>* pCL = 0;

		// pass parameters to copy constructor
		SAFENEWWITHCONSTRUCTOR(pCL, MusclePennestriReflexiveCL,
			MusclePennestriReflexiveCL(pGetDriveCaller()->pCopy(),
				PreStress,
				Li, L0, V0, F0,
				Activation.pGetDriveCaller()->pCopy(),
				bActivationOverflow,
				bActivationOverflowWarn,
				Kp.pGetDriveCaller()->pCopy(), Kd.pGetDriveCaller()->pCopy(),
				ReferenceLength.pGetDriveCaller()->pCopy()));
		return pCL;
	};

	virtual void Update(const doublereal& Eps, const doublereal& EpsPrime);
	virtual std::ostream& OutputAppend(std::ostream& out) const;
	virtual void NetCDFOutputAppend(OutputHandler& OH) const;
	virtual void OutputAppendPrepare(OutputHandler& OH, const std::string& name);

protected:
	virtual std::ostream& Restart_int(std::ostream& out) const;
};


class MusclePennestriReflexiveCLWithSRS
: public MuscleReflexiveCL {
public:
	enum SRSModel {
		SRS_LINEAR,
		SRS_EXPONENTIAL
	} m_SRSModel;
protected:
	doublereal SRSGamma;
	doublereal SRSDelta;
	doublereal SRSf;
	doublereal SRSdfdx;
#ifdef USE_NETCDF
	MBDynNcVar Var_dSRSf;
	MBDynNcVar Var_dSRSdfdx;
#endif // USE_NETCDF
public:
	MusclePennestriReflexiveCLWithSRS(const TplDriveCaller<doublereal> *pTplDC, doublereal dPreStress,
		doublereal Li, doublereal L0, doublereal V0, doublereal F0,
		const DriveCaller *pAct, bool bActivationOverflow, bool bActivationOverflowWarn,
		const DriveCaller *pKp, const DriveCaller *pKd, const DriveCaller *pReferenceLength,
		const doublereal SRSGamma, const doublereal SRSDelta, 
		const SRSModel m_SRSModel)
	: MuscleReflexiveCL(pTplDC, dPreStress, Li, L0, V0, F0, pAct, bActivationOverflow, bActivationOverflowWarn, pKp, pKd, pReferenceLength),
	m_SRSModel(m_SRSModel), SRSGamma(SRSGamma), SRSDelta(SRSDelta), SRSf(0), SRSdfdx(0)
	{
		NO_OP;
	};

	virtual ~MusclePennestriReflexiveCLWithSRS(void) {
		NO_OP;
	}

	virtual ConstitutiveLaw<doublereal, doublereal>* pCopy(void) const {
		ConstitutiveLaw<doublereal, doublereal>* pCL = 0;

		// pass parameters to copy constructor
		SAFENEWWITHCONSTRUCTOR(pCL, MusclePennestriReflexiveCLWithSRS,
			MusclePennestriReflexiveCLWithSRS(pGetDriveCaller()->pCopy(),
				PreStress,
				Li, L0, V0, F0,
				Activation.pGetDriveCaller()->pCopy(),
				bActivationOverflow,
				bActivationOverflowWarn,
				Kp.pGetDriveCaller()->pCopy(), Kd.pGetDriveCaller()->pCopy(),
				ReferenceLength.pGetDriveCaller()->pCopy(),
				SRSGamma, SRSDelta, m_SRSModel));
		return pCL;
	};

	virtual void Update(const doublereal& Eps, const doublereal& EpsPrime);
	virtual std::ostream& OutputAppend(std::ostream& out) const;
	virtual void NetCDFOutputAppend(OutputHandler& OH) const;
	virtual void OutputAppendPrepare(OutputHandler& OH, const std::string& name);

protected:
	virtual std::ostream& Restart_int(std::ostream& out) const;
};

extern "C" int
module_init(const char* module_name, void *pdm, void *php);

#endif
