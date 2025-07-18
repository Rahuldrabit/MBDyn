#! /bin/sh
# $Header: /var/cvs/mbdyn/mbdyn/mbdyn-1.0/tests/scripts/abs2rel/rotor.sh,v 1.7 2017/01/12 15:09:16 masarati Exp $
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
# 

# NOTE: abs2rel.awk must be in AWKPATH

FILE=rotor

awk -f abs2rel.awk \
	-v InputMode=matrix \
	-v OutputMode=vector \
	-v RefNode=1 \
	-v NumBlades=4 \
	-v BladeLabelOff=10 \
	-v RotorAxis=3 \
	"$FILE.mov" > "$FILE.mor"

