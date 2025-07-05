#ifndef MODULE_GENETIC_ALGORITHM_H
#define MODULE_GENETIC_ALGORITHM_H

#include "userelem.h"
#include "drive.h"
#include <vector>
#include <string>
#include <map>

class GeneticAlgorithmOptimization : public UserDefinedElem {
private:
    unsigned int inputsNumber;
    struct ElementInput {
        unsigned int label;
        std::string jointType;
        std::string privateDataType;
        std::string privateDataValue;
        std::string accessType;
    };
    std::vector<ElementInput> elements;
    std::string fitnessFunctionType;
    std::string constraintsFunctionLib;
    // ...other GA parameters...

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
