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
 * Author: Huimin Zhang <huimin.zhang@polimi.it> 2021
 */

#include "mbconfig.h"           /* This goes first in every *.c,*.cc file */

#include <limits>

#include "schurdataman.h"
#include "external.h"
#include "ls.h"
#include "solver.h"
#include "invsolver.h"
#include "stepsol.h"
#include "multistagestepsol_impl.h"
#include "stepsol.hc"

/* TunableBatheSolver - begin */

TunableBatheSolver::TunableBatheSolver(const doublereal Tl,
	const doublereal dSolTl,
	const integer iMaxIt,
	const DriveCaller* pRho,
	const DriveCaller* pAlgRho,
	const bool bmod_res_test)
: tplStageNIntegrator<2>(iMaxIt, Tl, dSolTl, bmod_res_test),
m_Rho(pRho), m_AlgebraicRho(pAlgRho)
{
	ASSERT(pRho != 0);
	ASSERT(pAlgRho != 0);
}

TunableBatheSolver::~TunableBatheSolver(void)
{
	NO_OP;
}

void
TunableBatheSolver::SetDriveHandler(const DriveHandler* pDH)
{
	m_Rho.pGetDriveCaller()->SetDrvHdl(pDH);
	m_AlgebraicRho.pGetDriveCaller()->SetDrvHdl(pDH);
}

void
TunableBatheSolver::SetCoefForStageS(unsigned uStage,
	doublereal dT,
	doublereal dAlpha,
	enum StepChange /* NewStep */)
{
	switch (uStage) {
	case 1:
		m_dRho = m_Rho.dGet();
		m_dAlgebraicRho = m_AlgebraicRho.dGet();

		if (m_dRho == 1.) {
			m_gamma = 1. / 4.;
		}
		else
		{
			m_gamma = (2. - sqrt(2. + 2. * m_dRho)) / (2. * (1. - m_dRho));
		}

		ASSERT(pDM != NULL);
		pDM->SetTime(pDM->dGetTime() - (1. - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

		m_a[IDX_A1][DIFFERENTIAL] = 1.;
		m_a[IDX_A2][DIFFERENTIAL] = 0.;	// Unused
		m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B1][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B2][DIFFERENTIAL] = 0.;	// Unused

		DEBUGCOUT("PredictForStageS(1)" << std::endl
			<< "Alpha = " << dAlpha << std::endl
			<< "Differential coefficients:" << std::endl
			<< "Asymptotic rho =" << m_dRho << std::endl 
			<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
			<< "a2    = not needed" << std::endl
			<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
			<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
			<< "b2    = not needed" << std::endl);
		break;

	case 2:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (1. - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_np[0] = 1.;
			//m_np[1] = 0.;

			//second-order prediction
			doublereal dalpha = (1. - 2. * m_gamma) / (2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((1. - 2. * m_gamma) * dT);
			m_mp[1] = -m_mp[0];
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);

			doublereal q0 = -(4. * m_gamma * m_gamma - 6. * m_gamma + 1.) / (4. * m_gamma);
			doublereal q1 = (1. - 2. * m_gamma) / (4. * m_gamma);

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 1.;
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = q1 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = q0 * dT;

			DEBUGCOUT("PredictForStageS(2)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl);
			break;
		}

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	m_a[IDX_A1][ALGEBRAIC] = m_a[IDX_A1][DIFFERENTIAL];
	m_a[IDX_A2][ALGEBRAIC] = m_a[IDX_A2][DIFFERENTIAL];
	m_b[IDX_B0][ALGEBRAIC] = m_b[IDX_B0][DIFFERENTIAL];
	m_b[IDX_B1][ALGEBRAIC] = m_b[IDX_B1][DIFFERENTIAL];
	m_b[IDX_B2][ALGEBRAIC] = m_b[IDX_B2][DIFFERENTIAL];

	DEBUGCOUT("Algebraic coefficients:" << std::endl
		<< "Asymptotic rho =" << m_dAlgebraicRho << " (ignored)" << std::endl 
		<< "a1    = " << m_a[IDX_A1][ALGEBRAIC] << std::endl
		<< "a2    = " << m_a[IDX_A2][ALGEBRAIC] << std::endl
		<< "b0    = " << m_b[IDX_B0][ALGEBRAIC] << std::endl
		<< "b1    = " << m_b[IDX_B1][ALGEBRAIC] << std::endl
		<< "b2    = " << m_b[IDX_B2][ALGEBRAIC] << std::endl);

	db0Differential = m_b[IDX_B0][DIFFERENTIAL];
	db0Algebraic = m_b[IDX_B0][ALGEBRAIC];
}

doublereal
TunableBatheSolver::dPredDerForStageS(unsigned uStage,
                                      const std::array<doublereal, 2>& dXm1mN,
                                      const std::array<doublereal, 3>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];//constant prediction 

	case 2:
		return m_mp[0]*dXm1mN[IDX_Xs1]
			+ m_mp[1]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs1]
			+ m_np[1]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
TunableBatheSolver::dPredStateForStageS(unsigned uStage,
                                        const std::array<doublereal, 2>& dXm1mN,
                                        const std::array<doublereal, 3>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
TunableBatheSolver::dPredDerAlgForStageS(unsigned uStage,
                                         const std::array<doublereal, 2>& dXm1mN,
                                         const std::array<doublereal, 3>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];

	case 2:
		return dXP0mN[IDX_XPs1]; // FIXME: at k-1 or at s1?

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
TunableBatheSolver::dPredStateAlgForStageS(unsigned uStage,
                                           const std::array<doublereal, 2>& dXm1mN,
                                           const std::array<doublereal, 3>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

/* TunableBatheSolver - end */

/* Msstc3Solver - begin */

Msstc3Solver::Msstc3Solver(const doublereal Tl,
	const doublereal dSolTl,
	const integer iMaxIt,
	const DriveCaller* pRho,
	const DriveCaller* pAlgRho,
	const bool bmod_res_test)
: tplStageNIntegrator<3>(iMaxIt, Tl, dSolTl, bmod_res_test),
m_Rho(pRho), m_AlgebraicRho(pAlgRho)
{
	ASSERT(pRho != 0);
	ASSERT(pAlgRho != 0);
}

Msstc3Solver::~Msstc3Solver(void)
{
	NO_OP;
}

void
Msstc3Solver::SetDriveHandler(const DriveHandler* pDH)
{
	m_Rho.pGetDriveCaller()->SetDrvHdl(pDH);
	m_AlgebraicRho.pGetDriveCaller()->SetDrvHdl(pDH);
}

void
Msstc3Solver::SetCoefForStageS(unsigned uStage,
	doublereal dT,
	doublereal dAlpha,
	enum StepChange /* NewStep */)
{
	switch (uStage) {
	case 1:
		m_dRho = m_Rho.dGet();
		m_dAlgebraicRho = m_AlgebraicRho.dGet();

		if (abs(m_dRho - 0.0) < 1.e-6)
		{
			m_gamma = 0.1804253064293983299659629;
		}else if (abs(m_dRho - 0.1) < 1.e-6)
		{
			m_gamma = 0.1786194582046580769940647;
		}else if (abs(m_dRho - 0.2) < 1.e-6)
		{
			m_gamma = 0.1769458066182237054864146;
		}else if (abs(m_dRho - 0.3) < 1.e-6)
		{
			m_gamma = 0.1753855158428457572394876;
		}else if (abs(m_dRho - 0.4) < 1.e-6)
		{
			m_gamma = 0.1739236078771971283352116;
		}else if (abs(m_dRho - 0.5) < 1.e-6)
		{
			m_gamma = 0.1725479614220893076481644;
		}else if (abs(m_dRho - 0.6) < 1.e-6)
		{
			m_gamma = 0.1712486185906910429732619;
		}else if (abs(m_dRho - 0.7) < 1.e-6)
		{
			m_gamma = 0.1700172917724764587443786;
		}else if (abs(m_dRho - 0.8) < 1.e-6)
		{
			m_gamma = 0.1688470046791676892894429;
		}else if (abs(m_dRho - 0.9) < 1.e-6)
		{
			m_gamma = 0.1677318257568867765350262;
		}else if (abs(m_dRho - 1.0) < 1.e-6)
		{
			m_gamma = 1./6.;
		}else 
		{
			silent_cerr("Please select rho in 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, and 1.0." << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		ASSERT(pDM != NULL);
		pDM->SetTime(pDM->dGetTime() - (1. - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

		m_a[IDX_A1][DIFFERENTIAL] = 1.;
		m_a[IDX_A2][DIFFERENTIAL] = 0.;	// Unused
		m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B1][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B2][DIFFERENTIAL] = 0.;	// Unused
		m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused

		DEBUGCOUT("PredictForStageS(1)" << std::endl
			<< "Alpha = " << dAlpha << std::endl
			<< "Differential coefficients:" << std::endl
			<< "Asymptotic rho =" << m_dRho << std::endl 
			<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
			<< "a2    = not needed" << std::endl
			<< "a3    = not needed" << std::endl
			<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
			<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
			<< "b2    = not needed" << std::endl
			<< "b3    = not needed" << std::endl);
		break;

	case 2:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + 2. * m_gamma * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = 1.;
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / (2. * m_gamma * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.; //Unused

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 1.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = 2. * m_gamma * dT;
			m_b[IDX_B2][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(2)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = not needed" << std::endl);
			break;
		}

	case 3:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (1. - 4. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;

			//second-order prediction
			doublereal dalpha = (1. - 4. * m_gamma) / (2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((1. - 4. * m_gamma) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.;
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.;

			//fourth-order prediction
			//doublereal dalpha = (1. - 4. * m_gamma) / (2. * m_gamma);
			//m_mp[0] = - dalpha*dalpha*(15.*dalpha*dalpha*dalpha + 68.*dalpha*dalpha + 99.*dalpha + 46.)/(4.*(1. - 4.*m_gamma)*dT);
			//m_mp[1] = 4.*dalpha*dalpha*(dalpha*dalpha + 3.*dalpha + 2.)/((1. - 4.*m_gamma)*dT);
			//m_mp[2] = - m_mp[0] - m_mp[1];
			//m_np[0] = 5.*dalpha*dalpha*dalpha*dalpha/4. + 6.*dalpha*dalpha*dalpha + 39.*dalpha*dalpha/4. + 6.*dalpha + 1.;
			//m_np[1] = dalpha*(5.*dalpha*dalpha*dalpha + 20.*dalpha*dalpha + 24.*dalpha + 8.);
			//m_np[2] = dalpha*(5.*dalpha*dalpha*dalpha + 16.*dalpha*dalpha + 15.*dalpha + 4.)/4.;

			doublereal q0 = ((2. * m_dRho - 6.) * m_gamma * m_gamma + 8. * m_gamma - 1.) / (8. * m_gamma);
			doublereal q1 = -((m_dRho + 1) * m_gamma - 1.) / 2.;
			doublereal q2 = (2. * (m_dRho + 1) * m_gamma * m_gamma - 4. * m_gamma + 1.) / (8. * m_gamma);

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 1.;
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = q2 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = q1 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = q0 * dT;

			DEBUGCOUT("PredictForStageS(3)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl);
			break;
		}

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	m_a[IDX_A1][ALGEBRAIC] = m_a[IDX_A1][DIFFERENTIAL];
	m_a[IDX_A2][ALGEBRAIC] = m_a[IDX_A2][DIFFERENTIAL];
	m_a[IDX_A3][ALGEBRAIC] = m_a[IDX_A3][DIFFERENTIAL];
	m_b[IDX_B0][ALGEBRAIC] = m_b[IDX_B0][DIFFERENTIAL];
	m_b[IDX_B1][ALGEBRAIC] = m_b[IDX_B1][DIFFERENTIAL];
	m_b[IDX_B2][ALGEBRAIC] = m_b[IDX_B2][DIFFERENTIAL];
	m_b[IDX_B3][ALGEBRAIC] = m_b[IDX_B3][DIFFERENTIAL];

	DEBUGCOUT("Algebraic coefficients:" << std::endl
		<< "Asymptotic rho =" << m_dAlgebraicRho << " (ignored)" << std::endl 
		<< "a1    = " << m_a[IDX_A1][ALGEBRAIC] << std::endl
		<< "a2    = " << m_a[IDX_A2][ALGEBRAIC] << std::endl
		<< "a3    = " << m_a[IDX_A3][ALGEBRAIC] << std::endl
		<< "b0    = " << m_b[IDX_B0][ALGEBRAIC] << std::endl
		<< "b1    = " << m_b[IDX_B1][ALGEBRAIC] << std::endl
		<< "b2    = " << m_b[IDX_B2][ALGEBRAIC] << std::endl
		<< "b3    = " << m_b[IDX_B3][ALGEBRAIC] << std::endl);

	db0Differential = m_b[IDX_B0][DIFFERENTIAL];
	db0Algebraic = m_b[IDX_B0][ALGEBRAIC];
}

doublereal
Msstc3Solver::dPredDerForStageS(unsigned uStage,
                                const std::array<doublereal, 3>& dXm1mN,
                                const std::array<doublereal, 4>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];//constant prediction

	case 2:
		return m_mp[0]*dXm1mN[IDX_Xs1]
			+ m_mp[1]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs1]
			+ m_np[1]*dXP0mN[IDX_XPm1]; 

	case 3:
		return m_mp[0]*dXm1mN[IDX_Xs2]
			+ m_mp[1]*dXm1mN[IDX_Xs1]
			+ m_mp[2]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs2]
			+ m_np[1]*dXP0mN[IDX_XPs1]
			+ m_np[2]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Msstc3Solver::dPredStateForStageS(unsigned uStage,
                                  const std::array<doublereal, 3>& dXm1mN,
                                  const std::array<doublereal, 4>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Msstc3Solver::dPredDerAlgForStageS(unsigned uStage,
                                   const std::array<doublereal, 3>& dXm1mN,
                                   const std::array<doublereal, 4>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];

	case 2:
		return dXP0mN[IDX_XPs1]; 

	case 3:
		return dXP0mN[IDX_XPs2];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Msstc3Solver::dPredStateAlgForStageS(unsigned uStage,
                                     const std::array<doublereal, 3>& dXm1mN,
                                     const std::array<doublereal, 4>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

/* Msstc3Solver - end */

/* Mssth3Solver - begin */

Mssth3Solver::Mssth3Solver(const doublereal Tl,
	const doublereal dSolTl,
	const integer iMaxIt,
	const DriveCaller* pRho,
	const DriveCaller* pAlgRho,
	const bool bmod_res_test)
: tplStageNIntegrator<3>(iMaxIt, Tl, dSolTl, bmod_res_test),
m_Rho(pRho), m_AlgebraicRho(pAlgRho)
{
	ASSERT(pRho != 0);
	ASSERT(pAlgRho != 0);
}

Mssth3Solver::~Mssth3Solver(void)
{
	NO_OP;
}

void
Mssth3Solver::SetDriveHandler(const DriveHandler* pDH)
{
	m_Rho.pGetDriveCaller()->SetDrvHdl(pDH);
	m_AlgebraicRho.pGetDriveCaller()->SetDrvHdl(pDH);
}

void
Mssth3Solver::SetCoefForStageS(unsigned uStage,
	doublereal dT,
	doublereal dAlpha,
	enum StepChange /* NewStep */)
{
	switch (uStage) {
	case 1:
		m_dRho = m_Rho.dGet();
		m_dAlgebraicRho = m_AlgebraicRho.dGet();

		if (abs(m_dRho - 0.0) < 1.e-6)
		{
			m_gamma = 0.4358665215084596700201303;
		}
		else if (abs(m_dRho - 0.1) < 1.e-6)
		{
			m_gamma = 0.4214868154094094654382729;
		}
		else if (abs(m_dRho - 0.2) < 1.e-6)
		{
			m_gamma = 0.4085007895129219579466451;
		}
		else if (abs(m_dRho - 0.3) < 1.e-6)
		{
			m_gamma = 0.3966472091211337147598215;
		}
		else if (abs(m_dRho - 0.4) < 1.e-6)
		{
			m_gamma = 0.3857310004608353604105275;
		}
		else if (abs(m_dRho - 0.5) < 1.e-6)
		{
			m_gamma = 0.3756022250152853403371012;
		}
		else if (abs(m_dRho - 0.6) < 1.e-6)
		{
			m_gamma = 0.3661428101033469273417609;
		}
		else if (abs(m_dRho - 0.7) < 1.e-6)
		{
			m_gamma = 0.3572578119672340513091058;
		}
		else if (abs(m_dRho - 0.8) < 1.e-6)
		{
			m_gamma = 0.3488694530748691624566504;
		}
		else if (abs(m_dRho - 0.9) < 1.e-6)
		{
			m_gamma = 0.3409129227719289167986005;
		}
		else if (abs(m_dRho - 1.0) < 1.e-6)
		{
			m_gamma = 1./3.;
		}else 
		{
			silent_cerr("Please select rho in 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, and 1.0." << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		m_c2 = (24. * m_gamma * m_gamma - 20. * m_gamma + 3.) / (24. * m_gamma * m_gamma - 24. * m_gamma + 4.);

		ASSERT(pDM != NULL);
		pDM->SetTime(pDM->dGetTime() - (1. - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

		m_a[IDX_A1][DIFFERENTIAL] = 1.;
		m_a[IDX_A2][DIFFERENTIAL] = 0.;	// Unused
		m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B1][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B2][DIFFERENTIAL] = 0.;	// Unused
		m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused

		DEBUGCOUT("PredictForStageS(1)" << std::endl
			<< "Alpha = " << dAlpha << std::endl
			<< "Differential coefficients:" << std::endl
			<< "Asymptotic rho =" << m_dRho << std::endl 
			<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
			<< "a2    = not needed" << std::endl
			<< "a3    = not needed" << std::endl
			<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
			<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
			<< "b2    = not needed" << std::endl
			<< "b3    = not needed" << std::endl);
		break;

	case 2:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (m_c2 - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = (m_c2 - 2. * m_gamma) / (2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((m_c2 - 2. * m_gamma) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.; //Unused

			doublereal a21 = m_c2 * (m_c2 - 2. * m_gamma) / (4. * m_gamma);
			doublereal a20 = m_c2 - m_gamma - a21;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 1.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = a21 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = a20 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(2)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = not needed" << std::endl);
			break;
		}

	case 3:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (1. - m_c2) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;

			//second-order prediction
			doublereal dalpha = (1. - m_c2) / (m_c2 - 2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((1. - m_c2) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.;
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.;

			//fourth-order prediction
			//doublereal dalpha1 = (1. - m_c2) / (m_c2 - 2. * m_gamma);
			//doublereal dalpha2 = (1. - m_c2) / (2. * m_gamma);
			//m_mp[0] = -(2. * dalpha1 * dalpha1 * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
			//		+ dalpha1 * dalpha1 * (10. * dalpha2 * dalpha2 * dalpha2 + 25. * dalpha2 * dalpha2 + 13. * dalpha2) 
			//		+ dalpha1 * (20. * dalpha2 * dalpha2 * dalpha2 + 20. * dalpha2 * dalpha2) + 10. * dalpha2 * dalpha2 * dalpha2)) 
			//		/ ((dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (1. - m_c2) * dT);
			//m_mp[1] = (2. * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
			//		+ dalpha1 * dalpha1 * (-5. * dalpha2 * dalpha2 * dalpha2 + dalpha2 * dalpha2 + 4. * dalpha2) 
			//		+ dalpha1 * (-7. * dalpha2 * dalpha2 * dalpha2 - dalpha2 * dalpha2) - 2. * dalpha2 * dalpha2 * dalpha2)) 
			//		/ (dalpha1 * (1. - m_c2) * dT);
			//m_mp[2] = (2. * dalpha2 * dalpha2 * dalpha2 * dalpha2 * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (10. * dalpha2 + 10.) 
			//		+ dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 20. * dalpha2 + 5.) + dalpha1 * (7. * dalpha2 * dalpha2 + 7. * dalpha2) 
			//		+ 2. * dalpha2 * dalpha2)) / (dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (1. - m_c2) * dT);
			//m_np[0] = ((1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
			//		+ dalpha1 * dalpha1 * (11. * dalpha2 * dalpha2 + 10. * dalpha2 + 1.) + dalpha1 * (7. * dalpha2 * dalpha2 + 2. * dalpha2) 
			//		+ dalpha2 * dalpha2)) / ((dalpha1 + dalpha2) * (dalpha1 + dalpha2));
			//m_np[1] = (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
			//		+ dalpha1 * dalpha1 * (12. * dalpha2 * dalpha2 + 12. * dalpha2 + 2.) + dalpha1 * (9. * dalpha2 * dalpha2 + 4. * dalpha2) 
			//		+ 2. * dalpha2 * dalpha2) / dalpha1;
			//m_np[2] = (dalpha2 * dalpha2 * dalpha2 * (1. + dalpha1) * (dalpha1 * dalpha1 * (5. * dalpha2 + 4.) 
			//		+ dalpha1 * (7. * dalpha2 + 2.) + 2. * dalpha2)) / (dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2));

			doublereal q1 = (3. * m_c2 + 6. * m_gamma - 6. * m_c2 * m_gamma - 2.) / (12. * m_gamma * (m_c2 - 2. * m_gamma));
			doublereal q2 = (6. * m_gamma * m_gamma - 6. * m_gamma + 1.) / (3. * m_c2 * (m_c2 - 2. * m_gamma));
			doublereal q0 = 1. - m_gamma - q1 - q2;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 1.;
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = q2 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = q1 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = q0 * dT;

			DEBUGCOUT("PredictForStageS(3)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl);
			break;
		}

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	m_a[IDX_A1][ALGEBRAIC] = m_a[IDX_A1][DIFFERENTIAL];
	m_a[IDX_A2][ALGEBRAIC] = m_a[IDX_A2][DIFFERENTIAL];
	m_a[IDX_A3][ALGEBRAIC] = m_a[IDX_A3][DIFFERENTIAL];
	m_b[IDX_B0][ALGEBRAIC] = m_b[IDX_B0][DIFFERENTIAL];
	m_b[IDX_B1][ALGEBRAIC] = m_b[IDX_B1][DIFFERENTIAL];
	m_b[IDX_B2][ALGEBRAIC] = m_b[IDX_B2][DIFFERENTIAL];
	m_b[IDX_B3][ALGEBRAIC] = m_b[IDX_B3][DIFFERENTIAL];

	DEBUGCOUT("Algebraic coefficients:" << std::endl
		<< "Asymptotic rho =" << m_dAlgebraicRho << " (ignored)" << std::endl 
		<< "a1    = " << m_a[IDX_A1][ALGEBRAIC] << std::endl
		<< "a2    = " << m_a[IDX_A2][ALGEBRAIC] << std::endl
		<< "a3    = " << m_a[IDX_A3][ALGEBRAIC] << std::endl
		<< "b0    = " << m_b[IDX_B0][ALGEBRAIC] << std::endl
		<< "b1    = " << m_b[IDX_B1][ALGEBRAIC] << std::endl
		<< "b2    = " << m_b[IDX_B2][ALGEBRAIC] << std::endl
		<< "b3    = " << m_b[IDX_B3][ALGEBRAIC] << std::endl);

	db0Differential = m_b[IDX_B0][DIFFERENTIAL];
	db0Algebraic = m_b[IDX_B0][ALGEBRAIC];
}

doublereal
Mssth3Solver::dPredDerForStageS(unsigned uStage,
                                const std::array<doublereal, 3>& dXm1mN,
                                const std::array<doublereal, 4>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];

	case 2:
		return m_mp[0]*dXm1mN[IDX_Xs1]
			+ m_mp[1]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs1]
			+ m_np[1]*dXP0mN[IDX_XPm1]; 

	case 3:
		return m_mp[0]*dXm1mN[IDX_Xs2]
			+ m_mp[1]*dXm1mN[IDX_Xs1]
			+ m_mp[2]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs2]
			+ m_np[1]*dXP0mN[IDX_XPs1]
			+ m_np[2]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Mssth3Solver::dPredStateForStageS(unsigned uStage,
                                  const std::array<doublereal, 3>& dXm1mN,
                                  const std::array<doublereal, 4>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Mssth3Solver::dPredDerAlgForStageS(unsigned uStage,
                                   const std::array<doublereal, 3>& dXm1mN,
                                   const std::array<doublereal, 4>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];

	case 2:
		return dXP0mN[IDX_XPs1]; 

	case 3:
		return dXP0mN[IDX_XPs2];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Mssth3Solver::dPredStateAlgForStageS(unsigned uStage,
                                     const std::array<doublereal, 3>& dXm1mN,
                                     const std::array<doublereal, 4>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPm1];


	case 3:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

/* Mssth3Solver - end */

/* Msstc4Solver - begin */

Msstc4Solver::Msstc4Solver(const doublereal Tl,
	const doublereal dSolTl,
	const integer iMaxIt,
	const DriveCaller* pRho,
	const DriveCaller* pAlgRho,
	const bool bmod_res_test)
: tplStageNIntegrator<4>(iMaxIt, Tl, dSolTl, bmod_res_test),
m_Rho(pRho), m_AlgebraicRho(pAlgRho)
{
	ASSERT(pRho != 0);
	ASSERT(pAlgRho != 0);
}

Msstc4Solver::~Msstc4Solver(void)
{
	NO_OP;
}

void
Msstc4Solver::SetDriveHandler(const DriveHandler* pDH)
{
	m_Rho.pGetDriveCaller()->SetDrvHdl(pDH);
	m_AlgebraicRho.pGetDriveCaller()->SetDrvHdl(pDH);
}

void
Msstc4Solver::SetCoefForStageS(unsigned uStage,
	doublereal dT,
	doublereal dAlpha,
	enum StepChange /* NewStep */)
{
	switch (uStage) {
	case 1:
		m_dRho = m_Rho.dGet();
		m_dAlgebraicRho = m_AlgebraicRho.dGet();

		if (abs(m_dRho - 0.0) < 1.e-6)
		{
			m_gamma = 0.1313787367304988795702059;
		}else if (abs(m_dRho - 0.1) < 1.e-6)
		{
			m_gamma = 0.1305486209465960556475039;
		}else if (abs(m_dRho - 0.2) < 1.e-6)
		{
			m_gamma = 0.129777583818700997797535;
		}else if (abs(m_dRho - 0.3) < 1.e-6)
		{
			m_gamma = 0.1290572072571367356896843;
		}else if (abs(m_dRho - 0.4) < 1.e-6)
		{
			m_gamma = 0.1283808049199898215775306;
		}else if (abs(m_dRho - 0.5) < 1.e-6)
		{
			m_gamma = 0.1277429705569184392732751;
		}else if (abs(m_dRho - 0.6) < 1.e-6)
		{
			m_gamma = 0.1271392659020577631245885;
		}else if (abs(m_dRho - 0.7) < 1.e-6)
		{
			m_gamma = 0.1265659990831402292865704;
		}else if (abs(m_dRho - 0.8) < 1.e-6)
		{
			m_gamma = 0.1260200635863823193094646;
		}else if (abs(m_dRho - 0.9) < 1.e-6)
		{
			m_gamma = 0.1254988188303918228427847;
		}else if (abs(m_dRho - 1.0) < 1.e-6)
		{
			m_gamma = 1./8.;
		}else 
		{
			silent_cerr("Please select rho in 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, and 1.0." << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		ASSERT(pDM != NULL);
		pDM->SetTime(pDM->dGetTime() - (1. - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

		m_a[IDX_A1][DIFFERENTIAL] = 1.;
		m_a[IDX_A2][DIFFERENTIAL] = 0.;	// Unused
		m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
		m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B1][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B2][DIFFERENTIAL] = 0.;	// Unused
		m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused

		DEBUGCOUT("PredictForStageS(1)" << std::endl
			<< "Alpha = " << dAlpha << std::endl
			<< "Differential coefficients:" << std::endl
			<< "Asymptotic rho =" << m_dRho << std::endl 
			<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
			<< "a2    = not needed" << std::endl
			<< "a3    = not needed" << std::endl
			<< "a4    = not needed" << std::endl
			<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
			<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
			<< "b2    = not needed" << std::endl
			<< "b3    = not needed" << std::endl
			<< "b4    = not needed" << std::endl);
		break;

	case 2:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + 2. * m_gamma * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;//Unused
			//m_mp[3] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;//Unused
			//m_np[3] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = 1.;
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / (2. * m_gamma * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.; //Unused
			m_mp[3] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.; //Unused
			m_np[3] = 0.; //Unused

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 1.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
			m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = 2. * m_gamma * dT;
			m_b[IDX_B2][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(2)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = not needed" << std::endl
											<< "a4    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = not needed" << std::endl
											<< "b4    = not needed" << std::endl);
			break;
		}

	case 3:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + 2. * m_gamma * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;
			//m_np[3] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = 1.;
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / (2. * m_gamma * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.;
			m_mp[3] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.;
			m_np[3] = 0.; //Unused

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 1.;
			m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = 2. * m_gamma * dT;
			m_b[IDX_B2][DIFFERENTIAL] = 2. * m_gamma * dT;
			m_b[IDX_B3][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(3)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "a4    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl
											<< "b4    =  not needed" << std::endl);
			break;
		}

	case 4:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (1. - 6. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;
			//m_np[3] = 0.;

			//second-order prediction
			doublereal dalpha = (1. - 6. * m_gamma) / (2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((1. - 6. * m_gamma) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.;
			m_mp[3] = 0.;
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.;
			m_np[3] = 0.;

			//fourth-order prediction
			//doublereal dalpha = (1. - 6. * m_gamma) / (2. * m_gamma);
			//m_mp[0] = - dalpha*dalpha*(15.*dalpha*dalpha*dalpha + 68.*dalpha*dalpha + 99.*dalpha + 46.)/(4.*(1. - 6.*m_gamma)*dT);
			//m_mp[1] = 4.*dalpha*dalpha*(dalpha*dalpha + 3.*dalpha + 2.)/((1. - 6.*m_gamma)*dT);
			//m_mp[2] = - m_mp[0] - m_mp[1];
			//m_mp[3] = 0.;
			//m_np[0] = 5.*dalpha*dalpha*dalpha*dalpha/4. + 6.*dalpha*dalpha*dalpha + 39.*dalpha*dalpha/4. + 6.*dalpha + 1.;
			//m_np[1] = dalpha*(5.*dalpha*dalpha*dalpha + 20.*dalpha*dalpha + 24.*dalpha + 8.);
			//m_np[2] = dalpha*(5.*dalpha*dalpha*dalpha + 16.*dalpha*dalpha + 15.*dalpha + 4.)/4.;
			//m_np[3] = 0.;

			//sixth-order prediction
			//doublereal dalpha = (1. - 6. * m_gamma) / (2. * m_gamma);
			//m_mp[0] = - dalpha*dalpha*(77.*dalpha*dalpha*dalpha*dalpha*dalpha + 774.*dalpha*dalpha*dalpha*dalpha + 3010.*dalpha*dalpha*dalpha + 5640.*dalpha*dalpha + 5073.*dalpha + 1746.)/(108.*(1. - 6.*m_gamma)*dT);
			//m_mp[1] = - dalpha*dalpha*dalpha*(7.*dalpha*dalpha*dalpha*dalpha + 60.*dalpha*dalpha*dalpha + 185.*dalpha*dalpha + 240.*dalpha + 108.)/(4.*(1. - 6.*m_gamma)*dT);
			//m_mp[2] = dalpha*dalpha*(dalpha + 3.)*(dalpha + 3.)*(7.*dalpha*dalpha*dalpha + 24.*dalpha*dalpha + 23.*dalpha + 6.)/(4.*(1. - 6.*m_gamma)*dT);
			//m_mp[3] = - m_mp[0] - m_mp[1] - m_mp[2];
			//m_np[0] = 7.*dalpha*dalpha*dalpha*dalpha*dalpha*dalpha/36. + 2.*dalpha*dalpha*dalpha*dalpha*dalpha + 145.*dalpha*dalpha*dalpha*dalpha/18. + 16.*dalpha*dalpha*dalpha + 193.*dalpha*dalpha/12. + 22.*dalpha/3. + 1.;
			//m_np[1] = dalpha*(7.*dalpha*dalpha*dalpha*dalpha*dalpha + 66.*dalpha*dalpha*dalpha*dalpha + 235.*dalpha*dalpha*dalpha + 388.*dalpha*dalpha + 288.*dalpha + 72.)/4.;
			//m_np[2] = dalpha*(7.*dalpha*dalpha*dalpha*dalpha*dalpha + 60.*dalpha*dalpha*dalpha*dalpha + 190.*dalpha*dalpha*dalpha + 272.*dalpha*dalpha + 171.*dalpha + 36.)/4.;
			//m_np[3] = dalpha*(7.*dalpha*dalpha*dalpha*dalpha*dalpha + 54.*dalpha*dalpha*dalpha*dalpha + 155.*dalpha*dalpha*dalpha + 204.*dalpha*dalpha + 120.*dalpha + 24.)/36.;

			doublereal q0 = ((40. * m_dRho + 216.) * m_gamma * m_gamma * m_gamma * m_gamma - (8. * m_dRho + 376.) * m_gamma * m_gamma * m_gamma + 144. * m_gamma * m_gamma - 20. * m_gamma + 1.) / (64. * m_gamma * m_gamma * (1. - 4. * m_gamma));
			doublereal q1 = -(104. * (m_dRho - 1.) * m_gamma * m_gamma * m_gamma * m_gamma - (24. * m_dRho - 88.) * m_gamma * m_gamma * m_gamma + 16. * m_gamma * m_gamma - 12. * m_gamma + 1.) / (64. * m_gamma * m_gamma * (1. - 4. * m_gamma));
			doublereal q2 = -((-88 * m_dRho + 88) * m_gamma * m_gamma * m_gamma * m_gamma + (24. * m_dRho - 216.) * m_gamma * m_gamma * m_gamma + 112. * m_gamma * m_gamma - 20. * m_gamma + 1.) / (64. * m_gamma * m_gamma * (1. - 4. * m_gamma));
			doublereal q3 = ((-24 * m_dRho + 24.) * m_gamma * m_gamma * m_gamma * m_gamma + (8. * m_dRho - 72.) * m_gamma * m_gamma * m_gamma + 48. * m_gamma * m_gamma - 12. * m_gamma + 1.) / (64. * m_gamma * m_gamma * (1. - 4. * m_gamma));

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.;
			m_a[IDX_A4][DIFFERENTIAL] = 1.;
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = q3 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = q2 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = q1 * dT;
			m_b[IDX_B4][DIFFERENTIAL] = q0 * dT;

			DEBUGCOUT("PredictForStageS(4)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "a4    = " << m_a[IDX_A4][DIFFERENTIAL] << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl
											<< "b4    = " << m_b[IDX_B4][DIFFERENTIAL] << std::endl);
			break;
		}

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	m_a[IDX_A1][ALGEBRAIC] = m_a[IDX_A1][DIFFERENTIAL];
	m_a[IDX_A2][ALGEBRAIC] = m_a[IDX_A2][DIFFERENTIAL];
	m_a[IDX_A3][ALGEBRAIC] = m_a[IDX_A3][DIFFERENTIAL];
	m_a[IDX_A4][ALGEBRAIC] = m_a[IDX_A4][DIFFERENTIAL];
	m_b[IDX_B0][ALGEBRAIC] = m_b[IDX_B0][DIFFERENTIAL];
	m_b[IDX_B1][ALGEBRAIC] = m_b[IDX_B1][DIFFERENTIAL];
	m_b[IDX_B2][ALGEBRAIC] = m_b[IDX_B2][DIFFERENTIAL];
	m_b[IDX_B3][ALGEBRAIC] = m_b[IDX_B3][DIFFERENTIAL];
	m_b[IDX_B4][ALGEBRAIC] = m_b[IDX_B4][DIFFERENTIAL];

	DEBUGCOUT("Algebraic coefficients:" << std::endl
		<< "Asymptotic rho =" << m_dAlgebraicRho << " (ignored)" << std::endl 
		<< "a1    = " << m_a[IDX_A1][ALGEBRAIC] << std::endl
		<< "a2    = " << m_a[IDX_A2][ALGEBRAIC] << std::endl
		<< "a3    = " << m_a[IDX_A3][ALGEBRAIC] << std::endl
		<< "a4    = " << m_a[IDX_A4][ALGEBRAIC] << std::endl
		<< "b0    = " << m_b[IDX_B0][ALGEBRAIC] << std::endl
		<< "b1    = " << m_b[IDX_B1][ALGEBRAIC] << std::endl
		<< "b2    = " << m_b[IDX_B2][ALGEBRAIC] << std::endl
		<< "b3    = " << m_b[IDX_B3][ALGEBRAIC] << std::endl
		<< "b4    = " << m_b[IDX_B4][ALGEBRAIC] << std::endl);

	db0Differential = m_b[IDX_B0][DIFFERENTIAL];
	db0Algebraic = m_b[IDX_B0][ALGEBRAIC];
}

doublereal
Msstc4Solver::dPredDerForStageS(unsigned uStage,
                                const std::array<doublereal, 4>& dXm1mN,
                                const std::array<doublereal, 5>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];//constant prediction

	case 2:
		return m_mp[0]*dXm1mN[IDX_Xs1]
			+ m_mp[1]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs1]
			+ m_np[1]*dXP0mN[IDX_XPm1];   

	case 3:
		return m_mp[0]*dXm1mN[IDX_Xs2]
			+ m_mp[1]*dXm1mN[IDX_Xs1]
			+ m_mp[2]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs2]
			+ m_np[1]*dXP0mN[IDX_XPs1]
			+ m_np[2]*dXP0mN[IDX_XPm1]; 

	case 4:
		return m_mp[0]*dXm1mN[IDX_Xs3]
			+ m_mp[1]*dXm1mN[IDX_Xs2]
			+ m_mp[2]*dXm1mN[IDX_Xs1]
			+ m_mp[3]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs3]
			+ m_np[1]*dXP0mN[IDX_XPs2]
			+ m_np[2]*dXP0mN[IDX_XPs1]
			+ m_np[3]*dXP0mN[IDX_XPm1]; 

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Msstc4Solver::dPredStateForStageS(unsigned uStage,
                                  const std::array<doublereal, 4>& dXm1mN,
                                  const std::array<doublereal, 5>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 4:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A4][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B4][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Msstc4Solver::dPredDerAlgForStageS(unsigned uStage,
                                   const std::array<doublereal, 4>& dXm1mN,
                                   const std::array<doublereal, 5>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];

	case 2:
		return dXP0mN[IDX_XPs1]; 

	case 3:
		return dXP0mN[IDX_XPs2];

	case 4:
		return dXP0mN[IDX_XPs3];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Msstc4Solver::dPredStateAlgForStageS(unsigned uStage,
                                     const std::array<doublereal, 4>& dXm1mN,
                                     const std::array<doublereal, 5>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 4:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A4][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B4][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

/* Msstc4Solver - end */

/* Mssth4Solver - begin */

Mssth4Solver::Mssth4Solver(const doublereal Tl,
	const doublereal dSolTl,
	const integer iMaxIt,
	const DriveCaller* pRho,
	const DriveCaller* pAlgRho,
	const bool bmod_res_test)
: tplStageNIntegrator<4>(iMaxIt, Tl, dSolTl, bmod_res_test),
m_Rho(pRho), m_AlgebraicRho(pAlgRho)
{
	ASSERT(pRho != 0);
	ASSERT(pAlgRho != 0);
}

Mssth4Solver::~Mssth4Solver(void)
{
	NO_OP;
}

void
Mssth4Solver::SetDriveHandler(const DriveHandler* pDH)
{
	m_Rho.pGetDriveCaller()->SetDrvHdl(pDH);
	m_AlgebraicRho.pGetDriveCaller()->SetDrvHdl(pDH);
}

void
Mssth4Solver::SetCoefForStageS(unsigned uStage,
	doublereal dT,
	doublereal dAlpha,
	enum StepChange /* NewStep */)
{
	switch (uStage) {
	case 1:
		m_dRho = m_Rho.dGet();
		m_dAlgebraicRho = m_AlgebraicRho.dGet();

		if (abs(m_dRho - 0.0) < 1.e-6)
		{
			m_gamma = 0.5728160624821350133117903;
			m_c2 = 0.5590985754229417305152542;
			m_c3 = 0.7414011664833654036144139;
		}
		else if (abs(m_dRho - 0.1) < 1.e-6)
		{
			m_gamma = 0.5483666449758298755412511;
			m_c2 = 0.6002938888698324121975147;
			m_c3 = 0.7584129875780372120885886;
		}
		else if (abs(m_dRho - 0.2) < 1.e-6)
		{
			m_gamma = 0.5263864568423862744239727;
			m_c2 = 0.638522814489160595332861;
			m_c3 = 0.7731436659604612460228168;
		}
		else if (abs(m_dRho - 0.3) < 1.e-6)
		{
			m_gamma = 0.5063301189707819505159136;
			m_c2 = 0.67524540713318077522649;
			m_c3 = 0.7860312064122737529814344;
		}
		else if (abs(m_dRho - 0.4) < 1.e-6)
		{
			m_gamma = 0.487797474812348086370406;
			m_c2 = 0.7116626313535582440037008;
			m_c3 = 0.7972514819203143643377985;
		}
		else if (abs(m_dRho - 0.5) < 1.e-6)
		{
			m_gamma = 0.4704805776216768320452388;
			m_c2 = 0.7489373901316857945701066;
			m_c3 = 0.8067140747427041791439706;
		}
		else if (abs(m_dRho - 0.6) < 1.e-6)
		{
			m_gamma = 0.4541307850365287057670116;
			m_c2 = 0.7884370115210036155119526;
			m_c3 = 0.813966252941913492868764;
		}
		else if (abs(m_dRho - 0.7) < 1.e-6)
		{
			m_gamma = 0.4385361899021925080610629;
			m_c2 = 0.8321495959968309360981758;
			m_c3 = 0.8179301032006714988753515;
		}
		else if (abs(m_dRho - 0.8) < 1.e-6)
		{
			m_gamma = 0.4235037660671788772859259;
			m_c2 = 0.8836585039419085905336449;
			m_c3 = 0.8162478946500242305006623;
		}
		else if (abs(m_dRho - 0.9) < 1.e-6)
		{
			m_gamma = 0.4088418661206993376389107;
			m_c2 = 0.9508833135343227253337252;
			m_c3 = 0.8036295352568830763217989;
		}
		else if (abs(m_dRho - 1.0) < 1.e-6)
		{
			m_gamma = 0.3943375672974065437870195;
			m_c2 = 1.05348034117028673017024;
			m_c3 = 0.7689305362617052663765094;
		}
		else
		{
			silent_cerr("Please select rho in 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, and 1.0." << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		ASSERT(pDM != NULL);
		pDM->SetTime(pDM->dGetTime() - (1. - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

		m_a[IDX_A1][DIFFERENTIAL] = 1.;
		m_a[IDX_A2][DIFFERENTIAL] = 0.;	// Unused
		m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
		m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B1][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B2][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused

		DEBUGCOUT("PredictForStageS(1)" << std::endl
			<< "Alpha = " << dAlpha << std::endl
			<< "Differential coefficients:" << std::endl
			<< "Asymptotic rho =" << m_dRho << std::endl 
			<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
			<< "a2    = not needed" << std::endl
			<< "a3    = not needed" << std::endl
			<< "a4    = not needed" << std::endl
			<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
			<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
			<< "b2    = not needed" << std::endl
			<< "b3    = not needed" << std::endl
			<< "b4    = not needed" << std::endl);
		break;

	case 2:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (m_c2 - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;//Unused
			//m_mp[3] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;//Unused
			//m_np[3] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = (m_c2 - 2. * m_gamma) / (2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((m_c2 - 2. * m_gamma) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.; //Unused
			m_mp[3] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.; //Unused
			m_np[3] = 0.; //Unused

			doublereal a21 = m_c2 * (m_c2 - 2. * m_gamma) / (4. * m_gamma);
			doublereal a20 = m_c2 - m_gamma - a21;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 1.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
			m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = a21 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = a20 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(2)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = not needed" << std::endl
											<< "a4    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = not needed" << std::endl
											<< "b4    = not needed" << std::endl);
			break;
		}

	case 3:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (m_c3 - m_c2) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;
			//m_np[3] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = (m_c3 - m_c2) / (m_c2 - 2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((m_c3 - m_c2) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.;
			m_mp[3] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.;
			m_np[3] = 0.; //Unused

			doublereal a31 = (m_c3 * (m_c3 - 2. * m_gamma) * (2. * m_c3 - 5. * m_c2 + 6. * m_gamma + 36. * m_c2 * m_gamma - 24. * m_c3 * m_gamma 
						- 48. * m_c2 * m_gamma * m_gamma - 24. * m_c2 * m_c2 * m_gamma + 72. * m_c3 * m_gamma * m_gamma - 48. * m_c3 * m_gamma * m_gamma * m_gamma 
						+ 4. * m_c2 * m_c2 - 40. * m_gamma * m_gamma + 48. * m_gamma * m_gamma * m_gamma + 24. * m_c2 * m_c2 * m_gamma * m_gamma)) / (4. * m_gamma * (24. * m_c2 * m_c2 * m_gamma * m_gamma - 24. * m_c2 * m_c2 * m_gamma + 4. * m_c2 * m_c2 - 48. * m_c2 * m_gamma * m_gamma * m_gamma + 24. * m_c2 * m_gamma * m_gamma + 12. * m_c2 * m_gamma - 3. * m_c2 + 48. * m_gamma * m_gamma * m_gamma - 40. * m_gamma * m_gamma + 6. * m_gamma));
			doublereal a32 = -(m_c3 * (m_c2 - m_c3) * (m_c3 - 2. * m_gamma) * (24. * m_gamma * m_gamma * m_gamma - 36. * m_gamma * m_gamma + 12. * m_gamma - 1.)) 
						/ (m_c2 * (24. * m_c2 * m_c2 * m_gamma * m_gamma - 24. * m_c2 * m_c2 * m_gamma + 4. * m_c2 * m_c2 - 48. * m_c2 * m_gamma * m_gamma * m_gamma + 24. * m_c2 * m_gamma * m_gamma + 12. * m_c2 * m_gamma - 3. * m_c2 + 48. * m_gamma * m_gamma * m_gamma - 40. * m_gamma * m_gamma + 6. * m_gamma));
			doublereal a30 = m_c3 - m_gamma - a31 - a32;


			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 1.;
			m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = a32 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = a31 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = a30 * dT;
			m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(3)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "a4    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl
											<< "b4    = not needed" << std::endl);
			break;
		}

	case 4:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (1. - m_c3) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;
			//m_np[3] = 0.;

			//second-order prediction
			//doublereal dalpha = (1. - m_c3) / (m_c3 - m_c2);
			//m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((1. - m_c3) * dT);
			//m_mp[1] = -m_mp[0];
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;
			//m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			//m_np[1] = dalpha * (2. + 3. * dalpha);
			//m_np[2] = 0.;
			//m_np[3] = 0.;

			//fourth-order prediction
			doublereal dalpha1 = (1. - m_c3) / (m_c3 - m_c2);
			doublereal dalpha2 = (1. - m_c3) / (m_c2 - 2. * m_gamma);
			m_mp[0] = -(2. * dalpha1 * dalpha1 * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
					+ dalpha1 * dalpha1 * (10. * dalpha2 * dalpha2 * dalpha2 + 25. * dalpha2 * dalpha2 + 13. * dalpha2) 
					+ dalpha1 * (20. * dalpha2 * dalpha2 * dalpha2 + 20. * dalpha2 * dalpha2) + 10. * dalpha2 * dalpha2 * dalpha2)) 
					/ ((dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (1. - m_c3) * dT);
			m_mp[1] = (2. * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
					+ dalpha1 * dalpha1 * (-5. * dalpha2 * dalpha2 * dalpha2 + dalpha2 * dalpha2 + 4. * dalpha2) 
					+ dalpha1 * (-7. * dalpha2 * dalpha2 * dalpha2 - dalpha2 * dalpha2) - 2. * dalpha2 * dalpha2 * dalpha2)) 
					/ (dalpha1 * (1. - m_c3) * dT);
			m_mp[2] = (2. * dalpha2 * dalpha2 * dalpha2 * dalpha2 * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (10. * dalpha2 + 10.) 
					+ dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 20. * dalpha2 + 5.) + dalpha1 * (7. * dalpha2 * dalpha2 + 7. * dalpha2) 
					+ 2. * dalpha2 * dalpha2)) / (dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (1. - m_c3) * dT);
			m_mp[3] = 0.;
			m_np[0] = ((1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
					+ dalpha1 * dalpha1 * (11. * dalpha2 * dalpha2 + 10. * dalpha2 + 1.) + dalpha1 * (7. * dalpha2 * dalpha2 + 2. * dalpha2) 
					+ dalpha2 * dalpha2)) / ((dalpha1 + dalpha2) * (dalpha1 + dalpha2));
			m_np[1] = (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
					+ dalpha1 * dalpha1 * (12. * dalpha2 * dalpha2 + 12. * dalpha2 + 2.) + dalpha1 * (9. * dalpha2 * dalpha2 + 4. * dalpha2) 
					+ 2. * dalpha2 * dalpha2) / dalpha1;
			m_np[2] = (dalpha2 * dalpha2 * dalpha2 * (1. + dalpha1) * (dalpha1 * dalpha1 * (5. * dalpha2 + 4.) 
					+ dalpha1 * (7. * dalpha2 + 2.) + 2. * dalpha2)) / (dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2));
			m_np[3] = 0.;

			//sixth-order prediction
			//doublereal dalpha1 = (1. - m_c3) / (m_c3 - m_c2);
			//doublereal dalpha2 = (1. - m_c3) / (m_c2 - 2. * m_gamma);
			//doublereal dalpha3 = (1. - m_c3) / (2. * m_gamma);
			//m_mp[0] = -(2. * (dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (21. * pow(dalpha1, 5) + 77. * pow(dalpha1, 4) * dalpha2 + 49. * pow(dalpha1, 4) * dalpha3 + 63. * pow(dalpha1, 4) + 108. * pow(dalpha1, 3) * pow(dalpha2, 2) 
			//		+ 136. * pow(dalpha1, 3) * dalpha2 * dalpha3 + 182. * pow(dalpha1, 3) * dalpha2 + 38. * pow(dalpha1, 3) * pow(dalpha3, 2) + 112. * pow(dalpha1, 3) * dalpha3 + 63. * pow(dalpha1, 3) + 72. * pow(dalpha1, 2) * pow(dalpha2, 3) 
			//		+ 134. * dalpha1 * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 188. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 72. * dalpha1 * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 230. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 133. * dalpha1 * dalpha1 * dalpha2 
			//		+ 10. * dalpha1 * dalpha1 * dalpha3 * dalpha3 * dalpha3 + 62. * dalpha1 * dalpha1 * dalpha3 * dalpha3 + 77. * dalpha1 * dalpha1 * dalpha3 + 21. * dalpha1 * dalpha1 + 23. * dalpha1 * pow(dalpha2, 4) + 56. * dalpha1 * pow(dalpha2, 3) * dalpha3 
			//		+ 82. * dalpha1 * pow(dalpha2, 3) + 43. * dalpha1 * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 149. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 87. * dalpha1 * dalpha2 * dalpha2 + 10. * dalpha1 * dalpha2 * pow(dalpha3, 3) + 77. * dalpha1 * dalpha2 * dalpha3 * dalpha3 
			//		+ 101. * dalpha1 * dalpha2 * dalpha3 + 28. * dalpha1 * dalpha2 + 10. * dalpha1 * pow(dalpha3, 3) + 24. * dalpha1 * dalpha3 * dalpha3 + 14. * dalpha1 * dalpha3 + 3. * pow(dalpha2, 5) + 9. * pow(dalpha2, 4) * dalpha3 + 13. * pow(dalpha2, 4) + 9. * pow(dalpha2, 3) * dalpha3 * dalpha3 
			//		+ 31. * pow(dalpha2, 3) * dalpha3 + 17. * pow(dalpha2, 3) + 3. * dalpha2 * dalpha2 * pow(dalpha3, 3) + 23. * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 29. * dalpha2 * dalpha2 * dalpha3 + 7. * dalpha2 * dalpha2 + 5. * dalpha2 * pow(dalpha3, 3) + 12. * dalpha2 * pow(dalpha3, 2) + 7. * dalpha2 * dalpha3)) 
			//		/ (pow(dalpha1, 3) * pow(dalpha1 + dalpha2, 3) * pow(dalpha1 + dalpha2 + dalpha3, 3) * (1. - m_c3) * dT);
			//m_mp[1] = -(2. * (dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (4. * pow(dalpha1, 4) * dalpha2 + 2. * pow(dalpha1, 4) * dalpha3 + 9. * pow(dalpha1, 3) * pow(dalpha2, 2) + 11. * pow(dalpha1, 3) * dalpha2 * dalpha3 + 22. * pow(dalpha1, 3) * dalpha2 
			//		+ 4. * pow(dalpha1, 3) * pow(dalpha3, 2) + 11. * pow(dalpha1, 3) * dalpha3 + 3. * pow(dalpha1, 2) * pow(dalpha2, 3) + 7. * pow(dalpha1, 2) * pow(dalpha2, 2) * dalpha3 + 31. * pow(dalpha1, 2) * pow(dalpha2, 2) + 6. * pow(dalpha1, 2) * dalpha2 * pow(dalpha3, 2) 
			//		+ 40. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 32. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * pow(dalpha3, 3) + 16. * dalpha1 * dalpha1 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 - 5. * dalpha1 * pow(dalpha2, 4) 
			//		- 11. * dalpha1 * pow(dalpha2, 3) * dalpha3 - 4. * dalpha1 * pow(dalpha2, 3) - 7. * dalpha1 * dalpha2 * dalpha2 * dalpha3 * dalpha3 - 2. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 15. * dalpha1 * dalpha2 * dalpha2 - dalpha1 * dalpha2 * pow(dalpha3, 3) 
			//		+ 7. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 22. * dalpha1 * dalpha2 * dalpha3 + 14. * dalpha1 * dalpha2 + 5. * dalpha1 * pow(dalpha3, 3) + 12. * dalpha1 * dalpha3 * dalpha3 + 7. * dalpha1 * dalpha3 - 3. * pow(dalpha2, 5) - 9. * pow(dalpha2, 4) * dalpha3 - 13. * pow(dalpha2, 4) 
			//		- 9. * pow(dalpha2, 3) * dalpha3 * dalpha3 - 31. * pow(dalpha2, 3) * dalpha3 - 17. * pow(dalpha2, 3) - 3. * dalpha2 * dalpha2 * pow(dalpha3, 3) - 23. * dalpha2 * dalpha2 * dalpha3 * dalpha3 - 29. * dalpha2 * dalpha2 * dalpha3 - 7. * dalpha2 * dalpha2 - 5. * dalpha2 * pow(dalpha3, 3) 
			//		- 12. * dalpha2 * dalpha3 * dalpha3 - 7. * dalpha2 * dalpha3)) / (pow(dalpha1, 3) * pow(dalpha2, 3) * pow(dalpha2 + dalpha3, 3) * (1. - m_c3) * dT);
			//m_mp[2] = (2. * (dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (-2. * pow(dalpha1, 4) * dalpha2 + 2. * pow(dalpha1, 4) * dalpha3 - 6. * pow(dalpha1, 3) * pow(dalpha2, 2) + 5. * pow(dalpha1, 3) * dalpha2 * dalpha3 - 11. * pow(dalpha1, 3) * dalpha2 + 4. * pow(dalpha1, 3) * pow(dalpha3, 2) 
			//		+ 11. * pow(dalpha1, 3) * dalpha3 - 6. * dalpha1 * dalpha1 * pow(dalpha2, 3) + 4. * dalpha1 * dalpha1 * dalpha2 * dalpha2 * dalpha3 - 26. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 12. * dalpha1 * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 25. * dalpha1 * dalpha1 * dalpha2 * dalpha3 
			//		- 16. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 - 2. * dalpha1 * pow(dalpha2, 4) + dalpha1 * pow(dalpha2, 3) * dalpha3 - 19. * dalpha1 * pow(dalpha2, 3) 
			//		+ 8. * dalpha1 * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha2 * dalpha2 * dalpha3 - 27. * dalpha1 * dalpha2 * dalpha2 + 5. * dalpha1 * dalpha2 * pow(dalpha3, 3) + 40. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 34. * dalpha1 * dalpha2 * dalpha3 - 7. * dalpha1 * dalpha2 
			//		+ 5. * dalpha1 * pow(dalpha3, 3) + 12. * dalpha1 * dalpha3 * dalpha3 + 7. * dalpha1 * dalpha3 - 4. * pow(dalpha2, 4) + 2. * pow(dalpha2, 3) * dalpha3 - 11. * pow(dalpha2, 3) + 16. * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 13. * dalpha2 * dalpha2 * dalpha3 - 7. * dalpha2 * dalpha2 
			//		+ 10. * dalpha2 * dalpha3 * dalpha3 * dalpha3 + 24. * dalpha2 * dalpha3 * dalpha3 + 14. * dalpha2 * dalpha3)) / (dalpha2 * dalpha2 * dalpha2 * dalpha3 * dalpha3 * dalpha3 * pow(dalpha1 + dalpha2, 3) * (1. - m_c3) * dT);
			//m_mp[3] = -m_mp[0] - m_mp[1] - m_mp[2];
			//m_np[0] = ((dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (pow(dalpha1, 4) + 3. * pow(dalpha1, 3) * dalpha2 + 2. * pow(dalpha1, 3) * dalpha3 + 10. * pow(dalpha1, 3) + 3. * pow(dalpha1, 2) * pow(dalpha2, 2) + 4. * pow(dalpha1, 2) * dalpha2 * dalpha3 + 23. * pow(dalpha1, 2) * dalpha2 
			//		+ dalpha1 * dalpha1 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 + 24. * dalpha1 * dalpha1 + dalpha1 * pow(dalpha2, 3) + 2. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 16. * dalpha1 * dalpha2 * dalpha2 + dalpha1 * dalpha2 * dalpha3 * dalpha3 + 22. * dalpha1 * dalpha2 * dalpha3 
			//		+ 37. * dalpha1 * dalpha2 + 6. * dalpha1 * dalpha3 * dalpha3 + 26. * dalpha1 * dalpha3 + 22. * dalpha1 + 3. * pow(dalpha2, 3) + 6. * dalpha2 * dalpha2 * dalpha3 + 13. * dalpha2 * dalpha2 + 3. * dalpha2 * dalpha3 * dalpha3 + 18. * dalpha2 * dalpha3 + 17. * dalpha2 + 5. * dalpha3 * dalpha3 
			//		+ 12. * dalpha3 + 7.)) / (dalpha1 * dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2 + dalpha3) * (dalpha1 + dalpha2 + dalpha3));
			//m_np[1] = ((dalpha1 + dalpha2 + 1.) * (2. * pow(dalpha1, 4) + 6. * pow(dalpha1, 3) * dalpha2 + 4. * pow(dalpha1, 3) * dalpha3 + 13. * pow(dalpha1, 3) + 6. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 8. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 29. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 * dalpha3 
			//		+ 20. * dalpha1 * dalpha1 * dalpha3 + 27. * dalpha1 * dalpha1 + 2. * dalpha1 * dalpha2 * dalpha2 * dalpha2 + 4. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 19. * dalpha1 * dalpha2 * dalpha2 + 2. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 26. * dalpha1 * dalpha2 * dalpha3 + 40. * dalpha1 * dalpha2 + 7. * dalpha1 * dalpha3 * dalpha3 
			//		+ 28. * dalpha1 * dalpha3 + 23. * dalpha1 + 3. * dalpha2 * dalpha2 * dalpha2 + 6. * dalpha2 * dalpha2 * dalpha3 + 13. * dalpha2 * dalpha2 + 3. * dalpha2 * dalpha3 * dalpha3 + 18. * dalpha2 * dalpha3 + 17. * dalpha2 + 5. * dalpha3 * dalpha3 + 12. * dalpha3 + 7.)) / (dalpha1 * dalpha1 * dalpha2 * dalpha2 * (dalpha2 + dalpha3) * (dalpha2 + dalpha3));
			//m_np[2] = ((dalpha1 + 1.) * (2. * pow(dalpha1, 4) + 6. * pow(dalpha1, 3) * dalpha2 + 4. * pow(dalpha1, 3) * dalpha3 + 13. * pow(dalpha1, 3) + 6. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 8. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 30. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 * dalpha3 
			//		+ 20. * dalpha1 * dalpha1 * dalpha3 + 27. * dalpha1 * dalpha1 + 2. * dalpha1 * dalpha2 * dalpha2 * dalpha2 + 4. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 21. * dalpha1 * dalpha2 * dalpha2 + 2. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 28. * dalpha1 * dalpha2 * dalpha3 + 42. * dalpha1 * dalpha2 + 7. * dalpha1 * dalpha3 * dalpha3 
			//		+ 28. * dalpha1 * dalpha3 + 23. * dalpha1 + 4. * dalpha2 * dalpha2 * dalpha2 + 8. * dalpha2 * dalpha2 * dalpha3 + 15. * dalpha2 * dalpha2 + 4. * dalpha2 * dalpha3 * dalpha3 + 20. * dalpha2 * dalpha3 + 18. * dalpha2 + 5. * dalpha3 * dalpha3 + 12. * dalpha3 + 7.)) / (dalpha2 * dalpha2 * dalpha3 * dalpha3 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2));
			//m_np[3] = ((dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (16. * dalpha1 + 11. * dalpha2 + 6. * dalpha3 + 15. * dalpha1 * dalpha2 + 8. * dalpha1 * dalpha3 + 4. * dalpha2 * dalpha3 + 2. * dalpha1 * dalpha2 * dalpha2 + 4. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 + 11. * dalpha1 * dalpha1 + 2. * dalpha1 * dalpha1 * dalpha1 
			//		+ 4. * dalpha2 * dalpha2 + 2. * dalpha1 * dalpha2 * dalpha3 + 7.)) / (dalpha3 * dalpha3 * (dalpha2 + dalpha3) * (dalpha2 + dalpha3) * (dalpha1 + dalpha2 + dalpha3) * (dalpha1 + dalpha2 + dalpha3));


			doublereal q1 = -(4. * m_c2 + 4. * m_c3 + 12. * m_gamma - 6. * m_c2 * m_c3 - 12. * m_c2 * m_gamma - 12. * m_c3 * m_gamma + 12. * m_c2 * m_c3 * m_gamma - 3.) / (24. * m_gamma * (m_c2 - 2. * m_gamma) * (m_c3 - 2. * m_gamma));
			doublereal q2 = -(4. * m_c3 + 20. * m_gamma - 24. * m_c3 * m_gamma + 24. * m_c3 * m_gamma * m_gamma - 24. * m_gamma * m_gamma - 3.) / (12. * m_c2 * (m_c2 - m_c3) * (m_c2 - 2. * m_gamma));
			doublereal q3 = (4. * m_c2 + 20. * m_gamma - 24. * m_c2 * m_gamma + 24. * m_c2 * m_gamma * m_gamma - 24. * m_gamma * m_gamma - 3.) / (12. * m_c3 * (m_c2 - m_c3) * (m_c3 - 2. * m_gamma));
			doublereal q0 = 1. - m_gamma - q1 - q2 - q3;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.;
			m_a[IDX_A4][DIFFERENTIAL] = 1.;
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = q3 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = q2 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = q1 * dT;
			m_b[IDX_B4][DIFFERENTIAL] = q0 * dT;

			DEBUGCOUT("PredictForStageS(4)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "a4    = " << m_a[IDX_A4][DIFFERENTIAL] << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl
											<< "b4    = " << m_b[IDX_B4][DIFFERENTIAL] << std::endl);
			break;
		}

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	m_a[IDX_A1][ALGEBRAIC] = m_a[IDX_A1][DIFFERENTIAL];
	m_a[IDX_A2][ALGEBRAIC] = m_a[IDX_A2][DIFFERENTIAL];
	m_a[IDX_A3][ALGEBRAIC] = m_a[IDX_A3][DIFFERENTIAL];
	m_a[IDX_A4][ALGEBRAIC] = m_a[IDX_A4][DIFFERENTIAL];
	m_b[IDX_B0][ALGEBRAIC] = m_b[IDX_B0][DIFFERENTIAL];
	m_b[IDX_B1][ALGEBRAIC] = m_b[IDX_B1][DIFFERENTIAL];
	m_b[IDX_B2][ALGEBRAIC] = m_b[IDX_B2][DIFFERENTIAL];
	m_b[IDX_B3][ALGEBRAIC] = m_b[IDX_B3][DIFFERENTIAL];
	m_b[IDX_B4][ALGEBRAIC] = m_b[IDX_B4][DIFFERENTIAL];

	DEBUGCOUT("Algebraic coefficients:" << std::endl
		<< "Asymptotic rho =" << m_dAlgebraicRho << " (ignored)" << std::endl 
		<< "a1    = " << m_a[IDX_A1][ALGEBRAIC] << std::endl
		<< "a2    = " << m_a[IDX_A2][ALGEBRAIC] << std::endl
		<< "a3    = " << m_a[IDX_A3][ALGEBRAIC] << std::endl
		<< "a4    = " << m_a[IDX_A4][ALGEBRAIC] << std::endl
		<< "b0    = " << m_b[IDX_B0][ALGEBRAIC] << std::endl
		<< "b1    = " << m_b[IDX_B1][ALGEBRAIC] << std::endl
		<< "b2    = " << m_b[IDX_B2][ALGEBRAIC] << std::endl
		<< "b3    = " << m_b[IDX_B3][ALGEBRAIC] << std::endl
		<< "b4    = " << m_b[IDX_B4][ALGEBRAIC] << std::endl);

	db0Differential = m_b[IDX_B0][DIFFERENTIAL];
	db0Algebraic = m_b[IDX_B0][ALGEBRAIC];
}

doublereal
Mssth4Solver::dPredDerForStageS(unsigned uStage,
                                const std::array<doublereal, 4>& dXm1mN,
                                const std::array<doublereal, 5>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];

	case 2:
		return m_mp[0]*dXm1mN[IDX_Xs1]
			+ m_mp[1]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs1]
			+ m_np[1]*dXP0mN[IDX_XPm1];

	case 3:
		return m_mp[0]*dXm1mN[IDX_Xs2]
			+ m_mp[1]*dXm1mN[IDX_Xs1]
			+ m_mp[2]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs2]
			+ m_np[1]*dXP0mN[IDX_XPs1]
			+ m_np[2]*dXP0mN[IDX_XPm1];

	case 4:
		return m_mp[0]*dXm1mN[IDX_Xs3]
			+ m_mp[1]*dXm1mN[IDX_Xs2]
			+ m_mp[2]*dXm1mN[IDX_Xs1]
			+ m_mp[3]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs3]
			+ m_np[1]*dXP0mN[IDX_XPs2]
			+ m_np[2]*dXP0mN[IDX_XPs1]
			+ m_np[3]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Mssth4Solver::dPredStateForStageS(unsigned uStage,
                                  const std::array<doublereal, 4>& dXm1mN,
                                  const std::array<doublereal, 5>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 4:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A4][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B4][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Mssth4Solver::dPredDerAlgForStageS(unsigned uStage,
                                   const std::array<doublereal, 4>& dXm1mN,
                                   const std::array<doublereal, 5>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];

	case 2:
		return dXP0mN[IDX_XPs1]; 

	case 3:
		return dXP0mN[IDX_XPs2];

	case 4:
		return dXP0mN[IDX_XPs3];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Mssth4Solver::dPredStateAlgForStageS(unsigned uStage,
                                     const std::array<doublereal, 4>& dXm1mN,
                                     const std::array<doublereal, 5>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 4:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A4][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B4][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

/* Mssth4Solver - end */

/* Msstc5Solver - begin */

Msstc5Solver::Msstc5Solver(const doublereal Tl,
	const doublereal dSolTl,
	const integer iMaxIt,
	const DriveCaller* pRho,
	const DriveCaller* pAlgRho,
	const bool bmod_res_test)
: tplStageNIntegrator<5>(iMaxIt, Tl, dSolTl, bmod_res_test),
m_Rho(pRho), m_AlgebraicRho(pAlgRho)
{
	ASSERT(pRho != 0);
	ASSERT(pAlgRho != 0);
}

Msstc5Solver::~Msstc5Solver(void)
{
	NO_OP;
}

void
Msstc5Solver::SetDriveHandler(const DriveHandler* pDH)
{
	m_Rho.pGetDriveCaller()->SetDrvHdl(pDH);
	m_AlgebraicRho.pGetDriveCaller()->SetDrvHdl(pDH);
}

void
Msstc5Solver::SetCoefForStageS(unsigned uStage,
	doublereal dT,
	doublereal dAlpha,
	enum StepChange /* NewStep */)
{
	switch (uStage) {
	case 1:
		m_dRho = m_Rho.dGet();
		m_dAlgebraicRho = m_AlgebraicRho.dGet();

		if (abs(m_dRho - 0.0) < 1.e-6)
		{
			m_gamma = 0.103557108920215;
		}
		else if (abs(m_dRho - 0.1) < 1.e-6)
		{
			m_gamma = 0.103095631511675;
		}
		else if (abs(m_dRho - 0.2) < 1.e-6)
		{
			m_gamma = 0.102666675025093;
		}
		else if (abs(m_dRho - 0.3) < 1.e-6)
		{
			m_gamma = 0.102265594492185;
		}
		else if (abs(m_dRho - 0.4) < 1.e-6)
		{
			m_gamma = 0.101888703879882;
		}
		else if (abs(m_dRho - 0.5) < 1.e-6)
		{
			m_gamma = 0.101533025147874;
		}
		else if (abs(m_dRho - 0.6) < 1.e-6)
		{
			m_gamma = 0.101196115073181;
		}
		else if (abs(m_dRho - 0.7) < 1.e-6)
		{
			m_gamma = 0.100875942445807;
		}
		else if (abs(m_dRho - 0.8) < 1.e-6)
		{
			m_gamma = 0.100570798918745;
		}else if (abs(m_dRho - 0.9) < 1.e-6)
		{
			m_gamma = 0.100279232954742;
		}else if (abs(m_dRho - 1.0) < 1.e-6)
		{
			m_gamma = 1./10.;
		}else 
		{
			silent_cerr("Please select rho in 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, and 1.0." << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		ASSERT(pDM != NULL);
		pDM->SetTime(pDM->dGetTime() - (1. - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

		m_a[IDX_A1][DIFFERENTIAL] = 1.;
		m_a[IDX_A2][DIFFERENTIAL] = 0.;	// Unused
		m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
		m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
		m_a[IDX_A5][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B1][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B2][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B5][DIFFERENTIAL] = 0.; // Unused

		DEBUGCOUT("PredictForStageS(1)" << std::endl
			<< "Alpha = " << dAlpha << std::endl
			<< "Differential coefficients:" << std::endl
			<< "Asymptotic rho =" << m_dRho << std::endl 
			<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
			<< "a2    = not needed" << std::endl
			<< "a3    = not needed" << std::endl
			<< "a4    = not needed" << std::endl
			<< "a5    = not needed" << std::endl
			<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
			<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
			<< "b2    = not needed" << std::endl
			<< "b3    = not needed" << std::endl
			<< "b4    = not needed" << std::endl
			<< "b5    = not needed" << std::endl);
		break;

	case 2:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + 2. * m_gamma * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;//Unused
			//m_mp[3] = 0.;//Unused
			//m_mp[4] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;//Unused
			//m_np[3] = 0.;//Unused
			//m_np[4] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = 1.;
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / (2. * m_gamma * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.; //Unused
			m_mp[3] = 0.; //Unused
			m_mp[4] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.; //Unused
			m_np[3] = 0.; //Unused
			m_np[4] = 0.; //Unused

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 1.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
			m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
			m_a[IDX_A5][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = 2. * m_gamma * dT;
			m_b[IDX_B2][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B5][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(2)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = not needed" << std::endl
											<< "a4    = not needed" << std::endl
											<< "a5    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = not needed" << std::endl
											<< "b4    = not needed" << std::endl
											<< "b5    = not needed" << std::endl);
			break;
		}

	case 3:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + 2. * m_gamma * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;//Unused
			//m_mp[4] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;
			//m_np[3] = 0.;//Unused
			//m_np[4] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = 1.;
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / (2. * m_gamma * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.;
			m_mp[3] = 0.; //Unused
			m_mp[4] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.;
			m_np[3] = 0.; //Unused
			m_np[4] = 0.; //Unused

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 1.;
			m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
			m_a[IDX_A5][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = 2. * m_gamma * dT;
			m_b[IDX_B2][DIFFERENTIAL] = 2. * m_gamma * dT;
			m_b[IDX_B3][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B5][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(3)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "a4    = not needed" << std::endl
											<< "a5    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl
											<< "b4    = not needed" << std::endl
											<< "b5    = not needed" << std::endl);
			break;
		}

	case 4:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + 2. * m_gamma * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;
			//m_np[4] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;
			//m_np[3] = 0.;
			//m_np[4] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = 1.;
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / (2. * m_gamma * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.;
			m_mp[3] = 0.;
			m_mp[4] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.;
			m_np[3] = 0.;
			m_np[4] = 0.; //Unused

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.;
			m_a[IDX_A4][DIFFERENTIAL] = 1.;
			m_a[IDX_A5][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = 2. * m_gamma * dT;
			m_b[IDX_B2][DIFFERENTIAL] = 2. * m_gamma * dT;
			m_b[IDX_B3][DIFFERENTIAL] = 2. * m_gamma * dT;
			m_b[IDX_B4][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B5][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(4)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "a4    = " << m_a[IDX_A4][DIFFERENTIAL] << std::endl
											<< "a5    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl
											<< "b4    = " << m_b[IDX_B4][DIFFERENTIAL] << std::endl
											<< "b5    = not needed" << std::endl);
			break;
		}

	case 5:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (1. - 8. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;
			//m_np[4] = 0.;
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;
			//m_np[3] = 0.;
			//m_np[4] = 0.;

			//second-order prediction
			doublereal dalpha = (1. - 8. * m_gamma) / (2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((1. - 8. * m_gamma) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.;
			m_mp[3] = 0.;
			m_mp[4] = 0.;
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.;
			m_np[3] = 0.;
			m_np[4] = 0.;

			//fourth-order prediction
			//doublereal dalpha = (1. - 8. * m_gamma) / (2. * m_gamma);
			//m_mp[0] = - dalpha*dalpha*(15.*dalpha*dalpha*dalpha + 68.*dalpha*dalpha + 99.*dalpha + 46.)/(4.*(1. - 8.*m_gamma)*dT);
			//m_mp[1] = 4.*dalpha*dalpha*(dalpha*dalpha + 3.*dalpha + 2.)/((1. - 8.*m_gamma)*dT);
			//m_mp[2] = - m_mp[0] - m_mp[1];
			//m_mp[3] = 0.;
			//m_mp[4] = 0.;
			//m_np[0] = 5.*dalpha*dalpha*dalpha*dalpha/4. + 6.*dalpha*dalpha*dalpha + 39.*dalpha*dalpha/4. + 6.*dalpha + 1.;
			//m_np[1] = dalpha*(5.*dalpha*dalpha*dalpha + 20.*dalpha*dalpha + 24.*dalpha + 8.);
			//m_np[2] = dalpha*(5.*dalpha*dalpha*dalpha + 16.*dalpha*dalpha + 15.*dalpha + 4.)/4.;
			//m_np[3] = 0.;
			//m_np[4] = 0.;

			//sixth-order prediction
			//doublereal dalpha = (1. - 8. * m_gamma) / (2. * m_gamma);
			//m_mp[0] = - dalpha*dalpha*(77.*dalpha*dalpha*dalpha*dalpha*dalpha + 774.*dalpha*dalpha*dalpha*dalpha + 3010.*dalpha*dalpha*dalpha + 5640.*dalpha*dalpha + 5073.*dalpha + 1746.)/(108.*(1. - 8.*m_gamma)*dT);
			//m_mp[1] = - dalpha*dalpha*dalpha*(7.*dalpha*dalpha*dalpha*dalpha + 60.*dalpha*dalpha*dalpha + 185.*dalpha*dalpha + 240.*dalpha + 108.)/(4.*(1. - 8.*m_gamma)*dT);
			//m_mp[2] = dalpha*dalpha*(dalpha + 3.)*(dalpha + 3.)*(7.*dalpha*dalpha*dalpha + 24.*dalpha*dalpha + 23.*dalpha + 6.)/(4.*(1. - 8.*m_gamma)*dT);
			//m_mp[3] = - m_mp[0] - m_mp[1] - m_mp[2];
			//m_mp[4] = 0.;
			//m_np[0] = 7.*dalpha*dalpha*dalpha*dalpha*dalpha*dalpha/36. + 2.*dalpha*dalpha*dalpha*dalpha*dalpha + 145.*dalpha*dalpha*dalpha*dalpha/18. + 16.*dalpha*dalpha*dalpha + 193.*dalpha*dalpha/12. + 22.*dalpha/3. + 1.;
			//m_np[1] = dalpha*(7.*dalpha*dalpha*dalpha*dalpha*dalpha + 66.*dalpha*dalpha*dalpha*dalpha + 235.*dalpha*dalpha*dalpha + 388.*dalpha*dalpha + 288.*dalpha + 72.)/4.;
			//m_np[2] = dalpha*(7.*dalpha*dalpha*dalpha*dalpha*dalpha + 60.*dalpha*dalpha*dalpha*dalpha + 190.*dalpha*dalpha*dalpha + 272.*dalpha*dalpha + 171.*dalpha + 36.)/4.;
			//m_np[3] = dalpha*(7.*dalpha*dalpha*dalpha*dalpha*dalpha + 54.*dalpha*dalpha*dalpha*dalpha + 155.*dalpha*dalpha*dalpha + 204.*dalpha*dalpha + 120.*dalpha + 24.)/36.;
			//m_np[4] = 0.;

			doublereal da1 = 1. - 5. * m_gamma;
			doublereal da2 = 10. * m_gamma * m_gamma - 5. * m_gamma + 1. / 2.;
			doublereal da3 = -(400. * m_gamma * m_gamma * m_gamma - 280. * m_gamma * m_gamma + 60. * m_gamma - 4. +
							   sqrt(2. * (64. * (5. * m_dRho + 805.) * m_gamma * m_gamma * m_gamma * m_gamma * m_gamma * m_gamma +
										  32. * (-2. * m_dRho - 2050.) * m_gamma * m_gamma * m_gamma * m_gamma * m_gamma + 16. * 2160. * m_gamma * m_gamma * m_gamma * m_gamma -
										  8. * 1200. * m_gamma * m_gamma * m_gamma + 4. * 370. * m_gamma * m_gamma - 120. * m_gamma + 4.))) / 8.;
			doublereal da4 = -(720. * m_gamma * m_gamma * m_gamma * m_gamma - 800. * m_gamma * m_gamma * m_gamma + 280. * m_gamma * m_gamma + (80. * da3 - 40.) * m_gamma - 16. * da3 + 2.) / 16.;
			doublereal da5 = m_dRho * m_gamma * m_gamma * m_gamma * m_gamma * m_gamma;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.;
			m_a[IDX_A4][DIFFERENTIAL] = 0.;
			m_a[IDX_A5][DIFFERENTIAL] = 1.;
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = (m_gamma / 16. + da1 / 16. + da2 / (16. * m_gamma) + da3 / (16. * m_gamma * m_gamma) + da4 / (16. * m_gamma * m_gamma * m_gamma) + da5 / (16. * m_gamma * m_gamma * m_gamma * m_gamma)) * dT;
			m_b[IDX_B2][DIFFERENTIAL] = (3. * m_gamma / 8. + da1 / 4. + da2 / (8. * m_gamma) - da4 / (8. * m_gamma * m_gamma * m_gamma) - da5 / (4. * m_gamma * m_gamma * m_gamma * m_gamma)) * dT;
			m_b[IDX_B3][DIFFERENTIAL] = (m_gamma + 3. * da1 / 8. - da3 / (8. * m_gamma * m_gamma) + 6. * da5 / (16. * m_gamma * m_gamma * m_gamma * m_gamma)) * dT;
			m_b[IDX_B4][DIFFERENTIAL] = (13. * m_gamma / 8. + da1 / 4. - da2 / (8. * m_gamma) + da4 / (8. * m_gamma * m_gamma * m_gamma) - da5 / (4. * m_gamma * m_gamma * m_gamma * m_gamma)) * dT;
			m_b[IDX_B5][DIFFERENTIAL] = (15. * m_gamma / 16. + da1 / 16. - da2 / (16. * m_gamma) + da3 / (16. * m_gamma * m_gamma) - da4 / (16. * m_gamma * m_gamma * m_gamma) + da5 / (16. * m_gamma * m_gamma * m_gamma * m_gamma)) * dT;

			DEBUGCOUT("PredictForStageS(5)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "a4    = " << m_a[IDX_A4][DIFFERENTIAL] << std::endl
											<< "a5    = " << m_a[IDX_A5][DIFFERENTIAL] << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl
											<< "b4    = " << m_b[IDX_B4][DIFFERENTIAL] << std::endl
											<< "b5    = " << m_b[IDX_B5][DIFFERENTIAL] << std::endl);
			break;
		}

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	m_a[IDX_A1][ALGEBRAIC] = m_a[IDX_A1][DIFFERENTIAL];
	m_a[IDX_A2][ALGEBRAIC] = m_a[IDX_A2][DIFFERENTIAL];
	m_a[IDX_A3][ALGEBRAIC] = m_a[IDX_A3][DIFFERENTIAL];
	m_a[IDX_A4][ALGEBRAIC] = m_a[IDX_A4][DIFFERENTIAL];
	m_a[IDX_A5][ALGEBRAIC] = m_a[IDX_A5][DIFFERENTIAL];
	m_b[IDX_B0][ALGEBRAIC] = m_b[IDX_B0][DIFFERENTIAL];
	m_b[IDX_B1][ALGEBRAIC] = m_b[IDX_B1][DIFFERENTIAL];
	m_b[IDX_B2][ALGEBRAIC] = m_b[IDX_B2][DIFFERENTIAL];
	m_b[IDX_B3][ALGEBRAIC] = m_b[IDX_B3][DIFFERENTIAL];
	m_b[IDX_B4][ALGEBRAIC] = m_b[IDX_B4][DIFFERENTIAL];
	m_b[IDX_B5][ALGEBRAIC] = m_b[IDX_B5][DIFFERENTIAL];

	DEBUGCOUT("Algebraic coefficients:" << std::endl
		<< "Asymptotic rho =" << m_dAlgebraicRho << " (ignored)" << std::endl 
		<< "a1    = " << m_a[IDX_A1][ALGEBRAIC] << std::endl
		<< "a2    = " << m_a[IDX_A2][ALGEBRAIC] << std::endl
		<< "a3    = " << m_a[IDX_A3][ALGEBRAIC] << std::endl
		<< "a4    = " << m_a[IDX_A4][ALGEBRAIC] << std::endl
		<< "a5    = " << m_a[IDX_A5][ALGEBRAIC] << std::endl
		<< "b0    = " << m_b[IDX_B0][ALGEBRAIC] << std::endl
		<< "b1    = " << m_b[IDX_B1][ALGEBRAIC] << std::endl
		<< "b2    = " << m_b[IDX_B2][ALGEBRAIC] << std::endl
		<< "b3    = " << m_b[IDX_B3][ALGEBRAIC] << std::endl
		<< "b4    = " << m_b[IDX_B4][ALGEBRAIC] << std::endl
		<< "b5    = " << m_b[IDX_B5][ALGEBRAIC] << std::endl);

	db0Differential = m_b[IDX_B0][DIFFERENTIAL];
	db0Algebraic = m_b[IDX_B0][ALGEBRAIC];
}

doublereal
Msstc5Solver::dPredDerForStageS(unsigned uStage,
                                const std::array<doublereal, 5>& dXm1mN,
                                const std::array<doublereal, 6>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];

	case 2:
		return m_mp[0]*dXm1mN[IDX_Xs1]
			+ m_mp[1]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs1]
			+ m_np[1]*dXP0mN[IDX_XPm1]; 

	case 3:
		return m_mp[0]*dXm1mN[IDX_Xs2]
			+ m_mp[1]*dXm1mN[IDX_Xs1]
			+ m_mp[2]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs2]
			+ m_np[1]*dXP0mN[IDX_XPs1]
			+ m_np[2]*dXP0mN[IDX_XPm1];

	case 4:
		return m_mp[0]*dXm1mN[IDX_Xs3]
			+ m_mp[1]*dXm1mN[IDX_Xs2]
			+ m_mp[2]*dXm1mN[IDX_Xs1]
			+ m_mp[3]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs3]
			+ m_np[1]*dXP0mN[IDX_XPs2]
			+ m_np[2]*dXP0mN[IDX_XPs1]
			+ m_np[3]*dXP0mN[IDX_XPm1];

	case 5:
		return m_mp[0]*dXm1mN[IDX_Xs4]
			+ m_mp[1]*dXm1mN[IDX_Xs3]
			+ m_mp[2]*dXm1mN[IDX_Xs2]
			+ m_mp[3]*dXm1mN[IDX_Xs1]
			+ m_mp[4]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs4]
			+ m_np[1]*dXP0mN[IDX_XPs3]
			+ m_np[2]*dXP0mN[IDX_XPs2]
			+ m_np[3]*dXP0mN[IDX_XPs1]
			+ m_np[4]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Msstc5Solver::dPredStateForStageS(unsigned uStage,
                                  const std::array<doublereal, 5>& dXm1mN,
                                  const std::array<doublereal, 6>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 4:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A4][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs4]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B4][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 5:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs4]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A4][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A5][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs4]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B4][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B5][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Msstc5Solver::dPredDerAlgForStageS(unsigned uStage,
                                   const std::array<doublereal, 5>& dXm1mN,
                                   const std::array<doublereal, 6>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];

	case 2:
		return dXP0mN[IDX_XPs1]; 

	case 3:
		return dXP0mN[IDX_XPs2];

	case 4:
		return dXP0mN[IDX_XPs3];

	case 5:
		return dXP0mN[IDX_XPs4];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Msstc5Solver::dPredStateAlgForStageS(unsigned uStage,
                                     const std::array<doublereal, 5>& dXm1mN,
                                     const std::array<doublereal, 6>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 4:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A4][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs4]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B4][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 5:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs4]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A4][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A5][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs4]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B4][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B5][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

/* Msstc5Solver - end */

/* Mssth5Solver - begin */

Mssth5Solver::Mssth5Solver(const doublereal Tl,
	const doublereal dSolTl,
	const integer iMaxIt,
	const DriveCaller* pRho,
	const DriveCaller* pAlgRho,
	const bool bmod_res_test)
: tplStageNIntegrator<5>(iMaxIt, Tl, dSolTl, bmod_res_test),
m_Rho(pRho), m_AlgebraicRho(pAlgRho)
{
	ASSERT(pRho != 0);
	ASSERT(pAlgRho != 0);
}

Mssth5Solver::~Mssth5Solver(void)
{
	NO_OP;
}

void
Mssth5Solver::SetDriveHandler(const DriveHandler* pDH)
{
	m_Rho.pGetDriveCaller()->SetDrvHdl(pDH);
	m_AlgebraicRho.pGetDriveCaller()->SetDrvHdl(pDH);
}

void
Mssth5Solver::SetCoefForStageS(unsigned uStage,
	doublereal dT,
	doublereal dAlpha,
	enum StepChange /* NewStep */)
{
	switch (uStage) {
	case 1:
		m_dRho = m_Rho.dGet();
		m_dAlgebraicRho = m_AlgebraicRho.dGet();

		if (abs(m_dRho - 0.0) < 1.e-6)
		{
			m_gamma = 0.2780538411364499307154574;
			m_c2 = 0.1;
			m_c3 = 0.9673258605571696255864822;
			m_c4 = 1.140134263244145573779065;
		}
		else if (abs(m_dRho - 0.1) < 1.e-6)
		{
			m_gamma = 0.2741413060318684813410073;
			m_c2 = 0.1;
			m_c3 = 0.9085500184865173967097007;
			m_c4 = 1.147028320924987188078603;
		}
		else if (abs(m_dRho - 0.2) < 1.e-6)
		{
			m_gamma = 0.270459886774581770296777;
			m_c2 = 0.1;
			m_c3 = 0.8510912674088796370241994;
			m_c4 = 1.153119767864889455566413;
		}
		else if (abs(m_dRho - 0.3) < 1.e-6)
		{
			m_gamma = 0.2669780439256505544243225;
			m_c2 = 0.1;
			m_c3 = 0.7951780419709296721109126;
			m_c4 = 1.158468410781347657234619;
		}
		else if (abs(m_dRho - 0.4) < 1.e-6)
		{
			m_gamma = 0.2636702317116055294121679;
			m_c2 = 0.1;
			m_c3 = 0.7409689864721386021173544;
			m_c4 = 1.163120553594974015965136;
		}
		else if (abs(m_dRho - 0.5) < 1.e-6)
		{
			m_gamma = 0.2605154166070549059952555;
			m_c2 = 0.1;
			m_c3 = 0.6885609787040850582329199;
			m_c4 = 1.167112017832381054560642;
		}
		else if (abs(m_dRho - 0.6) < 1.e-6)
		{
			m_gamma = 0.2574960298566747463056004;
			m_c2 = 0.1;
			m_c3 = 0.6379972790560722861741283;
			m_c4 = 1.170470299562808680349235;
		}
		else if (abs(m_dRho - 0.7) < 1.e-6)
		{
			m_gamma = 0.2545972081701334821524085;
			m_c2 = 0.1;
			m_c3 = 0.5892756783804685705163706;
			m_c4 = 1.173216142360941294242593;
		}
		else if (abs(m_dRho - 0.8) < 1.e-6)
		{
			m_gamma = 0.2518062311838496492022443;
			m_c2 = 0.1;
			m_c3 = 0.5423562209596147765111596;
			m_c4 = 1.175364704054515430087235;
		}
		else if (abs(m_dRho - 0.9) < 1.e-6)
		{
			m_gamma = 0.2491120965296297062874231;
			m_c2 = 0.1;
			m_c3 = 0.4971683414199104533715001;
			m_c4 = 1.176926433659807091913763;
		}
		else if (abs(m_dRho - 1.0) < 1.e-6)
		{
			m_gamma = 0.2465051931428201559270974;
			m_c2 = 0.1;
			m_c3 = 0.4536172576687555468843982;
			m_c4 = 1.177907736613444944495654;
		}
		else
		{
			silent_cerr("Please select rho in 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, and 1.0." << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		//m_c2 = 0.1;

		//m_c3 = -(756864000000. * pow(m_c2, 4) * pow(m_gamma, 14) - 3532032000000. * pow(m_c2, 4) * pow(m_gamma, 13) + 7364666880000. * pow(m_c2, 4) * pow(m_gamma, 12) - 9161399808000. * pow(m_c2, 4) * pow(m_gamma, 11) 
		//+ 7633038931200. * pow(m_c2, 4) * pow(m_gamma, 10) - 4516690176000. * pow(m_c2, 4) * pow(m_gamma, 9) + 1959141268800. * pow(m_c2, 4) * pow(m_gamma, 8) - 632872903680. * pow(m_c2, 4) * pow(m_gamma, 7) + 152986243680. * pow(m_c2, 4) * pow(m_gamma, 6) 
		//- 27542998320. * pow(m_c2, 4) * pow(m_gamma, 5) + 3637227020. * pow(m_c2, 4) * pow(m_gamma, 4) - 341933080. * pow(m_c2, 4) * pow(m_gamma, 3) + 21656460. * pow(m_c2, 4) * pow(m_gamma, 2) - 828180. * pow(m_c2, 4) * pow(m_gamma, 1) + 14445. * pow(m_c2, 4) 
		//- 3027456000000. * pow(m_c2, 3) * pow(m_gamma, 15) + 13371264000000. * pow(m_c2, 3) * pow(m_gamma, 14) - 26153694720000. * pow(m_c2, 3) * pow(m_gamma, 13) + 30003459840000. * pow(m_c2, 3) * pow(m_gamma, 12) - 22353310771200. * pow(m_c2, 3) * pow(m_gamma, 11) 
		//+ 11177695795200. * pow(m_c2, 3) * pow(m_gamma, 10) - 3651242409600. * pow(m_c2, 3) * pow(m_gamma, 9) + 647486265600. * pow(m_c2, 3) * pow(m_gamma, 8) + 24167773760. * pow(m_c2, 3) * pow(m_gamma, 7) - 51289274880. * pow(m_c2, 3) * pow(m_gamma, 6) + 16065159800. * pow(m_c2, 3) * pow(m_gamma, 5) 
		//- 2898529092. * pow(m_c2, 3) * pow(m_gamma, 4) + 337234624. * pow(m_c2, 3) * pow(m_gamma, 3) - 25093126. * pow(m_c2, 3) * pow(m_gamma, 2) + 1093074. * pow(m_c2, 3) * pow(m_gamma, 1) - 21297. * pow(m_c2, 3) + 6013440000000. * pow(m_c2, 2) * pow(m_gamma, 16) - 26507520000000. * pow(m_c2, 2) * pow(m_gamma, 15) 
		//+ 51547173120000. * pow(m_c2, 2) * pow(m_gamma, 14) - 58414192128000. * pow(m_c2, 2) * pow(m_gamma, 13) + 42704730892800. * pow(m_c2, 2) * pow(m_gamma, 12) - 20871334809600. * pow(m_c2, 2) * pow(m_gamma, 11) + 6733052064000. * pow(m_c2, 2) * pow(m_gamma, 10) - 1310036090240. * pow(m_c2, 2) * pow(m_gamma, 9) 
		//+ 102583470240. * pow(m_c2, 2) * pow(m_gamma, 8) + 8218100800. * pow(m_c2, 2) * pow(m_gamma, 7) + 1703186960. * pow(m_c2, 2) * pow(m_gamma, 6) - 2315435080. * pow(m_c2, 2) * pow(m_gamma, 5) + 673418760. * pow(m_c2, 2) * pow(m_gamma, 4) - 103919952. * pow(m_c2, 2) * pow(m_gamma, 3) + 9431070. * pow(m_c2, 2) * pow(m_gamma, 2) 
		//- 478830. * pow(m_c2, 2) * pow(m_gamma, 1) + 10578. * pow(m_c2, 2) - 11943936000000. * m_c2 * pow(m_gamma, 17) + 64613376000000. * m_c2 * pow(m_gamma, 16) - 160130304000000. * m_c2 * pow(m_gamma, 15) + 242566018560000. * m_c2 * pow(m_gamma, 14) - 252669647232000. * m_c2 * pow(m_gamma, 13) + 192552385536000. * m_c2 * pow(m_gamma, 12) 
		//- 111242677171200. * m_c2 * pow(m_gamma, 11) + 49726326817280. * m_c2 * pow(m_gamma, 10) - 17377123744960. * m_c2 * pow(m_gamma, 9) + 4759742354720. * m_c2 * pow(m_gamma, 8) - 1017645605760. * m_c2 * pow(m_gamma, 7) + 167935051024. * m_c2 * pow(m_gamma, 6) - 20963067456. * m_c2 * pow(m_gamma, 5) + 1913699448. * m_c2 * pow(m_gamma, 4) 
		//- 120540720. * m_c2 * pow(m_gamma, 3) + 4683648. * m_c2 * pow(m_gamma, 2) - 84624. * m_c2 * m_gamma + 11943936000000. * pow(m_gamma, 18) - 73654272000000. * pow(m_gamma, 17) + 206970163200000. * pow(m_gamma, 16) - 352838246400000. * pow(m_gamma, 15) + 409419429120000. * pow(m_gamma, 14) - 343166267520000. * pow(m_gamma, 13) + 214867994227200. * pow(m_gamma, 12) 
		//- 102437092531200. * pow(m_gamma, 11) + 37548986814720. * pow(m_gamma, 10) - 10612955123520. * pow(m_gamma, 9) + 2305582910400. * pow(m_gamma, 8) - 381243620288. * pow(m_gamma, 7) + 47113570880. * pow(m_gamma, 6) - 4215689696. * pow(m_gamma, 5) + 258288096. * pow(m_gamma, 4) - 9710304. * pow(m_gamma, 3) + 169248. * pow(m_gamma, 2)) / (3784320000000. * pow(m_c2, 4) * pow(m_gamma, 13) 
		//- 756864000000. * pow(m_c2, 4) * pow(m_gamma, 14) - 8373818880000. * pow(m_c2, 4) * pow(m_gamma, 12) + 10956135168000. * pow(m_c2, 4) * pow(m_gamma, 11) - 9536572339200. * pow(m_c2, 4) * pow(m_gamma, 10) + 5866312320000. * pow(m_c2, 4) * pow(m_gamma, 9) - 2635475443200. * pow(m_c2, 4) * pow(m_gamma, 8) + 879282278400. * pow(m_c2, 4) * pow(m_gamma, 7) - 219029301600. * pow(m_c2, 4) * pow(m_gamma, 6) 
		//+ 40559488800. * pow(m_c2, 4) * pow(m_gamma, 5) - 5500666600. * pow(m_c2, 4) * pow(m_gamma, 4) + 530408600. * pow(m_c2, 4) * pow(m_gamma, 3) - 34424900. * pow(m_c2, 4) * pow(m_gamma, 2) + 1348200. * pow(m_c2, 4) * m_gamma - 24075. * pow(m_c2, 4) + 3027456000000. * pow(m_c2, 3) * pow(m_gamma, 15) - 14380416000000. * pow(m_c2, 3) * pow(m_gamma, 14) + 30064158720000. * pow(m_c2, 3) * pow(m_gamma, 13) 
		//- 36711463680000. * pow(m_c2, 3) * pow(m_gamma, 12) + 29124681523200. * pow(m_c2, 3) * pow(m_gamma, 11) - 15637659148800. * pow(m_c2, 3) * pow(m_gamma, 10) + 5643555187200. * pow(m_c2, 3) * pow(m_gamma, 9) - 1247812665600. * pow(m_c2, 3) * pow(m_gamma, 8) + 88932973440. * pow(m_c2, 3) * pow(m_gamma, 7) + 42502832960. * pow(m_c2, 3) * pow(m_gamma, 6) - 17645963040. * pow(m_c2, 3) * pow(m_gamma, 5) 
		//+ 3500787240. * pow(m_c2, 3) * pow(m_gamma, 4) - 428497360. * pow(m_c2, 3) * pow(m_gamma, 3) + 32912520. * pow(m_c2, 3) * pow(m_gamma, 2) - 1463760. * pow(m_c2, 3) * pow(m_gamma, 1) + 28890. * pow(m_c2, 3) - 6013440000000. * pow(m_c2, 2) * pow(m_gamma, 16) + 28512000000000. * pow(m_c2, 2) * pow(m_gamma, 15) - 59302437120000. * pow(m_c2, 2) * pow(m_gamma, 14) + 71568499968000. * pow(m_c2, 2) * pow(m_gamma, 13) 
		//- 55696150540800. * pow(m_c2, 2) * pow(m_gamma, 12) + 29136715891200. * pow(m_c2, 2) * pow(m_gamma, 11) - 10242311404800. * pow(m_c2, 2) * pow(m_gamma, 10) + 2296484160000. * pow(m_c2, 2) * pow(m_gamma, 9) - 275395726240. * pow(m_c2, 2) * pow(m_gamma, 8) + 7822653440. * pow(m_c2, 2) * pow(m_gamma, 7) - 2519762080. * pow(m_c2, 2) * pow(m_gamma, 6) + 2670614400. * pow(m_c2, 2) * pow(m_gamma, 5) - 810441912. * pow(m_c2, 2) * pow(m_gamma, 4) 
		//+ 128604024. * pow(m_c2, 2) * pow(m_gamma, 3) - 11826756. * pow(m_c2, 2) * pow(m_gamma, 2) + 601344. * pow(m_c2, 2) * pow(m_gamma, 1) - 13167. * pow(m_c2, 2) + 11943936000000. * m_c2 * pow(m_gamma, 17) - 68594688000000. * m_c2 * pow(m_gamma, 16) + 178523136000000. * m_c2 * pow(m_gamma, 15) - 280986831360000. * m_c2 * pow(m_gamma, 14) + 301511107584000. * m_c2 * pow(m_gamma, 13) - 235126771430400. * m_c2 * pow(m_gamma, 12) + 138318861235200. * m_c2 * pow(m_gamma, 11) 
		//- 62739962572800. * m_c2 * pow(m_gamma, 10) + 22195537122560. * m_c2 * pow(m_gamma, 9) - 6144085554240. * m_c2 * pow(m_gamma, 8) + 1325269837440. * m_c2 * pow(m_gamma, 7) - 220109016320. * m_c2 * pow(m_gamma, 6) + 27554120672. * m_c2 * pow(m_gamma, 5) - 2509896456. * m_c2 * pow(m_gamma, 4) + 156712032. * m_c2 * pow(m_gamma, 3) - 5986800. * m_c2 * pow(m_gamma, 2) + 105336. * m_c2 * pow(m_gamma, 1) - 11943936000000. * pow(m_gamma, 18) + 77635584000000. * pow(m_gamma, 17) 
		//- 227872051200000. * pow(m_gamma, 16) + 402512025600000. * pow(m_gamma, 15) - 481030974720000. * pow(m_gamma, 14) + 413437840128000. * pow(m_gamma, 13) - 264592243468800. * pow(m_gamma, 12) + 128607098534400. * pow(m_gamma, 11) - 47955679609600. * pow(m_gamma, 10) + 13756482986240. * pow(m_gamma, 9) - 3024659796640. * pow(m_gamma, 8) + 504387271680. * pow(m_gamma, 7) - 62563091648. * pow(m_gamma, 6) + 5584312992. * pow(m_gamma, 5) - 338618880. * pow(m_gamma, 4) + 12476736. * pow(m_gamma, 3) - 210672. * pow(m_gamma, 2));

		//m_c4 = (m_c2 - 4. * m_gamma - 18. * m_c2 * m_gamma + 120. * m_c2 * m_gamma * m_gamma - 336. * m_c2 * m_gamma * m_gamma * m_gamma 
		//	+ 360. * m_c2 * m_gamma * m_gamma * m_gamma * m_gamma + 64. * m_gamma * m_gamma - 368. * m_gamma * m_gamma * m_gamma + 848. * m_gamma * m_gamma * m_gamma * m_gamma 
		//	- 720. * m_gamma * m_gamma * m_gamma * m_gamma * m_gamma) / (m_c2 - 4. * m_gamma - 16. * m_c2 * m_gamma + 96. * m_c2 * m_gamma * m_gamma - 240. * m_c2 * m_gamma * m_gamma * m_gamma 
		//	+ 240. * m_c2 * m_gamma * m_gamma * m_gamma * m_gamma + 56. * m_gamma * m_gamma - 288. * m_gamma * m_gamma * m_gamma + 600. * m_gamma * m_gamma * m_gamma * m_gamma - 480. * m_gamma * m_gamma * m_gamma * m_gamma * m_gamma);

		ASSERT(pDM != NULL);
		pDM->SetTime(pDM->dGetTime() - (1. - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

		m_a[IDX_A1][DIFFERENTIAL] = 1.;
		m_a[IDX_A2][DIFFERENTIAL] = 0.;	// Unused
		m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
		m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
		m_a[IDX_A5][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B1][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B2][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B5][DIFFERENTIAL] = 0.; // Unused

		DEBUGCOUT("PredictForStageS(1)" << std::endl
			<< "Alpha = " << dAlpha << std::endl
			<< "Differential coefficients:" << std::endl
			<< "Asymptotic rho =" << m_dRho << std::endl 
			<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
			<< "a2    = not needed" << std::endl
			<< "a3    = not needed" << std::endl
			<< "a4    = not needed" << std::endl
			<< "a5    = not needed" << std::endl
			<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
			<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
			<< "b2    = not needed" << std::endl
			<< "b3    = not needed" << std::endl
			<< "b4    = not needed" << std::endl
			<< "b5    = not needed" << std::endl);
		break;

	case 2:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (m_c2 - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;//Unused
			//m_mp[3] = 0.;//Unused
			//m_mp[4] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;//Unused
			//m_np[3] = 0.;//Unused
			//m_np[4] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = (m_c2 - 2. * m_gamma) / (2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((m_c2 - 2. * m_gamma) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.; //Unused
			m_mp[3] = 0.; //Unused
			m_mp[4] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.; //Unused
			m_np[3] = 0.; //Unused
			m_np[4] = 0.; //Unused

			doublereal a21 = m_c2 * (m_c2 - 2. * m_gamma) / (4. * m_gamma);
			doublereal a20 = m_c2 - m_gamma - a21;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 1.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
			m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
			m_a[IDX_A5][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = a21 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = a20 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B5][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(2)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = not needed" << std::endl
											<< "a4    = not needed" << std::endl
											<< "a5    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = not needed" << std::endl
											<< "b4    = not needed" << std::endl
											<< "b5    = not needed" << std::endl);
			break;
		}

	case 3:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (m_c3 - m_c2) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;//Unused
			//m_mp[4] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;
			//m_np[3] = 0.;//Unused
			//m_np[4] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = (m_c3 - m_c2) / (m_c2 - 2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((m_c3 - m_c2) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.;
			m_mp[3] = 0.; //Unused
			m_mp[4] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.;
			m_np[3] = 0.; //Unused
			m_np[4] = 0.; //Unused

			doublereal a31 = -(m_c3 * (m_c3 - 2. * m_gamma) * (20. * m_c2 - 8. * m_c3 - 24. * m_gamma - 25. * m_c2 * m_c4 + 10. * m_c3 * m_c4 - 160. * m_c2 * m_gamma 
						+ 100. * m_c3 * m_gamma + 30. * m_c4 * m_gamma + 20. * m_c2 * m_c2 * m_c4 + 240. * m_c2 * m_gamma * m_gamma + 100. * m_c2 * m_c2 * m_gamma 
						- 320. * m_c3 * m_gamma * m_gamma + 240. * m_c3 * m_gamma * m_gamma * m_gamma - 200. * m_c4 * m_gamma * m_gamma + 240. * m_c4 * m_gamma * m_gamma * m_gamma 
						- 15. * m_c2 * m_c2 + 180. * m_gamma * m_gamma - 240. * m_gamma * m_gamma * m_gamma - 120. * m_c2 * m_c2 * m_gamma * m_gamma + 120. * m_c2 * m_c2 * m_c4 * m_gamma * m_gamma 
						+ 180. * m_c2 * m_c4 * m_gamma - 120. * m_c3 * m_c4 * m_gamma - 240. * m_c2 * m_c4 * m_gamma * m_gamma - 120. * m_c2 * m_c2 * m_c4 * m_gamma 
						+ 360. * m_c3 * m_c4 * m_gamma * m_gamma - 240. * m_c3 * m_c4 * m_gamma * m_gamma * m_gamma)) / (4. * m_gamma * (m_c2 - 2. * m_gamma) * (15. * m_c2 + 15. * m_c4 + 90. * m_gamma - 20. * m_c2 * m_c4 
						- 100. * m_c2 * m_gamma - 100. * m_c4 * m_gamma + 120. * m_c2 * m_gamma * m_gamma + 120. * m_c4 * m_gamma * m_gamma - 120. * m_gamma * m_gamma + 120. * m_c2 * m_c4 * m_gamma - 120. * m_c2 * m_c4 * m_gamma * m_gamma - 12.));
			doublereal a32 = -(m_c3 * (m_c2 - m_c3) * (m_c3 - 2. * m_gamma) * (5. * m_c4 + 50. * m_gamma - 60. * m_c4 * m_gamma + 180. * m_c4 * m_gamma * m_gamma - 120. * m_c4 * m_gamma * m_gamma * m_gamma - 160. * m_gamma * m_gamma + 120. * m_gamma * m_gamma * m_gamma - 4.)) 
						/ (m_c2 * (m_c2 - 2. * m_gamma) * (15. * m_c2 + 15. * m_c4 + 90. * m_gamma - 20. * m_c2 * m_c4 - 100. * m_c2 * m_gamma - 100. * m_c4 * m_gamma + 120. * m_c2 * m_gamma * m_gamma + 120. * m_c4 * m_gamma * m_gamma - 120. * m_gamma * m_gamma + 120. * m_c2 * m_c4 * m_gamma - 120. * m_c2 * m_c4 * m_gamma * m_gamma - 12.));
			doublereal a30 = m_c3 - m_gamma - a31 - a32;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 1.;
			m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
			m_a[IDX_A5][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = a32 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = a31 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = a30 * dT;
			m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B5][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(3)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "a4    = not needed" << std::endl
											<< "a5    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl
											<< "b4    = not needed" << std::endl
											<< "b5    = not needed" << std::endl);
			break;
		}

	case 4:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (m_c4 - m_c3) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;
			//m_np[4] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;
			//m_np[3] = 0.;
			//m_np[4] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = (m_c4 - m_c3) / (m_c3 - m_c2);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((m_c4 - m_c3) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.;
			m_mp[3] = 0.;
			m_mp[4] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.;
			m_np[3] = 0.;
			m_np[4] = 0.; //Unused

			doublereal a41 = -(m_c4 * (m_c4 - 2. * m_gamma) * (120. * m_c2 * m_c2 * m_c3 * m_c3 * m_gamma * m_gamma - 120. * m_c2 * m_c2 * m_c3 * m_c3 * m_gamma + 20. * m_c2 * m_c2 * m_c3 * m_c3 - 240. * m_c2 * m_c2 * m_c3 * m_gamma * m_gamma + 180. * m_c2 * m_c2 * m_c3 * m_gamma 
						- 25. * m_c2 * m_c2 * m_c3 - 240. * m_c2 * m_c2 * m_c4 * m_gamma * m_gamma * m_gamma + 360. * m_c2 * m_c2 * m_c4 * m_gamma * m_gamma - 120. * m_c2 * m_c2 * m_c4 * m_gamma + 10. * m_c2 * m_c2 * m_c4 + 240. * m_c2 * m_c2 * m_gamma * m_gamma * m_gamma - 200. * m_c2 * m_c2 * m_gamma * m_gamma 
						+ 30. * m_c2 * m_c2 * m_gamma - 240. * m_c2 * m_c3 * m_c3 * m_gamma * m_gamma + 180. * m_c2 * m_c3 * m_c3 * m_gamma - 25. * m_c2 * m_c3 * m_c3 - 240. * m_c2 * m_c3 * m_c4 * m_gamma * m_gamma * m_gamma + 360. * m_c2 * m_c3 * m_c4 * m_gamma * m_gamma - 120. * m_c2 * m_c3 * m_c4 * m_gamma + 10. * m_c2 * m_c3 * m_c4 
						+ 240. * m_c2 * m_c3 * m_gamma * m_gamma * m_gamma + 160. * m_c2 * m_c3 * m_gamma * m_gamma - 190. * m_c2 * m_c3 * m_gamma + 26. * m_c2 * m_c3 + 240. * m_c2 * m_c4 * m_c4 * m_gamma * m_gamma * m_gamma - 360. * m_c2 * m_c4 * m_c4 * m_gamma * m_gamma + 120. * m_c2 * m_c4 * m_c4 * m_gamma - 10. * m_c2 * m_c4 * m_c4 
						+ 240. * m_c2 * m_c4 * m_gamma * m_gamma * m_gamma - 280. * m_c2 * m_c4 * m_gamma * m_gamma + 80. * m_c2 * m_c4 * m_gamma - 6. * m_c2 * m_c4 - 480. * m_c2 * m_gamma * m_gamma * m_gamma + 320. * m_c2 * m_gamma * m_gamma - 40. * m_c2 * m_gamma - 240. * m_c3 * m_c3 * m_c4 * m_gamma * m_gamma * m_gamma + 360. * m_c3 * m_c3 * m_c4 * m_gamma * m_gamma 
						- 120. * m_c3 * m_c3 * m_c4 * m_gamma + 10. * m_c3 * m_c3 * m_c4 + 240. * m_c3 * m_c3 * m_gamma * m_gamma * m_gamma - 200. * m_c3 * m_c3 * m_gamma * m_gamma + 30. * m_c3 * m_c3 * m_gamma + 480. * m_c3 * m_c4 * m_gamma * m_gamma * m_gamma * m_gamma - 240. * m_c3 * m_c4 * m_gamma * m_gamma * m_gamma - 360. * m_c3 * m_c4 * m_gamma * m_gamma + 160. * m_c3 * m_c4 * m_gamma 
						- 14. * m_c3 * m_c4 - 480. * m_c3 * m_gamma * m_gamma * m_gamma * m_gamma + 160. * m_c3 * m_gamma * m_gamma * m_gamma + 120. * m_c3 * m_gamma * m_gamma - 24. * m_c3 * m_gamma - 240. * m_c4 * m_c4 * m_gamma * m_gamma * m_gamma + 280. * m_c4 * m_c4 * m_gamma * m_gamma - 80. * m_c4 * m_c4 * m_gamma + 6. * m_c4 * m_c4 - 480. * m_c4 * m_gamma * m_gamma * m_gamma * m_gamma 
						+ 640. * m_c4 * m_gamma * m_gamma * m_gamma - 200. * m_c4 * m_gamma * m_gamma + 16. * m_c4 * m_gamma + 480. * m_gamma * m_gamma * m_gamma * m_gamma - 360. * m_gamma * m_gamma * m_gamma + 48. * m_gamma * m_gamma)) / (4. * m_gamma * (m_c2 - 2. * m_gamma) * (m_c3 - 2. * m_gamma) * (15. * m_c2 + 15. * m_c3 + 90. * m_gamma - 20. * m_c2 * m_c3 - 100. * m_c2 * m_gamma - 100. * m_c3 * m_gamma 
						+ 120. * m_c2 * m_gamma * m_gamma + 120. * m_c3 * m_gamma * m_gamma - 120. * m_gamma * m_gamma + 120. * m_c2 * m_c3 * m_gamma - 120. * m_c2 * m_c3 * m_gamma * m_gamma - 12.));
			doublereal a42 = (m_c4 * (m_c2 - m_c4) * (m_c4 - 2. * m_gamma) * (4. * m_c2 - 7. * m_c3 + 3. * m_c4 - 5. * m_c2 * m_c4 - 50. * m_c2 * m_gamma + 90. * m_c3 * m_gamma - 40. * m_c4 * m_gamma + 160. * m_c2 * m_gamma * m_gamma - 120. * m_c2 * m_gamma * m_gamma * m_gamma - 300. * m_c3 * m_gamma * m_gamma - 60. * m_c3 * m_c3 * m_gamma + 240. * m_c3 * m_gamma * m_gamma * m_gamma + 140. * m_c4 * m_gamma * m_gamma 
						- 120. * m_c4 * m_gamma * m_gamma * m_gamma + 5. * m_c3 * m_c3 + 180. * m_c3 * m_c3 * m_gamma * m_gamma - 120. * m_c3 * m_c3 * m_gamma * m_gamma * m_gamma + 60. * m_c2 * m_c4 * m_gamma - 180. * m_c2 * m_c4 * m_gamma * m_gamma + 120. * m_c2 * m_c4 * m_gamma * m_gamma * m_gamma)) / (m_c2 * (m_c2 - m_c3) * (m_c2 - 2. * m_gamma) * (15. * m_c2 + 15. * m_c3 + 90. * m_gamma - 20. * m_c2 * m_c3 
						- 100. * m_c2 * m_gamma - 100. * m_c3 * m_gamma + 120. * m_c2 * m_gamma * m_gamma + 120. * m_c3 * m_gamma * m_gamma - 120. * m_gamma * m_gamma + 120. * m_c2 * m_c3 * m_gamma - 120. * m_c2 * m_c3 * m_gamma * m_gamma - 12.));
			doublereal a43 = -(m_c4 * (m_c2 - m_c4) * (m_c3 - m_c4) * (m_c4 - 2. * m_gamma) * (5. * m_c2 + 40. * m_gamma - 60. * m_c2 * m_gamma + 180. * m_c2 * m_gamma * m_gamma - 120. * m_c2 * m_gamma * m_gamma * m_gamma - 140. * m_gamma * m_gamma + 120. * m_gamma * m_gamma * m_gamma - 3.)) / (m_c3 * (m_c2 - m_c3) * (m_c3 - 2. * m_gamma) * (15. * m_c2 + 15. * m_c3 + 90. * m_gamma - 20. * m_c2 * m_c3 
						- 100. * m_c2 * m_gamma - 100. * m_c3 * m_gamma + 120. * m_c2 * m_gamma * m_gamma + 120. * m_c3 * m_gamma * m_gamma - 120. * m_gamma * m_gamma + 120. * m_c2 * m_c3 * m_gamma - 120. * m_c2 * m_c3 * m_gamma * m_gamma - 12.));
			doublereal a40 = m_c4 - m_gamma - a41 - a42 - a43;



			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.;
			m_a[IDX_A4][DIFFERENTIAL] = 1.;
			m_a[IDX_A5][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = a43 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = a42 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = a41 * dT;
			m_b[IDX_B4][DIFFERENTIAL] = a40 * dT;
			m_b[IDX_B5][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(4)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "a4    = " << m_a[IDX_A4][DIFFERENTIAL] << std::endl
											<< "a5    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl
											<< "b4    = " << m_b[IDX_B4][DIFFERENTIAL] << std::endl
											<< "b5    = not needed" << std::endl);
			break;
		}

	case 5:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (1. - m_c4) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;
			//m_np[4] = 0.;
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;
			//m_np[3] = 0.;
			//m_np[4] = 0.;

			//second-order prediction
			//doublereal dalpha = (1. - m_c4) / (m_c4 - m_c3);
			//m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((1. - m_c4) * dT);
			//m_mp[1] = -m_mp[0];
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;
			//m_mp[4] = 0.;
			//m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			//m_np[1] = dalpha * (2. + 3. * dalpha);
			//m_np[2] = 0.;
			//m_np[3] = 0.;
			//m_np[4] = 0.;

			//fourth-order prediction
			doublereal dalpha1 = (1. - m_c4) / (m_c4 - m_c3);
			doublereal dalpha2 = (1. - m_c4) / (m_c3 - m_c2);
			m_mp[0] = -(2. * dalpha1 * dalpha1 * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
					+ dalpha1 * dalpha1 * (10. * dalpha2 * dalpha2 * dalpha2 + 25. * dalpha2 * dalpha2 + 13. * dalpha2) 
					+ dalpha1 * (20. * dalpha2 * dalpha2 * dalpha2 + 20. * dalpha2 * dalpha2) + 10. * dalpha2 * dalpha2 * dalpha2)) 
					/ ((dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (1. - m_c4) * dT);
			m_mp[1] = (2. * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
					+ dalpha1 * dalpha1 * (-5. * dalpha2 * dalpha2 * dalpha2 + dalpha2 * dalpha2 + 4. * dalpha2) 
					+ dalpha1 * (-7. * dalpha2 * dalpha2 * dalpha2 - dalpha2 * dalpha2) - 2. * dalpha2 * dalpha2 * dalpha2)) 
					/ (dalpha1 * (1. - m_c4) * dT);
			m_mp[2] = (2. * dalpha2 * dalpha2 * dalpha2 * dalpha2 * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (10. * dalpha2 + 10.) 
					+ dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 20. * dalpha2 + 5.) + dalpha1 * (7. * dalpha2 * dalpha2 + 7. * dalpha2) 
					+ 2. * dalpha2 * dalpha2)) / (dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (1. - m_c4) * dT);
			m_mp[3] = 0.;
			m_mp[4] = 0.;
			m_np[0] = ((1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
					+ dalpha1 * dalpha1 * (11. * dalpha2 * dalpha2 + 10. * dalpha2 + 1.) + dalpha1 * (7. * dalpha2 * dalpha2 + 2. * dalpha2) 
					+ dalpha2 * dalpha2)) / ((dalpha1 + dalpha2) * (dalpha1 + dalpha2));
			m_np[1] = (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
					+ dalpha1 * dalpha1 * (12. * dalpha2 * dalpha2 + 12. * dalpha2 + 2.) + dalpha1 * (9. * dalpha2 * dalpha2 + 4. * dalpha2) 
					+ 2. * dalpha2 * dalpha2) / dalpha1;
			m_np[2] = (dalpha2 * dalpha2 * dalpha2 * (1. + dalpha1) * (dalpha1 * dalpha1 * (5. * dalpha2 + 4.) 
					+ dalpha1 * (7. * dalpha2 + 2.) + 2. * dalpha2)) / (dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2));
			m_np[3] = 0.;
			m_np[4] = 0.;


			//sixth-order prediction
			//doublereal dalpha1 = (1. - m_c4) / (m_c4 - m_c3);
			//doublereal dalpha2 = (1. - m_c4) / (m_c3 - m_c2);
			//doublereal dalpha3 = (1. - m_c4) / (m_c2 - 2. * m_gamma);
			//m_mp[0] = -(2. * (dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (21. * pow(dalpha1, 5) + 77. * pow(dalpha1, 4) * dalpha2 + 49. * pow(dalpha1, 4) * dalpha3 + 63. * pow(dalpha1, 4) + 108. * pow(dalpha1, 3) * pow(dalpha2, 2) 
			//		+ 136. * pow(dalpha1, 3) * dalpha2 * dalpha3 + 182. * pow(dalpha1, 3) * dalpha2 + 38. * pow(dalpha1, 3) * pow(dalpha3, 2) + 112. * pow(dalpha1, 3) * dalpha3 + 63. * pow(dalpha1, 3) + 72. * pow(dalpha1, 2) * pow(dalpha2, 3) 
			//		+ 134. * dalpha1 * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 188. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 72. * dalpha1 * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 230. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 133. * dalpha1 * dalpha1 * dalpha2 
			//		+ 10. * dalpha1 * dalpha1 * dalpha3 * dalpha3 * dalpha3 + 62. * dalpha1 * dalpha1 * dalpha3 * dalpha3 + 77. * dalpha1 * dalpha1 * dalpha3 + 21. * dalpha1 * dalpha1 + 23. * dalpha1 * pow(dalpha2, 4) + 56. * dalpha1 * pow(dalpha2, 3) * dalpha3 
			//		+ 82. * dalpha1 * pow(dalpha2, 3) + 43. * dalpha1 * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 149. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 87. * dalpha1 * dalpha2 * dalpha2 + 10. * dalpha1 * dalpha2 * pow(dalpha3, 3) + 77. * dalpha1 * dalpha2 * dalpha3 * dalpha3 
			//		+ 101. * dalpha1 * dalpha2 * dalpha3 + 28. * dalpha1 * dalpha2 + 10. * dalpha1 * pow(dalpha3, 3) + 24. * dalpha1 * dalpha3 * dalpha3 + 14. * dalpha1 * dalpha3 + 3. * pow(dalpha2, 5) + 9. * pow(dalpha2, 4) * dalpha3 + 13. * pow(dalpha2, 4) + 9. * pow(dalpha2, 3) * dalpha3 * dalpha3 
			//		+ 31. * pow(dalpha2, 3) * dalpha3 + 17. * pow(dalpha2, 3) + 3. * dalpha2 * dalpha2 * pow(dalpha3, 3) + 23. * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 29. * dalpha2 * dalpha2 * dalpha3 + 7. * dalpha2 * dalpha2 + 5. * dalpha2 * pow(dalpha3, 3) + 12. * dalpha2 * pow(dalpha3, 2) + 7. * dalpha2 * dalpha3)) 
			//		/ (pow(dalpha1, 3) * pow(dalpha1 + dalpha2, 3) * pow(dalpha1 + dalpha2 + dalpha3, 3) * (1. - m_c4) * dT);
			//m_mp[1] = -(2. * (dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (4. * pow(dalpha1, 4) * dalpha2 + 2. * pow(dalpha1, 4) * dalpha3 + 9. * pow(dalpha1, 3) * pow(dalpha2, 2) + 11. * pow(dalpha1, 3) * dalpha2 * dalpha3 + 22. * pow(dalpha1, 3) * dalpha2 
			//		+ 4. * pow(dalpha1, 3) * pow(dalpha3, 2) + 11. * pow(dalpha1, 3) * dalpha3 + 3. * pow(dalpha1, 2) * pow(dalpha2, 3) + 7. * pow(dalpha1, 2) * pow(dalpha2, 2) * dalpha3 + 31. * pow(dalpha1, 2) * pow(dalpha2, 2) + 6. * pow(dalpha1, 2) * dalpha2 * pow(dalpha3, 2) 
			//		+ 40. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 32. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * pow(dalpha3, 3) + 16. * dalpha1 * dalpha1 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 - 5. * dalpha1 * pow(dalpha2, 4) 
			//		- 11. * dalpha1 * pow(dalpha2, 3) * dalpha3 - 4. * dalpha1 * pow(dalpha2, 3) - 7. * dalpha1 * dalpha2 * dalpha2 * dalpha3 * dalpha3 - 2. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 15. * dalpha1 * dalpha2 * dalpha2 - dalpha1 * dalpha2 * pow(dalpha3, 3) 
			//		+ 7. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 22. * dalpha1 * dalpha2 * dalpha3 + 14. * dalpha1 * dalpha2 + 5. * dalpha1 * pow(dalpha3, 3) + 12. * dalpha1 * dalpha3 * dalpha3 + 7. * dalpha1 * dalpha3 - 3. * pow(dalpha2, 5) - 9. * pow(dalpha2, 4) * dalpha3 - 13. * pow(dalpha2, 4) 
			//		- 9. * pow(dalpha2, 3) * dalpha3 * dalpha3 - 31. * pow(dalpha2, 3) * dalpha3 - 17. * pow(dalpha2, 3) - 3. * dalpha2 * dalpha2 * pow(dalpha3, 3) - 23. * dalpha2 * dalpha2 * dalpha3 * dalpha3 - 29. * dalpha2 * dalpha2 * dalpha3 - 7. * dalpha2 * dalpha2 - 5. * dalpha2 * pow(dalpha3, 3) 
			//		- 12. * dalpha2 * dalpha3 * dalpha3 - 7. * dalpha2 * dalpha3)) / (pow(dalpha1, 3) * pow(dalpha2, 3) * pow(dalpha2 + dalpha3, 3) * (1. - m_c4) * dT);
			//m_mp[2] = (2. * (dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (-2. * pow(dalpha1, 4) * dalpha2 + 2. * pow(dalpha1, 4) * dalpha3 - 6. * pow(dalpha1, 3) * pow(dalpha2, 2) + 5. * pow(dalpha1, 3) * dalpha2 * dalpha3 - 11. * pow(dalpha1, 3) * dalpha2 + 4. * pow(dalpha1, 3) * pow(dalpha3, 2) 
			//		+ 11. * pow(dalpha1, 3) * dalpha3 - 6. * dalpha1 * dalpha1 * pow(dalpha2, 3) + 4. * dalpha1 * dalpha1 * dalpha2 * dalpha2 * dalpha3 - 26. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 12. * dalpha1 * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 25. * dalpha1 * dalpha1 * dalpha2 * dalpha3 
			//		- 16. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 - 2. * dalpha1 * pow(dalpha2, 4) + dalpha1 * pow(dalpha2, 3) * dalpha3 - 19. * dalpha1 * pow(dalpha2, 3) 
			//		+ 8. * dalpha1 * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha2 * dalpha2 * dalpha3 - 27. * dalpha1 * dalpha2 * dalpha2 + 5. * dalpha1 * dalpha2 * pow(dalpha3, 3) + 40. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 34. * dalpha1 * dalpha2 * dalpha3 - 7. * dalpha1 * dalpha2 
			//		+ 5. * dalpha1 * pow(dalpha3, 3) + 12. * dalpha1 * dalpha3 * dalpha3 + 7. * dalpha1 * dalpha3 - 4. * pow(dalpha2, 4) + 2. * pow(dalpha2, 3) * dalpha3 - 11. * pow(dalpha2, 3) + 16. * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 13. * dalpha2 * dalpha2 * dalpha3 - 7. * dalpha2 * dalpha2 
			//		+ 10. * dalpha2 * dalpha3 * dalpha3 * dalpha3 + 24. * dalpha2 * dalpha3 * dalpha3 + 14. * dalpha2 * dalpha3)) / (dalpha2 * dalpha2 * dalpha2 * dalpha3 * dalpha3 * dalpha3 * pow(dalpha1 + dalpha2, 3) * (1. - m_c4) * dT);
			//m_mp[3] = -m_mp[0] - m_mp[1] - m_mp[2];
			//m_mp[4] = 0.;
			//m_np[0] = ((dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (pow(dalpha1, 4) + 3. * pow(dalpha1, 3) * dalpha2 + 2. * pow(dalpha1, 3) * dalpha3 + 10. * pow(dalpha1, 3) + 3. * pow(dalpha1, 2) * pow(dalpha2, 2) + 4. * pow(dalpha1, 2) * dalpha2 * dalpha3 + 23. * pow(dalpha1, 2) * dalpha2 
			//		+ dalpha1 * dalpha1 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 + 24. * dalpha1 * dalpha1 + dalpha1 * pow(dalpha2, 3) + 2. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 16. * dalpha1 * dalpha2 * dalpha2 + dalpha1 * dalpha2 * dalpha3 * dalpha3 + 22. * dalpha1 * dalpha2 * dalpha3 
			//		+ 37. * dalpha1 * dalpha2 + 6. * dalpha1 * dalpha3 * dalpha3 + 26. * dalpha1 * dalpha3 + 22. * dalpha1 + 3. * pow(dalpha2, 3) + 6. * dalpha2 * dalpha2 * dalpha3 + 13. * dalpha2 * dalpha2 + 3. * dalpha2 * dalpha3 * dalpha3 + 18. * dalpha2 * dalpha3 + 17. * dalpha2 + 5. * dalpha3 * dalpha3 
			//		+ 12. * dalpha3 + 7.)) / (dalpha1 * dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2 + dalpha3) * (dalpha1 + dalpha2 + dalpha3));
			//m_np[1] = ((dalpha1 + dalpha2 + 1.) * (2. * pow(dalpha1, 4) + 6. * pow(dalpha1, 3) * dalpha2 + 4. * pow(dalpha1, 3) * dalpha3 + 13. * pow(dalpha1, 3) + 6. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 8. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 29. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 * dalpha3 
			//		+ 20. * dalpha1 * dalpha1 * dalpha3 + 27. * dalpha1 * dalpha1 + 2. * dalpha1 * dalpha2 * dalpha2 * dalpha2 + 4. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 19. * dalpha1 * dalpha2 * dalpha2 + 2. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 26. * dalpha1 * dalpha2 * dalpha3 + 40. * dalpha1 * dalpha2 + 7. * dalpha1 * dalpha3 * dalpha3 
			//		+ 28. * dalpha1 * dalpha3 + 23. * dalpha1 + 3. * dalpha2 * dalpha2 * dalpha2 + 6. * dalpha2 * dalpha2 * dalpha3 + 13. * dalpha2 * dalpha2 + 3. * dalpha2 * dalpha3 * dalpha3 + 18. * dalpha2 * dalpha3 + 17. * dalpha2 + 5. * dalpha3 * dalpha3 + 12. * dalpha3 + 7.)) / (dalpha1 * dalpha1 * dalpha2 * dalpha2 * (dalpha2 + dalpha3) * (dalpha2 + dalpha3));
			//m_np[2] = ((dalpha1 + 1.) * (2. * pow(dalpha1, 4) + 6. * pow(dalpha1, 3) * dalpha2 + 4. * pow(dalpha1, 3) * dalpha3 + 13. * pow(dalpha1, 3) + 6. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 8. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 30. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 * dalpha3 
			//		+ 20. * dalpha1 * dalpha1 * dalpha3 + 27. * dalpha1 * dalpha1 + 2. * dalpha1 * dalpha2 * dalpha2 * dalpha2 + 4. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 21. * dalpha1 * dalpha2 * dalpha2 + 2. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 28. * dalpha1 * dalpha2 * dalpha3 + 42. * dalpha1 * dalpha2 + 7. * dalpha1 * dalpha3 * dalpha3 
			//		+ 28. * dalpha1 * dalpha3 + 23. * dalpha1 + 4. * dalpha2 * dalpha2 * dalpha2 + 8. * dalpha2 * dalpha2 * dalpha3 + 15. * dalpha2 * dalpha2 + 4. * dalpha2 * dalpha3 * dalpha3 + 20. * dalpha2 * dalpha3 + 18. * dalpha2 + 5. * dalpha3 * dalpha3 + 12. * dalpha3 + 7.)) / (dalpha2 * dalpha2 * dalpha3 * dalpha3 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2));
			//m_np[3] = ((dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (16. * dalpha1 + 11. * dalpha2 + 6. * dalpha3 + 15. * dalpha1 * dalpha2 + 8. * dalpha1 * dalpha3 + 4. * dalpha2 * dalpha3 + 2. * dalpha1 * dalpha2 * dalpha2 + 4. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 + 11. * dalpha1 * dalpha1 + 2. * dalpha1 * dalpha1 * dalpha1 
			//		+ 4. * dalpha2 * dalpha2 + 2. * dalpha1 * dalpha2 * dalpha3 + 7.)) / (dalpha3 * dalpha3 * (dalpha2 + dalpha3) * (dalpha2 + dalpha3) * (dalpha1 + dalpha2 + dalpha3) * (dalpha1 + dalpha2 + dalpha3));
			//m_np[4] = 0.;


			doublereal q1 = (15. * m_c2 + 15. * m_c3 + 15. * m_c4 + 60. * m_gamma - 20. * m_c2 * m_c3 - 20. * m_c2 * m_c4 - 20. * m_c3 * m_c4 - 60. * m_c2 * m_gamma - 60. * m_c3 * m_gamma - 60. * m_c4 * m_gamma + 30. * m_c2 * m_c3 * m_c4 + 60. * m_c2 * m_c3 * m_gamma + 60. * m_c2 * m_c4 * m_gamma + 60. * m_c3 * m_c4 * m_gamma - 60. * m_c2 * m_c3 * m_c4 * m_gamma - 12.) / (120. * m_gamma * (m_c2 - 2. * m_gamma) * (m_c3 - 2. * m_gamma) * (m_c4 - 2. * m_gamma));
			doublereal q2 = -(15. * m_c3 + 15. * m_c4 + 90. * m_gamma - 20. * m_c3 * m_c4 - 100. * m_c3 * m_gamma - 100. * m_c4 * m_gamma + 120. * m_c3 * m_gamma * m_gamma + 120. * m_c4 * m_gamma * m_gamma - 120. * m_gamma * m_gamma + 120. * m_c3 * m_c4 * m_gamma - 120. * m_c3 * m_c4 * m_gamma * m_gamma - 12.) / (60. * m_c2 * (m_c2 - m_c3) * (m_c2 - m_c4) * (m_c2 - 2. * m_gamma));
			doublereal q3 = (15. * m_c2 + 15. * m_c4 + 90. * m_gamma - 20. * m_c2 * m_c4 - 100. * m_c2 * m_gamma - 100. * m_c4 * m_gamma + 120. * m_c2 * m_gamma * m_gamma + 120. * m_c4 * m_gamma * m_gamma - 120. * m_gamma * m_gamma + 120. * m_c2 * m_c4 * m_gamma - 120. * m_c2 * m_c4 * m_gamma * m_gamma - 12.) / (60. * m_c3 * (m_c2 - m_c3) * (m_c3 - m_c4) * (m_c3 - 2. * m_gamma));
			doublereal q4 = -(15. * m_c2 + 15. * m_c3 + 90. * m_gamma - 20. * m_c2 * m_c3 - 100. * m_c2 * m_gamma - 100. * m_c3 * m_gamma + 120. * m_c2 * m_gamma * m_gamma + 120. * m_c3 * m_gamma * m_gamma - 120. * m_gamma * m_gamma + 120. * m_c2 * m_c3 * m_gamma - 120. * m_c2 * m_c3 * m_gamma * m_gamma - 12.) / (60. * m_c4 * (m_c2 - m_c4) * (m_c3 - m_c4) * (m_c4 - 2. * m_gamma));
			doublereal q0 = 1. - m_gamma - q1 - q2 - q3 - q4;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.;
			m_a[IDX_A4][DIFFERENTIAL] = 0.;
			m_a[IDX_A5][DIFFERENTIAL] = 1.;
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = q4 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = q3 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = q2 * dT;
			m_b[IDX_B4][DIFFERENTIAL] = q1 * dT;
			m_b[IDX_B5][DIFFERENTIAL] = q0 * dT;

			DEBUGCOUT("PredictForStageS(5)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
											<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "a4    = " << m_a[IDX_A4][DIFFERENTIAL] << std::endl
											<< "a5    = " << m_a[IDX_A5][DIFFERENTIAL] << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl
											<< "b4    = " << m_b[IDX_B4][DIFFERENTIAL] << std::endl
											<< "b5    = " << m_b[IDX_B5][DIFFERENTIAL] << std::endl);
			break;
		}

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	m_a[IDX_A1][ALGEBRAIC] = m_a[IDX_A1][DIFFERENTIAL];
	m_a[IDX_A2][ALGEBRAIC] = m_a[IDX_A2][DIFFERENTIAL];
	m_a[IDX_A3][ALGEBRAIC] = m_a[IDX_A3][DIFFERENTIAL];
	m_a[IDX_A4][ALGEBRAIC] = m_a[IDX_A4][DIFFERENTIAL];
	m_a[IDX_A5][ALGEBRAIC] = m_a[IDX_A5][DIFFERENTIAL];
	m_b[IDX_B0][ALGEBRAIC] = m_b[IDX_B0][DIFFERENTIAL];
	m_b[IDX_B1][ALGEBRAIC] = m_b[IDX_B1][DIFFERENTIAL];
	m_b[IDX_B2][ALGEBRAIC] = m_b[IDX_B2][DIFFERENTIAL];
	m_b[IDX_B3][ALGEBRAIC] = m_b[IDX_B3][DIFFERENTIAL];
	m_b[IDX_B4][ALGEBRAIC] = m_b[IDX_B4][DIFFERENTIAL];
	m_b[IDX_B5][ALGEBRAIC] = m_b[IDX_B5][DIFFERENTIAL];

	DEBUGCOUT("Algebraic coefficients:" << std::endl
		<< "Asymptotic rho =" << m_dAlgebraicRho << " (ignored)" << std::endl 
		<< "a1    = " << m_a[IDX_A1][ALGEBRAIC] << std::endl
		<< "a2    = " << m_a[IDX_A2][ALGEBRAIC] << std::endl
		<< "a3    = " << m_a[IDX_A3][ALGEBRAIC] << std::endl
		<< "a4    = " << m_a[IDX_A4][ALGEBRAIC] << std::endl
		<< "a5    = " << m_a[IDX_A5][ALGEBRAIC] << std::endl
		<< "b0    = " << m_b[IDX_B0][ALGEBRAIC] << std::endl
		<< "b1    = " << m_b[IDX_B1][ALGEBRAIC] << std::endl
		<< "b2    = " << m_b[IDX_B2][ALGEBRAIC] << std::endl
		<< "b3    = " << m_b[IDX_B3][ALGEBRAIC] << std::endl
		<< "b4    = " << m_b[IDX_B4][ALGEBRAIC] << std::endl
		<< "b5    = " << m_b[IDX_B5][ALGEBRAIC] << std::endl);

	db0Differential = m_b[IDX_B0][DIFFERENTIAL];
	db0Algebraic = m_b[IDX_B0][ALGEBRAIC];
}

doublereal
Mssth5Solver::dPredDerForStageS(unsigned uStage,
                                const std::array<doublereal, 5>& dXm1mN,
                                const std::array<doublereal, 6>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];

	case 2:
		return m_mp[0]*dXm1mN[IDX_Xs1]
			+ m_mp[1]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs1]
			+ m_np[1]*dXP0mN[IDX_XPm1]; 

	case 3:
		return m_mp[0]*dXm1mN[IDX_Xs2]
			+ m_mp[1]*dXm1mN[IDX_Xs1]
			+ m_mp[2]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs2]
			+ m_np[1]*dXP0mN[IDX_XPs1]
			+ m_np[2]*dXP0mN[IDX_XPm1];

	case 4:
		return m_mp[0]*dXm1mN[IDX_Xs3]
			+ m_mp[1]*dXm1mN[IDX_Xs2]
			+ m_mp[2]*dXm1mN[IDX_Xs1]
			+ m_mp[3]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs3]
			+ m_np[1]*dXP0mN[IDX_XPs2]
			+ m_np[2]*dXP0mN[IDX_XPs1]
			+ m_np[3]*dXP0mN[IDX_XPm1];

	case 5:
		return m_mp[0]*dXm1mN[IDX_Xs4]
			+ m_mp[1]*dXm1mN[IDX_Xs3]
			+ m_mp[2]*dXm1mN[IDX_Xs2]
			+ m_mp[3]*dXm1mN[IDX_Xs1]
			+ m_mp[4]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs4]
			+ m_np[1]*dXP0mN[IDX_XPs3]
			+ m_np[2]*dXP0mN[IDX_XPs2]
			+ m_np[3]*dXP0mN[IDX_XPs1]
			+ m_np[4]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Mssth5Solver::dPredStateForStageS(unsigned uStage,
                                  const std::array<doublereal, 5>& dXm1mN,
                                  const std::array<doublereal, 6>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 4:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A4][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs4]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B4][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 5:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs4]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A4][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A5][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs4]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B4][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B5][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Mssth5Solver::dPredDerAlgForStageS(unsigned uStage,
                                   const std::array<doublereal, 5>& dXm1mN,
                                   const std::array<doublereal, 6>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];

	case 2:
		return dXP0mN[IDX_XPs1]; 

	case 3:
		return dXP0mN[IDX_XPs2];

	case 4:
		return dXP0mN[IDX_XPs3];

	case 5:
		return dXP0mN[IDX_XPs4];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
Mssth5Solver::dPredStateAlgForStageS(unsigned uStage,
                                     const std::array<doublereal, 5>& dXm1mN,
                                     const std::array<doublereal, 6>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 4:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A4][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs4]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B4][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 5:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs4]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A4][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A5][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs4]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B4][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B5][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

/* Mssth5Solver - end */

/* DIRK33Solver - begin */

DIRK33Solver::DIRK33Solver(const doublereal Tl,
	const doublereal dSolTl,
	const integer iMaxIt,
	//const DriveCaller* pRho,
	//const DriveCaller* pAlgRho,
	const bool bmod_res_test)
: tplStageNIntegrator<3>(iMaxIt, Tl, dSolTl, bmod_res_test)
{
	NO_OP;
}

DIRK33Solver::~DIRK33Solver(void)
{
	NO_OP;
}


void
DIRK33Solver::SetCoefForStageS(unsigned uStage,
	doublereal dT,
	doublereal dAlpha,
	enum StepChange /* NewStep */)
{
	switch (uStage) {
	case 1:
		m_gamma = 0.43586652150845899941601945;

		ASSERT(pDM != NULL);
		pDM->SetTime(pDM->dGetTime() - (1. - 2.*m_gamma)*dT, dT, pDM->pGetDrvHdl()->iGetStep());
	
		m_a[IDX_A1][DIFFERENTIAL] = 1.;
		m_a[IDX_A2][DIFFERENTIAL] = 0.;	// Unused
		m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B1][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B2][DIFFERENTIAL] = 0.;	// Unused
		m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused

		DEBUGCOUT("PredictForStageS(1)" << std::endl
			<< "Alpha = " << dAlpha << std::endl
			<< "Differential coefficients:" << std::endl
                        //<< "Asymptotic rho =" << m_dRho << std::endl 
			<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
			<< "a2    = not needed" << std::endl
			<< "a3    = not needed" << std::endl
			<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
			<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
			<< "b2    = not needed" << std::endl
			<< "b3    = not needed" << std::endl);
		break;

	case 2:
		{
			m_c2 = 3. / 5.;
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (m_c2 - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = (m_c2 - 2. * m_gamma) / (2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((m_c2 - 2. * m_gamma) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.; //Unused

			doublereal a21 = m_c2 * (m_c2 - 2. * m_gamma) / (4. * m_gamma);
			doublereal a20 = m_c2 - m_gamma - a21;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 1.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = a21 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = a20 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(2)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
                                                                                        //<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = not needed" << std::endl);
			break;
		}

	case 3:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (1. - m_c2) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;

			//second-order prediction
			doublereal dalpha = (1. - m_c2) / (m_c2 - 2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((1. - m_c2) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.;
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.;

			//fourth-order prediction
			//doublereal dalpha1 = (1. - m_c2) / (m_c2 - 2. * m_gamma);
			//doublereal dalpha2 = (1. - m_c2) / (2. * m_gamma);
			//m_mp[0] = -(2. * dalpha1 * dalpha1 * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
			//		+ dalpha1 * dalpha1 * (10. * dalpha2 * dalpha2 * dalpha2 + 25. * dalpha2 * dalpha2 + 13. * dalpha2) 
			//		+ dalpha1 * (20. * dalpha2 * dalpha2 * dalpha2 + 20. * dalpha2 * dalpha2) + 10. * dalpha2 * dalpha2 * dalpha2)) 
			//		/ ((dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (1. - m_c2) * dT);
			//m_mp[1] = (2. * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
			//		+ dalpha1 * dalpha1 * (-5. * dalpha2 * dalpha2 * dalpha2 + dalpha2 * dalpha2 + 4. * dalpha2) 
			//		+ dalpha1 * (-7. * dalpha2 * dalpha2 * dalpha2 - dalpha2 * dalpha2) - 2. * dalpha2 * dalpha2 * dalpha2)) 
			//		/ (dalpha1 * (1. - m_c2) * dT);
			//m_mp[2] = (2. * dalpha2 * dalpha2 * dalpha2 * dalpha2 * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (10. * dalpha2 + 10.) 
			//		+ dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 20. * dalpha2 + 5.) + dalpha1 * (7. * dalpha2 * dalpha2 + 7. * dalpha2) 
			//		+ 2. * dalpha2 * dalpha2)) / (dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (1. - m_c2) * dT);
			//m_np[0] = ((1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
			//		+ dalpha1 * dalpha1 * (11. * dalpha2 * dalpha2 + 10. * dalpha2 + 1.) + dalpha1 * (7. * dalpha2 * dalpha2 + 2. * dalpha2) 
			//		+ dalpha2 * dalpha2)) / ((dalpha1 + dalpha2) * (dalpha1 + dalpha2));
			//m_np[1] = (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
			//		+ dalpha1 * dalpha1 * (12. * dalpha2 * dalpha2 + 12. * dalpha2 + 2.) + dalpha1 * (9. * dalpha2 * dalpha2 + 4. * dalpha2) 
			//		+ 2. * dalpha2 * dalpha2) / dalpha1;
			//m_np[2] = (dalpha2 * dalpha2 * dalpha2 * (1. + dalpha1) * (dalpha1 * dalpha1 * (5. * dalpha2 + 4.) 
			//		+ dalpha1 * (7. * dalpha2 + 2.) + 2. * dalpha2)) / (dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2));

			doublereal q1 = (-2. + 3. * m_c2 + 6. * m_gamma * (1. - m_c2)) / (12. * m_gamma * (m_c2 - 2. * m_gamma));
			doublereal q2 = (1. - 6. * m_gamma + 6. * m_gamma * m_gamma) / (3. * m_c2 * (m_c2 - 2. * m_gamma));
			doublereal q0 = 1. - m_gamma - q1 - q2;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 1.;
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = q2 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = q1 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = q0 * dT;

			DEBUGCOUT("PredictForStageS(3)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
                                  //<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl);
			break;
		}

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	m_a[IDX_A1][ALGEBRAIC] = m_a[IDX_A1][DIFFERENTIAL];
	m_a[IDX_A2][ALGEBRAIC] = m_a[IDX_A2][DIFFERENTIAL];
	m_a[IDX_A3][ALGEBRAIC] = m_a[IDX_A3][DIFFERENTIAL];
	m_b[IDX_B0][ALGEBRAIC] = m_b[IDX_B0][DIFFERENTIAL];
	m_b[IDX_B1][ALGEBRAIC] = m_b[IDX_B1][DIFFERENTIAL];
	m_b[IDX_B2][ALGEBRAIC] = m_b[IDX_B2][DIFFERENTIAL];
	m_b[IDX_B3][ALGEBRAIC] = m_b[IDX_B3][DIFFERENTIAL];

	DEBUGCOUT("Algebraic coefficients:" << std::endl
                //<< "Asymptotic rho =" << m_dAlgebraicRho << " (ignored)" << std::endl 
		<< "a1    = " << m_a[IDX_A1][ALGEBRAIC] << std::endl
		<< "a2    = " << m_a[IDX_A2][ALGEBRAIC] << std::endl
		<< "a3    = " << m_a[IDX_A3][ALGEBRAIC] << std::endl
		<< "b0    = " << m_b[IDX_B0][ALGEBRAIC] << std::endl
		<< "b1    = " << m_b[IDX_B1][ALGEBRAIC] << std::endl
		<< "b2    = " << m_b[IDX_B2][ALGEBRAIC] << std::endl
		<< "b3    = " << m_b[IDX_B3][ALGEBRAIC] << std::endl);

	db0Differential = m_b[IDX_B0][DIFFERENTIAL];
	db0Algebraic = m_b[IDX_B0][ALGEBRAIC];
}

doublereal
DIRK33Solver::dPredDerForStageS(unsigned uStage,
                                const std::array<doublereal, 3>& dXm1mN,
                                const std::array<doublereal, 4>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];//constant prediction

	case 2:
		return m_mp[0]*dXm1mN[IDX_Xs1]
			+ m_mp[1]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs1]
			+ m_np[1]*dXP0mN[IDX_XPm1]; 

	case 3:
		return m_mp[0]*dXm1mN[IDX_Xs2]
			+ m_mp[1]*dXm1mN[IDX_Xs1]
			+ m_mp[2]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs2]
			+ m_np[1]*dXP0mN[IDX_XPs1]
			+ m_np[2]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
DIRK33Solver::dPredStateForStageS(unsigned uStage,
                                  const std::array<doublereal, 3>& dXm1mN,
                                  const std::array<doublereal, 4>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
DIRK33Solver::dPredDerAlgForStageS(unsigned uStage,
                                   const std::array<doublereal, 3>& dXm1mN,
                                   const std::array<doublereal, 4>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];

	case 2:
		return dXP0mN[IDX_XPs1]; 

	case 3:
		return dXP0mN[IDX_XPs2];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
DIRK33Solver::dPredStateAlgForStageS(unsigned uStage,
                                     const std::array<doublereal, 3>& dXm1mN,
                                     const std::array<doublereal, 4>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

/* DIRK33Solver - end */

/* DIRK43Solver - begin */

DIRK43Solver::DIRK43Solver(const doublereal Tl,
	const doublereal dSolTl,
	const integer iMaxIt,
	//const DriveCaller* pRho,
	//const DriveCaller* pAlgRho,
	const bool bmod_res_test)
: tplStageNIntegrator<4>(iMaxIt, Tl, dSolTl, bmod_res_test)
{
	NO_OP;
}

DIRK43Solver::~DIRK43Solver(void)
{
	NO_OP;
}

void
DIRK43Solver::SetCoefForStageS(unsigned uStage,
	doublereal dT,
	doublereal dAlpha,
	enum StepChange /* NewStep */)
{
	switch (uStage) {
	case 1:
		m_gamma = 9. / 40.;

		ASSERT(pDM != NULL);
		pDM->SetTime(pDM->dGetTime() - (1. - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

		m_a[IDX_A1][DIFFERENTIAL] = 1.;
		m_a[IDX_A2][DIFFERENTIAL] = 0.;	// Unused
		m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
		m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B1][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B2][DIFFERENTIAL] = 0.;	// Unused
		m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused

		DEBUGCOUT("PredictForStageS(1)" << std::endl
			<< "Alpha = " << dAlpha << std::endl
			<< "Differential coefficients:" << std::endl
                        //<< "Asymptotic rho =" << m_dRho << std::endl 
			<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
			<< "a2    = not needed" << std::endl
			<< "a3    = not needed" << std::endl
			<< "a4    = not needed" << std::endl
			<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
			<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
			<< "b2    = not needed" << std::endl
			<< "b3    = not needed" << std::endl
			<< "b4    = not needed" << std::endl);
		break;

	case 2:
		{
			m_c2 = 9. * (2. + sqrt(2.)) / 40.;
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (m_c2 - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;//Unused
			//m_mp[3] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;//Unused
			//m_np[3] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = (m_c2 - 2. * m_gamma) / (2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((m_c2 - 2. * m_gamma) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.; //Unused
			m_mp[3] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.; //Unused
			m_np[3] = 0.; //Unused

			doublereal a21 = (m_c2 * (m_c2 - 2. * m_gamma)) / (4. * m_gamma);
			doublereal a20 = m_c2 - m_gamma - a21;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 1.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
			m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = a21 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = a20 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(2)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
                                                                                        //<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = not needed" << std::endl
											<< "a4    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = not needed" << std::endl
											<< "b4    = not needed" << std::endl);
			break;
		}

	case 3:
		{
			m_c3 = 3. / 5.;
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (m_c3 - m_c2) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;
			//m_np[3] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = (m_c3 - m_c2) / (m_c2 - 2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((m_c3 - m_c2) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.;
			m_mp[3] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.;
			m_np[3] = 0.; //Unused

			doublereal a31 = (-4. * m_gamma * m_gamma * m_gamma + 12. * m_c3 * m_gamma * m_gamma - 4. * m_c3 * m_c3 * m_gamma 
							- 2. * m_c2 * m_c3 * m_gamma + m_c2 * m_c3 * m_c3) / (4. * m_gamma * (m_c2 - 2. * m_gamma));
			doublereal a32 = m_gamma * (2. * m_gamma * m_gamma - 4. * m_c3 * m_gamma + m_c3 * m_c3) / (m_c2 * (m_c2 - 2. * m_gamma));
			doublereal a30 = m_c3 - m_gamma - a31 - a32;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 1.;
			m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = a32 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = a31 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = a30 * dT;
			m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(3)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
                                                                                        //<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "a4    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl
											<< "b4    =  not needed" << std::endl);
			break;
		}

	case 4:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (1. - m_c3) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;
			//m_np[3] = 0.;

			//second-order prediction
			doublereal dalpha = (1. - m_c3) / (m_c3 - m_c2);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((1. - m_c3) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.;
			m_mp[3] = 0.;
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.;
			m_np[3] = 0.;

			//fourth-order prediction
			//doublereal dalpha1 = (1. - m_c3) / (m_c3 - m_c2);
			//doublereal dalpha2 = (1. - m_c3) / (m_c2 - 2. * m_gamma);
			//m_mp[0] = -(2. * dalpha1 * dalpha1 * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
			//		+ dalpha1 * dalpha1 * (10. * dalpha2 * dalpha2 * dalpha2 + 25. * dalpha2 * dalpha2 + 13. * dalpha2) 
			//		+ dalpha1 * (20. * dalpha2 * dalpha2 * dalpha2 + 20. * dalpha2 * dalpha2) + 10. * dalpha2 * dalpha2 * dalpha2)) 
			//		/ ((dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (1. - m_c3) * dT);
			//m_mp[1] = (2. * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
			//		+ dalpha1 * dalpha1 * (-5. * dalpha2 * dalpha2 * dalpha2 + dalpha2 * dalpha2 + 4. * dalpha2) 
			//		+ dalpha1 * (-7. * dalpha2 * dalpha2 * dalpha2 - dalpha2 * dalpha2) - 2. * dalpha2 * dalpha2 * dalpha2)) 
			//		/ (dalpha1 * (1. - m_c3) * dT);
			//m_mp[2] = (2. * dalpha2 * dalpha2 * dalpha2 * dalpha2 * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (10. * dalpha2 + 10.) 
			//		+ dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 20. * dalpha2 + 5.) + dalpha1 * (7. * dalpha2 * dalpha2 + 7. * dalpha2) 
			//		+ 2. * dalpha2 * dalpha2)) / (dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (1. - m_c3) * dT);
			//m_mp[3] = 0.;
			//m_np[0] = ((1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
			//		+ dalpha1 * dalpha1 * (11. * dalpha2 * dalpha2 + 10. * dalpha2 + 1.) + dalpha1 * (7. * dalpha2 * dalpha2 + 2. * dalpha2) 
			//		+ dalpha2 * dalpha2)) / ((dalpha1 + dalpha2) * (dalpha1 + dalpha2));
			//m_np[1] = (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
			//		+ dalpha1 * dalpha1 * (12. * dalpha2 * dalpha2 + 12. * dalpha2 + 2.) + dalpha1 * (9. * dalpha2 * dalpha2 + 4. * dalpha2) 
			//		+ 2. * dalpha2 * dalpha2) / dalpha1;
			//m_np[2] = (dalpha2 * dalpha2 * dalpha2 * (1. + dalpha1) * (dalpha1 * dalpha1 * (5. * dalpha2 + 4.) 
			//		+ dalpha1 * (7. * dalpha2 + 2.) + 2. * dalpha2)) / (dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2));
			//m_np[3] = 0.;

			//sixth-order prediction
			//doublereal dalpha1 = (1. - m_c3) / (m_c3 - m_c2);
			//doublereal dalpha2 = (1. - m_c3) / (m_c2 - 2. * m_gamma);
			//doublereal dalpha3 = (1. - m_c3) / (2. * m_gamma);
			//m_mp[0] = -(2. * (dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (21. * pow(dalpha1, 5) + 77. * pow(dalpha1, 4) * dalpha2 + 49. * pow(dalpha1, 4) * dalpha3 + 63. * pow(dalpha1, 4) + 108. * pow(dalpha1, 3) * pow(dalpha2, 2) 
			//		+ 136. * pow(dalpha1, 3) * dalpha2 * dalpha3 + 182. * pow(dalpha1, 3) * dalpha2 + 38. * pow(dalpha1, 3) * pow(dalpha3, 2) + 112. * pow(dalpha1, 3) * dalpha3 + 63. * pow(dalpha1, 3) + 72. * pow(dalpha1, 2) * pow(dalpha2, 3) 
			//		+ 134. * dalpha1 * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 188. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 72. * dalpha1 * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 230. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 133. * dalpha1 * dalpha1 * dalpha2 
			//		+ 10. * dalpha1 * dalpha1 * dalpha3 * dalpha3 * dalpha3 + 62. * dalpha1 * dalpha1 * dalpha3 * dalpha3 + 77. * dalpha1 * dalpha1 * dalpha3 + 21. * dalpha1 * dalpha1 + 23. * dalpha1 * pow(dalpha2, 4) + 56. * dalpha1 * pow(dalpha2, 3) * dalpha3 
			//		+ 82. * dalpha1 * pow(dalpha2, 3) + 43. * dalpha1 * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 149. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 87. * dalpha1 * dalpha2 * dalpha2 + 10. * dalpha1 * dalpha2 * pow(dalpha3, 3) + 77. * dalpha1 * dalpha2 * dalpha3 * dalpha3 
			//		+ 101. * dalpha1 * dalpha2 * dalpha3 + 28. * dalpha1 * dalpha2 + 10. * dalpha1 * pow(dalpha3, 3) + 24. * dalpha1 * dalpha3 * dalpha3 + 14. * dalpha1 * dalpha3 + 3. * pow(dalpha2, 5) + 9. * pow(dalpha2, 4) * dalpha3 + 13. * pow(dalpha2, 4) + 9. * pow(dalpha2, 3) * dalpha3 * dalpha3 
			//		+ 31. * pow(dalpha2, 3) * dalpha3 + 17. * pow(dalpha2, 3) + 3. * dalpha2 * dalpha2 * pow(dalpha3, 3) + 23. * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 29. * dalpha2 * dalpha2 * dalpha3 + 7. * dalpha2 * dalpha2 + 5. * dalpha2 * pow(dalpha3, 3) + 12. * dalpha2 * pow(dalpha3, 2) + 7. * dalpha2 * dalpha3)) 
			//		/ (pow(dalpha1, 3) * pow(dalpha1 + dalpha2, 3) * pow(dalpha1 + dalpha2 + dalpha3, 3) * (1. - m_c3) * dT);
			//m_mp[1] = -(2. * (dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (4. * pow(dalpha1, 4) * dalpha2 + 2. * pow(dalpha1, 4) * dalpha3 + 9. * pow(dalpha1, 3) * pow(dalpha2, 2) + 11. * pow(dalpha1, 3) * dalpha2 * dalpha3 + 22. * pow(dalpha1, 3) * dalpha2 
			//		+ 4. * pow(dalpha1, 3) * pow(dalpha3, 2) + 11. * pow(dalpha1, 3) * dalpha3 + 3. * pow(dalpha1, 2) * pow(dalpha2, 3) + 7. * pow(dalpha1, 2) * pow(dalpha2, 2) * dalpha3 + 31. * pow(dalpha1, 2) * pow(dalpha2, 2) + 6. * pow(dalpha1, 2) * dalpha2 * pow(dalpha3, 2) 
			//		+ 40. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 32. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * pow(dalpha3, 3) + 16. * dalpha1 * dalpha1 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 - 5. * dalpha1 * pow(dalpha2, 4) 
			//		- 11. * dalpha1 * pow(dalpha2, 3) * dalpha3 - 4. * dalpha1 * pow(dalpha2, 3) - 7. * dalpha1 * dalpha2 * dalpha2 * dalpha3 * dalpha3 - 2. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 15. * dalpha1 * dalpha2 * dalpha2 - dalpha1 * dalpha2 * pow(dalpha3, 3) 
			//		+ 7. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 22. * dalpha1 * dalpha2 * dalpha3 + 14. * dalpha1 * dalpha2 + 5. * dalpha1 * pow(dalpha3, 3) + 12. * dalpha1 * dalpha3 * dalpha3 + 7. * dalpha1 * dalpha3 - 3. * pow(dalpha2, 5) - 9. * pow(dalpha2, 4) * dalpha3 - 13. * pow(dalpha2, 4) 
			//		- 9. * pow(dalpha2, 3) * dalpha3 * dalpha3 - 31. * pow(dalpha2, 3) * dalpha3 - 17. * pow(dalpha2, 3) - 3. * dalpha2 * dalpha2 * pow(dalpha3, 3) - 23. * dalpha2 * dalpha2 * dalpha3 * dalpha3 - 29. * dalpha2 * dalpha2 * dalpha3 - 7. * dalpha2 * dalpha2 - 5. * dalpha2 * pow(dalpha3, 3) 
			//		- 12. * dalpha2 * dalpha3 * dalpha3 - 7. * dalpha2 * dalpha3)) / (pow(dalpha1, 3) * pow(dalpha2, 3) * pow(dalpha2 + dalpha3, 3) * (1. - m_c3) * dT);
			//m_mp[2] = (2. * (dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (-2. * pow(dalpha1, 4) * dalpha2 + 2. * pow(dalpha1, 4) * dalpha3 - 6. * pow(dalpha1, 3) * pow(dalpha2, 2) + 5. * pow(dalpha1, 3) * dalpha2 * dalpha3 - 11. * pow(dalpha1, 3) * dalpha2 + 4. * pow(dalpha1, 3) * pow(dalpha3, 2) 
			//		+ 11. * pow(dalpha1, 3) * dalpha3 - 6. * dalpha1 * dalpha1 * pow(dalpha2, 3) + 4. * dalpha1 * dalpha1 * dalpha2 * dalpha2 * dalpha3 - 26. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 12. * dalpha1 * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 25. * dalpha1 * dalpha1 * dalpha2 * dalpha3 
			//		- 16. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 - 2. * dalpha1 * pow(dalpha2, 4) + dalpha1 * pow(dalpha2, 3) * dalpha3 - 19. * dalpha1 * pow(dalpha2, 3) 
			//		+ 8. * dalpha1 * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha2 * dalpha2 * dalpha3 - 27. * dalpha1 * dalpha2 * dalpha2 + 5. * dalpha1 * dalpha2 * pow(dalpha3, 3) + 40. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 34. * dalpha1 * dalpha2 * dalpha3 - 7. * dalpha1 * dalpha2 
			//		+ 5. * dalpha1 * pow(dalpha3, 3) + 12. * dalpha1 * dalpha3 * dalpha3 + 7. * dalpha1 * dalpha3 - 4. * pow(dalpha2, 4) + 2. * pow(dalpha2, 3) * dalpha3 - 11. * pow(dalpha2, 3) + 16. * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 13. * dalpha2 * dalpha2 * dalpha3 - 7. * dalpha2 * dalpha2 
			//		+ 10. * dalpha2 * dalpha3 * dalpha3 * dalpha3 + 24. * dalpha2 * dalpha3 * dalpha3 + 14. * dalpha2 * dalpha3)) / (dalpha2 * dalpha2 * dalpha2 * dalpha3 * dalpha3 * dalpha3 * pow(dalpha1 + dalpha2, 3) * (1. - m_c3) * dT);
			//m_mp[3] = -m_mp[0] - m_mp[1] - m_mp[2];
			//m_np[0] = ((dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (pow(dalpha1, 4) + 3. * pow(dalpha1, 3) * dalpha2 + 2. * pow(dalpha1, 3) * dalpha3 + 10. * pow(dalpha1, 3) + 3. * pow(dalpha1, 2) * pow(dalpha2, 2) + 4. * pow(dalpha1, 2) * dalpha2 * dalpha3 + 23. * pow(dalpha1, 2) * dalpha2 
			//		+ dalpha1 * dalpha1 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 + 24. * dalpha1 * dalpha1 + dalpha1 * pow(dalpha2, 3) + 2. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 16. * dalpha1 * dalpha2 * dalpha2 + dalpha1 * dalpha2 * dalpha3 * dalpha3 + 22. * dalpha1 * dalpha2 * dalpha3 
			//		+ 37. * dalpha1 * dalpha2 + 6. * dalpha1 * dalpha3 * dalpha3 + 26. * dalpha1 * dalpha3 + 22. * dalpha1 + 3. * pow(dalpha2, 3) + 6. * dalpha2 * dalpha2 * dalpha3 + 13. * dalpha2 * dalpha2 + 3. * dalpha2 * dalpha3 * dalpha3 + 18. * dalpha2 * dalpha3 + 17. * dalpha2 + 5. * dalpha3 * dalpha3 
			//		+ 12. * dalpha3 + 7.)) / (dalpha1 * dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2 + dalpha3) * (dalpha1 + dalpha2 + dalpha3));
			//m_np[1] = ((dalpha1 + dalpha2 + 1.) * (2. * pow(dalpha1, 4) + 6. * pow(dalpha1, 3) * dalpha2 + 4. * pow(dalpha1, 3) * dalpha3 + 13. * pow(dalpha1, 3) + 6. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 8. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 29. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 * dalpha3 
			//		+ 20. * dalpha1 * dalpha1 * dalpha3 + 27. * dalpha1 * dalpha1 + 2. * dalpha1 * dalpha2 * dalpha2 * dalpha2 + 4. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 19. * dalpha1 * dalpha2 * dalpha2 + 2. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 26. * dalpha1 * dalpha2 * dalpha3 + 40. * dalpha1 * dalpha2 + 7. * dalpha1 * dalpha3 * dalpha3 
			//		+ 28. * dalpha1 * dalpha3 + 23. * dalpha1 + 3. * dalpha2 * dalpha2 * dalpha2 + 6. * dalpha2 * dalpha2 * dalpha3 + 13. * dalpha2 * dalpha2 + 3. * dalpha2 * dalpha3 * dalpha3 + 18. * dalpha2 * dalpha3 + 17. * dalpha2 + 5. * dalpha3 * dalpha3 + 12. * dalpha3 + 7.)) / (dalpha1 * dalpha1 * dalpha2 * dalpha2 * (dalpha2 + dalpha3) * (dalpha2 + dalpha3));
			//m_np[2] = ((dalpha1 + 1.) * (2. * pow(dalpha1, 4) + 6. * pow(dalpha1, 3) * dalpha2 + 4. * pow(dalpha1, 3) * dalpha3 + 13. * pow(dalpha1, 3) + 6. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 8. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 30. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 * dalpha3 
			//		+ 20. * dalpha1 * dalpha1 * dalpha3 + 27. * dalpha1 * dalpha1 + 2. * dalpha1 * dalpha2 * dalpha2 * dalpha2 + 4. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 21. * dalpha1 * dalpha2 * dalpha2 + 2. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 28. * dalpha1 * dalpha2 * dalpha3 + 42. * dalpha1 * dalpha2 + 7. * dalpha1 * dalpha3 * dalpha3 
			//		+ 28. * dalpha1 * dalpha3 + 23. * dalpha1 + 4. * dalpha2 * dalpha2 * dalpha2 + 8. * dalpha2 * dalpha2 * dalpha3 + 15. * dalpha2 * dalpha2 + 4. * dalpha2 * dalpha3 * dalpha3 + 20. * dalpha2 * dalpha3 + 18. * dalpha2 + 5. * dalpha3 * dalpha3 + 12. * dalpha3 + 7.)) / (dalpha2 * dalpha2 * dalpha3 * dalpha3 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2));
			//m_np[3] = ((dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (16. * dalpha1 + 11. * dalpha2 + 6. * dalpha3 + 15. * dalpha1 * dalpha2 + 8. * dalpha1 * dalpha3 + 4. * dalpha2 * dalpha3 + 2. * dalpha1 * dalpha2 * dalpha2 + 4. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 + 11. * dalpha1 * dalpha1 + 2. * dalpha1 * dalpha1 * dalpha1 
			//		+ 4. * dalpha2 * dalpha2 + 2. * dalpha1 * dalpha2 * dalpha3 + 7.)) / (dalpha3 * dalpha3 * (dalpha2 + dalpha3) * (dalpha2 + dalpha3) * (dalpha1 + dalpha2 + dalpha3) * (dalpha1 + dalpha2 + dalpha3));


			doublereal q1 = -(12. * (m_c3 - 1.) * (m_c3 - m_c2 + 1.) * m_gamma * m_gamma * m_gamma + 2. * (12. * m_c3 - 3. * m_c2 + 6. * m_c2 * m_c3 - 18. * m_c3 * m_c3 + 2.) 
							* m_gamma * m_gamma + 2. * m_c3 * (6. * m_c3 - 3. * m_c2 + 3. * m_c2 * m_c3 - 4.) * m_gamma - m_c2 * m_c3 * (3. * m_c3 - 2.)) 
							/ (12. * m_gamma * (m_c2 - 2. * m_gamma) * (m_c3 * m_c3 - 4. * m_c3 * m_gamma + 2. * m_gamma * m_gamma));
			doublereal q2 = (m_gamma * (12. * (1. - m_c3) * m_gamma * m_gamma * m_gamma + 6. * (m_c3 * m_c3 + 2. * m_c3 - 2.) * m_gamma * m_gamma - 
							2. * (6. * m_c3 * m_c3 - 3. * m_c3 - 1.) * m_gamma + m_c3 * (3. * m_c3 - 2.))) 
							/ (3. * m_c2 * (m_c2 - 2. * m_gamma) * (m_c3 * m_c3 - 4. * m_c3 * m_gamma + 2. * m_gamma * m_gamma));
			doublereal q3 = -(6. * m_gamma * m_gamma * m_gamma - 18. * m_gamma * m_gamma + 9. * m_gamma - 1.) / (3. * (m_c3 * m_c3 - 4. * m_c3 * m_gamma + 2. * m_gamma * m_gamma));
			doublereal q0 = 1. - m_gamma - q1 - q2 - q3;


			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.;
			m_a[IDX_A4][DIFFERENTIAL] = 1.;
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = q3 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = q2 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = q1 * dT;
			m_b[IDX_B4][DIFFERENTIAL] = q0 * dT;

			DEBUGCOUT("PredictForStageS(4)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
                                                                                        //<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "a4    = " << m_a[IDX_A4][DIFFERENTIAL] << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl
											<< "b4    = " << m_b[IDX_B4][DIFFERENTIAL] << std::endl);
			break;
		}

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	m_a[IDX_A1][ALGEBRAIC] = m_a[IDX_A1][DIFFERENTIAL];
	m_a[IDX_A2][ALGEBRAIC] = m_a[IDX_A2][DIFFERENTIAL];
	m_a[IDX_A3][ALGEBRAIC] = m_a[IDX_A3][DIFFERENTIAL];
	m_a[IDX_A4][ALGEBRAIC] = m_a[IDX_A4][DIFFERENTIAL];
	m_b[IDX_B0][ALGEBRAIC] = m_b[IDX_B0][DIFFERENTIAL];
	m_b[IDX_B1][ALGEBRAIC] = m_b[IDX_B1][DIFFERENTIAL];
	m_b[IDX_B2][ALGEBRAIC] = m_b[IDX_B2][DIFFERENTIAL];
	m_b[IDX_B3][ALGEBRAIC] = m_b[IDX_B3][DIFFERENTIAL];
	m_b[IDX_B4][ALGEBRAIC] = m_b[IDX_B4][DIFFERENTIAL];

	DEBUGCOUT("Algebraic coefficients:" << std::endl
                //<< "Asymptotic rho =" << m_dAlgebraicRho << " (ignored)" << std::endl 
		<< "a1    = " << m_a[IDX_A1][ALGEBRAIC] << std::endl
		<< "a2    = " << m_a[IDX_A2][ALGEBRAIC] << std::endl
		<< "a3    = " << m_a[IDX_A3][ALGEBRAIC] << std::endl
		<< "a4    = " << m_a[IDX_A4][ALGEBRAIC] << std::endl
		<< "b0    = " << m_b[IDX_B0][ALGEBRAIC] << std::endl
		<< "b1    = " << m_b[IDX_B1][ALGEBRAIC] << std::endl
		<< "b2    = " << m_b[IDX_B2][ALGEBRAIC] << std::endl
		<< "b3    = " << m_b[IDX_B3][ALGEBRAIC] << std::endl
		<< "b4    = " << m_b[IDX_B4][ALGEBRAIC] << std::endl);

	db0Differential = m_b[IDX_B0][DIFFERENTIAL];
	db0Algebraic = m_b[IDX_B0][ALGEBRAIC];
}

doublereal
DIRK43Solver::dPredDerForStageS(unsigned uStage,
                                const std::array<doublereal, 4>& dXm1mN,
                                const std::array<doublereal, 5>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];//constant prediction

	case 2:
		return m_mp[0]*dXm1mN[IDX_Xs1]
			+ m_mp[1]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs1]
			+ m_np[1]*dXP0mN[IDX_XPm1];   

	case 3:
		return m_mp[0]*dXm1mN[IDX_Xs2]
			+ m_mp[1]*dXm1mN[IDX_Xs1]
			+ m_mp[2]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs2]
			+ m_np[1]*dXP0mN[IDX_XPs1]
			+ m_np[2]*dXP0mN[IDX_XPm1]; 

	case 4:
		return m_mp[0]*dXm1mN[IDX_Xs3]
			+ m_mp[1]*dXm1mN[IDX_Xs2]
			+ m_mp[2]*dXm1mN[IDX_Xs1]
			+ m_mp[3]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs3]
			+ m_np[1]*dXP0mN[IDX_XPs2]
			+ m_np[2]*dXP0mN[IDX_XPs1]
			+ m_np[3]*dXP0mN[IDX_XPm1]; 

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
DIRK43Solver::dPredStateForStageS(unsigned uStage,
                                  const std::array<doublereal, 4>& dXm1mN,
                                  const std::array<doublereal, 5>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 4:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A4][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B4][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
DIRK43Solver::dPredDerAlgForStageS(unsigned uStage,
                                   const std::array<doublereal, 4>& dXm1mN,
                                   const std::array<doublereal, 5>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];

	case 2:
		return dXP0mN[IDX_XPs1]; 

	case 3:
		return dXP0mN[IDX_XPs2];

	case 4:
		return dXP0mN[IDX_XPs3];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
DIRK43Solver::dPredStateAlgForStageS(unsigned uStage,
                                     const std::array<doublereal, 4>& dXm1mN,
                                     const std::array<doublereal, 5>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 4:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A4][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B4][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

/* DIRK43Solver - end */

/* DIRK54Solver - begin */

DIRK54Solver::DIRK54Solver(const doublereal Tl,
	const doublereal dSolTl,
	const integer iMaxIt,
	//const DriveCaller* pRho,
	//const DriveCaller* pAlgRho,
	const bool bmod_res_test)
: tplStageNIntegrator<5>(iMaxIt, Tl, dSolTl, bmod_res_test)
{
	NO_OP;
}

DIRK54Solver::~DIRK54Solver(void)
{
	NO_OP;
}


void
DIRK54Solver::SetCoefForStageS(unsigned uStage,
	doublereal dT,
	doublereal dAlpha,
	enum StepChange /* NewStep */)
{
	switch (uStage) {
	case 1:
		m_gamma = 31. / 125.;

		ASSERT(pDM != NULL);
		pDM->SetTime(pDM->dGetTime() - (1. - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

		m_a[IDX_A1][DIFFERENTIAL] = 1.;
		m_a[IDX_A2][DIFFERENTIAL] = 0.;	// Unused
		m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
		m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
		m_a[IDX_A5][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B1][DIFFERENTIAL] = m_gamma * dT;
		m_b[IDX_B2][DIFFERENTIAL] = 0.;	// Unused
		m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused
		m_b[IDX_B5][DIFFERENTIAL] = 0.; // Unused

		DEBUGCOUT("PredictForStageS(1)" << std::endl
			<< "Alpha = " << dAlpha << std::endl
			<< "Differential coefficients:" << std::endl
                        //<< "Asymptotic rho =" << m_dRho << std::endl 
			<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
			<< "a2    = not needed" << std::endl
			<< "a3    = not needed" << std::endl
			<< "a4    = not needed" << std::endl
			<< "a5    = not needed" << std::endl
			<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
			<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
			<< "b2    = not needed" << std::endl
			<< "b3    = not needed" << std::endl
			<< "b4    = not needed" << std::endl
			<< "b5    = not needed" << std::endl);
		break;

	case 2:
		{
			m_c2 = 486119545908. / 3346201505189.;
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (m_c2 - 2. * m_gamma) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;//Unused
			//m_mp[3] = 0.;//Unused
			//m_mp[4] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;//Unused
			//m_np[3] = 0.;//Unused
			//m_np[4] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = (m_c2 - 2. * m_gamma) / (2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((m_c2 - 2. * m_gamma) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.; //Unused
			m_mp[3] = 0.; //Unused
			m_mp[4] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.; //Unused
			m_np[3] = 0.; //Unused
			m_np[4] = 0.; //Unused

			doublereal a21 = -360286518617. / 7014585480527.;
			doublereal a20 = m_c2 - m_gamma - a21;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 1.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.; // Unused
			m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
			m_a[IDX_A5][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = a21 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = a20 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B5][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(2)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
                                                                                        //<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = not needed" << std::endl
											<< "a4    = not needed" << std::endl
											<< "a5    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = not needed" << std::endl
											<< "b4    = not needed" << std::endl
											<< "b5    = not needed" << std::endl);
			break;
		}

	case 3:
		{
			m_c3 = 1043. / 1706.;
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (m_c3 - m_c2) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;//Unused
			//m_mp[4] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;
			//m_np[3] = 0.;//Unused
			//m_np[4] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = (m_c3 - m_c2) / (m_c2 - 2. * m_gamma);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((m_c3 - m_c2) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.;
			m_mp[3] = 0.; //Unused
			m_mp[4] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.;
			m_np[3] = 0.; //Unused
			m_np[4] = 0.; //Unused

			doublereal a31 = -506388693497. / 5937754990171.;
			doublereal a32 = 7149918333491. / 13390931526268.;
			doublereal a30 = m_c3 - m_gamma - a31 - a32;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 1.;
			m_a[IDX_A4][DIFFERENTIAL] = 0.; // Unused
			m_a[IDX_A5][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = a32 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = a31 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = a30 * dT;
			m_b[IDX_B4][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B5][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(3)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
                                                                                        //<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "a4    = not needed" << std::endl
											<< "a5    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl
											<< "b4    = not needed" << std::endl
											<< "b5    = not needed" << std::endl);
			break;
		}

	case 4:
		{
			m_c4 = 1361. / 1300.;
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (m_c4 - m_c3) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;
			//m_np[4] = 0.;//Unused
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;
			//m_np[3] = 0.;
			//m_np[4] = 0.;//Unused

			//second-order prediction
			doublereal dalpha = (m_c4 - m_c3) / (m_c3 - m_c2);
			m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((m_c4 - m_c3) * dT);
			m_mp[1] = -m_mp[0];
			m_mp[2] = 0.;
			m_mp[3] = 0.;
			m_mp[4] = 0.; //Unused
			m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			m_np[1] = dalpha * (2. + 3. * dalpha);
			m_np[2] = 0.;
			m_np[3] = 0.;
			m_np[4] = 0.; //Unused

			doublereal a41 = -7628305438933. / 11061539393788.;
			doublereal a42 = 21592626537567. / 14352247503901.;
			doublereal a43 = 11630056083252. / 17263101053231.;
			doublereal a40 = m_c4 - m_gamma - a41 - a42 - a43;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.;
			m_a[IDX_A4][DIFFERENTIAL] = 1.;
			m_a[IDX_A5][DIFFERENTIAL] = 0.; // Unused
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = a43 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = a42 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = a41 * dT;
			m_b[IDX_B4][DIFFERENTIAL] = a40 * dT;
			m_b[IDX_B5][DIFFERENTIAL] = 0.; // Unused

			DEBUGCOUT("PredictForStageS(4)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
                                                                                        //<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "a4    = " << m_a[IDX_A4][DIFFERENTIAL] << std::endl
											<< "a5    = not needed" << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl
											<< "b4    = " << m_b[IDX_B4][DIFFERENTIAL] << std::endl
											<< "b5    = not needed" << std::endl);
			break;
		}

	case 5:
		{
			ASSERT(pDM != NULL);
			pDM->SetTime(pDM->dGetTime() + (1. - m_c4) * dT, dT, pDM->pGetDrvHdl()->iGetStep());

			//constant prediction
			//m_mp[0] = 0.;
			//m_mp[1] = 0.;
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;
			//m_np[4] = 0.;
			//m_np[0] = 1.;
			//m_np[1] = 0.;
			//m_np[2] = 0.;
			//m_np[3] = 0.;
			//m_np[4] = 0.;

			//second-order prediction
			//doublereal dalpha = (1. - m_c4) / (m_c4 - m_c3);
			//m_mp[0] = -6. * dalpha * dalpha * (1. + dalpha) / ((1. - m_c4) * dT);
			//m_mp[1] = -m_mp[0];
			//m_mp[2] = 0.;
			//m_mp[3] = 0.;
			//m_mp[4] = 0.;
			//m_np[0] = 1. + 4. * dalpha + 3. * dalpha * dalpha;
			//m_np[1] = dalpha * (2. + 3. * dalpha);
			//m_np[2] = 0.;
			//m_np[3] = 0.;
			//m_np[4] = 0.;

			//fourth-order prediction
			doublereal dalpha1 = (1. - m_c4) / (m_c4 - m_c3);
			doublereal dalpha2 = (1. - m_c4) / (m_c3 - m_c2);
			m_mp[0] = -(2. * dalpha1 * dalpha1 * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
					+ dalpha1 * dalpha1 * (10. * dalpha2 * dalpha2 * dalpha2 + 25. * dalpha2 * dalpha2 + 13. * dalpha2) 
					+ dalpha1 * (20. * dalpha2 * dalpha2 * dalpha2 + 20. * dalpha2 * dalpha2) + 10. * dalpha2 * dalpha2 * dalpha2)) 
					/ ((dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (1. - m_c4) * dT);
			m_mp[1] = (2. * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
					+ dalpha1 * dalpha1 * (-5. * dalpha2 * dalpha2 * dalpha2 + dalpha2 * dalpha2 + 4. * dalpha2) 
					+ dalpha1 * (-7. * dalpha2 * dalpha2 * dalpha2 - dalpha2 * dalpha2) - 2. * dalpha2 * dalpha2 * dalpha2)) 
					/ (dalpha1 * (1. - m_c4) * dT);
			m_mp[2] = (2. * dalpha2 * dalpha2 * dalpha2 * dalpha2 * (1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (10. * dalpha2 + 10.) 
					+ dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 20. * dalpha2 + 5.) + dalpha1 * (7. * dalpha2 * dalpha2 + 7. * dalpha2) 
					+ 2. * dalpha2 * dalpha2)) / (dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (1. - m_c4) * dT);
			m_mp[3] = 0.;
			m_mp[4] = 0.;
			m_np[0] = ((1. + dalpha1) * (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
					+ dalpha1 * dalpha1 * (11. * dalpha2 * dalpha2 + 10. * dalpha2 + 1.) + dalpha1 * (7. * dalpha2 * dalpha2 + 2. * dalpha2) 
					+ dalpha2 * dalpha2)) / ((dalpha1 + dalpha2) * (dalpha1 + dalpha2));
			m_np[1] = (dalpha1 * dalpha1 * dalpha1 * (5. * dalpha2 * dalpha2 + 8. * dalpha2 + 3.) 
					+ dalpha1 * dalpha1 * (12. * dalpha2 * dalpha2 + 12. * dalpha2 + 2.) + dalpha1 * (9. * dalpha2 * dalpha2 + 4. * dalpha2) 
					+ 2. * dalpha2 * dalpha2) / dalpha1;
			m_np[2] = (dalpha2 * dalpha2 * dalpha2 * (1. + dalpha1) * (dalpha1 * dalpha1 * (5. * dalpha2 + 4.) 
					+ dalpha1 * (7. * dalpha2 + 2.) + 2. * dalpha2)) / (dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2));
			m_np[3] = 0.;
			m_np[4] = 0.;

			//sixth-order prediction
			//doublereal dalpha1 = (1. - m_c4) / (m_c4 - m_c3);
			//doublereal dalpha2 = (1. - m_c4) / (m_c3 - m_c2);
			//doublereal dalpha3 = (1. - m_c4) / (m_c2 - 2. * m_gamma);
			//m_mp[0] = -(2. * (dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (21. * pow(dalpha1, 5) + 77. * pow(dalpha1, 4) * dalpha2 + 49. * pow(dalpha1, 4) * dalpha3 + 63. * pow(dalpha1, 4) + 108. * pow(dalpha1, 3) * pow(dalpha2, 2) 
			//		+ 136. * pow(dalpha1, 3) * dalpha2 * dalpha3 + 182. * pow(dalpha1, 3) * dalpha2 + 38. * pow(dalpha1, 3) * pow(dalpha3, 2) + 112. * pow(dalpha1, 3) * dalpha3 + 63. * pow(dalpha1, 3) + 72. * pow(dalpha1, 2) * pow(dalpha2, 3) 
			//		+ 134. * dalpha1 * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 188. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 72. * dalpha1 * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 230. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 133. * dalpha1 * dalpha1 * dalpha2 
			//		+ 10. * dalpha1 * dalpha1 * dalpha3 * dalpha3 * dalpha3 + 62. * dalpha1 * dalpha1 * dalpha3 * dalpha3 + 77. * dalpha1 * dalpha1 * dalpha3 + 21. * dalpha1 * dalpha1 + 23. * dalpha1 * pow(dalpha2, 4) + 56. * dalpha1 * pow(dalpha2, 3) * dalpha3 
			//		+ 82. * dalpha1 * pow(dalpha2, 3) + 43. * dalpha1 * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 149. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 87. * dalpha1 * dalpha2 * dalpha2 + 10. * dalpha1 * dalpha2 * pow(dalpha3, 3) + 77. * dalpha1 * dalpha2 * dalpha3 * dalpha3 
			//		+ 101. * dalpha1 * dalpha2 * dalpha3 + 28. * dalpha1 * dalpha2 + 10. * dalpha1 * pow(dalpha3, 3) + 24. * dalpha1 * dalpha3 * dalpha3 + 14. * dalpha1 * dalpha3 + 3. * pow(dalpha2, 5) + 9. * pow(dalpha2, 4) * dalpha3 + 13. * pow(dalpha2, 4) + 9. * pow(dalpha2, 3) * dalpha3 * dalpha3 
			//		+ 31. * pow(dalpha2, 3) * dalpha3 + 17. * pow(dalpha2, 3) + 3. * dalpha2 * dalpha2 * pow(dalpha3, 3) + 23. * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 29. * dalpha2 * dalpha2 * dalpha3 + 7. * dalpha2 * dalpha2 + 5. * dalpha2 * pow(dalpha3, 3) + 12. * dalpha2 * pow(dalpha3, 2) + 7. * dalpha2 * dalpha3)) 
			//		/ (pow(dalpha1, 3) * pow(dalpha1 + dalpha2, 3) * pow(dalpha1 + dalpha2 + dalpha3, 3) * (1. - m_c4) * dT);
			//m_mp[1] = -(2. * (dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (4. * pow(dalpha1, 4) * dalpha2 + 2. * pow(dalpha1, 4) * dalpha3 + 9. * pow(dalpha1, 3) * pow(dalpha2, 2) + 11. * pow(dalpha1, 3) * dalpha2 * dalpha3 + 22. * pow(dalpha1, 3) * dalpha2 
			//		+ 4. * pow(dalpha1, 3) * pow(dalpha3, 2) + 11. * pow(dalpha1, 3) * dalpha3 + 3. * pow(dalpha1, 2) * pow(dalpha2, 3) + 7. * pow(dalpha1, 2) * pow(dalpha2, 2) * dalpha3 + 31. * pow(dalpha1, 2) * pow(dalpha2, 2) + 6. * pow(dalpha1, 2) * dalpha2 * pow(dalpha3, 2) 
			//		+ 40. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 32. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * pow(dalpha3, 3) + 16. * dalpha1 * dalpha1 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 - 5. * dalpha1 * pow(dalpha2, 4) 
			//		- 11. * dalpha1 * pow(dalpha2, 3) * dalpha3 - 4. * dalpha1 * pow(dalpha2, 3) - 7. * dalpha1 * dalpha2 * dalpha2 * dalpha3 * dalpha3 - 2. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 15. * dalpha1 * dalpha2 * dalpha2 - dalpha1 * dalpha2 * pow(dalpha3, 3) 
			//		+ 7. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 22. * dalpha1 * dalpha2 * dalpha3 + 14. * dalpha1 * dalpha2 + 5. * dalpha1 * pow(dalpha3, 3) + 12. * dalpha1 * dalpha3 * dalpha3 + 7. * dalpha1 * dalpha3 - 3. * pow(dalpha2, 5) - 9. * pow(dalpha2, 4) * dalpha3 - 13. * pow(dalpha2, 4) 
			//		- 9. * pow(dalpha2, 3) * dalpha3 * dalpha3 - 31. * pow(dalpha2, 3) * dalpha3 - 17. * pow(dalpha2, 3) - 3. * dalpha2 * dalpha2 * pow(dalpha3, 3) - 23. * dalpha2 * dalpha2 * dalpha3 * dalpha3 - 29. * dalpha2 * dalpha2 * dalpha3 - 7. * dalpha2 * dalpha2 - 5. * dalpha2 * pow(dalpha3, 3) 
			//		- 12. * dalpha2 * dalpha3 * dalpha3 - 7. * dalpha2 * dalpha3)) / (pow(dalpha1, 3) * pow(dalpha2, 3) * pow(dalpha2 + dalpha3, 3) * (1. - m_c4) * dT);
			//m_mp[2] = (2. * (dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (-2. * pow(dalpha1, 4) * dalpha2 + 2. * pow(dalpha1, 4) * dalpha3 - 6. * pow(dalpha1, 3) * pow(dalpha2, 2) + 5. * pow(dalpha1, 3) * dalpha2 * dalpha3 - 11. * pow(dalpha1, 3) * dalpha2 + 4. * pow(dalpha1, 3) * pow(dalpha3, 2) 
			//		+ 11. * pow(dalpha1, 3) * dalpha3 - 6. * dalpha1 * dalpha1 * pow(dalpha2, 3) + 4. * dalpha1 * dalpha1 * dalpha2 * dalpha2 * dalpha3 - 26. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 12. * dalpha1 * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 25. * dalpha1 * dalpha1 * dalpha2 * dalpha3 
			//		- 16. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 - 2. * dalpha1 * pow(dalpha2, 4) + dalpha1 * pow(dalpha2, 3) * dalpha3 - 19. * dalpha1 * pow(dalpha2, 3) 
			//		+ 8. * dalpha1 * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha2 * dalpha2 * dalpha3 - 27. * dalpha1 * dalpha2 * dalpha2 + 5. * dalpha1 * dalpha2 * pow(dalpha3, 3) + 40. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 34. * dalpha1 * dalpha2 * dalpha3 - 7. * dalpha1 * dalpha2 
			//		+ 5. * dalpha1 * pow(dalpha3, 3) + 12. * dalpha1 * dalpha3 * dalpha3 + 7. * dalpha1 * dalpha3 - 4. * pow(dalpha2, 4) + 2. * pow(dalpha2, 3) * dalpha3 - 11. * pow(dalpha2, 3) + 16. * dalpha2 * dalpha2 * dalpha3 * dalpha3 + 13. * dalpha2 * dalpha2 * dalpha3 - 7. * dalpha2 * dalpha2 
			//		+ 10. * dalpha2 * dalpha3 * dalpha3 * dalpha3 + 24. * dalpha2 * dalpha3 * dalpha3 + 14. * dalpha2 * dalpha3)) / (dalpha2 * dalpha2 * dalpha2 * dalpha3 * dalpha3 * dalpha3 * pow(dalpha1 + dalpha2, 3) * (1. - m_c4) * dT);
			//m_mp[3] = -m_mp[0] - m_mp[1] - m_mp[2];
			//m_mp[4] = 0.;
			//m_np[0] = ((dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (pow(dalpha1, 4) + 3. * pow(dalpha1, 3) * dalpha2 + 2. * pow(dalpha1, 3) * dalpha3 + 10. * pow(dalpha1, 3) + 3. * pow(dalpha1, 2) * pow(dalpha2, 2) + 4. * pow(dalpha1, 2) * dalpha2 * dalpha3 + 23. * pow(dalpha1, 2) * dalpha2 
			//		+ dalpha1 * dalpha1 * dalpha3 * dalpha3 + 16. * dalpha1 * dalpha1 * dalpha3 + 24. * dalpha1 * dalpha1 + dalpha1 * pow(dalpha2, 3) + 2. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 16. * dalpha1 * dalpha2 * dalpha2 + dalpha1 * dalpha2 * dalpha3 * dalpha3 + 22. * dalpha1 * dalpha2 * dalpha3 
			//		+ 37. * dalpha1 * dalpha2 + 6. * dalpha1 * dalpha3 * dalpha3 + 26. * dalpha1 * dalpha3 + 22. * dalpha1 + 3. * pow(dalpha2, 3) + 6. * dalpha2 * dalpha2 * dalpha3 + 13. * dalpha2 * dalpha2 + 3. * dalpha2 * dalpha3 * dalpha3 + 18. * dalpha2 * dalpha3 + 17. * dalpha2 + 5. * dalpha3 * dalpha3 
			//		+ 12. * dalpha3 + 7.)) / (dalpha1 * dalpha1 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2) * (dalpha1 + dalpha2 + dalpha3) * (dalpha1 + dalpha2 + dalpha3));
			//m_np[1] = ((dalpha1 + dalpha2 + 1.) * (2. * pow(dalpha1, 4) + 6. * pow(dalpha1, 3) * dalpha2 + 4. * pow(dalpha1, 3) * dalpha3 + 13. * pow(dalpha1, 3) + 6. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 8. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 29. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 * dalpha3 
			//		+ 20. * dalpha1 * dalpha1 * dalpha3 + 27. * dalpha1 * dalpha1 + 2. * dalpha1 * dalpha2 * dalpha2 * dalpha2 + 4. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 19. * dalpha1 * dalpha2 * dalpha2 + 2. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 26. * dalpha1 * dalpha2 * dalpha3 + 40. * dalpha1 * dalpha2 + 7. * dalpha1 * dalpha3 * dalpha3 
			//		+ 28. * dalpha1 * dalpha3 + 23. * dalpha1 + 3. * dalpha2 * dalpha2 * dalpha2 + 6. * dalpha2 * dalpha2 * dalpha3 + 13. * dalpha2 * dalpha2 + 3. * dalpha2 * dalpha3 * dalpha3 + 18. * dalpha2 * dalpha3 + 17. * dalpha2 + 5. * dalpha3 * dalpha3 + 12. * dalpha3 + 7.)) / (dalpha1 * dalpha1 * dalpha2 * dalpha2 * (dalpha2 + dalpha3) * (dalpha2 + dalpha3));
			//m_np[2] = ((dalpha1 + 1.) * (2. * pow(dalpha1, 4) + 6. * pow(dalpha1, 3) * dalpha2 + 4. * pow(dalpha1, 3) * dalpha3 + 13. * pow(dalpha1, 3) + 6. * dalpha1 * dalpha1 * dalpha2 * dalpha2 + 8. * dalpha1 * dalpha1 * dalpha2 * dalpha3 + 30. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 * dalpha3 
			//		+ 20. * dalpha1 * dalpha1 * dalpha3 + 27. * dalpha1 * dalpha1 + 2. * dalpha1 * dalpha2 * dalpha2 * dalpha2 + 4. * dalpha1 * dalpha2 * dalpha2 * dalpha3 + 21. * dalpha1 * dalpha2 * dalpha2 + 2. * dalpha1 * dalpha2 * dalpha3 * dalpha3 + 28. * dalpha1 * dalpha2 * dalpha3 + 42. * dalpha1 * dalpha2 + 7. * dalpha1 * dalpha3 * dalpha3 
			//		+ 28. * dalpha1 * dalpha3 + 23. * dalpha1 + 4. * dalpha2 * dalpha2 * dalpha2 + 8. * dalpha2 * dalpha2 * dalpha3 + 15. * dalpha2 * dalpha2 + 4. * dalpha2 * dalpha3 * dalpha3 + 20. * dalpha2 * dalpha3 + 18. * dalpha2 + 5. * dalpha3 * dalpha3 + 12. * dalpha3 + 7.)) / (dalpha2 * dalpha2 * dalpha3 * dalpha3 * (dalpha1 + dalpha2) * (dalpha1 + dalpha2));
			//m_np[3] = ((dalpha1 + 1.) * (dalpha1 + dalpha2 + 1.) * (16. * dalpha1 + 11. * dalpha2 + 6. * dalpha3 + 15. * dalpha1 * dalpha2 + 8. * dalpha1 * dalpha3 + 4. * dalpha2 * dalpha3 + 2. * dalpha1 * dalpha2 * dalpha2 + 4. * dalpha1 * dalpha1 * dalpha2 + 2. * dalpha1 * dalpha1 * dalpha3 + 11. * dalpha1 * dalpha1 + 2. * dalpha1 * dalpha1 * dalpha1 
			//		+ 4. * dalpha2 * dalpha2 + 2. * dalpha1 * dalpha2 * dalpha3 + 7.)) / (dalpha3 * dalpha3 * (dalpha2 + dalpha3) * (dalpha2 + dalpha3) * (dalpha1 + dalpha2 + dalpha3) * (dalpha1 + dalpha2 + dalpha3));
			//m_np[4] = 0.;


			doublereal q1 = -12917657251. / 5222094901039.;
			doublereal q2 = 5602338284630. / 15643096342197.;
			doublereal q3 = 9002339615474. / 18125249312447.;
			doublereal q4 = -2420307481369. / 24731958684496.;
			doublereal q0 = 1. - m_gamma - q1 - q2 - q3 - q4;

			m_a[IDX_A1][DIFFERENTIAL] = 0.;
			m_a[IDX_A2][DIFFERENTIAL] = 0.;
			m_a[IDX_A3][DIFFERENTIAL] = 0.;
			m_a[IDX_A4][DIFFERENTIAL] = 0.;
			m_a[IDX_A5][DIFFERENTIAL] = 1.;
			m_b[IDX_B0][DIFFERENTIAL] = m_gamma * dT;
			m_b[IDX_B1][DIFFERENTIAL] = q4 * dT;
			m_b[IDX_B2][DIFFERENTIAL] = q3 * dT;
			m_b[IDX_B3][DIFFERENTIAL] = q2 * dT;
			m_b[IDX_B4][DIFFERENTIAL] = q1 * dT;
			m_b[IDX_B5][DIFFERENTIAL] = q0 * dT;

			DEBUGCOUT("PredictForStageS(5)" << std::endl
											<< "Alpha = " << dAlpha << std::endl
											<< "Differential coefficients:" << std::endl
                                                                                        //<< "Asymptotic rho =" << m_dRho << std::endl
											<< "a1    = " << m_a[IDX_A1][DIFFERENTIAL] << std::endl
											<< "a2    = " << m_a[IDX_A2][DIFFERENTIAL] << std::endl
											<< "a3    = " << m_a[IDX_A3][DIFFERENTIAL] << std::endl
											<< "a4    = " << m_a[IDX_A4][DIFFERENTIAL] << std::endl
											<< "a5    = " << m_a[IDX_A5][DIFFERENTIAL] << std::endl
											<< "b0    = " << m_b[IDX_B0][DIFFERENTIAL] << std::endl
											<< "b1    = " << m_b[IDX_B1][DIFFERENTIAL] << std::endl
											<< "b2    = " << m_b[IDX_B2][DIFFERENTIAL] << std::endl
											<< "b3    = " << m_b[IDX_B3][DIFFERENTIAL] << std::endl
											<< "b4    = " << m_b[IDX_B4][DIFFERENTIAL] << std::endl
											<< "b5    = " << m_b[IDX_B5][DIFFERENTIAL] << std::endl);
			break;
		}

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	m_a[IDX_A1][ALGEBRAIC] = m_a[IDX_A1][DIFFERENTIAL];
	m_a[IDX_A2][ALGEBRAIC] = m_a[IDX_A2][DIFFERENTIAL];
	m_a[IDX_A3][ALGEBRAIC] = m_a[IDX_A3][DIFFERENTIAL];
	m_a[IDX_A4][ALGEBRAIC] = m_a[IDX_A4][DIFFERENTIAL];
	m_a[IDX_A5][ALGEBRAIC] = m_a[IDX_A5][DIFFERENTIAL];
	m_b[IDX_B0][ALGEBRAIC] = m_b[IDX_B0][DIFFERENTIAL];
	m_b[IDX_B1][ALGEBRAIC] = m_b[IDX_B1][DIFFERENTIAL];
	m_b[IDX_B2][ALGEBRAIC] = m_b[IDX_B2][DIFFERENTIAL];
	m_b[IDX_B3][ALGEBRAIC] = m_b[IDX_B3][DIFFERENTIAL];
	m_b[IDX_B4][ALGEBRAIC] = m_b[IDX_B4][DIFFERENTIAL];
	m_b[IDX_B5][ALGEBRAIC] = m_b[IDX_B5][DIFFERENTIAL];

	DEBUGCOUT("Algebraic coefficients:" << std::endl
                //<< "Asymptotic rho =" << m_dAlgebraicRho << " (ignored)" << std::endl 
		<< "a1    = " << m_a[IDX_A1][ALGEBRAIC] << std::endl
		<< "a2    = " << m_a[IDX_A2][ALGEBRAIC] << std::endl
		<< "a3    = " << m_a[IDX_A3][ALGEBRAIC] << std::endl
		<< "a4    = " << m_a[IDX_A4][ALGEBRAIC] << std::endl
		<< "a5    = " << m_a[IDX_A5][ALGEBRAIC] << std::endl
		<< "b0    = " << m_b[IDX_B0][ALGEBRAIC] << std::endl
		<< "b1    = " << m_b[IDX_B1][ALGEBRAIC] << std::endl
		<< "b2    = " << m_b[IDX_B2][ALGEBRAIC] << std::endl
		<< "b3    = " << m_b[IDX_B3][ALGEBRAIC] << std::endl
		<< "b4    = " << m_b[IDX_B4][ALGEBRAIC] << std::endl
		<< "b5    = " << m_b[IDX_B5][ALGEBRAIC] << std::endl);

	db0Differential = m_b[IDX_B0][DIFFERENTIAL];
	db0Algebraic = m_b[IDX_B0][ALGEBRAIC];
}

doublereal
DIRK54Solver::dPredDerForStageS(unsigned uStage,
                                const std::array<doublereal, 5>& dXm1mN,
                                const std::array<doublereal, 6>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];

	case 2:
		return m_mp[0]*dXm1mN[IDX_Xs1]
			+ m_mp[1]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs1]
			+ m_np[1]*dXP0mN[IDX_XPm1]; 

	case 3:
		return m_mp[0]*dXm1mN[IDX_Xs2]
			+ m_mp[1]*dXm1mN[IDX_Xs1]
			+ m_mp[2]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs2]
			+ m_np[1]*dXP0mN[IDX_XPs1]
			+ m_np[2]*dXP0mN[IDX_XPm1];

	case 4:
		return m_mp[0]*dXm1mN[IDX_Xs3]
			+ m_mp[1]*dXm1mN[IDX_Xs2]
			+ m_mp[2]*dXm1mN[IDX_Xs1]
			+ m_mp[3]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs3]
			+ m_np[1]*dXP0mN[IDX_XPs2]
			+ m_np[2]*dXP0mN[IDX_XPs1]
			+ m_np[3]*dXP0mN[IDX_XPm1];

	case 5:
		return m_mp[0]*dXm1mN[IDX_Xs4]
			+ m_mp[1]*dXm1mN[IDX_Xs3]
			+ m_mp[2]*dXm1mN[IDX_Xs2]
			+ m_mp[3]*dXm1mN[IDX_Xs1]
			+ m_mp[4]*dXm1mN[IDX_Xm1]
			+ m_np[0]*dXP0mN[IDX_XPs4]
			+ m_np[1]*dXP0mN[IDX_XPs3]
			+ m_np[2]*dXP0mN[IDX_XPs2]
			+ m_np[3]*dXP0mN[IDX_XPs1]
			+ m_np[4]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
DIRK54Solver::dPredStateForStageS(unsigned uStage,
                                  const std::array<doublereal, 5>& dXm1mN,
                                  const std::array<doublereal, 6>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 4:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A4][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XPs4]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B4][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	case 5:
		return m_a[IDX_A1][DIFFERENTIAL]*dXm1mN[IDX_Xs4]
			+ m_a[IDX_A2][DIFFERENTIAL]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A3][DIFFERENTIAL]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A4][DIFFERENTIAL]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A5][DIFFERENTIAL]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][DIFFERENTIAL]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][DIFFERENTIAL]*dXP0mN[IDX_XPs4]
			+ m_b[IDX_B2][DIFFERENTIAL]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B3][DIFFERENTIAL]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B4][DIFFERENTIAL]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B5][DIFFERENTIAL]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
DIRK54Solver::dPredDerAlgForStageS(unsigned uStage,
                                   const std::array<doublereal, 5>& dXm1mN,
                                   const std::array<doublereal, 6>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return dXP0mN[IDX_XPm1];

	case 2:
		return dXP0mN[IDX_XPs1]; 

	case 3:
		return dXP0mN[IDX_XPs2];

	case 4:
		return dXP0mN[IDX_XPs3];

	case 5:
		return dXP0mN[IDX_XPs4];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

doublereal
DIRK54Solver::dPredStateAlgForStageS(unsigned uStage,
                                     const std::array<doublereal, 5>& dXm1mN,
                                     const std::array<doublereal, 6>& dXP0mN) const
{
	switch (uStage) {
	case 1:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 2:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 3:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 4:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A4][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XPs4]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B4][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	case 5:
		return m_a[IDX_A1][ALGEBRAIC]*dXm1mN[IDX_Xs4]
			+ m_a[IDX_A2][ALGEBRAIC]*dXm1mN[IDX_Xs3]
			+ m_a[IDX_A3][ALGEBRAIC]*dXm1mN[IDX_Xs2]
			+ m_a[IDX_A4][ALGEBRAIC]*dXm1mN[IDX_Xs1]
			+ m_a[IDX_A5][ALGEBRAIC]*dXm1mN[IDX_Xm1]
			+ m_b[IDX_B0][ALGEBRAIC]*dXP0mN[IDX_XP0]
			+ m_b[IDX_B1][ALGEBRAIC]*dXP0mN[IDX_XPs4]
			+ m_b[IDX_B2][ALGEBRAIC]*dXP0mN[IDX_XPs3]
			+ m_b[IDX_B3][ALGEBRAIC]*dXP0mN[IDX_XPs2]
			+ m_b[IDX_B4][ALGEBRAIC]*dXP0mN[IDX_XPs1]
			+ m_b[IDX_B5][ALGEBRAIC]*dXP0mN[IDX_XPm1];

	default:
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
}

/* DIRK54Solver - end */
