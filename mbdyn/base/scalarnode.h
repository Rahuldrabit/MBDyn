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

/* nodi */

#ifndef SCALARNODE_H
#define SCALARNODE_H

#include "myassert.h"

#include "output.h"
#include "withlab.h"
#include "dofown.h"
#include "simentity.h"

#include "node.h"

/* ScalarNode - begin */

class ScalarNode : public Node {
protected:
	/* scrive l'output */
	using ToBeOutput::Output;
        using Node::DescribeEq;
	virtual std::ostream& Output(std::ostream& out) const;
        RestartData::RestartEntity GetRestartEntity() const;
public:
	/* Costruttori */

	/* Costruttore */
	ScalarNode(unsigned int uL, const DofOwner* pDO, flag fOut);

	/* Distruttore */
	virtual ~ScalarNode(void);

	/* Funzioni di servizio */

	virtual OutputHandler::OutFiles GetOutputType(void) const = 0;

	/* Initializes output (must be called by specialized classes) */
	virtual void OutputPrepare_int(OutputHandler& OH, bool bDifferential);

	/* Scrive l'output come abstract */
	virtual void Output(OutputHandler& OH) const;

	/*
	 * Ritorna il numero di DoFs ( == 1).
	 * Non usa il DofOwner in quanto viene usata per generale il DofOwner stesso
	 * (per compatibilita' con gli elementi che generano gradi di
	 * liberta' ed in previsione di nodi con un numero variabile di DoF
	 */
	virtual unsigned int iGetNumDof(void) const;

	/* Metodi che operano sui valori del DoF.
	 * Funzioni che consentono l'accesso diretto ai dati privati.
	 * sono state definite perche' i nodi scalari sono usati nei
	 * modi piu' strani e quindi puo' essere necessario l'accesso diretto.
	 */

	/* Setta il valore del DoF */
	virtual void SetX(const doublereal& d) = 0;

	/* Ottiene il valore del DoF */
	virtual const doublereal& dGetX(void) const = 0;

	/*
	 * Setta il valore della derivata.
	 * Definito solo per nodi differenziali
	 */
	virtual void SetXPrime(const doublereal& d) = 0;

	/*
	 * Ottiene il valore della derivata.
	 * Definito solo per nodi differenziali
	 */
	virtual const doublereal& dGetXPrime(void) const = 0;

	virtual void AfterPredict(VectorHandler& X, VectorHandler& XP);

	 /* returns the dimension of the component */
	const virtual OutputHandler::Dimensions GetEquationDimension(integer index) const;

	/* describes the dimension of components of equation */
        virtual std::ostream& DescribeEq(std::ostream& out,
                                         const char *prefix = "",
                                         bool bInitial = false) const;
};

/* ScalarNode - end */


/* ScalarDifferentialNode - begin */

class ScalarDifferentialNode : virtual public ScalarNode {
protected:
	/* Valore del DoF */
	mutable doublereal dX;
	/* Valore della derivata del DoF */
	mutable doublereal dXP;

	/* Valore del DoF al passo precedente */
	doublereal dXPrev;
	/* Valore della derivata del DoF al passo precedente */
	doublereal dXPPrev;

	/* scrive l'output */
	virtual std::ostream& Output(std::ostream& out) const;

#ifdef USE_NETCDF
	MBDynNcVar m_Var_dX;
	MBDynNcVar m_Var_dXP;
#endif // USE_NETCDF

	/* Initializes output (must be called by specialized classes) */
	using ScalarNode::OutputPrepare_int;
	virtual void OutputPrepare_int(OutputHandler& OH,
		const std::string& var_name,
		const OutputHandler::Dimensions var_dim,
		const std::string& var_desc,
		const std::string& varP_name,
		const OutputHandler::Dimensions varP_dim,
		const std::string& varP_desc);

public:
	/* Costruttori */

	/*
	 * Costruttore.
	 * @param uL label
	 * @param pDO DofOwner
	 * @param fOut flag di output
	 * @param dx valore iniziale
	 * @param dxp valore iniziale della derivata
	 */
	ScalarDifferentialNode(unsigned int uL, const DofOwner* pDO,
		const doublereal& dx, const doublereal& dxp, flag fOut);
	/* Distruttore */
	virtual ~ScalarDifferentialNode(void);


	/* Metodi di servizio */

	/* Tipo di nodo */
	virtual Node::Type GetNodeType(void) const;

	/*
	 * Esegue operazioni sui DoF di proprieta' dell'elemento.
	 * In particolare ritorna il tipo di DoF in base all'indice i.
	 */
	virtual DofOrder::Order GetDofType(unsigned int i) const;

	/* Metodi sui DoF */

	/*
	 * Restituisce il valore del DoF iDof.
	 * Se differenziale, iOrder puo' essere = 1 per ottenere la derivata
	 */
	virtual const doublereal& dGetDofValue(int iDof, int iOrder = 0) const;

	/*
	 * Restituisce il valore del DoF iDof al passo precedente.
	 * Se differenziale, iOrder puo' essere = 1 per ottenere la derivata
	 */
	virtual const doublereal& dGetDofValuePrev(int iDof, int iOrder = 0) const;

	/*
	 * Setta il valore del DoF iDof a dValue.
	 * Se differenziale, iOrder puo' essere = 1 per ottenere la derivata
	 */
	virtual void SetDofValue(const doublereal& dValue,
		unsigned int iDof,
		unsigned int iOrder = 0);

	/*
	 * Funzione che consente l'accesso diretto ai dati privati.
	 * Sono state definite perche' i nodi scalari sono usati nei
	 * modi piu' strani e quindi puo' essere necessario l'accesso diretto
	 */
	virtual void SetX(const doublereal& d);

	/* Ottiene il valore del DoF. Vedi SetX() */
	virtual inline const doublereal& dGetX(void) const;

	/* Setta la derivata del DoF. Vedi SetX() */
	virtual void SetXPrime(const doublereal& d);

	/* Ottiene la derivata del DoF. Vedi GetX() */
	virtual inline const doublereal& dGetXPrime(void) const;

	/* Consente di settare il valore iniziale nel vettore della soluzione */
	virtual void SetValue(DataManager *pDM,
		VectorHandler& X, VectorHandler& XP,
		SimulationEntity::Hints *ph = 0);

	/* Aggiorna i valori interni */
	virtual void Update(const class VectorHandler&, const class VectorHandler&);

	/* restart */
	std::ostream& Restart(std::ostream& out) const;

        virtual void Restart(RestartData& oData, RestartData::RestartAction eAction) override;

	virtual OutputHandler::OutFiles GetOutputType(void) const { return OutputHandler::ABSTRACT; };
	virtual void OutputPrepare(OutputHandler& OH);
	virtual void Output(OutputHandler& OH) const;

	/*
	 * Metodi per l'estrazione di dati "privati".
	 * Si suppone che l'estrattore li sappia interpretare.
	 * Come default non ci sono dati privati estraibili
	 */
	virtual unsigned int iGetNumPrivData(void) const;

	/*
	 * Maps a string (possibly with substrings) to a private data;
	 * returns a valid index ( > 0 && <= iGetNumPrivData()) or 0 
	 * in case of unrecognized data; error must be handled by caller
	 */
	virtual unsigned int iGetPrivDataIdx(const char *s) const;

	/*
	 * Returns the current value of a private data
	 * with 0 < i <= iGetNumPrivData()
	 */
	virtual doublereal dGetPrivData(unsigned int i) const;

	 /* returns the dimension of the component */
	const virtual OutputHandler::Dimensions GetEquationDimension(integer index) const;

	/* describes the dimension of components of equation */
        virtual std::ostream& DescribeEq(std::ostream& out,
                                         const char *prefix = "",
                                         bool bInitial = false) const;
};

inline const doublereal&
ScalarDifferentialNode::dGetX(void) const
{
	return dX;
}


inline const doublereal&
ScalarDifferentialNode::dGetXPrime(void) const
{
	return dXP;
}
/* ScalarDifferentialNode - end */

/* ScalarAlgebraicNode - begin */

class ScalarAlgebraicNode: virtual public ScalarNode {
protected:
	/* Valore del DoF */
	mutable doublereal dX;

	/* Valore del DoF al passo precedente */
	doublereal dXPrev;
        const DofOrder::Equality eEqualityType;
     
	/* scrive l'output */
	virtual std::ostream& Output(std::ostream& out) const override;

#ifdef USE_NETCDF
	MBDynNcVar m_Var_dX;
#endif // USE_NETCDF

	/* Initializes output (must be called by specialized classes) */
	using ScalarNode::OutputPrepare_int;
	virtual void OutputPrepare_int(OutputHandler& OH,
		const std::string& var_name,
		const OutputHandler::Dimensions var_dim,
		const std::string& var_desc);

public:
	/* Costruttori */

	/* Costruttore */
	ScalarAlgebraicNode(unsigned int uL, const DofOwner* pDO,
                            doublereal dx, DofOrder::Equality eEqualityType, flag fOut);

	/* Distruttore */
	virtual ~ScalarAlgebraicNode(void);

	/* Metodi di servizio */

	/* Tipo di nodo */
	virtual Node::Type GetNodeType(void) const override;

	/*
	 * Esegue operazioni sui DoF di proprieta' dell'elemento.
	 * In particolare ritorna il tipo di DoF in base all'indice i.
	 */
	virtual DofOrder::Order GetDofType(unsigned int i) const override;

        virtual DofOrder::Equality GetEqualityType(unsigned int i) const override;
	/* Metodi che operano sul valore del DoF */

	/*
	 * Restituisce il valore del DoF iDof.
	 * Se differenziale, iOrder puo' essere = 1 per ottenere la derivata
	 */
	virtual const doublereal& dGetDofValue(int iDof, int iOrder = 0) const override;

	/*
	 * Restituisce il valore del DoF iDof al passo precedente.
	 * Se differenziale, iOrder puo' essere = 1 per ottenere la derivata
	 */
	virtual const doublereal& dGetDofValuePrev(int iDof, int iOrder = 0) const override;

	/*
	 * Setta il valore del DoF iDof a dValue.
	 * Se differenziale, iOrder puo' essere = 1 per operare sulla derivata
	 */
	virtual void SetDofValue(const doublereal& dValue,
		unsigned int iDof,
		unsigned int iOrder = 0) override;

	/*
	 * Funzione che consente l'accesso diretto ai dati privati.
	 * Sono state definite perche' i nodi astratti sono usati nei
	 * modi piu' strani e quindi puo' essere necessario l'accesso diretto
	 */
	virtual void SetX(const doublereal& d) override;

	/* Ottiene il valore del DoF. Vedi SetX() */
	virtual inline const doublereal& dGetX(void) const override;

	/* Non definito per nodi algebrici */
	virtual void SetXPrime(const doublereal& d) override;

	/* Non definito per nodi algebrici */
	virtual inline const doublereal& dGetXPrime(void) const override;

	/* Consente di settare il valore iniziale nel vettore della soluzione */
	virtual void SetValue(DataManager *pDM,
		VectorHandler& X, VectorHandler& XP,
		SimulationEntity::Hints *ph = 0) override;

	/* Aggiorna i valori interni */
	virtual void Update(const class VectorHandler&, const class VectorHandler&) override;

	/* restart */
	std::ostream& Restart(std::ostream& out) const override;

        virtual void Restart(RestartData& oData, RestartData::RestartAction eAction) override;
     
	virtual OutputHandler::OutFiles GetOutputType(void) const override { return OutputHandler::ABSTRACT; };
	virtual void OutputPrepare(OutputHandler& OH) override;
	virtual void Output(OutputHandler& OH) const override;

	/*
	 * Maps a string (possibly with substrings) to a private data;
	 * returns a valid index ( > 0 && <= iGetNumPrivData()) or 0 
	 * in case of unrecognized data; error must be handled by caller
	 */
	virtual unsigned int iGetPrivDataIdx(const char *s) const override;
};

inline const doublereal&
ScalarAlgebraicNode::dGetX(void) const
{
	return dX;
}

inline const doublereal&
ScalarAlgebraicNode::dGetXPrime(void) const
{
	DEBUGCERR("Error, getting derivative from algebraic dof!" << std::endl);
	throw Node::ErrGeneric(MBDYN_EXCEPT_ARGS);
}

/* ScalarAlgebraicNode - end */

/* ParameterNode - begin */

/* Parametri.
 * I nodi di tipo parametro sono derivati dai nodi scalari algebrici,
 * ma non sono veri nodi. In realta' sono entita' che possiedono un valore,
 * ma non generano DoFs ed equazioni. Sono usati per consentire di dare in
 * modo trasparente un valore in ingresso, sotto forma di nodo, a tutti quegli
 * elementi elettrici e Genel che normalmente usano un DoF scalare senza farlo
 * partecipare allo jacobiano.
 */
class ParameterNode : public ScalarAlgebraicNode {
public:
	/* Costruttori */

	/* Costruttore */
	ParameterNode(unsigned int uL, const DofOwner* pDO,
		doublereal dx, flag fOut);
	/* Distruttore */
	virtual ~ParameterNode(void);

	/* Metodi di servizio */

	/* Tipo del nodo. Usato solo per debug ecc. */
	virtual Node::Type GetNodeType(void) const;

	/*
	 * Ritorna il numero di dofs.
	 * non usa il DofOwner in quanto viene usato per generale il DofOwner stesso.
	 * Ritorna 0 perche' il parametro non ha DoFs
	 */
	virtual unsigned int iGetNumDof(void) const;
	virtual DofOrder::Order GetDofType(unsigned int i) const;

	/* Metodi che agiscono sul valore */

	/* Metodi relativi al metodo di intergazione */

	virtual OutputHandler::OutFiles GetOutputType(void) const { return OutputHandler::PARAMETERS; };
	/* Output di default per nodi di cui non si desidera output */
	virtual void Output(OutputHandler& OH) const;

	/* Inizializzazione del valore */
	void SetValue(DataManager *pDM,
		VectorHandler& X, VectorHandler& XP,
		SimulationEntity::Hints *ph = 0);

	/* Aggiorna dati in base alla soluzione */
	virtual void Update(const VectorHandler& XCurr,
		const VectorHandler& XPrimeCurr);

	/* Inverse Dynamics */
	virtual void Update(const VectorHandler& XCurr, const int iOrder);

	/* Elaborazione dati dopo la predizione */
	virtual void AfterPredict(VectorHandler& X,
		VectorHandler& XP);

	/* the value! */
	virtual unsigned int iGetNumPrivData(void) const {
		return 1;
	};
};

/* ParameterNode - end */


/* Node2Scalar - begin */

/*
 * Struttura di conversione da nodo generico a nodo scalare.
 * Questa struttura consente di usare un grado di liberta'
 * di un nodo generico come se fosse un nodo scalare
 */
struct NodeDof {
	/* DoF del nodo */
	int iDofNumber;     /* Dof of the node */
	/* Puntatore al nodo */
	Node* pNode;        /* Pointer to the node */

	/* Costruttori */

	/* Costruttore di default */
	NodeDof(void);
	/* Costruttore */
	NodeDof(int id, Node* p);
	/* Distruttore */
	virtual ~NodeDof(void);
};

/*
 * Classe di conversione da nodo generico a nodo scalare.
 * @see NodeDof
 */
class Node2Scalar : public ScalarNode {
protected:
	/* Struttura che punta ad un DoF di un nodo */
	NodeDof ND;
     
     	/* Costruttore */
	explicit Node2Scalar(const NodeDof& nd);
public:
        Node2Scalar(const Node2Scalar&) = delete;

	/* Costruttori */
        static Node2Scalar* pAllocateStatic(const NodeDof& nd);
     
	/* Distruttore */
	virtual ~Node2Scalar(void);

	/* Metodi di servizio */
	const Node *GetNode(void) const { return ND.pNode; };

	/* Tipo del nodo. Uusato per debug ecc. */
	virtual Node::Type GetNodeType(void) const;

	/* Contributo del nodo al file di restart */
	virtual std::ostream& Restart(std::ostream& out) const;

	virtual OutputHandler::OutFiles GetOutputType(void) const { return OutputHandler::ABSTRACT; };

	/*
	 * Ritorna il numero di dofs.
	 * Non usa il DofOwner in quanto viene usata per generare il DofOwner stesso
	 */
	virtual unsigned int iGetNumDof(void) const;

	/* Metodi che operano sui valori del DoF */

	/*
	 * Esegue operazioni sui DoF di proprieta' dell'elemento.
	 * In particolare ritorna il tipo di DoF in base all'indice i.
	 */
	virtual DofOrder::Order GetDofType(unsigned int i) const;

	/*
	 * Ritorna gli indici di riga.
	 * Tipicamente sono gli stessi di quelli di colonna
	 */
	virtual integer iGetFirstRowIndex(void) const;

	/*
	 * Ritorna gli indici di colonna.
	 * Tipicamente sono gli stessi di quelli di riga.
	 * @see iGetFirstRowIndex()
	 */
	virtual integer iGetFirstColIndex(void) const;

	/*
	 * Restituisce il valore del DoF iDof.
	 * Se differenziale, iOrder puo' essere = 1 per ottenere la derivata
	 */
	virtual const doublereal& dGetDofValue(int iDof, int iOrder = 0) const;

	/*
	 * Restituisce il valore del DoF iDof al passo precedente.
	 * Se differenziale, iOrder puo' essere = 1 per ottenere la derivata
	 */
	virtual const doublereal& dGetDofValuePrev(int iDof, int iOrder = 0) const;

	/*
	 * Setta il valore del DoF iDof a dValue.
	 * Se differenziale, iOrder puo' essere = 1 per operare sulla derivata
	 */
	virtual void SetDofValue(const doublereal& dValue,
		unsigned int iDof,
		unsigned int iOrder = 0);

	/*
	 * Funzione che consente l'accesso diretto ai dati privati.
	 * Sono state definite perche' i nodi astratti sono usati nei
	 * modi piu' strani e quindi puo' essere necessario l'accesso diretto
	 */
	virtual void SetX(const doublereal& d);

	/* Ottiene il valore del DoF */
	virtual inline const doublereal& dGetX(void) const;

	/* Setta il valore della derivata del DoF */
	virtual void SetXPrime(const doublereal& d);

	/* Setta il valore della derivata del DoF */
	virtual inline const doublereal& dGetXPrime(void) const;
};

inline const doublereal&
Node2Scalar::dGetX(void) const
{
	return dGetDofValue(1, 0);
}

inline const doublereal&
Node2Scalar::dGetXPrime(void) const
{
	return dGetDofValue(1, 1);
}

/* Node2Scalar - end */


/* ScalarDof - begin */

/*
 * Struttura che trasforma un nodo scalare in un grado di liberta' scalare.
 * In pratica consente di accedere ad un DoF scalare o alla derivata di un
 * nodo scalare in modo trasparente
 */
struct ScalarDof {
	/* Puntatore al nodo scalare */
	ScalarNode* pNode;
	/* Ordine del grado di liberta' */
	int iOrder;
	int iIndex;
	/* Costruttori */

	/* Costruttore di default */
	ScalarDof(void);
	ScalarDof(const ScalarDof& sd);
	/* Costruttore */
	ScalarDof(ScalarNode* p, int iO, int iI);
	/* Distruttore */
	~ScalarDof(void);

	/* Funzioni che operano sui valori del DoF */

	/* Ottiene il valore del DoF */
	const doublereal & dGetValue(void) const;

	/* Ottiene il valore del DoF al passo precedente */
	const doublereal & dGetValuePrev(void) const;

	/* Scrive nel file di Restart il contributo per la chiamata allo ScalarDof */
	std::ostream& RestartScalarDofCaller(std::ostream& out) const ;
};

/* ScalarDof - end */

#endif /* SCALARNODE_H */

