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

#ifndef AEROELEM_H
#define AEROELEM_H

/* Elementi aerodinamici bidimensionali */
/* 2D aerodynamic elements */

#include "aerodyn.h"
#include "rotor.h"
#include "beam.h"
#include "beam2.h"
#include "aerodata.h"

#include "gauss.h"
#ifdef USE_AEROD2_F
#include "aerod2.h"
#endif // USE_AEROD2_F
#include "shape.h"

class AerodynamicOutput {
public:
	enum eOutput {
		AEROD_OUT_NONE		= 0x0U,

		AEROD_OUT_STD		= (ToBeOutput::OUTPUT_PRIVATE << 0),
		AEROD_OUT_PGAUSS	= (ToBeOutput::OUTPUT_PRIVATE << 1),
		AEROD_OUT_NODE		= (ToBeOutput::OUTPUT_PRIVATE << 2),

		AEROD_OUT_MASK		= (AEROD_OUT_STD | AEROD_OUT_PGAUSS | AEROD_OUT_NODE)
	};

	// output mask
	enum {
		OUTPUT_NONE = 0x0U,

		OUTPUT_GP_X = (ToBeOutput::OUTPUT_PRIVATE << 4),
		OUTPUT_GP_R = (ToBeOutput::OUTPUT_PRIVATE << 5),
		OUTPUT_GP_V = (ToBeOutput::OUTPUT_PRIVATE << 6),
		OUTPUT_GP_W = (ToBeOutput::OUTPUT_PRIVATE << 7),
		OUTPUT_GP_CONFIGURATION = (OUTPUT_GP_X | OUTPUT_GP_R | OUTPUT_GP_V | OUTPUT_GP_W),

		OUTPUT_GP_F = (ToBeOutput::OUTPUT_PRIVATE << 8),
		OUTPUT_GP_M = (ToBeOutput::OUTPUT_PRIVATE << 9),

		OUTPUT_GP_FORCES = (OUTPUT_GP_F | OUTPUT_GP_M),

		OUTPUT_DEFAULT = (OUTPUT_GP_F | OUTPUT_GP_M),

		OUTPUT_GP_ALL = (ToBeOutput::OUTPUT_PRIVATE_MASK & (~AEROD_OUT_MASK))
	};

protected:
	flag m_eOutput;

	// output flags
	OrientationDescription od;

	// specific for NetCDF output and AEROD_OUT_PGAUSS
	typedef struct {
		doublereal alpha;
		Vec3 f;

#ifdef USE_NETCDF
		Vec3 X;
		Mat3x3 R;
		Vec3 V;
		Vec3 W;
		Vec3 F;
		Vec3 M;

		MBDynNcVar Var_alpha, Var_gamma, Var_Mach,
			Var_cl, Var_cd, Var_cm,
			Var_X, Var_Phi, Var_V, Var_W, Var_F, Var_M;
#endif /* USE_NETCDF */
	} Aero_output;

	std::vector<Aero_output> OutputData;
	std::vector<Aero_output>::iterator OutputIter;

public:
	AerodynamicOutput(flag f, int iNP,
		OrientationDescription ood);
	~AerodynamicOutput(void);

	void SetOutputFlag(flag f, int iNP);
	void ResetIterator(void);
	void SetData(const Vec3& v, const doublereal* pd,
		const Vec3& X, const Mat3x3& R, const Vec3& V, const Vec3& W,
		const Vec3& F, const Vec3& M);

	AerodynamicOutput::eOutput GetOutput(void) const;
	bool IsOutput(void) const;
	bool IsSTD(void) const;
	bool IsPGAUSS(void) const;
	bool IsNODE(void) const;
};

/* AerodynamicBody - begin */

/* Elemento aerodinamico associato ad un nodo.
 * Fisicamente corrisponde ad un rettangoloide, associato ad un nodo fisico,
 * dal quale riceve il movimento ed al quale trasmette le forze.
 * Inoltre puo' essere associato ad un elemento di rotore che gli modifica
 * la velocita' in funzione di qualche modello di velocita' indotta
 * ed al quale fornisce le forze generate per il calcolo della trazione totale
 * ecc.
 *
 * Single-node aerodynamic element. 
 * Physically, it represents a quadrilateral surface, associated to a node, from 
 * which it received the motion and to which it transmits forces.
 * Furthermore, it can be associated to a rotor element that modifies the velocity
 * according to some induced velocity model and to which it returns the generated
 * forces, for the computation of the total thrust, etc...
 */

/* Lo modifico in modo che possieda un driver relativo allo svergolamento,
 * che consenta di inserire un contributo di svergolamento costante su
 * tutta l'apertura dell'elemento in modo da consentire la modellazione di
 * una superficie mobile equivalente 
 *
 * The element is modified so that a twist drive can be associated with it,
 * in order to add contribution to the twist which is constant along the span,
 * thus allowing to model an equivalent control surface
 *
 */

template <unsigned iNN>
class Aerodynamic2DElem :
	public AerodynamicElem,
	public InitialAssemblyElem,
	public DriveOwner,
	public AerodynamicOutput
{
protected:
	AeroData* aerodata;
	InducedVelocityElem* pIndVel;
	bool bPassiveInducedVelocity;

	const ShapeOwner Chord;		/* corda / chord*/
	const ShapeOwner ForcePoint;	/* punto di app. della forza / force application point (1/4 c) */
	const ShapeOwner VelocityPoint;	/* punto di app. b.c. / boundary conditions application point (3/4 c) */
	const ShapeOwner Twist;		/* svergolamento / twist*/
	const ShapeOwner TipLoss;	/* tip loss model */

	GaussDataIterator GDI;	/* Iteratore sui punti di Gauss / iterator over Gauss points*/
	std::vector<outa_t> OUTA;

	// used for Jacobian with internal states
	Mat3xN vx, wx, fq, cq;

	bool bJacobian;		/* Compute Jacobian matrix contribution */

	/*
	 * overload della funzione di ToBeOutput();
	 * serve per allocare il vettore dei dati di output se il flag
	 * viene settato dopo la costruzione
	 *
	 * overload of the ToBeOutput(); method
	 * to allocate the output data vector in case the flag is set
	 * after construction
	 *
	 */
	virtual void SetOutputFlag(flag f = flag(1));
#ifdef USE_NETCDF
	void Output_NetCDF(OutputHandler& OH) const;
#endif // USE_NETCDF
	void AddForce_int(const StructNode *pN, const Vec3& F, const Vec3& M, const Vec3& X) const;
	void AddSectionalForce_int(unsigned uPnt,
		const Vec3& F, const Vec3& M, doublereal dW,
		const Vec3& X, const Mat3x3& R,
		const Vec3& V, const Vec3& W) const;

public:
	Aerodynamic2DElem(unsigned int uLabel,
		const DofOwner *pDO,
		InducedVelocityElem* pR, bool bPassive,
		const Shape* pC, const Shape* pF,
		const Shape* pV, const Shape* pT,
		const Shape* pTL,
		integer iN, AeroData* a,
		const DriveCaller* pDC,
		bool bUseJacobian,
		OrientationDescription ood,
		flag fOut);
	virtual ~Aerodynamic2DElem(void);

	/* Tipo dell'elemento (usato per debug ecc.) */
	/* Element type (useful for debug, etc...) */
	virtual Elem::Type GetElemType(void) const {
		return Elem::AERODYNAMIC;
	};

	/* inherited from SimulationEntity */
	virtual unsigned int iGetNumDof(void) const;
	virtual std::ostream& DescribeDof(std::ostream& out,
		const char *prefix = "", bool bInitial = false) const;
	virtual void DescribeDof(std::vector<std::string>& desc,
		bool bInitial = false, int i = -1) const;
	virtual std::ostream& DescribeEq(std::ostream& out,
		const char *prefix = "", bool bInitial = false) const;
	virtual void DescribeEq(std::vector<std::string>& desc,
		bool bInitial = false, int i = -1) const;
	virtual DofOrder::Order GetDofType(unsigned int) const;

	virtual void OutputPrepare(OutputHandler &OH);

	virtual void SetValue(DataManager *pDM,
			VectorHandler& X, VectorHandler& XP,
			SimulationEntity::Hints* h = 0);

	virtual void AfterConvergence(const VectorHandler& X,
			const VectorHandler& XP);

	/* Dimensioni del workspace */
	/* Workspace dimensions */
	virtual void
	WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;

	/* Numero di GDL iniziali */
	/* Initial DOF number */
	virtual unsigned int iGetInitialNumDof(void) const {
		return 0;
	};
	
	/* Dimensioni del workspace */
	/* Initial workspace dimensions */
	virtual void
	InitialWorkSpaceDim(integer* piNumRows, integer* piNumCols) const;

	/* assemblaggio jacobiano */
	/* jacobian matrix assembly */
	virtual VariableSubMatrixHandler&
	InitialAssJac(VariableSubMatrixHandler& WorkMat,
		      const VectorHandler& /* XCurr */);

	virtual const InducedVelocity *pGetInducedVelocity(void) const {
		return pIndVel;
	};
	/* ************************************************ */

	/* return s the dimension of the component */
	const virtual OutputHandler::Dimensions GetEquationDimension(integer index) const;
};

/* Aerodynamic2DElem - end */


/* AerodynamicBody - begin */

class AerodynamicBody :
	public Aerodynamic2DElem<1>
{
protected:
	const StructNode* pNode;
	
	/* Offset del punto di riferimento */
	/* Reference point offset */
	const Vec3 f;	
	/* Semiapertura del rettangoloide */
	/* Half-span of the quadrilateral */
	doublereal dHalfSpan;
	/* Rotaz. del sistema aerodinamico al nodo */
	/* Rotation matrix from the aerodynamic system to the nodes' */
	const Mat3x3 Ra;
	/* Terza colonna della matrice Ra */
	/* Third column of Ra matrix*/
	const Vec3 Ra3;		

	Vec3 F;			/* Force */
	Vec3 M;			/* Moment */

	/* Assemblaggio residuo */
	/* Residual assembly */
	void AssVec(SubVectorHandler& WorkVec,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

public:
	AerodynamicBody(unsigned int uLabel,
		const DofOwner *pDO,
		const StructNode* pN, InducedVelocityElem* pR, bool bPassive,
		const Vec3& fTmp, doublereal dS,
		const Mat3x3& RaTmp,
		const Shape* pC, const Shape* pF,
		const Shape* pV, const Shape* pT,
		const Shape* pTL,
		integer iN, AeroData* a,
		const DriveCaller* pDC,
		bool bUseJacobian,
		OrientationDescription ood,
		flag fOut);
	virtual ~AerodynamicBody(void);

	/* Scrive il contributo dell'elemento al file di restart */
	/* Writes the element contributiion to the restart file */
	virtual std::ostream& Restart(std::ostream& out) const;

	/* assemblaggio jacobiano */
	/* Jacobian matrix assembly */
	virtual VariableSubMatrixHandler&
	AssJac(VariableSubMatrixHandler& WorkMat,
	       doublereal  dCoef,
	       const VectorHandler& /* XCurr */ ,
	       const VectorHandler& /* XPrimeCurr */ );

	/* assemblaggio residuo */
	/* residual vector assembly */
	virtual SubVectorHandler&
	AssRes(SubVectorHandler& WorkVec,
	       doublereal dCoef,
	       const VectorHandler& XCurr,
	       const VectorHandler& XPrimeCurr);

	/*
	 * output; si assume che ogni tipo di elemento sappia, attraverso
	 * l'OutputHandler, dove scrivere il proprio output
	 *
	 * it is assumed that each elemen type knows, through the 
	 * OutputHandler, where to write its output 
	 */
	virtual void Output(OutputHandler& OH) const;

	/* assemblaggio residuo */
	/* residual vector assembly for initial assembly */
	virtual SubVectorHandler&
	InitialAssRes(SubVectorHandler& WorkVec,
		      const VectorHandler& XCurr);

	/* Tipo di elemento aerodinamico */
	/* Aerodynamic element type */
	virtual AerodynamicElemBase::Type GetAerodynamicElemType(void) const {
		return AerodynamicElemBase::AERODYNAMICBODY;
	};

	/* *******PER IL SOLUTORE PARALLELO******** */
	/* *******FOR PARALLEL SOLVER******* */
	/*
	 * Fornisce il tipo e la label dei nodi che sono connessi all'elemento
	 * utile per l'assemblaggio della matrice di connessione fra i dofs
	 *
	 * Gives the type and the label of the nodes that are connected to the
	 * element: useful for the assembly of the connectivity matrix
	 *
	 */
	virtual void
	GetConnectedNodes(std::vector<const Node *>& connectedNodes) const {
		connectedNodes.resize(1);
		connectedNodes[0] = pNode;
	};
	/* ************************************************ */
};

/* AerodynamicBody - end */


/* AerodynamicBeam - begin */

class AerodynamicBeam :
	public Aerodynamic2DElem<3>
{
protected:
	enum { NODE1 = 0, NODE2, NODE3, LASTNODE };
	enum { DELTAx1 = 0, DELTAg1, DELTAx2, DELTAg2, DELTAx3, DELTAg3 };

	const Beam* pBeam;
	const StructNode* pNode[3];

	const Vec3 f1;		/* Reference point offset */
	const Vec3 f2;		/* Reference point offset */
	const Vec3 f3;		/* Reference point offset */
	const Mat3x3 Ra1;	/* Rotation matrix from aerodynamics RF to node */
	const Mat3x3 Ra2;	/* Rotation matrix from aerodynamics RF to node */
	const Mat3x3 Ra3;	/* Rotation matrix from aerodynamic RF to node */
	const Vec3 Ra1_3;	/* Third column of Ra matrix */
	const Vec3 Ra2_3;	/* Third column of Ra matrix */
	const Vec3 Ra3_3;	/* Third column of Ra matrix */

	/*
	 * Nota: li lascio distinti perche' cosi' eventualmente ne posso fare
	 * l'output in modo agevole
	 *
	 * Note: they are in distinct vectors so that they can eventually be 
	 * output easily
	 *
	 */
	Vec3 F[LASTNODE];	/* Force */
	Vec3 M[LASTNODE];	/* Moment */

	/* Assemblaggio residuo */
	/* Residual vector assembly */
	void AssVec(SubVectorHandler& WorkVec,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

public:
	AerodynamicBeam(unsigned int uLabel,
		const DofOwner *pDO,
		const Beam* pB, InducedVelocityElem* pR, bool bPassive,
		const Vec3& fTmp1,
		const Vec3& fTmp2,
		const Vec3& fTmp3,
		const Mat3x3& Ra1Tmp,
		const Mat3x3& Ra2Tmp,
		const Mat3x3& Ra3Tmp,
		const Shape* pC, const Shape* pF,
		const Shape* pV, const Shape* pT,
		const Shape* pTL,
		integer iN, AeroData* a,
		const DriveCaller* pDC,
		bool bUseJacobian,
		OrientationDescription ood,
		flag fOut);
	virtual ~AerodynamicBeam(void);

	/* Scrive il contributo dell'elemento al file di restart */
	/* Writes the element contributiion to the restart file */
	virtual std::ostream& Restart(std::ostream& out) const;

	/* assemblaggio jacobiano */
	/* Jacobian matrix assembly... */
	virtual VariableSubMatrixHandler&
	AssJac(VariableSubMatrixHandler& WorkMat,
	       doublereal  dCoef  ,
	       const VectorHandler& /* XCurr */ ,
	       const VectorHandler& /* XPrimeCurr */ );

	/* assemblaggio residuo */
	/* Residual vector assembly */
	virtual SubVectorHandler&
	AssRes(SubVectorHandler& WorkVec,
	       doublereal dCoef,
	       const VectorHandler& XCurr,
	       const VectorHandler& XPrimeCurr);

	/* assemblaggio residuo */
	/* Residual vector assembly for initial assembly */
	virtual SubVectorHandler&
	InitialAssRes(SubVectorHandler& WorkVec,
		      const VectorHandler& XCurr);

	/*
	 * output; si assume che ogni tipo di elemento sappia, attraverso
	 * l'OutputHandler, dove scrivere il proprio output
	 *
	 * it is assumed that each elemen type knows, through the 
	 * OutputHandler, where to write its output 
	 */
	virtual void Output(OutputHandler& OH) const;

	/* Tipo di elemento aerodinamico */
	/* Type of aerodynamic element */
	virtual AerodynamicElemBase::Type GetAerodynamicElemType(void) const {
		return AerodynamicElemBase::AERODYNAMICBEAM;
	};

	/* *******PER IL SOLUTORE PARALLELO******** */
	/* *******FOR PARALLEL SOLVER******* */
	/*
	 * Fornisce il tipo e la label dei nodi che sono connessi all'elemento
	 * utile per l'assemblaggio della matrice di connessione fra i dofs
	 *
	 * Gives the type and the label of the nodes that are connected to the
	 * element: useful for the assembly of the connectivity matrix
	 *
	 */
	virtual void
	GetConnectedNodes(std::vector<const Node *>& connectedNodes) const {
		connectedNodes.resize(3);
		connectedNodes[0] = pNode[NODE1];
		connectedNodes[1] = pNode[NODE2];
		connectedNodes[2] = pNode[NODE3];
	};
   	/* ************************************************ */
};

/* AerodynamicBeam - end */

/* AerodynamicBeam2 - begin */

class AerodynamicBeam2 :
	public Aerodynamic2DElem<2>
{
protected:
	enum { NODE1 = 0, NODE2, LASTNODE };
	enum { DELTAx1 = 0, DELTAg1, DELTAx2, DELTAg2 };

	const Beam2* pBeam;
	const StructNode* pNode[2];

	const Vec3 f1;		/* Reference point offset */
	const Vec3 f2;		/* Reference point offset */
	const Mat3x3 Ra1;	/* Rotation matrix from aerodynamics RF to node */
	const Mat3x3 Ra2;	/* Rotation matrix from aerodynamics RF to node */
	const Vec3 Ra1_3;	/* Third column of Ra matrix */
	const Vec3 Ra2_3;	/* Third column of Ra matrix */

	/*
	 * Nota: li lascio distinti perche' cosi' eventualmente ne posso fare
	 * l'output in modo agevole
	 *
	 * Note: they are in distinct vectors so that they can eventually be 
	 * output easily
	 *
	 */
	Vec3 F[LASTNODE];	/* Force */
	Vec3 M[LASTNODE];	/* Moment */

	/* Assemblaggio residuo */
	/* Residual vector assembly */
	void AssVec(SubVectorHandler& WorkVec,
		doublereal dCoef,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

public:
	AerodynamicBeam2(unsigned int uLabel,
		const DofOwner *pDO,
		const Beam2* pB, InducedVelocityElem* pR, bool bPassive,
		const Vec3& fTmp1,
		const Vec3& fTmp2,
		const Mat3x3& Ra1Tmp,
		const Mat3x3& Ra2Tmp,
		const Shape* pC, const Shape* pF,
		const Shape* pV, const Shape* pT,
		const Shape* pTL,
		integer iN, AeroData* a,
		const DriveCaller* pDC,
		bool bUseJacobian,
		OrientationDescription ood,
		flag fOut);
	virtual ~AerodynamicBeam2(void);

	/* Scrive il contributo dell'elemento al file di restart */
	/* Writes the element contributiion to the restart file */
	virtual std::ostream& Restart(std::ostream& out) const;

	/* assemblaggio jacobiano */
	/* Jacobian matrix assembly... */
	virtual VariableSubMatrixHandler&
	AssJac(VariableSubMatrixHandler& WorkMat,
	       doublereal /* dCoef */ ,
	       const VectorHandler& /* XCurr */ ,
	       const VectorHandler& /* XPrimeCurr */ );

	/* assemblaggio residuo */
	/* Residual vector assembly */
	virtual SubVectorHandler&
	AssRes(SubVectorHandler& WorkVec,
	       doublereal dCoef,
	       const VectorHandler& XCurr,
	       const VectorHandler& XPrimeCurr);

	/* assemblaggio residuo */
	/* Residual vector assembly for initial assembly */
	virtual SubVectorHandler&
	InitialAssRes(SubVectorHandler& WorkVec,
		      const VectorHandler& XCurr);

	/*
	 * output; si assume che ogni tipo di elemento sappia, attraverso
	 * l'OutputHandler, dove scrivere il proprio output
	 *
	 * it is assumed that each elemen type knows, through the 
	 * OutputHandler, where to write its output 
	 */
	virtual void Output(OutputHandler& OH) const;

	/* Tipo di elemento aerodinamico */
	/* Type of aerodynamic element */
	virtual AerodynamicElemBase::Type GetAerodynamicElemType(void) const {
		return AerodynamicElemBase::AERODYNAMICBEAM;
	};

	/* *******PER IL SOLUTORE PARALLELO******** */
	/* *******FOR PARALLEL SOLVER******* */
	/*
	 * Fornisce il tipo e la label dei nodi che sono connessi all'elemento
	 * utile per l'assemblaggio della matrice di connessione fra i dofs
	 *
	 * Gives the type and the label of the nodes that are connected to the
	 * element: useful for the assembly of the connectivity matrix
	 *
	 */
	virtual void
	GetConnectedNodes(std::vector<const Node *>& connectedNodes) const {
		connectedNodes.resize(2);
		connectedNodes[0] = pNode[NODE1];
		connectedNodes[1] = pNode[NODE2];
	};
   	/* ************************************************ */
};

/* AerodynamicBeam - end */

class DataManager;
class MBDynParser;

extern void
ReadAerodynamicCustomOutput(DataManager* pDM, MBDynParser& HP, unsigned int uLabel,
	unsigned& uFlags, OrientationDescription& od);

extern void
ReadOptionalAerodynamicCustomOutput(DataManager* pDM, MBDynParser& HP, unsigned int uLabel,
	unsigned& uFlags, OrientationDescription& od);

extern Elem *
ReadAerodynamicBody(DataManager* pDM, MBDynParser& HP,
	const DofOwner *pDO, unsigned int uLabel);
extern Elem *
ReadAerodynamicBeam(DataManager* pDM, MBDynParser& HP,
	const DofOwner *pDO, unsigned int uLabel);
extern Elem *
ReadAerodynamicBeam2(DataManager* pDM, MBDynParser& HP,
	const DofOwner *pDO, unsigned int uLabel);

#endif /* AEROELEM_H */

