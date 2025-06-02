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

#ifndef GENEL__H
#define GENEL__H

#include "genel.h"
#include "drive.h"
#include "dataman.h"
#include "constltp.h"

/* GenelClamp - begin */

class GenelClamp : public Genel, public DriveOwner {
protected:
	ScalarDof SD;
	doublereal dRct;

public:
	GenelClamp(unsigned int uLabel,
		const DofOwner* pDO, const DriveCaller* pDC,
		const ScalarDof& sd, flag fOutput);

	virtual ~GenelClamp(void);

	virtual unsigned int iGetNumDof(void) const;

	/* esegue operazioni sui dof di proprieta' dell'elemento */
	virtual DofOrder::Order GetDofType(unsigned int i) const;

	/* esegue operazioni sui dof di proprieta' dell'elemento */
	virtual DofOrder::Order GetEqType(unsigned int i) const;

	/* Scrive il contributo dell'elemento al file di restart */
	virtual std::ostream& Restart(std::ostream& out) const;
        virtual void Restart(RestartData& oData, RestartData::RestartAction eAction) override;

	/* Tipo di Genel */
	virtual Genel::Type GetGenelType(void) const;

	/* Dimensioni del workspace */
	virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;

	void Output(OutputHandler& OH ) const;

	/* assemblaggio jacobiano */
	virtual VariableSubMatrixHandler&
	AssJac(VariableSubMatrixHandler& WorkMat,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	/* assemblaggio residuo */
	virtual SubVectorHandler&
	AssRes(SubVectorHandler& WorkVec,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& /* XPrimeCurr */ );

	void SetValue(DataManager *pDM,
		VectorHandler& X, VectorHandler& XP,
		SimulationEntity::Hints *ph = 0);

	/* *******PER IL SOLUTORE PARALLELO******** */
	/* Fornisce il tipo e la label dei nodi che sono connessi all'elemento
	 * utile per l'assemblaggio della matrice di connessione fra i dofs */
	virtual void GetConnectedNodes(std::vector<const Node *>& connectedNodes) const;
	/* ************************************************ */

	/* returns the dimension of the component */
	const virtual OutputHandler::Dimensions GetEquationDimension(integer index) const;

	/* describes the dimension of components of equation */
    virtual std::ostream& DescribeEq(std::ostream& out,
		  const char *prefix = "",
		  bool bInitial = false) const;
};

/* GenelClamp - end */


/* GenelDistance - begin */

class GenelDistance : public Genel, public DriveOwner {
protected:
	ScalarDof SD1;
	ScalarDof SD2;
	doublereal dRct;

public:
	GenelDistance(unsigned int uLabel, const DofOwner* pDO,
		const DriveCaller* pDC,
		const ScalarDof& sd1, const ScalarDof& sd2, flag fOutput);

	virtual ~GenelDistance(void);

	virtual unsigned int iGetNumDof(void) const;

	/* esegue operazioni sui dof di proprieta' dell'elemento */
	virtual DofOrder::Order GetDofType(unsigned int i ) const;

	/* Scrive il contributo dell'elemento al file di restart */
	virtual std::ostream& Restart(std::ostream& out) const;

	/* Tipo di Genel */
	virtual Genel::Type GetGenelType(void) const;

	/* Dimensioni del workspace */
	virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;

	void Output(OutputHandler& OH) const;

	/* assemblaggio jacobiano */
	virtual VariableSubMatrixHandler&
	AssJac(VariableSubMatrixHandler& WorkMat,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	/* assemblaggio residuo */
	virtual SubVectorHandler&
	AssRes(SubVectorHandler& WorkVec,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	void SetValue(DataManager *pDM,
		VectorHandler& X, VectorHandler& XP,
		SimulationEntity::Hints *ph = 0);

	/* *******PER IL SOLUTORE PARALLELO******** */
	/* Fornisce il tipo e la label dei nodi che sono connessi all'elemento
	 * utile per l'assemblaggio della matrice di connessione fra i dofs */
	virtual void GetConnectedNodes(std::vector<const Node *>& connectedNodes) const;
	/* ************************************************ */

	/* returns the dimension of the component */
	const virtual OutputHandler::Dimensions GetEquationDimension(integer index) const;

	/* describes the dimension of components of equation */
    virtual std::ostream& DescribeEq(std::ostream& out,
		  const char *prefix = "",
		  bool bInitial = false) const;
};

/* GenelDistance - end */


/* GenelSpring - begin */

class GenelSpring
: public Genel {
protected:
	ScalarDof SD1;
	ScalarDof SD2;

	doublereal dVal;
	ConstitutiveLaw1D * pDC;

public:
	GenelSpring(unsigned int uLabel, const DofOwner* pDO,
		ConstitutiveLaw1D*const pCL,
		const ScalarDof& sd1, const ScalarDof& sd2, flag fOutput);

	virtual ~GenelSpring(void);

	/* Scrive il contributo dell'elemento al file di restart */
	virtual std::ostream& Restart(std::ostream& out) const;

	virtual void AfterConvergence(const VectorHandler& X,
		const VectorHandler& XP);

	/* Tipo di Genel */
	virtual Genel::Type GetGenelType(void) const;

	/* Dimensioni del workspace */
	virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;

	/* assemblaggio jacobiano */
	virtual VariableSubMatrixHandler&
	AssJac(VariableSubMatrixHandler& WorkMat,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	/* assemblaggio residuo */
	virtual SubVectorHandler&
	AssRes(SubVectorHandler& WorkVec,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	/* *******PER IL SOLUTORE PARALLELO******** */
	/* Fornisce il tipo e la label dei nodi che sono connessi all'elemento
	 * utile per l'assemblaggio della matrice di connessione fra i dofs */
	virtual void GetConnectedNodes(std::vector<const Node *>& connectedNodes) const;
	/* ************************************************ */

	/* returns the dimension of the component */
	const virtual OutputHandler::Dimensions GetEquationDimension(integer index) const;

	/* describes the dimension of components of equation */
    virtual std::ostream& DescribeEq(std::ostream& out,
		  const char *prefix = "",
		  bool bInitial = false) const;
};

/* GenelSpring - end */


/* GenelSpringSupport - begin */

class GenelSpringSupport
: public Genel {
protected:
	ScalarDof SD;
	doublereal dVal;
	doublereal dInitVal;
	ConstitutiveLaw1D * pDC;
public:
	GenelSpringSupport(unsigned int uLabel, const DofOwner* pDO,
		ConstitutiveLaw1D*const pCL,
		const ScalarDof& sd, doublereal X0, flag fOutput);

	virtual ~GenelSpringSupport(void);

	/* Scrive il contributo dell'elemento al file di restart */
	virtual std::ostream& Restart(std::ostream& out) const;

	virtual void AfterConvergence(const VectorHandler& X,
		const VectorHandler& XP);

	/* Tipo di Genel */
	virtual Genel::Type GetGenelType(void) const;

	/* Dimensioni del workspace */
	virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;

	/* assemblaggio jacobiano */
	virtual VariableSubMatrixHandler&
	AssJac(VariableSubMatrixHandler& WorkMat,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	/* assemblaggio residuo */
	virtual SubVectorHandler&
	AssRes(SubVectorHandler& WorkVec,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	/* *******PER IL SOLUTORE PARALLELO******** */
	/* Fornisce il tipo e la label dei nodi che sono connessi all'elemento
	 * utile per l'assemblaggio della matrice di connessione fra i dofs */
	virtual void GetConnectedNodes(std::vector<const Node *>& connectedNodes) const;
	/* ************************************************ */

	virtual unsigned int iGetNumPrivData(void) const;
	virtual unsigned int iGetPrivDataIdx(const char *s) const;
	virtual doublereal dGetPrivData(unsigned int i) const;

	/* returns the dimension of the component */
	const virtual OutputHandler::Dimensions GetEquationDimension(integer index) const;

	/* describes the dimension of components of equation */
    virtual std::ostream& DescribeEq(std::ostream& out,
		  const char *prefix = "",
		  bool bInitial = false) const;
};

/* GenelSpringSupport - end */


/* GenelCrossSpringSupport - begin */

class GenelCrossSpringSupport
: public Genel {
protected:
	ScalarDof SDRow;
	ScalarDof SDCol;
	doublereal dVal;
	ConstitutiveLaw1D * pDC;

public:
	GenelCrossSpringSupport(unsigned int uLabel, const DofOwner* pDO,
		ConstitutiveLaw1D*const pCL,
		const ScalarDof& sdrow,
		const ScalarDof& sdcol,
		flag fOutput);

	virtual ~GenelCrossSpringSupport(void);

	/* Scrive il contributo dell'elemento al file di restart */
	virtual std::ostream& Restart(std::ostream& out) const;

	virtual void AfterConvergence(const VectorHandler& X,
		const VectorHandler& XP);

	/* Tipo di Genel */
	virtual Genel::Type GetGenelType(void) const;

	/* Dimensioni del workspace */
	virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;

	/* assemblaggio jacobiano */
	virtual VariableSubMatrixHandler&
	AssJac(VariableSubMatrixHandler& WorkMat,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	/* assemblaggio residuo */
	virtual SubVectorHandler&
	AssRes(SubVectorHandler& WorkVec,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	/* *******PER IL SOLUTORE PARALLELO******** */
	/* Fornisce il tipo e la label dei nodi che sono connessi all'elemento
	 * utile per l'assemblaggio della matrice di connessione fra i dofs */
	virtual void GetConnectedNodes(std::vector<const Node *>& connectedNodes) const;
	/* ************************************************ */

	/* returns the dimension of the component */
	const virtual OutputHandler::Dimensions GetEquationDimension(integer index) const;

	/* describes the dimension of components of equation */
    virtual std::ostream& DescribeEq(std::ostream& out,
		  const char *prefix = "",
		  bool bInitial = false) const;
};

/* GenelCrossSpringSupport - end */


/* GenelCrossSpringDamperSupport - begin */

class GenelCrossSpringDamperSupport
: public Genel {
protected:
	ScalarDof SDRow;
	ScalarDof SDCol;
	doublereal dVal;
	doublereal dValPrime;
	ConstitutiveLaw1D * pDC;

public:
	GenelCrossSpringDamperSupport(unsigned int uLabel, const DofOwner* pDO,
		ConstitutiveLaw1D*const pCL,
		const ScalarDof& sdrow,
		const ScalarDof& sdcol,
		flag fOutput);

	virtual ~GenelCrossSpringDamperSupport(void);

	/* Scrive il contributo dell'elemento al file di restart */
	virtual std::ostream& Restart(std::ostream& out) const;

	virtual void AfterConvergence(const VectorHandler& X,
		const VectorHandler& XP);

	/* Tipo di Genel */
	virtual Genel::Type GetGenelType(void) const;

	/* Dimensioni del workspace */
	virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;

	/* assemblaggio jacobiano */
	virtual VariableSubMatrixHandler&
	AssJac(VariableSubMatrixHandler& WorkMat,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	/* assemblaggio residuo */
	virtual SubVectorHandler&
	AssRes(SubVectorHandler& WorkVec,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	/* *******PER IL SOLUTORE PARALLELO******** */
	/* Fornisce il tipo e la label dei nodi che sono connessi all'elemento
	 * utile per l'assemblaggio della matrice di connessione fra i dofs */
	virtual void GetConnectedNodes(std::vector<const Node *>& connectedNodes) const;
	/* ************************************************ */

	/* returns the dimension of the component */
	const virtual OutputHandler::Dimensions GetEquationDimension(integer index) const;

	/* describes the dimension of components of equation */
    virtual std::ostream& DescribeEq(std::ostream& out,
		  const char *prefix = "",
		  bool bInitial = false) const;
};

/* GenelCrossSpringDamperSupport - end */


/* GenelSpringDamperSupport - begin */

class GenelSpringDamperSupport
: public Genel {
protected:
	ScalarDof SD;
	doublereal dVal;
	doublereal dInitVal;
	doublereal dValPrime;
	ConstitutiveLaw1D * pDC;

public:
	GenelSpringDamperSupport(unsigned int uLabel, const DofOwner* pDO,
		ConstitutiveLaw1D*const pCL,
		const ScalarDof& sd, doublereal X0, flag fOutput);

	virtual ~GenelSpringDamperSupport(void);

	/* Scrive il contributo dell'elemento al file di restart */
	virtual std::ostream& Restart(std::ostream& out) const;

	virtual void AfterConvergence(const VectorHandler& X,
		const VectorHandler& XP);

	/* Tipo di Genel */
	virtual Genel::Type GetGenelType(void) const;

	/* Dimensioni del workspace */
	virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;

	/* assemblaggio jacobiano */
	virtual VariableSubMatrixHandler&
	AssJac(VariableSubMatrixHandler& WorkMat,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	/* assemblaggio residuo */
	virtual SubVectorHandler& AssRes(SubVectorHandler& WorkVec,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	/* *******PER IL SOLUTORE PARALLELO******** */
	/* Fornisce il tipo e la label dei nodi che sono connessi all'elemento
	 * utile per l'assemblaggio della matrice di connessione fra i dofs */
	virtual void GetConnectedNodes(std::vector<const Node *>& connectedNodes) const;
	/* ************************************************ */

	/* returns the dimension of the component */
	const virtual OutputHandler::Dimensions GetEquationDimension(integer index) const;

	/* describes the dimension of components of equation */
    virtual std::ostream& DescribeEq(std::ostream& out,
		  const char *prefix = "",
		  bool bInitial = false) const;
};

/* GenelSpringDamperSupport - end */


/* GenelMass - begin */

class GenelMass : public Genel, public DriveOwner {
protected:
	ScalarDof SD;

public:
	GenelMass(unsigned int uLabel, const DofOwner* pDO, const DriveCaller* pDC,
		const ScalarDof& sd, flag fOutput);

	virtual ~GenelMass(void);

	virtual unsigned int iGetNumDof(void) const;

	/* esegue operazioni sui dof di proprieta' dell'elemento */
	virtual DofOrder::Order GetDofType(unsigned int i ) const;

	/* Scrive il contributo dell'elemento al file di restart */
	virtual std::ostream& Restart(std::ostream& out) const;

	/* Tipo di Genel */
	virtual Genel::Type GetGenelType(void) const;

	/* Dimensioni del workspace */
	virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;

	/* assemblaggio jacobiano */
	virtual VariableSubMatrixHandler&
	AssJac(VariableSubMatrixHandler& WorkMat,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	/* assemblaggio residuo */
	virtual SubVectorHandler&
	AssRes(SubVectorHandler& WorkVec,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	void SetValue(DataManager *pDM,
		VectorHandler& X, VectorHandler& XP,
		SimulationEntity::Hints *ph = 0);

	/* *******PER IL SOLUTORE PARALLELO******** */
	/* Fornisce il tipo e la label dei nodi che sono connessi all'elemento
	 * utile per l'assemblaggio della matrice di connessione fra i dofs */
	virtual void GetConnectedNodes(std::vector<const Node *>& connectedNodes) const;
	/* ************************************************ */

	/* returns the dimension of the component */
	const virtual OutputHandler::Dimensions GetEquationDimension(integer index) const;

	/* describes the dimension of components of equation */
    virtual std::ostream& DescribeEq(std::ostream& out,
		  const char *prefix = "",
		  bool bInitial = false) const;
};

/* GenelMass - end */

#endif // GENEL__H

