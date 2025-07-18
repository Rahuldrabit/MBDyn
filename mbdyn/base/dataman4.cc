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

/* Lettura elementi */

#include "mbconfig.h"           /* This goes first in every *.c,*.cc file */

#include "mbdefs.h"
#include "modules.h"

#include <float.h>
#include <vector>
#include <set>

#include "dataman.h"
#include "dataman_.h"

/* Elementi */
#include "autostr.h"   /* Elementi automatici associati ai nodi dinamici */
#include "autostrad.h"
#include "automodal.h"
#include "automodalad.h"
#include "gravity.h"   /* Elemento accelerazione di gravita' */
#include "body.h"
#include "inertia.h"
#include "joint.h"
#include "jointreg.h"
#include "force.h"
#include "beam.h"
#include "beam2.h"
#include "hbeam.h"
#include "aerodyn.h"   /* Classe di base degli elementi aerodinamici */
#include "rotor.h"
#include "aeroelem.h"
#include "aeromodal.h"
#include "instruments.h"
#include "genfm.h"
#ifdef USE_EXTERNAL
#include "aeroext.h"
#endif /* USE_EXTERNAL */

#include "driven.h"    /* driven elements */
#include "genel.h"
#include "j2p.h"       /* bind di elementi a nodi parametrici */

#include "preselem.h"
#include "elec.h"
#include "therm.h"
#include "bulk.h"

#include "drive.h"
#include "tpldrive.h"

#include "loadable.h"

#ifdef USE_RTAI
#include "rtai_out_elem.h"
#endif /* USE_RTAI */

#include "socketstream_out_elem.h"
#include "socketstreammotionelem.h"

#include "membrane.h"
#include "shell.h"
#include "solid.h"
#include "solidpress.h"

static int iNumTypes[Elem::LASTELEMTYPE];

/* enum delle parole chiave */
enum KeyWords {
	UNKNOWNKEYWORD = -1,
	END = 0,
	ELEMENTS,

	GRAVITY,
	BODY,
	AUTOMATICSTRUCTURAL,
	JOINT,
	JOINT_REGULARIZATION,
	COUPLE,
	BEAM3,
	BEAM,		/* same as BEAM3 */
	BEAM2,
	HBEAM,

	MEMBRANE4EAS,
	SHELL4EAS,
	SHELL4EASANS,
        HEXAHEDRON8,
        HEXAHEDRON8F,
        HEXAHEDRON8UPC,        
        HEXAHEDRON20,
        HEXAHEDRON20F,     
        HEXAHEDRON20UPC,
        HEXAHEDRON20R,
        HEXAHEDRON20FR,
        HEXAHEDRON20UPCR,
        HEXAHEDRON27,
        HEXAHEDRON27F,      
        PENTAHEDRON15,
        PENTAHEDRON15F,        
        PENTAHEDRON15UPC,
        TETRAHEDRON10,
        TETRAHEDRON10F,      
        TETRAHEDRON10UPC,
        PRESSUREQ4,
        PRESSUREQ8,
        PRESSUREQ9,
        PRESSUREQ8R,
        PRESSURET6,
        TRACTIONQ4,
        TRACTIONQ8,
        TRACTIONQ9,
        TRACTIONQ8R,
        TRACTIONT6,
        UNILATERALINPLANECONTACTQ4,
        UNILATERALINPLANECONTACTQ8,
        UNILATERALINPLANECONTACTQ9,
        UNILATERALINPLANECONTACTQ8R,
        UNILATERALINPLANECONTACTT6,
        UNILATERALINPLANECONTACTQ4L,
        UNILATERALINPLANECONTACTQ8L,
        UNILATERALINPLANECONTACTQ9L,
        UNILATERALINPLANECONTACTQ8RL,
        UNILATERALINPLANECONTACTT6L,

        
	AIRPROPERTIES,
	GUST,
	INDUCEDVELOCITY,
		ROTOR,
	AERODYNAMICBODY,
	AERODYNAMICBEAM3,
	AERODYNAMICBEAM,	/* same as AERODYNAMICBEAM3 */
	AERODYNAMICBEAM2,
	AERODYNAMICEXTERNAL,
	AERODYNAMICEXTERNALMODAL,
	AEROMODAL,
	AIRCRAFTINSTRUMENTS,
	GENERICAERODYNAMICFORCE,

	FORCE,

	GENEL,
	ELECTRIC,

	THERMAL,

	HYDRAULIC,

	BULK,
	USER_DEFINED,
		LOADABLE,		// deprecated
	DRIVEN,

	OUTPUT_ELEMENT,
	SOCKETSTREAM_OUTPUT,		// deprecated
	SOCKETSTREAM_MOTION_OUTPUT,	// deprecated
	RTAI_OUTPUT,			// deprecated

	INERTIA,

	EXISTING,
	OUTPUT,
	BIND,

	LASTKEYWORD
};

void
DataManager::ReadElems(MBDynParser& HP)
{
	DEBUGCOUTFNAME("DataManager::ReadElems");

	/* parole chiave del blocco degli elementi */
	const char* sKeyWords[] = {
		"end",

		"elements",

		"gravity",
		"body",
		"automatic" "structural",
		"joint",
			"joint" "regularization",
		"couple",
		"beam3",
		"beam",
		"beam2",
		"hbeam",

		"membrane4eas",
		"shell4eas",
		"shell4easans",
                "hexahedron8",
                "hexahedron8f",                
                "hexahedron8upc",                
                "hexahedron20",
                "hexahedron20f",                
                "hexahedron20upc",
                "hexahedron20r",
                "hexahedron20fr",
                "hexahedron20upcr",
                "hexahedron27",
                "hexahedron27f",
                "pentahedron15",
                "pentahedron15f",                
                "pentahedron15upc",
                "tetrahedron10",
                "tetrahedron10f",
                "tetrahedron10upc",
                "pressureq4",
                "pressureq8",
                "pressureq9",
                "pressureq8r",
                "pressuret6",
                "tractionq4",
                "tractionq8",
                "tractionq9",
                "tractionq8r",
                "tractiont6",
                "unilateral" "in" "plane" "contact" "q4",
                "unilateral" "in" "plane" "contact" "q8",
                "unilateral" "in" "plane" "contact" "q9",
                "unilateral" "in" "plane" "contact" "q8r",
                "unilateral" "in" "plane" "contact" "t6",

                "unilateral" "in" "plane" "contact" "q4l",
                "unilateral" "in" "plane" "contact" "q8l",
                "unilateral" "in" "plane" "contact" "q9l",
                "unilateral" "in" "plane" "contact" "q8rl",
                "unilateral" "in" "plane" "contact" "t6l",                
                
		"air" "properties",
		"gust",
		"induced" "velocity",
			"rotor",
		"aerodynamic" "body",
		"aerodynamic" "beam3",
		"aerodynamic" "beam",
		"aerodynamic" "beam2",
		"aerodynamic" "external",
		"aerodynamic" "external" "modal",
		"aero" "modal",
		"aircraft" "instruments",
		"generic" "aerodynamic" "force",

		"force",

		"genel",
		"electric",

		"thermal",

		"hydraulic",

		"bulk",
		"user" "defined",
			"loadable",		// deprecated
		"driven",

		"output" "element",
		"stream" "output",		// deprecated
		"stream" "motion" "output",	// deprecated
		"RTAI" "output",		// deprecated

		"inertia",

		"existing",
		"output",
		"bind",

		NULL
	};

	/* tabella delle parole chiave */
	KeyTable K(HP, sKeyWords);

	/* strutture di conteggio degli elementi letti */
	for (int i = 0; i < Elem::LASTELEMTYPE; i++) {
		iNumTypes[i] = ElemData[i].iExpectedNum;
	}

	int iMissingElems = Elems.size();
	DEBUGLCOUT(MYDEBUG_INPUT, "Expected elements: " << iMissingElems << std::endl);

	ElemVecType::iterator ei = Elems.begin();

	/* Aggiunta degli elementi strutturali automatici legati ai nodi dinamici */
	if (ElemData[Elem::AUTOMATICSTRUCTURAL].iExpectedNum > 0) {
		for (NodeContainerType::const_iterator i = NodeData[Node::STRUCTURAL].NodeContainer.begin();
			i != NodeData[Node::STRUCTURAL].NodeContainer.end(); ++i)
		{
                        const DynamicStructNode *pN = dynamic_cast<const DynamicStructNode *>(i->second);
                        if (pN != 0) {
                                Elem *pTmpEl = nullptr;

                                switch (pN->GetStructNodeType()) {
                                case StructNode::DYNAMIC:
                                     if (bUseAutoDiff()) {
                                          SAFENEWWITHCONSTRUCTOR(pTmpEl,
                                                                 AutomaticStructElemAd,
                                                                 AutomaticStructElemAd(dynamic_cast<const DynamicStructNodeAd*>(pN)));
                                     } else {
                                          SAFENEWWITHCONSTRUCTOR(pTmpEl,
                                                                 AutomaticStructElem,
                                                                 AutomaticStructElem(pN));
                                     }
                                     break;
                                case StructNode::MODAL:
                                     if (bUseAutoDiff()) {
                                          SAFENEWWITHCONSTRUCTOR(pTmpEl,
                                                                 AutomaticModalElemAd,
                                                                 AutomaticModalElemAd(dynamic_cast<const ModalNodeAd*>(pN)));
                                     } else {
                                          SAFENEWWITHCONSTRUCTOR(pTmpEl,
                                                                 AutomaticModalElem,
                                                                 AutomaticModalElem(dynamic_cast<const ModalNode*>(pN)));
                                     }
                                     break;
                                default:
                                     continue;
                                }
                                
				InsertElem(ElemData[Elem::AUTOMATICSTRUCTURAL], pN->GetLabel(), pTmpEl);

				*ei = pTmpEl;
				++ei;

				ASSERT(iNumTypes[Elem::AUTOMATICSTRUCTURAL] > 0);

				iMissingElems--;
				iNumTypes[Elem::AUTOMATICSTRUCTURAL]--;
				DEBUGLCOUT(MYDEBUG_INPUT,
					"Initializing automatic structural element linked to StructuralNode("
					<< pN->GetLabel() << ")" << std::endl);
				continue;
			}

			const DynamicStructDispNode *pDN = dynamic_cast<const DynamicStructDispNode *>(i->second);
			if (pDN != 0) {
				// NOTE: could be a modal node
				if (pDN->GetStructDispNodeType() != StructDispNode::DYNAMIC) {
					continue;
				}

				Elem *pTmpEl = 0;

                                if (bUseAutoDiff()) {
                                     SAFENEWWITHCONSTRUCTOR(pTmpEl,
                                                            AutomaticStructDispElemAd,
                                                            AutomaticStructDispElemAd(dynamic_cast<const DynamicStructDispNodeAd*>(pDN)));
                                } else {
                                     SAFENEWWITHCONSTRUCTOR(pTmpEl,
                                                            AutomaticStructDispElem,
                                                            AutomaticStructDispElem(pDN));
                                }
                                
				InsertElem(ElemData[Elem::AUTOMATICSTRUCTURAL], pDN->GetLabel(), pTmpEl);

				*ei = pTmpEl;
				++ei;

				ASSERT(iNumTypes[Elem::AUTOMATICSTRUCTURAL] > 0);

				iMissingElems--;
				iNumTypes[Elem::AUTOMATICSTRUCTURAL]--;
				DEBUGLCOUT(MYDEBUG_INPUT,
					"Initializing automatic structural element linked to StructDispNode(" << pDN->GetLabel() << ")" << std::endl);
				continue;
			}
		}
	}

	while (true) {
		KeyWords CurrDesc;
		try {
			CurrDesc = KeyWords(HP.GetDescription());
		}
		catch (...) {
			silent_cerr("Error while expecting \"end: elements;\" at line " << HP.GetLineData() << std::endl);
			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		if (CurrDesc == END) {
			break;

		} else if (CurrDesc == OUTPUT) {
			DEBUGLCOUT(MYDEBUG_INPUT, "Elements to be output: ");
			Elem::Type Typ;
			switch (KeyWords(HP.GetWord())) {
			case BODY: {
				DEBUGLCOUT(MYDEBUG_INPUT, "bodies" << std::endl);
				Typ = Elem::BODY;
				break;
			}

			case AUTOMATICSTRUCTURAL: {
				DEBUGLCOUT(MYDEBUG_INPUT, "automatic structural" << std::endl);
				Typ = Elem::AUTOMATICSTRUCTURAL;
				break;
			}

			case JOINT: {
				DEBUGLCOUT(MYDEBUG_INPUT, "joints" << std::endl);
				Typ = Elem::JOINT;
				break;
			}

			case BEAM:
			case BEAM3:			/* same as BEAM */
			case BEAM2:
			case HBEAM: {
				DEBUGLCOUT(MYDEBUG_INPUT, "beams" << std::endl);
				Typ = Elem::BEAM;
				break;
			}

			case MEMBRANE4EAS:
			case SHELL4EAS:
			case SHELL4EASANS: {
				DEBUGLCOUT(MYDEBUG_INPUT, "shells" << std::endl);
				Typ = Elem::PLATE;
				break;
			}

                        case HEXAHEDRON8:
                        case HEXAHEDRON8F:
                        case HEXAHEDRON8UPC:                             
                        case HEXAHEDRON20:
                        case HEXAHEDRON20F:                             
                        case HEXAHEDRON20UPC:
                        case HEXAHEDRON20R:
                        case HEXAHEDRON20FR:                             
                        case HEXAHEDRON20UPCR:
                        case HEXAHEDRON27:
                        case HEXAHEDRON27F:                             
                        case PENTAHEDRON15:
                        case PENTAHEDRON15F:                             
                        case PENTAHEDRON15UPC:
                        case TETRAHEDRON10:
                        case TETRAHEDRON10F:                             
                        case TETRAHEDRON10UPC: {
                             DEBUGLCOUT(MYDEBUG_INPUT, "solids\n");
                             Typ = Elem::SOLID;
                             break;
                        }

                        case PRESSUREQ4:
                        case PRESSUREQ8:
                        case PRESSUREQ9:
                        case PRESSUREQ8R:
                        case PRESSURET6:
                        case TRACTIONQ4:
                        case TRACTIONQ8:
                        case TRACTIONQ9:
                        case TRACTIONQ8R:
                        case TRACTIONT6:
                        case UNILATERALINPLANECONTACTQ4:
                        case UNILATERALINPLANECONTACTQ8:
                        case UNILATERALINPLANECONTACTQ9:
                        case UNILATERALINPLANECONTACTQ8R:
                        case UNILATERALINPLANECONTACTT6:
                        case UNILATERALINPLANECONTACTQ4L:
                        case UNILATERALINPLANECONTACTQ8L:
                        case UNILATERALINPLANECONTACTQ9L:
                        case UNILATERALINPLANECONTACTQ8RL:
                        case UNILATERALINPLANECONTACTT6L:                             
                        {
                             DEBUGLCOUT(MYDEBUG_INPUT, "surface loads\n");
                             Typ = Elem::SURFACE_LOAD;
                             break;
                        }

                        case INDUCEDVELOCITY:
			case ROTOR: {
				DEBUGLCOUT(MYDEBUG_INPUT, "induced velocity elements" << std::endl);
				Typ = Elem::INDUCEDVELOCITY;
				break;
			}

			case AEROMODAL: {
				DEBUGLCOUT(MYDEBUG_INPUT, "aeromodal" << std::endl);
				Typ = Elem::AEROMODAL;
				break;
			}

#ifdef USE_EXTERNAL
			case AERODYNAMICEXTERNAL:
			case AERODYNAMICEXTERNALMODAL: {
				DEBUGLCOUT(MYDEBUG_INPUT, "aerodynamic external" << std::endl);
				Typ = Elem::EXTERNAL;
				break;
			}
#endif /* USE_EXTERNAL */

			case AERODYNAMICBODY:
			case AERODYNAMICBEAM:
			case AERODYNAMICBEAM3:	/* same as AERODYNAMICBEAM */
			case AERODYNAMICBEAM2:
			case AIRCRAFTINSTRUMENTS:
			case GENERICAERODYNAMICFORCE: {
				DEBUGLCOUT(MYDEBUG_INPUT, "aerodynamic" << std::endl);
				Typ = Elem::AERODYNAMIC;
				break;
			}

			case FORCE:
			case COUPLE:
			{
				DEBUGLCOUT(MYDEBUG_INPUT, "forces" << std::endl);
				Typ = Elem::FORCE;
				break;
				}

			case GENEL: {
				DEBUGLCOUT(MYDEBUG_INPUT, "genels" << std::endl);
				Typ = Elem::GENEL;
				break;
			}

			case ELECTRIC: {
				DEBUGLCOUT(MYDEBUG_INPUT, "electric" << std::endl);
				Typ = Elem::ELECTRIC;
				break;
			}

			case THERMAL: {
				DEBUGLCOUT(MYDEBUG_INPUT, "thermal" << std::endl);
				Typ = Elem::THERMAL;
				break;
			}

			case HYDRAULIC: {
				DEBUGLCOUT(MYDEBUG_INPUT, "hydraulic elements" << std::endl);
				Typ = Elem::HYDRAULIC;
				break;
			}

			case BULK: {
				DEBUGLCOUT(MYDEBUG_INPUT, "bulk" << std::endl);
				Typ = Elem::BULK;
				break;
			}

			case USER_DEFINED:
			case LOADABLE: {
				DEBUGLCOUT(MYDEBUG_INPUT, "loadable" << std::endl);
				Typ = Elem::LOADABLE;
				break;
			}

			case UNKNOWNKEYWORD: {
				silent_cerr("Error: unknown element type, cannot modify output"
					<< std::endl);
				throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
			}

			default: {
				silent_cerr("Error: element type " << sKeyWords[CurrDesc]
					<< " at line " << HP.GetLineData() << " is not allowed"
					<< std::endl);
				throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			} /* end switch (KeyWords(HP.GetWord()))  */

			/* Elements list */
			while (HP.IsArg()) {
				if (HP.IsKeyWord("range")) {
					unsigned int uL = (unsigned int)HP.GetInt();
					unsigned int uEndL = (unsigned int)HP.GetInt();
					if (uEndL < uL) {
						silent_cerr("End label " << uEndL
							<< " must be larger than "
							"or equal to start label "
							<< uL << std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}
					for ( ; uL <= uEndL; uL++) {
						Elem* pE = dynamic_cast<Elem *>(pFindElem(Typ, uL));
						if (pE != 0) {
							DEBUGLCOUT(MYDEBUG_INPUT, "element " << uL << std::endl);
							pE->SetOutputFlag(flag(1));
							if (dynamic_cast<const Modal *>(pE)) {
								OutputOpen(OutputHandler::MODAL);
							}
						}
					}

				} else {
					unsigned int uL = (unsigned int)HP.GetInt();
					Elem* pE = dynamic_cast<Elem *>(pFindElem(Typ, uL));
					if (pE == 0) {
						silent_cerr(psElemNames[Typ] << "(" << uL << ") "
							"is not defined (ignored); output cannot be modified "
							"at line " << HP.GetLineData()
							<< std::endl);
					} else {
						DEBUGLCOUT(MYDEBUG_INPUT, "element " << uL << std::endl);
						pE->SetOutputFlag(flag(1) | ElemData[Typ].uOutputFlags);
						if (dynamic_cast<const Modal *>(pE)) {
							OutputOpen(OutputHandler::MODAL);
						}
					}
				}
			} /* end while (HP.IsArg()) */

		} else if (CurrDesc == BIND) {
			Elem* pEl = 0;
			Elem::Type t = Elem::UNKNOWN;

			if (HP.IsKeyWord("air" "properties")) {
				t = Elem::AIRPROPERTIES;
				if (ElemData[t].ElemContainer.empty()) {
					silent_cerr("air properties not defined; "
						"cannot bind at line "
						<< HP.GetLineData() << std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
				}
				ElemContainerType::iterator ap = ElemData[t].ElemContainer.begin();
				pEl = ap->second;

			} else {
				/* Label dell'elemento */
				unsigned int uL = HP.GetInt();

				/* Tipo dell'elemento */
				switch (KeyWords(HP.GetWord())) {
				case BODY:
					t = Elem::BODY;
					break;
	
				case AUTOMATICSTRUCTURAL:
					t = Elem::AUTOMATICSTRUCTURAL;
					break;
	
				case JOINT:
					t = Elem::JOINT;
					break;
	
				case FORCE:
				case COUPLE:
					t = Elem::FORCE;
					break;
	
				case INERTIA:
					t = Elem::INERTIA;
					break;
	
				case BEAM:
				case BEAM3:			/* same as BEAM */
				case BEAM2:
				case HBEAM:
					t = Elem::BEAM;
					break;

				case MEMBRANE4EAS:
				case SHELL4EAS:
				case SHELL4EASANS:
					t = Elem::PLATE;
					break;
                                        
                                case HEXAHEDRON8:
                                case HEXAHEDRON8F:
                                case HEXAHEDRON8UPC:
                                case HEXAHEDRON20:
                                case HEXAHEDRON20F:
                                case HEXAHEDRON20UPC:
                                case HEXAHEDRON20R:
                                case HEXAHEDRON20FR:                                     
                                case HEXAHEDRON20UPCR:
                                case HEXAHEDRON27:
                                case HEXAHEDRON27F:                                     
                                case PENTAHEDRON15:
                                case PENTAHEDRON15F:                                   
                                case PENTAHEDRON15UPC:
                                case TETRAHEDRON10:
                                case TETRAHEDRON10F:
                                case TETRAHEDRON10UPC:
                                        t = Elem::SOLID;
                                        break;

                                case PRESSUREQ4:
                                case PRESSUREQ8:
                                case PRESSUREQ9:
                                case PRESSUREQ8R:
                                case PRESSURET6:
                                case TRACTIONQ4:
                                case TRACTIONQ8:
                                case TRACTIONQ9:
                                case TRACTIONQ8R:
                                case TRACTIONT6:
                                case UNILATERALINPLANECONTACTQ4:
                                case UNILATERALINPLANECONTACTQ8:
                                case UNILATERALINPLANECONTACTQ9:
                                case UNILATERALINPLANECONTACTQ8R:
                                case UNILATERALINPLANECONTACTT6:
                                case UNILATERALINPLANECONTACTQ4L:
                                case UNILATERALINPLANECONTACTQ8L:
                                case UNILATERALINPLANECONTACTQ9L:
                                case UNILATERALINPLANECONTACTQ8RL:
                                case UNILATERALINPLANECONTACTT6L:                                     
                                        t = Elem::SURFACE_LOAD;
                                        break;

                                case INDUCEDVELOCITY:
				case ROTOR:
					t = Elem::INDUCEDVELOCITY;
					break;
	
				case AEROMODAL:
					t = Elem::AEROMODAL;
					break;
	
#ifdef USE_EXTERNAL
				case AERODYNAMICEXTERNAL:
				case AERODYNAMICEXTERNALMODAL:
					t = Elem::EXTERNAL;
					break;
#endif /* USE_EXTERNAL */

				case AERODYNAMICBODY:
				case AERODYNAMICBEAM:
				case AERODYNAMICBEAM3:	/* same as AERODYNAMICBEAM */
				case AERODYNAMICBEAM2:
				case AIRCRAFTINSTRUMENTS:
				case GENERICAERODYNAMICFORCE:
					t = Elem::AERODYNAMIC;
					break;
	
				case GENEL:
					t = Elem::GENEL;
					break;
	
				case ELECTRIC:
					t = Elem::ELECTRIC;
					break;
	
				case THERMAL:
					t = Elem::THERMAL;
					break;

				case HYDRAULIC:
					t = Elem::HYDRAULIC;
					break;

				case BULK:
					t = Elem::BULK;
					break;

				case USER_DEFINED:
				case LOADABLE:
					t = Elem::LOADABLE;
					break;

				case OUTPUT_ELEMENT:
				case RTAI_OUTPUT:			// deprecated
				case SOCKETSTREAM_OUTPUT:		// deprecated
				case SOCKETSTREAM_MOTION_OUTPUT:	// deprecated
					silent_cerr(psElemNames[Elem::SOCKETSTREAM_OUTPUT]
						<< " does not support bind" << std::endl);
				default:
					silent_cerr("DataManager::ReadElems: unknown element type (label=" << uL << ") "
						"at line " << HP.GetLineData() << std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
				} /* end switch (KeyWords(HP.GetWord())) */

				pEl = dynamic_cast<Elem*>(pFindElem(t, uL));

				if (!pEl) {
					silent_cerr("cannot find " << psElemNames[t] << " (" << uL << ") "
						"at line " << HP.GetLineData() << std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
				}
			}

			/* Label del nodo parameter */
			unsigned uL = HP.GetInt();

			Elem2Param* pNd = pFindNode<Elem2Param, ParameterNode, Node::PARAMETER>(uL);
			if (pNd == 0) {
				silent_cerr("can't find parameter node (" << uL << ") "
					"at line " << HP.GetLineData() << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}

			/* Numero d'ordine del dato privato a cui fare il binding */
			unsigned int i = 0;
			std::string s;
			if (HP.IsKeyWord("name") || HP.IsKeyWord("string" /* deprecated */ )) {
				s = HP.GetStringWithDelims();

				ASSERT(!s.empty());

				DEBUGCOUT("binding to " << psElemNames[pEl->GetElemType()]
					<< "(" << pEl->GetLabel() << ") private data "
					"\"" << s << "\"" << std::endl);

				i = pEl->iGetPrivDataIdx(s.c_str());

			} else {
				if (HP.IsKeyWord("index")) {
					// may become required
					NO_OP;
				}
				i = HP.GetInt();
			}

			/* indice del dato a cui il parametro e' bound */
			if (i <= 0 || i > pEl->iGetNumPrivData()) {
				if (!s.empty()) {
					silent_cerr("error in private data \"" << s << "\" "
						"for element " << psElemNames[t] << " (" << pEl->GetLabel() << ") "
						"while binding to ParameterNode(" << uL << ") "
						"at line " << HP.GetLineData() << std::endl);
					
				} else {
					silent_cerr("error in private data #" << i << " "
						"for element " << psElemNames[t] << " (" << pEl->GetLabel() << ") "
						"while binding to ParameterNode(" << uL << ") "
						"at line " << HP.GetLineData() << std::endl);
				}
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}

			/* fa il binding del ParameterNode all'elemento */
			DEBUGLCOUT(MYDEBUG_INPUT, "Binding " << psElemNames[t]
				<< " (" << pEl->GetLabel() << ") "
				"to Parameter " << pNd->GetLabel() << std::endl);
			pNd->Bind(pEl, i);

		/* Gust is attached to air properties... */
		} else if (CurrDesc == GUST) {
			if (ElemData[Elem::AIRPROPERTIES].ElemContainer.empty()) {
				silent_cerr("air properties not defined; "
					"cannot add gust at line "
					<< HP.GetLineData() << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			ElemContainerType::iterator ap = ElemData[Elem::AIRPROPERTIES].ElemContainer.begin();
			dynamic_cast<AirProperties *>(ap->second)->AddGust(ReadGustData(this, HP));

		/* gestisco a parte gli elementi automatici strutturali, perche'
		 * sono gia' stati costruiti altrove e li devo solo inizializzare;
		 * eventualmente si puo' fare altrimenti */
		} else if (CurrDesc == AUTOMATICSTRUCTURAL) {
			AutomaticStructDispElem* pASD = ReadElem<AutomaticStructDispElem, Elem::AUTOMATICSTRUCTURAL>(HP);
			ASSERT(pASD != 0);
			AutomaticStructElem* pAS = dynamic_cast<AutomaticStructElem *>(pASD);
			if (pAS) {
				DEBUGCOUT("reading AutomaticStructElem(" << pAS->GetLabel() << ")" << std::endl);

				/* forse e' il caso di usare il riferimento del nodo? */

				/* NOTE: i primi due sono gestiti direttamente
				 * dagli elementi con inerzia, e quindi non sono usati */
				Vec3 q(HP.GetVecAbs(::AbsRefFrame));
				Vec3 g(HP.GetVecAbs(::AbsRefFrame));
				Vec3 qp(HP.GetVecAbs(::AbsRefFrame));
				Vec3 gp(HP.GetVecAbs(::AbsRefFrame));

				DEBUGCOUT("Q  = " << q << std::endl
					<< "G  = " << g << std::endl
					<< "Qp = " << qp << std::endl
					<< "Gp = " << gp << std::endl);

				pAS->Init(q, g, qp, gp);

			} else {
				DEBUGCOUT("reading AutomaticStructDispElem(" << pASD->GetLabel() << ")" << std::endl);

				Vec3 q(HP.GetVecAbs(::AbsRefFrame));
				Vec3 qp(HP.GetVecAbs(::AbsRefFrame));

				DEBUGCOUT("Q  = " << q << std::endl
					<< "Qp = " << qp << std::endl);

				pASD->Init(q, qp);
			}

        /*  <<<<  D E F A U L T  >>>>  :  Read one element and create it */ 
		/* default: leggo un elemento e lo creo */
		} else {              			
			/* puntatore all'elemento */
			Elem* pE = 0;

			unsigned int uLabel;
			switch (CurrDesc) {
			/* Qui vengono elencati gli elementi unici, che non richiedono label
			 * (per ora: accelerazione di gravita' e proprieta' dell'aria */

			/* Accelerazione di gravita' */
			case GRAVITY: {
				silent_cout("Reading Gravity" << std::endl);

				if (iNumTypes[Elem::GRAVITY]-- <= 0) {
					DEBUGCERR("");
					silent_cerr("line " << HP.GetLineData() << ": "
						": Gravity is not defined" << std::endl);

					throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
				}

				pE = ReadGravity(this, HP);
				uLabel = 1;
				InsertElem(ElemData[Elem::GRAVITY], uLabel, pE);

				*ei = pE;
				++ei;

				break;
			}

			/* Elementi aerodinamici: proprieta' dell'aria */
			case AIRPROPERTIES: {
				silent_cout("Reading AirProperties" << std::endl);

				if (iNumTypes[Elem::AIRPROPERTIES]-- <= 0) {
					DEBUGCERR("");
					silent_cerr("line " << HP.GetLineData() << ": "
						"AirProperties is not defined"
						<< std::endl);

					throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
				}

				uLabel = 1;

				pE = ReadAirProperties(this, HP);
				if (pE != 0) {
					InsertElem(ElemData[Elem::AIRPROPERTIES], uLabel, pE);

					*ei = pE;
					++ei;
				}

				break;
			}

			/* Elemento generico: legge la label e fa un nuovo switch */
			default: {
				/* legge la label */
				uLabel = unsigned(HP.GetInt());

				/* in base al tipo, avviene l'allocazione */
				switch (CurrDesc) {
				case DRIVEN: {
					/* Reads the driver */
					DriveCaller* pDC = HP.GetDriveCaller();
					bool bActive(false);
					if (HP.IsKeyWord("initial" "state")) {
						if (HP.IsKeyWord("from" "drive")) {
							// trivial, but impossible when drive is postponed
							bActive = pDC->dGet();

						} else if (HP.IsKeyWord("active")) {
							bActive = true;

						} else if (HP.IsKeyWord("inactive")) {
							bActive = false;

						} else if (HP.IsArg()) {
							bActive = HP.GetBool();

						} else {
							silent_cerr("Driven(" << uLabel << "): "
								"unable to read initial state "
								"at line " << HP.GetLineData()
								<< std::endl);
							throw ErrGeneric(MBDYN_EXCEPT_ARGS);
						}

#if 0
						// FIXME: consistency check?
						if (dynamic_cast<PostponedDriveCaller *>(pDC) == 0) {
							bool bCheck = pDC->dGet();
							if ((bActive && !bCheck) || (!bActive && bCheck)) {
								silent_cerr("Driven(" << uLabel << "): "
									"initial state \"" << (bActive ? "active" : "inactive") << "\" "
									"inconsistent with drive caller's status "
									"at line " << HP.GetLineData()
									<< std::endl);
								throw ErrGeneric(MBDYN_EXCEPT_ARGS);
							}
						}
#endif

					} else {
						bActive = (pDC->dGet() != 0.);
					}

					std::vector<std::string> hints;
					for (unsigned i = 1; HP.IsKeyWord("hint"); i++) {
						// const char *hint = HP.GetStringWithDelims(HighParser::DEFAULTDELIM, false);
						// do escape; needed to deal with quotes in the hint specification
						const char *hint = HP.GetStringWithDelims();
						if (hint == 0) {
							silent_cerr("Driven(" << uLabel << "): "
								"unable to read hint #" << i
								<< " at line " << HP.GetLineData()
								<< std::endl);
							throw ErrGeneric(MBDYN_EXCEPT_ARGS);
						}
						hints.push_back(hint);
					}

					HP.ExpectDescription();
					KeyWords CurrDriven = KeyWords(HP.GetDescription());

#ifdef DEBUG
					switch (CurrDriven) {
					case FORCE:
					case BODY:
					case JOINT:
					case JOINT_REGULARIZATION:
					case COUPLE:
					case BEAM:
					case BEAM3:			/* same as BEAM */
					case BEAM2:
					case HBEAM:
					case SHELL4EAS:
					case SHELL4EASANS:
                                        case HEXAHEDRON8:
                                        case HEXAHEDRON8UPC:                                             
                                        case HEXAHEDRON20:
                                        case HEXAHEDRON20UPC:
                                        case HEXAHEDRON20UPCR:
                                        case HEXAHEDRON20R:
                                        case HEXAHEDRON27:
                                        case PENTAHEDRON15:
                                        case PENTAHEDRON15UPC:
                                        case TETRAHEDRON10:
                                        case TETRAHEDRON10UPC:
                                        case PRESSUREQ4:
                                        case PRESSUREQ8:
                                        case PRESSUREQ9:
                                        case PRESSUREQ8R:
                                        case PRESSURET6:
                                        case TRACTIONQ4:
                                        case TRACTIONQ8:
                                        case TRACTIONQ9:
                                        case TRACTIONQ8R:
                                        case TRACTIONT6:
                                        case UNILATERALINPLANECONTACTQ4:
                                        case UNILATERALINPLANECONTACTQ8:
                                        case UNILATERALINPLANECONTACTQ9:
                                        case UNILATERALINPLANECONTACTQ8R:
                                        case UNILATERALINPLANECONTACTT6:
                                        case UNILATERALINPLANECONTACTQ4L:
                                        case UNILATERALINPLANECONTACTQ8L:
                                        case UNILATERALINPLANECONTACTQ9L:
                                        case UNILATERALINPLANECONTACTQ8RL:
                                        case UNILATERALINPLANECONTACTT6L:                                                                                          
					case INDUCEDVELOCITY:
					case ROTOR:
					case AERODYNAMICBODY:
					case AERODYNAMICBEAM:
					case AERODYNAMICBEAM3:	/* same as AERODYNAMICBEAM */
					case AERODYNAMICBEAM2:
					case AIRCRAFTINSTRUMENTS:
					case GENERICAERODYNAMICFORCE:

					case GENEL:
					case ELECTRIC:

					case THERMAL:

					case HYDRAULIC:

					case BULK:
					case USER_DEFINED:
					case LOADABLE:
					case EXISTING:
						DEBUGLCOUT(MYDEBUG_INPUT, "OK, this element can be driven" << std::endl);
						break;

					case OUTPUT_ELEMENT:
					case RTAI_OUTPUT:			// deprecated
					case SOCKETSTREAM_OUTPUT:		// deprecated
					case SOCKETSTREAM_MOTION_OUTPUT:	// deprecated
						silent_cerr(psElemNames[Elem::SOCKETSTREAM_OUTPUT]
							<< " cannot be driven" << std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);

					default:
						DEBUGCERR("warning, this element cannot be driven" << std::endl);
						break;
					}
#endif /* DEBUG */

					Elem **ppE = 0;
					bool bExisting(false);
					if (CurrDriven == EXISTING) {
						bExisting = true;
						iMissingElems++;
						CurrDriven = KeyWords(HP.GetWord());
						unsigned int uL = (unsigned int)HP.GetInt();
						if (uL != uLabel) {
							silent_cerr("Error: the driving element must have "
								"the same label of the driven one"
								<< std::endl);

							throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
						}

						/* FIXME: use pFindElem() instead? */
						switch (CurrDriven) {
						case FORCE:
							ppE = ppFindElem(Elem::FORCE, uLabel);
							break;

						case BODY:
							ppE = ppFindElem(Elem::BODY, uLabel);
							break;

						case JOINT:
							ppE = ppFindElem(Elem::JOINT, uLabel);
							break;

						case JOINT_REGULARIZATION:
							ppE = ppFindElem(Elem::JOINT_REGULARIZATION, uLabel);
							break;

						case COUPLE:
							ppE = ppFindElem(Elem::FORCE, uLabel);
							break;

						case BEAM:
						case BEAM3:		/* same as BEAM */
						case BEAM2:
						case HBEAM:
							ppE = ppFindElem(Elem::BEAM, uLabel);
							break;

						case MEMBRANE4EAS:
						case SHELL4EAS:
						case SHELL4EASANS:
							ppE = ppFindElem(Elem::PLATE, uLabel);
							break;

                                                case HEXAHEDRON8:
                                                case HEXAHEDRON8UPC:                                                     
                                                case HEXAHEDRON20:
                                                case HEXAHEDRON20UPC:
                                                case HEXAHEDRON20UPCR:
                                                case HEXAHEDRON20R:
                                                case HEXAHEDRON27:
                                                case PENTAHEDRON15:
                                                case PENTAHEDRON15UPC:
                                                case TETRAHEDRON10:
                                                case TETRAHEDRON10UPC:
                                                        ppE = ppFindElem(Elem::SOLID, uLabel);
                                                        break;

                                                case PRESSUREQ4:
                                                case PRESSUREQ8:
                                                case PRESSUREQ9:
                                                case PRESSUREQ8R:
                                                case PRESSURET6:
                                                case TRACTIONQ4:
                                                case TRACTIONQ8:
                                                case TRACTIONQ9:
                                                case TRACTIONQ8R:
                                                case TRACTIONT6:
                                                case UNILATERALINPLANECONTACTQ4:
                                                case UNILATERALINPLANECONTACTQ8:
                                                case UNILATERALINPLANECONTACTQ9:
                                                case UNILATERALINPLANECONTACTQ8R:
                                                case UNILATERALINPLANECONTACTT6:
                                                case UNILATERALINPLANECONTACTQ4L:
                                                case UNILATERALINPLANECONTACTQ8L:
                                                case UNILATERALINPLANECONTACTQ9L:
                                                case UNILATERALINPLANECONTACTQ8RL:
                                                case UNILATERALINPLANECONTACTT6L:                                                     
                                                        ppE = ppFindElem(Elem::SURFACE_LOAD, uLabel);
                                                        break;

						case INDUCEDVELOCITY:
						case ROTOR:
							ppE = ppFindElem(Elem::INDUCEDVELOCITY, uLabel);
							break;

						case AERODYNAMICBODY:
						case AERODYNAMICBEAM:
						case AERODYNAMICBEAM3:	/* same as AERODYNAMICBEAM */
						case AERODYNAMICBEAM2:
						case AIRCRAFTINSTRUMENTS:
						case GENERICAERODYNAMICFORCE:
							ppE = ppFindElem(Elem::AERODYNAMIC, uLabel);
							break;

						case GENEL:
							ppE = ppFindElem(Elem::GENEL, uLabel);
							break;

						case ELECTRIC:
							ppE = ppFindElem(Elem::ELECTRIC, uLabel);
							break;

						case THERMAL:
							ppE = ppFindElem(Elem::THERMAL, uLabel);
							break;

						case HYDRAULIC:
							ppE = ppFindElem(Elem::HYDRAULIC, uLabel);
							break;

						case BULK:
							ppE = ppFindElem(Elem::BULK, uLabel);
							break;

						case USER_DEFINED:
						case LOADABLE:
							ppE = ppFindElem(Elem::LOADABLE, uLabel);
							break;

						default:
							DEBUGCERR("warning, this element can't be driven" << std::endl);
							break;
							
						}  /*switch (CurrDriven) */

						if (ppE == NULL) {
							silent_cerr("Error: element " << uLabel
								<< "cannot be driven since it doesn't exist"
								<< std::endl);

							throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
						}

						pE = *ppE;

						flag fOut = fReadOutput(HP, pE->GetElemType());
						pE->SetOutputFlag(fOut);

					} else {
						unsigned int uDummy = (unsigned int)HP.GetInt();
						if (uDummy != uLabel) {
							silent_cerr("Error: the element label "
								"(" << uDummy << ") "
								"must be the same of the driving element "
								"(" << uLabel << ") at line "
								<< HP.GetLineData() << std::endl);

							throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
						}

						/* Reads the true element */
						ppE = ReadOneElem(HP, uLabel, "", CurrDriven);
						if (ppE == NULL) {
							DEBUGCERR("");
							silent_cerr("error in allocation of element "
								<< uLabel << std::endl);

							throw ErrMemory(MBDYN_EXCEPT_ARGS);
						}

						pE = *ppE;
					}  /*  if (CurrDriven == EXISTING) {..} else {..}  */

					SimulationEntity::Hints *pHints = 0;
					if (!hints.empty()) {
						for (std::vector<std::string>::const_iterator i = hints.begin();
							i != hints.end(); ++i)
						{
							Hint *ph = pE->ParseHint(this, i->c_str());
							if (ph != 0) {
								if (pHints == 0) {
									pHints = new SimulationEntity::Hints;
								}
								pHints->push_back(ph);
							}
						}
					}

					/* Creates the driver for the element */
					Elem* pEl = 0;
					SAFENEWWITHCONSTRUCTOR(pEl,
						DrivenElem,
						DrivenElem(this, pDC, bActive, pE, pHints));

					if (bExisting) {
						ElemVecType::iterator eitmp = Elems.begin();
						for (; eitmp != Elems.end(); ++eitmp) {
							if (*eitmp == pE) {
								*eitmp = pEl;
								break;
							}
						}

						ASSERT(eitmp != Elems.end());

					} else {
						*ei = pEl;
						++ei;
					}

					/* Substitutes the element with the driver */
					pE = *ppE = pEl;

					break;
				}  /* end case DRIVEN: */

                /*  <<<<  N O R M A L   E L E M E N T S  >>>>>  */
				/* Normal element */
				case FORCE:

				case BODY:
				case INERTIA:
				case JOINT:
				case JOINT_REGULARIZATION:
				case COUPLE:
				case BEAM:
				case BEAM3:		/* same as BEAM */
				case BEAM2:
				case HBEAM:
				case MEMBRANE4EAS:
				case SHELL4EAS:
				case SHELL4EASANS:
                                case HEXAHEDRON8:
                                case HEXAHEDRON8F:                                     
                                case HEXAHEDRON8UPC:                                     
                                case HEXAHEDRON20:
                                case HEXAHEDRON20F:                                     
                                case HEXAHEDRON20UPC:
                                case HEXAHEDRON20R:
                                case HEXAHEDRON20FR:                                     
                                case HEXAHEDRON20UPCR:
                                case HEXAHEDRON27:
                                case HEXAHEDRON27F:                                     
                                case PENTAHEDRON15:
                                case PENTAHEDRON15F:                                     
                                case PENTAHEDRON15UPC:
                                case TETRAHEDRON10:
                                case TETRAHEDRON10F:                                     
                                case TETRAHEDRON10UPC:
                                case PRESSUREQ4:
                                case PRESSUREQ8:
                                case PRESSUREQ9:
                                case PRESSUREQ8R:
                                case PRESSURET6:
                                case TRACTIONQ4:
                                case TRACTIONQ8:
                                case TRACTIONQ9:
                                case TRACTIONQ8R:
                                case TRACTIONT6:
                                case UNILATERALINPLANECONTACTQ4:
                                case UNILATERALINPLANECONTACTQ8:
                                case UNILATERALINPLANECONTACTQ9:
                                case UNILATERALINPLANECONTACTQ8R:
                                case UNILATERALINPLANECONTACTT6:
                                case UNILATERALINPLANECONTACTQ4L:
                                case UNILATERALINPLANECONTACTQ8L:
                                case UNILATERALINPLANECONTACTQ9L:
                                case UNILATERALINPLANECONTACTQ8RL:
                                case UNILATERALINPLANECONTACTT6L:
                                     
				case GUST:
				case INDUCEDVELOCITY:
				case ROTOR:
				case AERODYNAMICBODY:
				case AERODYNAMICBEAM:
				case AERODYNAMICBEAM3:	/* same as AERODYNAMICBEAM */
				case AERODYNAMICBEAM2:
				case AIRCRAFTINSTRUMENTS:
				case GENERICAERODYNAMICFORCE:
#ifdef USE_EXTERNAL
				case AERODYNAMICEXTERNAL:
				case AERODYNAMICEXTERNALMODAL:
#endif /* USE_EXTERNAL */
				case AEROMODAL:

				case GENEL:
				case ELECTRIC:

				case THERMAL:

				case HYDRAULIC:

				case BULK:
				case USER_DEFINED:
				case LOADABLE:

				case OUTPUT_ELEMENT:
				case RTAI_OUTPUT:			// deprecated
				case SOCKETSTREAM_OUTPUT:		// deprecated
				case SOCKETSTREAM_MOTION_OUTPUT:	// deprecated
				{
					Elem **ppE = 0;

					/* Nome dell'elemento */
					std::string sName;
					if (HP.IsKeyWord("name")) {
						const char *sTmp = HP.GetStringWithDelims();
						if (sTmp == 0) {
							silent_cerr("error - element type "
								<< sKeyWords[CurrDesc] << ": "
								"unable to parse name"
								" at line " << HP.GetLineData()
								<< std::endl);
							throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
						}
						sName = sTmp;
					}

#ifdef USE_RUNTIME_LOADING
					if ((CurrDesc == LOADABLE || CurrDesc == USER_DEFINED)
						&& !moduleInitialized)
					{
						module_initialize();
						moduleInitialized = true;
					}
#endif // USE_RUNTIME_LOADING

					ppE = ReadOneElem(HP, uLabel, sName, CurrDesc);

					if (ppE != 0) {
						pE = *ppE;
						if (!sName.empty() && sName != pE->GetName()) {
							pE->PutName(sName);
						}

						*ei = *ppE;
						++ei;
					}

					break;
				}  /* end case 'Normal elements'*/

				/* in caso di tipo sconosciuto */
				case UNKNOWNKEYWORD: {
					DEBUGCERR("");
					silent_cerr("error - unknown element type at line "
						<< HP.GetLineData() << std::endl);

					throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
				}

				default: {
					DEBUGCERR("");
					silent_cerr("error - element type " << sKeyWords[CurrDesc]
						<< " at line " << HP.GetLineData()
						<< " is not allowed " << std::endl);

					throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
				}
				}  /* end switch (CurrDesc) 'in base al tipo'  */
				
			}  /* end case default: 'Elemento generico' */
			}  /* end switch (CurrDesc) 'Elemento generico' */

			/* verifica dell'allocazione */
			if (pE != 0) {
				/* decrementa il totale degli elementi mancanti */
				iMissingElems--;
			}
			
		}  /* end <<<<  D E F A U L T  >>>>  :  Read one element and create it */ 
		   /* end default: leggo un elemento e lo creo */
		 
	}  /* while ((CurrDesc = KeyWords(HP.GetDescription())) != END) */

	if (KeyWords(HP.GetWord()) != ELEMENTS) {
		DEBUGCERR("");
		silent_cerr("<end: elements;> expected at line"
			<< HP.GetLineData() << std::endl);

		throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	/* Se non c'e' il punto e virgola finale */
	if (HP.IsArg()) {
		DEBUGCERR("");
		silent_cerr("semicolon expected at line " << HP.GetLineData()
			<< std::endl);

		throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	if (iMissingElems > 0) {
		DEBUGCERR("");
		silent_cerr("error: " << iMissingElems
			<< " elements are missing;" << std::endl);
		for (int iCnt = 0; iCnt < Elem::LASTELEMTYPE; iCnt++) {
			if (iNumTypes[iCnt] > 0) {
				silent_cerr("  " << iNumTypes[iCnt]
					<< ' ' << psElemNames[iCnt] << std::endl);
			}
		}

		throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	/* Linka gli elementi che generano forze d'inerzia all'elemento
	 * accelerazione di gravita' */
	if (!ElemData[Elem::GRAVITY].ElemContainer.empty()) {
		Gravity* pGrav = dynamic_cast<Gravity *>(ElemData[Elem::GRAVITY].ElemContainer.begin()->second);
		ASSERT(pGrav != 0);

		for (int iCnt = 0; iCnt < Elem::LASTELEMTYPE; iCnt++) {
			if (ElemData[iCnt].bGeneratesInertiaForces()
				&& !ElemData[iCnt].ElemContainer.empty())
			{
				for (ElemContainerType::const_iterator p = ElemData[iCnt].ElemContainer.begin();
					p != ElemData[iCnt].ElemContainer.end(); ++p)
				{
					GravityOwner *pGO = Cast<GravityOwner>(p->second);

					ASSERT(pGO != 0);
					pGO->PutGravity(pGrav);
				}
			}
		}
	}

	/* Linka gli elementi che usano le proprieta' dell'aria all'elemento
	 * proprieta' dell'aria */
	if (!ElemData[Elem::AIRPROPERTIES].ElemContainer.empty()) {
		AirProperties* pProp = dynamic_cast<AirProperties *>(ElemData[Elem::AIRPROPERTIES].ElemContainer.begin()->second);
		ASSERT(pProp != 0);

		for (int iCnt = 0; iCnt < Elem::LASTELEMTYPE; iCnt++) {
			if (ElemData[iCnt].bUsesAirProperties()
				&& !ElemData[iCnt].ElemContainer.empty())
			{
				for (ElemContainerType::const_iterator p = ElemData[iCnt].ElemContainer.begin();
					p != ElemData[iCnt].ElemContainer.end(); ++p)
				{
					AerodynamicElemBase *pAE = Cast<AerodynamicElemBase>(p->second);

					ASSERT(pAE != 0);
					pAE->PutAirProperties(pProp);
				}
			}
		}

	} else { /* '' */
		/* Esegue un controllo per vedere se esistono elementi aerodinamici
		 * ma non sono definite le proprieta' dell'aria, nel qual caso
		 * il calcolo deve essere arrestato */
		bool bStop(false);

		for (int iCnt = 0; iCnt < Elem::LASTELEMTYPE; iCnt++) {
			if (ElemData[iCnt].bUsesAirProperties()
				&& !ElemData[iCnt].ElemContainer.empty())
			{
				for (ElemContainerType::const_iterator p = ElemData[iCnt].ElemContainer.begin();
					p != ElemData[iCnt].ElemContainer.end(); ++p)
				{
					if (dynamic_cast<AerodynamicElemBase *>(p->second)->NeedsAirProperties()) {
						if (!bStop) {
							silent_cerr("The following aerodynamic elements "
								"are defined: " << std::endl);
							bStop = true;
						}
					}
				}
			}
		}

		if (bStop) {
			silent_cerr("while no air properties are defined; aborting..."
				<< std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
	}

#ifdef DEBUG
	ASSERT(ei == Elems.end());

	/* count & initialize element array */
	unsigned iNumElems = 0;
	for (int iCnt = 0; iCnt < Elem::LASTELEMTYPE; iCnt++) {
		iNumElems += ElemData[iCnt].ElemContainer.size();
	}
	ASSERT(iNumElems == Elems.size());
#endif // DEBUG

	DEBUGLCOUT(MYDEBUG_INPUT, "End of elements data" << std::endl);
} /*  End of DataManager::ReadElems()  */

Elem**
DataManager::ReadOneElem(MBDynParser& HP, unsigned int uLabel, const std::string& sName, int CurrType)
{
	Elem* pE = nullptr;
	Elem** ppE = nullptr;

	switch (KeyWords(CurrType)) {
	/* forza */
	case FORCE:
	case COUPLE:
	{
		bool bCouple(false);
		if (KeyWords(CurrType) == FORCE) {
			silent_cout("Reading Force(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		} else /* if(KeyWords(CurrType) == COUPLE) */ {
			bCouple = true;
			silent_cout("Reading Couple(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);
		}

		if (iNumTypes[Elem::FORCE]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData()
				<< ": Force(" << uLabel << ") "
				<< "exceeds force elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::FORCE, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData()
				<< ": Force(" << uLabel << ") "
				"already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		pE = ReadForce(this, HP, uLabel, bCouple);
		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::FORCE], uLabel, pE);
		}

		break;
	}

	case BODY: {
		silent_cout("Reading Body(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		if (iNumTypes[Elem::BODY]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData()
				<< ": Body(" << uLabel << ") "
				"exceeds rigid body elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::BODY, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData()
				<< ": Body(" << uLabel << ") "
				"already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		pE = ReadBody(this, HP, uLabel);
		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::BODY], uLabel, pE);
		}

		break;
	}

	/* vincoli */
	case JOINT: {
		silent_cout("Reading Joint(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		if (iNumTypes[Elem::JOINT]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData()
				<< ": Joint(" << uLabel << ") "
				"exceeds joint elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::JOINT, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData()
				<< ": Joint(" << uLabel << ") "
				"already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		int i = ElemData[Elem::JOINT].iExpectedNum
			- iNumTypes[Elem::JOINT] - 1;
		DofOwner* pDO = DofData[DofOwner::JOINT].pFirstDofOwner + i;

		pE = ReadJoint(this, HP, pDO, uLabel);
		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::JOINT], uLabel, pE);
		}

		/* attenzione: i Joint aggiungono DofOwner e quindi devono
		 * completare la relativa struttura */
		break;
	}

	/* regolarizzazione vincoli */
	case JOINT_REGULARIZATION: {
		silent_cout("Reading JointRegularization(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		if (iNumTypes[Elem::JOINT_REGULARIZATION]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData()
				<< ": " << psElemNames[Elem::JOINT_REGULARIZATION]
				<< "(" << uLabel << ")"
				" exceeds " << psElemNames[Elem::JOINT_REGULARIZATION]
				<< " elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::JOINT_REGULARIZATION, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData()
				<< ": " << psElemNames[Elem::JOINT_REGULARIZATION]
				<< "(" << uLabel << ")"
				<< " already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		pE = ReadJointRegularization(this, HP, uLabel);
		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::JOINT_REGULARIZATION], uLabel, pE);
		}

		/* attenzione: i Joint aggiungono DofOwner e quindi devono
		 * completare la relativa struttura */
		break;
	}

	/* trave */
	case BEAM:
	case BEAM3:		/* same as BEAM */
	case BEAM2:
	case HBEAM: {
		silent_cout("Reading Beam(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		if (iNumTypes[Elem::BEAM]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"Beam(" << uLabel << ") "
				"exceeds beam elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::BEAM, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"Beam(" << uLabel << ") "
				"already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		switch (KeyWords(CurrType)) {
		case BEAM3:
		case BEAM:	/* same as BEAM3 */
			pE = ReadBeam(this, HP, uLabel);
			break;

		case BEAM2:
			pE = ReadBeam2(this, HP, uLabel);
			break;

		case HBEAM:
			pE = ReadHBeam(this, HP, uLabel);
			break;

		default:
			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::BEAM], uLabel, pE);
		}

		break;
	}

	/* shell */
	case MEMBRANE4EAS:
	case SHELL4EAS:
	case SHELL4EASANS: {
		const char *sType;
		switch (KeyWords(CurrType)) {
		case MEMBRANE4EAS:
			sType = "Membrane";
			break;

		case SHELL4EAS:
		case SHELL4EASANS:
			sType = "Shell";
			break;

		default:
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		silent_cout("Reading " << sType << "(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		if (iNumTypes[Elem::PLATE]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				<< sType << "(" << uLabel << ") "
				"exceeds plate elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::PLATE, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				<< sType << "(" << uLabel << ") "
				"already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		int i = ElemData[Elem::PLATE].iExpectedNum
			- iNumTypes[Elem::PLATE] - 1;
		DofOwner* pDO = DofData[DofOwner::PLATE].pFirstDofOwner + i;

		/* allocazione e creazione */
		switch (KeyWords(CurrType)) {
		case MEMBRANE4EAS:
			pE = ReadMembrane4EAS(this, HP, pDO, uLabel);
			break;

		case SHELL4EAS:
			pE = ReadShell4EAS(this, HP, pDO, uLabel);
			break;

		case SHELL4EASANS:
			pE = ReadShell4EASANS(this, HP, pDO, uLabel);
			break;

		default:
			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::PLATE], uLabel, pE);
		}

		break;
	}

        case HEXAHEDRON8:
        case HEXAHEDRON8F:
        case HEXAHEDRON8UPC:
        case HEXAHEDRON20:
        case HEXAHEDRON20F:
        case HEXAHEDRON20UPC:
        case HEXAHEDRON20R:
        case HEXAHEDRON20FR:
        case HEXAHEDRON20UPCR:
        case HEXAHEDRON27:
        case HEXAHEDRON27F:
        case PENTAHEDRON15:
        case PENTAHEDRON15F:
        case PENTAHEDRON15UPC:
        case TETRAHEDRON10:
        case TETRAHEDRON10F:
        case TETRAHEDRON10UPC: {
                static constexpr char sType[][17] = {
                        "Hexahedron8",
                        "Hexahedron8f",
                        "Hexahedron8upc",
                        "Hexahedron20",
                        "Hexahedron20f",
                        "Hexahedron20upc",
                        "Hexahedron20r",
                        "Hexahedron20fr",
                        "Hexahedron20upcr",
                        "Hexahedron27",
                        "Hexahedron27f",
                        "Pentahedron15",
                        "Pentahedron15f",
                        "Pentahedron15upc",
                        "Tetrahedron10",
                        "Tetrahedron10f",
                        "Tetrahedron10upc"
                };

                constexpr integer iNumElemTypes = sizeof(sType) / sizeof(sType[0]);

                static_assert(HEXAHEDRON8UPC - HEXAHEDRON8 < iNumElemTypes, "index out of range");
                static_assert(HEXAHEDRON8F - HEXAHEDRON8 < iNumElemTypes, "index out of range");
                static_assert(HEXAHEDRON20 - HEXAHEDRON8 < iNumElemTypes, "index out of range");
                static_assert(HEXAHEDRON20F - HEXAHEDRON8 < iNumElemTypes, "index out of range");
                static_assert(HEXAHEDRON20UPC - HEXAHEDRON8 < iNumElemTypes, "index out of range");
                static_assert(HEXAHEDRON20R - HEXAHEDRON8 < iNumElemTypes, "index out of range");
                static_assert(HEXAHEDRON20FR - HEXAHEDRON8 < iNumElemTypes, "index out of range");
                static_assert(HEXAHEDRON20UPCR - HEXAHEDRON8 < iNumElemTypes, "index out of range");
                static_assert(HEXAHEDRON27 - HEXAHEDRON8 < iNumElemTypes, "index out of range");
                static_assert(HEXAHEDRON27F - HEXAHEDRON8 < iNumElemTypes, "index out of range");
                static_assert(PENTAHEDRON15 - HEXAHEDRON8 < iNumElemTypes, "index out of range");
                static_assert(PENTAHEDRON15F - HEXAHEDRON8 < iNumElemTypes, "index out of range");
                static_assert(PENTAHEDRON15UPC - HEXAHEDRON8 < iNumElemTypes, "index out of range");
                static_assert(TETRAHEDRON10 - HEXAHEDRON8 < iNumElemTypes, "index out of range");
                static_assert(TETRAHEDRON10F - HEXAHEDRON8 < iNumElemTypes, "index out of range");
                static_assert(TETRAHEDRON10UPC - HEXAHEDRON8 < iNumElemTypes, "index out of range");

                ASSERT(CurrType - HEXAHEDRON8 < iNumElemTypes);

                silent_cout("Reading " << sType[CurrType - HEXAHEDRON8] << "(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")\n");

                if (!bUseAutoDiff()) {
                     silent_cerr("element type " << sType[CurrType - HEXAHEDRON8] << " requires support for automatic differentiation at line " << HP.GetLineData()
                                 << "\nadd the statement \"use automatic differentiation;\" inside the control data section in order to enable it!\n");
                     throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                }

                if (iNumTypes[Elem::SOLID]-- <= 0) {
                        DEBUGCERR("");
                        silent_cerr("line " << HP.GetLineData() << ": "
                                << sType[CurrType - HEXAHEDRON8] << "(" << uLabel << ") "
                                "exceeds solid elements number" << std::endl);

                        throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                }

                if (pFindElem(Elem::SOLID, uLabel) != NULL) {
                        DEBUGCERR("");
                        silent_cerr("line " << HP.GetLineData() << ": "
                                << sType[CurrType - HEXAHEDRON8] << "(" << uLabel << ") "
                                "already defined" << std::endl);

                        throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                }

                switch (CurrType) {
                case HEXAHEDRON8:
                     pE = ReadSolid<Hexahedron8, Gauss2x2x2>(this, HP, uLabel);
                     break;
                case HEXAHEDRON8F:
                     pE = ReadSolid<Hexahedron8f, Gauss2x2x2>(this, HP, uLabel);
                     break;
                case HEXAHEDRON8UPC:
                     pE = ReadSolid<Hexahedron8upc, Gauss2x2x2>(this, HP, uLabel);
                     break;
                case HEXAHEDRON20:
                     pE = ReadSolid<Hexahedron20, Gauss3x3x3>(this, HP, uLabel);
                     break;
                case HEXAHEDRON20F:
                     pE = ReadSolid<Hexahedron20f, Gauss3x3x3>(this, HP, uLabel);
                     break;
                case HEXAHEDRON20UPC:
                     pE = ReadSolid<Hexahedron20upc, Gauss3x3x3>(this, HP, uLabel);
                     break;
                case HEXAHEDRON20R:
                     pE = ReadSolid<Hexahedron20r, GaussH20r>(this, HP, uLabel);
                     break;
                case HEXAHEDRON20FR:
                     pE = ReadSolid<Hexahedron20fr, GaussH20r>(this, HP, uLabel);
                     break;
                case HEXAHEDRON20UPCR:
                     pE = ReadSolid<Hexahedron20upcr, GaussH20r>(this, HP, uLabel);
                     break;
                case HEXAHEDRON27:
                     pE = ReadSolid<Hexahedron27, Gauss3x3x3>(this, HP, uLabel);
                     break;
                case HEXAHEDRON27F:
                     pE = ReadSolid<Hexahedron27f, Gauss3x3x3>(this, HP, uLabel);
                     break;
                case PENTAHEDRON15:
                     pE = ReadSolid<Pentahedron15, CollocPenta15>(this, HP, uLabel);
                     break;
                case PENTAHEDRON15F:
                     pE = ReadSolid<Pentahedron15f, CollocPenta15>(this, HP, uLabel);
                     break;
                case PENTAHEDRON15UPC:
                     pE = ReadSolid<Pentahedron15upc, CollocPenta15>(this, HP, uLabel);
                     break;
                case TETRAHEDRON10:
                     pE = ReadSolid<Tetrahedron10, CollocTet10h>(this, HP, uLabel);
                     break;
                case TETRAHEDRON10F:
                     pE = ReadSolid<Tetrahedron10f, CollocTet10h>(this, HP, uLabel);
                     break;
                case TETRAHEDRON10UPC:
                     pE = ReadSolid<Tetrahedron10upc, CollocTet10h>(this, HP, uLabel);
                     break;
                default:
                     ASSERT(0);
                }

                if (pE) {
                        ppE = InsertElem(ElemData[Elem::SOLID], uLabel, pE);
                }

                break;
        }

        case PRESSUREQ4:
        case PRESSUREQ8:
        case PRESSUREQ9:
        case PRESSUREQ8R:
        case PRESSURET6:
        case TRACTIONQ4:
        case TRACTIONQ8:
        case TRACTIONQ9:
        case TRACTIONQ8R:
        case TRACTIONT6:
        case UNILATERALINPLANECONTACTQ4:
        case UNILATERALINPLANECONTACTQ8:
        case UNILATERALINPLANECONTACTQ9:
        case UNILATERALINPLANECONTACTQ8R:
        case UNILATERALINPLANECONTACTT6:
        case UNILATERALINPLANECONTACTQ4L:
        case UNILATERALINPLANECONTACTQ8L:
        case UNILATERALINPLANECONTACTQ9L:
        case UNILATERALINPLANECONTACTQ8RL:
        case UNILATERALINPLANECONTACTT6L: {
                static constexpr char sType[][29] = {
                        "Pressureq4",
                        "Pressureq8",
                        "Pressureq9",
                        "Pressureq8r",
                        "Pressuret6",
                        "Tractionq4",
                        "Tractionq8",
                        "Tractionq9",
                        "Tractionq8r",
                        "Tractiont6",
                        "Unilateralinplanecontactq4",
                        "Unilateralinplanecontactq8",
                        "Unilateralinplanecontactq9",
                        "Unilateralinplanecontactq8r",
                        "Unilateralinplanecontactt6",
                        "Unilateralinplanecontactq4l",
                        "Unilateralinplanecontactq8l",
                        "Unilateralinplanecontactq9l",
                        "Unilateralinplanecontactq8rl",
                        "Unilateralinplanecontactt6l"                                                
                };

#ifdef DEBUG
                constexpr integer iNumElemTypes = sizeof(sType) / sizeof(sType[0]);

                ASSERT(CurrType - PRESSUREQ4 < iNumElemTypes);
#endif
                silent_cout("Reading " << sType[CurrType - PRESSUREQ4] << "(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")\n");

                if (!bUseAutoDiff()) {
                     silent_cerr("element type " << sType[CurrType - PRESSUREQ4] << " requires support for automatic differentiation at line " << HP.GetLineData()
                                 << "\nadd the statement \"use automatic differentiation;\" inside the control data section in order to enable it!\n");
                     throw ErrGeneric(MBDYN_EXCEPT_ARGS);
                }

                if (iNumTypes[Elem::SURFACE_LOAD]-- <= 0) {
                        DEBUGCERR("");
                        silent_cerr("line " << HP.GetLineData() << ": "
                                << sType[CurrType - PRESSUREQ4] << "(" << uLabel << ") "
                                "exceeds surface load elements number" << std::endl);

                        throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                }

                if (pFindElem(Elem::SURFACE_LOAD, uLabel) != nullptr) {
                        DEBUGCERR("");
                        silent_cerr("line " << HP.GetLineData() << ": "
                                << sType[CurrType - PRESSUREQ4] << "(" << uLabel << ") "
                                "already defined" << std::endl);

                        throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
                }

                switch (CurrType) {
                case PRESSUREQ4:
                     pE = ReadPressureLoad<Quadrangle4, Gauss2x2>(this, HP, uLabel);
                     break;
                case PRESSUREQ8:
                     pE = ReadPressureLoad<Quadrangle8, Gauss3x3>(this, HP, uLabel);
                     break;
                case PRESSUREQ9:
                     pE = ReadPressureLoad<Quadrangle9, Gauss3x3>(this, HP, uLabel);
                     break;
                case PRESSUREQ8R:
                     pE = ReadPressureLoad<Quadrangle8r, Gauss3x3>(this, HP, uLabel);
                     break;
                case PRESSURET6:
                     pE = ReadPressureLoad<Triangle6h, CollocTria6h>(this, HP, uLabel);
                     break;
                case TRACTIONQ4:
                     pE = ReadTractionLoad<Quadrangle4, Gauss2x2>(this, HP, uLabel);
                     break;
                case TRACTIONQ8:
                     pE = ReadTractionLoad<Quadrangle8, Gauss3x3>(this, HP, uLabel);
                     break;
                case TRACTIONQ9:
                     pE = ReadTractionLoad<Quadrangle9, Gauss3x3>(this, HP, uLabel);
                     break;
                case TRACTIONQ8R:
                     pE = ReadTractionLoad<Quadrangle8r, Gauss3x3>(this, HP, uLabel);
                     break;
                case TRACTIONT6:
                     pE = ReadTractionLoad<Triangle6h, CollocTria6h>(this, HP, uLabel);
                     break;

                case UNILATERALINPLANECONTACTQ4:
                     pE = ReadUnilateralInPlaneContact<Quadrangle4, Gauss2x2>(this, HP, uLabel);
                     break;
                case UNILATERALINPLANECONTACTQ8:
                     pE = ReadUnilateralInPlaneContact<Quadrangle8, Gauss3x3>(this, HP, uLabel);
                     break;
                case UNILATERALINPLANECONTACTQ9:
                     pE = ReadUnilateralInPlaneContact<Quadrangle9, Gauss3x3>(this, HP, uLabel);
                     break;
                case UNILATERALINPLANECONTACTQ8R:
                     pE = ReadUnilateralInPlaneContact<Quadrangle8r, Gauss3x3>(this, HP, uLabel);
                     break;
                case UNILATERALINPLANECONTACTT6:
                     pE = ReadUnilateralInPlaneContact<Triangle6h, CollocTria6h>(this, HP, uLabel);
                     break;
                case UNILATERALINPLANECONTACTQ4L:
                     pE = ReadUnilateralInPlaneContact<Quadrangle4, Gauss2x2Lumped>(this, HP, uLabel);
                     break;
                case UNILATERALINPLANECONTACTQ8L:
                     pE = ReadUnilateralInPlaneContact<Quadrangle8, Gauss3x3Lumped>(this, HP, uLabel);
                     break;
                case UNILATERALINPLANECONTACTQ9L:
                     pE = ReadUnilateralInPlaneContact<Quadrangle9, Gauss3x3Lumped>(this, HP, uLabel);
                     break;
                case UNILATERALINPLANECONTACTQ8RL:
                     pE = ReadUnilateralInPlaneContact<Quadrangle8r, Gauss3x3Lumped>(this, HP, uLabel);
                     break;
                case UNILATERALINPLANECONTACTT6L:
                     throw ErrNotImplementedYet(MBDYN_EXCEPT_ARGS);
                     break;                                          
                default:
                     ASSERT(0);
                }

                if (pE) {
                        ppE = InsertElem(ElemData[Elem::SURFACE_LOAD], uLabel, pE);
                }
        } break;
	/* Elementi aerodinamici: rotori */
	case ROTOR:
	case INDUCEDVELOCITY: {
		silent_cout("Reading InducedVelocity(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		switch (KeyWords(CurrType)) {
		case ROTOR:
			silent_cerr("InducedVelocity(" << uLabel << "): deprecated \"rotor\", use \"induced velocity\" of type \"rotor\" instead at line " << HP.GetLineData() << std::endl);
			break;

		case INDUCEDVELOCITY:
			if (HP.IsKeyWord("rotor")) {
				// continue
			} else {
				silent_cerr("InducedVelocity(" << uLabel << "): unknown \"induced velocity\" type at line " << HP.GetLineData() << " (missing \"rotor\" keyword?)" << std::endl);
				throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			break;

		default:
			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		if (iNumTypes[Elem::INDUCEDVELOCITY]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("InducedVelocity(" << uLabel << "): "
				"exceeds induced velocity elements number "
				"at line " << HP.GetLineData() << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::INDUCEDVELOCITY, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("InducedVelocity(" << uLabel << ") "
				"already defined at line "
				<< HP.GetLineData() << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		int i = ElemData[Elem::INDUCEDVELOCITY].iExpectedNum
			- iNumTypes[Elem::INDUCEDVELOCITY] - 1;
		DofOwner* pDO = DofData[DofOwner::INDUCEDVELOCITY].pFirstDofOwner + i;

		pE = ReadRotor(this, HP, pDO, uLabel);
		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::INDUCEDVELOCITY], uLabel, pE);
		}

		break;
	}

	/* Elementi aerodinamici: modale */
	case AEROMODAL: {
		silent_cout("Reading AeroModal(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		if (iNumTypes[Elem::AEROMODAL]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"AeroModal(" << uLabel << ") "
				"exceeds aeromodal elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::AEROMODAL, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"AeroModal(" << uLabel << ") "
				"already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		int i = ElemData[Elem::AEROMODAL].iExpectedNum
			- iNumTypes[Elem::AEROMODAL] - 1;
		DofOwner* pDO = DofData[DofOwner::AEROMODAL].pFirstDofOwner + i;

		pE = ReadAerodynamicModal(this, HP, pDO, uLabel);
		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::AEROMODAL], uLabel, pE);
		}

		break;
	}

	/* Elementi aerodinamici: aeromodal */
	case AERODYNAMICEXTERNAL:
	case AERODYNAMICEXTERNALMODAL: {
#ifdef USE_EXTERNAL
		silent_cout("Reading External(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		if (iNumTypes[Elem::EXTERNAL]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"External(" << uLabel << ") "
				"exceeds external elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::EXTERNAL, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"External(" << uLabel << ") "
				"already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		switch (KeyWords(CurrType)) {
		case AERODYNAMICEXTERNAL:
			pE = ReadAerodynamicExternal(this, HP, uLabel);
			break;

		case AERODYNAMICEXTERNALMODAL:
			pE = ReadAerodynamicExternalModal(this, HP, uLabel);
			break;

		default:
			ASSERTMSG(0, "You shouldn't have reached this point");
			break;
		}
		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::EXTERNAL], uLabel, pE);
		}
#else /* !USE_EXTERNAL */
		silent_cerr("You need mpi and -DUSE_AERODYNAMIC_EXTERNAL "
			<< "to use this type of elements." << std::endl);
		throw ErrGeneric(MBDYN_EXCEPT_ARGS);
#endif /* !USE_EXTERNAL */
		break;
	}

	case AERODYNAMICBODY:
	case AERODYNAMICBEAM:
	case AERODYNAMICBEAM3:	/* same as AERODYNAMICBEAM */
	case AERODYNAMICBEAM2:
	case AIRCRAFTINSTRUMENTS:
	case GENERICAERODYNAMICFORCE: {
		silent_cout("Reading AerodynamicElement(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		if (iNumTypes[Elem::AERODYNAMIC]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"AerodynamicElement(" << uLabel << ") "
				"exceeds aerodynamic elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::AERODYNAMIC, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"AerodynamicElement(" << uLabel << ") "
				"already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		int i = ElemData[Elem::AERODYNAMIC].iExpectedNum
			- iNumTypes[Elem::AERODYNAMIC] - 1;
		DofOwner* pDO = DofData[DofOwner::AERODYNAMIC].pFirstDofOwner + i;

		/* allocazione e creazione */
		switch (KeyWords(CurrType)) {
		case AERODYNAMICBODY:
			pE = ReadAerodynamicBody(this, HP, pDO, uLabel);
			break;

		case AERODYNAMICBEAM3:
		case AERODYNAMICBEAM:	/* same as BEAM3 */
			pE = ReadAerodynamicBeam(this, HP, pDO, uLabel);
			break;

		case AERODYNAMICBEAM2:
			pE = ReadAerodynamicBeam2(this, HP, pDO, uLabel);
			break;

		case AIRCRAFTINSTRUMENTS:
			pE = ReadAircraftInstruments(this, HP, pDO, uLabel);
			break;

		case GENERICAERODYNAMICFORCE:
			pE = ReadGenericAerodynamicForce(this, HP, pDO, uLabel);
			break;

		default:
			ASSERTMSG(0, "You shouldn't have reached this point");
			break;
		}
		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::AERODYNAMIC], uLabel, pE);
		}

		break;
	}

	/* genel */
	case GENEL: {
		silent_cout("Reading Genel(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		if (iNumTypes[Elem::GENEL]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"Genel(" << uLabel << ") "
				"exceeds genel elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::GENEL, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"Genel(" << uLabel << ") "
				"already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		int i = ElemData[Elem::GENEL].iExpectedNum
			- iNumTypes[Elem::GENEL] - 1;
		DofOwner* pDO = DofData[DofOwner::GENEL].pFirstDofOwner + i;

		pE = ReadGenel(this, HP, pDO, uLabel);
		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::GENEL], uLabel, pE);
		}

		break;
	}

	/* elementi idraulici */
	case HYDRAULIC: {
		silent_cout("Reading HydraulicElement(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		if (iNumTypes[Elem::HYDRAULIC]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"HydraulicElement(" << uLabel << ") "
				"exceeds hydraulic elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::HYDRAULIC, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"HydraulicElement(" << uLabel << ") "
				"already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		int i = ElemData[Elem::HYDRAULIC].iExpectedNum
			- iNumTypes[Elem::HYDRAULIC] - 1;
		DofOwner* pDO = DofData[DofOwner::HYDRAULIC].pFirstDofOwner + i;

		pE = ReadHydraulicElem(this, HP, pDO, uLabel);
		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::HYDRAULIC], uLabel, pE);
		}

		/* attenzione: gli elementi elettrici aggiungono DofOwner
		 * e quindi devono completare la relativa struttura */

		break;
	}

	/* elementi elettrici */
	case ELECTRIC: {
		silent_cout("Reading ElectricElement(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		if (iNumTypes[Elem::ELECTRIC]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"ElectricElement(" << uLabel << ") "
				"exceeds electric elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::ELECTRIC, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"ElectricElement(" << uLabel << ") "
				"already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		int i = ElemData[Elem::ELECTRIC].iExpectedNum
			- iNumTypes[Elem::ELECTRIC] - 1;
		DofOwner* pDO = DofData[DofOwner::ELECTRIC].pFirstDofOwner + i;

		pE = ReadElectric(this, HP, pDO, uLabel);
		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::ELECTRIC], uLabel, pE);
		}

		/* attenzione: gli elementi elettrici aggiungono DofOwner
		 * e quindi devono completare la relativa struttura */

		break;
	}

	/* elementi termici */
	case THERMAL: {
		silent_cout("Reading ThermalElement(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		if (iNumTypes[Elem::THERMAL]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"ThermalElement(" << uLabel << ") "
				"exceeds thermal elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::THERMAL, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"ThermalElement(" << uLabel << ") "
				"already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		int i = ElemData[Elem::THERMAL].iExpectedNum
			- iNumTypes[Elem::THERMAL] - 1;
		DofOwner* pDO = DofData[DofOwner::THERMAL].pFirstDofOwner + i;

		pE = ReadThermal(this, HP, pDO, uLabel);
		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::THERMAL], uLabel, pE);
		}

		/* attenzione: gli elementi termici aggiungono DofOwner
		 * e quindi devono completare la relativa struttura */

		break;
	}

	/* elementi bulk */
	case BULK: {
		silent_cout("Reading BulkElement(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		if (iNumTypes[Elem::BULK]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"BulkElement(" << uLabel << ") "
				"exceeds bulk elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::BULK, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"BulkElement(" << uLabel << ") "
				"already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		pE = ReadBulk(this, HP, uLabel);
		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::BULK], uLabel, pE);
		}

		break;
	}

	/* elementi loadable */
	case USER_DEFINED:
	case LOADABLE: {
		silent_cout("Reading LoadableElement(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		if (iNumTypes[Elem::LOADABLE]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"LoadableElement(" << uLabel << ") "
				"exceeds loadable elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::LOADABLE, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"LoadableElement(" << uLabel << ") "
				"already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		int i = ElemData[Elem::LOADABLE].iExpectedNum
			- iNumTypes[Elem::LOADABLE] - 1;
		DofOwner* pDO = DofData[DofOwner::LOADABLE].pFirstDofOwner + i;

		if (KeyWords(CurrType) == USER_DEFINED) {
			pE = ParseUserDefinedElem(uLabel, pDO, this, HP);
		} else {
			pE = ReadLoadable(this, HP, pDO, uLabel);
		}

		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::LOADABLE], uLabel, pE);
		}

		break;
	}

	case RTAI_OUTPUT:			// deprecated
#ifndef USE_RTAI
		silent_cout("need --with-rtai to allow RTMBDyn mailboxes; "
			"using stream output instead..." << std::endl);
#endif /* ! USE_RTAI */
		/* fall thru... */

	case OUTPUT_ELEMENT:
	case SOCKETSTREAM_OUTPUT:		// deprecated
	case SOCKETSTREAM_MOTION_OUTPUT:	// deprecated
	{
		silent_cout("Reading StreamOutElem(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		if (iNumTypes[Elem::SOCKETSTREAM_OUTPUT]-- <= 0) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"StreamOutElem(" << uLabel << ") "
				"exceeds stream output elements number" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* verifica che non sia gia' definito */
		if (pFindElem(Elem::SOCKETSTREAM_OUTPUT, uLabel) != NULL) {
			DEBUGCERR("");
			silent_cerr("line " << HP.GetLineData() << ": "
				"StreamOutElem(" << uLabel << ") "
				"already defined" << std::endl);

			throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* allocazione e creazione */
		StreamOutElem::Type eType;
		StreamContent::Type sType;
		switch (KeyWords(CurrType)) {
		case RTAI_OUTPUT:
		case SOCKETSTREAM_OUTPUT:
			pedantic_cerr("SocketStreamElem(" << uLabel << "): "
				"deprecated in favour of 'output element: <label>, socket stream, ...' "
				"at line " << HP.GetLineData() << std::endl);
			eType = StreamOutElem::SOCKETSTREAM;
			sType = StreamContent::VALUES;
			break;

		case SOCKETSTREAM_MOTION_OUTPUT:
			pedantic_cerr("SocketStreamMotionElem(" << uLabel << "): "
				"deprecated in favour of 'output element: <label>, socket stream, motion, ...' "
				"at line " << HP.GetLineData() << std::endl);
			eType = StreamOutElem::SOCKETSTREAM;
			sType = StreamContent::MOTION;
			break;

		default:
			eType = StreamOutElem::UNDEFINED;
			sType = StreamContent::UNKNOWN;
			break;
		}

		pE = ReadOutputElem(this, HP, uLabel, eType, sType);

		if (pE != 0) {
			ppE = InsertElem(ElemData[Elem::SOCKETSTREAM_OUTPUT], uLabel, pE);
		}
		break;
	}

	case INERTIA: {
		silent_cout("Reading Inertia(" << uLabel << ( sName.empty() ? "" : ( std::string(", \"") + sName + "\"" ) ) << ")" << std::endl);

		Vec3 x(Zero3);
		if (HP.IsKeyWord("position")) {
			x = HP.GetPosAbs(::AbsRefFrame);
		}

		Mat3x3 R(Eye3);
		Mat3x3 RT(Eye3);
		if (HP.IsKeyWord("orientation")) {
			R = HP.GetRotAbs(::AbsRefFrame);
			RT = R.Transpose();
		}

		std::set<const GravityOwner *> elements;
		Elem::Type Type = Elem::UNKNOWN;
		bool bOut(false);
		bool bLog(true);
		bool bAlways(false);
		while (HP.IsArg()) {
			if (HP.IsKeyWord("output")) {
				do {
					if (HP.IsKeyWord("no")) {
						bLog = false;
						bOut = false;

					} else if (HP.IsKeyWord("yes")) {
						bLog = false;
						bOut = true;

					} else if (HP.IsKeyWord("log")) {
						bLog = true;
						bOut = false;

					} else if (HP.IsKeyWord("both")) {
						bLog = true;
						bOut = true;

					} else if (HP.IsKeyWord("always")) {
						bAlways = true;

					} else {
						silent_cerr("Inertia(" << uLabel << "): "
							"unknown output mode "
							"at line " << HP.GetLineData()
							<< std::endl);
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}

				} while(HP.IsKeyWord("output"));

				break;

			} else if (HP.IsKeyWord("body")) {
				Type = Elem::BODY;

			} else if (HP.IsKeyWord("joint")) {
				Type = Elem::JOINT;

			} else if (HP.IsKeyWord("user" "defined") || HP.IsKeyWord("loadable")) {
				Type = Elem::LOADABLE;

                        } else if (HP.IsKeyWord("solid")) {
				Type = Elem::SOLID;
#if 0
			} else if (HP.IsKeyWord("...")) {
				/* other types with inertia */
#endif
			}

			if (Type == Elem::UNKNOWN) {
				if (!sName.empty()) {
					silent_cerr("Inertia(" << uLabel << ", \"" << sName << "\"): ");
				} else {
					silent_cerr("Inertia(" << uLabel << "): ");
				}
				silent_cerr("unknown or undefined element type "
					"at line " << HP.GetLineData() << std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}

			/*
			 * FIXME: duplicate check?
			 */

			if (HP.IsKeyWord("all")) {
                             for (const auto& oTmpEl: ElemData[Type].ElemContainer)
				{
					GravityOwner *pEl = dynamic_cast<GravityOwner *>(oTmpEl.second);
					if (!pEl) {
						silent_cerr(psElemNames[Type]
							<< "(" << oTmpEl.second->GetLabel() << "): "
							"not a gravity related element "
							"at line " << HP.GetLineData()
							<< "\n");
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}

					if (elements.find(pEl) != elements.end()) {
						silent_cerr(psElemNames[Type]
							<< "(" << oTmpEl.second->GetLabel() << "): "
							" duplicate label at line "
							<< HP.GetLineData() << "\n");
						throw ErrGeneric(MBDYN_EXCEPT_ARGS);
					}
					elements.insert(pEl);
				}

			} else {
				unsigned int uL = (unsigned int)HP.GetInt();
				Elem *pTmpEl = pFindElem(Type, uL);
				if (pTmpEl == 0) {
					silent_cerr("Inertia(" << uLabel << "): "
						"unable to find " << psElemNames[Type]
						<< "(" << uL << ") "
						"at line " << HP.GetLineData() << std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
				}

				GravityOwner *pEl = dynamic_cast<GravityOwner *>(pTmpEl);
				if (!pEl) {
					silent_cerr("Inertia(" << uLabel << "): "
						<< psElemNames[Type]
						<< "(" << uL << "): "
						"not a gravity related element "
						"at line " << HP.GetLineData()
						<< "\n");
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
				}

				if (elements.find(pEl) != elements.end()) {
					silent_cerr("Inertia(" << uLabel << "): "
						<< psElemNames[Type] << "(" << uL << ") "
						"is duplicate at line " << HP.GetLineData()
						<< std::endl);
					throw ErrGeneric(MBDYN_EXCEPT_ARGS);
				}

				elements.insert(pEl);
			}
		}  /* end while (HP.IsArg()) */ 

		flag fOut = fReadOutput(HP, Elem::INERTIA);
		if (bLog) {
			fOut |= Inertia::OUTPUT_LOG;
		}
		if (bOut) {
			fOut |= Inertia::OUTPUT_OUT;
		}
		if (bAlways) {
			if (iNumTypes[Elem::INERTIA]-- <= 0) {
				DEBUGCERR("");
				silent_cerr("line " << HP.GetLineData() << ": "
					"Inertia(" << uLabel << ") "
					"exceeds inertia elements number" << std::endl);
	
				throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
			}

			/* verifica che non sia gia' definito */
			if (pFindElem(Elem::INERTIA, uLabel)) {
				DEBUGCERR("");
				silent_cerr("line " << HP.GetLineData() << ": "
					"Inertia(" << uLabel << ") "
					"already defined" << std::endl);
	
				throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
			}

			fOut |= Inertia::OUTPUT_ALWAYS; // was 0x8
		}

                Inertia* pInertia = nullptr;

                SAFENEWWITHCONSTRUCTOR(pInertia, Inertia,
                                       Inertia(uLabel, sName, std::move(elements), x, R, fOut));

                if ((fOut & Inertia::OUTPUT_OUT) && (!silent_output)) {
                     pInertia->OutputLog(std::cout);
                }

                if (fOut & Inertia::OUTPUT_LOG) {
                     pInertia->OutputLog(OutHdl.Log());
                }

                if (pInertia) {
                        if (bAlways) {
                                pE = pInertia;
                                ppE = InsertElem(ElemData[Elem::INERTIA], uLabel, pE);
                        } else {
                                SAFEDELETE(pInertia);
                                pInertia = nullptr;
                        }
                }

                break;
	}

	/* In case the element type is not correct */
	default:
		silent_cerr("You shouldn't be here" << std::endl);

		throw DataManager::ErrGeneric(MBDYN_EXCEPT_ARGS);
	}

	/* Ritorna il puntatore al puntatore all'elemento appena costruito */
	return ppE;
}
