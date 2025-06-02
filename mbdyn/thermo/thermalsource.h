#ifndef THERMALSOURCE_H
#define THERMALSOURCE_H

#include "therm.h"
#include "thermalnode.h"

class ThermalSource : public Thermal, public DriveOwner {
 private:
   const ThermalNode* pNode1;
   //doublereal thermalcapacitance;
 public:
   ThermalSource(unsigned int uL, 
   	const DofOwner* pDO,
   	const ThermalNode* p1, 
	const DriveCaller* pDC, 
	flag fOut);
   
   ~ThermalSource(void);
   
   /* Tipo di elemento termico (usato solo per debug ecc.) */
   virtual Thermal::Type GetThermalType(void) const;

   virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;
      
   VariableSubMatrixHandler& AssJac(VariableSubMatrixHandler& WorkMat,
				    doublereal dCoef,
				    const VectorHandler& XCurr, 
				    const VectorHandler& XPrimeCurr);
   
   SubVectorHandler& AssRes(SubVectorHandler& WorkVec,
			    doublereal dCoef,
			    const VectorHandler& XCurr, 
			    const VectorHandler& XPrimeCurr);
   
//    virtual void AfterConvergence(const VectorHandler& X, 
// 		   const VectorHandler& XP);
//    virtual void Output(OutputHandler& OH) const;
   
//    virtual void SetValue(DataManager *pDM,
// 		   VectorHandler& X, VectorHandler& XP,
// 		   SimulationEntity::Hints *ph = 0);

   /* *******PER IL SOLUTORE PARALLELO******** */        
   /* Fornisce il tipo e la label dei nodi che sono connessi all'elemento
      utile per l'assemblaggio della matrice di connessione fra i dofs */
   virtual void GetConnectedNodes(std::vector<const Node *>& connectedNodes) const;
   /* ************************************************ */

   /* returns the dimension of the component */
	const virtual OutputHandler::Dimensions GetEquationDimension(integer index) const;

   /* describes the dimension of components of equation */
   virtual std::ostream& DescribeEq(std::ostream& out,
		   const char *prefix = "",
		   bool bInitial = false) const;
};

#endif  /* THERMALSOURCE_H */
