#ifndef MODULE_GENETIC_ALGORITHM_H
#define MODULE_GENETIC_ALGORITHM_H

#include "userelem.h"
#include "drive.h"

class GeneticAlgorithmOptimization : public UserDefinedElem {
private:
    unsigned int inputsNumber;
    unsigned int outputNumber;
    std::vector<unsigned int> elementLabels;
    std::vector<std::string> elementTypes;
    std::vector<std::string> dataNames;
    std::vector<unsigned int> outputDrives;
    std::string fitnessFunction;
    std::string constraintsFunction;
    std::string selectionMethod;
    std::string hillClimbingMethod;/ Add more private members for GA operations

public:
    GeneticAlgorithmOptimization(unsigned uLabel, const DofOwner* pDO, 
                                DataManager* pDM, MBDynParser& HP);
    virtual ~GeneticAlgorithmOptimization(void);
    
    virtual void WorkSpaceDim(integer* piNumRows, integer* piNumCols) const;
    virtual VariableSubMatrixHandler& AssJac(VariableSubMatrixHandler& WorkMat,
                                           doublereal dCoef, 
                                           const VectorHandler& XCurr,
                                           const VectorHandler& XPrimeCurr);
    virtual SubVectorHandler& AssRes(SubVectorHandler& WorkVec,
                                   doublereal dCoef,
                                   const VectorHandler& XCurr, 
                                   const VectorHandler& XPrimeCurr);
    virtual unsigned int iGetNumDof(void) const;
    virtual void Output(OutputHandler& OH) const;
    virtual void SetValue(DataManager *pDM,
                         VectorHandler& X, VectorHandler& XP,
                         SimulationEntity::Hints *ph);
};

extern "C" bool genetic_algorithm_set(void);

#endif // MODULE_GENETIC_ALGORITHM_H
