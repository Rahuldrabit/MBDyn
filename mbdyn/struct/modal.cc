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
   classe per l'introduzione della flessibilita' modale nel codice multi-corpo

   29/7/99:  implementata la parte di corpo rigido (solo massa e momenti
             d'inerzia) verificata con massa isolata soggetta a forze
             e momenti esterni

   13/9/99:  corpo rigido OK (anche momenti statici ed eq. di vincolo)
             corpo deformabile isolato OK (verificato con osc. armonico)

   23/9/99:  corpo deformabile + eq. di vincolo (no moto rigido)  OK
             (verificato con trave incastrata)
             corpo rigido + deformabile OK (verificato con trave libera)

     10/99:  validazioni con NASTRAN: trave incastrata, trave libera
             con forza in mezzeria, trave libera con forza all'estremita'

   4/11/99:  completate le funzioni InitialAssJac e InitialAssRes
             per l'assemblaggio iniziale del 'vincolo'

  22/11/99:  modificata la funzione ReadModal per leggere il file
             generato da DADS

  26/11/99:  validazione con piastra vincolata con elementi elastici

     12/99:  validazione con piastra e bauchau

   01/2000:  modi rotanti

  30/11/99:  aggiunto lo smorzamento strutturale tra i dati d'ingresso
             aggiunte le inerzie concentrate

   1/12/99:  modifiche alla lettura dati d'ingresso (l'estensione *.fem
             al file con i dati del modello ad elementi viene aggiunta
             automaticamente dal programma, che crea un file di
             output con l'estensione *.mod)
             corretto un bug nella scrittura delle equazioni di vincolo

  17/12/99:  aggiunta la possibilita' di definire uno smorzamento strutturale
             variabile con le frequenze

  23/12/99:  nuova modifica alla lettura dati di ingresso

10/01/2000:  introdotta la possibilita' di definire un fattore di scala
             per i dati del file d'ingresso

22/01/2000:  tolto il fattore di scala perche' non funziona

   03/2000:  aggiunte funzioni che restituiscono dati dell'elemento
             (autovettori, modi ecc.) aggiunta la possibilita'
             di imporre delle deformate modali iniziali

   Modifiche fatte al resto del programma:

   Nella classe joint    : aggiunto il vincolo modale
   Nella classe strnode  : aggiunto il nodo modale
   Nella classe MatVec3n : aggiunte classi e funzioni per gestire matrici
                           Nx3 e NxN
   Nella classe SubMat   : corretto un bug nelle funzioni Add Mat3xN ecc.,
                           aggiunte funzioni per trattare sottomatrici Nx3
*/

/*
 * Copyright 1999-2023 Felice Felippone <ffelipp@tin.it>
 * Dipartimento di Ingegneria Aerospaziale - Politecnico di Milano
 */

/*
 * Copyright 1999-2023 Pierangelo Masarati  <pierangelo.masarati@polimi.it>
 * Dipartimento di Ingegneria Aerospaziale - Politecnico di Milano
 *
 * Modified by Pierangelo Masarati
 */

#include "mbconfig.h"           /* This goes first in every *.c,*.cc file */

/* FIXME: gravity in modal elements is eXperimental; undefine to disable */
#define MODAL_USE_GRAVITY

#include <cerrno>
#include <cstring>
#include <stdint.h>
#include <sys/stat.h>
#include <limits>
#include <algorithm>

#include "modal.h"
#include "modalad.h"
#include "dataman.h"
#include "Rot.hh"
#include "hint_impl.h"

Modal::StrNodeData::StrNodeData()
     :pNode(nullptr), pNodeAd(nullptr), bOut(false)
{
}

Modal::Modal(unsigned int uL,
        const ModalNode* pR,
        const Vec3& x0,
        const Mat3x3& R0,
        const DofOwner* pDO,
        unsigned int NM,         /* numero modi */
        unsigned int NI,         /* numero nodi d'interfaccia */
        unsigned int NF,         /* numero nodi FEM */
        doublereal dMassTmp,     /* inv. inerzia (m, m.stat., d'in.) */
        const Vec3& STmp,
        const Mat3x3& JTmp,
        std::vector<unsigned int>&& uModeNumber,
        MatNxN&& oGenMass,
        MatNxN&& oGenStiff,
        MatNxN&& oGenDamp,
        std::vector<std::string>&& IdFEMNodes,	/* label nodi FEM */
        Mat3xN&& oN,               /* posizione dei nodi FEM */
        std::vector<Modal::StrNodeData>&& snd,
        Mat3xN&& oPHItStrNode,     /* forme modali nodi d'interfaccia */
        Mat3xN&& oPHIrStrNode,
        Mat3xN&& oModeShapest,     /* autovettori: servono a aeromodal */
        Mat3xN&& oModeShapesr,
        Mat3xN&& oInv3,            /* invarianti d'inerzia I3...I11 */
        Mat3xN&& oInv4,
        Mat3xN&& oInv5,
        Mat3xN&& oInv8,
        Mat3xN&& oInv9,
        Mat3xN&& oInv10,
        Mat3xN&& oInv11,
        VecN&& aa,
        VecN&& bb,
        flag fOut)
: Joint(uL, pDO, fOut),
pModalNode(pR),
iRigidOffset(pModalNode ? 12 : 0),
x(x0),
R(R0),
RT(R0.Transpose()),
NModes(NM),
NStrNodes(NI),
NFEMNodes(NF),
IdFEMNodes(std::move(IdFEMNodes)),
oXYZFEMNodes(std::move(oN)),
dMass(dMassTmp),
Inv2(STmp),
Inv7(JTmp),
uModeNumber(std::move(uModeNumber)),
oModalMass(std::move(oGenMass)),
oModalStiff(std::move(oGenStiff)),
oModalDamp(std::move(oGenDamp)),
oPHIt(std::move(oPHItStrNode)),
oPHIr(std::move(oPHIrStrNode)),
oModeShapest(std::move(oModeShapest)),
oModeShapesr(std::move(oModeShapesr)),
oCurrXYZ{},
oCurrXYZVel{},
oInv3(std::move(oInv3)),
oInv4(std::move(oInv4)),
oInv5(std::move(oInv5)),
oInv8(std::move(oInv8)),
oInv9(std::move(oInv9)),
oInv10(std::move(oInv10)),
oInv11(std::move(oInv11)),
Inv3jaj(::Zero3),
Inv3jaPj(::Zero3),
Inv8jaj(::Zero3x3),
Inv8jaPj(::Zero3x3),
Inv5jaj(NModes, 0.),
Inv5jaPj(NModes, 0.),
Inv9jkajak(::Zero3x3),
Inv9jkajaPk(::Eye3),
a(aa), a0(aa),
aPrime(NModes, 0.), aPrime0(bb),
b(bb),
bPrime(NModes, 0.),
SND(std::move(snd))
{
     ASSERT(NStrNodes == SND.size());
     ASSERT(pModalNode == nullptr || pModalNode->GetStructNodeType() == StructNode::MODAL);
}

Modal::~Modal(void)
{
        /* FIXME: destroy all the other data ... */
}

Joint::Type
Modal::GetJointType(void) const
{
        return Joint::MODAL;
}

std::ostream&
Modal::Restart(std::ostream& out) const
{
        return out << "modal; # not implemented yet" << std::endl;
}

void Modal::Restart(RestartData& oData, RestartData::RestartAction eAction)
{
     oData.Sync(RestartData::ELEM_JOINTS, GetLabel(), "a", a, eAction);
     oData.Sync(RestartData::ELEM_JOINTS, GetLabel(), "aPrime", aPrime, eAction);
     oData.Sync(RestartData::ELEM_JOINTS, GetLabel(), "b", b, eAction);
     oData.Sync(RestartData::ELEM_JOINTS, GetLabel(), "bPrime", bPrime, eAction);
}

unsigned int
Modal::iGetNumDof(void) const
{
        /* i gradi di liberta' propri sono:
         *   2*M per i modi
         *   6*N per le reazioni vincolari dei nodi d'interfaccia */
        return 2*NModes + 6*NStrNodes;
}

std::ostream&
Modal::DescribeDof(std::ostream& out, const char *prefix, bool bInitial) const
{
        integer iModalIndex = iGetFirstIndex();

        out
                << prefix << iModalIndex + 1 << "->" << iModalIndex + NModes
                << ": modal displacements [q(1.." << NModes << ")]" << std::endl
                << prefix << iModalIndex + NModes + 1 << "->" << iModalIndex + 2*NModes
                << ": modal velocities [qP(1.." << NModes << ")]" << std::endl;
        iModalIndex += 2*NModes;
        for (unsigned iStrNodem1 = 0; iStrNodem1 < NStrNodes; iStrNodem1++, iModalIndex += 6) {
                out
                        << prefix << iModalIndex + 1 << "->" << iModalIndex + 3 << ": "
                                "StructNode(" << SND[iStrNodem1].pNode->GetLabel() << ") "
                                "reaction forces [Fx,Fy,Fz]" << std::endl
                        << prefix << iModalIndex + 4 << "->" << iModalIndex + 6 << ": "
                                "StructNode(" << SND[iStrNodem1].pNode->GetLabel() << ") "
                                "reaction couples [Mx,My,Mz]" << std::endl;
                if (bInitial) {
                        iModalIndex += 6;
                        out
                                << prefix << iModalIndex + 1 << "->" << iModalIndex + 3 << ": "
                                        "StructNode(" << SND[iStrNodem1].pNode->GetLabel() << ") "
                                        "reaction force derivatives [FPx,FPy,FPz]" << std::endl
                                << prefix << iModalIndex + 4 << "->" << iModalIndex + 6 << ": "
                                        "StructNode(" << SND[iStrNodem1].pNode->GetLabel() << ") "
                                        "reaction couple derivatives [MPx,MPy,MPz]" << std::endl;
                }
        }

        return out;
}

static const char xyz[] = "xyz";
static const char *mdof[] = {
        "modal displacement q",
        "modal velocity qP"
};
static const char *rdof[] = {
        "reaction force F",
        "reaction couple M",
        "reaction force derivative FP",
        "reaction couple derivative MP"
};
static const char *meq[] = {
        "modal velocity definition q",
        "modal equilibrium equation f"
};
static const char *req[] = {
        "position constraint P",
        "orientation constraint g",
        "velocity constraint v",
        "angular velocity constraint w"
};

void
Modal::DescribeDof(std::vector<std::string>& desc, bool bInitial, int i) const
{
        int iend = 1;
        if (i == -1) {
                iend = 2*NModes + 6*NStrNodes;
                if (bInitial) {
                        iend += 6*NStrNodes;
                }
        }
        desc.resize(iend);

        unsigned modulo = 6;
        if (bInitial) {
                modulo = 12;
        }

        std::ostringstream os;
        os << "Modal(" << GetLabel() << ")";

        if (i < -1 ) {
                // error
                throw ErrGeneric(MBDYN_EXCEPT_ARGS);

        } else if (i == -1) {
                std::string name(os.str());

                for (unsigned i = 0; i < 2*NModes; i++) {
                        os.str(name);
                        os.seekp(0, std::ios_base::end);
                        os << ": " << mdof[i/NModes] << "(" << i%NModes + 1 << ")";
                        desc[i] = os.str();
                }

                for (unsigned i = 0; i < modulo*NStrNodes; i++) {
                        os.str(name);
                        os.seekp(0, std::ios_base::end);
                        os << ": StructNode(" << SND[i/modulo].pNode->GetLabel() << ") "
                                << rdof[(i/3)%(modulo/3)] << xyz[i%3];
                        desc[2*NModes + i] = os.str();
                }

        } else {
                if (unsigned(i) < 2*NModes) {
                        // modes
                        os << ": " << mdof[i/NModes] << "(" << i%NModes + 1 << ")";

                } else {
                        // reactions
                        i -= 2*NModes;

                        if (unsigned(i) >= modulo*NStrNodes) {
                                // error
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                        os << ": StructNode(" << SND[i/modulo].pNode->GetLabel() << ") "
                                << rdof[(i/3)%(modulo/3)] << xyz[i%3];
                }
                desc[0] = os.str();
        }
}

std::ostream&
Modal::DescribeEq(std::ostream& out, const char *prefix, bool bInitial) const
{
        integer iModalIndex = iGetFirstIndex();

        out
                << prefix << iModalIndex + 1 << "->" << iModalIndex + NModes
                << ": modal velocity definitions [q(1.." << NModes << ")=qP(1.." << NModes << ")]" << std::endl
                << prefix << iModalIndex + NModes + 1 << "->" << iModalIndex + 2*NModes
                << ": modal equilibrium equations [1.." << NModes << "]" << std::endl;
        iModalIndex += 2*NModes;
        for (unsigned iStrNodem1 = 0; iStrNodem1 < NStrNodes; iStrNodem1++, iModalIndex += 6) {
                out
                        << prefix << iModalIndex + 1 << "->" << iModalIndex + 3 << ": "
                                "StructNode(" << SND[iStrNodem1].pNode->GetLabel() << ") "
                                "position constraints [Px=PxN,Py=PyN,Pz=PzN]" << std::endl
                        << prefix << iModalIndex + 4 << "->" << iModalIndex + 6 << ": "
                                "StructNode(" << SND[iStrNodem1].pNode->GetLabel() << ") "
                                "orientation constraints [gx=gxN,gy=gyN,gz=gzN]" << std::endl;
                if (bInitial) {
                        iModalIndex += 6;
                        out
                                << prefix << iModalIndex + 1 << "->" << iModalIndex + 3 << ": "
                                        "StructNode(" << SND[iStrNodem1].pNode->GetLabel() << ") "
                                        "velocity constraints [vx=vxN,vy=vyN,vz=vzN]" << std::endl
                                << prefix << iModalIndex + 4 << "->" << iModalIndex + 6 << ": "
                                        "StructNode(" << SND[iStrNodem1].pNode->GetLabel() << ") "
                                        "angular velocity constraints [wx=wxN,wy=wyN,wz=wzN]" << std::endl;
                }
        }

        return out;
}

void
Modal::DescribeEq(std::vector<std::string>& desc, bool bInitial, int i) const
{
        int iend = 1;
        if (i == -1) {
                iend = 2*NModes + 6*NStrNodes;
                if (bInitial) {
                        iend += 6*NStrNodes;
                }
        }
        desc.resize(iend);

        unsigned modulo = 6;
        if (bInitial) {
                modulo = 12;
        }

        std::ostringstream os;
        os << "Modal(" << GetLabel() << ")";

        if (i < -1 ) {
                // error
                throw ErrGeneric(MBDYN_EXCEPT_ARGS);

        } else if (i == -1) {
                std::string name(os.str());

                for (unsigned i = 0; i < 2*NModes; i++) {
                        os.str(name);
                        os.seekp(0, std::ios_base::end);
                        os << ": " << meq[i/NModes] << "(" << i%NModes + 1 << ")";
                        desc[i] = os.str();
                }

                for (unsigned i = 0; i < modulo*NStrNodes; i++) {
                        os.str(name);
                        os.seekp(0, std::ios_base::end);
                        os << ": StructNode(" << SND[i/modulo].pNode->GetLabel() << ") "
                                << req[(i/3)%(modulo/3)] << xyz[i%3];
                        desc[2*NModes + i] = os.str();
                }

        } else {
                if (unsigned(i) < 2*NModes) {
                        // modes
                        os << ": " << meq[i/NModes] << "(" << i%NModes + 1 << ")";

                } else {
                        // reactions
                        i -= 2*NModes;

                        if (unsigned(i) >= modulo*NStrNodes) {
                                // error
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                        os << ": StructNode(" << SND[i/modulo].pNode->GetLabel() << ") "
                                << req[(i/3)%(modulo/3)] << xyz[i%3];
                }
                desc[0] = os.str();
        }
}

DofOrder::Order
Modal::GetDofType(unsigned int i) const
{
        ASSERT(i < iGetNumDof());

        if (i < 2*NModes) {
                /* gradi di liberta' differenziali (eq. modali) */
                return DofOrder::DIFFERENTIAL;

        } /* else if (i >= 2*NModes && i < iGetNumDof()) { */
        /* gradi di liberta' algebrici (eq. di vincolo) */
        return DofOrder::ALGEBRAIC;
        /* } */
}

DofOrder::Order
Modal::GetEqType(unsigned int i) const
{
        ASSERT(i < iGetNumDof());

        if (i < 2*NModes) {
                /* gradi di liberta' differenziali (eq. modali) */
                return DofOrder::DIFFERENTIAL;

        } /* else if (i >= 2*NModes && i < iGetNumDof()) { */
        /* gradi di liberta' algebrici (eq. di vincolo) */
        return DofOrder::ALGEBRAIC;
        /* } */
}

void
Modal::WorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
     /* la matrice e' gestita come piena (c'e' un po' di spreco...) */
     *piNumCols = *piNumRows = iRigidOffset + iGetNumDof() + 6*NStrNodes;
}

VariableSubMatrixHandler&
Modal::AssJac(VariableSubMatrixHandler& WorkMat,
                doublereal dCoef,
                const VectorHandler& XCurr,
                const VectorHandler& XPrimeCurr)
{
        DEBUGCOUT("Entering Modal::AssJac()" << std::endl);

        FullSubMatrixHandler& WM = WorkMat.SetFull();
        integer iNumRows = 0;
        integer iNumCols = 0;
        WorkSpaceDim(&iNumRows, &iNumCols);
        WM.ResizeReset(iNumRows, iNumCols);

        /* gli indici sono ordinati cosi': i primi 6 sono le equazioni
         * per abbassare di grado il sistema,
         * quindi le 6 equazioni del moto rigido, quindi le 2*M modali,
         * quindi le eq. vincolari   */

        /* FIXME: by now, I add a test everywhere it's needed;
         * later, I'll try to group conditional parts in separate tests */

        /* indici della parte rigida */

        if (pModalNode) {
                integer iRigidIndex = pModalNode->iGetFirstIndex();

                for (unsigned int iCnt = 1; iCnt <= iRigidOffset; iCnt++) {
                        WM.PutRowIndex(iCnt, iRigidIndex + iCnt);
                        WM.PutColIndex(iCnt, iRigidIndex + iCnt);
                }
        }

        /* indici della parte deformabile e delle reazioni vincolari */
        integer iFlexIndex = iGetFirstIndex();
        unsigned int iNumDof = iGetNumDof();

        for (unsigned int iCnt = 1; iCnt <= iNumDof; iCnt++) {
                WM.PutRowIndex(iRigidOffset + iCnt, iFlexIndex + iCnt);
                WM.PutColIndex(iRigidOffset + iCnt, iFlexIndex + iCnt);
        }

        /* indici delle equazioni vincolari (solo per il nodo 2) */
        for (unsigned int iStrNodem1 = 0; iStrNodem1 < NStrNodes; iStrNodem1++) {
                integer iNodeFirstMomIndex =
                        SND[iStrNodem1].pNode->iGetFirstMomentumIndex();
                integer iNodeFirstPosIndex =
                        SND[iStrNodem1].pNode->iGetFirstPositionIndex();

                integer iOffset = iRigidOffset + iNumDof + 6*iStrNodem1;
                for (integer iCnt = 1; iCnt <= 6; iCnt++) {
                        WM.PutRowIndex(iOffset + iCnt, iNodeFirstMomIndex + iCnt);
                        WM.PutColIndex(iOffset + iCnt, iNodeFirstPosIndex + iCnt);
                }
        }

        /* Assemblaggio dello Jacobiano */

        Vec3 wr(::Zero3);
        Mat3x3 J(::Zero3x3);
        Vec3 S(::Zero3);
        if (pModalNode) {
                /* recupera e aggiorna i dati necessari */
                /* i prodotti Inv3j*aj ecc. sono gia' stati fatti da AssRes() */

                wr = pModalNode->GetWRef();
                /* R and RT are updated by AssRes() */
                Mat3x3 JTmp = Inv7;
                if (oInv8.iGetNumCols()) {
                        JTmp += Inv8jaj.Symm2();
                }
                J = R*JTmp*RT;
                Vec3 STmp = Inv2;
                if (oInv3.iGetNumCols()) {
                        STmp += Inv3jaj;
                }
                S = R*STmp;

                /* matrice di massa:        J[1,1] = Mtot  */
                for (int iCnt = 6 + 1; iCnt <= 6 + 3; iCnt++) {
                        WM.PutCoef(iCnt, iCnt, dMass);
                }

                /* momenti statici J[1,2] = -[S/\] + c[-2w/\S/\ + S/\w/\] */
                WM.Add(6 + 1, 9 + 1, Mat3x3(MatCrossCross, S, wr*dCoef)
                        - Mat3x3(MatCrossCross, wr, S*(2.*dCoef))
                        - Mat3x3(MatCross, S));

                /* J[2,1] = [S/\] */
                WM.Add(9 + 1, 6 + 1, Mat3x3(MatCross, S));

                /* momenti d'inerzia:       J[2,2] = J + c[ - (Jw)/\ + (w/\)J];    */
                WM.Add(9 + 1, 9 + 1, J + ((wr.Cross(J) - Mat3x3(MatCross, J*wr))*dCoef));
        }

        /* parte deformabile :
         *
         * | I  -cI  ||aP|
         * |         ||  |
         * |cK  M + cC ||bP|
         */

        for (unsigned int iCnt = 1; iCnt <= NModes; iCnt++) {
                unsigned int iiCnt = iRigidOffset + iCnt;

                WM.PutCoef(iiCnt, iiCnt, 1.);
                WM.PutCoef(iiCnt, iiCnt + NModes, -dCoef);
                for (unsigned int jCnt = 1; jCnt <= NModes; jCnt++) {
                        unsigned int jjCnt = iRigidOffset + jCnt;

                        WM.PutCoef(iiCnt + NModes, jjCnt,
                                        dCoef*oModalStiff.dGet(iCnt, jCnt));
                        WM.PutCoef(iiCnt + NModes, jjCnt + NModes,
                                        oModalMass.dGet(iCnt, jCnt)
                                        + dCoef*oModalDamp.dGet(iCnt, jCnt));
                }
        }

        if (pModalNode) {

                /* termini di accoppiamento moto rigido-deformabile;
                 * eventualmente l'utente potra' scegliere
                 * se trascurarli tutti, una parte o considerarli tutti */

                /* linearizzazione delle OmegaPrime:
                 * J13 = R*Inv3jaj
                 * J23 = R*Inv4 + Inv5jaj
                 * (questi termini ci vogliono sempre)
                 */

                if (oInv3.iGetNumCols()) {
                        Mat3xN Jac13(NModes, 0.);
                        WM.Add(6 + 1, iRigidOffset + NModes + 1, Jac13.LeftMult(R, oInv3));

                        WM.AddT(iRigidOffset + NModes + 1, 6 + 1, Jac13);

                        Mat3x3 Inv3jaPjWedge(MatCross, R*(Inv3jaPj*(2.*dCoef)));
                        WM.Sub(6 + 1, 9 + 1, Inv3jaPjWedge);
                }

                if (oInv4.iGetNumCols()) {
                        Mat3xN Jac23(NModes, 0.);
                        Jac23.LeftMult(R, oInv4);
                        if (oInv5.iGetNumCols()) {
                                Mat3xN Inv5jajRef(NModes, 0.);

                                Jac23 += Inv5jajRef.LeftMult(R, Inv5jaj);
                        }
                        WM.Add(9 + 1, iRigidOffset + NModes + 1, Jac23);

                        WM.AddT(iRigidOffset + NModes + 1, 9 + 1, Jac23);
                }


                /* termini di Coriolis: linearizzazione delle Omega
                 * (si puo' evitare se non ho grosse vel. angolari):
                 * J13 = -2*R*[Inv3jaPj/\]*RT
                 * J23 = 2*R*[Inv8jaPj - Inv9jkajaPk]*RT */

                if (oInv8.iGetNumCols()) {
                        Mat3x3 JTmp = Inv8jaPj;
                        if (oInv9.iGetNumCols()) {
                                JTmp -= Inv9jkajaPk;
                        }
                        WM.Add(9 + 1, 9 + 1, R*(JTmp*(RT*(2.*dCoef))));
                }

                /* termini di Coriolis: linearizzazione delle b;
                 * si puo' evitare 'quasi' sempre: le velocita'
                 * di deformazione dovrebbero essere sempre piccole
                 * rispetto ai termini rigidi
                 * Jac13 = 2*[Omega/\]*R*PHI
                 */
#if 0
                Jac13.LeftMult(wrWedge*R*2*dCoef, *pInv3);
                WM.Add(6 + 1, iRigidOffset + NModes + 1, Jac13);
#endif
                /* nota: non riesco a tirar fuori la Omega dall'equazione
                 * dei momenti:
                 * 2*[ri/\]*[Omega/\]*R*PHIi*{DeltaaP},  i = 1,...nnodi
                 * quindi questa equazione non si puo' linearizzare
                 * a meno di ripetere la sommatoria estesa a tutti i nodi
                 * a ogni passo del Newton-Rapson... */

                /* linearizzazione delle forze centrifughe e di Coriolis
                 * in base modale (anche questi termini dovrebbero essere
                 * trascurabili) */
#if 0
                for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                        Inv4j = pInv4->GetVec(jMode);
                        VInv5jaj = Inv5jaj.GetVec(jMode);
                        VInv5jaPj = Inv5jaPj.GetVec(jMode);
                        unsigned int jOffset = (jMode - 1)*3 + 1;
                        Inv8jTranspose = (pInv8->GetMat3x3(jOffset)).Transpose();
                        if (pInv9 != 0 ) {
                                Inv9jkak = 0.;
                                for (unsigned int kModem1 = 0; kModem1 < NModes; kModem1++)  {
                                        doublereal a_kMode = a.dGet(kModem1 + 1);
                                        integer iOffset = (jMode - 1)*3*NModes + kModem1*3 + 1;
                                        Inv9jkak += pInv9->GetMat3x3ScalarMult(iOffset, a_kMode);
                                }
                        }
                        for (int iCnt = 1; iCnt <= 3; iCnt++) {
                                doublereal temp1 = 0., temp2 = 0.;
                                for (int jCnt = 1; jCnt<=3; jCnt++) {
                                        if (pInv9 != 0) {
                                                temp1 += -2*wr.dGet(jCnt)*(R*(Inv8jTranspose - Inv9jkak)*RT).dGet(iCnt, jCnt);
                                        } else {
                                                temp1 += -2*wr.dGet(jCnt)*(R*(Inv8jTranspose)*RT).dGet(iCnt, jCnt);
                                        }
                                        temp2 += -(R*(Inv4j + VInv5jaj)).dGet(jCnt)*wrWedge.dGet(iCnt, jCnt);
                                }
                                WM.IncCoef(iRigidOffset + NModes + jMode, 9 + iCnt,
                                                dCoef*(temp1 + temp2));
                        }
                        for (int iCnt = 1; iCnt<=3; iCnt++) {
                                doublereal temp1 = 0.;
                                for (int jCnt = 1; jCnt<=3; jCnt++) {
                                        temp1 += (R*VInv5jaPj*2).dGet(jCnt);
                                }
                                WM.IncCoef(iRigidOffset + NModes + jMode, 9 + iCnt,
                                                dCoef*temp1);
                        }
                }
#endif
        }

        /* ciclo esteso a tutti i nodi d'interfaccia */
        for (unsigned int iStrNode = 1; iStrNode <= NStrNodes; iStrNode++) {
                unsigned int iStrNodem1 = iStrNode - 1;

                /* recupero le forme modali del nodo vincolato */
                Mat3xN PHIt(NModes), PHIr(NModes);
                for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                        integer iOffset = (jMode - 1)*NStrNodes + iStrNode;

                        PHIt.PutVec(jMode, oPHIt.GetVec(iOffset));
                        PHIr.PutVec(jMode, oPHIr.GetVec(iOffset));
                }

                MatNx3 PHItT(NModes), PHIrT(NModes);
                PHItT.Transpose(PHIt);
                PHIrT.Transpose(PHIr);

                /* nota: le operazioni
                 * d1tot = d1 + PHIt*a, R1tot = R*[I + (PHIr*a)/\]
                 * sono gia' state fatte da AssRes */

                Mat3xN SubMat1(NModes), SubMat2(NModes);
                MatNx3 SubMat3(NModes);
                MatNxN SubMat4(NModes);

                /* cerniera sferica */
                /* F e' aggiornata da AssRes */

                integer iReactionIndex = iRigidOffset + 2*NModes + 6*iStrNodem1;
                integer iStrNodeIndex = iRigidOffset + iNumDof + 6*iStrNodem1;

                Mat3x3 FTmpWedge(MatCross, SND[iStrNodem1].F*dCoef);

                Mat3xN PHItR(NModes);
                PHItR.LeftMult(R, PHIt);

                for (int iCnt = 1; iCnt <= 3; iCnt++) {
                        /* termini di reazione sui nodi */
                        WM.DecCoef(iStrNodeIndex + iCnt, iReactionIndex + iCnt, 1.);

                        /* termini di vincolo dovuti ai nodi */
                        WM.DecCoef(iReactionIndex + iCnt, iStrNodeIndex + iCnt, 1.);
                }

                if (pModalNode) {
                        for (int iCnt = 1; iCnt <= 3; iCnt++) {
                                /* termini di reazione sul nodo modale */
                                WM.IncCoef(6 + iCnt, iReactionIndex + iCnt, 1.);

                                /* termini di vincolo dovuti al nodo modale */
                                WM.IncCoef(iReactionIndex + iCnt, iCnt, 1.);
                        }

                        /* pd1Tot e' il puntatore all'array
                         * che contiene le posizioni del nodi FEM */
                        Mat3x3 dTmp1Wedge(MatCross, SND[iStrNodem1].d1tot);

                        WM.Add(9 + 1, iReactionIndex + 1, dTmp1Wedge);

                        /* termini del tipo: c*F/\*d/\*Deltag */
                        WM.Add(9 + 1, 3 + 1, FTmpWedge*dTmp1Wedge);

                        /* termini di vincolo dovuti ai nodi */
                        WM.Sub(iReactionIndex + 1, 3 + 1, dTmp1Wedge);

                        /* termini aggiuntivi dovuti alla deformabilita' */
                        SubMat3.RightMult(PHItT, RT*FTmpWedge);
                        WM.Add(iRigidOffset + NModes + 1, 3 + 1, SubMat3);

                        SubMat1.LeftMult(FTmpWedge, PHItR);
                        WM.Sub(9 + 1, iRigidOffset + 1, SubMat1);
                }

                /* contributo dovuto alla flessibilita' */
                WM.Add(iReactionIndex + 1, iRigidOffset + 1, PHItR);

                /* idem per pd2 e pR2 */
                Mat3x3 dTmp2Wedge(MatCross, SND[iStrNodem1].R2*SND[iStrNodem1].OffsetMB);
                WM.Sub(iStrNodeIndex + 3 + 1, iReactionIndex + 1, dTmp2Wedge);

                /* termini del tipo: c*F/\*d/\*Deltag */
                WM.Sub(iStrNodeIndex + 3 + 1, iStrNodeIndex + 3 + 1,
                                FTmpWedge*dTmp2Wedge);

                /* termini aggiuntivi dovuti alla deformabilita' */
                SubMat3.RightMult(PHItT, RT);
                WM.Add(iRigidOffset + NModes + 1, iReactionIndex + 1, SubMat3);

                /* modifica: divido le equazioni di vincolo per dCoef */

                /* termini di vincolo dovuti al nodo 2 */
                WM.Add(iReactionIndex + 1, iStrNodeIndex + 3 + 1, dTmp2Wedge);

                /* fine cerniera sferica */

                /* equazioni di vincolo : giunto prismatico */

                /* Vec3 M(XCurr, iModalIndex + 2*NModes + 6*iStrNodem1 + 3 + 1); */
                Vec3 MTmp = SND[iStrNodem1].R2*(SND[iStrNodem1].M*dCoef);
                Mat3x3 MTmpWedge(MatCross, MTmp);

                /* Eq. dei momenti */
                if (pModalNode) {
                        WM.Sub(9 + 1, iStrNodeIndex + 3 + 1, MTmpWedge);
                }
                WM.Add(iStrNodeIndex + 3 + 1, iStrNodeIndex + 3 + 1, MTmpWedge);

                /* Eq. d'equilibrio ai modi */
                SubMat3.RightMult(PHIrT, R*MTmpWedge);
                if (pModalNode) {
                        WM.Add(iRigidOffset + NModes + 1, 3 + 1, SubMat3);
                }
                WM.Sub(iRigidOffset + NModes + 1, iStrNodeIndex + 3 + 1, SubMat3);

                /* */
                if (pModalNode) {
                        WM.AddT(iReactionIndex + 3 + 1, 3 + 1, SND[iStrNodem1].R2);
                        WM.Add(9 + 1, iReactionIndex + 3 + 1, SND[iStrNodem1].R2);
                }
                WM.SubT(iReactionIndex + 3 + 1, iStrNodeIndex + 3 + 1, SND[iStrNodem1].R2);
                WM.Sub(iStrNodeIndex + 3 + 1, iReactionIndex + 3 + 1, SND[iStrNodem1].R2);

                /* */
                SubMat3.RightMult(PHIrT, RT*SND[iStrNodem1].R2);
                WM.Add(iRigidOffset + NModes + 1, iReactionIndex + 3 + 1, SubMat3);
                for (unsigned iMode = 1; iMode <= NModes; iMode++) {
                        for (unsigned jIdx = 1; jIdx <= 3; jIdx++) {
                                WM.IncCoef(iReactionIndex + 3 + jIdx, iRigidOffset + iMode,
                                        SubMat3(iMode, jIdx));
                        }
                }
        }

        return WorkMat;
}

SubVectorHandler&
Modal::AssRes(SubVectorHandler& WorkVec,
                doublereal dCoef,
                const VectorHandler& XCurr,
                const VectorHandler& XPrimeCurr)
{
        DEBUGCOUT("Entering Modal::AssRes()" << std::endl);

        integer iNumRows;
        integer iNumCols;

        WorkSpaceDim(&iNumRows, &iNumCols);
        WorkVec.ResizeReset(iNumRows);

        /*
         * Equations:
         *
         * 1		-> 12:		rigid body eq.
         *
         * 13		-> 12 + 2*NM:	modes eq. (\dot{a}=b); m\dot{b} + ka=f)
         *
         * 12 + 2*NM	-> 12 + 2*NM + 6*NN:		node constraints
         *
         * 12 + 2*NM + 6*NN	-> 12 + 2*NM + 6*NN + 6*NN: constr. nodes eq.
         *
         *
         * Unknowns:
         *
         * rigid body:			from modal node
         * modes:			iGetFirstIndex()
         * node reactions:		iGetFirstIndex() + 2*NM
         * nodes:			from constraint nodes
         */

        /* modal dofs and node constraints indices */
        integer iModalIndex = iGetFirstIndex();
        unsigned int iNumDof = iGetNumDof();
        for (unsigned int iCnt = 1; iCnt <= iNumDof; iCnt++) {
                WorkVec.PutRowIndex(iRigidOffset + iCnt, iModalIndex + iCnt);
        }

        /* interface nodes equilibrium indices */
        integer iOffset = iRigidOffset + iNumDof;
        for (unsigned iStrNodem1 = 0; iStrNodem1 < NStrNodes; iStrNodem1++) {
                integer iNodeFirstMomIndex =
                        SND[iStrNodem1].pNode->iGetFirstMomentumIndex();

                for (unsigned int iCnt = 1; iCnt <= 6; iCnt++) {
                        WorkVec.PutRowIndex(iOffset + iCnt,
                                        iNodeFirstMomIndex + iCnt);
                }

                iOffset += 6;
        }

        a.Copy(XCurr, iModalIndex + 1);
        aPrime.Copy(XPrimeCurr, iModalIndex + 1);
        b.Copy(XCurr, iModalIndex + NModes + 1);
        bPrime.Copy(XPrimeCurr, iModalIndex + NModes + 1);

        VecN Ka(NModes), CaP(NModes), MaPP(NModes);

        /*
         * aggiornamento invarianti
         */
        Ka.Mult(oModalStiff, a);
        CaP.Mult(oModalDamp, b);
        MaPP.Mult(oModalMass, bPrime);

#if 0
        std::cerr << "### Stiff" << std::endl;
        for (unsigned int i = 1; i <= NModes; i++) {
                for (unsigned int j = 1; j <= NModes; j++) {
                        std::cerr << std::setw(2) << i << ", "
                                << std::setw(2) << j << " "
                                << std::setw(20) << (*pModalStiff)(i,j)
                                << std::endl;

                }
        }
        std::cerr << "### Damp" << std::endl;
        for (unsigned int i = 1; i <= NModes; i++) {
                for (unsigned int j = 1; j <= NModes; j++) {
                        std::cerr << std::setw(2) << i << ", "
                                << std::setw(2) << j << " "
                                << std::setw(20) << (*pModalDamp)(i,j)
                                << std::endl;
                }
        }
        std::cerr << "### Mass" << std::endl;
        for (unsigned int i = 1; i <= NModes; i++) {
                for (unsigned int j = 1; j <= NModes; j++) {
                        std::cerr << std::setw(2) << i << ", "
                                << std::setw(2) << j << " "
                                << std::setw(20) << (*pModalMass)(i,j)
                                << std::endl;
                }
        }
        std::cerr << "### a" << std::endl;
        for (unsigned int i = 1; i <= NModes; i++) {
                std::cerr << std::setw(2) << i <<  " "
                        << std::setw(20) << a(i) << std::endl;
        }
        std::cerr << "### b" << std::endl;
        for (unsigned int i = 1; i <= NModes; i++) {
                std::cerr << std::setw(2) << i <<  " "
                        << std::setw(20) << b(i) << std::endl;
        }
        std::cerr << "### bP" << std::endl;
        for (unsigned int i = 1; i <= NModes; i++) {
                std::cerr << std::setw(2) << i <<  " "
                        << std::setw(20) << bPrime(i) << std::endl;
        }
#endif

        Vec3 Inv3jaPPj(::Zero3);
        if (oInv3.iGetNumCols()) {
                Inv3jaj = oInv3 * a;
                Inv3jaPj = oInv3 * b;
                Inv3jaPPj = oInv3 * bPrime;
        }

        /* invarianti rotazionali */
        if (oInv8.iGetNumCols()) {
                Inv8jaj.Reset();
                Inv8jaPj.Reset();
        }
        if (oInv5.iGetNumCols()) {
                Inv5jaj.Reset(0.);
                Inv5jaPj.Reset(0.);
        }

        // Mat3x3 MatTmp1(::Zero3x3), MatTmp2(::Zero3x3);

        if (oInv9.iGetNumCols()) {
                Inv9jkajak.Reset();
                Inv9jkajaPk.Reset();
        }
        Mat3x3 Inv10jaPj(::Zero3x3);

        if (oInv5.iGetNumCols() || oInv8.iGetNumCols() || oInv9.iGetNumCols() || oInv10.iGetNumCols()) {
                for (unsigned int jMode = 1; jMode <= NModes; jMode++)  {
                        doublereal a_jMode = a(jMode);
                        doublereal aP_jMode = b(jMode);

                        if (oInv5.iGetNumCols()) {
                                for (unsigned int kMode = 1; kMode <= NModes; kMode++) {
                                        Vec3 v = oInv5.GetVec((jMode - 1)*NModes + kMode);

                                        Inv5jaj.AddVec(kMode, v*a_jMode);
                                        Inv5jaPj.AddVec(kMode, v*aP_jMode);
                                }
                        }

                        if (oInv8.iGetNumCols()) {
                                Mat3x3 Inv8jajTmp;

                                for (unsigned int jCnt = 1; jCnt <= 3; jCnt++) {
                                        unsigned int iMjC = (jMode - 1)*3 + jCnt;

                                        Inv8jajTmp.PutVec(jCnt, oInv8.GetVec(iMjC));
                                }

                                Inv8jaj += Inv8jajTmp * a_jMode;
                                Inv8jaPj += Inv8jajTmp * aP_jMode;

                                if (oInv9.iGetNumCols()) {
                                        /*
                                         * questi termini si possono commentare perche' sono
                                         * (sempre ?) piccoli (termini del tipo a*a o a*b)
                                         * eventualmente dare all'utente la possibilita'
                                         * di scegliere se trascurarli o no
                                         */
                                        for (unsigned int kMode = 1; kMode <= NModes; kMode++) {
                                                doublereal a_kMode = a(kMode);
                                                doublereal aP_kMode = b(kMode);
                                                unsigned int iOffset = (jMode - 1)*3*NModes + (kMode - 1)*3 + 1;
                                                Inv9jkajak += oInv9.GetMat3x3ScalarMult(iOffset, a_jMode*a_kMode);
                                                Inv9jkajaPk += oInv9.GetMat3x3ScalarMult(iOffset, a_jMode*aP_kMode);
                                        }
                                }
                        }

                        if (oInv10.iGetNumCols()) {
                                Mat3x3 Inv10jaPjTmp;

                                for (unsigned int jCnt = 1; jCnt <= 3; jCnt++) {
                                        unsigned int iMjC = (jMode - 1)*3 + jCnt;

                                        Inv10jaPjTmp.PutVec(jCnt, oInv10.GetVec(iMjC)*aP_jMode);
                                }


                                Inv10jaPj += Inv10jaPjTmp;
                        }
                }
        } /* fine ciclo sui modi */

#ifdef MODAL_USE_GRAVITY
        /* forza di gravita' (decidere come inserire g) */
        /* FIXME: use a reasonable reference point where compute gravity */
        Vec3 GravityAcceleration(::Zero3);
        bool bGravity = GravityOwner::bGetGravity(x, GravityAcceleration);
#endif /* MODAL_USE_GRAVITY */

        Vec3 vP(::Zero3);
        Vec3 wP(::Zero3);
        Vec3 w(::Zero3);
        Vec3 RTw(::Zero3);
        if (pModalNode) {
                /* rigid body indices */
                integer iRigidIndex = pModalNode->iGetFirstIndex();
                for (unsigned int iCnt = 1; iCnt <= iRigidOffset; iCnt++) {
                        WorkVec.PutRowIndex(iCnt, iRigidIndex + iCnt);
                }

                /* recupera i dati necessari */
                x = pModalNode->GetXCurr();
                vP = pModalNode->GetXPPCurr();
                wP = pModalNode->GetWPCurr();
                w = Vec3(XCurr, iRigidIndex + 9 + 1);

                R = pModalNode->GetRCurr();
                RT = R.Transpose();
                RTw = RT*w;

                Mat3x3 JTmp = Inv7;
                if (oInv8.iGetNumCols()) {
                        JTmp += Inv8jaj.Symm2();
                        if (oInv9.iGetNumCols()) {
                                JTmp -= Inv9jkajak;
                        }
                }
                Mat3x3 J = R*JTmp*RT;
                Vec3 STmp = Inv2;
                if (oInv3.iGetNumCols()) {
                        STmp += Inv3jaj;
                }
                Vec3 S = R*STmp;

                /*
                 * fine aggiornamento invarianti
                 */

                /* forze d'inerzia */
                Vec3 FTmp = vP*dMass - S.Cross(wP) + w.Cross(w.Cross(S));
                if (oInv3.iGetNumCols()) {
                        FTmp += R*Inv3jaPPj + (w.Cross(R*Inv3jaPj))*2.;
                }
                WorkVec.Sub(6 + 1, FTmp);
#if 0
                std::cerr << "m=" << dMass << "; S=" << S
                        << "; a="  << a << "; aPrime =" << aPrime
                        << "; b=" << b <<  "; bPrime= " << bPrime
                        << "; tot=" << vP*dMass - S.Cross(wP) + w.Cross(w.Cross(S))
                        + (w.Cross(R*Inv3jaPj))*2 + R*Inv3jaPPj << std::endl;
#endif

                Vec3 MTmp = S.Cross(vP) + J*wP + w.Cross(J*w);
                if (oInv4.iGetNumCols()) {
                        Mat3xN Inv4Curr(NModes, 0);
                        Inv4Curr.LeftMult(R, oInv4);
                        MTmp += Inv4Curr*bPrime;
                }
                if (oInv5.iGetNumCols()) {
                        Mat3xN Inv5jajCurr(NModes, 0);
                        Inv5jajCurr.LeftMult(R, Inv5jaj);
                        MTmp += Inv5jajCurr*bPrime;
                }
                if (oInv8.iGetNumCols()) {
                        Mat3x3 Tmp = Inv8jaPj;
                        if (oInv9.iGetNumCols()) {
                                Tmp -= Inv9jkajaPk;
                        }
                        MTmp += R*Tmp*(RTw*2.);
                }
                /* termini dovuti alle inerzie rotazionali */
                if (oInv10.iGetNumCols()) {
                        Vec3 VTmp = Inv10jaPj.Symm2()*RTw;
                        if (oInv11.iGetNumCols()) {
                                VTmp += w.Cross(R*(oInv11*b));
                        }
                        MTmp += R*VTmp;
                }
                WorkVec.Sub(9 + 1, MTmp);

#ifdef MODAL_USE_GRAVITY
                /* forza di gravita' (decidere come inserire g) */
                if (bGravity) {
                        WorkVec.Add(6 + 1, GravityAcceleration*dMass);
                        WorkVec.Add(9 + 1, S.Cross(GravityAcceleration));
                }
#endif /* MODAL_USE_GRAVITY */
        }

        /* forze modali */
        for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                unsigned int jOffset = (jMode - 1)*3 + 1;
                doublereal d = - MaPP(jMode) - CaP(jMode) - Ka(jMode);

                if (oInv3.iGetNumCols()) {
                        Vec3 Inv3j = oInv3.GetVec(jMode);

                        d -= (R*Inv3j).Dot(vP);

#ifdef MODAL_USE_GRAVITY
                        /* forza di gravita': */
                        if (bGravity) {
                                WorkVec.IncCoef(iRigidOffset + NModes + jMode,
                                                (R*Inv3j).Dot(GravityAcceleration));
                        }
#endif /* MODAL_USE_GRAVITY */
                }

                if (oInv4.iGetNumCols()) {
                        Vec3 Inv4j = oInv4.GetVec(jMode);
                        if (oInv5.iGetNumCols()) {
                                Inv4j += Inv5jaj.GetVec(jMode);

                                d -= ((R*Inv5jaPj.GetVec(jMode)).Dot(w))*2.;
                        }

                        d -= (R*Inv4j).Dot(wP);
                }

                if (oInv8.iGetNumCols() || oInv9.iGetNumCols() || oInv10.iGetNumCols()) {
                        Mat3x3 MTmp(::Zero3x3);

                        if (oInv8.iGetNumCols()) {
                                MTmp += oInv8.GetMat3x3(jOffset).Transpose();
                                if (oInv9.iGetNumCols()) {
                                        for (unsigned int kModem1 = 0; kModem1 < NModes; kModem1++) {
                                                doublereal a_kMode = a(kModem1 + 1);
                                                integer kOffset = (jMode - 1)*3*NModes + kModem1*3 + 1;

                                                MTmp -= oInv9.GetMat3x3ScalarMult(kOffset, a_kMode);
                                        }
                                }
                        }

                        if (oInv10.iGetNumCols()) {
                                MTmp += oInv10.GetMat3x3(jOffset);
                        }

                        d += w.Dot(R*(MTmp*RTw));
                }

                WorkVec.IncCoef(iRigidOffset + NModes + jMode, d);

#if 0
                std::cerr << "(R*Inv3j).Dot(vP)=" << (R*Inv3j).Dot(vP)
                        << std::endl
                        << "(R*(Inv4j + VInv5jaj)).Dot(wP)="
                        << (R*(Inv4j + VInv5jaj)).Dot(wP) << std::endl
                        << "w.Dot(R*((Inv8jTranspose + Inv10j)*RTw))"
                        << w.Dot(R*((Inv8jTranspose + Inv10j)*RTw)) << std::endl
                        << " -(R*VInv5jaPj).Dot(w)*2."
                        << -(R*VInv5jaPj).Dot(w)*2.<< std::endl
                        << " -CaP.dGet(jMode)" << -CaP.dGet(jMode)<< std::endl
                        << "-Ka.dGet(jMode) "
                        << -CaP.dGet(jMode) - Ka.dGet(jMode) << std::endl;
#endif
        }

        /* equazioni per abbassare di grado il sistema */
        for (unsigned int iCnt = 1; iCnt <= NModes; iCnt++) {
                WorkVec.PutCoef(iRigidOffset + iCnt, b(iCnt) - aPrime(iCnt));
        }

        /* equazioni di vincolo */
        for (unsigned int iStrNode = 1; iStrNode <= NStrNodes; iStrNode++) {
                unsigned int iStrNodem1 = iStrNode - 1;
                integer iReactionIndex = iRigidOffset + 2*NModes + 6*iStrNodem1;
                integer iStrNodeIndex = iReactionIndex + 6*NStrNodes;

                /* recupero le forme modali del nodo vincolato */
                /* FIXME: what about using Blitz++ ? :) */
                Mat3xN PHIt(NModes), PHIr(NModes);
                for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                        integer iOffset = (jMode - 1)*NStrNodes + iStrNode;
                        PHIt.PutVec(jMode, oPHIt.GetVec(iOffset));
                        PHIr.PutVec(jMode, oPHIr.GetVec(iOffset));
                }

                /*
                 * aggiorno d1 e R con il contributo dovuto
                 * alla flessibilita':
                 * d1tot = R*[d1 + PHIt*a], R1tot = R*[I + (PHIr*a)/\]
                 */
                SND[iStrNodem1].d1tot = R*(SND[iStrNodem1].OffsetFEM + PHIt*a);
                //SND[iStrNodem1].R1tot = R*Mat3x3(1., PHIr*a);
                SND[iStrNodem1].R1tot = R*(Eye3 + Mat3x3(MatCross, PHIr*a));

                /* constraint reaction (force) */
                SND[iStrNodem1].F = Vec3(XCurr,
                                iModalIndex + 2*NModes + 6*iStrNodem1 + 1);
                const Vec3& x2 = SND[iStrNodem1].pNode->GetXCurr();

                /* FIXME: R2??? */
                SND[iStrNodem1].R2 = SND[iStrNodem1].pNode->GetRCurr();

                /* cerniera sferica */
                Vec3 dTmp1(SND[iStrNodem1].d1tot);
                Vec3 dTmp2(SND[iStrNodem1].R2*SND[iStrNodem1].OffsetMB);

                /* Eq. d'equilibrio, nodo 1 */
                if (pModalNode) {
                        WorkVec.Sub(6 + 1, SND[iStrNodem1].F);
                        WorkVec.Sub(9 + 1, dTmp1.Cross(SND[iStrNodem1].F));
                }

                /* termine aggiuntivo dovuto alla deformabilita':
                 * -PHItiT*RT*F */
                Vec3 vtemp = RT*SND[iStrNodem1].F;
                for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                        /* PHItTranspose(jMode) * vtemp */
                        WorkVec.DecCoef(iRigidOffset + NModes + jMode,
                                        PHIt.GetVec(jMode)*vtemp);
                }

                /* Eq. d'equilibrio, nodo 2 */
                WorkVec.Add(iStrNodeIndex + 1, SND[iStrNodem1].F);
                WorkVec.Add(iStrNodeIndex + 3 + 1, dTmp2.Cross(SND[iStrNodem1].F));

                /* Eq. di vincolo */
                ASSERT(dCoef != 0.);
                WorkVec.Add(iReactionIndex + 1, (x2 + dTmp2 - x - dTmp1)/dCoef);

                /* constraint reaction (moment) */
                SND[iStrNodem1].M = Vec3(XCurr,
                                iModalIndex + 2*NModes + 6*iStrNodem1 + 3 + 1);

                /* giunto prismatico */
                Vec3 ThetaCurr = RotManip::VecRot(SND[iStrNodem1].R2.MulTM(SND[iStrNodem1].R1tot));
                Vec3 MTmp = SND[iStrNodem1].R2*SND[iStrNodem1].M;

                /* Equazioni di equilibrio, nodo 1 */
                if (pModalNode) {
                        WorkVec.Sub(9 + 1, MTmp);
                }

                /* Equazioni di equilibrio, nodo 2 */
                WorkVec.Add(iStrNodeIndex + 3 + 1, MTmp);

                /* Contributo dovuto alla flessibilita' :-PHIrT*RtotT*M */
                vtemp = RT*MTmp;
                for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                        /* PHIrTranspose(jMode) * vtemp */
                        WorkVec.DecCoef(iRigidOffset + NModes + jMode,
                                        PHIr.GetVec(jMode)*vtemp);
                }

                /* Modifica: divido le equazioni di vincolo per dCoef */
                ASSERT(dCoef != 0.);
                /* Equazioni di vincolo di rotazione */
                WorkVec.Sub(iReactionIndex + 3 + 1, ThetaCurr/dCoef);
        }

#if 0
        std::cerr << "###" << std::endl;
        for (int i = 1; i <= WorkVec.iGetSize(); i++) {
                std::cerr << std::setw(2) << i << "("
                        << std::setw(2) << WorkVec.iGetRowIndex(i) << ")"
                        << std::setw(20) << WorkVec(i) << std::endl;
        }
#endif
        return WorkVec;
}

void
Modal::OutputPrepare(OutputHandler &OH)
{
        if (bToBeOutput()) {
#ifdef USE_NETCDF
                if (OH.UseNetCDF(OutputHandler::MODAL)) {
                        ASSERT(OH.IsOpen(OutputHandler::NETCDF));

                        using namespace std::string_literals;

                        const std::string prefix = "elem.joint."s + std::to_string(GetLabel());

                        OutputHandler::NcDimVec dimNModes(2);

                        dimNModes[0] = OH.DimTime();
                        dimNModes[1] = OH.CreateDim(prefix + "_NModes", NModes);

                        OutputHandler::AttrValVec attrsA(3);

                        attrsA[0] = OutputHandler::AttrVal("units", "-");
                        attrsA[1] = OutputHandler::AttrVal("type", "doublereal");
                        attrsA[2] = OutputHandler::AttrVal("description", "modal displacements");

                        OutputHandler::AttrValVec attrsAPrime(attrsA);
                        attrsAPrime[2] = OutputHandler::AttrVal("description", "modal velocities");

                        OutputHandler::AttrValVec attrsBPrime(attrsA);
                        attrsBPrime[2] = OutputHandler::AttrVal("description", "modal accelerations");

                        Var_a = OH.CreateVar(prefix + ".a", MbNcDouble, attrsA, dimNModes);
                        Var_aPrime = OH.CreateVar(prefix + ".aPrime", MbNcDouble, attrsAPrime, dimNModes);
                        Var_bPrime = OH.CreateVar(prefix + ".bPrime", MbNcDouble, attrsBPrime, dimNModes);
                }
#endif // USE_NETCDF
        }
}

void
Modal::Output(OutputHandler& OH) const
{
        if (bToBeOutput()) {
             if (OH.UseText(OutputHandler::MODAL)) {
                /* stampa sul file di output i modi */
                std::ostream& out = OH.Modal();

                for (unsigned int iCnt = 1; iCnt <= NModes; iCnt++) {
                        // element.mode q q' q''
                        out << std::setw(8) << GetLabel() << '.' << uModeNumber[iCnt - 1]
                                << " " << a(iCnt)
                                << " " << b(iCnt)
                                << " " << bPrime(iCnt)
                                << std::endl;
                }

                // TODO: find a way to link modal coordinates of attached modes and the corresponding FEM node, and use FEM node's label

                std::vector<StrNodeData>::const_iterator i = SND.cbegin();
                std::vector<StrNodeData>::const_iterator end = SND.cend();
                for (; i != end; ++i) {
                        if (i->bOut) {
                                // element@node_label F M
                                out << std::setw(8) << GetLabel() << "@" << i->pNode->GetLabel()
                                        << " ", i->F.Write(out, " ")
                                        << " ", i->M.Write(out, " ")
                                        << std::endl;
                        }
                }
             }

#ifdef USE_NETCDF
             if (OH.UseNetCDF(OutputHandler::MODAL)) {
                  std::vector<size_t> start(2, 0), count(2, 1);
                  start[0] = OH.GetCurrentStep();
                  for (unsigned i = 0; i < NModes; ++i) {
                       start[1] = i;
                       OH.WriteNcVar(Var_a, a(i + 1), start, count);
                       OH.WriteNcVar(Var_aPrime, aPrime(i + 1), start, count);
                       OH.WriteNcVar(Var_bPrime, bPrime(i + 1), start, count);
                  }
             }
#endif
        }
}

unsigned int
Modal::iGetInitialNumDof(void) const
{
        return 2*NModes + 12*NStrNodes;
}

void
Modal::InitialWorkSpaceDim(integer* piNumRows, integer* piNumCols) const
{
        *piNumRows = *piNumCols = iRigidOffset + iGetInitialNumDof() + 12*NStrNodes;
}

/* Contributo allo jacobiano durante l'assemblaggio iniziale */
VariableSubMatrixHandler&
Modal::InitialAssJac(VariableSubMatrixHandler& WorkMat,
                const VectorHandler& XCurr)
{
        DEBUGCOUT("Entering Modal::InitialAssJac()" << std::endl);

        FullSubMatrixHandler& WM = WorkMat.SetFull();
        integer iNumRows = 0;
        integer iNumCols = 0;
        InitialWorkSpaceDim(&iNumRows, &iNumCols);
        WM.ResizeReset(iNumRows, iNumCols);

        if (pModalNode) {
                integer iRigidIndex = pModalNode->iGetFirstIndex();

                for (unsigned int iCnt = 1; iCnt <= iRigidOffset; iCnt++) {
                        WM.PutRowIndex(iCnt, iRigidIndex + iCnt);
                        WM.PutColIndex(iCnt, iRigidIndex + iCnt);
                }
        }

        integer iFlexIndex = iGetFirstIndex();

        for (unsigned int iCnt = 1; iCnt <= iGetInitialNumDof(); iCnt++) {
                WM.PutRowIndex(iRigidOffset + iCnt, iFlexIndex + iCnt);
                WM.PutColIndex(iRigidOffset + iCnt, iFlexIndex + iCnt);
        }

        for (unsigned iStrNodem1 = 0; iStrNodem1 < NStrNodes; iStrNodem1++) {
                integer iNodeFirstPosIndex =
                        SND[iStrNodem1].pNode->iGetFirstPositionIndex();
                integer iNodeFirstVelIndex = iNodeFirstPosIndex + 6;

                integer iOffset = iRigidOffset + iGetInitialNumDof() + 12*iStrNodem1;
                for (unsigned int iCnt = 1; iCnt <= 6; iCnt++) {
                        WM.PutRowIndex(iOffset + iCnt,
                                        iNodeFirstPosIndex + iCnt);
                        WM.PutColIndex(iOffset + iCnt,
                                        iNodeFirstPosIndex + iCnt);
                        WM.PutRowIndex(iOffset + 6 + iCnt,
                                        iNodeFirstVelIndex + iCnt);
                        WM.PutColIndex(iOffset + 6 + iCnt,
                                        iNodeFirstVelIndex + iCnt);
                }
        }

        /* comincia l'assemblaggio dello Jacobiano */

        /* nota: nelle prime iRigidOffset + 2*M equazioni
         * metto dei termini piccoli per evitare singolarita'
         * quando non ho vincoli esterni o ho azzerato i modi */
        for (unsigned int iCnt = 1; iCnt <= iRigidOffset + 2*NModes; iCnt++) {
                WM.PutCoef(iCnt, iCnt, 1.e-12);
        }

        /* forze elastiche e viscose */
        for (unsigned int iCnt = 1; iCnt <= NModes; iCnt++) {
                for (unsigned int jCnt = 1; jCnt <= NModes; jCnt++) {
                        WM.PutCoef(iRigidOffset + iCnt, iRigidOffset + jCnt,
                                   oModalStiff.dGet(iCnt, jCnt));
                        WM.PutCoef(iRigidOffset + iCnt, iRigidOffset + NModes + jCnt,
                                   oModalDamp.dGet(iCnt, jCnt));
                }
        }

        /* equazioni di vincolo */

        /* cerniera sferica */
        for (unsigned int iStrNode = 1; iStrNode <= NStrNodes; iStrNode++) {
                unsigned int iStrNodem1 = iStrNode - 1;

                /* recupera i dati */
                const Mat3x3& R2(SND[iStrNodem1].pNode->GetRRef());
                Vec3 Omega1(::Zero3);
                if (pModalNode) {
                        Omega1 = pModalNode->GetWRef();
                }
                const Vec3& Omega2(SND[iStrNodem1].pNode->GetWRef());
                Vec3 F(XCurr, iFlexIndex + 2*NModes + 12*iStrNodem1 + 1);
                Vec3 FPrime(XCurr, iFlexIndex + 2*NModes + 12*iStrNodem1 + 6 + 1);

                Mat3xN PHIt(NModes, 0), PHIr(NModes, 0);
                MatNx3 PHItT(NModes, 0), PHIrT(NModes, 0);

                for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                        PHIt.PutVec(jMode, oPHIt.GetVec((jMode - 1)*NStrNodes + iStrNode));
                        PHIr.PutVec(jMode, oPHIr.GetVec((jMode - 1)*NStrNodes + iStrNode));
                }

                PHItT.Transpose(PHIt);
                PHIrT.Transpose(PHIr);

                Vec3 d1tot = R*(SND[iStrNodem1].OffsetFEM + PHIt*a);
                //Mat3x3 R1tot = R*Mat3x3(1., PHIr*a);
                Mat3x3 R1tot = Eye3 + Mat3x3(MatCross, PHIr*a);
                Mat3xN SubMat1(NModes, 0.);
                MatNx3 SubMat2(NModes, 0.);

                integer iOffset1 = iRigidOffset + 2*NModes + 12*iStrNodem1;
                integer iOffset2 = iRigidOffset + iGetInitialNumDof() + 12*iStrNodem1;
                if (pModalNode) {
                        for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                                /* Contributo di forza all'equazione
                                 * della forza, nodo 1 */
                                WM.IncCoef(iCnt, iOffset1 + iCnt, 1.);

                                /* Contrib. di der. di forza all'eq. della der.
                                 * della forza, nodo 1 */
                                WM.IncCoef(6 + iCnt, iOffset1 + 6 + iCnt, 1.);

                                /* Equazione di vincolo, nodo 1 */
                                WM.DecCoef(iOffset1 + iCnt, iCnt, 1.);

                                /* Derivata dell'equazione di vincolo, nodo 1 */
                                WM.DecCoef(iOffset1 + 6 + iCnt, 6 + iCnt, 1.);
                        }
                }

                for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                        /* Equazione di vincolo, nodo 2 */
                        WM.IncCoef(iOffset1 + iCnt, iOffset2 + iCnt, 1.);

                        /* Derivata dell'equazione di vincolo, nodo 2 */
                        WM.IncCoef(iOffset1 + 6 + iCnt, iOffset2 + 6 + iCnt, 1.);

                        /* Contributo di forza all'equazione della forza,
                         * nodo 2 */
                        WM.DecCoef(iOffset2 + iCnt, iOffset1 + iCnt, 1.);

                        /* Contrib. di der. di forza all'eq. della der.
                         * della forza, nodo 2 */
                        WM.DecCoef(iOffset2 + 6 + iCnt, iOffset1 + 6 + iCnt, 1.);
                }

                /* Distanza nel sistema globale */
                const Vec3& d1Tmp(d1tot);
                Vec3 d2Tmp(R2*SND[iStrNodem1].OffsetMB);

                /* Matrici F/\d1/\, -F/\d2/\ , F/\omega1/\ */
                Mat3x3 FWedged1Wedge(MatCrossCross, F, d1Tmp);
                Mat3x3 FWedged2Wedge(MatCrossCross, F, d2Tmp);
                Mat3x3 FWedgeO1Wedge(MatCrossCross, F, Omega1);

                /* Matrici (omega1/\d1)/\, -(omega2/\d2)/\ */
                Mat3x3 O1Wedged1Wedge(MatCross, Omega1.Cross(d1Tmp));
                Mat3x3 O2Wedged2Wedge(MatCross, d2Tmp.Cross(Omega2));

                /* d1Prime= w1/\d1 + R*PHIt*b */
                Mat3xN R1PHIt(NModes);
                R1PHIt.LeftMult(R, PHIt);
                Vec3 d1Prime(Omega1.Cross(d1Tmp) + R1PHIt*b);

                Mat3x3 FWedge(MatCross, F);
                if (pModalNode) {
                        /* Equazione di momento, nodo 1 */
                        WM.Add(3 + 1, 3 + 1, FWedged1Wedge);
                        WM.Add(3 + 1, iOffset1 + 1, Mat3x3(MatCross, d1Tmp));

                        /* Equazione di momento, contributo modale */
                        SubMat1.LeftMult(FWedge*R, PHIt);
                        WM.Sub(3 + 1, iRigidOffset + 1, SubMat1);

                        /* Eq. di equilibrio ai modi */
                        SubMat2.RightMult(PHItT, RT*FWedge);
                        WM.Add(iRigidOffset + 1, 3 + 1, SubMat2);
                }

                /* Equazione di momento, nodo 2 */
                WM.Sub(iOffset2 + 3 + 1,iOffset2 + 3 + 1, FWedged2Wedge);
                WM.Sub(iOffset2 + 3 + 1,iOffset1 + 1, Mat3x3(MatCross, d2Tmp));

                /* Eq. di equilibrio ai modi */
                SubMat2.RightMult(PHItT, RT);
                WM.Add(iRigidOffset + 1, iRigidOffset + 2*NModes + 1, SubMat2);

                if (pModalNode) {
                        /* derivata dell'equazione di momento, nodo 1 */
                        WM.Add(9 + 1, 3 + 1,
                                Mat3x3(MatCrossCross, FPrime, d1Tmp) + F.Cross(Mat3x3(MatCrossCross, Omega1, d1Tmp))
                                + Mat3x3(MatCrossCross, F, R*(PHIt*b)));
                        WM.Add(9 + 1, 9 + 1, FWedged1Wedge);
                        WM.Add(9 + 1, iOffset1 + 1, O1Wedged1Wedge + Mat3x3(MatCross, R*(PHIt*b)));
                        WM.Add(9 + 1, iOffset1 + 6 + 1, Mat3x3(MatCross, d1Tmp));

                        /* derivata dell'equazione di momento, contributo modale */
                        SubMat1.LeftMult((-FWedgeO1Wedge - Mat3x3(MatCross, FPrime))*R, PHIt);
                        WM.Add(9 + 1, iRigidOffset + 1, SubMat1);
                        SubMat1.LeftMult(-FWedge*R, PHIt);
                        WM.Add(9 + 1, iRigidOffset + NModes + 1, SubMat1);

                        /* derivata dell'eq. di equilibrio ai modi */
                        SubMat2.RightMult(PHItT, RT*FWedge);
                        WM.Add(iRigidOffset + NModes + 1, 9 + 1, SubMat2);
                        SubMat2.RightMult(PHItT, RT*FWedgeO1Wedge);
                        WM.Add(iRigidOffset + NModes + 1, 3 + 1, SubMat2);
                }

                SubMat2.RightMult(PHItT, RT);
                WM.Add(iRigidOffset + NModes + 1, iRigidOffset + 2*NModes + 6 + 1, SubMat2);

                /* Derivata dell'equazione di momento, nodo 2 */
                WM.Sub(iOffset2 + 9 + 1, iOffset2 + 3 + 1,
                        Mat3x3(MatCrossCross, FPrime, d2Tmp)
                        + F.Cross(Mat3x3(MatCrossCross, Omega2, d2Tmp)));
                WM.Sub(iOffset2 + 9 + 1, iOffset2 + 9 + 1, FWedged2Wedge);
                WM.Add(iOffset2 + 9 + 1, iOffset1 + 1, O2Wedged2Wedge);
                WM.Sub(iOffset2 + 9 + 1, iOffset1 + 6 + 1, Mat3x3(MatCross, d2Tmp));

                /* Equazione di vincolo */
                if (pModalNode) {
                        WM.Add(iOffset1 + 1, 3 + 1, Mat3x3(MatCross, d1Tmp));
                }
                WM.Sub(iOffset1 + 1, iOffset2 + 3 + 1, Mat3x3(MatCross, d2Tmp));

                /* Equazione di vincolo, contributo modale */
                SubMat1.LeftMult(R, PHIt);
                WM.Sub(iOffset1 + 1, iRigidOffset + 1, SubMat1);

                /* Derivata dell'equazione di vincolo */
                if (pModalNode) {
                        WM.Add(iOffset1 + 6 + 1, 3 + 1, O1Wedged1Wedge + Mat3x3(MatCross, R*(PHIt*b)));
                        WM.Add(iOffset1 + 6 + 1, 9 + 1, Mat3x3(MatCross, d1Tmp));
                }
                WM.Add(iOffset1 + 6 + 1, iOffset2 + 3 + 1, O2Wedged2Wedge);
                WM.Sub(iOffset1 + 6 + 1, iOffset2 + 9 + 1, Mat3x3(MatCross, d2Tmp));

                /* Derivata dell'equazione di vincolo, contributo modale */
                SubMat1.LeftMult(Omega1.Cross(R), PHIt);
                WM.Sub(iOffset1 + 6 + 1, iRigidOffset + 1, SubMat1);
                SubMat1.LeftMult(R, PHIt);
                WM.Sub(iOffset1 + 6 + 1, iRigidOffset + NModes + 1, SubMat1);

                /* giunto prismatico */
                Vec3 M(XCurr, iFlexIndex + 2*NModes + 12*iStrNodem1 + 3 + 1);
                Vec3 MPrime(XCurr, iFlexIndex + 2*NModes + 12*iStrNodem1 + 9 + 1);
                Vec3 e1tota(R1tot.GetVec(1));
                Vec3 e2tota(R1tot.GetVec(2));
                Vec3 e3tota(R1tot.GetVec(3));
                Vec3 e1b(R2.GetVec(1));
                Vec3 e2b(R2.GetVec(2));
                Vec3 e3b(R2.GetVec(3));

                /* */
                Mat3x3 MWedge(Mat3x3(MatCrossCross, e3b, e2tota*M(1))
                        + Mat3x3(MatCrossCross, e1b, e3tota*M(2))
                        + Mat3x3(MatCrossCross, e2b, e1tota*M(3)));

                /* Equilibrio */
                if (pModalNode) {
                        WM.Add(3 + 1, 3 + 1, MWedge);
                        WM.SubT(3 + 1, iOffset2 + 3 + 1, MWedge);

                        WM.AddT(iOffset2 + 3 + 1, 3 + 1, MWedge);
                }
                WM.Sub(iOffset2 + 3 + 1, iOffset2 + 3 + 1, MWedge);

                /* Equilibrio dei momenti, termini aggiuntivi modali */
                Vec3 e1ra(R.GetVec(1));
                Vec3 e2ra(R.GetVec(2));
                Vec3 e3ra(R.GetVec(3));
                Mat3x3 M1Wedge(Mat3x3(MatCrossCross, e3b, e2ra*M(1))
                        + Mat3x3(MatCrossCross, e1b, e3ra*M(2))
                        + Mat3x3(MatCrossCross, e2b, e1ra*M(3)));

                SubMat1.LeftMult(M1Wedge, PHIr);
                if (pModalNode) {
                        WM.Add(3 + 1, iRigidOffset + 1, SubMat1);
                }
                WM.Sub(iOffset2 + 3 + 1, iRigidOffset + 1, SubMat1);

                /* Equilibrio ai modi */
                if (pModalNode) {
                        SubMat2.RightMult(PHIrT, R1tot.MulTM(MWedge));
                        WM.Add(iRigidOffset + 1, 3 + 1, SubMat2);
                }
                SubMat2.RightMult(PHIrT, R1tot.MulTMT(MWedge));
                WM.Sub(iRigidOffset + 1, iOffset2 + 3 + 1, SubMat2);

                Vec3 MA(Mat3x3(e2tota.Cross(e3b), e3tota.Cross(e1b),
                                        e1tota.Cross(e2b))*M);
                if (pModalNode) {
                        SubMat2.RightMult(PHIrT, R1tot.MulTM(Mat3x3(MatCross, MA)));
                        WM.Add(iRigidOffset + 1, 3 + 1, SubMat2);
                }

                Mat3x3 R1TMAWedge(MatCross, RT*MA);
                SubMat1.LeftMult(R1TMAWedge, PHIr);
                Mat3xN SubMat3(NModes);
                MatNxN SubMat4(NModes);
                SubMat3.LeftMult(R1tot.MulTM(M1Wedge), PHIr);
                SubMat1 += SubMat3;
                SubMat4.Mult(PHIrT, SubMat1);
                for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                        for (unsigned int kMode = 1; kMode <= NModes; kMode++) {
                                WM.IncCoef(iRigidOffset + jMode, iRigidOffset + kMode,
                                                SubMat4(jMode, kMode));
                        }
                }

                /* Derivate dell'equilibrio */
                if (pModalNode) {
                        WM.Add(9 + 1, 9 + 1, MWedge);
                        WM.SubT(9 + 1, iOffset2 + 9 + 1, MWedge);

                        WM.AddT(iOffset2 + 9 + 1, 9 + 1, MWedge);
                }
                WM.Sub(iOffset2 + 9 + 1, iOffset2 + 9 + 1, MWedge);

                MWedge = ((Mat3x3(MatCrossCross, e3b, Omega1) + Mat3x3(MatCross, Omega2.Cross(e3b*M(1))))
                        + Mat3x3(MatCross, e3b*MPrime(1)))*Mat3x3(MatCross, e2tota)
                        + ((Mat3x3(MatCrossCross, e1b, Omega1) + Mat3x3(MatCross, Omega2.Cross(e1b*M(2))))
                        + Mat3x3(MatCross, e1b*MPrime(2)))*Mat3x3(MatCross, e3tota)
                        + ((Mat3x3(MatCrossCross, e2b, Omega1) + Mat3x3(MatCross, Omega2.Cross(e2b*M(3))))
                        + Mat3x3(MatCross, e2b*MPrime(3)))*Mat3x3(MatCross, e1tota);

                if (pModalNode) {
                        WM.Add(9 + 1, 3 + 1, MWedge);
                        WM.Sub(iOffset2 + 9 + 1, 3 + 1, MWedge);
                }

                MWedge = ((Mat3x3(MatCrossCross, e2tota, Omega2) + Mat3x3(MatCross, Omega1.Cross(e2tota*M(1))))
                        + Mat3x3(MatCross, e2tota*MPrime(1)))*Mat3x3(MatCross, e3b)
                        + ((Mat3x3(MatCrossCross, e3tota, Omega2) + Mat3x3(MatCross, Omega1.Cross(e3tota*M(2))))
                        + Mat3x3(MatCross, e3tota*MPrime(2)))*Mat3x3(MatCross, e1b)
                        + ((Mat3x3(MatCrossCross, e1tota, Omega2) + Mat3x3(MatCross, Omega1.Cross(e1tota*M(3))))
                        + Mat3x3(MatCross, e1tota*MPrime(3)))*Mat3x3(MatCross, e2b);

                if (pModalNode) {
                        WM.Sub(9 + 1, iOffset2 + 3 + 1, MWedge);
                }
                WM.Add(iOffset2 + 9 + 1, iOffset2 + 3 + 1, MWedge);

                /* Derivate dell'equilibrio, termini aggiuntivi modali */
                SubMat1.LeftMult(M1Wedge, PHIr);
                if (pModalNode) {
                        WM.Add(9 + 1, iRigidOffset + NModes + 1, SubMat1);
                }
                WM.Sub(iOffset2 + 9 + 1, iRigidOffset + NModes + 1, SubMat1);

                Vec3 v1(e2tota.Cross(e3b));
                Vec3 v2(e3tota.Cross(e1b));
                Vec3 v3(e1tota.Cross(e2b));

                /* Error handling: il programma si ferma,
                 * pero' segnala dov'e' l'errore */
                if (v1.Dot() < std::numeric_limits<doublereal>::epsilon()
                                || v2.Dot() < std::numeric_limits<doublereal>::epsilon()
                                || v3.Dot() < std::numeric_limits<doublereal>::epsilon())
                {
                        silent_cerr("Modal(" << GetLabel() << "):"
                                << "warning, first node hinge axis "
                                "and second node hinge axis "
                                "are (nearly) orthogonal; aborting..."
                                << std::endl);
                        throw ErrNullNorm(MBDYN_EXCEPT_ARGS);
                }

                MWedge = Mat3x3(v1, v2, v3);

                /* Equilibrio */
                if (pModalNode) {
                        WM.Add(3 + 1, iOffset1 + 3 + 1, MWedge);
                }
                WM.Sub(iOffset2 + 3 + 1, iOffset1 + 3 + 1, MWedge);

                SubMat2.RightMult(PHIrT, R1tot.MulTM(MWedge));
                WM.Add(iRigidOffset + 1, iOffset1 + 3 + 1, SubMat2);

                /* Derivate dell'equilibrio */
                if (pModalNode) {
                        WM.Add(9 + 1, iOffset1 + 9 + 1, MWedge);
                }
                WM.Sub(iOffset2 + 9 + 1, iOffset1 + 9 + 1, MWedge);

                // NOTE: from this point on, MWedge = MWedge.Transpose();

                /* Equaz. di vincolo */
                if (pModalNode) {
                        WM.AddT(iOffset1 + 3 + 1, 3 + 1, MWedge);
                }
                WM.SubT(iOffset1 + 3 + 1, iOffset2 + 3 + 1, MWedge);

                /* Equaz. di vincolo: termine aggiuntivo
                 * dovuto alla flessibilita' */
                Vec3 u1(e2ra.Cross(e3b));
                Vec3 u2(e3ra.Cross(e1b));
                Vec3 u3(e1ra.Cross(e2b));

                M1Wedge = (Mat3x3(u1, u2, u3)).Transpose();
                SubMat1.LeftMult(M1Wedge, PHIr);
                WM.Add(iOffset1 + 3 + 1, iRigidOffset + 1, SubMat1);

                /* Derivate delle equaz. di vincolo */
                if (pModalNode) {
                        WM.AddT(iOffset1 + 9 + 1, 9 + 1, MWedge);
                }
                WM.SubT(iOffset1 + 9 + 1, iOffset2 + 9 + 1, MWedge);

                v1 = e3b.Cross(e2tota.Cross(Omega1))
                        + e2tota.Cross(Omega2.Cross(e3b));
                v2 = e1b.Cross(e3tota.Cross(Omega1))
                        + e3tota.Cross(Omega2.Cross(e1b));
                v3 = e2b.Cross(e1tota.Cross(Omega1))
                        + e1tota.Cross(Omega2.Cross(e2b));

                MWedge = Mat3x3(v1, v2, v3);

                /* Derivate dell'equilibrio */
                if (pModalNode) {
                        WM.Add(9 + 1, iOffset1 + 3 + 1, MWedge);
                }
                WM.Sub(iOffset2 + 9 + 1, iOffset1 + 3 + 1, MWedge);

                /* Derivate delle equaz. di vincolo */
                Omega1 = Omega1 - Omega2;

                v1 = e2tota.Cross(e3b.Cross(Omega1));
                Vec3 v1p(e3b.Cross(Omega1.Cross(e2tota)));
                v2 = e3tota.Cross(e1b.Cross(Omega1));
                Vec3 v2p(e1b.Cross(Omega1.Cross(e3tota)));
                v3 = e1tota.Cross(e2b.Cross(Omega1));
                Vec3 v3p(e2b.Cross(Omega1.Cross(e1tota)));

                if (pModalNode) {
                        for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                                doublereal d;

                                d = v1(iCnt);
                                WM.IncCoef(iOffset1 + 9 + 1, 3 + iCnt, d);

                                d = v2(iCnt);
                                WM.IncCoef(iOffset1 + 9 + 2, 3 + iCnt, d);

                                d = v3(iCnt);
                                WM.IncCoef(iOffset1 + 9 + 3, 3 + iCnt, d);
                        }
                }

                for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                        doublereal d;

                        d = v1p(iCnt);
                        WM.IncCoef(iOffset1 + 9 + 1, iOffset2 + 3 + iCnt, d);

                        d = v2p(iCnt);
                        WM.IncCoef(iOffset1 + 9 + 2, iOffset2 + 3 + iCnt, d);

                        d = v3p(iCnt);
                        WM.IncCoef(iOffset1 + 9 + 3, iOffset2 + 3 + iCnt, d);
                }

        } /* fine ciclo sui nodi vincolati */

        return WorkMat;
}

/* Contributo al residuo durante l'assemblaggio iniziale */
SubVectorHandler&
Modal::InitialAssRes(SubVectorHandler& WorkVec,
                const VectorHandler& XCurr)
{
        DEBUGCOUT("Entering Modal::InitialAssRes()" << std::endl);

        integer iNumRows;
        integer iNumCols;
        InitialWorkSpaceDim(&iNumRows, &iNumCols);
        WorkVec.ResizeReset(iNumRows);

        VecN Ka(NModes);

        if (pModalNode) {
                integer iRigidIndex = pModalNode->iGetFirstIndex();

                for (unsigned int iCnt = 1; iCnt <= iRigidOffset; iCnt++) {
                        WorkVec.PutRowIndex(iCnt, iRigidIndex + iCnt);
                }
        }

        integer iFlexIndex = iGetFirstIndex();

        for (unsigned int iCnt = 1; iCnt <= iGetInitialNumDof(); iCnt++) {
                WorkVec.PutRowIndex(iRigidOffset + iCnt, iFlexIndex + iCnt);
        }

        for (unsigned iStrNodem1 = 0; iStrNodem1 < NStrNodes; iStrNodem1++) {
                integer iNodeFirstPosIndex =
                        SND[iStrNodem1].pNode->iGetFirstPositionIndex();
                integer iNodeFirstVelIndex = iNodeFirstPosIndex + 6;

                integer iOffset2 = iRigidOffset + iGetInitialNumDof() + 12*iStrNodem1;
                for (unsigned int iCnt = 1; iCnt <= 6; iCnt++) {
                        WorkVec.PutRowIndex(iOffset2 + iCnt,
                                        iNodeFirstPosIndex + iCnt);
                        WorkVec.PutRowIndex(iOffset2 + 6 + iCnt,
                                        iNodeFirstVelIndex + iCnt);
                }
        }

        /* updates modal displacements and velocities: a, b (== aP) */
        for (unsigned int iCnt = 1; iCnt <= NModes; iCnt++) {
                a.Put(iCnt, XCurr(iFlexIndex + iCnt));
                b.Put(iCnt, XCurr(iFlexIndex + NModes + iCnt));
        }

        /* updates modal forces: K*a + C*aP */
        for (unsigned int iCnt = 1; iCnt <= NModes; iCnt++) {
                doublereal temp = 0.;

                for (unsigned int jCnt = 1; jCnt <= NModes; jCnt++) {
                        temp += oModalStiff.dGet(iCnt, jCnt)*a(jCnt)
                                + oModalDamp.dGet(iCnt, jCnt)*b(jCnt);
                }
                WorkVec.DecCoef(iRigidOffset + iCnt, temp);
        }

        /* equazioni di vincolo */

        /* Recupera i dati */
        Vec3 v1;
        Vec3 Omega1;

        if (pModalNode) {
                v1 = pModalNode->GetVCurr();
                R = pModalNode->GetRCurr();
                RT = R.Transpose();
                Omega1 = pModalNode->GetWCurr();

        } else {
                v1 = ::Zero3;
                Omega1 = ::Zero3;
        }

        for (unsigned int iStrNode = 1; iStrNode <= NStrNodes; iStrNode++) {
                unsigned int iStrNodem1 = iStrNode - 1;
                Mat3xN PHIt(NModes,0), PHIr(NModes,0);

                integer iOffset1 = iRigidOffset + 2*NModes + 12*iStrNodem1;
                integer iOffset2 = iRigidOffset + iGetInitialNumDof() + 12*iStrNodem1;

                for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                        for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                                PHIt.Put(iCnt, jMode, oPHIt.dGet(iCnt, (jMode - 1)*NStrNodes + iStrNode));
                                PHIr.Put(iCnt, jMode, oPHIr.dGet(iCnt, (jMode - 1)*NStrNodes + iStrNode));
                        }
                }

                MatNx3 PHItT(NModes), PHIrT(NModes);
                PHItT.Transpose(PHIt);
                PHIrT.Transpose(PHIr);

                Vec3 d1tot = R*(SND[iStrNodem1].OffsetFEM + PHIt*a);
                Mat3x3 R1tot = R*(Eye3 + Mat3x3(MatCross, PHIr*a));

                const Vec3& x2(SND[iStrNodem1].pNode->GetXCurr());
                const Vec3& v2(SND[iStrNodem1].pNode->GetVCurr());
                const Mat3x3& R2(SND[iStrNodem1].pNode->GetRCurr());
                const Vec3& Omega2(SND[iStrNodem1].pNode->GetWCurr());
                Vec3 F(XCurr, iFlexIndex + 2*NModes + 12*iStrNodem1 + 1);
                Vec3 FPrime(XCurr, iFlexIndex + 2*NModes + 12*iStrNodem1 + 6 + 1);

                /* cerniera sferica */

                /* Distanza nel sistema globale */
                const Vec3& d1Tmp(d1tot);
                Vec3 d2Tmp(R2*SND[iStrNodem1].OffsetMB);

                /* Vettori omega1/\d1, -omega2/\d2 */
                Vec3 O1Wedged1(Omega1.Cross(d1Tmp));
                Vec3 O2Wedged2(Omega2.Cross(d2Tmp));

                /* d1Prime= w1/\d1 + R*PHIt*b */
                Mat3xN R1PHIt(NModes);
                R1PHIt.LeftMult(R, PHIt);
                Vec3 d1Prime(O1Wedged1 + R1PHIt*b);

                /* Equazioni di equilibrio, nodo 1 */
                if (pModalNode) {
                        WorkVec.Sub(1, F);
                        WorkVec.Add(3 + 1, F.Cross(d1Tmp)); /* F/\d = -d/\F */
                }

                /* Eq. d'equilibrio ai modi */
                for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                        doublereal temp = 0.;
                        for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                                temp += PHIt(iCnt, jMode)*(RT*F).dGet(iCnt);
                        }
                        WorkVec.DecCoef(iRigidOffset + jMode, temp);
                }

                /* Derivate delle equazioni di equilibrio, nodo 1 */
                if (pModalNode) {
                        WorkVec.Sub(6 + 1, FPrime);
                        WorkVec.Add(9 + 1, FPrime.Cross(d1Tmp) - d1Prime.Cross(F));
                }

                /* Derivata dell'eq. di equilibrio ai modi */
                MatNx3 PHItTR1T(NModes);
                PHItTR1T.RightMult(PHItT, RT);
                for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                        doublereal temp = 0.;

                        for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                                temp += PHItTR1T(jMode, iCnt)*(Omega1.Cross(F) - FPrime).dGet(iCnt);
                        }
                        WorkVec.IncCoef(iRigidOffset + NModes + jMode, temp);
                }

                /* Equazioni di equilibrio, nodo 2 */
                WorkVec.Add(iOffset2 + 1, F);
                WorkVec.Add(iOffset2 + 3 + 1, d2Tmp.Cross(F));

                /* Derivate delle equazioni di equilibrio, nodo 2 */
                WorkVec.Add(iOffset2 + 6 + 1, FPrime);
                WorkVec.Add(iOffset2 + 9 + 1, d2Tmp.Cross(FPrime) + O2Wedged2.Cross(F));

                /* Equazione di vincolo */
                WorkVec.Add(iOffset1 + 1, x + d1Tmp - x2 - d2Tmp);

                /* Derivata dell'equazione di vincolo */
                WorkVec.Add(iOffset1 + 6 + 1, v1 + d1Prime - v2 - O2Wedged2);

                /* giunto prismatico */
                Vec3 M(XCurr, iFlexIndex + 2*NModes + 12*iStrNodem1 + 3 + 1);
                Vec3 MPrime(XCurr, iFlexIndex + 2*NModes + 12*iStrNodem1 + 9 + 1);

                Vec3 e1a(R1tot.GetVec(1));
                Vec3 e2a(R1tot.GetVec(2));
                Vec3 e3a(R1tot.GetVec(3));
                Vec3 e1b(R2.GetVec(1));
                Vec3 e2b(R2.GetVec(2));
                Vec3 e3b(R2.GetVec(3));

                Vec3 MTmp(e2a.Cross(e3b*M(1))
                                + e3a.Cross(e1b*M(2))
                                + e1a.Cross(e2b*M(3)));

                /* Equazioni di equilibrio, nodo 1 */
                if (pModalNode) {
                        WorkVec.Sub(3 + 1, MTmp);
                }

                /* Equazioni di equilibrio, nodo 2 */
                WorkVec.Add(iOffset2 + 3 + 1, MTmp);

                /* Equazioni di equilibrio, contributo modale */
                Vec3 MTmpRel(R1tot.MulTV(MTmp));
                for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                        WorkVec.DecCoef(iRigidOffset + jMode, PHIrT.GetVec(jMode)*MTmpRel);
                }

                /* eaPrime = w/\ea + R*[(PHIr*b)/\]ia */
                Mat3x3 Tmp = R*Mat3x3(MatCross, PHIr*b);
                Vec3 e1aPrime = Omega1.Cross(e1a) + Tmp.GetVec(1);
                Vec3 e2aPrime = Omega1.Cross(e2a) + Tmp.GetVec(2);
                Vec3 e3aPrime = Omega1.Cross(e3a) + Tmp.GetVec(3);
                Vec3 MTmpPrime(::Zero3);
                // FIXME: check (always M(1)?)
                MTmpPrime =
                        (e2a.Cross(Omega2.Cross(e3b)) - e3b.Cross(e2aPrime))*M(1)
                        + (e3a.Cross(Omega2.Cross(e1b)) - e1b.Cross(e3aPrime))*M(2)
                        + (e1a.Cross(Omega2.Cross(e2b)) - e2b.Cross(e1aPrime))*M(3)
                        + e2a.Cross(e3b*MPrime(1))
                        + e3a.Cross(e1b*MPrime(2))
                        + e1a.Cross(e2b*MPrime(3));

                /* Derivate delle equazioni di equilibrio, nodo 1 */
                if (pModalNode) {
                        WorkVec.Sub(9 + 1, MTmpPrime);
                }

                /* Derivate delle equazioni di equilibrio, nodo 2 */
                WorkVec.Add(iOffset2 + 9 + 1, MTmpPrime);

                /* Derivate delle equazioni di equilibrio, contributo modale */
                MatNx3 SubMat1(NModes);
                SubMat1.RightMult(PHIrT, R1tot.Transpose());

                /* FIXME: temporary ((PHIr*b).Cross(RT*MTmp)) */
                Vec3 T1 = MTmpPrime - Omega1.Cross(MTmp);
                Vec3 T2 = (PHIr*b).Cross(RT*MTmp);

                for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                        doublereal temp = 0;

                        for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                                temp += SubMat1(jMode, iCnt)*T1(iCnt)
                                        - PHIrT(jMode, iCnt)*T2(iCnt);
                        }
                        WorkVec.DecCoef(iRigidOffset + NModes + jMode, temp);
                }

                /* Equazioni di vincolo di rotazione */
                WorkVec.DecCoef(iOffset1 + 3 + 1, e3b.Dot(e2a));
                WorkVec.DecCoef(iOffset1 + 3 + 2, e1b.Dot(e3a));
                WorkVec.DecCoef(iOffset1 + 3 + 3, e2b.Dot(e1a));

                /* Derivate delle equazioni di vincolo di rotazione */
                WorkVec.IncCoef(iOffset1 + 9 + 1,
                                e3b.Dot(Omega2.Cross(e2a) - e2aPrime));
                WorkVec.IncCoef(iOffset1 + 9 + 2,
                                e1b.Dot(Omega2.Cross(e3a) - e3aPrime));
                WorkVec.IncCoef(iOffset1 + 9 + 3,
                                e2b.Dot(Omega2.Cross(e1a) - e1aPrime));

        } /* fine equazioni di vincolo */

        return WorkVec;
}

void
Modal::SetInitialValue(VectorHandler& X)
{
        /* inizializza la soluzione prima dell'assemblaggio iniziale */

        int iFlexIndex = iGetFirstIndex();

        for (unsigned int iCnt = 1; iCnt <= NModes; iCnt++) {
                /* modal multipliers */
                X.PutCoef(iFlexIndex + iCnt, a(iCnt));

                /* derivatives of modal multipliers */
                X.PutCoef(iFlexIndex + NModes + iCnt, b(iCnt));
        }

        if (pModalNode) {
                integer iRigidIndex = pModalNode->iGetFirstIndex();

                X.Put(iRigidIndex + 6 + 1, pModalNode->GetVCurr());
                X.Put(iRigidIndex + 9 + 1, pModalNode->GetWCurr());
        }
}

void
Modal::SetValue(DataManager *pDM,
                VectorHandler& X, VectorHandler& XP,
                SimulationEntity::Hints *ph)
{
        /* inizializza la soluzione e la sua derivata
         * subito dopo l'assemblaggio iniziale
         * e prima del calcolo delle derivate */

        int iFlexIndex = iGetFirstIndex();

        for (unsigned int iCnt = 1; iCnt <= NModes; iCnt++) {
                /* modal multipliers */
                X.PutCoef(iFlexIndex + iCnt, a(iCnt));

                /* derivatives of modal multipliers */
                X.PutCoef(iFlexIndex + NModes + iCnt, b(iCnt));
                XP.PutCoef(iFlexIndex + NModes + iCnt, bPrime(iCnt));
                XP.PutCoef(iFlexIndex + iCnt, b(iCnt));
        }
}

#if 0
/* Aggiorna dati durante l'iterazione fittizia iniziale */
void
Modal::DerivativesUpdate(const VectorHandler& X,
        const VectorHandler& XP)
{
        // NOTE: identical to SetValue()...
        int iFlexIndex = iGetFirstIndex();

        for (unsigned int iCnt = 1; iCnt <= NModes; iCnt++) {
                /* modal multipliers */
                const_cast<VectorHandler &>(X).PutCoef(iFlexIndex + iCnt, a.dGet(iCnt));

                /* derivatives of modal multipliers */
                const_cast<VectorHandler &>(X).PutCoef(iFlexIndex + NModes + iCnt, b.dGet(iCnt));
                const_cast<VectorHandler &>(XP).PutCoef(iFlexIndex + iCnt, b.dGet(iCnt));
        }
}
#endif

unsigned int
Modal::iGetNumPrivData(void) const
{
        return 3*NModes + 18*NFEMNodes;
}

unsigned int
Modal::iGetPrivDataIdx(const char *s) const
{
        /*
         * q[n]   | qP[n]   | qPP[n]
         *
         * where n is the 1-based mode number
         *
         * q[#i]  | qP[#i]  | qPP[#i]
         *
         * where i is the 1-based mode index (if only a subset of modes is used)
         *
         * x[n,i] | xP[n,i] | xPP[n,i]
         *          w[n,i]  | wP[n,i]
         *
         * where n is the FEM node label and i is the index number (1-3)
         */

        unsigned p = 0;
        char what = s[0];

        /* che cosa e' richiesto? */
        switch (what) {
        case 'q':
                /* modal dofs */
                break;

        case 'x':
                /* structural nodes positions */
                break;

        case 'w':
                /* structural nodes angular velocities */
                p = 1;
                break;

        default:
                /* unknown priv data */
                return 0;
        }

        while ((++s)[0] == 'P') {
                p++;
                if (p > 2) {
                        /* no more than two derivative levels allowed */
                        return 0;
                }
        }

        if (s[0] != '[') {
                return 0;
        }
        s++;

        /* trova la parentesi chiusa (e il terminatore di stringa) */
        const char *end = std::strchr(s, ']');
        if (end == 0 || end[1] != '\0') {
                return 0;
        }

        if (what == 'q') {
                bool bIdx(false);

                if (s[0] == '#') {
                        bIdx = true;
                        s++;
                }

                /* buffer per numero (dimensione massima: 32 bit) */
                std::string buf(s, end);

                /* leggi il numero */
                if (buf[0] == '-') {
                        return 0;
                }

                errno = 0;
                char *next;
                unsigned long n = strtoul(buf.c_str(), &next, 10);
                int save_errno = errno;
                end = next;
                if (end == buf.c_str()) {
                        return 0;

                } else if (end[0] != '\0') {
                        return 0;

                } else if (save_errno == ERANGE) {
                        silent_cerr("Modal(" << GetLabel() << "): "
                                    "warning, private data mode index "
                                    << std::string(buf.c_str(), end)
                                    << " overflows" << std::endl);
                        return 0;
                }

                if (bIdx) {
                        if (n > NModes) {
                                return 0;
                        }

                } else {
                        std::vector<unsigned int>::const_iterator iv
                                = std::find(uModeNumber.cbegin(), uModeNumber.cend(), n);
                        if (iv == uModeNumber.cend()) {
                                return 0;
                        }

                        n = iv - uModeNumber.begin() + 1;
                }

                return p*NModes + n;
        }

        end = std::strchr(s, ',');
        if (end == 0) {
                return 0;
        }

        std::string FEMLabel(s, end - s);

        end++;
        if (end[0] == '-') {
                return 0;
        }

        char *next;
        errno = 0;
        unsigned int index = strtoul(end, &next, 10);
        int save_errno = errno;
        if (next == end || next[0] != ']') {
                return 0;

        } else if (save_errno == ERANGE) {
                silent_cerr("Modal(" << GetLabel() << "): "
                        "warning, private data FEM node component "
                        << std::string(end, next - end)
                        << " overflows" << std::endl);
                return 0;
        }

        if (index == 0 || index > 3) {
                return 0;
        }

        std::vector<std::string>::const_iterator ii = find(IdFEMNodes.cbegin(), IdFEMNodes.cend(), FEMLabel);
        if (ii == IdFEMNodes.cend()) {
                return 0;
        }
        unsigned int i = ii - IdFEMNodes.cbegin();

        return 3*NModes + 18*i + (what == 'w' ? 3 : 0) + 6*p + index;
}


doublereal
Modal::dGetPrivData(unsigned int i) const
{
        ASSERT(i > 0 && i < iGetNumPrivData());

        if (i <= NModes) {
                return a(i);
        }

        i -= NModes;
        if (i <= NModes) {
                return b(i);
        }

        i -= NModes;
        if (i <= NModes) {
                return bPrime(i);
        }

        i -= NModes;
        unsigned int n = (i - 1) / 18;
        unsigned int index = (i - 1) % 18 + 1;
        unsigned int p = (index - 1) / 6;
        unsigned int c = (index - 1) % 6 + 1;
        unsigned int w = (c - 1) / 3;
        if (w) {
                c -= 3;
        }

        switch (p) {
        case 0:
                if (w) {
                        // TODO: orientation, somehow?
                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);

                } else {
                        // x == 0, R == eye otherwise
                        if (pModalNode) {
                                R = pModalNode->GetRCurr();
                                x = pModalNode->GetXCurr();
                        }
                        Vec3 Xn(oXYZFEMNodes.GetVec(n + 1));
                        for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                                integer iOffset = (jMode - 1)*NFEMNodes + n + 1;
                                Xn += oModeShapest.GetVec(iOffset)*a(jMode);
                        }

                        Vec3 X(x + R*Xn);
                        return X(c);
                }
                break;

        case 1:
                if (w) {
                        if (pModalNode) {
                                R = pModalNode->GetRCurr();
                        }
                        Vec3 Wn(::Zero3);
                        for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                                integer iOffset = (jMode - 1)*NFEMNodes + n + 1;
                                Wn += oModeShapesr.GetVec(iOffset)*b(jMode);
                        }

                        Vec3 W(R*Wn);
                        if (pModalNode) {
                                W += pModalNode->GetWCurr();
                        }
                        return W(c);

                } else {
                        if (pModalNode) {
                                R = pModalNode->GetRCurr();
                        }
                        Vec3 Vn(::Zero3);
                        Vec3 Xn(oXYZFEMNodes.GetVec(n + 1));
                        for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                                integer iOffset = (jMode - 1)*NFEMNodes + n + 1;
                                Vec3 v = oModeShapest.GetVec(iOffset);
                                Vn += v*b(jMode);
                                Xn += v*a(jMode);
                        }
                        Vec3 V(R*Vn);
                        if (pModalNode) {
                                V += pModalNode->GetWCurr().Cross(R*Xn);
                                V += pModalNode->GetVCurr();
                        }
                        return V(c);
                }
                break;

        case 2:
                if (w) {
                        if (pModalNode) {
                                R = pModalNode->GetRCurr();
                        }
                        Vec3 WPn(::Zero3);
                        Vec3 Wn(::Zero3);
                        for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                                integer iOffset = (jMode - 1)*NFEMNodes + n + 1;
                                Vec3 v = oModeShapesr.GetVec(iOffset);
                                WPn += v*bPrime(jMode);
                                Wn += v*b(jMode);
                        }

                        Vec3 WP(R*WPn);
                        if (pModalNode) {
                                WP += pModalNode->GetWCurr().Cross(R*Wn);
                                WP += pModalNode->GetWPCurr();
                        }
                        return WP(c);

                } else {
                        if (pModalNode) {
                                R = pModalNode->GetRCurr();
                        }
                        Vec3 XPPn(::Zero3);
                        Vec3 XPn(::Zero3);
                        Vec3 Xn(oXYZFEMNodes.GetVec(n + 1));
                        for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                                integer iOffset = (jMode - 1)*NFEMNodes + n + 1;
                                Vec3 v = oModeShapest.GetVec(iOffset);
                                XPPn += v*bPrime(jMode);
                                XPn += v*b(jMode);
                                Xn += v*a(jMode);
                        }
                        Vec3 XPP(R*XPPn);
                        if (pModalNode) {
                                const Vec3& W0 = pModalNode->GetWCurr();
                                XPP += W0.Cross(R*(XPn*2.));

                                Vec3 X(R*Xn);
                                XPP += W0.Cross(W0.Cross(X));
                                XPP += pModalNode->GetWPCurr().Cross(X);
                                XPP += pModalNode->GetXPPCurr();
                        }
                        return XPP(c);
                }
                break;
        }

        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
}

const Mat3xN&
Modal::GetCurrFEMNodesPosition(void)
{
        if (!oCurrXYZ.iGetNumCols()) {
             oCurrXYZ.Resize(NFEMNodes);
             oCurrXYZ.Reset();
        }

        if (pModalNode) {
                R = pModalNode->GetRCurr();
                x = pModalNode->GetXCurr();
        }

        for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                        for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                                oCurrXYZ.Add(iCnt, iNode,
                                        oXYZFEMNodes.dGet(iCnt, iNode) + oModeShapest.dGet(iCnt, (jMode - 1)*NFEMNodes + iNode) * a(jMode) );
                        }
                }
        }

        oCurrXYZ.LeftMult(R);
        for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                        oCurrXYZ.Add(iCnt, iNode, x(iCnt));
                }
        }

        return oCurrXYZ;
}

const Mat3xN&
Modal::GetCurrFEMNodesVelocity(void)
{
        if (!oCurrXYZVel.iGetNumCols()) {
             oCurrXYZVel.Resize(NFEMNodes);
             oCurrXYZVel.Reset();
        }

        Vec3 w(::Zero3);
        Vec3 v(::Zero3);

        if (pModalNode) {
                R = pModalNode->GetRCurr();
                w = pModalNode->GetWCurr();
                v = pModalNode->GetVCurr();
        }

        Mat3x3 wWedge(MatCross, w);

        Mat3xN CurrXYZTmp(NFEMNodes, 0.);
        for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                        for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                                doublereal d = oModeShapest.dGet(iCnt,(jMode - 1)*NFEMNodes + iNode);
                                oCurrXYZVel.Add(iCnt, iNode, oXYZFEMNodes.dGet(iCnt,iNode) + d * a(jMode) );
                                CurrXYZTmp.Add(iCnt, iNode,d * b(jMode) );
                        }
                }
        }
        oCurrXYZVel.LeftMult(wWedge*R);
        CurrXYZTmp.LeftMult(R);
        for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                        oCurrXYZVel.Add(iCnt, iNode, CurrXYZTmp(iCnt, iNode) + v(iCnt));
                }
        }

        return oCurrXYZVel;
}

/* from gravity.h */
/* massa totale */
doublereal
Modal::dGetM(void) const
{
        return dMass;
}

/* momento statico */
Vec3
Modal::GetS_int(void) const
{
        if (pModalNode != 0) {
                x = pModalNode->GetXCurr();
                R = pModalNode->GetRCurr();
        }

        Vec3 STmp = Inv2;
        if (oInv3.iGetNumCols()) {
                STmp += Inv3jaj;
        }

        return R*STmp + x*dMass;
}

/* momento d'inerzia */
Mat3x3
Modal::GetJ_int(void) const
{
        if (pModalNode != 0) {
                x = pModalNode->GetXCurr();
                R = pModalNode->GetRCurr();
        }

        Vec3 STmp = Inv2;
        if (oInv3.iGetNumCols()) {
                STmp += Inv3jaj;
        }
        Vec3 s(R*STmp);

        Mat3x3 JTmp = Inv7;
        if (oInv8.iGetNumCols()) {
                JTmp += Inv8jaj.Symm2();
                if (oInv9.iGetNumCols()) {
                        JTmp -= Inv9jkajak;
                }
        }

        return R*JTmp.MulMT(R)
                - Mat3x3(MatCrossCross, x, x*dMass)
                - Mat3x3(MatCrossCross, x, s)
                - Mat3x3(MatCrossCross, s, x);
}

// momentum
Vec3
Modal::GetB_int(void) const
{
        // FIXME: gross estimate
        if (pModalNode != 0) {
                return pModalNode->GetVCurr()*dMass;
        }

        return ::Zero3;
}

// momenta moment
Vec3
Modal::GetG_int(void) const
{
        return ::Zero3;
}

const OutputHandler::Dimensions
Modal::GetEquationDimension(integer index) const {
        // DOF is varible
        std::map<int, OutputHandler::Dimensions> index_map;

        unsigned int i, j;

        for (i = 1; i <= NModes; i++) {
                index_map[i] = OutputHandler::Dimensions::Momentum;
        }

        for ( i = NModes + 1; i <= 2 * NModes; i++) {
                index_map[i] = OutputHandler::Dimensions::Force;
        }

        for (i=0; i<NStrNodes; i++) {
                for ( j = 2*NModes+6*i+1; j <= 2*NModes+6*i+3 ; j++) {
                        index_map[j] = OutputHandler::Dimensions::Length;
                }

                for ( j = 2*NModes+6*i+4; j <= 2*NModes+6*i+6; j++) {
                        index_map[j] = OutputHandler::Dimensions::rad;
                }
        }

        return index_map[index];
}

Joint *
ReadModal(DataManager* pDM,
        MBDynParser& HP,
        const DofOwner* pDO,
        unsigned int uLabel)
{
        /* legge i dati d'ingresso e li passa al costruttore dell'elemento */
        Joint* pEl = 0;

        const ModalNode *pModalNode = 0;
        Vec3 X0(::Zero3);
        Mat3x3 R(::Eye3);

        /* If the modal element is clamped, a fixed position
         * and orientation of the reference point can be added */
        if (HP.IsKeyWord("clamped")) {
                if (HP.IsKeyWord("position")) {
                        X0 = HP.GetPosAbs(::AbsRefFrame);
                }

                if (HP.IsKeyWord("orientation")) {
                        R = HP.GetRotAbs(::AbsRefFrame);
                }

        /* otherwise a structural node of type "modal" must be given;
         * the "origin node" of the FEM model, if given, or the 0,0,0
         * coordinate point of the mesh will be put in the modal node */
        } else {
                unsigned int uNode = HP.GetInt();

                DEBUGCOUT("Linked to Modal Node: " << uNode << std::endl);

                /* verifica di esistenza del nodo */
                const StructNode* pTmpNode = pDM->pFindNode<const StructNode, Node::STRUCTURAL>(uNode);

                if (pTmpNode == 0) {
                        silent_cerr("Modal(" << uLabel << "): "
                                "StructuralNode(" << uNode << ") "
                                "at line " << HP.GetLineData()
                                << " not defined" << std::endl);
                        throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                }

                if (pTmpNode->GetStructNodeType() != StructNode::MODAL) {
                        silent_cerr("Modal(" << uLabel << "): "
                                "illegal type for "
                                "StructuralNode(" << uNode << ") "
                                "at line " << HP.GetLineData()
                                << std::endl);
                        throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                }
                pModalNode = dynamic_cast<const ModalNode *>(pTmpNode);
                ASSERT(pModalNode != 0);

                if (pModalNode->pGetRBK() != 0) {
                    silent_cerr("Modal(" << uLabel
                                << "): StructuralNode(" << uNode
                                << ") uses rigid body kinematics at line "
                                << HP.GetLineData() << std::endl);
                    throw ErrNotImplementedYet(MBDYN_EXCEPT_ARGS);
                }
                /* la posizione del nodo modale e' quella dell'origine del SdR
                 * del corpo flessibile */
                X0 = pModalNode->GetXCurr();

                /* orientazione del corpo flessibile, data come orientazione
                 * del nodo modale collegato */
                R = pModalNode->GetRCurr();
        }

        /* Legge i dati relativi al corpo flessibile */
        int tmpNModes = HP.GetInt();     /* numero di modi */
        if (tmpNModes <= 0) {
                silent_cerr("Modal(" << uLabel << "): "
                        "illegal number of modes " << tmpNModes << " at line "
                        << HP.GetLineData() << std::endl);
                throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
        }
        unsigned int NModes = (unsigned int)tmpNModes;
        unsigned int uLargestMode(0);

        std::vector<unsigned int> uModeNumber(NModes);
        if (HP.IsKeyWord("list")) {
                for (unsigned int iCnt = 0; iCnt < NModes; iCnt++) {
                        int n = HP.GetInt();

                        if (n <= 0) {
                                silent_cerr("Modal(" << uLabel << "): "
                                        "illegal mode number "
                                        "for mode #" << iCnt + 1
                                        << " at line " << HP.GetLineData()
                                        << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                        std::vector<unsigned int>::const_iterator
                                iv = std::find(uModeNumber.cbegin(),
                                        uModeNumber.cend(), (unsigned int)n);
                        if (iv != uModeNumber.cend()) {
                                silent_cerr("Modal(" << uLabel << "): "
                                        "mode #" << iCnt + 1
                                        << " already defined "
                                        "at line " << HP.GetLineData()
                                        << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                        if ((unsigned int)n > uLargestMode) {
                                uLargestMode = (unsigned int)n;
                        }

                        uModeNumber[iCnt] = n;
                }

        } else {
                for (unsigned int iCnt = 1; iCnt <= NModes; iCnt++) {
                        uModeNumber[iCnt - 1] = iCnt;
                }
                uLargestMode = NModes;
        }

        std::vector<doublereal> InitialValues(NModes, 0.);
        std::vector<doublereal> InitialValuesP(NModes, 0.);
        bool bInitialValues(false);
        if (HP.IsKeyWord("initial" "value")) {
                bInitialValues = true;

                if (HP.IsKeyWord("mode")) {
                        int iCnt = 0;
                        do {
                                int n = HP.GetInt();

                                if (n <= 0) {
                                        silent_cerr("Modal(" << uLabel << "): "
                                                "illegal mode number "
                                                "for initial value #" << iCnt + 1
                                                << " at line " << HP.GetLineData()
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                std::vector<unsigned int>::const_iterator
                                        iv = std::find(uModeNumber.cbegin(),
                                                uModeNumber.cend(), (unsigned int)n);
                                if (iv == uModeNumber.cend()) {
                                        silent_cerr("Modal(" << uLabel << "): "
                                                "mode " << n << " not defined "
                                                "at line " << HP.GetLineData()
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                unsigned iIdx = iv - uModeNumber.begin();
                                InitialValues[iIdx] = HP.GetReal();
                                InitialValuesP[iIdx] = HP.GetReal();

                                iCnt++;
                        } while (HP.IsKeyWord("mode"));

                } else {
                        for (unsigned int iCnt = 0; iCnt < NModes; iCnt++) {
                                InitialValues[0] = HP.GetReal();
                                InitialValuesP[0] = HP.GetReal();
                        }
                }
        }

        /* numero di nodi FEM del modello */
        unsigned int NFEMNodes = unsigned(-1);
        if (!HP.IsKeyWord("from" "file")) {
                int tmpNFEMNodes = HP.GetInt();
                if (tmpNFEMNodes <= 0) {
                        silent_cerr("Modal(" << uLabel << "): "
                                "illegal number of FEM nodes " << tmpNFEMNodes
                                << " at line " << HP.GetLineData() << std::endl);
                        throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                }
                NFEMNodes = unsigned(tmpNFEMNodes);
        }

#ifdef MODAL_SCALE_DATA
        /* Legge gli eventuali fattori di scala per le masse nodali
         * (scale masses) e per i modi (scale modes)   */

        /* NOTA: AL MOMENTO NON E' USATO */
        doublereal scalemasses(1.);
        doublereal scaleinertia(1.);
        doublereal scalemodes(1.);

        if (HP.IsKeyWord("scale" "masses")) {
                scalemasses = HP.GetReal();
        }

        if (HP.IsKeyWord("scale" "modes")) {
                scalemodes = HP.GetReal();
        }

        scaleinertia = scalemasses*(scalemodes*scalemodes);
#endif /* MODAL_SCALE_DATA */

        /* Legge i coefficienti di smorzamento */
        enum {
                DAMPING_FROM_FILE = 0,
                DAMPING_NO,
                // DAMPING_PROPORTIONAL, // obsoleted
                DAMPING_RAYLEIGH,
                DAMPING_SINGLE_FACTOR,
                DAMPING_DIAG
        } eDamp(DAMPING_FROM_FILE);
        const char *sDamp[] = {
                "damping from file",
                "no damping",
                // "proportional damping", // obsoleted
                "Rayleigh damping",
                "single factor damping",
                "diag damping",
                NULL
        };

        doublereal damp_factor(0.), damp_coef_M(0.), damp_coef_K(0.);
        std::vector<doublereal> DampRatios;

        if (HP.IsKeyWord("no" "damping")) {
                eDamp = DAMPING_NO;

        } else if (HP.IsKeyWord("rayleigh" "damping")) {
                eDamp = DAMPING_RAYLEIGH;
                damp_coef_M = HP.GetReal();
                if (damp_coef_M < 0.) {
                        silent_cerr("Modal(" << uLabel << "): "
                                "warning, negative mass damping coefficient for \"Rayleigh damping\" "
                                "at line " << HP.GetLineData() << std::endl);
                }
                damp_coef_K = HP.GetReal();
                if (damp_coef_K < 0.) {
                        silent_cerr("Modal(" << uLabel << "): "
                                "warning, negative stiffness damping coefficient for \"Rayleigh damping\" "
                                "at line " << HP.GetLineData() << std::endl);
                }

        } else if (HP.IsKeyWord("single" "factor" "damping")) {
                eDamp = DAMPING_SINGLE_FACTOR;
                damp_factor = HP.GetReal();
                if (damp_factor < 0.) {
                        silent_cerr("Modal(" << uLabel << "): "
                                "warning, negative damping factor for \"single factor damping\" "
                                "at line " << HP.GetLineData() << std::endl);
                }

        } else if (HP.IsKeyWord("proportional" "damping")) {
                silent_cerr("Modal(" << uLabel << "): "
                        "warning, \"proportional damping\" is deprecated; "
                        "use \"single factor damping\" with the desired damping factor instead "
                        "at line " << HP.GetLineData() << std::endl);
                // NOTE: "proportional damping" deprecated, replaced by "single factor damping"
                eDamp = DAMPING_SINGLE_FACTOR;
                damp_factor = HP.GetReal();
                if (damp_factor < 0.) {
                        silent_cerr("Modal(" << uLabel << "): "
                                "warning, negative damping factor for \"proportional damping\" "
                                "at line " << HP.GetLineData() << std::endl);
                }

        } else if (HP.IsKeyWord("diag" "damping"))  {
                eDamp = DAMPING_DIAG;
                DampRatios.resize(NModes);
                fill(DampRatios.begin(), DampRatios.end(), 0.);

                if (HP.IsKeyWord("all")) {
                        for (unsigned int iCnt = 0; iCnt < NModes; iCnt ++) {
                                damp_factor = HP.GetReal();
                                if (damp_factor < 0.) {
                                        silent_cerr("Modal(" << uLabel << "): "
                                                "warning, negative damping factor for \"diag damping\" "
                                                "of mode " << (iCnt + 1 ) << " "
                                                "at line " << HP.GetLineData() << std::endl);
                                }
                                DampRatios[iCnt] = damp_factor;
                        }

                } else {
                        int iDM = HP.GetInt();
                        if (iDM <= 0 || unsigned(iDM) > NModes) {
                                silent_cerr("invalid number of damped modes "
                                        "at line " << HP.GetLineData() <<std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                        std::vector<bool> gotit(NModes, false);

                        for (unsigned int iCnt = 1; iCnt <= unsigned(iDM); iCnt ++) {
                                integer iDampedMode = HP.GetInt();
                                if (iDampedMode <= 0 || unsigned(iDampedMode) > NModes) {
                                        silent_cerr("invalid index " << iDampedMode
                                                << " for damped mode #" << iCnt
                                                << " of " << iDM
                                                << " at line " << HP.GetLineData() <<std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (gotit[iDampedMode - 1]) {
                                        silent_cerr("damping already provided for mode " << iDampedMode
                                                << " (damped mode #" << iCnt
                                                << " of " << iDM
                                                << ") at line " << HP.GetLineData() <<std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);

                                }
                                gotit[iDampedMode - 1] = true;
                                damp_factor = HP.GetReal();
                                if (damp_factor < 0.) {
                                        silent_cerr("Modal(" << uLabel << "): "
                                                "warning, negative damping factor for \"diag damping\" "
                                                "of mode " << iDampedMode << " "
                                                "at line " << HP.GetLineData() << std::endl);
                                }
                                DampRatios[iDampedMode - 1] = damp_factor;
                        }
                }
        } else if (HP.IsKeyWord("damping" "from" "file")) {
                eDamp = DAMPING_FROM_FILE;
        } else {
                silent_cout("Modal(" << uLabel << "): "
                                "no damping is assumed at line "
                                << HP.GetLineData() << " (deprecated)"
                                << std::endl);
        }

        DEBUGCOUT("Number of Modes Imported : " << NModes << std::endl);
        DEBUGCOUT("Number of FEM Nodes Imported : " << NFEMNodes << std::endl);
        DEBUGCOUT("Origin of FEM Model : " << X0 << std::endl);
        DEBUGCOUT("Orientation of FEM Model : " << R << std::endl);
        /* DEBUGCOUT("Damping coefficient: "<< cdamp << std::endl); */

        doublereal dMass = 0;              // total mass
        Vec3 XTmpIn(::Zero3);              // center of mass location
        Vec3 STmp(::Zero3);                // static moment
        Mat3x3 JTmp(::Zero3x3);            // total inertia from input
        Mat3x3 JTmpIn(::Zero3x3);          // total inertia after manipulation
        std::vector<doublereal> FEMMass;   // nodal masses
        std::vector<Vec3> FEMJ;            // nodal inertia (diagonal)

        Mat3xN oModeShapest;               // displacement and rotation shapes
        Mat3xN oModeShapesr;
        Mat3xN oPHIti(NModes, 0.);         // i-th node shapes: 3*nmodes
        Mat3xN oPHIri(NModes, 0.);
        Mat3xN oXYZFEMNodes;               // nodal coords
        MatNxN oGenMass;                   // modal mass
        MatNxN oGenStiff;                  // modal stiffness
        MatNxN oGenDamp;                   // modal damping
        std::vector<unsigned> rgGenStressStiffIdx; // modal stress stiffening index
        std::vector<MatNxN> rgGenStressStiff; // modal stress stiffening
        // inertia invariants
        Mat3xN oInv3;
        Mat3xN oInv4;
        Mat3xN oInv5;
        Mat3xN oInv8;
        Mat3xN oInv9;
        Mat3xN oInv10;
        Mat3xN oInv11;

        // modal displacements and velocities
        VecN a;
        VecN aP;

        // FEM database file name
        const char *s = HP.GetFileName();
        if (s == 0) {
                silent_cerr("Modal(" << uLabel << "): unable to get "
                        "modal data file name at line " << HP.GetLineData()
                        << std::endl);
                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
        }

        std::string sFileFEM(s);

        doublereal dMassThreshold = 0.;
        doublereal dDampingThreshold = 0.;
        doublereal dStiffnessThreshold = 0.;
        while (true) {
                if (HP.IsKeyWord("threshold")) {
                        dStiffnessThreshold = HP.GetReal();
                        if (dStiffnessThreshold < 0.) {
                                silent_cerr("Modal(" << uLabel << "): "
                                        "invalid threshold " << dStiffnessThreshold
                                        << " at line " << HP.GetLineData()
                                        << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }
                        dMassThreshold = dDampingThreshold = dStiffnessThreshold;

                } else if (HP.IsKeyWord("mass" "threshold")) {
                        dMassThreshold = HP.GetReal();
                        if (dMassThreshold < 0.) {
                                silent_cerr("Modal(" << uLabel << "): "
                                        "invalid mass threshold " << dMassThreshold
                                        << " at line " << HP.GetLineData()
                                        << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                } else if (HP.IsKeyWord("damping" "threshold")) {
                        dDampingThreshold = HP.GetReal();
                        if (dDampingThreshold < 0.) {
                                silent_cerr("Modal(" << uLabel << "): "
                                        "invalid damping threshold " << dDampingThreshold
                                        << " at line " << HP.GetLineData()
                                        << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                } else if (HP.IsKeyWord("stiffness" "threshold")) {
                        dStiffnessThreshold = HP.GetReal();
                        if (dStiffnessThreshold < 0.) {
                                silent_cerr("Modal(" << uLabel << "): "
                                        "invalid stiffness threshold " << dStiffnessThreshold
                                        << " at line " << HP.GetLineData()
                                        << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                } else {
                        break;
                }
        }

        /* loop for binary keywords */
        bool bCreateBinary = false;
        bool bUseBinary = false;
        bool bUpdateBinary = false;
        while (true) {
                if (HP.IsKeyWord("create" "binary")) {
                        bCreateBinary = true;

                } else if (HP.IsKeyWord("use" "binary")) {
                        bUseBinary = true;

                } else if (HP.IsKeyWord("update" "binary")) {
                        bUpdateBinary = true;

                } else {
                        break;
                }
        }

        std::string sEchoFileName;
        int iEchoPrecision(13);
        if (HP.IsKeyWord("echo")) {
                const char *s = HP.GetFileName();
                if (s == 0) {
                        silent_cerr("Modal(" << uLabel << "): "
                                "unable to parse echo file name at line " << HP.GetLineData()
                                << std::endl);
                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                }
                sEchoFileName = s;

                if (HP.IsKeyWord("precision")) {
                        iEchoPrecision = HP.GetInt();
                        if (iEchoPrecision <= 0) {
                                silent_cerr("Modal(" << uLabel << "): "
                                        "invalid precision at line " << HP.GetLineData()
                                        << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }
                }
        }

        /* stuff for binary FEM file handling */
        std::string	sBinFileFEM;
        struct stat	stFEM, stBIN;
        bool		bReadFEM = true,
                        bWriteBIN = false,
                        bCheckBIN = false;

        if (stat(sFileFEM.c_str(), &stFEM) == -1) {
                int	save_errno = errno;
                char	*errmsg = strerror(save_errno);

                silent_cerr("Modal(" << uLabel << "): "
                                "unable to stat(\"" << sFileFEM << "\") "
                                "(" << save_errno << ": " << errmsg << ")"
                                << std::endl);
                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
        }

        if (bUseBinary || bCreateBinary || bUpdateBinary) {
                sBinFileFEM = sFileFEM + ".bin";

                if (stat(sBinFileFEM.c_str(), &stBIN) == -1) {
                        int	save_errno = errno;
                        char	*errmsg = strerror(save_errno);

                        if (save_errno == ENOENT && (bCreateBinary || bUpdateBinary)) {
                                silent_cerr("Modal(" << uLabel << "): "
                                                "creating binary file "
                                                "\"" << sBinFileFEM << "\" "
                                                "from file "
                                                "\"" << sFileFEM << "\""
                                                << std::endl);

                        } else {
                                silent_cerr("Modal(" << uLabel << "): "
                                                "unable to stat(\"" << sBinFileFEM << "\") "
                                                "(" << save_errno << ": " << errmsg << ")"
                                                << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                } else {
                        /* can check bin */
                        bCheckBIN = true;
                }
        }

        if (bUseBinary && bCheckBIN) {
                /* if timestamp of binary is later than of ASCII use binary */
                if (stBIN.st_mtime > stFEM.st_mtime) {
                        bReadFEM = false;

                /* otherwise, if requested, update binary */
                } else {
                        if (bUpdateBinary) {
                                bWriteBIN = true;

                                silent_cout("Modal(" << uLabel << "): "
                                                "binary database file \"" << sBinFileFEM << "\" "
                                                "older than text database file \"" << sFileFEM << "\"; "
                                                "updating"
                                                << std::endl);

                        } else {
                                silent_cerr("Modal(" << uLabel << "): "
                                                "warning, binary database file \"" << sBinFileFEM << "\" "
                                                "older than text database file \"" << sFileFEM << "\"; "
                                                "using text database file "
                                                "(enable \"update binary\" to refresh binary database file)"
                                                << std::endl);
                        }
                }

        } else if (bCreateBinary || bUpdateBinary) {
                bWriteBIN = true;
        }

        /* alloca la memoria per le matrici necessarie a memorizzare i dati
         * relativi al corpo flessibile
         * nota: devo usare i puntatori perche' altrimenti non si riesce
         * a passarli al costruttore
         */
        oGenMass.Resize(NModes);
        oGenMass.Reset();
        oGenStiff.Resize(NModes);
        oGenStiff.Reset();
        oGenDamp.Resize(NModes);
        oGenDamp.Reset();

        a.Resize(NModes);
        a.Reset();
        aP.Resize(NModes);
        aP.Reset();

        /* array contenente le label dei nodi FEM */
        std::vector<std::string> IdFEMNodes;
        std::vector<bool> bActiveModes;

        enum {
                MODAL_VERSION_UNKNOWN = 0,

                MODAL_VERSION_1 = 1,
                MODAL_VERSION_2 = 2,
                MODAL_VERSION_3 = 3, // after adding record 13 (generalized damping)
                MODAL_VERSION_4 = 4, // after making sizes portable
                MODAL_VERSION_5 = 5, // after adding Inv3, Inv4 and Inv8 from modal element data for AVL-EXCITE (https://www.avl.com/excite)
                MODAL_VERSION_6 = 6, // after adding record 19
                MODAL_VERSION_LAST
        };

        /* NOTE: increment this each time the binary format changes! */
        const char	magic[5] = "bmod";
        const uint32_t	BinVersion = MODAL_VERSION_6;

        uint32_t	currBinVersion = MODAL_VERSION_UNKNOWN;
        char		checkPoint;
        uint32_t	NFEMNodesFEM = 0, NModesFEM = 0;

        bool		bBuildInvariants = false;

        enum {
                // note: we use 1-based array
                MODAL_RECORD_1 = 1,
                MODAL_RECORD_2 = 2,
                MODAL_RECORD_3 = 3,
                MODAL_RECORD_4 = 4,
                MODAL_RECORD_5 = 5,
                MODAL_RECORD_6 = 6,
                MODAL_RECORD_7 = 7,
                MODAL_RECORD_8 = 8,
                MODAL_RECORD_9 = 9,
                MODAL_RECORD_10 = 10,
                MODAL_RECORD_11 = 11,
                MODAL_RECORD_12 = 12,
                MODAL_RECORD_13 = 13,
                MODAL_RECORD_14 = 14,
                MODAL_RECORD_15 = 15,
                MODAL_RECORD_16 = 16,
                MODAL_RECORD_17 = 17,
                MODAL_RECORD_18 = 18,
                MODAL_RECORD_19 = 19,
                // NOTE: do not exceed 127

                MODAL_LAST_RECORD,

                MODAL_END_OF_FILE = char(-1)
        };

        /* Note: to keep it readable, we use a base 1 array */
        bool bRecordGroup[MODAL_LAST_RECORD] = { false };

        // record 1: header
        // record 2: FEM node labels
        // record 3: initial modal amplitudes
        // record 4: initial modal amplitude rates
        // record 5: FEM node X
        // record 6: FEM node Y
        // record 7: FEM node Z
        // record 8: shapes
        // record 9: generalized mass matrix
        // record 10: generalized stiffness matrix
        // record 11: (optional) diagonal mass matrix
        // record 12: (optional) rigid body inertia
        // record 13: (optional) damping matrix
        // record 14: (optional) invariant 3
        // record 15: (optional) invariant 4
        // record 16: (optional) invariant 8

        std::string fname;
        if (bReadFEM) {
                /* apre il file con i dati del modello FEM */
                std::ifstream fdat(sFileFEM.c_str());
                if (!fdat) {
                        silent_cerr("Modal(" << uLabel << "): "
                                "unable to open file \"" << sFileFEM << "\""
                                "at line " << HP.GetLineData() << std::endl);
                        throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                }

                /* don't leave behind a corrupted .bin file */
                try {

                std::ofstream fbin;
                if (bWriteBIN) {
                        fbin.open(sBinFileFEM.c_str(), std::ios::binary | std::ios::trunc);
                        if (!fbin) {
                                silent_cerr("Modal(" << uLabel << "): "
                                        "unable to open file \"" << sBinFileFEM << "\""
                                        "at line " << HP.GetLineData() << std::endl);
                                throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                        fbin.write(&magic[0], sizeof(4));
                        currBinVersion = BinVersion;
                        fbin.write((const char *)&currBinVersion, sizeof(currBinVersion));
                }

                /* carica i dati relativi a coordinate nodali, massa, momenti statici
                 * e d'inerzia, massa e rigidezza generalizzate dal file nomefile.
                 */

                doublereal	d;
                unsigned int	NAttModes = 0, NCstModes = 0, NRejModes = 0;
                char		str[BUFSIZ];

                while (fdat && !fdat.eof()) {        /* parsing del file */
                        fdat.getline(str, sizeof(str));

                        if (strncmp("** END OF FILE", str, STRLENOF("** END OF FILE")) == 0) {
                                break;
                        }

                        /* legge il primo blocco (HEADER) */
                        if (strncmp("** RECORD GROUP ", str,
                                STRLENOF("** RECORD GROUP ")) != 0) {
                                continue;
                        }

                        char *next;
                        long rg = std::strtol(&str[STRLENOF("** RECORD GROUP ")], &next, 10);
                        if (next == &str[STRLENOF("** RECORD GROUP ")]) {
                                silent_cerr("file=\"" << sFileFEM << "\": "
                                        "unable to parse \"RECORD GROUP\" number <" << &str[STRLENOF("** RECORD GROUP ")] << ">"
                                        << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                        if (!bRecordGroup[MODAL_RECORD_1] && rg != MODAL_RECORD_1) {
                                silent_cerr("file=\"" << sFileFEM << "\": "
                                        "\"RECORD GROUP 1\" not parsed yet; cannot parse \"RECORD GROUP " << rg << "\""
                                        << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                        switch (rg) {
                        case MODAL_RECORD_1: {
                                if (bRecordGroup[MODAL_RECORD_1]) {
                                        silent_cerr("file=\"" << sFileFEM << "\": "
                                                "\"RECORD GROUP 1\" already parsed"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                fdat.getline(str, sizeof(str));

                                /* ignore REV by now */
                                fdat >> str;

                                /* FEM nodes number */
                                fdat >> NFEMNodesFEM;

                                /* add to modes number */
                                fdat >> NModesFEM;

                                fdat >> NAttModes;
                                NModesFEM += NAttModes;
                                fdat >> NCstModes;
                                NModesFEM += NCstModes;

                                /* "rejected" modes (subtract from modes number) */
                                fdat >> NRejModes;
                                NModesFEM -= NRejModes;

                                /* consistency checks */
                                if (NFEMNodes == unsigned(-1)) {
                                        NFEMNodes = NFEMNodesFEM;

                                } else if (NFEMNodes != NFEMNodesFEM) {
                                        silent_cerr("Modal(" << uLabel << "), "
                                                "file \"" << sFileFEM << "\": "
                                                "FEM nodes number " << NFEMNodes
                                                << " does not match node number "
                                                << NFEMNodesFEM
                                                << std::endl);
                                        throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (NModes > NModesFEM) {
                                        silent_cerr("Modal(" << uLabel << "), "
                                                "file '" << sFileFEM << "': "
                                                "number of requested modes "
                                                "(" << NModes << ") "
                                                "exceeds number of available "
                                                "modes (" << NModesFEM << ")"
                                                << std::endl);
                                        throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);

                                }

                                if (NModes < NModesFEM) {
                                        silent_cout("Modal(" << uLabel << "), "
                                                "file '" << sFileFEM << "': "
                                                "using " << NModes
                                                << " of " << NModesFEM
                                                << " available modes"
                                                << std::endl);
                                }

                                if (uLargestMode > NModesFEM) {
                                        silent_cerr("Modal(" << uLabel << "), "
                                                "file '" << sFileFEM << "': "
                                                "largest requested mode "
                                                "(" << uLargestMode << ") "
                                                "exceeds available modes "
                                                "(" << NModesFEM << ")"
                                                << std::endl);
                                        throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (!bActiveModes.empty()) {
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                IdFEMNodes.resize(NFEMNodes);

                                oXYZFEMNodes.Resize(NFEMNodes);
                                oXYZFEMNodes.Reset();
                                oModeShapest.Resize(NFEMNodes * NModes);
                                oModeShapest.Reset();
                                oModeShapesr.Resize(NFEMNodes * NModes);
                                oModeShapesr.Reset();

                                bActiveModes.resize(NModesFEM + 1, false);

                                for (unsigned int iCnt = 0; iCnt < NModes; iCnt++) {
                                        if (uModeNumber[iCnt] > NModesFEM) {
                                                silent_cerr("Modal(" << uLabel << "): "
                                                        "mode " << uModeNumber[iCnt]
                                                        << " is not available (max = "
                                                        << NModesFEM << ")"
                                                        << std::endl);
                                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                        }
                                        bActiveModes[uModeNumber[iCnt]] = true;
                                }

                                if (bWriteBIN) {
                                        checkPoint = MODAL_RECORD_1;
                                        fbin.write(&checkPoint, sizeof(checkPoint));
                                        fbin.write((const char *)&NFEMNodesFEM, sizeof(NFEMNodesFEM));
                                        fbin.write((const char *)&NModesFEM, sizeof(NModesFEM));
                                }

                                bRecordGroup[MODAL_RECORD_1] = true;
                                } break;

                        case MODAL_RECORD_2: {
                                /* legge il secondo blocco (Id.nodi) */
                                if (bRecordGroup[MODAL_RECORD_2]) {
                                        silent_cerr("file=\"" << sFileFEM << "\": "
                                                "\"RECORD GROUP 2\" already parsed"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                for (unsigned int iNode = 0; iNode < NFEMNodes; iNode++) {
                                        // NOTE: this precludes the possibility
                                        // to have blanks in node labels
                                        fdat >> IdFEMNodes[iNode];
                                }

                                if (bWriteBIN) {
                                        checkPoint = MODAL_RECORD_2;
                                        fbin.write(&checkPoint, sizeof(checkPoint));
                                        for (unsigned int iNode = 0; iNode < NFEMNodes; iNode++) {
                                                uint32_t len = IdFEMNodes[iNode].size();
                                                fbin.write((const char *)&len, sizeof(len));
                                                fbin.write((const char *)IdFEMNodes[iNode].c_str(), len);
                                        }
                                }

                                bRecordGroup[2] = true;
                                } break;

                        case MODAL_RECORD_3: {
                                /* deformate iniziali dei modi */
                                if (bRecordGroup[MODAL_RECORD_3]) {
                                        silent_cerr("file=\"" << sFileFEM << "\": "
                                                "\"RECORD GROUP 3\" already parsed"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                unsigned int iCnt = 1;

                                if (bActiveModes.empty()) {
                                        silent_cerr("Modal(" << uLabel << "): "
                                                "input file \"" << sFileFEM << "\" "
                                                "is bogus (RECORD GROUP 3)"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bWriteBIN) {
                                        checkPoint = MODAL_RECORD_3;
                                        fbin.write(&checkPoint, sizeof(checkPoint));
                                }

                                for (unsigned int jMode = 1; jMode <= NModesFEM; jMode++) {
                                        fdat >> d;

                                        if (bWriteBIN) {
                                                fbin.write((const char *)&d, sizeof(d));
                                        }

                                        if (!bActiveModes[jMode]) {
                                                continue;
                                        }
                                        a.Put(iCnt, d);
                                        iCnt++;
                                }

                                bRecordGroup[MODAL_RECORD_3] = true;
                                } break;

                        case MODAL_RECORD_4: {
                                /* velocita' iniziali dei modi */
                                if (bRecordGroup[MODAL_RECORD_4]) {
                                        silent_cerr("file=\"" << sFileFEM << "\": "
                                                "\"RECORD GROUP 4\" already parsed"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                unsigned int iCnt = 1;

                                if (bActiveModes.empty()) {
                                        silent_cerr("Modal(" << uLabel << "): "
                                                "input file \"" << sFileFEM << "\" "
                                                "is bogus (RECORD GROUP 4)"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bWriteBIN) {
                                        checkPoint = MODAL_RECORD_4;
                                        fbin.write(&checkPoint, sizeof(checkPoint));
                                }

                                for (unsigned int jMode = 1; jMode <= NModesFEM; jMode++) {
                                        fdat >> d;

                                        if (bWriteBIN) {
                                                fbin.write((const char *)&d, sizeof(d));
                                        }

                                        if (!bActiveModes[jMode]) {
                                                continue;
                                        }
                                        aP.Put(iCnt, d);
                                        iCnt++;
                                }

                                bRecordGroup[MODAL_RECORD_4] = true;
                                } break;

                        case MODAL_RECORD_5: {
                                /* Coordinate X dei nodi */
                                if (bRecordGroup[MODAL_RECORD_5]) {
                                        silent_cerr("file=\"" << sFileFEM << "\": "
                                                "\"RECORD GROUP 5\" already parsed"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bWriteBIN) {
                                        checkPoint = MODAL_RECORD_5;
                                        fbin.write(&checkPoint, sizeof(checkPoint));
                                }

                                for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                                        fdat >> d;

                                        if (bWriteBIN) {
                                                fbin.write((const char *)&d, sizeof(d));
                                        }

#ifdef MODAL_SCALE_DATA
                                        d *= scalemodes;
#endif /* MODAL_SCALE_DATA */
                                        oXYZFEMNodes.Put(1, iNode, d);
                                }

                                bRecordGroup[MODAL_RECORD_5] = true;
                                } break;

                        case MODAL_RECORD_6: {
                                /* Coordinate Y dei nodi*/
                                if (bRecordGroup[MODAL_RECORD_6]) {
                                        silent_cerr("file=\"" << sFileFEM << "\": "
                                                "\"RECORD GROUP 6\" already parsed"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bWriteBIN) {
                                        checkPoint = MODAL_RECORD_6;
                                        fbin.write(&checkPoint, sizeof(checkPoint));
                                }

                                for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                                        fdat >> d;

                                        if (bWriteBIN) {
                                                fbin.write((const char *)&d, sizeof(d));
                                        }

#ifdef MODAL_SCALE_DATA
                                        d *= scalemodes;
#endif /* MODAL_SCALE_DATA */
                                        oXYZFEMNodes.Put(2, iNode, d);
                                }

                                bRecordGroup[MODAL_RECORD_6] = true;
                                } break;

                        case MODAL_RECORD_7: {
                                /* Coordinate Z dei nodi*/
                                if (bRecordGroup[MODAL_RECORD_7]) {
                                        silent_cerr("file=\"" << sFileFEM << "\": "
                                                "\"RECORD GROUP 7\" already parsed"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bWriteBIN) {
                                        checkPoint = MODAL_RECORD_7;
                                        fbin.write(&checkPoint, sizeof(checkPoint));
                                }

                                for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                                        fdat >> d;

                                        if (bWriteBIN) {
                                                fbin.write((const char *)&d, sizeof(d));
                                        }

#ifdef MODAL_SCALE_DATA
                                        d *= scalemodes;
#endif /* MODAL_SCALE_DATA */
                                        oXYZFEMNodes.Put(3, iNode, d);
                                }

                                bRecordGroup[MODAL_RECORD_7] = true;
                                } break;

                        case MODAL_RECORD_8: {
                                /* Forme modali */
                                if (bRecordGroup[MODAL_RECORD_8]) {
                                        silent_cerr("file=\"" << sFileFEM << "\": "
                                                "\"RECORD GROUP 8\" already parsed"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bWriteBIN) {
                                        checkPoint = MODAL_RECORD_8;
                                        fbin.write(&checkPoint, sizeof(checkPoint));
                                }

                                // we assume rejected modes come first
                                for (unsigned int jMode = 1; jMode <= NRejModes; jMode++) {
                                        // header
                                        fdat.getline(str, sizeof(str));

                                        silent_cout("Modal(" << uLabel << "): rejecting mode #" << jMode
                                                << " (" << str << ")" << std::endl);

                                        // mode
                                        for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                                                doublereal n[6];

                                                fdat >> n[0] >> n[1] >> n[2]
                                                        >> n[3] >> n[4] >> n[5];
                                        }

                                        /* FIXME: siamo sicuri di avere
                                         * raggiunto '\n'? */
                                        fdat.getline(str, sizeof(str));
                                }

                                if (bActiveModes.empty()) {
                                        silent_cerr("Modal(" << uLabel << "): "
                                                "input file \"" << sFileFEM << "\" "
                                                "is bogus (RECORD GROUP 8)"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                unsigned int iCnt = 1;
                                for (unsigned int jMode = 1; jMode <= NModesFEM; jMode++) {
                                        fdat.getline(str, sizeof(str));
                                        if (str[0] != '\0' && strncmp("**", str, STRLENOF("**")) != 0)
                                        {
                                                if (NRejModes) {
                                                        silent_cerr("Modal(" << uLabel << "): "
                                                                "input file \"" << sFileFEM << "\""
                                                                "is bogus (\"**\" expected before mode #" << jMode
                                                                << " of " << NModesFEM << "; "
                                                                "#" << jMode + NRejModes
                                                                << " of " << NModesFEM + NRejModes
                                                                << " including rejected)"
                                                                << std::endl);
                                                } else {
                                                        silent_cerr("Modal(" << uLabel << "): "
                                                                "input file \"" << sFileFEM << "\""
                                                                "is bogus (\"**\" expected before mode #" << jMode
                                                                << " of " << NModesFEM << ")"
                                                                << std::endl);
                                                }
                                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                        }

                                        for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                                                doublereal n[6];

                                                fdat >> n[0] >> n[1] >> n[2]
                                                        >> n[3] >> n[4] >> n[5];

                                                if (bWriteBIN) {
                                                        fbin.write((const char *)n, sizeof(n));
                                                }

                                                if (!bActiveModes[jMode]) {
                                                        continue;
                                                }

#ifdef MODAL_SCALE_DATA
                                                n[0] *= scalemodes;
                                                n[1] *= scalemodes;
                                                n[2] *= scalemodes;
#endif /* MODAL_SCALE_DATA */
                                                oModeShapest.PutVec((iCnt - 1)*NFEMNodes + iNode, Vec3(&n[0]));
                                                oModeShapesr.PutVec((iCnt - 1)*NFEMNodes + iNode, Vec3(&n[3]));
                                        }

                                        if (bActiveModes[jMode]) {
                                                iCnt++;
                                        }
                                        fdat.getline(str, sizeof(str));
                                }

                                bRecordGroup[MODAL_RECORD_8] = true;
                                } break;

                        case MODAL_RECORD_9: {
                                /* Matrice di massa modale */
                                if (bRecordGroup[MODAL_RECORD_9]) {
                                        silent_cerr("file=\"" << sFileFEM << "\": "
                                                "\"RECORD GROUP 9\" already parsed"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bActiveModes.empty()) {
                                        silent_cerr("Modal(" << uLabel << "): "
                                                "input file \"" << sFileFEM << "\" "
                                                "is bogus (RECORD GROUP 9)"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bWriteBIN) {
                                        checkPoint = MODAL_RECORD_9;
                                        fbin.write(&checkPoint, sizeof(checkPoint));
                                }

                                unsigned int iCnt = 1;
                                for (unsigned int jMode = 1; jMode <= NModesFEM; jMode++) {
                                        unsigned int jCnt = 1;

                                        for (unsigned int kMode = 1; kMode <= NModesFEM; kMode++) {
                                                fdat >> d;

                                                if (dMassThreshold > 0.0 && fabs(d) < dMassThreshold) {
                                                        d = 0.0;
                                                }

                                                if (bWriteBIN) {
                                                        fbin.write((const char *)&d, sizeof(d));
                                                }

                                                if (!bActiveModes[jMode] || !bActiveModes[kMode]) {
                                                        continue;
                                                }
                                                oGenMass.Put(iCnt, jCnt, d);
                                                jCnt++;
                                        }

                                        if (bActiveModes[jMode]) {
                                                iCnt++;
                                        }
                                }

                                bRecordGroup[MODAL_RECORD_9] = true;
                                } break;

                        case MODAL_RECORD_10: {
                                /* Matrice di rigidezza modale */
                                if (bRecordGroup[MODAL_RECORD_10]) {
                                        silent_cerr("file=\"" << sFileFEM << "\": "
                                                "\"RECORD GROUP 10\" already parsed"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bActiveModes.empty()) {
                                        silent_cerr("Modal(" << uLabel << "): "
                                                "input file \"" << sFileFEM << "\" "
                                                "is bogus (RECORD GROUP 10)"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bWriteBIN) {
                                        checkPoint = MODAL_RECORD_10;
                                        fbin.write(&checkPoint, sizeof(checkPoint));
                                }

                                unsigned int iCnt = 1;
                                for (unsigned int jMode = 1; jMode <= NModesFEM; jMode++) {
                                        unsigned int jCnt = 1;

                                        for (unsigned int kMode = 1; kMode <= NModesFEM; kMode++) {
                                                fdat >> d;

                                                if (dStiffnessThreshold > 0.0 && fabs(d) < dStiffnessThreshold) {
                                                        d = 0.0;
                                                }

                                                if (bWriteBIN) {
                                                        fbin.write((const char *)&d, sizeof(d));
                                                }

                                                if (!bActiveModes[jMode] || !bActiveModes[kMode]) {
                                                        continue;
                                                }
                                                oGenStiff.Put(iCnt, jCnt, d);
                                                jCnt++;
                                        }

                                        if (bActiveModes[jMode]) {
                                                iCnt++;
                                        }
                                }

                                bRecordGroup[MODAL_RECORD_10] = true;
                                } break;

                        case MODAL_RECORD_11: {
                                /* Lumped Masses */
                                if (bRecordGroup[MODAL_RECORD_11]) {
                                        silent_cerr("file=\"" << sFileFEM << "\": "
                                                "\"RECORD GROUP 11\" already parsed"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bWriteBIN) {
                                        checkPoint = MODAL_RECORD_11;
                                        fbin.write(&checkPoint, sizeof(checkPoint));
                                }

                                FEMMass.resize(NFEMNodes);
                                FEMJ.resize(NFEMNodes);

                                for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                                        doublereal tmpmass = 0.;
                                        for (unsigned int jCnt = 1; jCnt <= 6; jCnt++) {
                                                fdat >> d;

                                                if (bWriteBIN) {
                                                        fbin.write((const char *)&d, sizeof(d));
                                                }

                                                switch (jCnt) {
                                                case 1:
                                                        tmpmass = d;
#ifdef MODAL_SCALE_DATA
                                                        d *= scalemass;
#endif /* MODAL_SCALE_DATA */
                                                        FEMMass[iNode - 1] = d;
                                                        break;

                                                case 2:
                                                case 3:
                                                        if (d != tmpmass) {
                                                                silent_cout("Warning: component(" << jCnt << ") "
                                                                        "of FEM node(" << iNode << ") mass "
                                                                        "differs from component(1)=" << tmpmass << std::endl);
                                                        }
                                                        break;

                                                case 4:
                                                case 5:
                                                case 6:
#ifdef MODAL_SCALE_DATA
                                                        d *= scaleinertia;
#endif /* MODAL_SCALE_DATA */
                                                        FEMJ[iNode - 1](jCnt - 3) = d;
                                                        break;
                                                }
                                        }
                                }

                                bBuildInvariants = true;

                                bRecordGroup[MODAL_RECORD_11] = true;
                                } break;

                        case MODAL_RECORD_12: {
                                /* Rigid body inertia */
                                if (bRecordGroup[MODAL_RECORD_12]) {
                                        silent_cerr("file=\"" << sFileFEM << "\": "
                                                "\"RECORD GROUP 12\" already parsed"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bWriteBIN) {
                                        checkPoint = MODAL_RECORD_12;
                                        fbin.write(&checkPoint, sizeof(checkPoint));
                                }

                                fdat >> d;

                                if (bWriteBIN) {
                                        fbin.write((const char *)&d, sizeof(d));
                                }

                                dMass = d;

                                // NOTE: the CM location is read and temporarily stored in STmp
                                // later, it will be multiplied by the mass
                                for (int iRow = 1; iRow <= 3; iRow++) {
                                        fdat >> d;

                                        if (bWriteBIN) {
                                                fbin.write((const char *)&d, sizeof(d));
                                        }

                                        XTmpIn(iRow) = d;
                                }

                                // here JTmp is with respect to the center of mass
                                for (int iRow = 1; iRow <= 3; iRow++) {
                                        for (int iCol = 1; iCol <= 3; iCol++) {
                                                fdat >> d;

                                                if (bWriteBIN) {
                                                        fbin.write((const char *)&d, sizeof(d));
                                                }

                                                JTmpIn(iRow, iCol) = d;
                                        }
                                }

                                bRecordGroup[MODAL_RECORD_12] = true;
                                } break;

                        case MODAL_RECORD_13: {
                                /* Matrice di smorzamento modale */
                                if (bRecordGroup[MODAL_RECORD_13]) {
                                        silent_cerr("file=\"" << sFileFEM << "\": "
                                                "\"RECORD GROUP 13\" already parsed"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bActiveModes.empty()) {
                                        silent_cerr("Modal(" << uLabel << "): "
                                                "input file \"" << sFileFEM << "\" "
                                                "is bogus (RECORD GROUP 13)"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bWriteBIN) {
                                        checkPoint = MODAL_RECORD_13;
                                        fbin.write(&checkPoint, sizeof(checkPoint));
                                }

                                unsigned int iCnt = 1;
                                for (unsigned int jMode = 1; jMode <= NModesFEM; jMode++) {
                                        unsigned int jCnt = 1;

                                        for (unsigned int kMode = 1; kMode <= NModesFEM; kMode++) {
                                                fdat >> d;

                                                if (dDampingThreshold > 0.0 && fabs(d) < dDampingThreshold) {
                                                        d = 0.0;
                                                }

                                                if (bWriteBIN) {
                                                        fbin.write((const char *)&d, sizeof(d));
                                                }

                                                if (!bActiveModes[jMode] || !bActiveModes[kMode]) {
                                                        continue;
                                                }

                                                if (eDamp == DAMPING_FROM_FILE) {
                                                        oGenDamp.Put(iCnt, jCnt, d);
                                                }

                                                jCnt++;
                                        }

                                        if (bActiveModes[jMode]) {
                                                iCnt++;
                                        }
                                }

                                bRecordGroup[MODAL_RECORD_13] = true;
                                } break;
                        case MODAL_RECORD_14: {
                                if (bRecordGroup[MODAL_RECORD_14]) {
                                        silent_cerr("file=\"" << sFileFEM << "\": "
                                                "\"RECORD GROUP 14\" already parsed"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bActiveModes.empty()) {
                                        silent_cerr("Modal(" << uLabel << "): "
                                                "input file \"" << sFileFEM << "\" "
                                                "is bogus (RECORD GROUP 14)"
                                                << std::endl);
                                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                }

                                if (bWriteBIN) {
                                        checkPoint = MODAL_RECORD_14;
                                        fbin.write(&checkPoint, sizeof(checkPoint));
                                }

                                oInv3.Resize(NModes);
                                oInv3.Reset();

                                for (unsigned int iRow = 1; iRow <= 3; ++iRow) {
                                    unsigned int iMode = 1;
                                    for (unsigned int jMode = 1; jMode <= NModesFEM; ++jMode) {
                                        fdat >> d;

                                        if (bWriteBIN) {
                                            fbin.write((const char *)&d, sizeof(d));
                                        }

                                        if (!bActiveModes[jMode]) {
                                            continue;
                                        }

                                        oInv3.Put(iRow, iMode++, d);
                                    }
                                }

                                bRecordGroup[MODAL_RECORD_14] = true;
                                } break;
                        case MODAL_RECORD_15: {
                            if (bRecordGroup[MODAL_RECORD_15]) {
                                silent_cerr("file=\"" << sFileFEM << "\": "
                                            "\"RECORD GROUP 15\" already parsed"
                                            << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                            }

                            if (bActiveModes.empty()) {
                                silent_cerr("Modal(" << uLabel << "): "
                                            "input file \"" << sFileFEM << "\" "
                                            "is bogus (RECORD GROUP 15)"
                                            << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                            }

                            if (bWriteBIN) {
                                checkPoint = MODAL_RECORD_15;
                                fbin.write(&checkPoint, sizeof(checkPoint));
                            }

                            oInv4.Resize(NModes);
                            oInv4.Reset();

                            for (unsigned int iRow = 1; iRow <= 3; ++iRow) {
                                unsigned int iMode = 1;
                                for (unsigned int jMode = 1; jMode <= NModesFEM; ++jMode) {
                                    fdat >> d;

                                    if (bWriteBIN) {
                                        fbin.write((const char *)&d, sizeof(d));
                                    }

                                    if (!bActiveModes[jMode]) {
                                        continue;
                                    }

                                    oInv4.Put(iRow, iMode++, d);
                                }
                            }

                            bRecordGroup[MODAL_RECORD_15] = true;
                        } break;
                        case MODAL_RECORD_16: {
                            if (bRecordGroup[MODAL_RECORD_16]) {
                                silent_cerr("file=\"" << sFileFEM << "\": "
                                            "\"RECORD GROUP 16\" already parsed"
                                            << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                            }

                            if (bActiveModes.empty()) {
                                silent_cerr("Modal(" << uLabel << "): "
                                            "input file \"" << sFileFEM << "\" "
                                            "is bogus (RECORD GROUP 16)"
                                            << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                            }

                            if (bWriteBIN) {
                                checkPoint = MODAL_RECORD_16;
                                fbin.write(&checkPoint, sizeof(checkPoint));
                            }

                            oInv8.Resize(3 * NModes);
                            oInv8.Reset();

                            for (unsigned int iRow = 1; iRow <= 3; ++iRow) {
                                unsigned int iMode = 0;
                                for (unsigned int jMode = 1; jMode <= NModesFEM; ++jMode) {
                                    for (unsigned int iCol = 1; iCol <= 3; ++iCol) {
                                        fdat >> d;

                                        if (bWriteBIN) {
                                            fbin.write((const char *)&d, sizeof(d));
                                        }

                                        if (!bActiveModes[jMode]) {
                                            continue;
                                        }

                                        oInv8.Put(iRow, 3 * iMode + iCol, d);
                                    }

                                    if (bActiveModes[jMode]) {
                                        ++iMode;
                                    }
                                }
                            }

                            bRecordGroup[MODAL_RECORD_16] = true;
                        } break;
                        case MODAL_RECORD_17: {
                            if (bRecordGroup[MODAL_RECORD_17]) {
                                silent_cerr("file=\"" << sFileFEM << "\": "
                                            "\"RECORD GROUP 17\" already parsed"
                                            << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                            }

                            if (bActiveModes.empty()) {
                                silent_cerr("Modal(" << uLabel << "): "
                                            "input file \"" << sFileFEM << "\" "
                                            "is bogus (RECORD GROUP 17)"
                                            << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                            }

                            if (bWriteBIN) {
                                checkPoint = MODAL_RECORD_17;
                                fbin.write(&checkPoint, sizeof(checkPoint));
                            }

                            oInv5.Resize(3 * NModes * NModes);
                            oInv5.Reset();

                            for (unsigned int iRow = 1; iRow <= 3; ++iRow) {
                                unsigned int jMode = 0;
                                for (unsigned int j = 1; j <= NModesFEM; ++j) {
                                    unsigned int kMode = 0;
                                    for (unsigned int k = 1; k <= NModesFEM; ++k) {
                                        fdat >> d;

                                        if (bWriteBIN) {
                                            fbin.write((const char *)&d, sizeof(d));
                                        }

                                        if (!bActiveModes[j] || !bActiveModes[k]) {
                                            continue;
                                        }

                                        oInv5.Put(iRow, jMode * NModes + kMode + 1, d);

                                        ++kMode;
                                    }

                                    if (bActiveModes[j]) {
                                        ++jMode;
                                    }
                                }
                            }

                            bRecordGroup[MODAL_RECORD_17] = true;
                        } break;
                        case MODAL_RECORD_18: {
                            if (bRecordGroup[MODAL_RECORD_18]) {
                                silent_cerr("file=\"" << sFileFEM << "\": "
                                            "\"RECORD GROUP 18\" already parsed"
                                            << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                            }

                            if (bActiveModes.empty()) {
                                silent_cerr("Modal(" << uLabel << "): "
                                            "input file \"" << sFileFEM << "\" "
                                            "is bogus (RECORD GROUP 18)"
                                            << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                            }

                            if (bWriteBIN) {
                                checkPoint = MODAL_RECORD_18;
                                fbin.write(&checkPoint, sizeof(checkPoint));
                            }

                            oInv9.Resize(3 * 3 * NModes * NModes);
                            oInv9.Reset();

                            for (unsigned int iRow = 1; iRow <= 3; ++iRow) {
                                unsigned int jMode = 0;
                                for (unsigned int j = 1; j <= NModesFEM; ++j) {
                                    unsigned int kMode = 0;
                                    for (unsigned int k = 1; k <= NModesFEM; ++k) {
                                        for (unsigned int iCol = 1; iCol <= 3; ++iCol) {
                                            fdat >> d;

                                            if (bWriteBIN) {
                                                fbin.write((const char *)&d, sizeof(d));
                                            }

                                            if (!bActiveModes[j] || !bActiveModes[k]) {
                                                continue;
                                            }

                                            oInv9.Put(iRow, iCol + 3 * (jMode * NModes + kMode), d);
                                        }

                                        if (bActiveModes[k]) {
                                            ++kMode;
                                        }
                                    }

                                    if (bActiveModes[j]) {
                                        ++jMode;
                                    }
                                }
                            }

                            bRecordGroup[MODAL_RECORD_18] = true;
                        } break;
                        case MODAL_RECORD_19: {
                            if (bRecordGroup[MODAL_RECORD_19]) {
                                silent_cerr("file=\"" << sFileFEM << "\": "
                                            "\"RECORD GROUP 19\" already parsed"
                                            << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                            }

                            if (bActiveModes.empty()) {
                                 silent_cerr("Modal(" << uLabel << "): "
                                             "input file \"" << sFileFEM << "\" "
                                             "is bogus (RECORD GROUP 19)"
                                             << std::endl);
                                 throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                            }

                            if (bWriteBIN) {
                                 checkPoint = MODAL_RECORD_19;
                                 fbin.write(&checkPoint, sizeof(checkPoint));
                            }

                            unsigned int NStressStiff;

                            fdat >> NStressStiff;

                            if (!fdat) {
                                 silent_cerr("file=\"" << sFileFEM << "\": "
                                             "parse error in \"RECORD GROUP 19\""
                                             << std::endl);
                                 throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                            }

                            if (bWriteBIN) {
                                 fbin.write(reinterpret_cast<char*>(&NStressStiff), sizeof(NStressStiff));
                            }

                            ASSERT(rgGenStressStiffIdx.empty());
                            ASSERT(rgGenStressStiff.empty());

                            rgGenStressStiffIdx.reserve(NStressStiff);
                            rgGenStressStiff.reserve(NStressStiff);

                            for (unsigned int kCnt = 1; kCnt <= NStressStiff; ++kCnt) {
                                 unsigned uStressStiffIdx;

                                 fdat >> uStressStiffIdx;

                                 if (!fdat) {
                                      silent_cerr("file=\"" << sFileFEM << "\": "
                                                  "parse error in \"RECORD GROUP 19\""
                                                  << std::endl);
                                      throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                                 }

                                 if (bWriteBIN) {
                                      fbin.write(reinterpret_cast<char*>(&uStressStiffIdx), sizeof(uStressStiffIdx));
                                 }

                                 MatNxN oGenStressStiff(NModesFEM, 0.);

                                 unsigned int iCnt = 1;
                                 for (unsigned int jMode = 1; jMode <= NModesFEM; jMode++) {
                                      unsigned int jCnt = 1;

                                      for (unsigned int kMode = 1; kMode <= NModesFEM; kMode++) {
                                           fdat >> d;

                                           if (bWriteBIN) {
                                                fbin.write(reinterpret_cast<char*>(&d), sizeof(d));
                                           }

                                           if (!bActiveModes[jMode] || !bActiveModes[kMode]) {
                                                continue;
                                           }
                                           oGenStressStiff.Put(iCnt, jCnt, d);
                                           jCnt++;
                                      }

                                      if (bActiveModes[jMode]) {
                                           iCnt++;
                                      }
                                 }

                                 rgGenStressStiffIdx.push_back(uStressStiffIdx);
                                 rgGenStressStiff.emplace_back(std::move(oGenStressStiff));
                            }

                            bRecordGroup[MODAL_RECORD_19] = true;
                        } break;
                        default:
                                silent_cerr("file=\"" << sFileFEM << "\": "
                                        "\"RECORD GROUP " << rg << "\" unhandled"
                                        << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }
                }

                fdat.close();
                if (bWriteBIN) {
                        checkPoint = MODAL_END_OF_FILE;
                        fbin.write(&checkPoint, sizeof(checkPoint));
                        fbin.close();
                }

                /* unlink binary file if create/update failed */
                } catch (...) {
                        if (bWriteBIN) {
                                // NOTE: might be worth leaving in place
                                // for debugging purposes
                                (void)unlink(sBinFileFEM.c_str());
                        }
                        throw;
                }

                fname = sFileFEM;

        } else {
                std::ifstream fbin(sBinFileFEM.c_str(), std::ios::binary);
                if (!fbin) {
                        silent_cerr("Modal(" << uLabel << "): "
                                "unable to open file \"" << sBinFileFEM << "\""
                                "at line " << HP.GetLineData() << std::endl);
                        throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                }
                silent_cout("Modal(" << uLabel << "): "
                        "reading flexible body data from file "
                        "\"" << sBinFileFEM << "\"" << std::endl);

                /* check version */
                // NOTE: any time the format of the binary file changes,
                // the BinVersion should be incremented.
                // When reading the binary file, the version of the file
                // is compared with that of the code.  If they do not match:
                // if the file is newer than the code, abort.
                // if the file is older than the code, try to deal with it
                // as much as possible
                //
                // History:
                //
                // 1: first implementation
                //
                // 2: July 2008, at LaMSID
                //	- string FEM labels
                //	- 3 & 4 optional
                //	- 11 & 12 optional and no longer mutually exclusive
                //	- -1 as the last checkpoint
                //	- version 1 tolerated
                // 3: November 2012
                //	- record 13, generalized damping matrix
                // 4: December 2012; incompatible with previous ones
                //	- bin file portable (fixed size types, magic signature)
                char currMagic[5] = { 0 };
                fbin.read((char *)&currMagic, sizeof(4));
                if (memcmp(magic, currMagic, 4) != 0) {
                        silent_cerr("Modal(" << uLabel << "): "
                                        "file \"" << sBinFileFEM << "\" "
                                        "unexpected signature" << std::endl);
                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                }

                fbin.read((char *)&currBinVersion, sizeof(currBinVersion));
                if (currBinVersion > BinVersion) {
                        silent_cerr("Modal(" << uLabel << "): "
                                        "file \"" << sBinFileFEM << "\" "
                                        "version " << currBinVersion << " unsupported" << std::endl);
                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);

                } else if (currBinVersion < BinVersion) {
                        silent_cout("Modal(" << uLabel << "): "
                                        "file \"" << sBinFileFEM << "\" "
                                        "version " << currBinVersion << " is outdated; "
                                        "trying to read anyway..." << std::endl);
                }

                pedantic_cout("Modal(" << uLabel << "): "
                        "binary version " << currBinVersion << std::endl);

                // NOTE: the binary file is expected to be in order;
                // however, if generated from a non-ordered textual file
                // it will be out of order...  perhaps also the textual
                // file should always be ordered?

                /* legge il primo blocco (HEADER) */
                fbin.read(&checkPoint, sizeof(checkPoint));
                if (checkPoint != MODAL_RECORD_1) {
                        silent_cerr("Modal(" << uLabel << "): "
                                        "file \"" << sBinFileFEM << "\" "
                                        "looks broken (expecting checkpoint 1)"
                                        << std::endl);
                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                }

                pedantic_cout("Modal(" << uLabel << "): "
                        "reading block " << unsigned(checkPoint) << std::endl);

                fbin.read((char *)&NFEMNodesFEM, sizeof(NFEMNodesFEM));
                fbin.read((char *)&NModesFEM, sizeof(NModesFEM));

                /* consistency checks */
                if (NFEMNodes == unsigned(-1)) {
                        NFEMNodes = NFEMNodesFEM;

                } else if (NFEMNodes != NFEMNodesFEM) {
                        silent_cerr("Modal(" << uLabel << "), "
                                "file \"" << sFileFEM << "\": "
                                "FEM nodes number " << NFEMNodes
                                << " does not match node number "
                                << NFEMNodesFEM
                                << std::endl);
                        throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                }

                if (NModes != NModesFEM) {
                        silent_cout("Modal(" << uLabel
                                        << "), file '" << sFileFEM
                                        << "': using " << NModes
                                        << " of " << NModesFEM
                                        << " modes" << std::endl);
                }

                if (!bActiveModes.empty()) {
                        throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                }

                IdFEMNodes.resize(NFEMNodes);

                oXYZFEMNodes.Resize(NFEMNodes);
                oXYZFEMNodes.Reset();
                oModeShapest.Resize(NFEMNodes * NModes);
                oModeShapest.Reset();
                oModeShapesr.Resize(NFEMNodes * NModes);
                oModeShapesr.Reset();

                bActiveModes.resize(NModesFEM + 1, false);

                for (unsigned int iCnt = 0; iCnt < NModes; iCnt++) {
                        if (uModeNumber[iCnt] > NModesFEM) {
                                silent_cerr("Modal(" << uLabel << "): "
                                        "mode " << uModeNumber[iCnt]
                                        << " is not available (max = "
                                        << NModesFEM << ")"
                                        << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }
                        bActiveModes[uModeNumber[iCnt]] = true;
                }

                bRecordGroup[1] = true;

                for (;;) {
                        fbin.read(&checkPoint, sizeof(checkPoint));

                        if (checkPoint == MODAL_END_OF_FILE) {
                                pedantic_cout("Modal(" << uLabel << "): "
                                        "end of binary file \"" << sBinFileFEM << "\""
                                        << std::endl);
                                fbin.close();
                                break;
                        }

                        if (checkPoint < MODAL_RECORD_1 || checkPoint >= MODAL_LAST_RECORD) {
                                silent_cerr("Modal(" << uLabel << "): "
                                        "file \"" << sBinFileFEM << "\" "
                                        "looks broken (unknown block " << unsigned(checkPoint) << ")"
                                        << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                        if (bRecordGroup[unsigned(checkPoint)]) {
                                silent_cerr("Modal(" << uLabel << "): "
                                        "file \"" << sBinFileFEM << "\" "
                                        "looks broken (block " << unsigned(checkPoint) << " already parsed)"
                                        << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                        pedantic_cout("Modal(" << uLabel << "): "
                                "reading block " << unsigned(checkPoint)
                                << " from file \"" << sBinFileFEM << "\"" << std::endl);

                        switch (checkPoint) {
                        case MODAL_RECORD_2:
                                /* legge il secondo blocco (Id.nodi) */
                                for (unsigned int iNode = 0; iNode < NFEMNodes; iNode++) {
                                        uint32_t len;
                                        fbin.read((char *)&len, sizeof(len));
                                        ASSERT(len > 0);
                                        IdFEMNodes[iNode].resize(len);
                                        fbin.read((char *)IdFEMNodes[iNode].c_str(), len);
                                }
                                break;

                        case MODAL_RECORD_3:
                                /* deformate iniziali dei modi */
                                for (unsigned int iCnt = 1, jMode = 1; jMode <= NModesFEM; jMode++) {
                                        doublereal	d;

                                        fbin.read((char *)&d, sizeof(d));

                                        if (!bActiveModes[jMode]) {
                                                continue;
                                        }

                                        a.Put(iCnt, d);
                                        iCnt++;
                                }
                                break;

                        case MODAL_RECORD_4:
                                /* velocita' iniziali dei modi */
                                for (unsigned int iCnt = 1, jMode = 1; jMode <= NModesFEM; jMode++) {
                                        doublereal	d;

                                        fbin.read((char *)&d, sizeof(d));

                                        if (!bActiveModes[jMode]) {
                                                continue;
                                        }

                                        aP.Put(iCnt, d);
                                        iCnt++;
                                }
                                break;

                        case MODAL_RECORD_5:
                                /* Coordinate X dei nodi */
                                for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                                        doublereal	d;

                                        fbin.read((char *)&d, sizeof(d));

#ifdef MODAL_SCALE_DATA
                                        d *= scalemodes;
#endif /* MODAL_SCALE_DATA */

                                        oXYZFEMNodes.Put(1, iNode, d);
                                }
                                break;

                        case MODAL_RECORD_6:
                                /* Coordinate Y dei nodi*/
                                for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                                        doublereal	d;

                                        fbin.read((char *)&d, sizeof(d));

#ifdef MODAL_SCALE_DATA
                                        d *= scalemodes;
#endif /* MODAL_SCALE_DATA */

                                        oXYZFEMNodes.Put(2, iNode, d);
                                }
                                break;

                        case MODAL_RECORD_7:
                                /* Coordinate Z dei nodi*/
                                for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                                        doublereal	d;

                                        fbin.read((char *)&d, sizeof(d));

#ifdef MODAL_SCALE_DATA
                                        d *= scalemodes;
#endif /* MODAL_SCALE_DATA */

                                        oXYZFEMNodes.Put(3, iNode, d);
                                }
                                break;

                        case MODAL_RECORD_8:
                                /* Forme modali */
                                for (unsigned int iCnt = 1, jMode = 1; jMode <= NModesFEM; jMode++) {
                                        for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                                                doublereal n[6];

                                                fbin.read((char *)n, sizeof(n));

                                                if (!bActiveModes[jMode]) {
                                                        continue;
                                                }

#ifdef MODAL_SCALE_DATA
                                                n[0] *= scalemodes;
                                                n[1] *= scalemodes;
                                                n[2] *= scalemodes;
#endif /* MODAL_SCALE_DATA */
                                                oModeShapest.PutVec((iCnt - 1)*NFEMNodes + iNode, Vec3(&n[0]));
                                                oModeShapesr.PutVec((iCnt - 1)*NFEMNodes + iNode, Vec3(&n[3]));
                                        }

                                        if (bActiveModes[jMode]) {
                                                iCnt++;
                                        }
                                }
                                break;

                        case MODAL_RECORD_9:
                                /* Matrice di massa modale */
                                for (unsigned int iCnt = 1, jMode = 1; jMode <= NModesFEM; jMode++) {
                                        unsigned int jCnt = 1;

                                        for (unsigned int kMode = 1; kMode <= NModesFEM; kMode++) {
                                                doublereal	d;

                                                fbin.read((char *)&d, sizeof(d));

                                                if (!bActiveModes[jMode] || !bActiveModes[kMode]) {
                                                        continue;
                                                }
                                                oGenMass.Put(iCnt, jCnt, d);
                                                jCnt++;
                                        }

                                        if (bActiveModes[jMode]) {
                                                iCnt++;
                                        }
                                }
                                break;

                        case MODAL_RECORD_10:
                                /* Matrice di rigidezza  modale */
                                for (unsigned int iCnt = 1, jMode = 1; jMode <= NModesFEM; jMode++) {
                                        unsigned int jCnt = 1;

                                        for (unsigned int kMode = 1; kMode <= NModesFEM; kMode++) {
                                                doublereal	d;

                                                fbin.read((char *)&d, sizeof(d));

                                                if (!bActiveModes[jMode] || !bActiveModes[kMode]) {
                                                        continue;
                                                }
                                                oGenStiff.Put(iCnt, jCnt, d);
                                                jCnt++;
                                        }

                                        if (bActiveModes[jMode]) {
                                                iCnt++;
                                        }
                                }
                                break;

                        case MODAL_RECORD_11:
                                /* Lumped Masses */
                                FEMMass.resize(NFEMNodes);
                                FEMJ.resize(NFEMNodes);

                                for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                                        for (unsigned int jCnt = 1; jCnt <= 6; jCnt++) {
                                                doublereal	d;

                                                fbin.read((char *)&d, sizeof(d));

                                                switch (jCnt) {
                                                case 1:
#ifdef MODAL_SCALE_DATA
                                                        d *= scalemass;
#endif /* MODAL_SCALE_DATA */
                                                        FEMMass[iNode - 1] = d;
                                                        break;

                                                case 4:
                                                case 5:
                                                case 6:
#ifdef MODAL_SCALE_DATA
                                                        d *= scaleinertia;
#endif /* MODAL_SCALE_DATA */
                                                        FEMJ[iNode - 1](jCnt - 3) = d;
                                                        break;
                                                }
                                        }
                                }

                                bBuildInvariants = true;
                                break;

                        case MODAL_RECORD_12: {
                                doublereal      d;
                                fbin.read((char *)&d, sizeof(d));
                                dMass = d;

                                // NOTE: the CM location is read and temporarily stored in XTmpIn
                                // later, it will be multiplied by the mass
                                for (int iRow = 1; iRow <= 3; iRow++) {
                                        fbin.read((char *)&d, sizeof(d));

                                        XTmpIn(iRow) = d;
                                }

                                // here JTmpIn is with respect to the center of mass
                                for (int iRow = 1; iRow <= 3; iRow++) {
                                        for (int iCol = 1; iCol <= 3; iCol++) {
                                                fbin.read((char *)&d, sizeof(d));

                                                JTmpIn(iRow, iCol) = d;
                                        }
                                }
                                } break;

                        case MODAL_RECORD_13:
                                for (unsigned int iCnt = 1, jMode = 1; jMode <= NModesFEM; jMode++) {
                                        unsigned int jCnt = 1;

                                        for (unsigned int kMode = 1; kMode <= NModesFEM; kMode++) {
                                                doublereal	d;

                                                fbin.read((char *)&d, sizeof(d));

                                                if (!bActiveModes[jMode] || !bActiveModes[kMode]) {
                                                        continue;
                                                }

                                                if (eDamp == DAMPING_FROM_FILE) {
                                                        oGenDamp.Put(iCnt, jCnt, d);
                                                }

                                                jCnt++;
                                        }

                                        if (bActiveModes[jMode]) {
                                                iCnt++;
                                        }
                                }
                                break;

                        case MODAL_RECORD_14:
                             oInv3.Resize(NModes);
                             oInv3.Reset();

                                for (unsigned int iRow = 1; iRow <= 3; ++iRow) {
                                        unsigned int iMode = 1;
                                        for (unsigned int jMode = 1; jMode <= NModesFEM; ++jMode) {
                                                doublereal d;
                                                fbin.read((char*)&d, sizeof(d));

                                                if (!bActiveModes[jMode]) {
                                                        continue;
                                                }

                                                oInv3.Put(iRow, iMode++, d);
                                        }
                                }
                                break;
                        case MODAL_RECORD_15:
                             oInv4.Resize(NModes);
                             oInv4.Reset();

                                for (unsigned int iRow = 1; iRow <= 3; ++iRow) {
                                        unsigned int iMode = 1;
                                        for (unsigned int jMode = 1; jMode <= NModesFEM; ++jMode) {
                                                doublereal d;
                                                fbin.read((char *)&d, sizeof(d));

                                                if (!bActiveModes[jMode]) {
                                                        continue;
                                                }

                                                oInv4.Put(iRow, iMode++, d);
                                        }
                                }
                                break;
                        case MODAL_RECORD_16:
                             oInv8.Resize(3 * NModes);
                             oInv8.Reset();

                                for (unsigned int iRow = 1; iRow <= 3; ++iRow) {
                                        unsigned int iMode = 0;
                                        for (unsigned int jMode = 1; jMode <= NModesFEM; ++jMode) {
                                                for (unsigned int iCol = 1; iCol <= 3; ++iCol) {
                                                        doublereal d;
                                                        fbin.read((char *)&d, sizeof(d));

                                                        if (!bActiveModes[jMode]) {
                                                                continue;
                                                        }

                                                        oInv8.Put(iRow, 3 * iMode + iCol, d);
                                                }

                                                if (bActiveModes[jMode]) {
                                                        ++iMode;
                                                }
                                        }
                                }
                                break;
                        case MODAL_RECORD_17:
                             oInv5.Resize(3 * NModes * NModes);
                             oInv5.Reset();

                            for (unsigned int iRow = 1; iRow <= 3; ++iRow) {
                                unsigned int jMode = 0;
                                for (unsigned int j = 1; j <= NModesFEM; ++j) {
                                    unsigned int kMode = 0;
                                    for (unsigned int k = 1; k <= NModesFEM; ++k) {
                                        doublereal d;

                                        fbin.read((char *)&d, sizeof(d));

                                        if (!bActiveModes[j] || !bActiveModes[k]) {
                                            continue;
                                        }

                                        oInv5.Put(iRow, jMode * NModes + kMode + 1, d);

                                        ++kMode;
                                    }

                                    if (bActiveModes[j]) {
                                        ++jMode;
                                    }
                                }
                            }
                            break;
                        case MODAL_RECORD_18:
                             oInv9.Resize(3 * 3 * NModes * NModes);
                             oInv9.Reset();

                            for (unsigned int iRow = 1; iRow <= 3; ++iRow) {
                                unsigned int jMode = 0;
                                for (unsigned int j = 1; j <= NModesFEM; ++j) {
                                    unsigned int kMode = 0;
                                    for (unsigned int k = 1; k <= NModesFEM; ++k) {
                                        for (unsigned int iCol = 1; iCol <= 3; ++iCol) {
                                            doublereal d;

                                            fbin.read((char *)&d, sizeof(d));

                                            if (!bActiveModes[j] || !bActiveModes[k]) {
                                                continue;
                                            }

                                            oInv9.Put(iRow, iCol + 3 * (jMode * NModes + kMode), d);
                                        }

                                        if (bActiveModes[k]) {
                                            ++kMode;
                                        }
                                    }

                                    if (bActiveModes[j]) {
                                        ++jMode;
                                    }
                                }
                            }
                            break;
                        case MODAL_RECORD_19: {
                             unsigned int NStressStiff;

                             fbin.read(reinterpret_cast<char*>(&NStressStiff), sizeof(NStressStiff));

                             ASSERT(rgGenStressStiffIdx.empty());
                             ASSERT(rgGenStressStiff.empty());

                             rgGenStressStiffIdx.reserve(NStressStiff);
                             rgGenStressStiff.reserve(NStressStiff);

                             for (unsigned int kCnt = 1; kCnt <= NStressStiff; ++kCnt) {
                                  unsigned uStressStiffIdx;

                                  fbin.read(reinterpret_cast<char*>(&uStressStiffIdx), sizeof(uStressStiffIdx));

                                  MatNxN oGenStressStiff(NModesFEM, 0.);
                                  for (unsigned int iCnt = 1, jMode = 1; jMode <= NModesFEM; jMode++) {
                                       unsigned int jCnt = 1;

                                       for (unsigned int kMode = 1; kMode <= NModesFEM; kMode++) {
                                            doublereal	d;

                                            fbin.read(reinterpret_cast<char *>(&d), sizeof(d));

                                            if (!bActiveModes[jMode] || !bActiveModes[kMode]) {
                                                 continue;
                                            }
                                            oGenStressStiff.Put(iCnt, jCnt, d);
                                            jCnt++;
                                       }

                                       if (bActiveModes[jMode]) {
                                            iCnt++;
                                       }
                                  }

                                  rgGenStressStiffIdx.push_back(uStressStiffIdx);
                                  rgGenStressStiff.emplace_back(std::move(oGenStressStiff));
                             }
                        } break;
                        default:
                                silent_cerr("Modal(" << uLabel << "): "
                                        "file \"" << sBinFileFEM << "\" "
                                        "looks broken (unknown block " << unsigned(checkPoint) << ")"
                                        << std::endl);
                                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                        bRecordGroup[unsigned(checkPoint)] = true;
                }

                fname = sBinFileFEM;
        }

        unsigned reqMR[] = {
                MODAL_RECORD_1,
                MODAL_RECORD_2,
                // 3 & 4 no longer required; explicit check is present when currBinVersion == 1
                MODAL_RECORD_5,
                MODAL_RECORD_6,
                MODAL_RECORD_7,
                MODAL_RECORD_8,
                MODAL_RECORD_9,
                MODAL_RECORD_10
        };
        bool bBailOut(false);
        for (unsigned iCnt = 0; iCnt < sizeof(reqMR)/sizeof(unsigned); iCnt++) {
                if (!bRecordGroup[reqMR[iCnt]]) {
                        silent_cerr("Modal(" << uLabel << "): "
                                "incomplete input file \"" << fname << "\", "
                                "record group " << reqMR[iCnt] << " missing "
                                "at line " << HP.GetLineData() << std::endl);
                        bBailOut = true;
                }
        }

        if (bBailOut) {
                throw ErrGeneric(MBDYN_EXCEPT_ARGS);
        }

        // JTmp is now referred to the origin
        JTmp = JTmpIn - Mat3x3(MatCrossCross, XTmpIn, XTmpIn*dMass);

        if (!sEchoFileName.empty()) {
                std::ofstream fecho(sEchoFileName.c_str());

                fecho.setf(std::ios::scientific);
                fecho.precision(iEchoPrecision);

                time_t t;
                time(&t);

                fecho
                        << "** echo of file \"" << fname << "\" generated " << ctime(&t)
                        << "** RECORD GROUP 1, HEADER" << std::endl
                        << "**   REVISION.  NODES.  NORMAL, ATTACHMENT, CONSTRAINT, REJECTED MODES." << std::endl
                        << "VER" << currBinVersion << " " << NFEMNodes << " " << NModes << " 0 0 0" << std::endl
                        << "**" << std::endl
                        << "** RECORD GROUP 2, FINITE ELEMENT NODE LIST" << std::endl;
                for (unsigned r = 0; r <= (IdFEMNodes.size() - 1)/6; r++) {
                        for (unsigned c = 0; c < std::min(6U, unsigned(IdFEMNodes.size() - 6*r)); c++) {
                                fecho << IdFEMNodes[6*r + c] << " ";
                        }
                        fecho << std::endl;
                }

                fecho
                        << "**" << std::endl
                        << "** RECORD GROUP 3, INITIAL MODAL DISPLACEMENTS"<< std::endl;
                for (int r = 1; r <= a.iGetNumRows(); r++) {
                        fecho << a(r) << std::endl;
                }

                fecho
                        << "**" << std::endl
                        << "** RECORD GROUP 4, INITIAL MODAL VELOCITIES"<< std::endl;
                for (int r = 1; r <= aP.iGetNumRows(); r++) {
                        fecho << aP(r) << std::endl;
                }

                fecho
                        << "**" << std::endl
                        << "** RECORD GROUP 5, NODAL X COORDINATES" << std::endl;
                for (int r = 1; r <= oXYZFEMNodes.iGetNumCols(); r++) {
                        fecho << oXYZFEMNodes(1, r) << std::endl;
                }

                fecho
                        << "**" << std::endl
                        << "** RECORD GROUP 6, NODAL Y COORDINATES" << std::endl;
                for (int r = 1; r <= oXYZFEMNodes.iGetNumCols(); r++) {
                        fecho << oXYZFEMNodes(2, r) << std::endl;
                }

                fecho
                        << "**" << std::endl
                        << "** RECORD GROUP 7, NODAL Z COORDINATES" << std::endl;
                for (int r = 1; r <= oXYZFEMNodes.iGetNumCols(); r++) {
                        fecho << oXYZFEMNodes(3, r) << std::endl;
                }

                fecho
                        << "**" << std::endl
                        << "** RECORD GROUP 8, NON-ORTHOGONALIZED MODE SHAPES" << std::endl;
                for (unsigned m = 0; m < NModes; m++) {
                        fecho
                                << "**    NORMAL MODE SHAPE # " << uModeNumber[m] << std::endl;
                        for (unsigned n = 1; n <= NFEMNodes; n++) {
                                fecho
                                        << oModeShapest(1, m*NFEMNodes + n) << " "
                                        << oModeShapest(2, m*NFEMNodes + n) << " "
                                        << oModeShapest(3, m*NFEMNodes + n) << " "
                                        << oModeShapesr(1, m*NFEMNodes + n) << " "
                                        << oModeShapesr(2, m*NFEMNodes + n) << " "
                                        << oModeShapesr(3, m*NFEMNodes + n) << std::endl;
                        }
                }

                fecho
                        << "**" << std::endl
                        << "** RECORD GROUP 9, MODAL MASS MATRIX.  COLUMN-MAJOR FORM" << std::endl;
                for (unsigned r = 1; r <= NModes; r++) {
                        for (unsigned c = 1; c <= NModes; c++) {
                                fecho << oGenMass(r, c) << " ";
                        }
                        fecho << std::endl;
                }

                fecho
                        << "**" << std::endl
                        << "** RECORD GROUP 10, MODAL STIFFNESS MATRIX.  COLUMN-MAJOR FORM" << std::endl;
                for (unsigned r = 1; r <= NModes; r++) {
                        for (unsigned c = 1; c <= NModes; c++) {
                                fecho << oGenStiff(r, c) << " ";
                        }
                        fecho << std::endl;
                }

                if (bBuildInvariants) {
                        fecho
                                << "**" << std::endl
                                << "** RECORD GROUP 11, DIAGONAL OF LUMPED MASS MATRIX" << std::endl;
                        for (unsigned r = 0; r < FEMMass.size(); r++) {
                                fecho
                                        << FEMMass[r] << " " << FEMMass[r] << " " << FEMMass[r] << " "
                                        << FEMJ[r] << std::endl;
                        }
                }

                if (bRecordGroup[MODAL_RECORD_12]) {
                        fecho
                                << "**" << std::endl
                                << "** RECORD GROUP 12, GLOBAL INERTIA" << std::endl
                                << dMass << std::endl
                                << XTmpIn << std::endl,
                                JTmpIn.Write(fecho, " ", "\n") << std::endl;
                }

                if (bRecordGroup[MODAL_RECORD_13]) {
                        fecho
                                << "**" << std::endl
                                << "** RECORD GROUP 13, MODAL DAMPING MATRIX.  COLUMN-MAJOR FORM" << std::endl;
                        for (unsigned r = 1; r <= NModes; r++) {
                                for (unsigned c = 1; c <= NModes; c++) {
                                        fecho << oGenDamp(r, c) << " ";
                                }
                                fecho << std::endl;
                        }
                }
        }

        /* lettura dati di vincolo:
         * l'utente specifica la label del nodo FEM e del nodo rigido
         * d'interfaccia.
         * L'orientamento del nodo FEM e' quello del nodo modale, la
         * posizione e' la somma di quella modale e di quella FEM   */

        /* array contenente le forme modali dei nodi d'interfaccia */
        Mat3xN oPHItStrNode;
        Mat3xN oPHIrStrNode;

        /* traslazione origine delle coordinate */
        Vec3 Origin(::Zero3);
        bool bOrigin(false);

        if (HP.IsKeyWord("origin" "node")) {
                /* numero di nodi FEM del modello */
                std::string FEMOriginNode;
                if (HP.IsStringWithDelims()) {
                        FEMOriginNode = HP.GetStringWithDelims();

                } else {
                        pedantic_cerr("Modal(" << uLabel << "): "
                                "origin node expected as string with delimiters"
                                << std::endl);
                        FEMOriginNode = HP.GetString("");
                }

                unsigned int iNode;
                for (iNode = 0; iNode < NFEMNodes; iNode++) {
                        if (FEMOriginNode == IdFEMNodes[iNode]) {
                                break;
                        }
                }

                if (iNode == NFEMNodes) {
                        silent_cerr("Modal(" << uLabel << "): "
                                "FEM node \"" << FEMOriginNode << "\""
                                << " at line " << HP.GetLineData()
                                << " not defined " << std::endl);
                        throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                }

                iNode++;
                Origin = oXYZFEMNodes.GetVec(iNode);

                pedantic_cout("Modal(" << uLabel << "): origin x={" << Origin << "}" << std::endl);
                bOrigin = true;

        } else if (HP.IsKeyWord("origin" "position")) {
                Origin = HP.GetPosAbs(::AbsRefFrame);
                bOrigin = true;
        }

        if (bOrigin) {
                for (unsigned int iStrNode = 1; iStrNode <= NFEMNodes; iStrNode++) {
                        oXYZFEMNodes.SubVec(iStrNode, Origin);
                }

                if (!bBuildInvariants) {
                        XTmpIn -= Origin;
                        JTmp = JTmpIn - Mat3x3(MatCrossCross, XTmpIn, XTmpIn*dMass);
                }
        }

        /* numero di nodi d'interfaccia */
        const unsigned int NStrNodes = HP.GetInt();
        DEBUGCOUT("Number of Interface Nodes : " << NStrNodes << std::endl);

        ASSERT(rgGenStressStiff.size() == rgGenStressStiffIdx.size());

        for (unsigned i = 0u; i < rgGenStressStiffIdx.size(); ++i) {
             const unsigned uIndex = rgGenStressStiffIdx[i];

             if (!(uIndex >= 1u && uIndex <= 12u + NStrNodes * 6u)) {
                  silent_cerr("Modal(" << uLabel << "): invalid index for record group 19\n"
                              "expected [1:" << (12u + NStrNodes * 6) << "]\n"
                              "got" << uIndex << " at line "
                              << HP.GetLineData() << "\n");
                  throw ErrGeneric(MBDYN_EXCEPT_ARGS);
             }
        }

        oPHItStrNode.Resize(NStrNodes * NModes);
        oPHItStrNode.Reset();
        oPHIrStrNode.Resize(NStrNodes * NModes);
        oPHIrStrNode.Reset();

        std::vector<Modal::StrNodeData> SND(NStrNodes);

        bool bOut = false;
        if (HP.IsKeyWord("output")) {
                bOut = HP.GetYesNoOrBool();
        }

        doublereal dTol = 1e-12;
        if (HP.IsKeyWord("interface" "tolerance")) {
                dTol = HP.GetReal();
                if (dTol == 0.0) {
                        // error

                } else if (dTol < 0.0) {
                        // error
                }
        }

        for (unsigned int iStrNode = 1; iStrNode <= NStrNodes; iStrNode++) {
                unsigned int iNode = 0;
                std::string Node1;
                doublereal dCurrTol = dTol;
                if (HP.IsKeyWord("find" "closest")) {
                        // find FEM node closest to MB one later, after MB node is input
                        if (HP.IsKeyWord("interface" "tolerance")) {
                                dCurrTol = HP.GetReal();
                                if (dCurrTol == 0.0) {
                                        // error

                                } else if (dCurrTol < 0.0) {
                                        // error
                                }
                        }

                } else {
                        /* nodo collegato 1 (e' il nodo FEM) */
                        Node1 = HP.GetString("");
                        // NOTE: we should check whether a label includes
                        // whitespace.  In any case, it wouldn't match
                        // any of the labels read, so it is pointless,
                        // but could help debugging...
                        if (Node1.find(' ') != std::string::npos) {
                                silent_cout("Modal(" << uLabel << "): "
                                        "FEM node \"" << Node1 << "\""
                                        << " at line " << HP.GetLineData()
                                        << " contains a blank" << std::endl);
                        }

                        DEBUGCOUT("Linked to FEM Node " << Node1 << std::endl);

                        /* verifica di esistenza del nodo 1*/
                        for (iNode = 0; iNode < NFEMNodes; iNode++) {
                                if (Node1 == IdFEMNodes[iNode]) {
                                        break;
                                }
                        }

                        if (iNode == NFEMNodes) {
                                silent_cerr("Modal(" << uLabel << "): "
                                        "FEM node \"" << Node1 << "\""
                                        << " at line " << HP.GetLineData()
                                        << " not defined " << std::endl);
                                throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                        iNode++;
                }

                /* nodo collegato 2 (e' il nodo multibody) */
                unsigned int uNode2 = (unsigned int)HP.GetInt();
                DEBUGCOUT("Linked to Multi-Body Node " << uNode2 << std::endl);

                /* verifica di esistenza del nodo 2 */
                SND[iStrNode - 1].pNode = pDM->pFindNode<const StructNode, Node::STRUCTURAL>(uNode2);
                if (SND[iStrNode - 1].pNode == 0) {
                        silent_cerr("Modal(" << uLabel << "): "
                                "StructuralNode(" << uNode2 << ") "
                                "at line " << HP.GetLineData()
                                << " not defined" << std::endl);
                        throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                }

                if (pDM->bUseAutoDiff()) {
                     SND[iStrNode - 1].pNodeAd = &dynamic_cast<const StructNodeAd&>(*SND[iStrNode - 1].pNode);
                }

                /* offset del nodo Multi-Body */
                ReferenceFrame RF = ReferenceFrame(SND[iStrNode - 1].pNode);
                Vec3 d2(HP.GetPosRel(RF));
                Mat3x3 R2(::Eye3);
                bool bRot(false);
                if (HP.IsKeyWord("hinge")) {
                        silent_cerr("Modal(" << uLabel << "): "
                                "warning, keyword \"hinge\" for StructuralNode(" << uNode2 << ") "
                                "deprecated; use \"orientation\" instead "
                                "at line " << HP.GetLineData() << std::endl);
                        bRot = true;

                } else if (HP.IsKeyWord("orientation")) {
                        bRot = true;
                }

                if (bRot) {
                        R2 = HP.GetRotRel(RF);
                }

                SND[iStrNode - 1].OffsetMB = d2;
                SND[iStrNode - 1].RotMB = R2;	// FIXME: not used (so far)

                DEBUGCOUT("Multibody Node reference frame d2: " << std::endl
                                << d2 << std::endl);

                // find closest
                if (iNode == 0) {
                        // FIXME: origin? orientation?
                        // NOTE: the first node whose distance is below tolerance is accepted, not the absolute closest!
                        Vec3 xMBRel(R.MulTV(SND[iStrNode - 1].pNode->GetXCurr() + SND[iStrNode - 1].OffsetMB - X0));

                        bool bGotIt(false);
                        unsigned int iFirst = 0;
                        doublereal dNorm = 2*dCurrTol;
                        for (iNode = 0; iNode < NFEMNodes; iNode++) {
                                Vec3 xFEMRel(oXYZFEMNodes.GetVec(iNode + 1));
                                doublereal d = (xFEMRel - xMBRel).Norm();
                                if (d < dCurrTol) {
                                        if (!bGotIt) {
                                                bGotIt = true;
                                                iFirst = iNode;
                                                dNorm = d;

                                                pedantic_cout("Modal(" << uLabel << "): "
                                                        "FEM node #" << iNode << " (\"" << IdFEMNodes[iNode] << "\")"
                                                        " is close enough to multibody node \"" << SND[iStrNode - 1].pNode->GetLabel() << "\", distance=" << d
                                                        << " at line " << HP.GetLineData() << std::endl);

                                        } else {
                                                if (d >= dNorm) {
                                                        pedantic_cerr("Modal(" << uLabel << "): "
                                                                "warning, FEM node #" << iNode << " (\"" << IdFEMNodes[iNode] << "\")"
                                                                " is also close enough to multibody node \"" << SND[iStrNode - 1].pNode->GetLabel() << "\", distance=" << d
                                                                << " but not as close as FEM node #" << iFirst << " (\"" << IdFEMNodes[iFirst] << "\"), distance=" << dNorm
                                                                << " at line " << HP.GetLineData() << std::endl);
                                                } else {
                                                        pedantic_cerr("Modal(" << uLabel << "): "
                                                                "warning, FEM node #" << iNode << " (\"" << IdFEMNodes[iNode] << "\")"
                                                                " is also close enough to multibody node \"" << SND[iStrNode - 1].pNode->GetLabel() << "\", distance=" << d
                                                                << " and closer than FEM node #" << iFirst << " (\"" << IdFEMNodes[iFirst] << "\"), distance=" << dNorm
                                                                << " at line " << HP.GetLineData() << std::endl);
                                                        iFirst = iNode;
                                                        dNorm = d;
                                                }
                                        }
                                }
                        }

                        if (!bGotIt) {
                                silent_cerr("Modal(" << uLabel << "): "
                                        "no FEM node close enough to multibody node \"" << SND[iStrNode - 1].pNode->GetLabel() << "\" was found"
                                        << " at line " << HP.GetLineData() << std::endl);
                                throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                        }

                        iNode = iFirst;

                        // label of FEM node
                        Node1 = IdFEMNodes[iNode];

                        silent_cout("Modal(" << uLabel << "): "
                                "multibody node \"" << SND[iStrNode - 1].pNode->GetLabel() << "\""
                                " matched with FEM node \"" << Node1 << "\", distance=" << dNorm
                                << " at line " << HP.GetLineData() << std::endl);

                        iNode++;
                }

                int iNodeCurr = iNode;

                /* recupera la posizione del nodo FEM, somma di posizione
                 * e eventuale offset;
                 *
                 * HEADS UP: non piu' offset per i nodi FEM !!!!!!!!!
                 *
                 * nota: iNodeCurr contiene la posizione a cui si trova
                 * il nodo FEM d'interfaccia nell'array pXYZNodes */
                SND[iStrNode-1].OffsetFEM = oXYZFEMNodes.GetVec(iNodeCurr);

                /* salva le forme modali del nodo d'interfaccia
                 * nell'array pPHIStrNode */
                for (unsigned int jMode = 0; jMode < NModes; jMode++) {
                        oPHItStrNode.PutVec(jMode*NStrNodes + iStrNode,
                                            oModeShapest.GetVec(jMode*NFEMNodes + iNodeCurr));
                        oPHIrStrNode.PutVec(jMode*NStrNodes + iStrNode,
                                            oModeShapesr.GetVec(jMode*NFEMNodes + iNodeCurr));
                }

                /* salva le label dei nodi vincolati nell'array IntNodes;
                 * puo' servire per il restart? */
                SND[iStrNode - 1].FEMNode = Node1;

                if (HP.IsKeyWord("output")) {
                        SND[iStrNode - 1].bOut = HP.GetYesNoOrBool();
                }

                const Vec3& xMB(SND[iStrNode - 1].pNode->GetXCurr());
                pedantic_cout("Interface node " << iStrNode << ":" << std::endl
                                << "        MB node " << uNode2 << " x={" << xMB << "}" << std::endl);
                Vec3 xFEMRel(oXYZFEMNodes.GetVec(iNodeCurr));
                Vec3 xFEM(X0 + R*xFEMRel);
                pedantic_cout("        FEM node \"" << Node1 << "\" x={" << xFEM << "} "
                        "xrel={" << xFEMRel << "}" << std::endl);
                pedantic_cout("        offset={" << xFEM - xMB << "}" << std::endl);
        }  /* fine ciclo sui nodi d'interfaccia */

        if (bOut) {
                std::vector<Modal::StrNodeData>::iterator i = SND.begin();
                std::vector<Modal::StrNodeData>::iterator end = SND.end();
                for (; i != end; ++i) {
                        i->bOut = bOut;
                }
        }

        /* fine ciclo caricamento dati */

        /*
         * calcola gli invarianti d'inerzia (massa, momenti statici
         * e d'inerzia, termini di accoppiamento nel SdR locale)
         */

        /* NOTE: right now, either they are all used (except for Inv9)
         * or none is used, and the global inertia of the body is expected */
        if (bBuildInvariants) {
                doublereal dMassInv = 0.;
                Vec3 STmpInv(::Zero3);
                Mat3x3 JTmpInv(::Zero3x3);

                MatNxN GenMass(NModes, 0.);

                /* TODO: select what terms are used */

                if (!oInv3.iGetNumCols()) {
                     oInv3.Resize(NModes);
                     oInv3.Reset();
                }

                ASSERT(static_cast<unsigned>(oInv3.iGetNumCols()) == NModes);

                if (!oInv4.iGetNumCols()) {
                     oInv4.Resize(NModes);
                     oInv4.Reset();
                }

                ASSERT(static_cast<unsigned>(oInv4.iGetNumCols()) == NModes);

                /* NOTE: we assume that Inv5 is used only if Inv4 is used as well */
                /* Inv5 e' un 3xMxM */
                if (!oInv5.iGetNumCols()) {
                     oInv5.Resize(NModes * NModes);
                     oInv5.Reset();
                }

                ASSERT(static_cast<unsigned>(oInv5.iGetNumCols()) == NModes * NModes);

                if (!oInv8.iGetNumCols()) {
                /* Inv8 e' un 3x3xM */
                     oInv8.Resize(3 * NModes);
                     oInv8.Reset();
                }

                ASSERT(static_cast<unsigned>(oInv8.iGetNumCols()) == 3 * NModes);

                /* NOTE: we assume that Inv9 is used only if Inv8 is used as well */
                /* by default: no */
                if (HP.IsKeyWord("use" "invariant" "9")) {
                     if (!oInv9.iGetNumCols()) {
                        /* Inv9 e' un 3x3xMxM */
                          oInv9.Resize(3 * NModes * NModes);
                          oInv9.Reset();
                }

                    ASSERT(static_cast<unsigned>(oInv9.iGetNumCols()) == 3 * NModes * NModes);
                }
                /* Inv10 e' un 3x3xM */
                oInv10.Resize(3 * NModes);
                oInv10.Reset();
                oInv11.Resize(NModes);
                oInv11.Reset();

                /* inizio ciclo scansione nodi */
                for (unsigned int iNode = 1; iNode <= NFEMNodes; iNode++) {
                        doublereal mi = FEMMass[iNode - 1];

                        /* massa totale (Inv 1) */
                        dMassInv += mi;

                        /* posizione nodi FEM */
                        Vec3 ui = oXYZFEMNodes.GetVec(iNode);

                        Mat3x3 uiWedge(MatCross, ui);
                        Mat3x3 JiNodeTmp(::Zero3x3);

                        JiNodeTmp(1, 1) = FEMJ[iNode - 1](1);
                        JiNodeTmp(2, 2) = FEMJ[iNode - 1](2);
                        JiNodeTmp(3, 3) = FEMJ[iNode - 1](3);

                        JTmpInv += JiNodeTmp - Mat3x3(MatCrossCross, ui, ui*mi);
                        STmpInv += ui*mi;

                        /* estrae le forme modali del nodo i-esimo */
                        for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                                unsigned int iOffset = (jMode - 1)*NFEMNodes + iNode;

                                oPHIti.PutVec(jMode, oModeShapest.GetVec(iOffset));
                                oPHIri.PutVec(jMode, oModeShapesr.GetVec(iOffset));
                        }

                        /* TODO: only build what is required */

                        Mat3xN Inv3Tmp(NModes, 0.);
                        Mat3xN Inv4Tmp(NModes, 0.);
                        Mat3xN Inv4JTmp(NModes, 0.);
                        Inv3Tmp.Copy(oPHIti);

                        /* Inv3 = mi*PHIti,      i = 1,...nnodi */
                        Inv3Tmp *= mi;

                        /* Inv4 = mi*ui/\*PHIti + Ji*PHIri, i = 1,...nnodi */
                        Inv4Tmp.LeftMult(uiWedge*mi, oPHIti);
                        Inv4JTmp.LeftMult(JiNodeTmp, oPHIri);
                        Inv4Tmp += Inv4JTmp;
                        oInv3 += Inv3Tmp;
                        oInv4 += Inv4Tmp;
                        oInv11 += Inv4JTmp;

                        /* inizio ciclo scansione modi */
                        for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                                Vec3 PHItij = oPHIti.GetVec(jMode);
                                Vec3 PHIrij = oPHIri.GetVec(jMode);

                                Mat3x3 PHItijvett_mi(MatCross, PHItij*mi);
                                Mat3xN Inv5jTmp(NModes, 0);

                                /* Inv5 = mi*PHItij/\*PHIti,
                                 * i = 1,...nnodi, j = 1,...nmodi */
                                Inv5jTmp.LeftMult(PHItijvett_mi, oPHIti);
                                for (unsigned int kMode = 1; kMode <= NModes; kMode++)  {
                                        oInv5.AddVec((jMode - 1)*NModes + kMode,
                                                        Inv5jTmp.GetVec(kMode));

                                        /* compute the modal mass matrix
                                         * using the FEM inertia and the
                                         * mode shapes */
                                        GenMass(jMode, kMode) += (PHItij*oPHIti.GetVec(kMode))*mi
                                                + PHIrij*(JiNodeTmp*oPHIri.GetVec(kMode));
                                }

                                /* Inv8 = -mi*ui/\*PHItij/\,
                                 * i = 1,...nnodi, j = 1,...nmodi */
                                Mat3x3 Inv8jTmp = -uiWedge*PHItijvett_mi;
                                oInv8.AddMat3x3((jMode - 1)*3 + 1, Inv8jTmp);

                                /* Inv9 = mi*PHItij/\*PHItik/\,
                                 * i = 1,...nnodi, j, k = 1...nmodi */
                                if (oInv9.iGetNumCols()) {
                                        for (unsigned int kMode = 1; kMode <= NModes; kMode++) {
                                                Mat3x3 PHItikvett(MatCross, oPHIti.GetVec(kMode));
                                                Mat3x3 Inv9jkTmp = PHItijvett_mi*PHItikvett;

                                                oInv9.AddMat3x3((jMode - 1)*3*NModes + (kMode - 1)*3 + 1, Inv9jkTmp);
                                        }
                                }

                                /* Inv10 = [PHIrij/\][J0i],
                                 * i = 1,...nnodi, j = 1,...nmodi */
                                Mat3x3 Inv10jTmp = PHIrij.Cross(JiNodeTmp);
                                oInv10.AddMat3x3((jMode - 1)*3 + 1, Inv10jTmp);
                        } /*  fine ciclo scansione modi */
                } /* fine ciclo scansione nodi */

                if (bRecordGroup[12]) {
                        Mat3x3 DJ = JTmp - JTmpInv;
                        pedantic_cerr("  Rigid-body mass: "
                                "input - computed" << std::endl
                                << "    " << dMass - dMassInv << std::endl);
                        pedantic_cerr("  Rigid-body CM location: "
                                "input - computed" << std::endl
                                << "    " << XTmpIn - STmpInv/dMassInv << std::endl);
                        pedantic_cerr("  Rigid-body inertia: "
                                "input - computed" << std::endl
                                << "    " << DJ.GetVec(1) << std::endl
                                << "    " << DJ.GetVec(2) << std::endl
                                << "    " << DJ.GetVec(3) << std::endl);
                }

                /*
                 * TODO: check modal mass
                 */
                pedantic_cerr("  Generalized mass: input - computed" << std:: endl);
                for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                        pedantic_cerr("   ");
                        for (unsigned int kMode = 1; kMode <= NModes; kMode++) {
                                pedantic_cerr(" " << oGenMass.dGet(jMode, kMode) - GenMass(jMode, kMode));
                        }
                        pedantic_cerr(std::endl);
                }

                // if record 12 was read, leave its data in place,
                // otherwise replace rigid-body invariants with those from record 12
                if (!bRecordGroup[12]) {
                        dMass = dMassInv;
                        STmp = STmpInv;
                        JTmp = JTmpInv;
                }
        }

        // if record 12 was read, fix static moment
        if (bRecordGroup[12]) {
                /* left over when reading XCG */
                STmp = XTmpIn*dMass;
        }

        /*
         * TODO: Check rank of modal stiffness matrix
         */

        // check if mass matrix is symmetric and diagonal
        bool bIsMSym = true;
        bool bIsMDiag = true;
        for (unsigned iRow = 2; iRow <= NModes; iRow++) {
                for (unsigned iCol = 1; iCol < iRow; iCol++) {
                        doublereal mrc = oGenMass.dGet(iRow, iCol);
                        doublereal mcr = oGenMass.dGet(iCol, iRow);
                        if (mrc != mcr) {
                                if (bIsMSym) {
                                        silent_cerr("Modal(" << uLabel << "): mass matrix is not symmetric: (at least) "
                                                "M(" << iRow << ", " << iCol << ")=" << mrc << " "
                                                "!= "
                                                "M(" << iCol << ", " << iRow << ")=" << mcr << " "
                                                << std::endl);
                                }
                                bIsMSym = false;
                        }

                        if (mrc != 0. || mcr != 0.) {
                                if (bIsMDiag) {
                                        silent_cerr("Modal(" << uLabel << "): mass matrix is not diagonal: (at least) "
                                                "M(" << iRow << ", " << iCol << ")=" << mrc << " "
                                                "and/or "
                                                "M(" << iCol << ", " << iRow << ")=" << mcr << " "
                                                "!= 0.0"
                                                << std::endl);
                                }
                                bIsMDiag = false;
                        }
                }
        }

        bool bIsKSym = true;
        bool bIsKDiag = true;
        for (unsigned iRow = 2; iRow <= NModes; iRow++) {
                for (unsigned iCol = 1; iCol < iRow; iCol++) {
                        doublereal mrc = oGenStiff.dGet(iRow, iCol);
                        doublereal mcr = oGenStiff.dGet(iCol, iRow);
                        if (mrc != mcr) {
                                if (bIsKSym) {
                                        silent_cerr("Modal(" << uLabel << "): stiffness matrix is not symmetric: (at least) "
                                                "K(" << iRow << ", " << iCol << ")=" << mrc << " "
                                                "!= "
                                                "K(" << iCol << ", " << iRow << ")=" << mcr << " "
                                                << std::endl);
                                }
                                bIsKSym = false;
                        }

                        if (mrc != 0. || mcr != 0.) {
                                if (bIsKDiag) {
                                        silent_cerr("Modal(" << uLabel << "): stiffness matrix is not diagonal: (at least) "
                                                "K(" << iRow << ", " << iCol << ")=" << mrc << " "
                                                "and/or "
                                                "K(" << iCol << ", " << iRow << ")=" << mcr << " "
                                                "!= 0.0"
                                                << std::endl);
                                }
                                bIsKDiag = false;
                        }
                }
        }

        if (eDamp == DAMPING_FROM_FILE) {
                bool bIsCSym = true;
                bool bIsCDiag = true;
                for (unsigned iRow = 2; iRow <= NModes; iRow++) {
                        for (unsigned iCol = 1; iCol < iRow; iCol++) {
                                doublereal mrc = oGenDamp.dGet(iRow, iCol);
                                doublereal mcr = oGenDamp.dGet(iCol, iRow);
                                if (mrc != mcr) {
                                        if (bIsCSym) {
                                                silent_cerr("Modal(" << uLabel << "): damping matrix is not symmetric: (at least) "
                                                        "C(" << iRow << ", " << iCol << ") "
                                                        "!= "
                                                        "C(" << iCol << ", " << iRow << ") "
                                                        << std::endl);
                                        }
                                        bIsCSym = false;
                                }

                                if (mrc != 0. || mcr != 0.) {
                                        if (bIsCDiag) {
                                                silent_cerr("Modal(" << uLabel << "): damping matrix is not diagonal: (at least) "
                                                        "C(" << iRow << ", " << iCol << ") "
                                                        "and/or "
                                                        "C(" << iCol << ", " << iRow << ") "
                                                        "!= 0.0"
                                                        << std::endl);
                                        }
                                        bIsCDiag = false;
                                }
                        }
                }

        } else {
                /*
                 * costruisce la matrice di smorzamento:
                 * il termine diagonale i-esimo e' pari a
                 * cii = 2*cdampi*(ki*mi)^.5
                 */

                switch (eDamp) {
                case DAMPING_DIAG:
                case DAMPING_SINGLE_FACTOR:
                        if (!bIsMDiag || !bIsKDiag) {
                                silent_cerr("Modal(" << uLabel << "): "
                                        "warning, " << sDamp[eDamp]
                                        << " with non-diagonal mass and/or stiffness matrix" << std::endl);
                        }

                        for (unsigned int iCnt = 1; iCnt <= NModes; iCnt++) {
                                doublereal k = oGenStiff.dGet(iCnt, iCnt);
                                doublereal m = oGenMass.dGet(iCnt, iCnt);
                                doublereal d = sqrt(k*m);

                                if (eDamp == DAMPING_DIAG) {
                                        oGenDamp.Put(iCnt, iCnt, 2.*DampRatios[iCnt - 1]*d);

                                } else if (eDamp == DAMPING_SINGLE_FACTOR) {
                                        oGenDamp.Put(iCnt, iCnt, 2.*damp_factor*d);
                                }
                        }
                        break;

                case DAMPING_RAYLEIGH:
                                for (unsigned int iRow = 1; iRow <= NModes; iRow++) {
                                        for (unsigned int iCol = 1; iCol <= NModes; iCol++) {
                                                doublereal m = oGenMass.dGet(iRow, iCol);
                                                doublereal k = oGenStiff.dGet(iRow, iCol);
                                                oGenDamp.Put(iRow, iCol, damp_coef_M*m + damp_coef_K*k);
                                        }
                                }
                        break;

                default:
                        // nothing to do
                        break;
                }
        }

        if (pedantic_output) {
                std::ostream &out = std::cout;

                out << "  Total Mass: " << dMass << std::endl;
                out << "  Inertia Matrix (referred to modal node): " << std::endl
                        << "    ", JTmp.Write(out, " ", "\n    ") << std::endl;
                out << "  Static Moment Vector: " << STmp << std::endl;

                out << "  Generalized Stiffness: " << std::endl;
                for (unsigned int iCnt = 1; iCnt <= NModes; iCnt++) {
                        out << "   ";
                        for (unsigned int jCnt = 1; jCnt <= NModes; jCnt++) {
                                out << " " << oGenStiff.dGet(iCnt, jCnt);
                        }
                        out << std::endl;
                }

                out << "  Generalized Mass: " << std::endl;
                for (unsigned int iCnt = 1; iCnt <= NModes; iCnt++) {
                        out << "   ";
                        for (unsigned int jCnt = 1; jCnt <= NModes; jCnt++) {
                                out << " " << oGenMass.dGet(iCnt, jCnt);
                        }
                        out << std::endl;
                }

                out << "  Generalized Damping: " << std::endl;
                for (unsigned int iCnt = 1; iCnt <= NModes; iCnt++) {
                        out << "   ";
                        for (unsigned int jCnt = 1; jCnt <= NModes; jCnt++) {
                                out << " " << oGenDamp.dGet(iCnt, jCnt);
                        }
                        out << std::endl;
                }

                if (oInv3.iGetNumCols()) {
                        out << "  Inv3: " << std::endl;
                        for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                                out << "   ";
                                for (unsigned int jCnt = 1; jCnt <= NModes; jCnt++) {
                                        out << " " << oInv3.dGet(iCnt, jCnt);
                                }
                                out << std::endl;
                        }
                } else {
                        out << "  Inv3: unused" << std::endl;
                }

                if (oInv4.iGetNumCols()) {
                        out << "  Inv4: " << std::endl;
                        for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                                out << "   ";
                                for (unsigned int jCnt = 1; jCnt <= NModes; jCnt++) {
                                        out << " " << oInv4.dGet(iCnt, jCnt);
                                }
                                out << std::endl;
                        }
                } else {
                        out << "  Inv4: unused" << std::endl;
                }

                if (oInv5.iGetNumCols()) {
                        for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                                out << "  Inv5j(j=" << jMode << "): " << std::endl;
                                for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                                        out << "   ";
                                        for (unsigned int kMode = 1; kMode <= NModes; kMode++) {
                                                out << " " << oInv5.dGet(iCnt, (jMode - 1)*NModes + kMode);
                                        }
                                        out << std::endl;
                                }
                        }
                } else {
                        out << "  Inv5: unused" << std::endl;
                }

                if (oInv8.iGetNumCols()) {
                        for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                                out << "  Inv8j(j=" << jMode << "): " << std::endl;
                                for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                                        out << "   ";
                                        for (unsigned int jCnt = 1; jCnt <= 3; jCnt++) {
                                                out << " " << oInv8.dGet(iCnt, (jMode - 1)*3 + jCnt);
                                        }
                                        out << std::endl;
                                }
                        }
                } else {
                        out << "  Inv8: unused" << std::endl;
                }

                if (oInv9.iGetNumCols()) {
                        for (unsigned int jMode = 1; jMode <= NModes; jMode++) {
                                for (unsigned int kMode = 1; kMode <= NModes; kMode++) {
                                        out << "  Inv9jk(j=" << jMode << ",k=" << kMode << "): " << std::endl;
                                        for (unsigned int iCnt = 1; iCnt <= 3; iCnt++) {
                                                out << "   ";
                                                for (unsigned int jCnt = 1; jCnt <= 3; jCnt++) {
                                                        out << " " << oInv9.dGet(iCnt, (jMode - 1)*3*NModes + (kMode - 1)*3 + jCnt);
                                                }
                                                out << std::endl;
                                        }
                                }
                        }
                } else {
                        out << "  Inv9: unused" << std::endl;
                }
        }

        if (HP.IsStringWithDelims()) {
                const char *sTmp = HP.GetFileName();
                silent_cout("Modal(" << uLabel << "): warning, the syntax changed "
                        "since 1.2.7; the output now occurs to a common \".mod\" file, "
                        "the per-element file \"" << sTmp << "\" is no longer required, "
                        "and will actually be ignored." << std::endl);
        }
        flag fOut = pDM->fReadOutput(HP, Elem::JOINT);

        if (bInitialValues) {
                for (unsigned int iCnt = 0; iCnt < NModes; iCnt++) {
                        a.Put(iCnt + 1, InitialValues[iCnt]);
                        aP.Put(iCnt + 1, InitialValuesP[iCnt]);
                }
        }

        if (pDM->bUseAutoDiff()) {
                SAFENEWWITHCONSTRUCTOR(pEl,
                                       ModalAd,
                                       ModalAd(uLabel,
                                               dynamic_cast<const ModalNodeAd*>(pModalNode),
                                               X0,
                                               R,
                                               pDO,
                                               NModes,
                                               NStrNodes,
                                               NFEMNodes,
                                               dMass,
                                               STmp,
                                               JTmp,
                                               std::move(uModeNumber),
                                               std::move(oGenMass),
                                               std::move(oGenStiff),
                                               std::move(oGenDamp),
                                               std::move(IdFEMNodes),
                                               std::move(oXYZFEMNodes),
                                               std::move(SND),
                                               std::move(oPHItStrNode),
                                               std::move(oPHIrStrNode),
                                               std::move(oModeShapest),
                                               std::move(oModeShapesr),
                                               std::move(oInv3),
                                               std::move(oInv4),
                                               std::move(oInv5),
                                               std::move(oInv8),
                                               std::move(oInv9),
                                               std::move(oInv10),
                                               std::move(oInv11),
                                               std::move(a),
                                               std::move(aP),
                                               rgGenStressStiffIdx,
                                               std::move(rgGenStressStiff),
                                               fOut));
        } else {
                SAFENEWWITHCONSTRUCTOR(pEl,
                                       Modal,
                                       Modal(uLabel,
                                             pModalNode,
                                             X0,
                                             R,
                                             pDO,
                                             NModes,
                                             NStrNodes,
                                             NFEMNodes,
                                             dMass,
                                             STmp,
                                             JTmp,
                                             std::move(uModeNumber),
                                             std::move(oGenMass),
                                             std::move(oGenStiff),
                                             std::move(oGenDamp),
                                             std::move(IdFEMNodes),
                                             std::move(oXYZFEMNodes),
                                             std::move(SND),
                                             std::move(oPHItStrNode),
                                             std::move(oPHIrStrNode),
                                             std::move(oModeShapest),
                                             std::move(oModeShapesr),
                                             std::move(oInv3),
                                             std::move(oInv4),
                                             std::move(oInv5),
                                             std::move(oInv8),
                                             std::move(oInv9),
                                             std::move(oInv10),
                                             std::move(oInv11),
                                             std::move(a),
                                             std::move(aP),
                                             fOut));
        }

        if (fOut) {
                if (pDM->pGetOutHdl()->UseText(OutputHandler::MODAL)) {
                        pDM->OutputOpen(OutputHandler::MODAL);
                } else {
                        silent_cerr("Modal(" << uLabel << "): warning, requested output but text output is disabled."
                                        << " NetCDF output of Modal Joint is not implemented yet." << std::endl;);
                }
        }

        std::ostream& os = pDM->GetLogFile();

        // If we do not cast the label to integer, the output will be UINT_MAX if no modal node is defined
        os << "modal: " << pEl->GetLabel() << " "
                << static_cast<integer>(pModalNode ? pModalNode->GetLabel() : -1) << " "
                << dMass << " "
                << (dMass == 0. ? ::Zero3 : STmp / dMass) << " "
                << JTmp << std::endl;

        return pEl;
}
