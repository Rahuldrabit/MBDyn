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

#include "mbconfig.h"           /* This goes first in every *.c,*.cc file */

#include <sstream>

#include "mynewmem.h"
#include "scalarnode.h"
#include "nodead.h"
#include "solman.h"

/* ScalarNode - begin */

ScalarNode::ScalarNode(unsigned int uL, const DofOwner* pDO, flag fOut)
: Node(uL, pDO, fOut)
{
	NO_OP;
}

ScalarNode::~ScalarNode(void)
{
	NO_OP;
}

RestartData::RestartEntity ScalarNode::GetRestartEntity() const
{
     RestartData::RestartEntity eEntity;
     
     switch (GetNodeType()) {
     case Node::ABSTRACT:
          eEntity = RestartData::NODES_ABSTRACT;
          break;
     case Node::ELECTRIC:
          eEntity = RestartData::NODES_ELECTRIC;
          break;
     case Node::THERMAL:
          eEntity = RestartData::NODES_THERMAL;
          break;
     case Node::HYDRAULIC:
          eEntity = RestartData::NODES_HYDRAULIC;
          break;
     case Node::PARAMETER:
          eEntity = RestartData::NODES_PARAMETER;
          break;
     default:
          ASSERT(0);
          throw ErrNotImplementedYet(MBDYN_EXCEPT_ARGS);
     }

     return eEntity;
}

void
ScalarNode::OutputPrepare_int(OutputHandler& OH, bool bDifferential)
{
	if (bToBeOutput()) {
#ifdef USE_NETCDF
		if (OH.UseNetCDF(GetOutputType())) {
			ASSERT(OH.IsOpen(OutputHandler::NETCDF));

			const char *type = 0;
			switch (GetOutputType()) {
			case OutputHandler::ELECTRIC:
				type = "elec"; // FIXME: use ShortDesc!
				break;

			case OutputHandler::ABSTRACT:
				type = "abstr"; // FIXME: use ShortDesc!
				break;

			case OutputHandler::THERMALNODES:
				type = "thermal"; // FIXME: use ShortDesc!
				break;

			case OutputHandler::PRESNODES:
				type = "pres"; // FIXME: use ShortDesc!
				break;

			default:
				silent_cerr("Unhandled scalar node type" << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}

			const char *ad = bDifferential ? "differential" : "algebraic";

			std::ostringstream os;
			os << "node." << type << "." << GetLabel();
			(void)OH.CreateVar(os.str(), ad);

			// node sub-data
			m_sOutputNameBase = os.str();
		}
#endif // USE_NETCDF
	}
}

/* default output: do nothing */
std::ostream&
ScalarNode::Output(std::ostream& out) const
{
	return out;
}

/* Scrive l'output come abstract */
void
ScalarNode::Output(OutputHandler& OH) const
{
	(void)Output(OH.Get(GetOutputType()));
}

unsigned int
ScalarNode::iGetNumDof(void) const
{
	return 1;
}

void
ScalarNode::AfterPredict(VectorHandler& X, VectorHandler& XP)
{
	Update(X, XP);
}

const OutputHandler::Dimensions 
ScalarNode::GetEquationDimension(integer index) const {
   // DOF == 2
   OutputHandler::Dimensions dimension = OutputHandler::Dimensions::UnknownDimension;

	switch (index)
	{
		case 1:
			dimension = OutputHandler::Dimensions::UnknownDimension;
			break;
	}

	return dimension;
}

std::ostream&
ScalarNode::DescribeEq(std::ostream& out, const char *prefix, bool bInitial) const
{

	integer iIndex = iGetFirstIndex();

	out
		<< prefix << iIndex + 1 << ": " <<
			"scalar node equation" << std::endl;

	return out;
}
/* ScalarNode - end */


/* ScalarDifferentialNode - begin */

ScalarDifferentialNode::ScalarDifferentialNode(unsigned int uL,
	const DofOwner* pDO,
	const doublereal& dx,
	const doublereal& dxp,
	flag fOut)
: ScalarNode(uL, pDO, fOut), dX(dx), dXP(dxp), dXPrev(dx), dXPPrev(dxp)
{
	NO_OP;
}

ScalarDifferentialNode::~ScalarDifferentialNode(void)
{
	NO_OP;
}

/* Tipo di nodo */
Node::Type
ScalarDifferentialNode::GetNodeType(void) const
{
	/* Should be Node::SCALAR; keep using Node::ABSTRACT
	 * for backward compatibility */
	return Node::ABSTRACT;
}

/* esegue operazioni sui dof di proprieta' dell'elemento
 * in particolare ritorna il tipo di Dof in base all'indice i. Di default
 * i Dof dei nodi sono assunti differenziali */
DofOrder::Order 
ScalarDifferentialNode::GetDofType(unsigned int i) const
{
	ASSERT(i < iGetNumDof());
	return DofOrder::DIFFERENTIAL;
}

/* Restituisce il valore del dof iDof;
 * se differenziale, iOrder puo' essere = 1 per la derivata */
const doublereal&
ScalarDifferentialNode::dGetDofValue(int iDof, int iOrder) const
{
	ASSERT(iDof == 1);
	ASSERT(iOrder == 0 || iOrder == 1);
	if (iOrder == 0) {
		return dX;
	}
	return dXP;
}

/* Restituisce il valore del dof iDof al passo precedente;
 * se differenziale, iOrder puo' essere = 1 per la derivata */
const doublereal&
ScalarDifferentialNode::dGetDofValuePrev(int iDof, int iOrder) const
{
	ASSERT(iDof == 1);
	ASSERT(iOrder == 0 || iOrder == 1);
	if (iOrder == 0) {
		return dXPrev;
	}
	return dXPPrev;
}

/* Setta il valore del dof iDof a dValue;
 * se differenziale, iOrder puo' essere = 1 per la derivata */
void
ScalarDifferentialNode::SetDofValue(const doublereal& dValue,
	unsigned int iDof,
	unsigned int iOrder)
{
	ASSERT(iDof == 1);
	ASSERT(iOrder == 0 || iOrder == 1);
	if (iOrder == 0) {
		dX = dValue;
	} else if (iOrder == 1) {
		dXP = dValue;
	}
}

/* Funzioni "spurie": consentono l'accesso ai dati privati;
 * sono state definite perche' i nodi astratti sono usati nei
 * modi piu' strani e quindi puo' essere necessario l'accesso */
void
ScalarDifferentialNode::SetX(const doublereal& d)
{
	dX = d;
}

/* only for differential nodes!?! */
void
ScalarDifferentialNode::SetXPrime(const doublereal& d)
{
	dXP = d;
}

void
ScalarDifferentialNode::SetValue(DataManager *pDM,
	VectorHandler& X, VectorHandler& XP,
	SimulationEntity::Hints *ph)
{
	integer iIndex = iGetFirstIndex();
	X.PutCoef(iIndex + 1, dX);
	XP.PutCoef(iIndex + 1, dXP);
}

void
ScalarDifferentialNode::Update(const class VectorHandler& X,
	const class VectorHandler& XP)
{
	integer iFirstIndex = iGetFirstIndex() + 1;

	dXPrev = dX;
	dXPPrev = dXP;

	dX = X(iFirstIndex);
	dXP = XP(iFirstIndex);
}

std::ostream&
ScalarDifferentialNode::Output(std::ostream& out) const
{
	out << std::setw(8) << GetLabel()
		<< " " << dX
		<< " " << dXP
		<< std::endl;

	return out;
}

/*
 * Each node should prepend its type
 */
std::ostream&
ScalarDifferentialNode::Restart(std::ostream& out) const
{
	out << "  " << psReadNodesNodes[GetNodeType()]
		<< ": " << GetLabel();

	if (!GetName().empty()) {
		out << ", name, \"" << GetName() << "\"";
	}

	return out << ", value, " << dX
		<< ", derivative, " << dXP << ";" << std::endl;
}

void ScalarDifferentialNode::Restart(RestartData& oData, RestartData::RestartAction eAction)
{
     const auto eEntity = GetRestartEntity();
     oData.Sync(eEntity, GetLabel(), "dX", dX, eAction);
     oData.Sync(eEntity, GetLabel(), "dXP", dXP, eAction);
}

void
ScalarDifferentialNode::OutputPrepare_int(OutputHandler& OH,
	const std::string& var_name,
	const OutputHandler::Dimensions var_dim,
	const std::string& var_desc,
	const std::string& varP_name,
	const OutputHandler::Dimensions varP_dim,
	const std::string& varP_desc)
{
	ScalarNode::OutputPrepare_int(OH, true);

#ifdef USE_NETCDF
	if (OH.UseNetCDF(GetOutputType())) {
		ASSERT(OH.IsOpen(OutputHandler::NETCDF));
		ASSERT(!m_sOutputNameBase.empty());

		ScalarDifferentialNode::m_Var_dX = OH.CreateVar<doublereal>(m_sOutputNameBase + "." + var_name,
			var_dim, var_desc);

		ScalarDifferentialNode::m_Var_dXP = OH.CreateVar<doublereal>(m_sOutputNameBase + "." + varP_name,
			varP_dim, varP_desc);
	}
#endif // USE_NETCDF
}

void
ScalarDifferentialNode::OutputPrepare(OutputHandler& OH)
{
	if (fToBeOutput()) {
		OutputPrepare_int(OH,
			"X", OutputHandler::Dimensions::Dimensionless, "State",
			"XP", OutputHandler::Dimensions::Frequency, "State time derivative");
	}
}

void
ScalarDifferentialNode::Output(OutputHandler& OH) const
{
	if (bToBeOutput()) {
#ifdef USE_NETCDF
		if (OH.UseNetCDF(GetOutputType())) {
			OH.WriteNcVar(m_Var_dX, dX);
			OH.WriteNcVar(m_Var_dXP, dXP);
		}
#endif /* USE_NETCDF */

		if (OH.UseText(GetOutputType())) {
			Output(OH.Get(GetOutputType()));
		}
	}
}

unsigned int
ScalarDifferentialNode::iGetNumPrivData(void) const
{
	return 2;
}

unsigned int
ScalarDifferentialNode::iGetPrivDataIdx(const char *s) const
{
	ASSERT(s != 0);

	/*
	 *	"x"	=>	1
	 *	"xP"	=>	2
	 */

	if (s[0] != 'x') {
		return 0;
	}

	if (s[1] == '\0') {
		return 1;
	}

	if (s[1] == 'P' && s[2] == '\0') {
		return 2;
	}

	return 0;
}

doublereal
ScalarDifferentialNode::dGetPrivData(unsigned int i) const
{
	switch (i) {
	case 1:
		return dX;

	case 2:
		return dXP;

	}

	throw ErrGeneric(MBDYN_EXCEPT_ARGS);
}

const OutputHandler::Dimensions 
ScalarDifferentialNode::GetEquationDimension(integer index) const {
   // DOF == 2
   OutputHandler::Dimensions dimension = OutputHandler::Dimensions::UnknownDimension;

	switch (index)
	{
		case 1:
			dimension = OutputHandler::Dimensions::UnknownDimension;
			break;
	}

	return dimension;
}

std::ostream&
ScalarDifferentialNode::DescribeEq(std::ostream& out, const char *prefix, bool bInitial) const
{

	integer iIndex = iGetFirstIndex();

	out
		<< prefix << iIndex + 1 << ": " <<
			"scalar differential node equation" << std::endl;

	return out;
}

/* ScalarDifferentialNode - end */


/* ScalarAlgebraicNode - begin */

ScalarAlgebraicNode::ScalarAlgebraicNode(unsigned int uL,
                                         const DofOwner* pDO,
                                         doublereal dx,
                                         DofOrder::Equality eEqualityType,
                                         flag fOut)
: ScalarNode(uL, pDO, fOut), dX(dx), dXPrev(dx), eEqualityType(eEqualityType)
{
	NO_OP;
}

ScalarAlgebraicNode::~ScalarAlgebraicNode(void)
{
	NO_OP;
}

/* Tipo di nodo */
Node::Type
ScalarAlgebraicNode::GetNodeType(void) const
{
	/* Should be Node::SCALAR; keep using Node::ABSTRACT
	 * for backward compatibility */
	return Node::ABSTRACT;
}

/* esegue operazioni sui dof di proprieta' dell'elemento
 * in particolare ritorna il tipo di Dof in base all'indice i. Di default
 * i Dof dei nodi sono assunti differenziali */
DofOrder::Order
ScalarAlgebraicNode::GetDofType(unsigned int i) const
{
	ASSERT(i < iGetNumDof());
	return DofOrder::ALGEBRAIC;
}

DofOrder::Equality ScalarAlgebraicNode::GetEqualityType(unsigned int i) const
{
     ASSERT(i < iGetNumDof());
     
     return eEqualityType;
}

/* Restituisce il valore del dof iDof;
 * se differenziale, iOrder puo' essere = 1 per la derivata */
const doublereal&
ScalarAlgebraicNode::dGetDofValue(int iDof, int iOrder) const
{
	ASSERT(iDof == 1);
	ASSERT(iOrder == 0);
	return dX;
}

/* Restituisce il valore del dof iDof al passo precedente;
 * se differenziale, iOrder puo' essere = 1 per la derivata */
const doublereal&
ScalarAlgebraicNode::dGetDofValuePrev(int iDof, int iOrder) const
{
	ASSERT(iDof == 1);
	ASSERT(iOrder == 0);
	return dXPrev;
}

/* Setta il valore del dof iDof a dValue;
 * se differenziale, iOrder puo' essere = 1 per la derivata */
void
ScalarAlgebraicNode::SetDofValue(const doublereal& dValue,
	unsigned int iDof,
	unsigned int iOrder)
{
	ASSERT(iDof == 1);
	ASSERT(iOrder == 0);
	dX = dValue;
}

/* Funzioni "spurie": consentono l'accesso ai dati privati;
 * sono state definite perche' i nodi astratti sono usati nei
 * modi piu' strani e quindi puo' essere necessario l'accesso */
void
ScalarAlgebraicNode::SetX(const doublereal& d)
{
	dX = d;
}


/* only for differential nodes!?! */
void
ScalarAlgebraicNode::SetXPrime(const doublereal& /* d */ )
{
	DEBUGCERR("Error, setting derivative from algebraic dof" << std::endl);
	throw Node::ErrGeneric(MBDYN_EXCEPT_ARGS);
}

void
ScalarAlgebraicNode::SetValue(DataManager *pDM,
	VectorHandler& X, VectorHandler& /* XP */ ,
	SimulationEntity::Hints *ph)
{
	integer iIndex = iGetFirstIndex();
	X.PutCoef(iIndex + 1, dX);
}

void
ScalarAlgebraicNode::Update(const class VectorHandler& X,
	const class VectorHandler& /* XP */ )
{
	integer iFirstIndex = iGetFirstIndex()+1;

	dXPrev = dX;
	dX = X(iFirstIndex);
}

std::ostream&
ScalarAlgebraicNode::Output(std::ostream& out) const
{
	out << std::setw(8) << GetLabel()
		<< " " << dX << std::endl;

	return out;
}

/*
 * Each node should prepend its type
 */
std::ostream&
ScalarAlgebraicNode::Restart(std::ostream& out) const
{
	out << "  " << psReadNodesNodes[GetNodeType()]
		<< ": " << GetLabel();

	if (!GetName().empty()) {
		out << ", name, \"" << GetName() << "\"";
	}

	return out << ", value, " << dX << ";" << std::endl;
}

void ScalarAlgebraicNode::Restart(RestartData& oData, RestartData::RestartAction eAction)
{
     const auto eEntity = GetRestartEntity();
     oData.Sync(eEntity, GetLabel(), "dX", dX, eAction);
}

void
ScalarAlgebraicNode::OutputPrepare_int(OutputHandler& OH,
	const std::string& var_name,
	const OutputHandler::Dimensions var_dim,
	const std::string& var_desc)
{
	ScalarNode::OutputPrepare_int(OH, false);

#ifdef USE_NETCDF
	if (OH.UseNetCDF(GetOutputType())) {
		ASSERT(OH.IsOpen(OutputHandler::NETCDF));
		ASSERT(!m_sOutputNameBase.empty());

		ScalarAlgebraicNode::m_Var_dX = OH.CreateVar<doublereal>(m_sOutputNameBase + "." + var_name,
			var_dim, var_desc);
	}
#endif // USE_NETCDF
}

void
ScalarAlgebraicNode::OutputPrepare(OutputHandler& OH)
{
	if (fToBeOutput()) {
		OutputPrepare_int(OH,
			"X", OutputHandler::Dimensions::Dimensionless, "State");
	}
}

void
ScalarAlgebraicNode::Output(OutputHandler& OH) const
{
	if (bToBeOutput()) {
#ifdef USE_NETCDF
		if (OH.UseNetCDF(GetOutputType())) {
			OH.WriteNcVar(m_Var_dX, dX);
		}
#endif /* USE_NETCDF */

		if (OH.UseText(GetOutputType())) {
			Output(OH.Get(GetOutputType()));
		}
	}
}

unsigned int
ScalarAlgebraicNode::iGetPrivDataIdx(const char *s) const
{
	ASSERT(s != 0);

	/*
	 *	"x"	=>	1
	 */

	if (s[0] == 'x' && s[1] == '\0') {
		return 1;
	}

	return 0;
}

/* ScalarAlgebraicNode - end */


/* ParameterNode - begin */

ParameterNode::ParameterNode(unsigned int uL,
	const DofOwner* pDO,
	doublereal dx,
	flag fOut)
     : ScalarNode(uL, pDO, fOut),
       ScalarAlgebraicNode(uL, pDO, dx, DofOrder::EQUALITY, fOut)
{
	NO_OP;
}

ParameterNode::~ParameterNode(void)
{
	NO_OP;
}

/* Tipo del nodo (usato solo per debug ecc.) */
Node::Type
ParameterNode::GetNodeType(void) const
{
	return Node::PARAMETER;
}

unsigned int
ParameterNode::iGetNumDof(void) const
{
	return 0;
}

DofOrder::Order
ParameterNode::GetDofType(unsigned int i) const
{
	throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	return DofOrder::UNKNOWN;
}

/* Output di default per nodi di cui non si desidera output */
void
ParameterNode::Output(OutputHandler& OH) const
{
	if (bToBeOutput()) {
		// TODO: NetCDF
		std::ostream& out = OH.Parameters();

		out << std::setw(8) << GetLabel()
			<< " " << dGetX() << std::endl;
	}
}

void
ParameterNode::SetValue(DataManager *pDM,
	VectorHandler& /* X */ , VectorHandler& /* XP */ ,
	SimulationEntity::Hints *ph)
{
	NO_OP;
}

/* Aggiorna dati in base alla soluzione */
void
ParameterNode::Update(const VectorHandler& /* XCurr */ ,
	const VectorHandler& /* XPrimeCurr */ )
{
	NO_OP;
}

/* Inverse Dynamics */
void
ParameterNode::Update(const VectorHandler& /* XCurr */ ,
	const int iOrder)
{
	NO_OP;
}

void
ParameterNode::AfterPredict(VectorHandler& /* X */ ,
	VectorHandler& /* XP */ )
{
	NO_OP;
}

/* ParameterNode - end */


/* Node2Scalar - begin */

NodeDof::NodeDof(void)
: iDofNumber(-1), pNode(0)
{
	NO_OP;
}

NodeDof::NodeDof(int id, Node* p)
: iDofNumber(id), pNode(p)
{
	NO_OP;
}

NodeDof::~NodeDof(void)
{
	NO_OP;
}

Node2Scalar* Node2Scalar::pAllocateStatic(const NodeDof& nd)
{
     // FIXME: pointer must be deleted by the caller
     return new Node2Scalar(nd);
}

Node2Scalar::Node2Scalar(const NodeDof& nd)
: ScalarNode(nd.pNode->GetLabel(), nd.pNode->pGetDofOwner(), 0), ND(nd)
{
	NO_OP;
}

Node2Scalar::~Node2Scalar(void)
{
	NO_OP;
}

/* Tipo del nodo (usato solo per debug ecc.) */
Node::Type
Node2Scalar::GetNodeType(void) const
{
	return ND.pNode->GetNodeType();
}

/* Contributo del nodo al file di restart */
std::ostream&
Node2Scalar::Restart(std::ostream& out) const
{
	out << "# Node2Scalar: warning, not implemented yet " << std::endl;
	return out;
}

unsigned int
Node2Scalar::iGetNumDof(void) const
{
	return 1;
}

/* esegue operazioni sui dof di proprieta' dell'elemento
 * in particolare ritorna il tipo di Dof in base all'indice i.
 * Di default i Dof dei nodi sono assunti differenziali */
DofOrder::Order
Node2Scalar::GetDofType(unsigned int i) const
{
	ASSERT(i < iGetNumDof());
	return DofOrder::DIFFERENTIAL;
}

/* Ritorna gli indici di riga e colonna. Tipicamente sono gli stessi */
integer
Node2Scalar::iGetFirstRowIndex(void) const
{
	return ND.pNode->iGetFirstRowIndex() + ND.iDofNumber;
}

integer Node2Scalar::iGetFirstColIndex(void) const
{
	return ND.pNode->iGetFirstColIndex() + ND.iDofNumber;
}

/* Restituisce il valore del dof iDof;
 * se differenziale, iOrder puo' essere = 1 per la derivata */
const doublereal&
Node2Scalar::dGetDofValue(int iDof, int iOrder) const
{
	if (iDof != 1) {
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
	return ND.pNode->dGetDofValue(ND.iDofNumber + 1, iOrder);
}

/* Restituisce il valore del dof iDof al passo precedente;
 * se differenziale, iOrder puo' essere = 1 per la derivata */
const doublereal&
Node2Scalar::dGetDofValuePrev(int iDof, int iOrder) const
{
	if (iDof != 1) {
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
	}
	return ND.pNode->dGetDofValuePrev(ND.iDofNumber + 1, iOrder);
}

/* Setta il valore del dof iDof a dValue;
 * se differenziale, iOrder puo' essere = 1 per la derivata */
void
Node2Scalar::SetDofValue(const doublereal& dValue,
	unsigned int iDof,
	unsigned int iOrder)
{
	ASSERT(iDof == 1);
	if (iDof == 1) {
		dynamic_cast<Node*>(ND.pNode)->SetDofValue(dValue, ND.iDofNumber + 1, iOrder);
	}
	throw ErrGeneric(MBDYN_EXCEPT_ARGS);
}

/* Funzioni "spurie": consentono l'accesso ai dati privati;
 * sono state definite perche' i nodi astratti sono usati nei
 * modi piu' strani e quindi puo' essere necessario l'accesso */
void
Node2Scalar::SetX(const doublereal& d)
{
	SetDofValue(d, 1, 0);
}

/* only for differential nodes!?! */
void
Node2Scalar::SetXPrime(const doublereal& d)
{
	SetDofValue(d, 1, 1);
}

/* Node2Scalar - end */


/* ScalarDof - begin */

ScalarDof::ScalarDof(void)
: pNode(0), iOrder(0), iIndex(0)
{
	NO_OP;
}

ScalarDof::ScalarDof(const ScalarDof& sd)
: pNode(sd.pNode), iOrder(sd.iOrder), iIndex(sd.iIndex)
{
	NO_OP;
}

ScalarDof::ScalarDof(ScalarNode* p, int iO, int iI)
: pNode(p), iOrder(iO), iIndex(iI)
{
	NO_OP;
}

ScalarDof::~ScalarDof(void)
{
	NO_OP;
}

const doublereal &
ScalarDof::dGetValue(void) const
{
	return pNode->dGetDofValue(1, iOrder);
}

const doublereal &
ScalarDof::dGetValuePrev(void) const
{
	return pNode->dGetDofValuePrev(1, iOrder);
}

std::ostream&
ScalarDof::RestartScalarDofCaller(std::ostream& out) const
{
	Node::Type type = pNode->GetNodeType();
	switch (type) {
	case Node::PARAMETER :
		out << pNode->GetLabel() << ", "
			<< psReadNodesNodes[type];
		break;

	case Node::STRUCTURAL :
		out << pNode->GetLabel() << ", "
	   		<< psReadNodesNodes[type] << ", "
			<< iIndex << ", ";
			if (iOrder > 0) {
				out << "order, " << iOrder << std::endl;
			} else {
				out << "algebraic";
			}
		break;

	case Node::ABSTRACT:
	case Node::ELECTRIC:
	case Node::THERMAL:
	case Node::HYDRAULIC:
		out << "# RestartScalarDofCaller: warning, not implemented yet";
		break;

	default :
		NO_OP;
	}
	return out;
}

/* ScalarDof - end */
