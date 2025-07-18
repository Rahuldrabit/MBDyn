# $Header: /var/cvs/mbdyn/mbdyn/mbdyn-1.0/tests/plates/sze2004/sze35/sze35.awk,v 1.11 2017/01/12 15:08:35 masarati Exp $
#
# MBDyn (C) is a multibody analysis code. 
# http://www.mbdyn.org
# 
# Copyright (C) 1996-2017
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

# Benchmarks from K.Y. Sze, X.H. Liu, S.H. Lo,
# "Popular benchmark problems for geometric nonlinear analysis of shells",
# Finite Elements in Analysis and Design 40 (2004) 1551­1569
# doi:10.1016/j.finel.2003.11.001
# 
# Author: Pierangelo Masarati <masarati@aero.polimi.it> (except when noted)
# 
# 3.5 Pullout of an open-ended cylindrical shell

function header(out) {
	printf "" > out;
	print "# MBDyn (C) is a multibody analysis code. " >> out;
	print "# http://www.mbdyn.org" >> out;
	print "# " >> out;
	print "# Copyright (C) 1996-2017" >> out;
	print "# " >> out;
	print "# Pierangelo Masarati	<masarati@aero.polimi.it>" >> out;
	print "# Paolo Mantegazza	<mantegazza@aero.polimi.it>" >> out;
	print "# " >> out;
	print "# Dipartimento di Ingegneria Aerospaziale - Politecnico di Milano" >> out;
	print "# via La Masa, 34 - 20156 Milano, Italy" >> out;
	print "# http://www.aero.polimi.it" >> out;
	print "# " >> out;
	print "# Changing this copyright notice is forbidden." >> out;
	print "# " >> out;
	print "# This program is free software; you can redistribute it and/or modify" >> out;
	print "# it under the terms of the GNU General Public License as published by" >> out;
	print "# the Free Software Foundation (version 2 of the License)." >> out;
	print "# " >> out;
	print "# " >> out;
	print "# This program is distributed in the hope that it will be useful," >> out;
	print "# but WITHOUT ANY WARRANTY; without even the implied warranty of" >> out;
	print "# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the" >> out;
	print "# GNU General Public License for more details." >> out;
	print "# " >> out;
	print "# You should have received a copy of the GNU General Public License" >> out;
	print "# along with this program; if not, write to the Free Software" >> out;
	print "# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA" >> out;
	print "# " >> out;

	print "# Benchmarks from K.Y. Sze, X.H. Liu, S.H. Lo," >> out;
	print "# \"Popular benchmark problems for geometric nonlinear analysis of shells\"," >> out;
	print "# Finite Elements in Analysis and Design 40 (2004) 1551­1569" >> out;
	print "# doi:10.1016/j.finel.2003.11.001" >> out;
	print "# " >> out;
	print "# Author: Pierangelo Masarati <masarati@aero.polimi.it> (except when noted)" >> out;
	print "# " >> out;
	print "# 3.5 Pullout of an open-ended cylindrical shell" >> out;
	print "#" >> out;
	print "# GENERATED BY 'sze35.awk' - DO NOT EDIT" >> out;
	print "" >> out;
}

BEGIN {
	E = 10.5e6;
	nu = 0.3125;
	R = 4.953;
	L = 10.35;
	h = 0.094;
	Pmax = 40000.;

	# mesh = "16x24";
	# mesh = "24x36";

	if (mesh == "16x24") {
		N_Y = 16;
		N_C = 24;

	} else if (mesh == "24x36") {
		N_Y = 24;
		N_C = 36;

	} else if (mesh == "") {
		print "### need \"mesh\"" > "/dev/stderr";
		exit;

	} else {
		nf = match(mesh, "([[:digit:]]+)x([[:digit:]]+)", a);
		if (nf == 0) {
			print "### mesh " mesh " not in \"<nradial>x<ntangential>\" format" > "/dev/stderr";
			exit;
		}

		N_Y = a[1] + 0;
		N_C = a[2] + 0;

		if (N_Y <= 0 || N_C <= 0) {
			printf "### invalid mesh \"%dx%d\"\n", N_Y, N_C > "/dev/stderr";
			exit;
		}

		printf "### mesh \"%dx%d\" non-standard; YMMV\n", N_Y, N_C > "/dev/stderr";
	}
	fname = "sze35_m";

	out = fname ".set";
	header(out);

	printf "set: const integer NNODES = %d;\n", (N_Y + 1)*(N_C + 1) >> out;
	printf "set: const integer NSHELLS = %d;\n", N_Y*N_C >> out;
	printf "set: const integer NJOINTS = %d;\n", 2*N_Y + N_C + 1 >> out;
	printf "set: const integer NFORCES = %d;\n", 1 >> out;
	print "" >> out;
	printf "set: const real E = %.6e;\n", E >> out;
	printf "set: const real nu = %.6e;\n", nu >> out;
	printf "set: const real R = %.6e;\n", R >> out;
	printf "set: const real L = %.6e;\n", L >> out;
	printf "set: const real h = %.6e;\n", h >> out;
	printf "set: const real Pmax = %.6e;\n", Pmax >> out;
	print "" >> out;
	printf "# %s:ft=mbd\n", "vim" >> out;

	out = fname ".nod";
	header(out);

	DELTA_N_Y = 1000;
	POINT_A = 999998;
	POINT_B = 999999;
	POINT_C = 0;
	for (ny = 0; ny <= N_Y; ny++) {
		y = (L/2)*(ny/N_Y);
		for (nc = 0; nc <= N_C; nc++) {
			theta = 3.14159265358979323846/2*nc/N_C;

			x = R*cos(theta);
			z = R*sin(theta);

			label = DELTA_N_Y*ny + nc;
			if (ny == N_Y) {
				if (nc == 0) {
					label = POINT_B;
				} else if (nc == N_C) {
					label = POINT_A;
				}
			}

			printf "structural: %d, static,\n", label >> out;
			printf "\treference, 0, %+.8e, %+.8e, %+.8e,\n", x, y, z >> out;
			printf "\treference, 0,\n" >> out;
			printf "\t\t2, 0., 1., 0.,\n" >> out;
			printf "\t\t1, %+.8e, %+.8e, %+.8e,\n", x, 0., z >> out;
			print "\treference, 0, null," >> out;
			print "\treference, 0, null;" >> out;
			print "" >> out;
		}
	}
	print "" >> out;
	printf "# %s:ft=mbd\n", "vim" >> out;

	out = fname ".elm";
	header(out);

	print "# symmetry constraints along C-B" >> out;
	for (ny = 0; ny < N_Y; ny++) {
		y = (L/2)*(ny/N_Y);

		x = R;
		z = 0.;
		printf "joint: %d, total pin joint,\n", DELTA_N_Y*ny >> out;
		printf "\t%d, position, reference, node, null,\n", DELTA_N_Y*ny >> out;
		printf "\tposition, reference, 0, %+.8e, %+.8e, %+.8e,\n", x, y, z >> out;
		print "\tposition constraint, 0, 0, 1, null," >> out;
		print "\torientation constraint, 1, 1, 0, null;" >> out;
		print "" >> out;

		x = 0.;
		z = R;
		printf "joint: %d, total pin joint,\n", DELTA_N_Y*ny + N_C >> out;
		printf "\t%d, position, reference, node, null,\n", DELTA_N_Y*ny + N_C >> out;
		print "\t\tposition orientation, reference, 0, eye," >> out;
		print "\t\trotation orientation, reference, 0, eye," >> out;
		printf "\tposition, reference, 0, %+.8e, %+.8e, %+.8e,\n", x, y, z >> out;
		print "\tposition constraint, 1, 0, 0, null," >> out;
		print "\torientation constraint, 0, 1, 1, null;" >> out;
		print "" >> out;
	}
	print "" >> out;

	print "# symmetry constraints in A & B" >> out;
	printf "joint: %d, total pin joint,\n", DELTA_N_Y*N_Y >> out;
	printf "\t%d, position, reference, node, null,\n", POINT_B >> out;
	printf "\tposition, reference, 0, %+.8e, %+.8e, %+.8e,\n", R, L/2, 0. >> out;
	print "\tposition constraint, 0, 1, 1, null," >> out;
	print "\torientation constraint, 1, 1, 1, null;" >> out;
	print "" >> out;
	print "" >> out;

	printf "joint: %d, total pin joint,\n", POINT_A >> out;
	printf "\t%d, position, reference, node, null,\n", POINT_A >> out;
	print "\t\tposition orientation, reference, 0, eye," >> out;
	print "\t\trotation orientation, reference, 0, eye," >> out;
	printf "\tposition, reference, 0, %+.8e, %+.8e, %+.8e,\n", 0., L/2, R >> out;
	print "\tposition constraint, 1, 1, 0, null," >> out;
	print "\t# position constraint, 1, 1, 1, 0., 0., 1., cosine, 0., pi/1., 3./2, half, 0.," >> out;
	print "\torientation constraint, 1, 1, 1, null;" >> out;
	print "" >> out;
	print "" >> out;

	print "# symmetry constraints along B-A" >> out;
	for (nc = 1; nc < N_C; nc++) {
		theta = 3.14159265358979323846/2*nc/N_C;

		x = R*cos(theta);
		y = L/2;
		z = R*sin(theta);
		printf "joint: %d, total pin joint,\n", DELTA_N_Y*N_Y + nc >> out;
		printf "\t%d, position, reference, node, null,\n", DELTA_N_Y*N_Y + nc >> out;
		print "\t\tposition orientation, reference, node, eye," >> out;
		print "\t\trotation orientation, reference, node, eye," >> out;
		printf "\tposition, reference, 0, %+.8e, %+.8e, %+.8e,\n", x, y, z >> out;
		print "\tposition orientation, reference, 0," >> out;
		print "\t\t2, 0., 1., 0.," >> out;
		printf "\t\t1, %+.8e, %+.8e, %+.8e,\n", x, 0., z >> out;
		print "\trotation orientation, reference, 0," >> out;
		print "\t\t2, 0., 1., 0.," >> out;
		printf "\t\t1, %+.8e, %+.8e, %+.8e,\n", x, 0., z >> out;
		print "\tposition constraint, 0, 1, 0, null," >> out;
		print "\torientation constraint, 1, 0, 1, null;" >> out;
		print "" >> out;
	}
	print "" >> out;

	printf "force: %d, absolute,\n", POINT_A >> out;
	printf "\t%d, position, null,\n", POINT_A >> out;
	print "\t0., 0., 1., reference, 999998;" >> out;
	print "" >> out;

	# type = "shell4eas";
	type = "shell4easans";

	for (ny = 1; ny <= N_Y; ny++) {
		for (nc = 1; nc <= N_C; nc++) {
			label1 = DELTA_N_Y*ny + (nc - 1);
			label2 = DELTA_N_Y*ny + nc;
			label3 = DELTA_N_Y*(ny - 1) + nc;
			label4 = DELTA_N_Y*(ny - 1) + (nc - 1);
			if (ny == N_Y) {
				if (nc == 1) {
					label1 = POINT_B;
				} else if (nc == N_C) {
					label2 = POINT_A;
				}
			}

			printf "%s: %d,\n", type, DELTA_N_Y*ny + nc >> out;
			printf "\t%d, %d, %d, %d,\n",
				label1, label2, label3, label4 >> out;
			print "\tisotropic, E, E, nu, nu, thickness, h;" >> out;
			print "" >> out;
		}
	}
	print "" >> out;
	printf "# %s:ft=mbd\n", "vim" >> out;
}
