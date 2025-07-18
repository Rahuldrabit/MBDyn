# $Header: /var/cvs/mbdyn/mbdyn/mbdyn-1.0/tests/plates/khosravi2007/kho81/kho81.awk,v 1.7 2017/01/12 15:07:34 masarati Exp $
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

	print "# GENERATED BY 'kho81.awk' - DO NOT EDIT" >> out;
	print "" >> out;
}

BEGIN {
	E = 196.2e9;
	nu = 0.3;
	L = 0.6;
	b = 0.3;
	h = 0.001;
	Mmax = 56.445;

	if (mesh == "12x6") {
		N_L = 12;
		N_B = 6;

	} else if (mesh == "") {
		print "### need \"mesh\"" > "/dev/stderr";
		exit;

	} else {
		nf = match(mesh, "([[:digit:]]+)x([[:digit:]]+)", a);
		if (nf == 0) {
			print "### mesh " mesh " not in \"<nL>x<nb>\" format" > "/dev/stderr";
			exit;
		}

		N_L = a[1] + 0;
		N_B = a[2] + 0;

		if (N_L <= 0 || N_B <= 0) {
			printf "### invalid mesh \"%dx%d\"\n", N_L, N_B > "/dev/stderr";
			exit;
		}

		printf "### mesh \"%dx%d\" non-standard; YMMV\n", N_L, N_B > "/dev/stderr";
	}
	fname = "kho81_m";

	out = fname ".set";
	header(out);

	printf "set: const integer NNODES = %d;\n", (N_L + 1)*(N_B + 1) >> out;
	printf "set: const integer NSHELLS = %d;\n", N_L*N_B >> out;
	printf "set: const integer NJOINTS = %d;\n", N_B + 1 >> out;
	printf "set: const integer NFORCES = %d;\n", N_B + 1 >> out;
	print "" >> out;
	printf "set: const real E = %.6e;\n", E >> out;
	printf "set: const real nu = %.6e;\n", nu >> out;
	printf "set: const real L = %.6e;\n", L >> out;
	printf "set: const real b = %.6e;\n", b >> out;
	printf "set: const real h = %.6e;\n", h >> out;
	printf "set: const real Mmax = %.6e;\n", Mmax >> out;
	print "" >> out;
	printf "# %s:ft=mbd\n", "vim" >> out;

	out = fname ".nod";
	header(out);

	DELTA_N_L = 1000;
	for (nl = 0; nl <= N_L; nl++) {
		x = L*(nl/N_L);
		for (nb = 0; nb <= N_B; nb++) {
			y = b*(-.5 + nb/N_B);
			z = 0.;

			label = DELTA_N_L*nl + nb;

			printf "structural: %d, static,\n", label >> out;
			printf "\treference, 0, %+.8e, %+.8e, %+.8e,\n", x, y, z >> out;
			printf "\treference, 0, eye,\n" >> out;
			print "\treference, 0, null," >> out;
			print "\treference, 0, null;" >> out;
			print "" >> out;
		}
	}
	print "" >> out;
	printf "# %s:ft=mbd\n", "vim" >> out;

	out = fname ".elm";
	header(out);

	for (nb = 0; nb <= N_B; nb++) {
		x = 0.;
		y = b*(-.5 + nb/N_B);
		z = 0.;
		printf "joint: %d, total pin joint,\n", nb >> out;
		printf "\t%d, position, reference, node, null,\n", nb >> out;
		print "\t\tposition orientation, reference, node, eye," >> out;
		print "\t\trotation orientation, reference, node, eye," >> out;
		printf "\tposition, reference, 0, %+.8e, %+.8e, %+.8e,\n", x, y, z >> out;
		print "\tposition orientation, reference, 0, eye," >> out;
		print "\trotation orientation, reference, 0, eye," >> out;
		print "\tposition constraint, 1, 1, 1, null," >> out;
		print "\torientation constraint, 1, 1, 1, null;" >> out;
		print "" >> out;
	}
	print "" >> out;

	for (nb = 0; nb <= N_B; nb++) {
		ref = 99;
		if (nb == 0 || nb == N_B) {
			ref = 98;
		}
		printf "couple: %d, follower,\n", DELTA_N_L*N_L + nb >> out;
		printf "\t%d, position, null,\n", DELTA_N_L*N_L + nb >> out;
		printf "\t0., -1., 0., reference, %d;\n", ref >> out;
		print "" >> out;
	}

	# type = "shell4eas";
	type = "shell4easans";
	for (nl = 1; nl <= N_L; nl++) {
		for (nb = 1; nb <= N_B; nb++) {
			label1 = DELTA_N_L*nl + (nb - 1);
			label2 = DELTA_N_L*nl + nb;
			label3 = DELTA_N_L*(nl - 1) + nb;
			label4 = DELTA_N_L*(nl - 1) + (nb - 1);

			printf "%s: %d,\n", type, DELTA_N_L*nl + nb >> out;
			printf "\t%d, %d, %d, %d,\n",
				label1, label2, label3, label4 >> out;
			print "\tisotropic, E, E, nu, nu, thickness, h;" >> out;
			print "" >> out;
		}
	}
	print "" >> out;
	printf "# %s:ft=mbd\n", "vim" >> out;
}
