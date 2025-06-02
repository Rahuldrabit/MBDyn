/* $Header$ */
/* 
 * MBDyn (C) is a multibody analysis code. 
 * http://www.mbdyn.org
 *
 * Copyright (C) 1996-2023
 *
 * Pierangelo Masarati  <pierangelo.masarati@polimi.it>
 * Paolo Mantegazza     <paolo.mantegazza@polimi.it>
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

// This goes first in every *.c,*.cc file
#include "mbconfig.h"           

#include "dataman.h"
#include "userelem.h"

#include <iostream>
#include <limits>
#include <cfloat>
#include <limits>

#include "module-MFtire.h"

class MFtire : public UserDefinedElem
{
private:

	// wheel nodes
	const StructNode *m_pHub;
	const StructNode *m_pRim;

	// wheel axle direction (local reference frame)
	Vec3 m_WheelAxle;

	// wheel geometry
	doublereal m_UnloadedRadius;
	doublereal m_EffectiveRollingRadius;
	doublereal m_VerticalStiffness;

	// friction data (magic formula coefficients)
	doublereal m_Dx;
	doublereal m_Cx;
	doublereal m_Bx;
	doublereal m_Ex;
	doublereal m_Dy;
	doublereal m_Cy;
	doublereal m_By;
	doublereal m_Ey;

	// unit vectors
	Vec3 m_n; // normal to ground (0,0,1) for now
	Vec3 m_s; // wheel axis
	Vec3 m_t; // lateral direction
	Vec3 m_l; // longitudinal direction

	Vec3 m_w; // wheel centre position
	Vec3 m_wp; // wheel centre velocity
	Vec3 m_o_R; // rim angular velocity

	doublereal m_vx; // contact patch longitudinal velocity
	doublereal m_vy; // contact patch lateral velocity
	doublereal m_vs; // contact patch slip velocity

	// practical slips
	doublereal m_kappaL;
	doublereal m_alphaL;
	doublereal m_kappaH;
	doublereal m_alphaH;		
	doublereal m_kappa;
	doublereal m_alpha;

	// low velocity thresholds
	doublereal m_vxLow;
	doublereal m_vxDmp;

	// output data
	doublereal m_Fx;
	doublereal m_Fy;
	doublereal m_Fz;
	Vec3 m_F; // tire forces at wheel centre
	Vec3 m_M; // tire moments at wheel centre
	doublereal m_loadedRadius;
	doublereal m_deltaL;
	doublereal m_muX;
	doublereal m_muY;

public:
	MFtire(unsigned uLabel, const DofOwner *pDO, DataManager* pDM, MBDynParser& HP);
	virtual ~MFtire(void);

	virtual void Output(OutputHandler& OH) const;
	virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;
	
	VariableSubMatrixHandler& 
	AssJac(VariableSubMatrixHandler& WorkMat,
		doublereal dCoef, 
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);
	
	SubVectorHandler& 
	AssRes(SubVectorHandler& WorkVec,
		doublereal dCoef,
		const VectorHandler& XCurr, 
		const VectorHandler& XPrimeCurr);
	
	unsigned int iGetNumPrivData(void) const;
	int iGetNumConnectedNodes(void) const;
	void GetConnectedNodes(std::vector<const Node *>& connectedNodes) const;
	void SetValue(DataManager *pDM, VectorHandler& X, VectorHandler& XP, SimulationEntity::Hints *ph);
	std::ostream& Restart(std::ostream& out) const;
	virtual unsigned int iGetInitialNumDof(void) const;
	virtual void InitialWorkSpaceDim(integer* piNumRows, integer* piNumCols) const;
   	
	VariableSubMatrixHandler& InitialAssJac(VariableSubMatrixHandler& WorkMat, const VectorHandler& XCurr);
	SubVectorHandler& InitialAssRes(SubVectorHandler& WorkVec, const VectorHandler& XCurr);
};

MFtire::MFtire(unsigned uLabel, const DofOwner *pDO, DataManager* pDM, MBDynParser& HP) : UserDefinedElem(uLabel, pDO)
{
	// help
	if (HP.IsKeyWord("help")) {
		silent_cout(
            "\n"
            "Module:	MFtire\n"
            "Andrea Fontana	<andrea8.fontana@mail.polimi.it>\n"
            "Organization:	Politecnico di Milano\n"
            "	http://www.aero.polimi.it\n"
            "\n"
            "	All rights reserved\n"
            "\n"
            "2 structural nodes required:\n"
            "	- Wheel Hub\n"
            "	- Wheel Rim\n"
            "\n"
            "Note:\n"
            "   - The Wheel Hub and the Wheel Rim structural nodes must be connected\n"
            "	  by a joint that allows relative rotations only about one axis\n"
            "   - The center of the wheel is assumed coincident with\n"
            "	  the position of the Wheel Hub and Rim structural nodes\n"
            "   - The model assumes a flat  infinite plane as road\n"
            "	- Forces, moments and slips are defined according to ISO convention.\n"
            "\n"
            "   - Input:\n"
            "		<wheel structural node label>, \n"
            "		<wheel axle direction>, \n"
            "		<unloaded wheel radius>, \n"
            "		<effective rolling radius>, \n"
            "		<vertical stiffness> ,\n"
            "		<Dx>, <Cx>, <Bx>, <Ex>, \n"
            "       <Dy>, <Cy>, <By>, <Ey>, \n"
            "		[low speed, <low speed threshold>, <low speed damping>]\n"
            "\n"
            "   - Output:\n"
            "		1)		element label\n"
            "		2-4)	tire force in global reference frame\n"
            "		5-7)	tire couple in global reference frame\n"
            "		8)		slip ratio\n"
            "		9)		slip angle\n"
            "		10)		loaded radius\n"
		);

		if (!HP.IsArg()) {
			//Exit quietly if nothing else is provided
			throw NoErr(MBDYN_EXCEPT_ARGS);
		}
	}

	// read wheel nodes
	m_pHub = pDM->ReadNode<StructNode, Node::STRUCTURAL>(HP);
	m_pRim = pDM->ReadNode<StructNode, Node::STRUCTURAL>(HP);

	// read wheel axle
	m_WheelAxle = HP.GetVec3();
	m_WheelAxle /= m_WheelAxle.Norm();

	try {
		// wheel geometry
		m_UnloadedRadius = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));
		m_EffectiveRollingRadius = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));
		m_VerticalStiffness = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));
	} catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
		silent_cerr("error: invalid tire parameters" << e.Get() << " (must be positive) " << e.what() << " for MFtire at line " << HP.GetLineData() << std::endl);
		throw e;
	}

	// Magic Formula coefficients
	try {
		m_Dx = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));
		m_Cx = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));
		m_Bx = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));
		m_Ex = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

		// note: Dy must be negative
		m_Dy = HP.GetReal(0., HighParser::range_lt<doublereal>(0.));
		m_Cy = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));
		m_By = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));
		m_Ey = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));
	} catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
		silent_cerr("error: invalid Pacejka coefficient " << e.Get() << " (all coefficients must be positive except Dy, that must be negative) " << e.what() << " for MFtire at line " << HP.GetLineData() << std::endl);
		throw e;
	}

	if (HP.IsKeyWord("low" "speed")) {
		try {
			m_vxLow = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));
			m_vxDmp = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));
		} catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
			silent_cerr("error: invalid low speed coefficients" << e.Get() << " (must be positive) " << e.what() << " for MFtire at line " << HP.GetLineData() << std::endl);
			throw e;
		}
	} else {
		m_vxLow = 1;
		m_vxDmp = 0.1;
		silent_cout("Warning, no low speed coefficients provided; using vxLow = 1 and vxDmp = 0.1 at line " << HP.GetLineData() << std::endl);
	}

	SetOutputFlag(pDM->fReadOutput(HP, Elem::LOADABLE));
	
	std::ostream& out = pDM->GetLogFile();
	out << "MFtire: " << uLabel
	<< " " << m_pHub->GetLabel()		//node label
	<< " " << m_pRim->GetLabel()		//node label
	<< std::endl;
}

MFtire::~MFtire(void)
{
	NO_OP;
}

void MFtire::Output(OutputHandler& OH) const
{
	if (bToBeOutput()) {
		std::ostream& out = OH.Loadable();
		out << std::setw(8) << GetLabel()		// 1:	label
		<< " " << m_Fx							// 2:	force
		<< " " << m_Fy							// 3:	force
		<< " " << m_Fz							// 4:	force
		<< " " << m_kappa						// 8: 	long. slip
		<< " " << m_alpha						// 9: 	lat. slip
		<< " " << m_muX							// 10:	longitudinal friction coefficient
		<< " " << m_muY							// 11:	lateral friction coefficient
		<< std::endl;
	}
}

void MFtire::WorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
	// 3 force and 3 moment equilibrium of rim
	*piNumRows = 6;
	// 3 linear velocity, 3 angular velocity of hub, 3 angular velocity of rim
	*piNumCols = 9;
}

VariableSubMatrixHandler& 
MFtire::AssJac(
	VariableSubMatrixHandler& WorkMat,
	doublereal dCoef, 
	const VectorHandler& XCurr,
	const VectorHandler& XPrimeCurr)
{  

	//
	if (0) {
		WorkMat.SetNullMatrix();
		return WorkMat;
	}

	FullSubMatrixHandler& WM = WorkMat.SetFull();

	// resize matrix
	integer iNumRows = 0;
	integer iNumCols = 0;
	WorkSpaceDim(&iNumRows, &iNumCols);
	WM.ResizeReset(iNumRows, iNumCols);

	integer iHubFirstPosIndex = m_pHub->iGetFirstPositionIndex();
	integer iRimFirstPosIndex = m_pRim->iGetFirstPositionIndex();
	integer iRimFirstMomIndex = m_pRim->iGetFirstMomentumIndex();

	// first to sixth rows: rim forces and moments
	// first to sixth columns: hub position and rotation parameters
	for(int iCnt = 1; iCnt <= 6; iCnt++){
		WM.PutRowIndex(iCnt, iRimFirstMomIndex + iCnt);
		WM.PutColIndex(iCnt, iHubFirstPosIndex + iCnt);
	}
	
	// seventh to ninth columns: rim rotation parameters
	for (int iCnt = 1; iCnt <= 3; iCnt++) {
		WM.PutColIndex(6 + iCnt, iRimFirstPosIndex + 3 + iCnt);
	}

	// TODO: deltaL < 0 --> return WM

	// vectors derivatives
	Mat3x3 J_s(MatCross, -m_s);
	doublereal d((m_s.Cross(m_n)).Norm());	// already tested > tol in AssRes
	Mat3x3 J_l((Mat3x3(MatCross, m_n/(-d)) + m_l.Tens()*Mat3x3(MatCross, m_n/d))*J_s);
	Mat3x3 J_t(Mat3x3(MatCross, m_n)*J_l);

	Vec3 J_vx_g(J_l.MulTV(m_wp));
	Vec3 J_vx_wp(m_l);
	Vec3 J_vy_g(J_t.MulTV(m_wp));
	Vec3 J_vy_wp(m_t);
	Vec3 J_vs_g(J_l.MulTV(m_o_R.Cross(m_n*(m_EffectiveRollingRadius))));
	Vec3 J_vs_o_R(m_n.Cross(m_l*(m_EffectiveRollingRadius)));

	Vec3 J_aL_g(J_vy_g);
	Vec3 J_aL_wp(J_vy_wp);
	Vec3 J_kL_g(J_vs_g - J_vx_g);
	Vec3 J_kL_wp(-J_vx_wp);
	Vec3 J_kL_o_R(J_vs_o_R);

	Vec3 J_k_g;
	Vec3 J_k_wp; 
	Vec3 J_k_o_R;
	Vec3 J_a_g;
	Vec3 J_a_wp; 

	if (m_vx == 0) {
		J_k_g = J_kL_g;
		J_k_wp = J_kL_wp;
		J_k_o_R = J_kL_o_R;
		J_a_g = J_aL_g;
		J_a_wp = J_aL_wp;
	} else {
		Vec3 J_aH_g((J_vy_g*m_vx + J_vx_g*(-m_vy))*(1/(pow(m_vx,2) + pow(m_vy,2))));
		Vec3 J_aH_wp((J_vy_wp*m_vx + J_vx_wp*(-m_vy))*(1/(pow(m_vx,2) + pow(m_vy,2))));
		Vec3 J_kH_g(J_vs_g*(1/m_vx) + J_vx_g*(-m_vs/pow(m_vx,2)));
		Vec3 J_kH_wp(J_vx_wp*(-m_vs/pow(m_vx,2)));
		Vec3 J_kH_o_R(J_vs_o_R*(1/m_vx));

		// transition factor between slipLow and slipHigh, and its derivative
		doublereal x = 0.5*(1 + tanh((fabs(m_vx) - m_vxLow)/m_vxDmp));
		doublereal dx = m_vx/(2*m_vxDmp*fabs(m_vx)*pow(cosh((fabs(m_vx) - m_vxLow)/m_vxDmp), 2));

		J_k_g = J_vx_g*((m_kappaH-m_kappaL)*dx) + J_kH_g*x + J_kL_g*(1-x);
		J_k_wp = J_vx_wp*((m_kappaH-m_kappaL)*dx) + J_kH_wp*x + J_kL_wp*(1-x);
		J_k_o_R = J_kH_o_R*x + J_kL_o_R*(1-x);
		J_a_g = J_vx_g*((m_alphaH-m_alphaL)*dx) + J_aH_g*x + J_aL_g*(1-x);
		J_a_wp = J_vx_wp*((m_alphaH-m_alphaL)*dx) + J_aH_wp*x + J_aL_wp*(1-x);
	}

	// derivative of friction coefficient function
	doublereal mudX = -m_Cx*m_Dx*(m_Ex*(m_Bx - m_Bx/(pow(m_Bx*m_kappa,2) + 1)) - m_Bx)*cos(m_Cx*atan(m_Ex*(m_Bx*m_kappa - atan(m_Bx*m_kappa)) - m_Bx*m_kappa))/(pow(m_Ex*(m_Bx*m_kappa - atan(m_Bx*m_kappa)) - m_Bx*m_kappa, 2) + 1);
	doublereal mudY = -m_Cy*m_Dy*(m_Ey*(m_By - m_By/(pow(m_By*m_alpha,2) + 1)) - m_By)*cos(m_Cy*atan(m_Ey*(m_By*m_alpha - atan(m_By*m_alpha)) - m_By*m_alpha))/(pow(m_Ey*(m_By*m_alpha - atan(m_By*m_alpha)) - m_By*m_alpha, 2) + 1);

	// forces and moments derivatives
	Vec3 J_Fx_w(m_n*(-m_muX*m_VerticalStiffness));
	Vec3 J_Fx_g(J_k_g*(m_Fz*mudX));
	Vec3 J_Fx_wp(J_k_wp*(m_Fz*mudX));
	Vec3 J_Fx_o_R(J_k_o_R*(m_Fz*mudX));

	Vec3 J_Fy_w(m_n*(-m_muY*m_VerticalStiffness));
	Vec3 J_Fy_g(J_a_g*(m_Fz*mudY));
	Vec3 J_Fy_wp(J_a_wp*(m_Fz*mudY));

	Mat3x3 J_F_w(m_n.Tens()*(-m_VerticalStiffness) + m_l.Tens(J_Fx_w) + m_t.Tens(J_Fy_w));
	Mat3x3 J_F_g(m_l.Tens(J_Fx_g) + J_l*m_Fx + m_t.Tens(J_Fy_g) + J_t*m_Fy);
	Mat3x3 J_F_wp(m_l.Tens(J_Fx_wp) + m_t.Tens(J_Fy_wp));
	Mat3x3 J_F_o_R(m_l.Tens(J_Fx_o_R));

	Mat3x3 J_M_w((m_F.Cross(m_n)).Tens(m_n) + Mat3x3(MatCross, -m_n*m_loadedRadius)*J_F_w);
	Mat3x3 J_M_g((Mat3x3(MatCross, -m_n*m_loadedRadius)*J_F_g));
	Mat3x3 J_M_wp((Mat3x3(MatCross, -m_n*m_loadedRadius)*J_F_wp));
	Mat3x3 J_M_o_R((Mat3x3(MatCross, -m_n*m_loadedRadius)*J_F_o_R));

	WM.Sub(1, 1, J_F_wp + J_F_w*dCoef);
	WM.Sub(1, 4, J_F_g*dCoef);
	WM.Sub(1, 7, J_F_o_R*(Eye3 - Mat3x3(MatCross, m_o_R*dCoef)));
	WM.Sub(4, 1, J_M_wp + J_M_w*dCoef);
	WM.Sub(4, 4, J_M_g*dCoef);
	WM.Sub(4, 7, J_M_o_R*(Eye3 - Mat3x3(MatCross, m_o_R*dCoef)));

	//				wpH							gpH									gpR
	//	DF	(J_F_w*dcoef + J_F_wp)			(J_F_g*dCoef)		(J_F_o_R*(Eye3 - Mat3x3(MatCross, m_o_R)*dCoef))
	//	DM	(J_F_w*dcoef + J_F_wp)			(J_F_g*dCoef)		(J_F_o_R*(Eye3 - Mat3x3(MatCross, m_o_R)*dCoef))

	// nelle colonne 1-3 ci va (Delta wp + h*b0*Delta w) di hub
	// nelle colonne 4-6 ci va (Delta gp + h*b0*Delta g) di hub
	// nelle colonne 7-9 ci va (Delta gp + h*b0*Delta g) di rim
	
	// Log for debugging, if needed

	// const doublereal* _J_a_g = J_a_g.pGetVec();
	// const doublereal* _J_a_wp = J_a_wp.pGetVec();
	// const doublereal* _J_k_g = J_k_g.pGetVec();
	// const doublereal* _J_k_wp = J_k_wp.pGetVec();
	// const doublereal* _J_k_o_R = J_k_o_R.pGetVec();
	// const doublereal* _J_Fx_w = J_Fx_w.pGetVec();
	// const doublereal* _J_Fx_g = J_Fx_g.pGetVec();
	// const doublereal* _J_Fx_wp = J_Fx_wp.pGetVec();
	// const doublereal* _J_Fx_o_R = J_Fx_o_R.pGetVec();
	// const doublereal* _J_Fy_w = J_Fy_w.pGetVec();
	// const doublereal* _J_Fy_g = J_Fy_g.pGetVec();
	// const doublereal* _J_Fy_wp = J_Fy_wp.pGetVec();
	// const doublereal* _J_F_w  = J_F_w.pGetMat();
	// const doublereal* _J_F_g  = J_F_g.pGetMat();
	// const doublereal* _J_F_wp  = J_F_wp.pGetMat();
	// const doublereal* _J_F_o_R  = J_F_o_R.pGetMat();
	// const doublereal* _J_M_w  = J_M_w.pGetMat();
	// const doublereal* _J_M_g  = J_M_g.pGetMat();
	// const doublereal* _J_M_wp  = J_M_wp.pGetMat();
	// const doublereal* _J_M_o_R  = J_M_o_R.pGetMat();
	// const doublereal* _s  = m_s.pGetVec();
	// const doublereal* _t  = m_t.pGetVec();
	// const doublereal* _l  = m_l.pGetVec();
	// const doublereal* _w  = m_w.pGetVec();
	// const doublereal* _wp  = m_wp.pGetVec();
	// const doublereal* _o_R  = m_o_R.pGetVec();
	// silent_cout("Iteration\n");
	// silent_cout("vx = " << m_vx << "\n");
	// silent_cout("vy = " << m_vy << "\n");
	// silent_cout("vs = " << m_vs << "\n");
	// silent_cout("w = [" << _w[0] << ", " << _w[1] << ", " << _w[2] << "]\n");
	// silent_cout("wp = [" << _wp[0] << ", " << _wp[1] << ", " << _wp[2] << "]\n");
	// silent_cout("o_R = [" << _o_R[0] << ", " << _o_R[1] << ", " << _o_R[2] << "]\n");
	// silent_cout("s = [" << _s[0] << ", " << _s[1] << ", " << _s[2] << "]\n");
	// silent_cout("t = [" << _t[0] << ", " << _t[1] << ", " << _t[2] << "]\n");
	// silent_cout("l = [" << _l[0] << ", " << _l[1] << ", " << _l[2] << "]\n");
	// silent_cout("J_a_g = [" << _J_a_g[0] << ", " << _J_a_g[1] << ", " << _J_a_g[2] << "]\n");
	// silent_cout("J_a_wp = [" << _J_a_wp[0] << ", " << _J_a_wp[1] << ", " << _J_a_wp[2] << "]\n");
	// silent_cout("J_k_g = [" << _J_k_g[0] << ", " << _J_k_g[1] << ", " << _J_k_g[2] << "]\n");
	// silent_cout("J_k_wp = [" << _J_k_wp[0] << ", " << _J_k_wp[1] << ", " << _J_k_wp[2] << "]\n");
	// silent_cout("J_k_o_R = [" << _J_k_o_R[0] << ", " << _J_k_o_R[1] << ", " << _J_k_o_R[2] << "]\n");
	// silent_cout("mudX = " << mudX << "\n");
	// silent_cout("mudX = " << mudY << "\n");
	// silent_cout("J_Fx_w = [" << _J_Fx_w[0] << ", " << _J_Fx_w[1] << ", " << _J_Fx_w[2] << "]\n");
	// silent_cout("J_Fx_g = [" << _J_Fx_g[0] << ", " << _J_Fx_g[1] << ", " << _J_Fx_g[2] << "]\n");
	// silent_cout("J_Fx_wp = [" << _J_Fx_wp[0] << ", " << _J_Fx_wp[1] << ", " << _J_Fx_wp[2] << "]\n");
	// silent_cout("J_Fx_o_R = [" << _J_Fx_o_R[0] << ", " << _J_Fx_o_R[1] << ", " << _J_Fx_o_R[2] << "]\n");
	// silent_cout("J_Fy_w = [" << _J_Fy_w[0] << ", " << _J_Fy_w[1] << ", " << _J_Fy_w[2] << "]\n");
	// silent_cout("J_Fy_g = [" << _J_Fy_g[0] << ", " << _J_Fy_g[1] << ", " << _J_Fy_g[2] << "]\n");
	// silent_cout("J_Fy_wp = [" << _J_Fy_wp[0] << ", " << _J_Fy_wp[1] << ", " << _J_Fy_wp[2] << "]\n");
	
	// silent_cout("J_F_w = [" << _J_F_w[0] << ", " << _J_F_w[3] << ", " << _J_F_w[6] << "]\n");
	// silent_cout("J_F_w = [" << _J_F_w[1] << ", " << _J_F_w[4] << ", " << _J_F_w[7] << "]\n");
	// silent_cout("J_F_w = [" << _J_F_w[2] << ", " << _J_F_w[5] << ", " << _J_F_w[8] << "]\n");

	// silent_cout("J_F_g = [" << _J_F_g[0] << ", " << _J_F_g[3] << ", " << _J_F_g[6] << "]\n");
	// silent_cout("J_F_g = [" << _J_F_g[1] << ", " << _J_F_g[4] << ", " << _J_F_g[7] << "]\n");
	// silent_cout("J_F_g = [" << _J_F_g[2] << ", " << _J_F_g[5] << ", " << _J_F_g[8] << "]\n");

	// silent_cout("J_F_wp = [" << _J_F_wp[0] << ", " << _J_F_wp[3] << ", " << _J_F_wp[6] << "]\n");
	// silent_cout("J_F_wp = [" << _J_F_wp[1] << ", " << _J_F_wp[4] << ", " << _J_F_wp[7] << "]\n");
	// silent_cout("J_F_wp = [" << _J_F_wp[2] << ", " << _J_F_wp[5] << ", " << _J_F_wp[8] << "]\n");

	// silent_cout("J_F_o_R = [" << _J_F_o_R[0] << ", " << _J_F_o_R[3] << ", " << _J_F_o_R[6] << "]\n");
	// silent_cout("J_F_o_R = [" << _J_F_o_R[1] << ", " << _J_F_o_R[4] << ", " << _J_F_o_R[7] << "]\n");
	// silent_cout("J_F_o_R = [" << _J_F_o_R[2] << ", " << _J_F_o_R[5] << ", " << _J_F_o_R[8] << "]\n");

	// silent_cout("J_M_w = ["   << _J_M_w[0]   << ", " << _J_M_w[3]   << ", " << _J_M_w[6] << "]\n");
	// silent_cout("J_M_w = ["   << _J_M_w[1]   << ", " << _J_M_w[4]   << ", " << _J_M_w[7] << "]\n");
	// silent_cout("J_M_w = ["   << _J_M_w[2]   << ", " << _J_M_w[5]   << ", " << _J_M_w[8] << "]\n");
	// silent_cout("J_M_g = ["   << _J_M_g[0]   << ", " << _J_M_g[3]   << ", " << _J_M_g[6] << "]\n");
	// silent_cout("J_M_g = ["   << _J_M_g[1]   << ", " << _J_M_g[4]   << ", " << _J_M_g[7] << "]\n");
	// silent_cout("J_M_g = ["   << _J_M_g[2]   << ", " << _J_M_g[5]   << ", " << _J_M_g[8] << "]\n");
	// silent_cout("J_M_wp = ["  << _J_M_wp[0]  << ", " << _J_M_wp[3]  << ", " << _J_M_wp[6] << "]\n");
	// silent_cout("J_M_wp = ["  << _J_M_wp[1]  << ", " << _J_M_wp[4]  << ", " << _J_M_wp[7] << "]\n");
	// silent_cout("J_M_wp = ["  << _J_M_wp[2]  << ", " << _J_M_wp[5]  << ", " << _J_M_wp[8] << "]\n");
	// silent_cout("J_M_o_R = [" << _J_M_o_R[0] << ", " << _J_M_o_R[3] << ", " << _J_M_o_R[6] << "]\n");
	// silent_cout("J_M_o_R = [" << _J_M_o_R[1] << ", " << _J_M_o_R[4] << ", " << _J_M_o_R[7] << "]\n");
	// silent_cout("J_M_o_R = [" << _J_M_o_R[2] << ", " << _J_M_o_R[5] << ", " << _J_M_o_R[8] << "]\n");
	
	return WorkMat;
}

SubVectorHandler& 
MFtire::AssRes(
	SubVectorHandler& WorkVec,
	doublereal dCoef,
	const VectorHandler& XCurr, 
	const VectorHandler& XPrimeCurr)
{
	// ground orientation in the absolute frame
	m_n = Vec3(0., 0., 1.);

	// Wheel spin axis in global reference frame
	m_s = (m_pHub->GetRCurr())*m_WheelAxle;

	// "forward" direction: axle cross normal to ground
	m_l = m_s.Cross(m_n);
	doublereal d = m_l.Dot();
	if (d < std::numeric_limits<doublereal>::epsilon()) {
		silent_cerr("MFtire(" << GetLabel() << "): "
			"wheel axle is (nearly) orthogonal "
			"to the ground" << std::endl);
		throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
	m_l /= sqrt(d);

	// "lateral" direction: normal to ground cross forward
	m_t = m_n.Cross(m_l);

	// wheel hub position and velocity
	m_w = m_pHub->GetXCurr();
	m_wp = m_pHub->GetVCurr();

	// wheel rim angular velocity
	m_o_R = m_pRim->GetWCurr();

	// contact when dDeltaL > 0
	m_loadedRadius = m_w.Dot(m_n);
	m_deltaL = m_UnloadedRadius - m_loadedRadius;

	// resize residual
	integer iNumRows = 0;
	integer iNumCols = 0;
	WorkSpaceDim(&iNumRows, &iNumCols);
   
	WorkVec.ResizeReset(iNumRows);

	integer iRimFirstMomIndex = m_pRim->iGetFirstMomentumIndex();

	// equations indexes
	for (int iCnt = 1; iCnt <= 6; iCnt++) {
		WorkVec.PutRowIndex(iCnt, iRimFirstMomIndex + iCnt);
	}
	
	// relative speed between wheel and ground at contact point
	m_vx = m_l.Dot(m_wp);
	m_vy = m_t.Dot(m_wp);
	m_vs = m_l.Dot(m_o_R.Cross(m_n*m_EffectiveRollingRadius));

	// transition between slipLow and slipHigh to avoid division by zero
	doublereal x = 0.5*(1 + tanh((fabs(m_vx) - m_vxLow)/m_vxDmp));

	m_kappaL = m_vs - m_vx;
	m_kappaH = m_vx == 0 ? m_kappaL : m_vs/m_vx - 1;

	m_alphaL = m_vy;
	m_alphaH = m_vx == 0 ? m_alphaL : atan2(m_vy, m_vx);

	m_kappa = x*m_kappaH + (1-x)*m_kappaL;
	m_alpha = x*m_alphaH + (1-x)*m_alphaL;

	// vertical force
	m_Fz = m_VerticalStiffness*m_deltaL;

	// longitudinal friction coefficient
	m_muX = m_Dx*sin(m_Cx*atan(m_Bx*m_kappa - m_Ex*(m_Bx*m_kappa - atan(m_Bx*m_kappa))));

	// lateral friction coefficient (the minus sign is due to ISO convention: negative force for positive slip)
	m_muY = m_Dy*sin(m_Cy*atan(m_By*m_alpha - m_Ey*(m_By*m_alpha - atan(m_By*m_alpha))));

	// lateral and longitudinal force
	m_Fx = m_Fz*m_muX;
	m_Fy = m_Fz*m_muY;

	// Total tire force
	m_F = m_n*m_Fz + m_l*m_Fx + m_t*m_Fy;

	// Total tire moment
	m_M = (m_n*(-m_loadedRadius)).Cross(m_F);

	WorkVec.Add(1, m_F);
	WorkVec.Add(4, m_M);

	return WorkVec;
}

unsigned int
MFtire::iGetNumPrivData(void) const
{
	return 0;
}

int
MFtire::iGetNumConnectedNodes(void) const
{
	// wheel + ground
	return 1;
}

void
MFtire::GetConnectedNodes(std::vector<const Node *>& connectedNodes) const
{
	// wheel + ground
	connectedNodes.resize(2);
	connectedNodes[0] = m_pRim;
	connectedNodes[1] = m_pHub;
}

void 
MFtire::SetValue(DataManager *pDM,
	VectorHandler& X, VectorHandler& XP,
	SimulationEntity::Hints *ph)
{
   	NO_OP;
}

std::ostream& 
MFtire::Restart(std::ostream& out) const
{
   	return out << "# not implemented yet" << std::endl;
}

unsigned
MFtire::iGetInitialNumDof(void) const
{
	return 0;
}

void 
MFtire::InitialWorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
	*piNumRows = 0;
	*piNumCols = 0;
}

VariableSubMatrixHandler&
MFtire::InitialAssJac(VariableSubMatrixHandler& WorkMat, 
	const VectorHandler& XCurr)
{
	WorkMat.SetNullMatrix();
	return WorkMat;
}

SubVectorHandler& 
MFtire::InitialAssRes(SubVectorHandler& WorkVec, const VectorHandler& XCurr)
{
	WorkVec.Resize(0);
	return WorkVec;
}

bool
MFtire_set(void)
{
	UserDefinedElemRead *rf = new UDERead<MFtire>;

	if (!SetUDE("MFtire", rf)) {
		delete rf;
		return false;
	}

	return true;
}

//#ifndef STATIC_MODULES
extern "C" int
module_init(const char *module_name, void *pdm, void *php)
{
	if (!MFtire_set()) {
		silent_cerr("MFtire: "
			"module_init(" << module_name << ") "
			"failed" << std::endl);
		return -1;
	}
	return 0;
}
//#endif // ! STATIC_MODULES

