# $Header: /var/cvs/mbdyn/mbdyn/mbdyn-1.0/tests/plates/membrane/cylinder_mxn.awk,v 1.1 2012/08/24 11:55:29 masarati Exp $
#
# MBDyn (C) is a multibody analysis code. 
# http://www.mbdyn.org
# 
# Copyright (C) 1996-2010
# 
# Pierangelo Masarati	<masarati@aero.polimi.it>
# Paolo Mantegazza	<mantegazza@aero.polimi.it>
# 
# Dipartimento di Ingegneria Aerospaziale - Politecnico di Milano
# via La Masa, 34 - 20156 Milano, Italy
# http://www.aero.polimi.it
# 
# Changing this copyright notice is forbidden.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation (version 2 of the License).
# 
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

function header(filename) {
	printf "" > filename;
	print "# MBDyn (C) is a multibody analysis code. " >> filename;
	print "# http://www.mbdyn.org" >> filename;
	print "# " >> filename;
	print "# Copyright (C) 1996-2010" >> filename;
	print "# " >> filename;
	print "# Pierangelo Masarati	<masarati@aero.polimi.it>" >> filename;
	print "# Paolo Mantegazza	<mantegazza@aero.polimi.it>" >> filename;
	print "# " >> filename;
	print "# Dipartimento di Ingegneria Aerospaziale - Politecnico di Milano" >> filename;
	print "# via La Masa, 34 - 20156 Milano, Italy" >> filename;
	print "# http://www.aero.polimi.it" >> filename;
	print "# " >> filename;
	print "# Changing this copyright notice is forbidden." >> filename;
	print "# " >> filename;
	print "# This program is free software; you can redistribute it and/or modify" >> filename;
	print "# it under the terms of the GNU General Public License as published by" >> filename;
	print "# the Free Software Foundation (version 2 of the License)." >> filename;
	print "# " >> filename;
	print "# " >> filename;
	print "# This program is distributed in the hope that it will be useful," >> filename;
	print "# but WITHOUT ANY WARRANTY; without even the implied warranty of" >> filename;
	print "# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the" >> filename;
	print "# GNU General Public License for more details." >> filename;
	print "# " >> filename;
	print "# You should have received a copy of the GNU General Public License" >> filename;
	print "# along with this program; if not, write to the Free Software" >> filename;
	print "# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA" >> filename;
	print "# " >> filename;
	print "# GENERATED BY 'cylinder_mxn.awk'; DO NOT EDIT!" >> filename;
}

BEGIN {
	PI = 3.14159265358979;
	# use four-node elements
	NSHELLS_AXIS = 3;
	NSHELLS_CIRC = 3;
	NSHELLS = NSHELLS_AXIS*NSHELLS_CIRC;

	RADIUS = 1;
	LENGTH = 1;
	METAL_THICK = 0.001;

	E = 1e-6 * 1e+09;
	RHO = 1;
	printf "RHO = %e\n", RHO >> "/dev/stdout";

	filename = "cylinder_mxn.set";
	header(filename);

	print "" >> filename;
	printf "set: const integer CYL_REF = 100;\n" >> filename;
	print "" >> filename;
	printf "set: const real RADIUS = %.8e;\n", RADIUS >> filename;
	printf "set: const real LENGTH = %.8e;\n", LENGTH >> filename;
	print "" >> filename;
	printf "set: const integer NNODES = %d;\n", (NSHELLS_AXIS + 1)*(NSHELLS_CIRC + 1) >> filename;
	printf "set: const integer NJOINTS = %d;\n", 2*(NSHELLS_AXIS + 1) + 2*(NSHELLS_CIRC - 1) >> filename;
	printf "set: const integer NBODIES = %d;\n", (NSHELLS_AXIS + 1)*(NSHELLS_CIRC + 1) >> filename;
	printf "set: const integer NSHELLS = %d;\n", NSHELLS >> filename;
	printf "set: const integer NFORCES = %d;\n", (NSHELLS_AXIS + 1)*(NSHELLS_CIRC + 1) >> filename;
	print "" >> filename;
	printf "set: const real E = %.8e;", E >> filename;
	print "set: const real NU = 0.3;" >> filename;
	printf "set: const real RHO = %.8e;", RHO >> filename;
	print "" >> filename;
	print "set: real G = E/(2*(1 + NU));" >> filename;
	print "" >> filename;
	print "set: const real CYL_DAMP = 0.;" >> filename;
	print "" >> filename;
	printf "# %s:ft=mbd\n", "vim" >> filename;

	filename = "cylinder_mxn.ref";
	header(filename);

	print "" >> filename;
	print "reference: CYL_REF," >> filename;
	print "\treference, global, null," >> filename;
	print "\treference, global, eye," >> filename;
	print "\treference, global, null," >> filename;
	print "\treference, global, null;" >> filename;
	print "" >> filename;
	printf "# %s:ft=mbd\n", "vim" >> filename;

	filename = "cylinder_mxn.nod";
	header(filename);

	print "" >> filename;
	for (c = 0; c <= NSHELLS_CIRC; c++) {
		for (a = 0; a <= NSHELLS_AXIS; a++) {
			if (c == 0 || a == 0 || c == NSHELLS_CIRC || a == NSHELLS_AXIS) {
				type = "dynamic";
			} else {
				type = "dynamic displacement";
			}
			printf "structural: %d, " type ",\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
			printf "\treference, CYL_REF, %+.8e, %+.8e, %+.8e,\n", RADIUS*(cos(c/NSHELLS_CIRC*PI/2)), RADIUS*(sin(c/NSHELLS_CIRC*PI/2)), LENGTH*a/NSHELLS_AXIS >> filename;
			if (type == "dynamic") {
				printf "\treference, CYL_REF, eye,\n" >> filename;
				printf "\treference, CYL_REF, null,\n" >> filename;
			}
			printf "\treference, CYL_REF, null;\n" >> filename;
			print "" >> filename;
		}
	}

	filename = "cylinder_mxn.elm";
	header(filename);

	# CONSTRAINTS
	print "" >> filename;
#	c = 0;
#	for (a = 0; a <= NSHELLS_AXIS; a++) {
#		printf "joint: %d, clamp,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
#		printf "\t%d,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
#		print "\tnode, node;" >> filename;
#		print "" >> filename;
#	}
#	c = NSHELLS_CIRC;
#	for (a = 0; a <= NSHELLS_AXIS; a++) {
#		printf "joint: %d, clamp,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
#		printf "\t%d,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
#		print "\tnode, node;" >> filename;
#		print "" >> filename;
#	}
	c = 0;
	for (a = 1; a < NSHELLS_AXIS; a++) {
		printf "joint: %d, total pin joint,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
		printf "\t%d,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
		print "\t\tposition, reference, CYL_REF, null," >> filename;
		print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
		print "\t\tposition, reference, CYL_REF, null," >> filename;
		print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
		print "\tposition constraint, inactive, active, inactive," >> filename;
		print "\t\tnull," >> filename;
		print "\torientation constraint, active, inactive, active," >> filename;
		print "\t\tnull;" >> filename;
		print "" >> filename;
	}
	c = NSHELLS_CIRC;
	for (a = 1; a < NSHELLS_AXIS; a++) {
		printf "joint: %d, total pin joint,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
		printf "\t%d,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
		print "\t\tposition, reference, CYL_REF, null," >> filename;
		print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
		print "\t\tposition, reference, CYL_REF, null," >> filename;
		print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
		print "\tposition constraint, active, inactive, inactive," >> filename;
		print "\t\tnull," >> filename;
		print "\torientation constraint, inactive, active, active," >> filename;
		print "\t\tnull;" >> filename;
		print "" >> filename;
	}
	a = 0;
	for (c = 1; c < NSHELLS_CIRC; c++) {
		printf "joint: %d, total pin joint,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
		printf "\t%d,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
		print "\t\tposition, reference, CYL_REF, null," >> filename;
		print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
		print "\t\tposition, reference, CYL_REF, null," >> filename;
		print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
		print "\tposition constraint, inactive, inactive, active," >> filename;
		print "\t\tnull," >> filename;
		print "\torientation constraint, active, active, inactive," >> filename;
		print "\t\tnull;" >> filename;
		print "" >> filename;		
	}
	a = NSHELLS_AXIS;
	for (c = 1; c < NSHELLS_CIRC; c++) {
		printf "joint: %d, total pin joint,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
		printf "\t%d,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
		print "\t\tposition, reference, CYL_REF, null," >> filename;
		print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
		print "\t\tposition, reference, CYL_REF, null," >> filename;
		print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
		print "\tposition constraint, inactive, inactive, active," >> filename;
		print "\t\tnull," >> filename;
		print "\torientation constraint, active, active, inactive," >> filename;
		print "\t\tnull;" >> filename;
		print "" >> filename;		
	}

	# 1
	c = 0;
	a = 0;
	printf "joint: %d, total pin joint,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
	printf "\t%d,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
	print "\t\tposition, reference, CYL_REF, null," >> filename;
	print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
	print "\t\tposition, reference, CYL_REF, null," >> filename;
	print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
	print "\tposition constraint, inactive, active, active," >> filename;
	print "\t\tnull," >> filename;
	print "\torientation constraint, active, active, active," >> filename;
	print "\t\tnull;" >> filename;
	print "" >> filename;		

	# 3
	c = 0;
	a = NSHELLS_AXIS;
	printf "joint: %d, total pin joint,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
	printf "\t%d,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
	print "\t\tposition, reference, CYL_REF, null," >> filename;
	print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
	print "\t\tposition, reference, CYL_REF, null," >> filename;
	print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
	print "\tposition constraint, inactive, active, active," >> filename;
	print "\t\tnull," >> filename;
	print "\torientation constraint, active, active, active," >> filename;
	print "\t\tnull;" >> filename;
	print "" >> filename;

	# 10
	c = NSHELLS_CIRC;
	a = 0;
	printf "joint: %d, total pin joint,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
	printf "\t%d,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
	print "\t\tposition, reference, CYL_REF, null," >> filename;
	print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
	print "\t\tposition, reference, CYL_REF, null," >> filename;
	print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
	print "\tposition constraint, active, inactive, active," >> filename;
	print "\t\tnull," >> filename;
	print "\torientation constraint, active, active, active," >> filename;
	print "\t\tnull;" >> filename;
	print "" >> filename;		

	# 12
	c = NSHELLS_CIRC;
	a = NSHELLS_AXIS;
	printf "joint: %d, total pin joint,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
	printf "\t%d,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
	print "\t\tposition, reference, CYL_REF, null," >> filename;
	print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
	print "\t\tposition, reference, CYL_REF, null," >> filename;
	print "\t\tposition orientation, reference, CYL_REF, eye," >> filename;
	print "\tposition constraint, active, inactive, active," >> filename;
	print "\t\tnull," >> filename;
	print "\torientation constraint, active, active, active," >> filename;
	print "\t\tnull;" >> filename;
	print "" >> filename;

	# FORCES
	PRESSURE = 1e00 * 1e+06; # Pa
	printf "PRESSURE = %e\n", PRESSURE >> "/dev/stdout";
	D_CIRC = PI * RADIUS / 2 / NSHELLS_CIRC;
	D_AXIS = LENGTH / NSHELLS_AXIS;
	AREA_SHELL = D_CIRC * D_AXIS;
	FORCE = PRESSURE * AREA_SHELL;
	print "" >> filename;
	for (c = 0; c <= NSHELLS_CIRC; c++) {
	# for (c = 1; c < NSHELLS_CIRC; c++) {
		if (c == 0 || c == NSHELLS_CIRC) {
			AMPL_TMP = FORCE/2;
		} else {
			AMPL_TMP = FORCE;
		}
		for (a = 0; a <= NSHELLS_AXIS; a++) {
			if (a == 0 || a == NSHELLS_AXIS) {
				AMPL = AMPL_TMP/2;
			} else {
				AMPL = AMPL_TMP;
			}
			printf "force: %d, absolute displacement,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
			printf "\t%d,\n", c*(NSHELLS_AXIS + 1) + (a + 1) >> filename;
			# printf "\tposition, null,\n" >> filename;
			printf "\t%.8e, %.8e, %.8e,\n", cos(c/NSHELLS_CIRC*PI/2), sin(c/NSHELLS_CIRC*PI/2), 0. >> filename;
			printf "\t\tcosine, 0., 2*pi/1, %.8e, half, 0.;", AMPL/2 >> filename;
			print "" >> filename;
		}
	}

	# BODIES
	NODE_VOL = AREA_SHELL * METAL_THICK;
	for (c = 0; c <= NSHELLS_CIRC; c++) {
		if (c == 0 || c == NSHELLS_CIRC) {
			TMP_VOL = NODE_VOL/2;
		} else {
			TMP_VOL = NODE_VOL;
		}
		for (a = 0; a <= NSHELLS_AXIS; a++) {
			LABEL = c*(NSHELLS_AXIS + 1) + (a + 1);
			printf "body: %d, %d,\n", LABEL, LABEL >> filename;
			# printf "\tcondense, 2,\n" >> filename;
			if (a == 0 || a == NSHELLS_AXIS) {
				VOL = TMP_VOL/2;
			} else {
				VOL = TMP_VOL;
			}
			printf "\t%.8e*RHO\n", VOL >> filename;
			if (c == 0 || a == 0 || c == NSHELLS_CIRC || a == NSHELLS_AXIS) {
				print ",\n" >> filename;
				printf "\treference, node, 0., 0., 0.," >> filename;
				printf "\tdiag, 1.e-9, 1.e-9, 1.e-9;\n" >> filename;
			} else {
				print ";\n" >> filename;
			}

		}
		print "" >> filename;
	}


	print "inertia: 0, body, all;	# defines which body outputs inertia params" >> filename;
	print "" >> filename;

	# shell_type = "shell4eas";
	# shell_type = "shell4easans";
	shell_type = "membrane4eas";

	for (c = 1; c <= NSHELLS_CIRC; c++) {
		for (a = 1; a <= NSHELLS_AXIS; a++) {
			printf "%s: %d,\n", shell_type, (c - 1)*NSHELLS_AXIS + a >> filename;
			printf "\t%d, %d, %d, %d,\n",
				c*(NSHELLS_AXIS + 1) + a + 1,
				(c - 1)*(NSHELLS_AXIS + 1) + a + 1,
				(c - 1)*(NSHELLS_AXIS + 1) + a,
				c*(NSHELLS_AXIS + 1) + a >> filename;
			printf "\tisotropic, E, E, nu, NU, thickness, %.16e;\n", METAL_THICK >> filename;
			print "" >> filename;
		}
	}

	printf "# %s:ft=mbd\n", "vim" >> filename;

}
