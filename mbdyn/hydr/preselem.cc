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
 * Copyright 1999-2000 Lamberto Puggelli <puggelli@tiscalinet.it>
 * Dipartimento di Ingegneria Aerospaziale - Politecnico di Milano
 */

#include "mbconfig.h"           /* This goes first in every *.c,*.cc file */

#include <cfloat>
#include <limits>

#include "dataman.h"
#include "preselem.h"

#include "actuator.h"
#include "hfluid.h"
#include "hminor.h"
#include "hutils.h"
#include "pipe.h"
#include "valve.h"

/* HydraulicElem - begin */

HydraulicElem::HydraulicElem(unsigned int uL, const DofOwner* pDO,
			     HydraulicFluid* hf, flag fOut)
: Elem(uL, fOut), 
DofOwnerOwner(pDO), 
HF(hf)
{
   ASSERT(HF != NULL);
} 


HydraulicElem::~HydraulicElem(void) 
{
   if (HF != NULL) {
      SAFEDELETE(HF);
   }
}


/* Tipo dell'elemento (usato per debug ecc.) */
Elem::Type HydraulicElem::GetElemType(void) const
{
   return Elem::HYDRAULIC;
}


/* Contributo al file di restart 
 * (Nota: e' incompleta, deve essere chiamata dalla funzione corrispndente
 * relativa alla classe derivata */
std::ostream& HydraulicElem::Restart(std::ostream& out) const 
{
   return out << "  hydraulic: " << GetLabel();
}

/* HydraulicElem - end */





Elem* ReadHydraulicElem(DataManager* pDM,
			MBDynParser& HP, 
			const DofOwner* pDO, 
			unsigned int uLabel)
{
   DEBUGCOUT("ReadHydraulicElem()");
   
   const char* sKeyWords[] = {
      "minor" "loss",
      "three" "way" "minor" "loss",
      "control" "valve",
      "control" "valve" "2",
      "dynamic" "control" "valve",
      "pressure" "flow" "control",
      "pressure" "valve",
      "flow" "valve",
      "orifice",
      "accumulator",
      "tank",
      "pipe",
      "dynamic" "pipe",
      "actuator",
      NULL
   };
   
   /* enum delle parole chiave */
   enum KeyWords {
      UNKNOWN = -1,
      MINOR_LOSS = 0, 
      THREEWAYMINORLOSS,
      CONTROL_VALVE,
      CONTROL_VALVE2,
      DYNAMIC_CONTROL_VALVE,
      PRESSURE_FLOW_CONTROL_VALVE,
      PRESSURE_VALVE,
      FLOW_VALVE,
      ORIFICE,
      ACCUMULATOR,
      TANK,
      PIPE,
      DYNAMIC_PIPE,
      ACTUATOR,
      
      LASTKEYWORD
   };
   
   /* tabella delle parole chiave */
   KeyTable K(HP, sKeyWords);
   
   /* lettura del tipo di elemento elettrico */   
   KeyWords CurrKeyWord = KeyWords(HP.GetWord());
   
#ifdef DEBUG   
   if (CurrKeyWord >= 0) {      
      std::cout << "hydraulic element type: "
	<< sKeyWords[CurrKeyWord] << std::endl;
   }   
#endif   
   
   Elem* pEl = NULL;
   
   switch (CurrKeyWord) {
      
    case ACTUATOR: {
       /* lettura dei dati specifici */
       /* due nodi idraulici e due nodi strutturali */
       
       /* nodo idraulico 1 */
       const PressureNode* pNodeHyd1 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo idraulico 2 */
       const PressureNode* pNodeHyd2 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo strutturale 1 */
       const StructNode* pNodeStr1 = pDM->ReadNode<const StructNode, Node::STRUCTURAL>(HP);
       
       Vec3 f1(HP.GetPosRel(ReferenceFrame(pNodeStr1)));
       DEBUGCOUT("Offset 1: " << f1 << std::endl);
       
       /* nodo strutturale 2 */
       const StructNode* pNodeStr2 = pDM->ReadNode<const StructNode, Node::STRUCTURAL>(HP);
       
       Vec3 f2(HP.GetPosRel(ReferenceFrame(pNodeStr2)));
       DEBUGCOUT("Offset 2: " << f2 << std::endl);  
       
       ReferenceFrame RF(pNodeStr1);
       Vec3 axis(0., 0., 1.); 
       if (HP.IsKeyWord("direction")) {
          try {
	     axis = HP.GetUnitVecRel(RF);
          } catch (ErrNullNorm& err) {
	     silent_cerr("Actuator(" << uLabel << "): "
		     "need a definite direction, not {"
		     << axis << "}!" << std::endl);
	     throw ErrNullNorm(MBDYN_EXCEPT_ARGS);
	  }
       } 
       
       /* Area nodo1 */
       doublereal area1;
       try {
               area1 = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid first chamber area " << e.Get() << " (must be positive) [" << e.what() << "] for Actuator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Area1: " << area1 << std::endl);
       
       /* Area nodo2 */
       doublereal area2;
       try {
               area2 = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid second chamber area " << e.Get() << " (must be positive) [" << e.what() << "] for Actuator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Area2: " << area2 << std::endl);
       
       /* lunghezza cilindro (a meno dello spessore */
       doublereal dl;
       try {
               dl = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid cylinder stroke " << e.Get() << " (must be positive) [" << e.what() << "] for Actuator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("dl: " << dl << std::endl);
       
       HydraulicFluid* hf1 = HP.GetHydraulicFluid();
       ASSERT(hf1 != NULL);
       
       HydraulicFluid* hf2 = NULL; 
       if (HP.IsKeyWord("same")) {
	  hf2 = hf1->pCopy();
       } else {
	  hf2 = HP.GetHydraulicFluid();
       }
       ASSERT(hf2 != NULL);

       flag fOut = pDM->fReadOutput(HP, Elem::HYDRAULIC);
       
       SAFENEWWITHCONSTRUCTOR(pEl, 
			      Actuator,
			      Actuator(uLabel, pDO, 
				       pNodeHyd1, pNodeHyd2, 
				       pNodeStr1, pNodeStr2,
				       f1, f2, axis, hf1, hf2, 
				       area1, area2, dl,
				       fOut));
       
       break;
    }	
      
    case MINOR_LOSS: {
       
       /* nodo 1 */
       const PressureNode* pNode1 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 2 */
       const PressureNode* pNode2 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* Kappa1 diretto */
       doublereal dKappa1 = HP.GetReal();
       if (dKappa1 < 0.) {		  
	  silent_cerr("MinorLoss(" << uLabel << "): "
		  "negative Kappa1 at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Kappa1: " << dKappa1 << std::endl);
       
       /* Kappa2 inverso */
       doublereal dKappa2 = HP.GetReal();
       if (dKappa2 < 0.) {		  
	  silent_cerr("MinorLoss(" << uLabel << "): "
		  "negative Kappa2 at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Kappa2: " << dKappa2 << std::endl);
       
       /* Area */
       doublereal area = HP.GetReal();
       if (area <= std::numeric_limits<doublereal>::epsilon()) {		  
	  silent_cerr("MinorLoss(" << uLabel << "): "
		  "null or negative area "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	 
       DEBUGCOUT("Area: " << area << std::endl);
       
       HydraulicFluid* hf = HP.GetHydraulicFluid();
       ASSERT(hf != NULL);

       flag fOut = pDM->fReadOutput(HP, Elem::HYDRAULIC);
       
       SAFENEWWITHCONSTRUCTOR(pEl, 
			      MinorLoss,
                              MinorLoss(uLabel, pDO, hf, pNode1, pNode2, 
					 dKappa1, dKappa2, area, fOut));
       
       break;
    }

    case THREEWAYMINORLOSS: {
       
       /* nodo 0 */
       const PressureNode* pNode0 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 1 */
       const PressureNode* pNode1 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 2 */
       const PressureNode* pNode2 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* Kappa1 diretto */
       doublereal dKappa1 = HP.GetReal();
       if (dKappa1 < 0.) {		  
	  silent_cerr("ThreeWayMinorLoss(" << uLabel << "): "
		  "negative Kappa1 at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Kappa1: " << dKappa1 << std::endl);
       
       /* Kappa2 inverso */
       doublereal dKappa2 = HP.GetReal();
       if (dKappa2 < 0.) {		  
	  silent_cerr("ThreeWayMinorLoss(" << uLabel << "): "
		  "negative Kappa2 at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Kappa2: " << dKappa2 << std::endl);
       
       /* Area 1 */
       doublereal area1 = HP.GetReal();
       if (area1 <= std::numeric_limits<doublereal>::epsilon()) {		  
	  silent_cerr("ThreeWayMinorLoss(" << uLabel << "): "
		  "null or negative area1 "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	 
       DEBUGCOUT("Area: " << area1 << std::endl);
       
       /* Area 2 */
       doublereal area2 = HP.GetReal();
       if (area1 <= std::numeric_limits<doublereal>::epsilon()) {		  
	  silent_cerr("ThreeWayMinorLoss(" << uLabel << "): "
		  "null or negative area2 "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	 
       DEBUGCOUT("Area: " << area2 << std::endl);
       
       HydraulicFluid* hf = HP.GetHydraulicFluid();
       ASSERT(hf != NULL);

       flag fOut = pDM->fReadOutput(HP, Elem::HYDRAULIC);
       
       SAFENEWWITHCONSTRUCTOR(pEl, 
			      ThreeWayMinorLoss,
                              ThreeWayMinorLoss(uLabel, pDO, hf, 
				      pNode0, pNode1, pNode2, 
				      dKappa1, dKappa2, area1, area2, fOut));
       
       break;
    }

    case CONTROL_VALVE:
    case CONTROL_VALVE2: {
       
       /* nodo 1 */
       const PressureNode* pNode1 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 2 */
       const PressureNode* pNode2 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 3 */
       const PressureNode* pNode3 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 4 */
       const PressureNode* pNode4 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* Area massima della valvola */
       doublereal area_max = HP.GetReal();
       if (area_max <= 0.) {		  
	  silent_cerr("ControlValve(" << uLabel << "): "
		  "null or negative area_max "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Area_max: " << area_max << std::endl);
       
       /* Area di trafilamento in % sull'area massima:valore di default = 1.e-6 */
       doublereal loss_area = 0.; /* 1.e-6; */
       if (HP.IsKeyWord("loss")) {
	  loss_area = HP.GetReal();
	  if (loss_area  < 0.) {		  
	     silent_cerr("ControlValve(" << uLabel << "): "
		     "negative loss_area "
		     "at line " << HP.GetLineData() << std::endl);
	     throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
	  }	  
	  DEBUGCOUT("Loss_area in %= " << loss_area << std::endl); 
       }
       
       /* Stato */
       DriveCaller* pDC = HP.GetDriveCaller();
       
       HydraulicFluid* hf = HP.GetHydraulicFluid();
       ASSERT(hf != NULL);
       
       flag fOut = pDM->fReadOutput(HP, Elem::HYDRAULIC);
      
       switch (CurrKeyWord) {
       case CONTROL_VALVE:
       SAFENEWWITHCONSTRUCTOR(pEl, 
			      Control_valve,
			      Control_valve(uLabel, pDO, hf, 
				      pNode1, pNode2, pNode3, pNode4, 
				      area_max, loss_area, pDC, fOut));
       break;

       case CONTROL_VALVE2:
       SAFENEWWITHCONSTRUCTOR(pEl, 
			      Control_valve2,
			      Control_valve2(uLabel, pDO, hf, 
				      pNode1, pNode2, pNode3, pNode4, 
				      area_max, loss_area, pDC, fOut));
       break;

       default:
          throw ErrGeneric(MBDYN_EXCEPT_ARGS);
       }
       
       break;
    }
      
    case DYNAMIC_CONTROL_VALVE: {
       
       /* nodo 1 */
       const PressureNode* pNode1 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 2 */
       const PressureNode* pNode2 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 3 */
       const PressureNode* pNode3 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 4 */
       const PressureNode* pNode4 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* Forza */
       DriveCaller* pDC = HP.GetDriveCaller();
       
       /* spostamento iniziale */
       doublereal start = HP.GetReal();
       DEBUGCOUT("Start: " << start << std::endl);
       
       /* Spostamento massimo della valvola */
       doublereal s_max = HP.GetReal();
       if (s_max < 0.) {		  
	  silent_cerr("DynamicControlValve(" << uLabel << "): "
		  "negative s_max at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("S_max: " << s_max << std::endl);
       
       /* Larghezza del condotto */
       doublereal width = HP.GetReal();
       if (width <= 0.) {		  
	  silent_cerr("DynamicControlValve(" << uLabel << "): "
		  "null or negative width "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Width: " << width << std::endl);
       
       /* Area di trafilamento in % sull'area massima(==width*s_max):valore di default = 1.e-6 */
       doublereal loss_area = 0.; /* 1.e-6; */
       if (HP.IsKeyWord("loss")) {
	  loss_area = HP.GetReal();
	  if (loss_area < 0.) {		  
	     silent_cerr("DynamicControlValve(" << uLabel << "): "
		     "negative loss_area "
		     "at line " << HP.GetLineData() << std::endl);
	     throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
	  }
	  DEBUGCOUT("Loss_area in %= " << loss_area << std::endl); 
       }
       
       /* Diametro della valvola */
       doublereal valve_diameter = HP.GetReal();
       if (valve_diameter <= 0.) {		  
	  silent_cerr("DynamicControlValve(" << uLabel << "): "
		  "null or negative valve diameter "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Valve diameter: " << valve_diameter << std::endl);
       
       /* Densita' del corpo della valvola */
       doublereal valve_density = HP.GetReal();
       if (valve_density <= 0.) {		  
	  silent_cerr("DynamicControlValve(" << uLabel << "): "
		  "null or negative valve density "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Valve density: " << valve_density << std::endl);
       
       /* c dello spostamento */
       doublereal c_spost = HP.GetReal();
       DEBUGCOUT("c_spost: " << c_spost << std::endl);
       
       /* c della velocita' */
       doublereal c_vel = HP.GetReal();
       DEBUGCOUT("c_vel: " << c_vel << std::endl);
       
       /* c della accelerazione */
       doublereal c_acc = HP.GetReal();
       DEBUGCOUT("c_acc: " << c_acc << std::endl);
       	
       HydraulicFluid* hf = HP.GetHydraulicFluid();
       ASSERT(hf != NULL);
       
       flag fOut = pDM->fReadOutput(HP, Elem::HYDRAULIC);
       
       SAFENEWWITHCONSTRUCTOR(pEl, 
			      Dynamic_control_valve,
			      Dynamic_control_valve(uLabel, pDO, hf, 
						    pNode1, pNode2, 
						    pNode3, pNode4, 
						    pDC, start,
						    s_max, width, 
						    loss_area, 
						    valve_diameter, 
						    valve_density,
						    c_spost, c_vel, c_acc,
						    fOut));
       break;
    }

    case PRESSURE_FLOW_CONTROL_VALVE: {
       
       /* nodo 1 */
       const PressureNode* pNode1 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 2 */
       const PressureNode* pNode2 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 3 */
       const PressureNode* pNode3 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 4 */
       const PressureNode* pNode4 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
         /* nodo 5 */
       const PressureNode* pNode5 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
         /* nodo 6 */
       const PressureNode* pNode6 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
              
       /* Forza */
       DriveCaller* pDC = HP.GetDriveCaller();
       
       /* spostamento iniziale */
       doublereal start = HP.GetReal();
       if (start < 0.) {		  
	  silent_cerr("PressureFlowControlValve(" << uLabel << "): "
		  "negative start "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       } 
       DEBUGCOUT("Start: " << start << std::endl);
       
       /* Spostamento massimo della valvola */
       doublereal s_max = HP.GetReal();
       if (s_max < 0.) {		  
	  silent_cerr("PressureFlowControlValve(" << uLabel << "): "
		  "negative s_max "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("S_max: " << s_max << std::endl);
       
       /* Larghezza del condotto */
       doublereal width = HP.GetReal();
       if (width <= 0.) {		  
	  silent_cerr("PressureFlowControlValve(" << uLabel << "): "
		  "null or negative width "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Width: " << width << std::endl);
       
       /* Area di trafilamento in % sull'area massima(==width*s_max):valore di default = 1.e-6 */
       doublereal loss_area = 0.; /* 1.e-6; */
       if (HP.IsKeyWord("loss")) {
	  loss_area = HP.GetReal();
	  if (loss_area < 0.) {		  
	     silent_cerr("PressureFlowControlValve(" << uLabel << "): "
		     "negative loss_area "
		     "at line " << HP.GetLineData() << std::endl);
	     throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
	  }
	  DEBUGCOUT("Loss_area in %= " << loss_area << std::endl); 
       }
       
       /* Diametro della valvola */
       doublereal valve_diameter = HP.GetReal();
       if (valve_diameter <= 0.) {		  
	  silent_cerr("PressureFlowControlValve(" << uLabel << "): "
		  "null or negative valve diameter "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Valve diameter: " << valve_diameter << std::endl);
       
       /* Densita' del corpo della valvola */
       doublereal valve_density = HP.GetReal();
       if (valve_density <= 0.) {		  
	  silent_cerr("PressureFlowControlValve(" << uLabel << "): "
		  "null or negative valve density "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Valve density: " << valve_density << std::endl);
       
       /* c dello spostamento */
       doublereal c_spost = HP.GetReal();
       DEBUGCOUT("c_spost: " << c_spost << std::endl);
       
       /* c della velocita' */
       doublereal c_vel = HP.GetReal();
       DEBUGCOUT("c_vel: " << c_vel << std::endl);
       
       /* c della accelerazione */
       doublereal c_acc = HP.GetReal();
       DEBUGCOUT("c_acc: " << c_acc << std::endl);
       	
       HydraulicFluid* hf = HP.GetHydraulicFluid();
       ASSERT(hf != NULL);
       
       flag fOut = pDM->fReadOutput(HP, Elem::HYDRAULIC);
       
       SAFENEWWITHCONSTRUCTOR(pEl, 
			      Pressure_flow_control_valve,
			      Pressure_flow_control_valve(uLabel, pDO, hf, 
						    pNode1, pNode2, 
						    pNode3, pNode4, 
						    pNode5, pNode6, 
						    pDC, start,
						    s_max, width, 
						    loss_area, 
						    valve_diameter, 
						    valve_density,
						    c_spost, c_vel, c_acc,
						    fOut));
       break;
    }
      
      
    case PRESSURE_VALVE: {
       
       /* nodo 1 */
       const PressureNode* pNode1 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 2 */
       const PressureNode* pNode2 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* Area diaframma */
       doublereal area_diaf = HP.GetReal();
       if (area_diaf <= 0.) {		  
	  silent_cerr("PressureValve(" << uLabel << "): "
		  "null or negative area_diaf "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Area_diaf: " << area_diaf << std::endl);
       
       /* Massa valvola */
       doublereal mass = HP.GetReal();
       if (mass <= 0.) {		  
	  silent_cerr("PressureValve(" << uLabel << "): "
		  "null or negative valve mass "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Valve mass: " << mass << std::endl);
       
       /* Area massima della valvola */
       doublereal area_max = HP.GetReal();
       if (area_max <= 0.) {		  
	  silent_cerr("PressureValve(" << uLabel << "): "
		  "null or negative area_max "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Area_max: " << area_max << std::endl);
       
       /* Spostamento massimo della valvola */
       doublereal s_max = HP.GetReal();
       if (s_max < 0.) {		  
	  silent_cerr("PressureValve(" << uLabel << "): "
		  "negative s_max "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("S_max: " << s_max << std::endl);
       
       /* Kappa : costante della molla */
       doublereal Kappa = HP.GetReal();
       if (Kappa < 0.) {		  
	  silent_cerr("PressureValve(" << uLabel << "): "
		  "negative Kappa "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Kappa: " << Kappa << std::endl);
       
       /* Forza0: precarico della molla */
       doublereal force0 = HP.GetReal();
       if (force0 < 0.) {		  
	  silent_cerr("PressureValve(" << uLabel << "): "
		  "negative force0 "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	 
       DEBUGCOUT("Force0: " << force0 << std::endl);
       
       /* Larghezza luce di passaggio */
       doublereal width = HP.GetReal();
       if (width <= 0.) {		  
	  silent_cerr("PressureValve(" << uLabel << "): "
		  "null or negative width "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Width: " << width << std::endl);
       
       /* c dello spostamento */
       doublereal c_spost = HP.GetReal();
       DEBUGCOUT("c_spost: " << c_spost << std::endl);
       
       /* c della velocita' */
       doublereal c_vel = HP.GetReal();
       DEBUGCOUT("c_vel: " << c_vel << std::endl);
       
       /* c della accelerazione */
       doublereal c_acc = HP.GetReal();
       DEBUGCOUT("c_acc: " << c_acc << std::endl);
       
       HydraulicFluid* hf = HP.GetHydraulicFluid();
       ASSERT(hf != NULL);
	       
       flag fOut = pDM->fReadOutput(HP, Elem::HYDRAULIC);
       
       SAFENEWWITHCONSTRUCTOR(pEl, 
			      Pressure_valve,
                              Pressure_valve(uLabel, pDO, hf, pNode1, pNode2, 
					     area_diaf, mass, area_max, 
					     s_max, Kappa, force0, width,
					     c_spost, c_vel, c_acc,
					     fOut));
       
       break;
    }
      
    case FLOW_VALVE: {
       
       /* nodo 1 */
       const PressureNode* pNode1 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 2 */
       const PressureNode* pNode2 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 3 */
       const PressureNode* pNode3 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* Area diaframma */
       doublereal area_diaf;
       try {
               area_diaf = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: diaphragm area " << e.Get() << " (must be positive) [" << e.what() << "] for FlowValve(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Area_diaf: " << area_diaf << std::endl);
       
       /* Massa valvola */
       doublereal mass;
       try {
               mass = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: valve mass " << e.Get() << " (must be positive) [" << e.what() << "] for FlowValve(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Valve mass: " << mass << std::endl);
       
       /* Area tubo */
       doublereal area_pipe;
       try {
               area_pipe = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: pipe area " << e.Get() << " (must be positive) [" << e.what() << "] for FlowValve(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Area_pipe: " << area_pipe << std::endl);
            
       /* Area massima della valvola */
       doublereal area_max;
       try {
               area_max = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: valve max area " << e.Get() << " (must be positive) [" << e.what() << "] for FlowValve(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Area_max: " << area_max << std::endl);
       
       /* Kappa : costante della molla */
       doublereal Kappa;
       try {
               Kappa = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: spring stiffness " << e.Get() << " (must be positive) [" << e.what() << "] for FlowValve(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Kappa: " << Kappa << std::endl);
       
       /* Forza0: precarico della molla */
       doublereal force0;
       try {
               force0 = HP.GetReal(0., HighParser::range_ge<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: spring preload " << e.Get() << " (must be non-negative) [" << e.what() << "] for FlowValve(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Force0: " << force0 << std::endl);
       
       /* Larghezza luce di passaggio */
       doublereal width;
       try {
               width = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: width " << e.Get() << " (must be positive) [" << e.what() << "] for FlowValve(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Width: " << width << std::endl);
       
       /* Corsa massima della valvola */
       doublereal s_max;
       try {
               s_max = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: max stroke " << e.Get() << " (must be positive) [" << e.what() << "] for FlowValve(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("s_max: " << s_max << std::endl);
       
       /* c dello spostamento */
       doublereal c_spost;
       try {
               c_spost = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: displacement penalty " << e.Get() << " (must be positive) [" << e.what() << "] for FlowValve(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("c_spost: " << c_spost << std::endl);
       
       /* c della velocita' */
       doublereal c_vel;
       try {
               c_vel = HP.GetReal(0., HighParser::range_ge<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: velocity penalty " << e.Get() << " (must be non-negative) [" << e.what() << "] for FlowValve(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("c_vel: " << c_vel << std::endl);
       
       /* c della accelerazione */
       doublereal c_acc;
       try {
               c_acc = HP.GetReal(0., HighParser::range_ge<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: acceleration penalty " << e.Get() << " (must be non-negative) [" << e.what() << "] for FlowValve(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("c_acc: " << c_acc << std::endl);
       
       HydraulicFluid* hf = HP.GetHydraulicFluid();
       ASSERT(hf != NULL);
       
       flag fOut = pDM->fReadOutput(HP, Elem::HYDRAULIC);
       
       SAFENEWWITHCONSTRUCTOR(pEl, 
			      Flow_valve,
                              Flow_valve(uLabel, pDO, hf, 
					 pNode1, pNode2, pNode3,
					 area_diaf, mass,area_pipe, area_max,
					 Kappa, force0, width, s_max,
					 c_spost, c_vel, c_acc,
					 fOut));
       
       break;
    }
      
    case ORIFICE: {
       
       /* nodo 1 */
       const PressureNode* pNode1 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 2 */
       const PressureNode* pNode2 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* Diametro */
       doublereal diameter = HP.GetReal();
       if (diameter <= 0.) {		  
	  silent_cerr("Orifice(" << uLabel << "): "
		  "null or negative diameter "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Diameter: " << diameter << std::endl);
       
       /* Area diaframma */
       doublereal area_diaf = HP.GetReal();
       if (area_diaf <= 0.) {		  
	  silent_cerr("Orifice(" << uLabel << "): "
		  "null or negative area_diaf "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	 
       DEBUGCOUT("Area_diaf: " << area_diaf << std::endl);
  
       /* Area del tubo */
       doublereal area_pipe = diameter*diameter*M_PI/4.;
       if (HP.IsKeyWord("area")) 
	 {
	    area_pipe = HP.GetReal();
	    if (area_pipe <= 0.) 
	      {		  
		 silent_cerr("Orifice(" << uLabel << "): "
			 "null or negative area_pipe "
			 "at line " << HP.GetLineData() << std::endl);
		 throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
	      }	 
	 }
       DEBUGCOUT("Area_pipe: " << area_pipe << std::endl);
       
       doublereal ReCr = 10;
       if (HP.IsKeyWord("reynolds")) 
	 {
	    ReCr = HP.GetReal();
	    if (ReCr <= 0.) 
	      {		  
		 silent_cerr("Orifice(" << uLabel << "): "
			 "null or negative Reynold's number "
			 "at line " << HP.GetLineData() << std::endl);
		 throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
	      }	 
	 }
       DEBUGCOUT("Reynold critico: " << ReCr << std::endl);
       
       HydraulicFluid* hf = HP.GetHydraulicFluid();
       ASSERT(hf != NULL);
       
       flag fOut = pDM->fReadOutput(HP, Elem::HYDRAULIC);
       
       SAFENEWWITHCONSTRUCTOR(pEl, 
			      Orifice,
			      Orifice(uLabel, pDO, hf, 
				      pNode1, pNode2, 
				      diameter, 
				      area_diaf, area_pipe, ReCr, fOut));
       break;
    }
      
    case ACCUMULATOR: {
       
       /* nodo */
       const PressureNode* pNode = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* Corsa pistone */
       doublereal stroke(0.);
       try {
               stroke = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid stroke " << e.Get() << " (must be positive) [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Stroke: " << stroke << std::endl);
          
       doublereal start(0.);
       if (HP.IsKeyWord("start")) {	       
	   // Corsa iniziale del setto
           try {
                   start = HP.GetReal(0., HighParser::range_ge_le<doublereal>(0., stroke));

           } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
                   silent_cerr("error: invalid start " << e.Get() << " (must be non-negative and lower than or equal to stroke=" << stroke << ") [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
                   throw e;
           }
       }	    	  
       DEBUGCOUT("start: " << start << std::endl);
       
       /* Area stantuffo */
       doublereal area(0.);
       try {
               area = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid area " << e.Get() << " (must be positive) [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Area: " << area << std::endl);
       
       /* Area pipe */
       doublereal area_pipe(0.);
       try {
               area_pipe = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid pipe area " << e.Get() << " (must be positive) [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("area_pipe: " << area_pipe << std::endl); 
       
       /* Massa stantuffo */
       doublereal mass;
       try {
               mass = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid mass " << e.Get() << " (must be positive) [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Mass: " << mass << std::endl);
         
       doublereal h_in(1.);
       if (HP.IsKeyWord("loss" "in")) {	       
	   // Perdita di carico entrata
           try {
                   h_in = HP.GetReal(0., HighParser::range_ge<doublereal>(0.));

           } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
                   silent_cerr("error: invalid loss in " << e.Get() << " (must be non-negative) [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
                   throw e;
           }
       }	    	  
       DEBUGCOUT("Loss_in: " << h_in << std::endl);
       
       doublereal h_out(0.5);
       if (HP.IsKeyWord("loss" "out")) {	       
           // Perdita di carico uscita    	   
           try {
                   h_out = HP.GetReal(0., HighParser::range_ge<doublereal>(0.));

           } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
                   silent_cerr("error: invalid loss out " << e.Get() << " (must be non-negative) [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
                   throw e;
           }
       }
       DEBUGCOUT("loss_out: " << h_out << std::endl);
       
       doublereal press0(0.);
       doublereal press_max(0.);
       doublereal Kappa(0.);
       
       if (HP.IsKeyWord("gas")) {
	  /* Pressione gas accumulatore scarico */
          try {
               press0 = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

          } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid gas reference pressure " << e.Get() << " (must be positive) [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
          }
	  DEBUGCOUT("press0: " << press0 << std::endl);
	  
	  /* Pressione massima del gas */
          try {
               press_max = HP.GetReal(0., HighParser::range_ge<doublereal>(press0));

          } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid gas max pressure " << e.Get() << " (must be greater than or at most equal to reference pressure " << press0 << ") [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
          }
	  DEBUGCOUT("Pressure max: " << press_max << std::endl);
	  
          try {
               Kappa = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

          } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid gas polytropic exponent " << e.Get() << " (must be positive) [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
          }
	  DEBUGCOUT("Kappa: " << Kappa << std::endl);
       }
       
       doublereal weight(0.);
       if (HP.IsKeyWord("weight")) {
          try {
               weight = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

          } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid gravity acceleration (?) constant " << e.Get() << " (must be positive) [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
          }
	  DEBUGCOUT("weight: " << weight << std::endl);
       }
       
       doublereal spring(0.);
       doublereal force0(0.);
       if (HP.IsKeyWord("spring")) {
          try {
               spring = HP.GetReal(0., HighParser::range_ge<doublereal>(0.));

          } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid spring characteristic constant " << e.Get() << " (must be non-negative) [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
          }
	  DEBUGCOUT("spring: " << spring << std::endl);
	  
          try {
               force0 = HP.GetReal(0., HighParser::range_ge<doublereal>(0.));

          } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid spring preload " << e.Get() << " (must be non-negative) [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
          }
	  DEBUGCOUT("force0: " << force0 << std::endl);
       }
       
       /* c dello spostamento */
       doublereal c_spost;
       try {
            c_spost = HP.GetReal(0., HighParser::range_ge<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
            silent_cerr("error: invalid end position displacement constant " << e.Get() << " (must be non-negative) [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
            throw e;
       }
       DEBUGCOUT("c_spost: " << c_spost << std::endl);
       
       /* c della velocita' */
       doublereal c_vel;
       try {
            c_vel = HP.GetReal(0., HighParser::range_ge<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
            silent_cerr("error: invalid end position velocity constant " << e.Get() << " (must be non-negative) [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
            throw e;
       }
       DEBUGCOUT("c_vel: " << c_vel << std::endl);
       
       /* c della accelerazione */
       doublereal c_acc;
       try {
            c_acc = HP.GetReal(0., HighParser::range_ge<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
            silent_cerr("error: invalid end position acceleration constant " << e.Get() << " (must be non-negative) [" << e.what() << "] for Accumulator(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
            throw e;
       }
       DEBUGCOUT("c_acc: " << c_acc << std::endl);
       
       HydraulicFluid* hf = HP.GetHydraulicFluid();
       ASSERT(hf != NULL);
       
       flag fOut = pDM->fReadOutput(HP, Elem::HYDRAULIC);
       
       SAFENEWWITHCONSTRUCTOR(pEl, 
			      Accumulator,
			      Accumulator(uLabel, pDO, hf, pNode, 
					  stroke, start, area, area_pipe, 
					  mass,h_in, h_out,
					  press0, press_max,
					  Kappa, weight, spring, force0, 
					  c_spost, c_vel, c_acc, fOut));
       break;
    }
      
    case TANK: {
       
       /* nodo 1 */
       const PressureNode* pNode1 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 2 */
       const PressureNode* pNode2 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* Pressione serbatoio */
       doublereal press = HP.GetReal();
       if (press <= 0.) {		  
	  silent_cerr("Tank(" << uLabel << "): "
		  "null or negative pressure "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Pressure: " << press << std::endl);
       
       /* Area pipe */
       doublereal area_pipe = HP.GetReal();
       if (area_pipe <= 0.) {		  
	  silent_cerr("Tank(" << uLabel << "): "
		  "null or negative area_pipe "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Area_pipe: " << area_pipe << std::endl); 
       
       /* Area serbatoio */
       doublereal area_serb = HP.GetReal();
       if (area_serb <= 0.) {		  
	  silent_cerr("Tank(" << uLabel << "): "
		  "null or negative area_serb "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Area serbatoio: " << area_serb << std::endl);
       
       /* Livello massimo dell'olio */
       doublereal s_max = HP.GetReal();
       if (s_max < 0.) {		  
	  silent_cerr("Tank(" << uLabel << "): "
		  "negative s_max "
		  "at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }	     
       DEBUGCOUT("Livello massimo dell'olio: " << s_max << std::endl);
       
       /* Livello iniziale */
       doublereal level= .5*s_max; /* valore di default 50% del massimo */
       
       if (HP.IsKeyWord("startlevel")) {
	  level = HP.GetReal();
	  if (level < 0.) {		  
	     silent_cerr("Tank(" << uLabel << "): "
		     "negative level "
		     "at line " << HP.GetLineData() << std::endl);
	     throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
	  }	     
	  DEBUGCOUT("Livello iniziale: " << level << std::endl);
       }
       
       /* Soglia di allarme */
       doublereal s_min = .1*s_max; /* valore di default 10% del massimo */
       if (HP.IsKeyWord("alarmlevel")) {
	  doublereal s_min = HP.GetReal();
	  if (s_min < 0.) {
	     silent_cerr("Tank(" << uLabel << "): "
		     "negative s_min "
		     "at line " << HP.GetLineData() << std::endl);
	     throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
	  }	     
	  DEBUGCOUT("Soglia di allarme: " << s_min << std::endl);
       }
       
       /* c dello spostamento */
       doublereal c_spost = HP.GetReal();
       DEBUGCOUT("c_spost: " << c_spost << std::endl);
       
       HydraulicFluid* hf = HP.GetHydraulicFluid();
       ASSERT(hf != NULL);
       
       flag fOut = pDM->fReadOutput(HP, Elem::HYDRAULIC);
       
       SAFENEWWITHCONSTRUCTOR(pEl, 
			      Tank,
			      Tank (uLabel, pDO, hf, pNode1,pNode2, press,
				    area_pipe, area_serb,
				    level, s_max, s_min, c_spost, fOut));
       break;
    }
      
    case PIPE: {
       
       /* nodo 1 */
       const PressureNode* pNode1 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 2 */
       const PressureNode* pNode2 = pDM->ReadNode<const PressureNode, Node::HYDRAULIC>(HP);
       
       /* Diametro */
       doublereal diameter;
       try {
               diameter = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid diameter " << e.Get() << " (must be positive) [" << e.what() << "] for Pipe(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Diameter: " << diameter << std::endl);
       
       // Area
       // actually, it should be the other way 'round: get the area first, then estimate the hydraulic diameter as sqrt(A/pi)
       doublereal area(diameter*diameter*M_PI_4);
       if (HP.IsKeyWord("area")) {
           try {
               area = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

           } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid area " << e.Get() << " (must be positive) [" << e.what() << "] for Pipe(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
           }
       }
       DEBUGCOUT("Area: " << area << std::endl);
       
       /* Lunghezza */
       doublereal length;
       try {
               length = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid length " << e.Get() << " (must be positive) [" << e.what() << "] for Pipe(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Length: " << length << std::endl); 
       
       /* Transizione se e' 0 parto da laminare se e' 1 parto da turbolento */
       flag turbulent(0);
       if (HP.IsKeyWord("turbulent")) {
	  turbulent = 1;
       }
       DEBUGCOUT("Turbulent: " << (turbulent == 0 ? "false" : "true") << std::endl); 
       doublereal q0(0.);
       if (HP.IsKeyWord("initial" "value")) {
	  q0 = HP.GetReal();
       }
       DEBUGCOUT("Initial flow = " << q0 << std::endl); 
       
       HydraulicFluid* hf = HP.GetHydraulicFluid();
       ASSERT(hf != NULL);
       
       flag fOut = pDM->fReadOutput(HP, Elem::HYDRAULIC);
       
       SAFENEWWITHCONSTRUCTOR(pEl, 
			      Pipe,
			      Pipe(uLabel, pDO, hf, pNode1, pNode2, 
				   diameter, 
				   area, length, turbulent, q0, fOut));
       break;
    }
      
    case DYNAMIC_PIPE: {
       
       /* nodo 1 */
       const PressureNode* pNode1 = pDM->ReadNode<PressureNode, Node::HYDRAULIC>(HP);
       
       /* nodo 2 */
       const PressureNode* pNode2 = pDM->ReadNode<PressureNode, Node::HYDRAULIC>(HP);

       if (pNode2 == pNode1) {
	  silent_cerr("DynamicPipe(" << uLabel << "): "
		  "node 2 and 1 (" << pNode2->GetLabel() << ") "
		  "must differ at line " << HP.GetLineData() << std::endl);
	  throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
       }
       
       doublereal diameter;
       try {
               diameter = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid diameter " << e.Get() << " (must be positive) [" << e.what() << "] for DynamicPipe(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Diameter: " << diameter << std::endl);
       
       // Area      	   
       doublereal area = diameter*diameter*M_PI_4;
       if (HP.IsKeyWord("area")) {
          try {
               area = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

          } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid area " << e.Get() << " (must be positive) [" << e.what() << "] for DynamicPipe(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
          }
       }
       DEBUGCOUT("Area: " << area << std::endl);
       
       /* Lunghezza */
       doublereal length;
       try {
               length = HP.GetReal(0., HighParser::range_gt<doublereal>(0.));

       } catch (HighParser::ErrValueOutOfRange<doublereal>& e) {
               silent_cerr("error: invalid length " << e.Get() << " (must be positive) [" << e.what() << "] for DynamicPipe(" << uLabel << ") at line " << HP.GetLineData() << std::endl);
               throw e;
       }
       DEBUGCOUT("Length: " << length << std::endl); 
       
       /* Transizione se e' 0 parto da laminare se e' 1 parto da turbolento */
       flag turbulent = 0;
       if (HP.IsKeyWord("turbulent")) {
	  turbulent = 1;
	  DEBUGCOUT("Turbulent" << std::endl); 
       }
       doublereal q0 = 0.;
       if (HP.IsKeyWord("initial" "value")) {
	  q0 = HP.GetReal();
	  DEBUGCOUT("Initial q = " << q0 << std::endl); 
       }
       
       HydraulicFluid* hf = HP.GetHydraulicFluid();
       ASSERT(hf != NULL);
       
       flag fOut = pDM->fReadOutput(HP, Elem::HYDRAULIC);

       SAFENEWWITHCONSTRUCTOR(pEl,
			      DynamicPipe,
			      DynamicPipe(uLabel, pDO, hf,
					   pNode1, pNode2, diameter, 
					   area, length, turbulent, q0, fOut));
       break;
    }	   
      
      /* Aggiungere altri elementi idraulici */
      
    default: {
       silent_cerr("unknown hydraulic element type "
	       "for hydraulic element " << uLabel
      	       << " at line " << HP.GetLineData() << std::endl);
       throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
    }	
   }
 
   /* Se non c'e' il punto e virgola finale */
   if (HP.IsArg()) {
      silent_cerr("semicolon expected "
	      "at line " << HP.GetLineData() << std::endl);
      throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
   }
   
   return pEl;
}
