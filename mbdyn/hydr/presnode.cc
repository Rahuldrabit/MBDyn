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
 AUTHOR: Reinhard Resch <mbdyn-user@a1.net>
        Copyright (C) 2022(-2023) all rights reserved.

        The copyright of this code is transferred
        to Pierangelo Masarati and Paolo Mantegazza
        for use in the software MBDyn as described
        in the GNU Public License version 2.1
*/

#include "mbconfig.h"           /* This goes first in every *.c,*.cc file */

#include "presnode.h"

PressureNode::PressureNode(unsigned int uL, const DofOwner* pDO, doublereal dx, DofOrder::Equality eEqualityType, flag fOut) 
     :ScalarNode(uL, pDO, fOut),
      ScalarAlgebraicNode(uL, pDO, dx, eEqualityType, fOut)
{
     NO_OP;
}
   
PressureNode::~PressureNode()
{ 
     NO_OP;      
};
   
Node::Type PressureNode::GetNodeType() const
{
     return Node::HYDRAULIC;
}
   
void
PressureNode::OutputPrepare(OutputHandler &OH)
{
	if (bToBeOutput()) {
#ifdef USE_NETCDF
		if (OH.UseNetCDF(OutputHandler::PRESNODES)) {
			ASSERT(OH.IsOpen(OutputHandler::NETCDF));

			ScalarAlgebraicNode::OutputPrepare_int(OH,
				"p", OutputHandler::Dimensions::Pressure, "Pressure");
		}
#endif // USE_NETCDF
	}
}

/* returns the dimension of the component */
const OutputHandler::Dimensions PressureNode::GetEquationDimension(integer index) const
{
     OutputHandler::Dimensions dimension = OutputHandler::Dimensions::UnknownDimension;

     switch (index)
     {
     case 1:
          dimension = OutputHandler::Dimensions::MassFlow;
          break;
     }

     return dimension;
}

/* describes the dimension of components of equation */
std::ostream& PressureNode::DescribeEq(std::ostream& out, const char *prefix, bool bInitial) const
{
     integer iIndex = iGetFirstIndex();

     out << prefix << iIndex + 1 << ": "
         << "mass flow balance" << std::endl;

     return out;
}
