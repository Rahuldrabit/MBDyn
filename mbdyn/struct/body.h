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

/* elementi di massa, tipo: Elem::Type BODY */

#ifndef BODY_H
#define BODY_H

/* include per derivazione della classe */

#include "elem.h"
#include "strnode.h"
#include "gravity.h"

/* Mass - begin */

class Mass : public InitialAssemblyElem, public GravityOwner {
protected:
        const StructDispNode *pNode;
        doublereal dMass;

        /* momento statico */
        Vec3 GetS_int(void) const override;

        /* momento d'inerzia */
        Mat3x3 GetJ_int(void) const override;

        /* Scrive il contributo dell'elemento al file di restart */
        virtual std::ostream& Restart(std::ostream& out) const override;

        virtual void Restart(RestartData& oData, RestartData::RestartAction eAction) override;
     
        void
        AssVecRBK_int(SubVectorHandler& WorkVec);

        void
        AssMatsRBK_int(
                FullSubMatrixHandler& WMA,
                FullSubMatrixHandler& WMB,
                const doublereal& dCoef);

public:
        /* Costruttore definitivo (da mettere a punto) */
        Mass(unsigned int uL, const StructDispNode *pNode,
                doublereal dMassTmp, flag fOut);

        virtual ~Mass(void);

        /* massa totale */
        doublereal dGetM(void) const override;

        /* nodo */
        const StructDispNode *pGetNode(void) const;

        /* Tipo dell'elemento (usato solo per debug ecc.) */
        virtual Elem::Type GetElemType(void) const override {
                return Elem::BODY;
        }

	/* Deformable element */
	virtual bool bIsDeformable() const override {
		return true;
	}

        /* Numero gdl durante l'assemblaggio iniziale */
        virtual unsigned int iGetInitialNumDof(void) const override {
                return 0;
        }

        /* Accesso ai dati privati */
        virtual unsigned int iGetNumPrivData(void) const override;
        virtual unsigned int iGetPrivDataIdx(const char *s) const override;
        virtual doublereal dGetPrivData(unsigned int i) const override;

        /******** PER IL SOLUTORE PARALLELO *********/
        /* Fornisce il tipo e la label dei nodi che sono connessi all'elemento
         * utile per l'assemblaggio della matrice di connessione fra i dofs */
        virtual void
        GetConnectedNodes(std::vector<const Node *>& connectedNodes) const override {
                connectedNodes.resize(1);
                connectedNodes[0] = pNode;
        }
        /**************************************************/
};

/* Mass - end */

/* DynamicMass - begin */

class DynamicMass : public Mass {
private:

        virtual Vec3 GetB_int(void) const override;
        virtual Vec3 GetG_int(void) const override;
        /* Assembla le due matrici necessarie per il calcolo degli
         * autovalori e per lo jacobiano */
        void AssMats(FullSubMatrixHandler& WorkMatA,
                FullSubMatrixHandler& WorkMatB,
                doublereal dCoef,
                bool bGravity,
                const Vec3& GravityAcceleration);

public:
        /* Costruttore definitivo (da mettere a punto) */
        DynamicMass(unsigned int uL, const DynamicStructDispNode* pNode,
                doublereal dMassTmp, flag fOut);

        virtual ~DynamicMass(void);

        virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const override {
                *piNumRows = 3;
                *piNumCols = 3;
        }

        virtual VariableSubMatrixHandler&
        AssJac(VariableSubMatrixHandler& WorkMat,
                doublereal dCoef,
                const VectorHandler& XCurr,
                const VectorHandler& XPrimeCurr) override;

        void AssMats(VariableSubMatrixHandler& WorkMatA,
                VariableSubMatrixHandler& WorkMatB,
                const VectorHandler& XCurr,
                const VectorHandler& XPrimeCurr) override;

        virtual SubVectorHandler&
        AssRes(SubVectorHandler& WorkVec,
                doublereal dCoef,
                const VectorHandler& XCurr,
                const VectorHandler& XPrimeCurr) override;

        /* Dimensione del workspace durante l'assemblaggio iniziale.
         * Occorre tener conto del numero di dof che l'elemento definisce
         * in questa fase e dei dof dei nodi che vengono utilizzati.
         * Sono considerati dof indipendenti la posizione e la velocita'
         * dei nodi */
        virtual void
        InitialWorkSpaceDim(integer* piNumRows, integer* piNumCols) const override {
                *piNumRows = 3;
                *piNumCols = 3;
        }

        /* Contributo allo jacobiano durante l'assemblaggio iniziale */
        virtual VariableSubMatrixHandler&
        InitialAssJac(VariableSubMatrixHandler& WorkMat,
                const VectorHandler& XCurr) override;

        /* Contributo al residuo durante l'assemblaggio iniziale */
        virtual SubVectorHandler&
        InitialAssRes(SubVectorHandler& WorkVec, const VectorHandler& XCurr) override;

        /* Usata per inizializzare la quantita' di moto */
        virtual void SetValue(DataManager *pDM,
                VectorHandler& X, VectorHandler& XP,
                SimulationEntity::Hints *ph = 0) override;
};

/* DynamicMass - end */

/* StaticMass - begin */

class StaticMass : public Mass {
private:
        /* Assembla le due matrici necessarie per il calcolo degli
         * autovalori e per lo jacobiano */
        bool AssMats(FullSubMatrixHandler& WorkMatA,
                FullSubMatrixHandler& WorkMatB,
                doublereal dCoef);

public:
        /* Costruttore definitivo (da mettere a punto) */
        StaticMass(unsigned int uL, const StaticStructDispNode* pNode,
                doublereal dMass, flag fOut);

        virtual ~StaticMass(void);

        void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const override {
                *piNumRows = 3;
                *piNumCols = 3;
        }

        virtual VariableSubMatrixHandler&
        AssJac(VariableSubMatrixHandler& WorkMat,
                doublereal dCoef,
                const VectorHandler& XCurr,
                const VectorHandler& XPrimeCurr) override;

        void AssMats(VariableSubMatrixHandler& WorkMatA,
                VariableSubMatrixHandler& WorkMatB,
                const VectorHandler& XCurr,
                const VectorHandler& XPrimeCurr) override;

        virtual SubVectorHandler&
        AssRes(SubVectorHandler& WorkVec,
                doublereal dCoef,
                const VectorHandler& XCurr,
                const VectorHandler& XPrimeCurr) override;

        /* inverse dynamics capable element */
        virtual bool bInverseDynamics(void) const override;

        /* Inverse Dynamics: */
        virtual SubVectorHandler&
        AssRes(SubVectorHandler& WorkVec,
                const VectorHandler& /* XCurr */ ,
                const VectorHandler& /* XPrimeCurr */ ,
                const VectorHandler& /* XPrimePrimeCurr */ ,
                InverseDynamics::Order iOrder = InverseDynamics::INVERSE_DYNAMICS) override;

        /* Dimensione del workspace durante l'assemblaggio iniziale.
         * Occorre tener conto del numero di dof che l'elemento definisce
         * in questa fase e dei dof dei nodi che vengono utilizzati.
         * Sono considerati dof indipendenti la posizione e la velocita'
         * dei nodi */
        virtual void
        InitialWorkSpaceDim(integer* piNumRows, integer* piNumCols) const override {
                *piNumRows = 3;
                *piNumCols = 3;
        }

        /* Contributo allo jacobiano durante l'assemblaggio iniziale */
        virtual VariableSubMatrixHandler&
        InitialAssJac(VariableSubMatrixHandler& WorkMat,
                const VectorHandler& XCurr) override;

        /* Contributo al residuo durante l'assemblaggio iniziale */
        virtual SubVectorHandler&
        InitialAssRes(SubVectorHandler& WorkVec, const VectorHandler& XCurr) override;

        /* Usata per inizializzare la quantita' di moto */
        virtual void SetValue(DataManager *pDM,
                VectorHandler& X, VectorHandler& XP,
                SimulationEntity::Hints *ph = 0) override;
};

/* StaticMass - end */

/* Body - begin */

class Body :
public InitialAssemblyElem, public GravityOwner {
protected:
        const StructNode *pNode;
	doublereal dMass;
	Vec3 Xgc;
	Vec3 S0;
	Mat3x3 J0;
 
	mutable Vec3 STmp;
	mutable Mat3x3 JTmp;

	/* momento statico */
	Vec3 GetS_int(void) const override;

	/* momento d'inerzia */
	Mat3x3 GetJ_int(void) const override;

	void
	AssVecRBK_int(SubVectorHandler& WorkVec);

	void
	AssMatsRBK_int(
		FullSubMatrixHandler& WMA,
		FullSubMatrixHandler& WMB,
		const doublereal& dCoef,
		const Vec3& Sc);

public:
        /* Costruttore definitivo (da mettere a punto) */
        Body(unsigned int uL, const StructNode *pNode,
                doublereal dMassTmp, const Vec3& XgcTmp, const Mat3x3& JTmp,
                flag fOut);

        virtual ~Body(void);

        /* Scrive il contributo dell'elemento al file di restart */
        virtual std::ostream& Restart(std::ostream& out) const override;

        virtual void Restart(RestartData& oData, RestartData::RestartAction eAction) override;
     
        /* massa totale */
        doublereal dGetM(void) const override;

        /* nodo */
        const StructNode *pGetNode(void) const;

        /* Tipo dell'elemento (usato solo per debug ecc.) */
        virtual Elem::Type GetElemType(void) const override {
                return Elem::BODY;
        }

	/* Deformable element */
	virtual bool bIsDeformable() const override {
		return true;
	}

        /* Numero gdl durante l'assemblaggio iniziale */
        virtual unsigned int iGetInitialNumDof(void) const override {
                return 0;
        }

        virtual void AfterPredict(VectorHandler& X, VectorHandler& XP) override;

        /* Accesso ai dati privati */
        virtual unsigned int iGetNumPrivData(void) const override;
        virtual unsigned int iGetPrivDataIdx(const char *s) const override;
        virtual doublereal dGetPrivData(unsigned int i) const override;

        /******** PER IL SOLUTORE PARALLELO *********/
        /* Fornisce il tipo e la label dei nodi che sono connessi all'elemento
         * utile per l'assemblaggio della matrice di connessione fra i dofs */
        virtual void
        GetConnectedNodes(std::vector<const Node *>& connectedNodes) const override {
                connectedNodes.resize(1);
                connectedNodes[0] = pNode;
        }
        /**************************************************/
};

/* Body - end */


/* DynamicBody - begin */

class DynamicBody : public Body {
private:

        Vec3 GetB_int(void) const;
        Vec3 GetG_int(void) const;

        /* Assembla le due matrici necessarie per il calcolo degli
         * autovalori e per lo jacobiano */
        void AssMats(FullSubMatrixHandler& WorkMatA,
                FullSubMatrixHandler& WorkMatB,
                doublereal dCoef,
                bool bGravity,
                const Vec3& GravityAcceleration);

public:
	/* Costruttore definitivo (da mettere a punto) */
	DynamicBody(unsigned int uL, const DynamicStructNode* pNodeTmp, 
		doublereal dMassTmp, const Vec3& XgcTmp, const Mat3x3& JTmp, 
		flag fOut);

	virtual ~DynamicBody(void);
 
	virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;
 
	virtual VariableSubMatrixHandler&
	AssJac(VariableSubMatrixHandler& WorkMat,
		doublereal dCoef,
		const VectorHandler& XCurr, 
		const VectorHandler& XPrimeCurr);

	virtual void
	AssMats(VariableSubMatrixHandler& WorkMatA,
		VariableSubMatrixHandler& WorkMatB,
		const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);
 
	virtual SubVectorHandler&
	AssRes(SubVectorHandler& WorkVec,
		doublereal dCoef,
		const VectorHandler& XCurr, 
		const VectorHandler& XPrimeCurr);
 
	/* Dimensione del workspace durante l'assemblaggio iniziale.
	 * Occorre tener conto del numero di dof che l'elemento definisce
	 * in questa fase e dei dof dei nodi che vengono utilizzati.
	 * Sono considerati dof indipendenti la posizione e la velocita'
	 * dei nodi */
	virtual void 
	InitialWorkSpaceDim(integer* piNumRows, integer* piNumCols) const;

	/* Contributo allo jacobiano durante l'assemblaggio iniziale */
	virtual VariableSubMatrixHandler& 
	InitialAssJac(VariableSubMatrixHandler& WorkMat,
		const VectorHandler& XCurr);

	/* Contributo al residuo durante l'assemblaggio iniziale */   
	virtual SubVectorHandler& 
	InitialAssRes(SubVectorHandler& WorkVec, const VectorHandler& XCurr);

	/* Usata per inizializzare la quantita' di moto */
	virtual void SetValue(DataManager *pDM,
		VectorHandler& X, VectorHandler& XP,
		SimulationEntity::Hints *ph = 0);
};

/* DynamicBody - end */


/* ModalBody - begin */

class ModalBody : public DynamicBody {
private:
        Vec3 XPP, WP;

        /* Assembla le due matrici necessarie per il calcolo degli
         * autovalori e per lo jacobiano */
        void AssMats(FullSubMatrixHandler& WorkMatA,
                     FullSubMatrixHandler& WorkMatB,
                     doublereal dCoef,
                     const VectorHandler& XCurr,
                     const VectorHandler& XPrimeCurr,
                     bool bGravity,
                     const Vec3& GravityAcceleration);
public:
        /* Costruttore definitivo (da mettere a punto) */
        ModalBody(unsigned int uL, const ModalNode* pNodeTmp,
                doublereal dMassTmp, const Vec3& XgcTmp, const Mat3x3& JTmp,
                flag fOut);

        virtual ~ModalBody(void);

        void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;

        virtual VariableSubMatrixHandler&
        AssJac(VariableSubMatrixHandler& WorkMat,
               doublereal dCoef,
               const VectorHandler& XCurr,
               const VectorHandler& XPrimeCurr);

        void AssMats(VariableSubMatrixHandler& WorkMatA,
                     VariableSubMatrixHandler& WorkMatB,
                     const VectorHandler& XCurr,
                     const VectorHandler& XPrimeCurr);
                
        virtual SubVectorHandler&
        AssRes(SubVectorHandler& WorkVec,
               doublereal dCoef,
               const VectorHandler& XCurr,
               const VectorHandler& XPrimeCurr);

        virtual void
        SetValue(DataManager *pDM,
                 VectorHandler& X, VectorHandler& XP,
                 SimulationEntity::Hints *ph = 0);
};

/* ModalBody - end */


/* StaticBody - begin */

class StaticBody : public Body {
private:
        /* Assembla le due matrici necessarie per il calcolo degli
         * autovalori e per lo jacobiano */
        bool AssMats(FullSubMatrixHandler& WorkMatA,
                FullSubMatrixHandler& WorkMatB,
                doublereal dCoef);

public:
        /* Costruttore definitivo (da mettere a punto) */
        StaticBody(unsigned int uL, const StaticStructNode* pNode,
                doublereal dMass, const Vec3& Xgc, const Mat3x3& J,
                flag fOut);

        virtual ~StaticBody(void);

        void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const override {
                *piNumRows = 6;
                *piNumCols = 6;
        }

        virtual VariableSubMatrixHandler&
        AssJac(VariableSubMatrixHandler& WorkMat,
                doublereal dCoef,
                const VectorHandler& XCurr,
                const VectorHandler& XPrimeCurr) override;

        void AssMats(VariableSubMatrixHandler& WorkMatA,
                VariableSubMatrixHandler& WorkMatB,
                const VectorHandler& XCurr,
                const VectorHandler& XPrimeCurr) override;

        virtual SubVectorHandler&
        AssRes(SubVectorHandler& WorkVec,
                doublereal dCoef,
                const VectorHandler& XCurr,
                const VectorHandler& XPrimeCurr) override;

        /* inverse dynamics capable element */
        virtual bool bInverseDynamics(void) const override;

        /* Inverse Dynamics: */
        virtual SubVectorHandler&
        AssRes(SubVectorHandler& WorkVec,
                const VectorHandler& /* XCurr */ ,
                const VectorHandler& /* XPrimeCurr */ ,
                const VectorHandler& /* XPrimePrimeCurr */ ,
                InverseDynamics::Order iOrder = InverseDynamics::INVERSE_DYNAMICS) override;

        /* Dimensione del workspace durante l'assemblaggio iniziale.
         * Occorre tener conto del numero di dof che l'elemento definisce
         * in questa fase e dei dof dei nodi che vengono utilizzati.
         * Sono considerati dof indipendenti la posizione e la velocita'
         * dei nodi */
        virtual void
        InitialWorkSpaceDim(integer* piNumRows, integer* piNumCols) const override {
                *piNumRows = 6;
                *piNumCols = 6;
        }

        /* Contributo allo jacobiano durante l'assemblaggio iniziale */
        virtual VariableSubMatrixHandler&
        InitialAssJac(VariableSubMatrixHandler& WorkMat,
                const VectorHandler& XCurr) override;

        /* Contributo al residuo durante l'assemblaggio iniziale */
        virtual SubVectorHandler&
        InitialAssRes(SubVectorHandler& WorkVec, const VectorHandler& XCurr) override;

        /* Usata per inizializzare la quantita' di moto */
        virtual void SetValue(DataManager *pDM,
                VectorHandler& X, VectorHandler& XP,
                SimulationEntity::Hints *ph = 0) override;
};

/* StaticBody - end */

class DataManager;
class MBDynParser;

extern Elem* ReadBody(DataManager* pDM, MBDynParser& HP, unsigned int uLabel);

#endif /* BODY_H */
