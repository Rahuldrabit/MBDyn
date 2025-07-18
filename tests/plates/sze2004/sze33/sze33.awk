# $Header: /var/cvs/mbdyn/mbdyn/mbdyn-1.0/tests/plates/sze2004/sze33/sze33.awk,v 1.11 2017/01/12 15:08:25 masarati Exp $
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
# 3.3 Slit annular plate subjected to lifting line force

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
	print "# 3.3 Slit annular plate subjected to lifting line force" >> out;
	print "#" >> out;
	print "# GENERATED BY 'sze33.awk' - DO NOT EDIT" >> out;
	print "" >> out;
}

BEGIN {
	E = 21.e6;
	nu = 0.;
	Ri = 6.;
	Ro = 10.;
	h = 0.03;
	Pmax = 0.8;

	# mesh = "6x30";
	# mesh = "10x80";

	if (mesh == "6x30") {
		N_R = 6;
		N_C = 30;

	} else if (mesh == "10x80") {
		N_R = 10;
		N_C = 80;

	} else if (mesh == "") {
		print "### need \"mesh\"" > "/dev/stderr";
		exit;

	} else {
		nf = match(mesh, "([[:digit:]]+)x([[:digit:]]+)", a);
		if (nf == 0) {
			print "### mesh " mesh " not in \"<nradial>x<ntangential>\" format" > "/dev/stderr";
			exit;
		}

		N_R = a[1] + 0;
		N_C = a[2] + 0;

		if (N_R <= 0 || N_C <= 0) {
			printf "### invalid mesh \"%dx%d\"\n", N_R, N_C > "/dev/stderr";
			exit;
		}

		printf "### mesh \"%dx%d\" non-standard; YMMV\n", N_R, N_C > "/dev/stderr";
	}
	fname = "sze33_m";

	out = fname ".set";
	header(out);

	printf "set: const integer NNODES = %d;\n", (N_R + 1)*(N_C + 1) >> out;
	printf "set: const integer NSHELLS = %d;\n", N_R*N_C >> out;
	printf "set: const integer NJOINTS = %d;\n", N_R + 1 >> out;
	printf "set: const integer NFORCES = %d;\n", N_R + 1 >> out;
	print "" >> out;
	printf "set: const real E = %.6e;\n", E >> out;
	printf "set: const real nu = %.6e;\n", nu >> out;
	printf "set: const real Ri = %.6e;\n", Ri >> out;
	printf "set: const real Ro = %.6e;\n", Ro >> out;
	printf "set: const real h = %.6e;\n", h >> out;
	printf "set: const real Pmax = %.6e;\n", Pmax >> out;
	print "" >> out;
	printf "# %s:ft=mbd\n", "vim" >> out;

	out = fname ".nod";
	header(out);

	DELTA_N_R = 1000;
	POINT_B = 999999;
	for (r = 0; r <= N_R; r++) {
		rho = Ri + r/N_R*(Ro - Ri);
		for (c = 0; c <= N_C; c++) {
			theta = 2*3.14159265358979323846*c/N_C;

			x = rho*cos(theta);
			y = rho*sin(theta);

			label = DELTA_N_R*r + c;
			if (c == 0 && r == N_R) {
				label = POINT_B;
			}

			printf "structural: %d, static,\n", label >> out;
			printf "\treference, 0, %+.8e, %+.8e, %+.8e,\n", x, y, 0. >> out;
			printf "\treference, 0,\n" >> out;
			printf "\t\t3, 0., 0., 1.,\n" >> out;
			printf "\t\t1, %+.8e, %+.8e, %+.8e,\n", x, y, 0. >> out;
			print "\treference, 0, null," >> out;
			print "\treference, 0, null;" >> out;
			print "" >> out;
		}
	}
	print "" >> out;
	printf "# %s:ft=mbd\n", "vim" >> out;

	out = fname ".elm";
	header(out);

	for (r = 0; r <= N_R; r++) {
		printf "joint: %d, clamp, %d, node, node;\n", DELTA_N_R*r + N_C, DELTA_N_R*r + N_C >> out;
	}
	print "" >> out;

	for (r = 0; r <= N_R; r++) {
		s = 1;
		if (r == 0 || r == N_R) {
			s = 2;
		}

		label = DELTA_N_R*r;
		if (r == N_R) {
			label = POINT_B;
		}

		printf "force: %d, absolute,\n", DELTA_N_R*r >> out;
		printf "\t%d, position, null,\n",  label >> out;
		print "\t0., 0., 1.," >> out;
		printf "\t\tcosine, 0., pi/1., (Pmax*(Ro - Ri)/%d)/2, half, 0.;\n", N_R*s >> out;
	}

	for (r = 1; r <= N_R; r++) {
		for (c = 1; c <= N_C; c++) {
			label = DELTA_N_R*r + (c - 1);
			if (r == N_R && c == 1) {
				label = POINT_B;
			}
			printf "shell4eas: %d,\n", DELTA_N_R*r + c >> out;
			printf "\t%d, %d, %d, %d,\n",
				label,
				DELTA_N_R*r + c,
				DELTA_N_R*(r - 1) + c,
				DELTA_N_R*(r - 1) + (c - 1) >> out;
			print "\tisotropic, E, E, nu, nu, thickness, h;" >> out;
			print "" >> out;
		}
	}
	print "" >> out;
	printf "# %s:ft=mbd\n", "vim" >> out;
}
