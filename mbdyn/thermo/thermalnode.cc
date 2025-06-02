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

#include "mynewmem.h"
#include "thermalnode.h"
#include "solman.h"


/* ThermalNode - begin */ 

/* Costruttore */
ThermalNode::ThermalNode(unsigned int uL,
	const DofOwner* pDO,
	doublereal dx,
	doublereal dxp,
	flag fOut)
     :ScalarNode(uL, pDO, fOut),
      ScalarDifferentialNode(uL, pDO, dx, dxp, fOut)
{
	NO_OP;
}

  /* Distruttore (per ora e' banale) */
ThermalNode::~ThermalNode(void)
{
	NO_OP;
}

/* Tipo di nodo */
Node::Type
ThermalNode::GetNodeType(void) const
{
	return Node::THERMAL;
}

void
ThermalNode::OutputPrepare(OutputHandler &OH)
{
	if (bToBeOutput()) {
#ifdef USE_NETCDF
		if (OH.UseNetCDF(OutputHandler::THERMALNODES)) {
			ASSERT(OH.IsOpen(OutputHandler::NETCDF));

			ScalarDifferentialNode::OutputPrepare_int(OH,
				"T", OutputHandler::Dimensions::Temperature, "Temperature",
				"TP", OutputHandler::Dimensions::TemperatureDerivative, "Temperature time derivative");
		}
#endif // USE_NETCDF
	}
}

const OutputHandler::Dimensions 
ThermalNode::GetEquationDimension(integer index) const {

		OutputHandler::Dimensions dimension = OutputHandler::Dimensions::UnknownDimension;

		switch (index)
		{
		case 1:
			dimension = OutputHandler::Dimensions::Power;
			break;
		}

		return dimension;
}

std::ostream&
ThermalNode::DescribeEq(std::ostream& out, const char *prefix, bool bInitial) const
{

	integer iIndex = iGetFirstIndex();

	out
		<< prefix << iIndex + 1 << ": "
			"heat flux balance" << std::endl;

	return out;
}

/* ThermalNode - end */
