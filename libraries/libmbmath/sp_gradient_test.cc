/*
 * MBDyn (C) is a multibody analysis code.
 * http://www.mbdyn.org
 *
 * Copyright (C) 1996-2023
 *
 * Pierangelo Masarati  <pierangelo.masarati@polimi.it>
 * Paolo Mantegazza     <paolo.mantegazza@polimi.it>
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
  Copyright (C) 2020(-2023) all rights reserved.

  The copyright of this code is transferred
  to Pierangelo Masarati and Paolo Mantegazza
  for use in the software MBDyn as described
  in the GNU Public License version 2.1
*/

#include "mbconfig.h"

#ifdef HAVE_FEENABLEEXCEPT
#define _GNU_SOURCE 1
#include <fenv.h>
#endif // HAVE_FENV_H

#if _POSIX_C_SOURCE >= 200809L
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "myassert.h"
#include "mynewmem.h"
#include "submat.h"
#include "matvec3.h"
#include "matvec6.h"
#include "spmh.h"
#include "ccmh.h"
#include "sp_gradient.h"
#include "sp_matrix_base.h"
#include "sp_gradient_op.h"
#include "sp_matvecass.h"
#include "sp_gradient_spmh.h"
#include "restart_data.h"

#ifdef USE_TRILINOS


#undef HAVE_BLAS
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#include "epetravh.h"
#include "epetraspmh.h"
#include <Epetra_SerialComm.h>
#pragma GCC diagnostic pop
#endif

#include "sp_gradient_test_func.h"

namespace sp_grad_test {
     inline doublereal* front(std::vector<doublereal>& v) {
          return v.size() ? &v.front() : nullptr;
     }

     inline const doublereal* front(const std::vector<doublereal>& v) {
          return v.size() ? &v.front() : nullptr;
     }

     template <typename RAND_NZ_T, typename RAND_DOF_T, typename RAND_VAL_T, typename GEN_T>
     index_type sp_grad_rand_gen(SpGradient& u, RAND_NZ_T& randnz, RAND_DOF_T& randdof, RAND_VAL_T& randval, GEN_T& gen) {
          std::vector<SpDerivRec> ud;

          index_type N = randnz(gen);

          ud.reserve(N > 0 ? N : 0);

          for (index_type i = 0; i < N; ++i) {
               auto d = randval(gen);

               ud.emplace_back(randdof(gen), d);
          }

          doublereal v = randval(gen);

#ifdef SP_GRAD_RANDGEN_NO_ZEROS
          if (v == 0.) {
               v = sqrt(std::numeric_limits<doublereal>::epsilon());
          }
#endif

          u.Reset(v, ud);

          u.Sort();

          return u.iGetSize();
     }

     template <typename RAND_NZ_T, typename RAND_DOF_T, typename RAND_VAL_T, typename GEN_T>
     index_type sp_grad_rand_gen(GpGradProd& u, RAND_NZ_T& randnz, RAND_DOF_T& randdof, RAND_VAL_T& randval, GEN_T& gen) {
          doublereal d = randval(gen);
          doublereal v = randval(gen);

          u.Reset(v, d);

          return 1;
     }

     template <typename RAND_NZ_T, typename RAND_DOF_T, typename RAND_VAL_T, typename GEN_T>
     index_type sp_grad_rand_gen(doublereal& u, RAND_NZ_T& randnz, RAND_DOF_T& randdof, RAND_VAL_T& randval, GEN_T& gen) {
          u = randval(gen);
          return 0;
     }

     void sp_grad_assert_equal(doublereal u, doublereal v, doublereal dTol) {
          MBDYN_TESTSUITE_ASSERT(fabs(u - v) / std::max(1., fabs(u) + fabs(v)) <= dTol);
     }

     void sp_grad_assert_equal(const SpGradient& u, const SpGradient& v, doublereal dTol) {
          SpGradDofStat s;

          u.GetDofStat(s);
          v.GetDofStat(s);

          sp_grad_assert_equal(u.dGetValue(), v.dGetValue(), dTol);

          for (index_type i = s.iMinDof; i <= s.iMaxDof; ++i) {
               sp_grad_assert_equal(u.dGetDeriv(i), v.dGetDeriv(i), dTol);
          }
     }

     void testx() {
          SpMatrix<doublereal> zero(0, 0, 0);
          SpMatrix<doublereal> A(3, 3, 0);
          SpMatrix<doublereal, 3, 3> B(3, 3, 0);

          A.ResizeReset(2, 2, 0);

          for (index_type i = 1; i <= A.iGetNumRows(); ++i) {
               for (index_type j = 1; j <= A.iGetNumCols(); ++j) {
                    A(i, j) = 10. * i + j;
               }
          }

          B.ResizeReset(3, 3, 0);

          for (index_type i = 1; i <= B.iGetNumRows(); ++i) {
               for (index_type j = 1; j <= B.iGetNumCols(); ++j) {
                    B(i, j) = 10. * i + j;
               }
          }

          std::cout << "A=" << A << "\nB=" << B << "\n";
     }

     void test0(index_type inumloops, index_type inumnz, index_type inumdof) {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-1., 1.);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          SpGradExpDofMapHelper<SpGradient> oDofMap;

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               const doublereal dTol = std::pow(std::numeric_limits<doublereal>::epsilon(), 0.9);

               Mat3x3 A, C;
               Mat3xN A3n(3), C3n(3);
               MatNx3 An3(3), Cn3(3);
               MatNxN Ann(3, 3), Cnn(3, 3);
               Vec3 g;
               Vec3 b;
               Vec6 gb6;
               doublereal d;
               SpMatrixA<doublereal, 3, 3> A1, C1;
               SpMatrix<doublereal, 2, 2> E2x2(2, 2, 0);
               SpMatrix<SpGradient, 3, 3> X1(3, 3, 0);
               SpMatrix<doublereal, 3, 3> X2(3, 3, 0);
               SpMatrix<SpGradient, 3, 3> X3a(3, 3, 0), X4a(3, 3, 0), X5a(3, 3, 0);
               SpMatrix<SpGradient, 3, 3> X3b(3, 3, 0), X4b(3, 3, 0), X5b(3, 3, 0);
               SpMatrixA<SpGradient, 3, 3, 10> X6a, X6b;
               SpMatrixA<SpGradient, 3, 3> X7a(10), X7b(0);
               SpColVectorA<SpGradient, 3, 10> X8a, X8b;
               SpRowVectorA<SpGradient, 3, 10> X9a, X9b;
               SpMatrix<SpGradient> X10a, X10b, X11a(3, 3, 10), X11b(3, 3, 10);
               SpMatrixA<SpGradient, 2, 3> X12a, X12b, X12c;
               SpMatrixA<SpGradient, 3, 2> X13a, X12d;
               SpRowVectorA<SpGradient, 3> X14a, X14b;
               SpMatrixA<SpGradient, 3, 3> X15a, X15b;
               SpMatrix<doublereal> A6x7(6, 7, 0);
               SpMatrix<SpGradient> A6x7g(6, 7, 0);
               SpMatrix<doublereal, 6, 7> A6x7f(6, 7, 0);
               SpMatrix<SpGradient, 6, 7> A6x7gf(6, 7, 0);
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, 7> A6x7f2(6, 7, 0);
               SpMatrix<SpGradient, SpMatrixSize::DYNAMIC, 7> A6x7gf2(6, 7, 0);

               for (index_type j = 1; j <= A6x7.iGetNumCols(); ++j) {
                    for (index_type i = 1; i <= A6x7.iGetNumRows(); ++i) {
                         sp_grad_rand_gen(A6x7(i, j), randnz, randdof, randval, gen);
                         sp_grad_rand_gen(A6x7g(i, j), randnz, randdof, randval, gen);
                         A6x7f(i, j) = A6x7(i, j);
                         A6x7gf(i, j) = A6x7g(i, j);
                         A6x7f2(i, j) = A6x7(i, j);
                         A6x7gf2(i, j) = A6x7g(i, j);
                    }
               }

               SpMatrix<doublereal> A2x3 = Transpose(SubMatrix(Transpose(A6x7), 3, 2, 3, 2, 4, 2));
               SpMatrix<SpGradient> A2x3g = Transpose(SubMatrix(Transpose(A6x7g), 3, 2, 3, 2, 4, 2));

               oDofMap.ResetDofStat();
               oDofMap.GetDofStat(A6x7g);
               oDofMap.Reset();
               oDofMap.InsertDof(A6x7g);
               oDofMap.InsertDone();

               SpMatrix<SpGradient> A2x3gMap(Transpose(SubMatrix(Transpose(A6x7g), 3, 2, 3, 2, 4, 2)), oDofMap);
               SpMatrix<SpGradient> B2x3g = A2x3g * 3. - A2x3g * 0.5;
               SpMatrix<SpGradient> B2x3gMap(A2x3g * 3. - A2x3g * 0.5, oDofMap);
               SpMatrix<doublereal, 2, 3> A2x3f = Transpose(SubMatrix<3, 2, 3, 2, 4, 2>(Transpose(A6x7f)));
               SpMatrix<SpGradient, 2, 3> A2x3gf = Transpose(SubMatrix<3, 2, 3, 2, 4, 2>(Transpose(A6x7gf)));
               SpMatrix<SpGradient, 2, 3> A2x3gfMap(Transpose(SubMatrix<3, 2, 3, 2, 4, 2>(Transpose(A6x7gf))), oDofMap);
               SpMatrix<doublereal, 2, 3> A2x3f2 = Transpose(SubMatrix<3, 2>(Transpose(A6x7f), 3, 2, 2, 4));
               SpMatrix<SpGradient, 2, 3> A2x3gf2 = Transpose(SubMatrix<3, 2>(Transpose(A6x7gf), 3, 2, 2, 4));
               SpMatrix<SpGradient, 2, 3> A2x3gf2Map(Transpose(SubMatrix<3, 2>(Transpose(A6x7gf), 3, 2, 2, 4)), oDofMap);
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, 3> A2x3f3 = Transpose(SubMatrix<3, 2, 3>(Transpose(A6x7f2), 2, 4, 2));
               SpMatrix<SpGradient, SpMatrixSize::DYNAMIC, 3> A2x3gf3 = Transpose(SubMatrix<3, 2, 3>(Transpose(A6x7gf2), 2, 4, 2));
               SpMatrix<SpGradient, SpMatrixSize::DYNAMIC, 3> A2x3gf3Map(Transpose(SubMatrix<3, 2, 3>(Transpose(A6x7gf2), 2, 4, 2)), oDofMap);
               SpColVector<SpGradient, 3> A2x3gf1 = Transpose(A2x3gf.GetRow(1));
               SpMatrix<SpGradient, 3, 3> CrossX8a(MatCrossVec(A2x3gf1, 5.));
               SpMatrix<SpGradient, 3, 3> CrossX8aMap(MatCrossVec(A2x3gf1, 5.), oDofMap);

               SpMatrix<SpGradient, 6, 7> A6x7gf_oDofMap(A6x7gf, oDofMap);
               SpMatrix<SpGradient, 6, 7> A6x7gf_sum(6, 7, oDofMap);
               SpMatrix<SpGradient, 6, 7> A6x7gf_sum_neg(6, 7, oDofMap);

               SpMatrix<SpGradient> A6x7g_oDofMap(A6x7g, oDofMap);
               SpMatrix<SpGradient> A6x7g_sum(6, 7, oDofMap);
               SpMatrix<SpGradient> A6x7g_sum_neg(6, 7, oDofMap);

               SpMatrix<SpGradient> A7x7g_sum(7, 7, oDofMap);
               SpMatrix<SpGradient> A7x7g_sum_neg(7, 7, oDofMap);
               SpMatrix<SpGradient> A7x7g = Transpose(A6x7g) * A6x7g;

               SpMatrix<SpGradient, 7, 7> A7x7gf_sum(7, 7, oDofMap);
               SpMatrix<SpGradient, 7, 7> A7x7gf_sum_neg(7, 7, oDofMap);
               SpMatrix<SpGradient, 7, 7> A7x7gf = Transpose(A6x7gf) * A6x7gf;
               SpMatrix<SpGradient, 7, 7> A7x7gfT = Transpose(Transpose(Transpose(A6x7gf) * A6x7gf));
               SpMatrix<SpGradient, 7, 7> A7x7gfTT = Transpose(Transpose(Transpose(Transpose(A6x7gf) * A6x7gf)));
               SpMatrix<SpGradient, 7, 7> A7x7gfTTT = Transpose(Transpose(Transpose(Transpose(Transpose(A6x7gf) * A6x7gf))));
               SpMatrix<SpGradient, 7, 7> A7x7gfTTTT = Transpose(Transpose(Transpose(Transpose(Transpose(Transpose(A6x7gf) * A6x7gf)))));
               SpMatrix<SpGradient, 7, 7> A7x7gfT2 = Transpose(A6x7gf) * Transpose(Transpose(A6x7gf));
               SpMatrix<SpGradient, 7, 7> A7x7gfTT2 = Transpose(Transpose(Transpose(A6x7gf))) * Transpose(Transpose(A6x7gf));
               SpMatrix<SpGradient, 7, 7> A7x7gfTTT2 = Transpose(Transpose(Transpose(A6x7gf))) * Transpose(Transpose(Transpose(Transpose(A6x7gf))));
               SpMatrix<SpGradient, 7, 7> A7x7gfT3 = Transpose(Transpose(A6x7gf) * Transpose(Transpose(A6x7gf)));
               SpMatrix<SpGradient, 7, 7> A7x7gfTT3 = Transpose(Transpose(Transpose(A6x7gf) * Transpose(Transpose(A6x7gf))));
               SpMatrix<SpGradient, 7, 7> A7x7gfTTT3 = Transpose(Transpose(Transpose(Transpose(A6x7gf) * Transpose(Transpose(A6x7gf)))));
               Mat6x6 A6x6, B6x6;

               for (index_type i = 1; i <= 6; ++i) {
                    for (index_type j = 1; j <= 6; ++j) {
                         A6x6(i, j) = 10 * i + j;
                         B6x6(j, i) = 10 * i + j;
                    }
               }

               Mat6x6 C6x6 = A6x6 * B6x6;

               static constexpr doublereal C6x6ref[6][6] = {
                    {1111,1921,2731,3541,4351,5161},
                    {1921,3331,4741,6151,7561,8971},
                    {2731,4741,6751,8761,10771,12781},
                    {3541,6151,8761,11371,13981,16591},
                    {4351,7561,10771,13981,17191,20401},
                    {5161,8971,12781,16591,20401,24211}
               };

               for (index_type i = 1; i <= 6; ++i) {
                    for (index_type j = 1; j <= 6; ++j) {
                         sp_grad_assert_equal(C6x6(i, j), C6x6ref[i - 1][j - 1], dTol);
                    }
               }

               for (index_type i = 1; i <= 20; ++i) {
                    A6x7gf_sum.Add(A6x7gf_oDofMap * 0.5, oDofMap);
                    A6x7gf_sum_neg.Sub(A6x7gf_oDofMap * 0.5, oDofMap);
                    A6x7g_sum.Add(A6x7g_oDofMap * 0.5, oDofMap);
                    A6x7g_sum_neg.Sub(A6x7g_oDofMap * 0.5, oDofMap);

                    A7x7g_sum.Add(Transpose(A6x7g) * A6x7g, oDofMap);
                    A7x7g_sum_neg.Sub(Transpose(A6x7g) * A6x7g, oDofMap);
                    A7x7gf_sum.Add(Transpose(A6x7gf) * A6x7gf, oDofMap);
                    A7x7gf_sum_neg.Sub(Transpose(A6x7gf) * A6x7gf, oDofMap);
               }

               A6x7gf_sum /= 10.;
               A6x7gf_sum_neg /= -10.;
               A6x7g_sum /= 10.;
               A6x7g_sum_neg /= -10.;
               A7x7g_sum /= 20.;
               A7x7g_sum_neg /= -20.;
               A7x7gf_sum /= 20.;
               A7x7gf_sum_neg /= -20.;

               for (index_type i = 1; i <= A6x7gf.iGetNumRows(); ++i) {
                    for (index_type j = 1; j <= A6x7gf.iGetNumCols(); ++j) {
                         sp_grad_assert_equal(A6x7gf_sum(i, j), A6x7gf(i, j), dTol);
                         sp_grad_assert_equal(A6x7gf_sum_neg(i, j), A6x7gf(i, j), dTol);
                         sp_grad_assert_equal(A6x7g_sum(i, j), A6x7g(i, j), dTol);
                         sp_grad_assert_equal(A6x7g_sum_neg(i, j), A6x7g(i, j), dTol);
                         sp_grad_assert_equal(A7x7g_sum(i, j), A7x7g(i, j), dTol);
                         sp_grad_assert_equal(A7x7g_sum_neg(i, j), A7x7g(i, j), dTol);
                         sp_grad_assert_equal(A7x7gf_sum(i, j), A7x7gf(i, j), dTol);
                         sp_grad_assert_equal(A7x7gf_sum_neg(i, j), A7x7gf(i, j), dTol);
                         sp_grad_assert_equal(A7x7gfT(i, j), A7x7gf(i, j), dTol);
                         sp_grad_assert_equal(A7x7gfTT(i, j), A7x7gf(i, j), dTol);
                         sp_grad_assert_equal(A7x7gfTTT(i, j), A7x7gf(i, j), dTol);
                         sp_grad_assert_equal(A7x7gfTTTT(i, j), A7x7gf(i, j), dTol);
                         sp_grad_assert_equal(A7x7gfT2(i, j), A7x7gf(i, j), dTol);
                         sp_grad_assert_equal(A7x7gfTT2(i, j), A7x7gf(i, j), dTol);
                         sp_grad_assert_equal(A7x7gfTTT2(i, j), A7x7gf(i, j), dTol);
                         sp_grad_assert_equal(A7x7gfT3(i, j), A7x7gf(i, j), dTol);
                         sp_grad_assert_equal(A7x7gfTT3(i, j), A7x7gf(i, j), dTol);
                         sp_grad_assert_equal(A7x7gfTTT3(i, j), A7x7gf(i, j), dTol);
                    }
               }

               for (index_type i = 1; i <= 3; ++i) {
                    for (index_type j = 1; j <= 3; ++j) {
                         sp_grad_assert_equal(CrossX8a(i, j), CrossX8aMap(i, j), dTol);
                    }
               }

               sp_grad_assert_equal(A2x3(1, 1), A6x7(2, 3), dTol);
               sp_grad_assert_equal(A2x3(1, 2), A6x7(2, 5), dTol);
               sp_grad_assert_equal(A2x3(1, 3), A6x7(2, 7), dTol);
               sp_grad_assert_equal(A2x3(2, 1), A6x7(6, 3), dTol);
               sp_grad_assert_equal(A2x3(2, 2), A6x7(6, 5), dTol);
               sp_grad_assert_equal(A2x3(2, 3), A6x7(6, 7), dTol);

               sp_grad_assert_equal(A2x3g(1, 1), A6x7g(2, 3), dTol);
               sp_grad_assert_equal(A2x3g(1, 2), A6x7g(2, 5), dTol);
               sp_grad_assert_equal(A2x3g(1, 3), A6x7g(2, 7), dTol);
               sp_grad_assert_equal(A2x3g(2, 1), A6x7g(6, 3), dTol);
               sp_grad_assert_equal(A2x3g(2, 2), A6x7g(6, 5), dTol);
               sp_grad_assert_equal(A2x3g(2, 3), A6x7g(6, 7), dTol);

               sp_grad_assert_equal(A2x3gMap(1, 1), A6x7g(2, 3), dTol);
               sp_grad_assert_equal(A2x3gMap(1, 2), A6x7g(2, 5), dTol);
               sp_grad_assert_equal(A2x3gMap(1, 3), A6x7g(2, 7), dTol);
               sp_grad_assert_equal(A2x3gMap(2, 1), A6x7g(6, 3), dTol);
               sp_grad_assert_equal(A2x3gMap(2, 2), A6x7g(6, 5), dTol);
               sp_grad_assert_equal(A2x3gMap(2, 3), A6x7g(6, 7), dTol);

               sp_grad_assert_equal(A2x3f(1, 1), A6x7f(2, 3), dTol);
               sp_grad_assert_equal(A2x3f(1, 2), A6x7f(2, 5), dTol);
               sp_grad_assert_equal(A2x3f(1, 3), A6x7f(2, 7), dTol);
               sp_grad_assert_equal(A2x3f(2, 1), A6x7f(6, 3), dTol);
               sp_grad_assert_equal(A2x3f(2, 2), A6x7f(6, 5), dTol);
               sp_grad_assert_equal(A2x3f(2, 3), A6x7f(6, 7), dTol);

               sp_grad_assert_equal(A2x3f2(1, 1), A6x7f(2, 3), dTol);
               sp_grad_assert_equal(A2x3f2(1, 2), A6x7f(2, 5), dTol);
               sp_grad_assert_equal(A2x3f2(1, 3), A6x7f(2, 7), dTol);
               sp_grad_assert_equal(A2x3f2(2, 1), A6x7f(6, 3), dTol);
               sp_grad_assert_equal(A2x3f2(2, 2), A6x7f(6, 5), dTol);
               sp_grad_assert_equal(A2x3f2(2, 3), A6x7f(6, 7), dTol);

               sp_grad_assert_equal(A2x3f3(1, 1), A6x7f(2, 3), dTol);
               sp_grad_assert_equal(A2x3f3(1, 2), A6x7f(2, 5), dTol);
               sp_grad_assert_equal(A2x3f3(1, 3), A6x7f(2, 7), dTol);
               sp_grad_assert_equal(A2x3f3(2, 1), A6x7f(6, 3), dTol);
               sp_grad_assert_equal(A2x3f3(2, 2), A6x7f(6, 5), dTol);
               sp_grad_assert_equal(A2x3f3(2, 3), A6x7f(6, 7), dTol);

               sp_grad_assert_equal(A2x3gf(1, 1), A6x7gf(2, 3), dTol);
               sp_grad_assert_equal(A2x3gf(1, 2), A6x7gf(2, 5), dTol);
               sp_grad_assert_equal(A2x3gf(1, 3), A6x7gf(2, 7), dTol);
               sp_grad_assert_equal(A2x3gf(2, 1), A6x7gf(6, 3), dTol);
               sp_grad_assert_equal(A2x3gf(2, 2), A6x7gf(6, 5), dTol);
               sp_grad_assert_equal(A2x3gf(2, 3), A6x7gf(6, 7), dTol);

               sp_grad_assert_equal(A2x3gf2(1, 1), A6x7gf(2, 3), dTol);
               sp_grad_assert_equal(A2x3gf2(1, 2), A6x7gf(2, 5), dTol);
               sp_grad_assert_equal(A2x3gf2(1, 3), A6x7gf(2, 7), dTol);
               sp_grad_assert_equal(A2x3gf2(2, 1), A6x7gf(6, 3), dTol);
               sp_grad_assert_equal(A2x3gf2(2, 2), A6x7gf(6, 5), dTol);
               sp_grad_assert_equal(A2x3gf2(2, 3), A6x7gf(6, 7), dTol);

               sp_grad_assert_equal(A2x3gf3(1, 1), A6x7gf(2, 3), dTol);
               sp_grad_assert_equal(A2x3gf3(1, 2), A6x7gf(2, 5), dTol);
               sp_grad_assert_equal(A2x3gf3(1, 3), A6x7gf(2, 7), dTol);
               sp_grad_assert_equal(A2x3gf3(2, 1), A6x7gf(6, 3), dTol);
               sp_grad_assert_equal(A2x3gf3(2, 2), A6x7gf(6, 5), dTol);
               sp_grad_assert_equal(A2x3gf3(2, 3), A6x7gf(6, 7), dTol);

               SpMatrix<doublereal> A2x3b = SubMatrix(A6x7, 2, 4, 2, 3, 2, 3);
               SpMatrix<SpGradient> A2x3gb = SubMatrix(A6x7g, 2, 4, 2, 3, 2, 3);
               SpMatrix<SpGradient> A2x3gbMap(SubMatrix(A6x7g, 2, 4, 2, 3, 2, 3), oDofMap);
               SpMatrix<doublereal, 2, 3> A2x3bf = SubMatrix<2, 4, 2, 3, 2, 3>(A6x7f);
               SpMatrix<SpGradient, 2, 3> A2x3gbf = SubMatrix<2, 4, 2, 3, 2, 3>(A6x7gf);
               SpMatrix<SpGradient, 2, 3> A2x3gbfMap(SubMatrix<2, 4, 2, 3, 2, 3>(A6x7gf), oDofMap);
               SpMatrix<doublereal, 2, 3> A2x3bf2 = SubMatrix<2, 3>(A6x7f, 2, 4, 3, 2);
               SpMatrix<SpGradient, 2, 3> A2x3gbf2 = SubMatrix<2, 3>(A6x7gf, 2, 4, 3, 2);
               SpMatrix<SpGradient, 2, 3> A2x3gbf2Map(SubMatrix<2, 3>(A6x7gf, 2, 4, 3, 2), oDofMap);

               for (index_type i = 1; i <= A2x3.iGetNumRows(); ++i) {
                    for (index_type j = 1; j <= A2x3.iGetNumCols(); ++j) {
                         sp_grad_assert_equal(A2x3(i, j), A2x3b(i, j), dTol);
                         sp_grad_assert_equal(A2x3g(i, j), A2x3gb(i, j), dTol);
                         sp_grad_assert_equal(A2x3f(i, j), A2x3bf(i, j), dTol);
                         sp_grad_assert_equal(A2x3gf(i, j), A2x3gbf(i, j), dTol);
                         sp_grad_assert_equal(A2x3f2(i, j), A2x3bf(i, j), dTol);
                         sp_grad_assert_equal(A2x3gf2(i, j), A2x3gbf(i, j), dTol);
                         sp_grad_assert_equal(B2x3gMap(i, j), B2x3g(i, j), dTol);
                         sp_grad_assert_equal(A2x3gfMap(i, j), A2x3gf(i, j), dTol);
                         sp_grad_assert_equal(A2x3gf2Map(i, j), A2x3gf(i, j), dTol);
                         sp_grad_assert_equal(A2x3gbMap(i, j), A2x3gb(i, j), dTol);
                         sp_grad_assert_equal(A2x3gbfMap(i, j), A2x3gb(i, j), dTol);
                         sp_grad_assert_equal(A2x3gbf2Map(i, j), A2x3gb(i, j), dTol);
                    }
               }

               SpMatrix<doublereal> A2x3T_A2x3 = Transpose(A2x3) * A2x3b;
               SpMatrix<SpGradient> A2x3gT_A2x3g = Transpose(A2x3g) * A2x3gb;
               SpMatrix<SpGradient> A2x3gT_A2x3gMap(Transpose(A2x3g) * A2x3gb, oDofMap);
               SpMatrix<doublereal> A2x3T_A2x3b = SubMatrix(Transpose(A6x7), 3, 2, 3, 2, 4, 2) * Transpose(SubMatrix(Transpose(A6x7), 3, 2, 3, 2, 4, 2));
               SpMatrix<SpGradient> A2x3gT_A2x3gb = SubMatrix(Transpose(A6x7g), 3, 2, 3, 2, 4, 2) * Transpose(SubMatrix(Transpose(A6x7g), 3, 2, 3, 2, 4, 2));
               SpMatrix<SpGradient> A2x3gT_A2x3gbMap(SubMatrix(Transpose(A6x7g), 3, 2, 3, 2, 4, 2) * Transpose(SubMatrix(Transpose(A6x7g), 3, 2, 3, 2, 4, 2)), oDofMap);
               SpMatrix<doublereal> A2x3T_A2x3c = Transpose(SubMatrix(A6x7, 2, 4, 2, 3, 2, 3)) * SubMatrix(A6x7, 2, 4, 2, 3, 2, 3);
               SpMatrix<SpGradient> A2x3gT_A2x3gc = Transpose(SubMatrix(A6x7g, 2, 4, 2, 3, 2, 3)) * SubMatrix(A6x7g, 2, 4, 2, 3, 2, 3);
               SpMatrix<SpGradient> A2x3gT_A2x3gcMap(Transpose(SubMatrix(A6x7g, 2, 4, 2, 3, 2, 3)) * SubMatrix(A6x7g, 2, 4, 2, 3, 2, 3), oDofMap);
               SpMatrix<doublereal> A2x3T_A2x3d =  Transpose(Transpose(SubMatrix(A6x7, 2, 4, 2, 3, 2, 3)) * SubMatrix(A6x7, 2, 4, 2, 3, 2, 3));
               SpMatrix<SpGradient> A2x3gT_A2x3gd =  Transpose(Transpose(SubMatrix(A6x7g, 2, 4, 2, 3, 2, 3)) * SubMatrix(A6x7g, 2, 4, 2, 3, 2, 3));
               SpMatrix<SpGradient> A2x3gT_A2x3gdMap(Transpose(Transpose(SubMatrix(A6x7g, 2, 4, 2, 3, 2, 3)) * SubMatrix(A6x7g, 2, 4, 2, 3, 2, 3)), oDofMap);
               SpMatrix<doublereal> A2x3T_A2x3e =  Transpose(SubMatrix(Transpose(A6x7), 3, 2, 3, 2, 4, 2) * SubMatrix(A6x7, 2, 4, 2, 3, 2, 3));
               SpMatrix<SpGradient> A2x3gT_A2x3ge =  Transpose(SubMatrix(Transpose(A6x7g), 3, 2, 3, 2, 4, 2) * SubMatrix(A6x7g, 2, 4, 2, 3, 2, 3));
               SpMatrix<SpGradient> A2x3gT_A2x3geMap(Transpose(SubMatrix(Transpose(A6x7g), 3, 2, 3, 2, 4, 2) * SubMatrix(A6x7g, 2, 4, 2, 3, 2, 3)), oDofMap);
               SpMatrix<SpGradient> A2x3gT_A2x3gT = Transpose(Transpose(A2x3g) * A2x3g);
               SpMatrix<SpGradient> A2x3gT_A2x3gTMap(Transpose(Transpose(A2x3g) * A2x3g), oDofMap);
               SpMatrix<SpGradient> A2x3gT_A2x3gTb =  SubMatrix(Transpose(A6x7g), 3, 2, 3, 2, 4, 2) * SubMatrix(A6x7g, 2, 4, 2, 3, 2, 3);
               SpMatrix<SpGradient> A2x3gT_A2x3gTbMap(SubMatrix(Transpose(A6x7g), 3, 2, 3, 2, 4, 2) * SubMatrix(A6x7g, 2, 4, 2, 3, 2, 3), oDofMap);

               SpMatrix<doublereal, 3, 3> A2x3T_A2x3f = Transpose(A2x3f) * A2x3bf;
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gf = Transpose(A2x3gf) * A2x3gbf;
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gfMap(Transpose(A2x3gf) * A2x3gbf, oDofMap);
               SpMatrix<doublereal, 3, 3> A2x3T_A2x3bf = SubMatrix<3, 2, 3, 2, 4, 2>(Transpose(A6x7f)) * Transpose(SubMatrix<3, 2, 3, 2, 4, 2>(Transpose(A6x7f)));
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gbf = SubMatrix<3, 2, 3, 2, 4, 2>(Transpose(A6x7gf)) * Transpose(SubMatrix<3, 2, 3, 2, 4, 2>(Transpose(A6x7gf)));
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gbfMap(SubMatrix<3, 2, 3, 2, 4, 2>(Transpose(A6x7gf)) * Transpose(SubMatrix<3, 2, 3, 2, 4, 2>(Transpose(A6x7gf))), oDofMap);
               SpMatrix<doublereal, 3, 3> A2x3T_A2x3cf = Transpose(SubMatrix<2, 4, 2, 3, 2, 3>(A6x7f)) * SubMatrix<2, 4, 2, 3, 2, 3>(A6x7f);
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gcf = Transpose(SubMatrix<2, 4, 2, 3, 2, 3>(A6x7gf)) * SubMatrix<2, 4, 2, 3, 2, 3>(A6x7gf);
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gcfMap(Transpose(SubMatrix<2, 4, 2, 3, 2, 3>(A6x7gf)) * SubMatrix<2, 4, 2, 3, 2, 3>(A6x7gf), oDofMap);
               SpMatrix<doublereal, 3, 3> A2x3T_A2x3df =  Transpose(Transpose(SubMatrix<2, 4, 2, 3, 2, 3>(A6x7f)) * SubMatrix<2, 4, 2, 3, 2, 3>(A6x7f));
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gdf =  Transpose(Transpose(SubMatrix<2, 4, 2, 3, 2, 3>(A6x7gf)) * SubMatrix<2, 4, 2, 3, 2, 3>(A6x7gf));
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gdfMap(Transpose(Transpose(SubMatrix<2, 4, 2, 3, 2, 3>(A6x7gf)) * SubMatrix<2, 4, 2, 3, 2, 3>(A6x7gf)), oDofMap);
               SpMatrix<doublereal, 3, 3> A2x3T_A2x3ef =  Transpose(SubMatrix<3, 2, 3, 2, 4, 2>(Transpose(A6x7f)) * SubMatrix<2, 4, 2, 3, 2, 3>(A6x7f));
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gef =  Transpose(SubMatrix<3, 2, 3, 2, 4, 2>(Transpose(A6x7gf)) * SubMatrix<2, 4, 2, 3, 2, 3>(A6x7gf));
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gefMap(Transpose(SubMatrix<3, 2, 3, 2, 4, 2>(Transpose(A6x7gf)) * SubMatrix<2, 4, 2, 3, 2, 3>(A6x7gf)), oDofMap);
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gTf = Transpose(Transpose(A2x3gf) * A2x3gf);
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gTfMap(Transpose(Transpose(A2x3gf) * A2x3gf), oDofMap);
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gTbf =  SubMatrix<3, 2, 3, 2, 4, 2>(Transpose(A6x7gf)) * SubMatrix<2, 4, 2, 3, 2, 3>(A6x7gf);
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gTbfMap(SubMatrix<3, 2, 3, 2, 4, 2>(Transpose(A6x7gf)) * SubMatrix<2, 4, 2, 3, 2, 3>(A6x7gf), oDofMap);

               SpMatrix<doublereal, 3, 3> A2x3T_A2x3f2 = Transpose(A2x3f2) * A2x3bf2;
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gf2 = Transpose(A2x3gf2) * A2x3gbf2;
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gf2Map(Transpose(A2x3gf2) * A2x3gbf2, oDofMap);
               SpMatrix<doublereal, 3, 3> A2x3T_A2x3bf2 = SubMatrix<3, 2>(Transpose(A6x7f), 3, 2, 2, 4) * Transpose(SubMatrix<3, 2>(Transpose(A6x7f), 3, 2, 2, 4));
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gbf2 = SubMatrix<3, 2>(Transpose(A6x7gf), 3, 2, 2, 4) * Transpose(SubMatrix<3, 2>(Transpose(A6x7gf), 3, 2, 2, 4));
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gbf2Map(SubMatrix<3, 2>(Transpose(A6x7gf), 3, 2, 2, 4) * Transpose(SubMatrix<3, 2>(Transpose(A6x7gf), 3, 2, 2, 4)), oDofMap);
               SpMatrix<doublereal, 3, 3> A2x3T_A2x3cf2 = Transpose(SubMatrix<2, 3>(A6x7f, 2, 4, 3, 2)) * SubMatrix<2, 3>(A6x7f, 2, 4, 3, 2);
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gcf2 = Transpose(SubMatrix<2, 3>(A6x7gf, 2, 4, 3, 2)) * SubMatrix<2, 3>(A6x7gf, 2, 4, 3, 2);
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gcf2Map(Transpose(SubMatrix<2, 3>(A6x7gf, 2, 4, 3, 2)) * SubMatrix<2, 3>(A6x7gf, 2, 4, 3, 2), oDofMap);
               SpMatrix<doublereal, 3, 3> A2x3T_A2x3df2 =  Transpose(Transpose(SubMatrix<2, 3>(A6x7f, 2, 4, 3, 2)) * SubMatrix<2, 3>(A6x7f, 2, 4, 3, 2));
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gdf2 =  Transpose(Transpose(SubMatrix<2, 3>(A6x7gf, 2, 4, 3, 2)) * SubMatrix<2, 3>(A6x7gf, 2, 4, 3, 2));
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gdf2Map(Transpose(Transpose(SubMatrix<2, 3>(A6x7gf, 2, 4, 3, 2)) * SubMatrix<2, 3>(A6x7gf, 2, 4, 3, 2)), oDofMap);
               SpMatrix<doublereal, 3, 3> A2x3T_A2x3ef2 =  Transpose(SubMatrix<3, 2>(Transpose(A6x7f), 3, 2, 2, 4) * SubMatrix<2, 3>(A6x7f, 2, 4, 3, 2));
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gef2 =  Transpose(SubMatrix<3, 2>(Transpose(A6x7gf), 3, 2, 2, 4) * SubMatrix<2, 3>(A6x7gf, 2, 4, 3, 2));
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gef2Map(Transpose(SubMatrix<3, 2>(Transpose(A6x7gf), 3, 2, 2, 4) * SubMatrix<2, 3>(A6x7gf, 2, 4, 3, 2)), oDofMap);
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gTf2 = Transpose(Transpose(A2x3gf) * A2x3gf);
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gTf2Map(Transpose(Transpose(A2x3gf) * A2x3gf), oDofMap);
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gTbf2 =  SubMatrix<3, 2>(Transpose(A6x7gf), 3, 2, 2, 4) * SubMatrix<2, 3>(A6x7gf, 2, 4, 3, 2);
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gTbf2Map(SubMatrix<3, 2>(Transpose(A6x7gf), 3, 2, 2, 4) * SubMatrix<2, 3>(A6x7gf, 2, 4, 3, 2), oDofMap);

               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gf3 = Transpose(A2x3gf3) * A2x3gf3;
               SpMatrix<SpGradient, 3, 3> A2x3gT_A2x3gf3Map(Transpose(A2x3gf3) * A2x3gf3, oDofMap);

               for (index_type i = 1; i <= A2x3T_A2x3.iGetNumRows(); ++i) {
                    for (index_type j = 1; j <= A2x3T_A2x3.iGetNumCols(); ++j) {
                         sp_grad_assert_equal(A2x3T_A2x3(i, j), A2x3T_A2x3b(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3T_A2x3(i, j), A2x3T_A2x3c(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3T_A2x3(i, j), A2x3T_A2x3d(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3T_A2x3(i, j), A2x3T_A2x3e(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));

                         sp_grad_assert_equal(A2x3gT_A2x3g(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3g(i, j), A2x3gT_A2x3gb(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3g(i, j), A2x3gT_A2x3gc(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3g(i, j), A2x3gT_A2x3gd(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3g(i, j), A2x3gT_A2x3ge(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3g(i, j), A2x3gT_A2x3gT(j, i), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3g(i, j), A2x3gT_A2x3gTb(j, i), sqrt(std::numeric_limits<doublereal>::epsilon()));

                         sp_grad_assert_equal(A2x3T_A2x3f(i, j), A2x3T_A2x3(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3T_A2x3f(i, j), A2x3T_A2x3bf(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3T_A2x3f(i, j), A2x3T_A2x3cf(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3T_A2x3f(i, j), A2x3T_A2x3df(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3T_A2x3f(i, j), A2x3T_A2x3ef(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));

                         sp_grad_assert_equal(A2x3gT_A2x3gf(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gf(i, j), A2x3gT_A2x3gbf(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gf(i, j), A2x3gT_A2x3gcf(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gf(i, j), A2x3gT_A2x3gdf(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gf(i, j), A2x3gT_A2x3gef(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gf(i, j), A2x3gT_A2x3gTf(j, i), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gf(i, j), A2x3gT_A2x3gTbf(j, i), sqrt(std::numeric_limits<doublereal>::epsilon()));

                         sp_grad_assert_equal(A2x3gT_A2x3gf2(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gf2(i, j), A2x3gT_A2x3gbf2(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gf2(i, j), A2x3gT_A2x3gcf2(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gf2(i, j), A2x3gT_A2x3gdf2(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gf2(i, j), A2x3gT_A2x3gef2(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gf2(i, j), A2x3gT_A2x3gTf2(j, i), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gf2(i, j), A2x3gT_A2x3gTbf2(j, i), sqrt(std::numeric_limits<doublereal>::epsilon()));

                         sp_grad_assert_equal(A2x3gT_A2x3gf3(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gMap(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));

                         sp_grad_assert_equal(A2x3gT_A2x3gMap(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gbMap(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gcMap(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gdMap(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3geMap(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));

                         sp_grad_assert_equal(A2x3gT_A2x3gTMap(j, i), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gTbMap(j, i), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gfMap(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gbfMap(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gcfMap(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gdfMap(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gefMap(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gTfMap(j, i), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gTbfMap(j, i), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gf2Map(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gf2Map(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gbf2Map(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gcf2Map(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gdf2Map(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gef2Map(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gTf2Map(j, i), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gTbf2Map(j, i), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(A2x3gT_A2x3gf3Map(i, j), A2x3gT_A2x3g(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                    }
               }

               X10a.ResizeReset(3, 3, 10);
               X10b.ResizeReset(3, 3, 10);

               sp_grad_rand_gen(d, randnz, randdof, randval, gen);

               for (index_type i = 1; i <= 2; ++i) {
                    for (index_type j = 1; j <= 2; ++j) {
                         sp_grad_rand_gen(E2x2(i, j), randnz, randdof, randval, gen);
                    }
               }

               for (index_type i = 1; i <= 3; ++i) {
                    for (index_type j = 1; j <= 3; ++j) {
                         sp_grad_rand_gen(A(i, j), randnz, randdof, randval, gen);
                         sp_grad_rand_gen(C(i, j), randnz, randdof, randval, gen);
                         sp_grad_rand_gen(X1(i, j), randnz, randdof, randval, gen);
                         sp_grad_rand_gen(X2(i, j), randnz, randdof, randval, gen);
                         A1(i, j) = Ann(i, j) = An3(i, j) = A3n(i, j) = A(i, j);
                         C1(i, j) = Cnn(i, j) = Cn3(i, j) = C3n(i, j) = C(i, j);
                    }

                    sp_grad_rand_gen(b(i), randnz, randdof, randval, gen);
                    sp_grad_rand_gen(g(i), randnz, randdof, randval, gen);
                    gb6(i) = g(i);
                    gb6(i + 3) = b(i);
               }

               for (index_type i = 1; i <= 3; ++i) {
                    for (index_type j = 1; j <= 2; ++j) {
                         sp_grad_rand_gen(X12a(j, i), randnz, randdof, randval, gen);
                         sp_grad_rand_gen(X13a(i, j), randnz, randdof, randval, gen);
                    }
               }

               Vec3 u = A * b + C.Transpose() * g;
               SpColVector<doublereal, 3> u1 = Transpose(Transpose(b) * Transpose(A)) + Transpose(C) * g;
               SpColVector<doublereal, 3> u2 = A * SubColVector<4, 1, 3>(gb6) + Transpose(C) * SubColVector<1, 1, 3>(gb6);
               SpColVector<doublereal, 3> u3 = Transpose(Transpose(SubColVector<4, 1, 3>(gb6)) * Transpose(A) + Transpose(SubColVector<1, 1, 3>(gb6)) * C);
               SpRowVector<doublereal, 3> u4 = Transpose(A * SubColVector<4, 1, 3>(gb6) + Transpose(C) * SubColVector<1, 1, 3>(gb6));
               SpRowVector<doublereal, 3> u5 = Transpose(SubColVector<4, 1, 3>(gb6)) * Transpose(A) + Transpose(SubColVector<1, 1, 3>(gb6)) * C;
               SpRowVector<doublereal, 3> u6 = Transpose(A * SubColVector<4, 1, 3>(gb6)) + Transpose(Transpose(C) * SubColVector<1, 1, 3>(gb6));

               doublereal d0 = u.Dot();
               doublereal d1 = Dot(u1, u1);
               doublereal d2 = Dot(Transpose(Transpose(u1)), Transpose(Transpose(u2)));
               doublereal d3 = Dot(Transpose(u4), u1);
               doublereal d4 = Dot(u1, Transpose(u4));
               doublereal d5 = Dot(Transpose(Transpose(u1)), Transpose(u5));
               doublereal d6 = Dot(u, A * SubColVector<4, 1, 3>(gb6)) + Dot(Transpose(C) * SubColVector<1, 1, 3>(gb6), u);
               doublereal d7 = Dot(A * SubColVector<4, 1, 3>(gb6), Transpose(u5)) + Dot(Transpose(u6), Transpose(C) * SubColVector<1, 1, 3>(gb6));

               Mat3x3 AC = A * C;
               SpMatrix<doublereal, 3, 3> AC1 = Transpose(Transpose(C) * Transpose(A));
               SpMatrixA<doublereal, 3, 3> AC2, AC4, AC7, AC8, AC10;
               SpMatrix<doublereal> AC3(3, 3, 0), AC5(3, 3, 0), AC6(3, 3, 0), AC9(3, 3, 0), AC11(3, 3, 0);

               for (index_type i = 1; i <= 3; ++i) {
                    for (index_type j = 1; j <= 3; ++j) {
                         AC2(i, j) = Dot(Transpose(A1.GetRow(i)), C1.GetCol(j));
                         AC3(i, j) = Dot(C1.GetCol(j), Transpose(A1.GetRow(i)));
                         AC4(i, j) = Dot(A.GetRow(i), C.GetCol(j));
                         AC5(i, j) = Dot(C.GetCol(j), A.GetRow(i));
                         AC6(i, j) = Dot(Transpose(SubMatrix<1, 3>(A1, i, 1, 1, 1)), SubMatrix<3, 1>(C1, 1, 1, j, 1));
                         AC7(i, j) = Dot(SubMatrix<3, 1>(C1, 1, 1, j, 1), Transpose(SubMatrix<1, 3>(A1, i, 1, 1, 1)));
                         AC8(i, j) = Dot(Transpose(SubMatrix<1, 3>(A, i, 1, 1, 1)), SubMatrix<3, 1>(C, 1, 1, j, 1));
                         AC9(i, j) = Dot(SubMatrix<3, 1>(C, 1, 1, j, 1), Transpose(SubMatrix<1, 3>(A, i, 1, 1, 1)));
                         AC10(i, j) = Dot(Transpose(SubMatrix<1, 2>(A, i, 1, 1, 2)), SubMatrix<2, 1>(C, 1, 2, j, 1)) + A(i, 2) * C(2, j);
                         AC11(i, j) = Dot(Transpose(SubMatrix<1, 2>(A1, i, 1, 1, 2)), SubMatrix<2, 1>(C1, 1, 2, j, 1)) + A1(i, 2) * C1(2, j);
                    }
               }

               for (index_type i = 1; i <= 3; ++i) {
                    sp_grad_assert_equal(u(i), u1(i), sqrt(std::numeric_limits<doublereal>::epsilon()));
                    sp_grad_assert_equal(u(i), u2(i), sqrt(std::numeric_limits<doublereal>::epsilon()));
                    sp_grad_assert_equal(u(i), u3(i), sqrt(std::numeric_limits<doublereal>::epsilon()));
                    sp_grad_assert_equal(u(i), u4(i), sqrt(std::numeric_limits<doublereal>::epsilon()));
                    sp_grad_assert_equal(u(i), u5(i), sqrt(std::numeric_limits<doublereal>::epsilon()));
                    sp_grad_assert_equal(u(i), u6(i), sqrt(std::numeric_limits<doublereal>::epsilon()));

                    for (index_type j = 1; j <= 3; ++j) {
                         sp_grad_assert_equal(AC(i, j), AC1(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(AC(i, j), AC2(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(AC(i, j), AC3(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(AC(i, j), AC4(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(AC(i, j), AC5(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(AC(i, j), AC6(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(AC(i, j), AC7(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(AC(i, j), AC8(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(AC(i, j), AC9(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(AC(i, j), AC10(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                         sp_grad_assert_equal(AC(i, j), AC11(i, j), sqrt(std::numeric_limits<doublereal>::epsilon()));
                    }
               }

               sp_grad_assert_equal(d0, d1, sqrt(std::numeric_limits<doublereal>::epsilon()));
               sp_grad_assert_equal(d0, d2, sqrt(std::numeric_limits<doublereal>::epsilon()));
               sp_grad_assert_equal(d0, d3, sqrt(std::numeric_limits<doublereal>::epsilon()));
               sp_grad_assert_equal(d0, d4, sqrt(std::numeric_limits<doublereal>::epsilon()));
               sp_grad_assert_equal(d0, d5, sqrt(std::numeric_limits<doublereal>::epsilon()));
               sp_grad_assert_equal(d0, d6, sqrt(std::numeric_limits<doublereal>::epsilon()));
               sp_grad_assert_equal(d0, d7, sqrt(std::numeric_limits<doublereal>::epsilon()));

               SpMatrix<doublereal, 3, 3> Asp(A), Csp(C);
               SpMatrix<doublereal, 2, 2> InvE2x2 = Inv(E2x2);
               SpMatrix<doublereal, 2, 2> InvE2x2_E2x2 = InvE2x2 * E2x2;
               SpMatrix<doublereal, 3, 3> Asp4{A(1, 1), A(2, 1), A(3, 1),
                                               A(1, 2), A(2, 2), A(3, 2),
                                               A(1, 3), A(2, 3), A(3, 3)};
               Mat3x3 A4(A(1, 1), A(2, 1), A(3, 1),
                         A(1, 2), A(2, 2), A(3, 2),
                         A(1, 3), A(2, 3), A(3, 3));
               SpColVector<doublereal, 3> gsp(g);
               SpColVector<doublereal, 3> gsp4{g(1), g(2), g(3)};
               SpRowVector<doublereal, 3> gsp5{g(1), g(2), g(3)};
               Mat3x3 RDelta(CGR_Rot::MatR, g);
               Mat3x3 RVec = RotManip::Rot(g);
               Vec3 g2 = RotManip::VecRot(RVec);
               Vec3 g3 = -g2;
               SpMatrix<SpGradient, 3, 3> Asp2{Asp}, Asp3{A};
               SpMatrix<doublereal, 3, 3> Asp5{Asp2.GetValue()};
               SpColVector<SpGradient, 3> gsp3{g};
               Asp2 = Transpose(A);
               gsp3 = 2 * g / Dot(g, g);
               SpMatrix<doublereal, 3, 3> RDeltasp = MatRVec(gsp);
               SpMatrix<doublereal, 3, 3> RVecsp = MatRotVec(gsp);
               SpColVector<doublereal, 3> gsp2 = VecRotMat(RVecsp);
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, 3> Aspn(3, 3, 0), Cspn(3, 3, 0);
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC> Aspnn(3, 3, 0), Cspnn(3, 3, 0);
               SpColVector<SpGradient, 3> xsp{g};
               xsp /= Norm(xsp);
               SpGradient nx = Norm(xsp);


               for (index_type i = 1; i <= 3; ++i) {
                    for (index_type j = 1; j <= 3; ++j) {
                         Aspnn(i, j) = Aspn(i, j) = A(i, j);
                         Cspnn(i, j) = Cspn(i, j) = C(i, j);
                    }
               }

               SpColVector<doublereal, 3> bsp(b);
               SpColVector<doublereal, SpMatrixSize::DYNAMIC> bspn(3, 0);

               Asp = A;
               bsp = b;

               for (index_type i = 1; i <= 3; ++i) {
                    bspn(i) = bsp(i);
               }

               SpColVector<doublereal, 3> csp1 = Asp * b;
               SpColVector<doublereal, 3> csp2 = A * bsp;
               SpColVector<doublereal, 3> csp3 = Asp * bsp;
               SpColVector<doublereal, 3> csp4{A * b};

               SpColVector<doublereal, 3> q = 3. * csp1 + 1.5 * csp1;

               Vec3 c = A * b;

               Mat3x3 D = A * C;

               SpMatrix<doublereal, 3, 3> Dsp1{A * C};
               SpMatrix<doublereal, 3, 3> Dsp2 = Asp * C;
               SpMatrix<doublereal, 3, 3> Dsp3 = A * Csp;
               SpMatrix<doublereal, 3, 3> Dsp4 = Asp * Csp;
               SpMatrix<doublereal, 3, SpMatrixSize::DYNAMIC> Dsp5 = Asp * C3n;
               SpMatrix<doublereal, 3, 3> Dsp6 = A3n * Cspn;
               SpMatrix<doublereal, 3, 3> Dsp7 = Transpose(Transpose(Csp) * Transpose(Asp));
               SpMatrix<doublereal, 3, 3> Dsp8 = Transpose(C.Transpose() * Transpose(Asp));
               SpMatrix<doublereal, 3, 3> Dsp9 = Transpose(Transpose(Csp) * A.Transpose());
               SpMatrix<doublereal, 3, SpMatrixSize::DYNAMIC> Dsp10 = Transpose(Transpose(C3n) * Transpose(Asp));
               SpMatrix<doublereal, 3, 3> Dsp11 = Transpose(Transpose(Cspn) * Transpose(A3n));
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC> Dsp12 = An3 * C3n;
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC> Dsp13 = Transpose(Transpose(C3n) * Transpose(An3));
               SpMatrix<doublereal, 3, 3> Dsp14 = A3n * Cn3;
               SpMatrix<doublereal, 3, 3> Dsp15 = Transpose(Transpose(Cn3) * Transpose(A3n));
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC> Dsp16 = Ann * Cnn;
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC> Dsp17 = Aspnn * Cspnn;
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC> Dsp18 = Aspnn * Cnn;
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC> Dsp19 = Ann * Cspnn;
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC> Dsp20 = Transpose(Transpose(Cspnn) * Transpose(Aspnn));
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC> Dsp21 = Transpose(Transpose(Cnn) * Transpose(Ann));
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC> Dsp22 = Transpose(Transpose(Cspnn) * Transpose(Ann));
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC> Dsp23 = Transpose(Transpose(Cnn) * Transpose(Aspnn));

               Mat3x3 E = A.Transpose() * C;
               SpMatrix<doublereal, 3, 3> Esp1 = Transpose(Asp) * Csp;
               SpMatrix<doublereal, 3, 3> Esp2 = A.Transpose() * Csp;
               SpMatrix<doublereal, 3, 3> Esp3 = Transpose(Asp) * C;
               SpMatrix<doublereal, 3, 3> Esp4{A.Transpose() * C};
               SpMatrix<doublereal, 3, 3> Esp5 = Transpose(A) * Csp;
               SpMatrix<doublereal, 3, 3> Esp6 = Transpose(A) * C;
               SpMatrix<doublereal, 3, 3> Esp7 = Transpose(Transpose(C) * A);
               SpMatrix<doublereal, 3, 3> Esp8 = Transpose(Transpose(Csp) * Asp);
               SpMatrix<doublereal, 3, 3> Esp9 = Transpose(Transpose(C) * Asp);
               SpMatrix<doublereal, 3, 3> Esp10 = Transpose(Transpose(Csp) * A);

               Mat3x3 F = A.Transpose() + C;
               SpMatrix<doublereal, 3, 3> Fsp1 = Transpose(Asp) + Csp;
               SpMatrix<doublereal, 3, 3> Fsp2 = A.Transpose() + Csp;
               SpMatrix<doublereal, 3, 3> Fsp3 = Transpose(Asp) + C;
               SpMatrix<doublereal, 3, 3> Fsp4{A.Transpose() + C};

               Mat3x3 G = A.Transpose() - C;
               SpMatrix<doublereal, 3, 3> Gsp1 = Transpose(Asp) - Csp;
               SpMatrix<doublereal, 3, 3> Gsp2 = A.Transpose() - Csp;
               SpMatrix<doublereal, 3, 3> Gsp3 = Transpose(Asp) - C;
               SpMatrix<doublereal, 3, 3> Gsp4{A.Transpose() - C};

               Mat3x3 H = A.Transpose() + C;
               SpMatrix<doublereal, 3, 3> Hsp1 = Transpose(Asp) + Csp;
               SpMatrix<doublereal, 3, 3> Hsp2 = A.Transpose() + Csp;
               SpMatrix<doublereal, 3, 3> Hsp3 = Transpose(Asp) + C;
               SpMatrix<doublereal, 3, 3> Hsp4{A.Transpose() + C};

               Mat3x3 I = A.Transpose() * d;
               SpMatrix<doublereal, 3, 3> Isp1 = Transpose(Asp) * d;
               SpMatrix<doublereal, 3, 3> Isp2{A.Transpose() * d};
               SpMatrix<doublereal, 3, 3> Isp3 = d * Transpose(Asp);

               Mat3x3 J = C.Transpose() * (A.Transpose() * 5. * (C * 0.5 + b.Tens(b) / b.Dot() * 2.5)).Transpose() * (3. / 2.) * C;
               SpMatrix<doublereal, 3, 3> Jsp1 = Transpose(Csp) * Transpose(5. * Transpose(Asp) * (Csp * 0.5 + bsp * Transpose(bsp) / Dot(bsp, bsp) * 2.5)) * (3. / 2.) * Csp;
               SpMatrix<doublereal, 3, 3> Jsp2 = C.Transpose() * Transpose(5. * Transpose(Asp) * (Csp * 0.5 + bsp * Transpose(bsp) / Dot(bsp, bsp) * 2.5)) * (3. / 2.) * Csp;
               SpMatrix<doublereal, 3, 3> Jsp3 = Transpose(Csp) * Transpose(5. * Transpose(Asp) * (C * 0.5 + bsp * Transpose(bsp) / Dot(bsp, bsp) * 2.5)) * (3. / 2.) * Csp;
               SpMatrix<doublereal, 3, 3> Jsp4 = Transpose(Csp) * Transpose(5. * Transpose(Asp) * (Csp * 0.5 + bsp * Transpose(bsp) / Dot(bsp, bsp) * 2.5)) * (3. / 2.) * C;
               SpMatrix<doublereal, 3, 3> Jsp5 = Transpose(Csp) * Transpose(A.Transpose() * 5. * (Csp * 0.5 + bsp * Transpose(bsp) / Dot(bsp, bsp) * 2.5)) * (3. / 2.) * Csp;
               SpMatrix<doublereal, 3, 3> Jsp6 = Transpose(Csp) * Transpose(5. * Transpose(Asp) * (Csp * 0.5 + b * Transpose(bsp) / Dot(bsp, bsp) * 2.5)) * (3. / 2.) * Csp;
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC> Jsp7 = Transpose(Cnn) * Transpose(5. * Transpose(Ann) * (Cnn * 0.5 + bspn * Transpose(bspn) / Dot(bspn, bspn) * 2.5)) * (3. / 2.) * Cnn;

               Mat3x3 K = A.Transpose() / d;
               SpMatrix<doublereal, 3, 3> Ksp1 = Transpose(Asp) / d;
               SpMatrix<doublereal, 3, 3> Ksp2{A.Transpose() / d};

               SpMatrix<doublereal, 3, 3> L = b.Tens();
               SpMatrix<doublereal, 3, 3> Lsp1 = bsp * Transpose(bsp);
               SpMatrix<doublereal, SpMatrixSize::DYNAMIC, SpMatrixSize::DYNAMIC> Lsp2 = bspn * Transpose(bspn);

               sp_grad_assert_equal(nx.dGetValue(), 1.0, dTol);

               for (index_type i = 1; i <= 2; ++i) {
                    for (index_type j = 1; j <= 2; ++j) {
                         sp_grad_assert_equal(InvE2x2_E2x2(i, j), ::Eye3(i, j), dTol);
                    }
               }

               X3a = X1 + X2;
               X3b = X1;
               X3b += X2;

               SpMatrix<SpGradient, 3, 3> R1a = MatRVec(X8a);
               SpMatrix<SpGradient, 3, 3> G1a = MatGVec(X8a);
               SpMatrix<SpGradient, 3, 3> Eye1a = Transpose(R1a) * R1a;

               SpMatrix<SpGradient, 3, 3> R1b = MatRVec(X8b);
               SpMatrix<SpGradient, 3, 3> G1b = MatGVec(X8b);
               SpMatrix<SpGradient, 3, 3> Eye1b = R1b * Transpose(R1b);

               X4a = X1 * 2. + X2 / 3.;

               SpMatrix<SpGradient, 3, 3> R9a = MatRVec(A2x3gf1);
               SpMatrix<SpGradient, 3, 3> G9a = MatGVec(A2x3gf1);
               SpMatrix<SpGradient, 3, 3> Eye9a = Transpose(R9a) * R9a;

               for (index_type i = 1; i <= 3; ++i) {
                    for (index_type j = 1; j <= 3; ++j) {
                         SpGradient deltaij(i == j ? 1. : 0.);
                         sp_grad_assert_equal(Eye1a(i, j), deltaij, dTol);
                         sp_grad_assert_equal(Eye1b(i, j), deltaij, dTol);
                         sp_grad_assert_equal(Eye9a(i, j), deltaij, dTol);
                    }
               }

               X4b = X1;
               X4b *= 2.;
               X4b += X2 / 3.;
               X5a = (X1 + X2) * 3. + (X3a - X4a) / 4. + (X1 - X2) / 2.;
               X5b = X1;
               X5b += X2;
               X5b *= 3.;
               X5b += X3b / 4.;
               X5b -= X4b * 0.25;
               X5b += X1 * 0.5;
               X5b -= X2 / 2.;
               X6a = (X1 + X2) * 3. + (X3a - X4a) / 4. + (X1 - X2) / 2.;
               X6b = X1;
               X6b += X2;
               X6b *= 3.;
               X6b += X3b / 4.;
               X6b -= X4b * 0.25;
               X6b += X1 * 0.5;
               X6b -= X2 / 2.;
               X7a = (X1 + X2) * 3. + (X3a - X4a) / 4. + (X1 - X2) / 2.;
               X7b = X1;
               X7b += X2;
               X7b *= 3.;
               X7b += X3b / 4.;
               X7b -= X4b * 0.25;
               X7b += X1 * 0.5;
               X7b -= X2 / 2.;

               X8a = (X1.GetCol(3) + X2.GetCol(3)) * 3. + (X3a.GetCol(3) - X4a.GetCol(3)) / 4. + (X1.GetCol(3) - X2.GetCol(3)) / 2.;

               X8b = X1.GetCol(3);
               X8b += X2.GetCol(3);
               X8b *= 3.;
               X8b += X3b.GetCol(3) / 4.;
               X8b -= X4b.GetCol(3) * 0.25;
               X8b += X1.GetCol(3) * 0.5;
               X8b -= X2.GetCol(3) / 2.;

               X9a = (Transpose(X1.GetCol(3)) + Transpose(X2.GetCol(3))) * 3. + (Transpose(X3a.GetCol(3)) - Transpose(X4a.GetCol(3))) / 4. + Transpose(X1.GetCol(3) - X2.GetCol(3)) / 2.;

               X9b = Transpose(X1.GetCol(3));
               X9b += Transpose(X2.GetCol(3));
               X9b *= 3.;
               X9b += Transpose(X3b.GetCol(3) / 4.);
               X9b -= Transpose(X4b.GetCol(3) * 0.25);
               X9b += Transpose(X1.GetCol(3)) * 0.5;
               X9b -= Transpose(X2.GetCol(3)) / 2.;

               X12a = Transpose(X13a) * 3.;
               X12b = Transpose(X13a);
               X12b += Transpose(X13a) * 2.;
               X12c = Transpose(X13a) / Norm(X13a.GetCol(1));
               X12c *= 3. * Norm(X13a.GetCol(1));
               X12d += Transpose(Transpose(X13a) / Norm(X13a.GetCol(1)));
               X12d *= 3. * Norm(X13a.GetCol(1));

               X14a = Transpose(X13a.GetCol(1) + X13a.GetCol(2));

               for (index_type i = 1; i <= X13a.iGetNumCols(); ++i) {
                    X14b += Transpose(X13a.GetCol(i));
               }

               X15a = Transpose(X1) * X2 + X3a * Transpose(X4a) - X5a / Norm(X5a.GetCol(1)) + X2 * Transpose(X2);
               X15b += Transpose(X1) * X2;
               X15b += X3a * Transpose(X4a);
               X15b -= X5a / Norm(X5a.GetCol(1));
               X15b += X2 * Transpose(X2);

               for (index_type i = 0; i < 10; ++i) {
                    SpMatrix<SpGradient, 3, 3> X3c{X3a};
                    SpGradient X3a11 = X3c(1, 1), X3a0;

                    X3a11 *= 3.;
                    X3a11 += X3a0;
                    X3a0 += X3a11;

                    {
                         SpGradient X3a01 = std::move(X3a0);
                         SpGradient X3a02 = std::move(X3a01);
                         X3a0 = std::move(X3a02);
                         X3a11 = X3a0;
                         X3a11 = X3c(1, 1);
                         X3a01 = std::move(X3a11);
                         X3a02 = std::move(X3a01);
                         X3a11 = std::move(X3a02);
                    }
                    SpMatrix<SpGradient, 3, 3> X3d = std::move(X3c);
                    SpMatrix<SpGradient, 3, 3> X3e = std::move(X3d);
                    SpGradient X3a03 = X3e(1, 1);
                    SpGradient X3a04 = X3a03 + X3a11;
                    SpGradient X3a05 = std::move(X3a04);
                    SpGradient X3a07 = std::move(X3e(1,1)) + std::move(X3e(2,2));
                    SpGradient X3a08 = X3e(1,1) + X3e(2,2) + X3e(3,3);
                    SpGradient X3a09 = std::move(X3e(1, 1));
                    {
                         SpGradient X3e10 = std::move(X3a09);
                         X3a09 = std::move(X3e10);
                    }
                    X3e(1,1) = std::move(X3a09);
                    X3e(2,2) = std::move(X3a05);
                    X3e(3,3) += X3e(1, 1) + X3e(2, 2) + 2. * X3e(3, 3);
                    X3a07 = std::move(X3a08);
                    SpMatrix<SpGradient, 3, 3> X3f = X3e;
                    X3e += X3f;
                    X3f = std::move(X3e);
                    {
                         SpGradient X3e11;
                         X3a07 += X3e11;
                         X3e11 *= 10.;
                         X3e11 += 10.;
                         X3a07 = std::move(X3e11);
                    }
                    X3e(1,1) = std::move(X3a07);

                    {
                         std::vector<SpGradient> rgVec;

                         rgVec.reserve(10);

                         for (index_type i = 1; i <= 3; ++i) {
                              for (index_type j = 1; j <= 3; ++j) {
                                   rgVec.emplace_back(std::move(X3e(i,j)));
                              }
                         }

                         rgVec.push_back(rgVec.front());
                         rgVec.push_back(rgVec.back());
                         rgVec.push_back(X3e(1, 1));
                         rgVec.emplace_back(X3e(3, 2));
                         rgVec.push_back(X3e(1, 1) + X3e(2,2));
                         rgVec.emplace_back(X3e(1, 1) + X3e(2,2));
                         X3e(3, 3) = std::move(rgVec.front());
                         X3e(1, 2) = std::move(rgVec[3]);
                    }
               }
               for (index_type i = 1; i <= 3; ++i) {
                    for (index_type j = 1; j <= 3; ++j) {
                         X10a(i, j) = (X1(i, j) + X2(i, j)) * 3. + (X3a(i, j) - X4a(i, j)) / 4. + (X1(i, j) - X2(i, j)) / 2.;
                         X10b(i, j) = X1(i, j);
                         X10b(i, j) += X2(i, j);
                         X10b(i, j) *= 3.;
                         X10b(i, j) += X3b(i, j) / 4.;
                         X10b(i, j) -= X4b(i, j) * 0.25;
                         X10b(i, j) += X1(i, j) * 0.5;
                         X10b(i, j) -= X2(i, j) / 2.;

                         X11a(i, j) = (X1(i, j) + X2(i, j)) * 3. + (X3a(i, j) - X4a(i, j)) / 4. + (X1(i, j) - X2(i, j)) / 2.;
                         X11b(i, j) = X1(i, j);
                         X11b(i, j) += X2(i, j);
                         X11b(i, j) *= 3.;
                         X11b(i, j) += X3b(i, j) / 4.;
                         X11b(i, j) -= X4b(i, j) * 0.25;
                         X11b(i, j) += X1(i, j) * 0.5;
                         X11b(i, j) -= X2(i, j) / 2.;
                    }
               }

               for (index_type i = 1; i <= 3; ++i) {
                    sp_grad_assert_equal(csp1(i), c(i), dTol);
                    sp_grad_assert_equal(csp2(i), c(i), dTol);
                    sp_grad_assert_equal(csp3(i), c(i), dTol);
                    sp_grad_assert_equal(csp4(i), c(i), dTol);
                    sp_grad_assert_equal(q(i), 4.5 * c(i), dTol);
                    sp_grad_assert_equal(gsp2(i), gsp(i), dTol);
                    sp_grad_assert_equal(gsp2(i), g(i), dTol);
                    sp_grad_assert_equal(gsp4(i), g(i), dTol);
                    sp_grad_assert_equal(gsp5(i), g(i), dTol);

                    for (index_type j = 1; j <= 3; ++j) {
                         sp_grad_assert_equal(Dsp1(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp2(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp3(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp4(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp5(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp6(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp7(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp8(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp9(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp10(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp11(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp12(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp13(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp14(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp15(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp16(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp17(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp18(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp19(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp20(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp21(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp22(i, j), D(i, j), dTol);
                         sp_grad_assert_equal(Dsp23(i, j), D(i, j), dTol);

                         sp_grad_assert_equal(Esp1(i, j), E(i, j), dTol);
                         sp_grad_assert_equal(Esp2(i, j), E(i, j), dTol);
                         sp_grad_assert_equal(Esp3(i, j), E(i, j), dTol);
                         sp_grad_assert_equal(Esp4(i, j), E(i, j), dTol);
                         sp_grad_assert_equal(Esp5(i, j), E(i, j), dTol);
                         sp_grad_assert_equal(Esp6(i, j), E(i, j), dTol);
                         sp_grad_assert_equal(Esp7(i, j), E(i, j), dTol);
                         sp_grad_assert_equal(Esp8(i, j), E(i, j), dTol);
                         sp_grad_assert_equal(Esp9(i, j), E(i, j), dTol);
                         sp_grad_assert_equal(Esp10(i, j), E(i, j), dTol);

                         sp_grad_assert_equal(Fsp1(i, j), F(i, j), dTol);
                         sp_grad_assert_equal(Fsp2(i, j), F(i, j), dTol);
                         sp_grad_assert_equal(Fsp3(i, j), F(i, j), dTol);
                         sp_grad_assert_equal(Fsp4(i, j), F(i, j), dTol);

                         sp_grad_assert_equal(Gsp1(i, j), G(i, j), dTol);
                         sp_grad_assert_equal(Gsp2(i, j), G(i, j), dTol);
                         sp_grad_assert_equal(Gsp3(i, j), G(i, j), dTol);
                         sp_grad_assert_equal(Gsp4(i, j), G(i, j), dTol);

                         sp_grad_assert_equal(Hsp1(i, j), H(i, j), dTol);
                         sp_grad_assert_equal(Hsp2(i, j), H(i, j), dTol);
                         sp_grad_assert_equal(Hsp3(i, j), H(i, j), dTol);
                         sp_grad_assert_equal(Hsp4(i, j), H(i, j), dTol);

                         sp_grad_assert_equal(Isp1(i, j), I(i, j), dTol);
                         sp_grad_assert_equal(Isp2(i, j), I(i, j), dTol);
                         sp_grad_assert_equal(Isp3(i, j), I(i, j), dTol);

                         sp_grad_assert_equal(Jsp1(i, j), J(i, j), dTol);
                         sp_grad_assert_equal(Jsp2(i, j), J(i, j), dTol);
                         sp_grad_assert_equal(Jsp3(i, j), J(i, j), dTol);
                         sp_grad_assert_equal(Jsp4(i, j), J(i, j), dTol);
                         sp_grad_assert_equal(Jsp5(i, j), J(i, j), dTol);
                         sp_grad_assert_equal(Jsp6(i, j), J(i, j), dTol);
                         sp_grad_assert_equal(Jsp7(i, j), J(i, j), dTol);

                         sp_grad_assert_equal(Ksp1(i, j), K(i, j), dTol);
                         sp_grad_assert_equal(Ksp2(i, j), K(i, j), dTol);

                         sp_grad_assert_equal(Lsp1(i, j), L(i, j), dTol);

                         sp_grad_assert_equal(RDeltasp(i, j), RDelta(i, j), dTol);
                         sp_grad_assert_equal(RVecsp(i, j), RVec(i, j), dTol);
                         sp_grad_assert_equal(Asp4(i, j), A(i, j), dTol);
                         sp_grad_assert_equal(Asp4(i, j), A4(i, j), dTol);
                         sp_grad_assert_equal(X3a(i, j), X3b(i, j), dTol);
                         sp_grad_assert_equal(X4a(i, j), X4b(i, j), dTol);
                         sp_grad_assert_equal(X5b(i, j), X5a(i, j), dTol);
                         sp_grad_assert_equal(X6a(i, j), X5a(i, j), dTol);
                         sp_grad_assert_equal(X6b(i, j), X5a(i, j), dTol);
                         sp_grad_assert_equal(X7a(i, j), X5a(i, j), dTol);
                         sp_grad_assert_equal(X7b(i, j), X5a(i, j), dTol);
                         sp_grad_assert_equal(X10a(i, j), X5a(i, j), dTol);
                         sp_grad_assert_equal(X10b(i, j), X5a(i, j), dTol);
                         sp_grad_assert_equal(X11a(i, j), X5a(i, j), dTol);
                         sp_grad_assert_equal(X11b(i, j), X5a(i, j), dTol);
                         sp_grad_assert_equal(Asp5(i, j), A(i, j), dTol);
                         sp_grad_assert_equal(X15b(i, j), X15a(i, j), dTol);
                    }
                    sp_grad_assert_equal(X8a(i), X5a(i, 3), dTol);
                    sp_grad_assert_equal(X8b(i), X5a(i, 3), dTol);
                    sp_grad_assert_equal(X9a(i), X5a(i, 3), dTol);
                    sp_grad_assert_equal(X9b(i), X5a(i, 3), dTol);
                    sp_grad_assert_equal(X14b(i), X14a(i), dTol);
                    sp_grad_assert_equal(X14a(i), X13a(i, 1) + X13a(i, 2), dTol);
               }

               for (index_type i = 1; i <= X12a.iGetNumRows(); ++i) {
                    for (index_type j = 1; j <= X12a.iGetNumCols(); ++j) {
                         sp_grad_assert_equal(X12b(i, j), X12a(i, j), dTol);
                         sp_grad_assert_equal(X12c(i, j), X12a(i, j), dTol);
                         sp_grad_assert_equal(X12d(j, i), X12a(i, j), dTol);
                    }
               }
          }


     }

     template <typename T, sp_grad::index_type N>
     void testInv(const index_type inumloops, const index_type inumdof, const index_type inumnz) {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-1., 1.);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          SpGradExpDofMapHelper<T> oDofMap;

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               SpMatrixA<T, N, N> A;

               for (index_type j = 1; j <= A.iGetNumCols(); ++j) {
                    for (index_type i = 1; i <= A.iGetNumRows(); ++i) {
                         sp_grad_rand_gen(A(i, j), randnz, randdof, randval, gen);
                    }
               }

               oDofMap.ResetDofStat();
               oDofMap.GetDofStat(A);
               oDofMap.Reset();
               oDofMap.InsertDof(A);
               oDofMap.InsertDone();

               const SpMatrix<T, N, N> B = Transpose(A) + A;

               const SpMatrix<T, N, N> BMap(Transpose(A) + A, oDofMap);

               const SpMatrixA<T, N, N> invA = Inv(A);
               SpMatrixA<T, N, N> invA2, invB2, invAMap, invBMap;
               T detA, detB2, detAMap, detBMap;

               Inv(A, invA2, detA);
               InvSymm(B, invB2, detB2);
               Inv(A, invAMap, detAMap, oDofMap);
               InvSymm(B, invBMap, detBMap, oDofMap);

               SpMatrix<T, N, N> I1 = invA * A;
               SpMatrix<T, N, N> I2 = A * invA;
               SpMatrix<T, N, N> I3 = invA2 * A;
               SpMatrix<T, N, N> I4 = A * invA2;
               SpMatrix<T, N, N> I5 = invB2 * B;
               SpMatrix<T, N, N> I6 = B * invB2;
               SpMatrix<T, N, N> I1Map(invAMap * A);
               SpMatrix<T, N, N> I2Map(A * invA, oDofMap);
               SpMatrix<T, N, N> I3Map(invA2 * A, oDofMap);
               SpMatrix<T, N, N> I4Map(A * invA2, oDofMap);
               SpMatrix<T, N, N> I5Map(invB2 * B, oDofMap);
               SpMatrix<T, N, N> I6Map(B * invB2, oDofMap);

               const doublereal dTol = sqrt(std::numeric_limits<doublereal>::epsilon());

               for (index_type j = 1; j <= N; ++j) {
                    for (index_type i = 1; i <= N; ++i) {
                         T deltaij{(i == j) ? 1. : 0.};

                         sp_grad_assert_equal(I1(i, j), deltaij, dTol);
                         sp_grad_assert_equal(I2(i, j), deltaij, dTol);
                         sp_grad_assert_equal(I3(i, j), deltaij, dTol);
                         sp_grad_assert_equal(I4(i, j), deltaij, dTol);
                         sp_grad_assert_equal(I5(i, j), deltaij, dTol);
                         sp_grad_assert_equal(I6(i, j), deltaij, dTol);

                         sp_grad_assert_equal(I1Map(i, j), deltaij, dTol);
                         sp_grad_assert_equal(I2Map(i, j), deltaij, dTol);
                         sp_grad_assert_equal(I3Map(i, j), deltaij, dTol);
                         sp_grad_assert_equal(I4Map(i, j), deltaij, dTol);
                         sp_grad_assert_equal(I5Map(i, j), deltaij, dTol);
                         sp_grad_assert_equal(I6Map(i, j), deltaij, dTol);
                    }
               }
          }
     }

     template <typename T>
     SpColVector<T, 2> func_grad_prod0(const SpColVector<T, 2>& X) {
          return SpColVector<T, 2>{sin(2. * X(1) - X(2) * 3.) * exp(X(1) / X(2)) + sqrt(X(1)), 4. * cos(X(1) * X(2)) * pow(X(1), X(2))};
     }

     void test0gp()
     {
          using namespace sp_grad;

          constexpr index_type N = 2;

          SpColVector<doublereal, N> X{0.5, 0.3}, Y{-0.7, 0.1};
          SpColVectorA<GpGradProd, N> Xg;

          for (index_type i = 1; i <= N; ++i) {
               Xg(i).Reset(X(i), Y(i));
          }

          SpColVectorA<doublereal, N> F = func_grad_prod0(X);

          SpColVectorA<GpGradProd, N> Fg = func_grad_prod0(Xg);

          const doublereal dF1_dX1 = 2. * cos(2. * X(1) - 3. * X(2)) * exp(X(1) / X(2)) + sin(2 * X(1) - X(2) * 3) * exp(X(1) / X(2)) / X(2) + 1. / (2. * sqrt(X(1)));
          const doublereal dF1_dX2 = -3. * cos(2. * X(1) - 3. * X(2)) * exp(X(1) / X(2)) - sin(2. * X(1) - X(2) * 3.) * exp(X(1) / X(2)) * X(1) / (X(2) * X(2));
          const doublereal dF2_dX1 = -4. * sin(X(1) * X(2)) * X(2) * pow(X(1), X(2)) + 4. * cos(X(1) * X(2)) * X(2) * pow(X(1), X(2) - 1.);
          const doublereal dF2_dX2 = -4. * sin(X(1) * X(2)) * X(1) * pow(X(1), X(2)) + 4. * cos(X(1) * X(2)) * pow(X(1), X(2)) * log(X(1));

          SpMatrixA<doublereal, N, N> J = {dF1_dX1, dF2_dX1, dF1_dX2, dF2_dX2};

          SpColVector<doublereal, N> JY = J * Y;

          const doublereal dTol = std::pow(std::numeric_limits<doublereal>::epsilon(), 0.9);

          for (index_type i = 1; i <= N; ++i) {
               sp_grad_assert_equal(Fg(i).dGetValue(), F(i), dTol);
               sp_grad_assert_equal(Fg(i).dGetDeriv(), JY(i), dTol);
          }

          SpColVector<GpGradProd, N> Ag = J * Fg;
          SpColVector<GpGradProd, N> ATg = Transpose(J) * Fg;
          SpColVector<doublereal, N> A = J * F;
          SpColVector<doublereal, N> AT = Transpose(J) * F;
          SpColVectorA<doublereal, N> Fd;

          for (index_type i = 1; i <= N; ++i) {
               Fd(i) = Fg(i).dGetDeriv();
          }

          SpColVector<doublereal, N> Ad = J * Fd;
          SpColVector<doublereal, N> ATd = Transpose(J) * Fd;

          SpMatrix<GpGradProd, N, N> FFTg= (Inv(J) * Fg * Transpose(J * Fg) * Transpose(J) + 3. * Fg * Transpose(Fg)) / 2.5 * (Ag(1) + Ag(2));
          SpMatrix<GpGradProd, N, N> FFTTg= Transpose(Inv(J) * Fg * Transpose(J * Fg) * Transpose(J) + 3. * Fg * Transpose(Fg)) / 2.5 * (Ag(1) + Ag(2));
          SpMatrix<doublereal, N, N> FFT = (Inv(J) * F * Transpose(J * F) * Transpose(J) + 3. * F * Transpose(F)) / 2.5 * (A(1) + A(2));
          SpMatrix<doublereal, N, N> FFTd = (Inv(J) * (F * Transpose(J * Fd) + Fd * Transpose(J * F)) * Transpose(J) + 3. * (Fd * Transpose(F) + F * Transpose(Fd))) / 2.5 * (A(1) + A(2))
                                          + (Inv(J) * F * Transpose(J * F) * Transpose(J) + 3. * F * Transpose(F)) / 2.5 * (Ad(1) + Ad(2));

          for (index_type i = 1; i <= N; ++i) {
               for (index_type j = 1; j <= N; ++j) {
                    sp_grad_assert_equal(FFTg(i, j).dGetValue(), FFT(i, j), dTol);
                    sp_grad_assert_equal(FFTg(i, j).dGetDeriv(), FFTd(i, j), dTol);
                    sp_grad_assert_equal(FFTTg(j, i).dGetValue(), FFT(i, j), dTol);
                    sp_grad_assert_equal(FFTTg(j, i).dGetDeriv(), FFTd(i, j), dTol);
               }
          }

          FFTg *= 0.75;
          FFTTg *= 0.75;
          FFT *= 0.75;
          FFTd *= 0.75;

          for (index_type i = 1; i <= N; ++i) {
               for (index_type j = 1; j <= N; ++j) {
                    sp_grad_assert_equal(FFTg(i, j).dGetValue(), FFT(i, j), dTol);
                    sp_grad_assert_equal(FFTg(i, j).dGetDeriv(), FFTd(i, j), dTol);
                    sp_grad_assert_equal(FFTTg(j, i).dGetValue(), FFT(i, j), dTol);
                    sp_grad_assert_equal(FFTTg(j, i).dGetDeriv(), FFTd(i, j), dTol);
               }
          }

          FFTg /= 11.5;
          FFTTg /= 11.5;
          FFT /= 11.5;
          FFTd /= 11.5;

          for (index_type i = 1; i <= N; ++i) {
               for (index_type j = 1; j <= N; ++j) {
                    sp_grad_assert_equal(FFTg(i, j).dGetValue(), FFT(i, j), dTol);
                    sp_grad_assert_equal(FFTg(i, j).dGetDeriv(), FFTd(i, j), dTol);
                    sp_grad_assert_equal(FFTTg(j, i).dGetValue(), FFT(i, j), dTol);
                    sp_grad_assert_equal(FFTTg(j, i).dGetDeriv(), FFTd(i, j), dTol);
               }
          }

          FFTg *= Dot(Ag, Ag);
          FFTTg *= Dot(Ag, Ag);

          SpMatrix<doublereal, N, N> FFTdTmp = FFTd * Dot(A, A);

          FFTd = 2. * FFT * Dot(Ad, A) + FFTdTmp;
          FFT *= Dot(A, A);

          for (index_type i = 1; i <= N; ++i) {
               for (index_type j = 1; j <= N; ++j) {
                    sp_grad_assert_equal(FFTg(i, j).dGetValue(), FFT(i, j), dTol);
                    sp_grad_assert_equal(FFTg(i, j).dGetDeriv(), FFTd(i, j), dTol);
                    sp_grad_assert_equal(FFTTg(j, i).dGetValue(), FFT(i, j), dTol);
                    sp_grad_assert_equal(FFTTg(j, i).dGetDeriv(), FFTd(i, j), dTol);
               }
          }

          FFTg /= Dot(ATg, ATg);
          FFTTg /= Dot(ATg, ATg);
          FFTdTmp = FFTd / Dot(AT, AT);
          FFTd = FFTdTmp - FFT / pow(Dot(AT, AT), 2) * 2. * Dot(ATd, AT);
          FFT /= Dot(AT, AT);

          for (index_type i = 1; i <= N; ++i) {
               for (index_type j = 1; j <= N; ++j) {
                    sp_grad_assert_equal(FFTg(i, j).dGetValue(), FFT(i, j), dTol);
                    sp_grad_assert_equal(FFTg(i, j).dGetDeriv(), FFTd(i, j), dTol);
                    sp_grad_assert_equal(FFTTg(j, i).dGetValue(), FFT(i, j), dTol);
                    sp_grad_assert_equal(FFTTg(j, i).dGetDeriv(), FFTd(i, j), dTol);
               }
          }

          FFTg *= ATg(1) + ATg(2);
          FFTTg *= ATg(1) + ATg(2);
          FFTdTmp = FFTd * (AT(1) + AT(2));
          FFTd = FFT * (ATd(1) + ATd(2)) + FFTdTmp;
          FFT *= AT(1) + AT(2);

          for (index_type i = 1; i <= N; ++i) {
               for (index_type j = 1; j <= N; ++j) {
                    sp_grad_assert_equal(FFTg(i, j).dGetValue(), FFT(i, j), dTol);
                    sp_grad_assert_equal(FFTg(i, j).dGetDeriv(), FFTd(i, j), dTol);
                    sp_grad_assert_equal(FFTTg(j, i).dGetValue(), FFT(i, j), dTol);
                    sp_grad_assert_equal(FFTTg(j, i).dGetDeriv(), FFTd(i, j), dTol);
               }
          }

          FFTg /= ATg(1) + ATg(2);
          FFTTg /= ATg(1) + ATg(2);
          FFTdTmp = FFTd / (AT(1) + AT(2));
          FFTd = -FFT / pow(AT(1) + AT(2), 2) * (ATd(1) + ATd(2)) + FFTdTmp;
          FFT /= AT(1) + AT(2);

          std::vector<doublereal> a(N);
          std::vector<GpGradProd> b(N);
          std::vector<GpGradProd> c(N);

          for (index_type i = 0; i < N; ++i) {
               a[i] = (i + 1) / 5.;
               b[i].Reset(3. / (i + 1), 10. / (i + 1));
               c[i].Reset((i + 1.) / 4., (i + 1) / 2.);
          }

          GpGradProd adotb, bdotc;

          adotb.MapInnerProduct(a.begin(), a.end(), 1L, b.begin(), b.end(), 1L);
          bdotc.MapInnerProduct(b.begin(), b.end(), 1L, c.begin(), c.end(), 1L);

          sp_grad_assert_equal(adotb.dGetValue(), 3. / 5. * N, dTol);
          sp_grad_assert_equal(adotb.dGetDeriv(), 10. / 5. * N, dTol);
          sp_grad_assert_equal(bdotc.dGetValue(), 3. / 4. * N, dTol);
          sp_grad_assert_equal(bdotc.dGetDeriv(), (10. / 4. + 3. / 2.) * N, dTol);

          for (index_type i = 1; i <= N; ++i) {
               sp_grad_assert_equal(Ag(i).dGetValue(), A(i), dTol);
               sp_grad_assert_equal(Ag(i).dGetDeriv(), Ad(i), dTol);
               sp_grad_assert_equal(ATg(i).dGetValue(), AT(i), dTol);
               sp_grad_assert_equal(ATg(i).dGetDeriv(), ATd(i), dTol);

               for (index_type j = 1; j <= N; ++j) {
                    sp_grad_assert_equal(FFTg(i, j).dGetValue(), FFT(i, j), dTol);
                    sp_grad_assert_equal(FFTg(i, j).dGetDeriv(), FFTd(i, j), dTol);
                    sp_grad_assert_equal(FFTTg(j, i).dGetValue(), FFT(i, j), dTol);
                    sp_grad_assert_equal(FFTTg(j, i).dGetDeriv(), FFTd(i, j), dTol);
               }
          }
     }

     void test1()
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";
          vector<SpDerivRec> v{{3, 3.2}, {4, 4.2}, {5, 5.2}, {6, 6.2}, {12, 12.2}, {13, 13.2}, {15, 15.2}, {14, 14.2}};
          SpGradient g1(10., {{13, 13.1}, {1, 1.1}, {2, 2.1}, {3, 3.1}, {5, 5.1}, {6, 6.1}, {10, 10.1}, {12, 12.1}});
          SpGradient g2, g3(20., v), g4, g5;

          g2.Reset(20., {{3, 3.2}, {4, 4.2}, {5, 5.2}, {6, 6.2}, {12, 12.2}, {13, 13.2}, {15, 15.2}, {14, 14.2}});
          g4.Reset(g2.dGetValue(), 3, g2.dGetDeriv(3));
#ifdef SP_GRAD_DEBUG
          cout << "g1=" << g1 << endl;
          cout << "g2=" << g2 << endl;
          cout << "g3=" << g3 << endl;
          cout << "g4=" << g4 << endl;
#endif
          SpGradient r1 = pow(2. * (g1 + g2), 1e-2 * g1);
          SpGradient r2 = -cos(r1) * sin(g2 - g1 / (1. - sqrt(pow(g1, 2) + pow(g2, 2))));
          SpGradient r3 = r2, r4, r5, r6;

          SpGradient a{1.0, {{1, 10.}}};
          SpGradient b{2.0, {{1, 20.}}};
          SpGradient f = sqrt(a + b) / (a - b);

#ifdef SP_GRAD_DEBUG
          cout << f << endl;
#endif
          r4 = r1;
          r5 = r4;
          r6 = std::move(r4);

          SpGradient r7(std::move(r5));
          SpGradient r8(r7);

          r1 = r8;

          cout << fixed << std::setprecision(3);
#ifdef SP_GRAD_DEBUG
          cout << "r=" << r1 << endl;

          cout << "g1=" << g1 << endl;
          cout << "g2=" << g2 << endl;
#endif
          constexpr int w = 8;

          cout << "|" << setw(w) << left << "idof"
               << "|" << setw(w) << left << "g1"
               << "|" << setw(w) << left << "g2"
               << "|" << setw(w) << left << "r1"
               << "|" << setw(w) << left << "r2"
               << "|" << endl;

          cout << "+" << setw(w) << setfill('-') << '-'
               << "+" << setw(w) << setfill('-') << '-'
               << "+" << setw(w) << setfill('-') << '-'
               << "+" << setw(w) << setfill('-') << '-'
               << "+" << setw(w) << setfill('-') << '-'
               << "+" << endl << setfill(' ');

          cout << "|" << setw(w) << right << "#"
               << "|" << setw(w) << right << g1.dGetValue()
               << "|" << setw(w) << right << g2.dGetValue()
               << "|" << setw(w) << right << r1.dGetValue()
               << "|" << setw(w) << right << r2.dGetValue()
               << "|" << endl;

          cout << "+" << setw(w) << setfill('-') << '-'
               << "+" << setw(w) << setfill('-') << '-'
               << "+" << setw(w) << setfill('-') << '-'
               << "+" << setw(w) << setfill('-') << '-'
               << "+" << setw(w) << setfill('-') << '-'
               << "+" << endl << setfill(' ');

          for (index_type idof = 1; idof <= 15; ++idof) {
               cout << "|" << setw(w) << right << idof
                    << "|" << setw(w) << right << g1.dGetDeriv(idof)
                    << "|" << setw(w) << right << g2.dGetDeriv(idof)
                    << "|" << setw(w) << right << r1.dGetDeriv(idof)
                    << "|" << setw(w) << right << r2.dGetDeriv(idof)
                    << "|" << endl;
          }
     }

     void test2(index_type inumloops, index_type inumnz, index_type inumdof)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-1., 1.);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          SpGradient u, v, w, f, fc, f2;

          doublereal e = randval(gen), fVal;
          index_type unz = 0, vnz = 0, wnz = 0;
          index_type fnz = 0, f2nz = 0, funz = 0, f2unz = 0, fcnz = 0;
          const doublereal dTol = pow(numeric_limits<doublereal>::epsilon(), 0.5);
          vector<doublereal> ud, vd, wd, fd, work;
          duration<long long, ratio<1L, 1000000000L> > sp_grad_time(0), sp_grad_compr_time(0), sp_grad2_time(0), c_full_time(0);

          // decltype(sp_grad_time) sp_grad_s_time(0);

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               sp_grad_rand_gen(u, randnz, randdof, randval, gen);
               sp_grad_rand_gen(v, randnz, randdof, randval, gen);
               sp_grad_rand_gen(w, randnz, randdof, randval, gen);

               unz += u.iGetSize();
               vnz += v.iGetSize();
               wnz += w.iGetSize();

               SpGradDofStat s;

               u.GetDofStat(s);
               v.GetDofStat(s);
               w.GetDofStat(s);

               const index_type nbdirs = s.iNumNz ? s.iMaxDof : 0;

               func_scalar1(u.dGetValue(), v.dGetValue(), w.dGetValue(), e, fVal);

               auto sp_grad_start = high_resolution_clock::now();

               func_scalar1(u, v, w, e, f);

               sp_grad_time += high_resolution_clock::now() - sp_grad_start;

               funz += f.iGetSize();

               f.Sort();

               fnz += f.iGetSize();

               auto sp_grad_compr_start = high_resolution_clock::now();

               func_scalar1_compressed(u, v, w, e, fc);

               sp_grad_compr_time += high_resolution_clock::now() - sp_grad_compr_start;

               fcnz += fc.iGetSize();

               auto sp_grad2_start = high_resolution_clock::now();

               func_scalar2(u, v, w, e, f2);

               sp_grad2_time += high_resolution_clock::now() - sp_grad2_start;

               f2unz += f2.iGetSize();

               f2.Sort();

               f2nz += f2.iGetSize();

               MBDYN_TESTSUITE_ASSERT(fabs(f.dGetValue() / fVal - 1.) < dTol);

               MBDYN_TESTSUITE_ASSERT(fabs(fc.dGetValue() / fVal - 1.) < dTol);

               MBDYN_TESTSUITE_ASSERT(fabs(f2.dGetValue() / fVal - 1.) < dTol);

               ud.clear();
               vd.clear();
               wd.clear();
               fd.clear();
               work.clear();
               ud.resize(nbdirs);
               vd.resize(nbdirs);
               wd.resize(nbdirs);
               fd.resize(nbdirs);
               work.resize(4 * nbdirs);

               for (index_type i = 1; i <= s.iMaxDof; ++i) {
                    ud[i - 1] = u.dGetDeriv(i);
                    vd[i - 1] = v.dGetDeriv(i);
                    wd[i - 1] = w.dGetDeriv(i);
               }

               auto c_full_start = high_resolution_clock::now();

               func_scalar1_dv(nbdirs,
                               u.dGetValue(),
                               front(ud),
                               v.dGetValue(),
                               front(vd),
                               w.dGetValue(),
                               front(wd),
                               e,
                               fVal,
                               front(fd),
                               front(work));

               c_full_time += high_resolution_clock::now() - c_full_start;

               MBDYN_TESTSUITE_ASSERT(fabs(f.dGetValue() - fVal) < dTol * max(1., fabs(fVal)));

               SP_GRAD_TRACE("fref f\n");
               SP_GRAD_TRACE(fVal << " " << f.dGetValue() << endl);

               for (index_type i = 1; i <= s.iMaxDof; ++i) {
                    MBDYN_TESTSUITE_ASSERT(fabs(fd[i - 1] - f.dGetDeriv(i)) < dTol * max(1.0, fabs(fd[i - 1])));
                    MBDYN_TESTSUITE_ASSERT(fabs(fd[i - 1] - fc.dGetDeriv(i)) < dTol * max(1.0, fabs(fd[i - 1])));
                    MBDYN_TESTSUITE_ASSERT(fabs(fd[i - 1] - f2.dGetDeriv(i)) < dTol * max(1.0, fabs(fd[i - 1])));
                    SP_GRAD_TRACE(fd[i - 1] << " " << f.dGetDeriv(i) << endl);
               }
          }

          auto sp_grad_time_ns = duration_cast<nanoseconds>(sp_grad_time).count();
          auto sp_grad_compr_time_ns = duration_cast<nanoseconds>(sp_grad_compr_time).count();
          auto sp_grad2_time_ns = duration_cast<nanoseconds>(sp_grad2_time).count();
          auto c_full_time_ns = duration_cast<nanoseconds>(c_full_time).count();

          cerr << "test2: test passed with tolerance "
               << scientific << setprecision(6)
               << dTol
               << " and nz=" << inumnz
               << " unz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(unz) / inumloops)
               << " vnz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(vnz) / inumloops)
               << " wnz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(wnz) / inumloops)
               << endl;

          cerr << "test2: sp_grad_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_ns) / 1e9
               << " nz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(fnz) / inumloops)
               << " unz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(funz) / inumloops)
               << endl;
          cerr << "test2: sp_grad_compressed_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_compr_time_ns) / 1e9
               << " nz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(fcnz) / inumloops)
               << endl;

          cerr << "test2: sp_grad2_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad2_time_ns) / 1e9
               << " nz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(f2nz) / inumloops)
               << " unz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(f2unz) / inumloops)
               << endl;
          cerr << "test2: c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(c_full_time_ns) / 1e9 << endl;
          cerr << "test2: sp_grad_time / c_full_time = "
               << static_cast<doublereal>(sp_grad_time_ns) / max<int64_t>(1L, c_full_time_ns) // Avoid division by zero
               << endl;
          cerr << "test2: sp_grad_compr_time / c_full_time = "
               << static_cast<doublereal>(sp_grad_compr_time_ns) / max<int64_t>(1L, c_full_time_ns) // Avoid division by zero
               << endl;
          cerr << "test2: sp_grad2_time / c_full_time = "
               << static_cast<doublereal>(sp_grad2_time_ns) / max<int64_t>(1L, c_full_time_ns) // Avoid division by zero
               << endl;
     }

     void test2gp(index_type inumloops, index_type inumnz, index_type inumdof)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-1., 1.);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          GpGradProd u, v, w, f, f2;

          doublereal e = randval(gen), fVal;
          const doublereal dTol = pow(numeric_limits<doublereal>::epsilon(), 0.5);
          vector<doublereal> ud, vd, wd, fd, work;
          duration<long long, ratio<1L, 1000000000L> > sp_grad_time(0), sp_grad2_time(0), c_full_time(0);

          // decltype(sp_grad_time) sp_grad_s_time(0);

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               sp_grad_rand_gen(u, randnz, randdof, randval, gen);
               sp_grad_rand_gen(v, randnz, randdof, randval, gen);
               sp_grad_rand_gen(w, randnz, randdof, randval, gen);

               const index_type nbdirs = 1;

               func_scalar1(u.dGetValue(), v.dGetValue(), w.dGetValue(), e, fVal);

               auto sp_grad_start = high_resolution_clock::now();

               func_scalar1(u, v, w, e, f);

               sp_grad_time += high_resolution_clock::now() - sp_grad_start;

               auto sp_grad2_start = high_resolution_clock::now();

               func_scalar2(u, v, w, e, f2);

               sp_grad2_time += high_resolution_clock::now() - sp_grad2_start;

               MBDYN_TESTSUITE_ASSERT(fabs(f.dGetValue() / fVal - 1.) < dTol);

               MBDYN_TESTSUITE_ASSERT(fabs(f2.dGetValue() / fVal - 1.) < dTol);

               ud.clear();
               vd.clear();
               wd.clear();
               fd.clear();
               work.clear();
               ud.resize(nbdirs);
               vd.resize(nbdirs);
               wd.resize(nbdirs);
               fd.resize(nbdirs);
               work.resize(4 * nbdirs);

               ud[0] = u.dGetDeriv();
               vd[0] = v.dGetDeriv();
               wd[0] = w.dGetDeriv();

               auto c_full_start = high_resolution_clock::now();

               func_scalar1_dv(nbdirs,
                               u.dGetValue(),
                               front(ud),
                               v.dGetValue(),
                               front(vd),
                               w.dGetValue(),
                               front(wd),
                               e,
                               fVal,
                               front(fd),
                               front(work));

               c_full_time += high_resolution_clock::now() - c_full_start;

               MBDYN_TESTSUITE_ASSERT(fabs(f.dGetValue() - fVal) < dTol * max(1., fabs(fVal)));

               SP_GRAD_TRACE("fref f\n");
               SP_GRAD_TRACE(fVal << " " << f.dGetValue() << endl);
               MBDYN_TESTSUITE_ASSERT(fabs(fd[0] - f.dGetDeriv()) < dTol * max(1.0, fabs(fd[0])));
               MBDYN_TESTSUITE_ASSERT(fabs(fd[0] - f2.dGetDeriv()) < dTol * max(1.0, fabs(fd[0])));
               SP_GRAD_TRACE(fd[0] << " " << f.dGetDeriv() << endl);
          }

          auto sp_grad_time_ns = duration_cast<nanoseconds>(sp_grad_time).count();
          auto sp_grad2_time_ns = duration_cast<nanoseconds>(sp_grad2_time).count();
          auto c_full_time_ns = duration_cast<nanoseconds>(c_full_time).count();

          cerr << "test2: test passed with tolerance "
               << scientific << setprecision(6)
               << dTol
               << endl;

          cerr << "test2: sp_grad_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_ns) / 1e9
               << endl;

          cerr << "test2: sp_grad2_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad2_time_ns) / 1e9
               << endl;

          cerr << "test2: c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(c_full_time_ns) / 1e9 << endl;

          cerr << "test2: sp_grad_time / c_full_time = "
               << static_cast<doublereal>(sp_grad_time_ns) / max<int64_t>(1L, c_full_time_ns) // Avoid division by zero
               << endl;

          cerr << "test2: sp_grad2_time / c_full_time = "
               << static_cast<doublereal>(sp_grad2_time_ns) / max<int64_t>(1L, c_full_time_ns) // Avoid division by zero
               << endl;
     }

     void test_bool1(index_type inumloops, index_type inumnz, index_type inumdof)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-1., 1.);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          SpGradient u, v, w;
          doublereal e = randval(gen);

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               sp_grad_rand_gen(u, randnz, randdof, randval, gen);
               sp_grad_rand_gen(v, randnz, randdof, randval, gen);
               sp_grad_rand_gen(w, randnz, randdof, randval, gen);

#define SP_GRAD_BOOL_TEST_FUNC(func)                                    \
               {                                                        \
                    bool f1 = func(u, v, w, e);                         \
                    bool f2 = func(u.dGetValue(), v.dGetValue(), w.dGetValue(), e); \
                    bool f3 = func(u, v.dGetValue(), w.dGetValue(), e); \
                    bool f4 = func(u.dGetValue(), v, w.dGetValue(), e); \
                    bool f5 = func(u.dGetValue(), v.dGetValue(), w, e); \
                    bool f6 = func(u, v, w.dGetValue(), e);             \
                    bool f7 = func(u.dGetValue(), v, w, e);             \
                    bool f8 = func(u, v.dGetValue(), w, e);             \
                                                                        \
                    MBDYN_TESTSUITE_ASSERT(f1 == f2);                                   \
                    MBDYN_TESTSUITE_ASSERT(f1 == f3);                                   \
                    MBDYN_TESTSUITE_ASSERT(f1 == f4);                                   \
                    MBDYN_TESTSUITE_ASSERT(f1 == f5);                                   \
                    MBDYN_TESTSUITE_ASSERT(f1 == f6);                                   \
                    MBDYN_TESTSUITE_ASSERT(f1 == f7);                                   \
                    MBDYN_TESTSUITE_ASSERT(f1 == f8);                                   \
               }

               SP_GRAD_BOOL_TEST_FUNC(func_bool1);
               SP_GRAD_BOOL_TEST_FUNC(func_bool2);
               SP_GRAD_BOOL_TEST_FUNC(func_bool3);
               SP_GRAD_BOOL_TEST_FUNC(func_bool4);
               SP_GRAD_BOOL_TEST_FUNC(func_bool5);
               SP_GRAD_BOOL_TEST_FUNC(func_bool6);
          }
     }

     template <typename TA, typename TB>
     void test3(const index_type inumloops, const index_type inumnz, const index_type inumdof, const index_type imatsize)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-1., 1.);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          const doublereal dTol = pow(numeric_limits<doublereal>::epsilon(), 0.5);

          vector<TA> A;
          vector<TB> B;
          vector<doublereal> Av(imatsize), Bv(imatsize), Ad, Bd, fd;
          index_type fnz = 0, f2nz = 0, funz = 0, f2unz = 0, f3nz = 0, f3unz = 0;
          index_type anz = 0, bnz = 0;

          A.resize(imatsize);
          B.resize(imatsize);

          duration<long long, ratio<1L, 1000000000L> >
               sp_grad_time(0),
               sp_grad2_time(0),
               sp_grad3_time(0),
               c_full_time(0);

          typedef typename util::ResultType<TA, TB>::Type ScalarType;
          typedef SpGradientTraits<ScalarType> TypeTraits;
          ScalarType f, f2, f3;

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               for (auto& a:A) {
                    anz += sp_grad_rand_gen(a, randnz, randdof, randval, gen);
               }

               for (auto& b:B) {
                    bnz += sp_grad_rand_gen(b, randnz, randdof, randval, gen);
               }

               auto sp_grad_start = high_resolution_clock::now();

               func_mat_mul1(f, A, B);

               sp_grad_time += high_resolution_clock::now() - sp_grad_start;

               funz += TypeTraits::iGetSize(f);

               TypeTraits::Sort(f);

               fnz += TypeTraits::iGetSize(f);

               auto sp_grad2_start = high_resolution_clock::now();

               func_mat_mul2(f2, A, B);

               sp_grad2_time += high_resolution_clock::now() - sp_grad2_start;

               f2unz += TypeTraits::iGetSize(f2);

               TypeTraits::Sort(f2);

               f2nz += TypeTraits::iGetSize(f2);

               auto sp_grad3_start = high_resolution_clock::now();

               func_mat_mul3(f3, A, B);

               sp_grad3_time += high_resolution_clock::now() - sp_grad3_start;

               f3unz += TypeTraits::iGetSize(f3);

               TypeTraits::Sort(f3);

               f3nz += TypeTraits::iGetSize(f3);

               SpGradDofStat s;

               for (const auto& a: A) {
                    SpGradientTraits<TA>::GetDofStat(a, s);
               }

               for (const auto& b: B) {
                    SpGradientTraits<TB>::GetDofStat(b, s);
               }

               const index_type nbdirs = s.iNumNz ? s.iMaxDof : 0;
               doublereal fVal;

               Ad.resize(nbdirs * imatsize);
               Bd.resize(nbdirs * imatsize);
               fd.resize(nbdirs);

               for (index_type i = 0; i < imatsize; ++i) {
                    Av[i] = SpGradientTraits<TA>::dGetValue(A[i]);
                    Bv[i] = SpGradientTraits<TB>::dGetValue(B[i]);

                    for (index_type j = 1; j <= s.iMaxDof; ++j) {
                         Ad[i + (j - 1) * imatsize] = SpGradientTraits<TA>::dGetDeriv(A[i], j);
                         Bd[i + (j - 1) * imatsize] = SpGradientTraits<TB>::dGetDeriv(B[i], j);
                    }
               }

               auto c_full_start = high_resolution_clock::now();

               func_mat_mul1_dv(imatsize,
                                front(Av),
                                front(Ad),
                                front(Bv),
                                front(Bd),
                                fVal,
                                front(fd),
                                s.iMaxDof);

               c_full_time += high_resolution_clock::now() - c_full_start;

               MBDYN_TESTSUITE_ASSERT(fabs(TypeTraits::dGetValue(f) - fVal) < dTol * max(1., fabs(fVal)));
               MBDYN_TESTSUITE_ASSERT(fabs(TypeTraits::dGetValue(f2) - fVal) < dTol * max(1., fabs(fVal)));
               MBDYN_TESTSUITE_ASSERT(fabs(TypeTraits::dGetValue(f3) - fVal) < dTol * max(1., fabs(fVal)));

               for (index_type i = 1; i <= s.iMaxDof; ++i) {
                    MBDYN_TESTSUITE_ASSERT(fabs(fd[i - 1] - TypeTraits::dGetDeriv(f, i)) < dTol * max(1.0, fabs(fd[i - 1])));
               }

               MBDYN_TESTSUITE_ASSERT(fabs(TypeTraits::dGetValue(f2) - fVal) < dTol * max(1., fabs(fVal)));

               for (index_type i = 1; i <= s.iMaxDof; ++i) {
                    MBDYN_TESTSUITE_ASSERT(fabs(fd[i - 1] - TypeTraits::dGetDeriv(f2, i)) < dTol * max(1.0, fabs(fd[i - 1])));
               }

               MBDYN_TESTSUITE_ASSERT(fabs(TypeTraits::dGetValue(f3) - fVal) < dTol * max(1., fabs(fVal)));

               for (index_type i = 1; i <= s.iMaxDof; ++i) {
                    MBDYN_TESTSUITE_ASSERT(fabs(fd[i - 1] - TypeTraits::dGetDeriv(f3, i)) < dTol * max(1.0, fabs(fd[i - 1])));
               }
          }

          cerr << "test3: test passed with tolerance "
               << scientific << dTol
               << " and nz=" << inumnz
               << " anz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(anz) / (inumloops * imatsize))
               << " bnz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(bnz) / (inumloops * imatsize))
               << endl;

          auto sp_grad_time_ns = duration_cast<nanoseconds>(sp_grad_time).count();
          auto sp_grad2_time_ns = duration_cast<nanoseconds>(sp_grad2_time).count();
          auto sp_grad3_time_ns = duration_cast<nanoseconds>(sp_grad3_time).count();
          auto c_full_time_ns = duration_cast<nanoseconds>(c_full_time).count();

          cerr << "test3: sp_grad_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_ns) / 1e9
               << " nz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(fnz) / inumloops)
               << " unz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(funz) / inumloops) << endl;
          cerr << "test3: sp_grad2_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad2_time_ns) / 1e9
               << " nz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(f2nz) / inumloops)
               << " unz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(f2unz) / inumloops) << endl;
          cerr << "test3: sp_grad3_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad3_time_ns) / 1e9
               << " nz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(f3nz) / inumloops)
               << " unz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(f3unz) / inumloops) << endl;
          cerr << "test3: c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(c_full_time_ns) / 1e9 << endl;
          cerr << "test3: sp_grad_time / c_full_time = "
               << static_cast<doublereal>(sp_grad_time_ns) / max<int64_t>(1L, c_full_time_ns) // Avoid division by zero
               << endl;
          cerr << "test3: sp_grad2_time / c_full_time = "
               << static_cast<doublereal>(sp_grad2_time_ns) / max<int64_t>(1L, c_full_time_ns) // Avoid division by zero
               << endl;
          cerr << "test3: sp_grad3_time / c_full_time = "
               << static_cast<doublereal>(sp_grad3_time_ns) / max<int64_t>(1L, c_full_time_ns) // Avoid division by zero
               << endl;
     }

     template <typename TA, typename TX>
     void test4(const index_type inumloops,
                const index_type inumnz,
                const index_type inumdof,
                const index_type imatrows,
                const index_type imatcols)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";
          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-1., 1.);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          const doublereal dTol = pow(numeric_limits<doublereal>::epsilon(), 0.5);

          typedef typename util::ResultType<TA,TX>::Type TB;
          vector<TA> A;
          vector<TX> x;
          vector<TB> b4, b5, b6, b7;

          vector<doublereal> Av, Ad, xv, bv, xd, bd;
          index_type xnz = 0, anz = 0, b4nz = 0, b5unz = 0, b5nz = 0;

          A.resize(imatrows * imatcols);
          Av.resize(imatrows * imatcols);
          x.resize(imatcols);
          b4.resize(imatrows);
          b5.resize(imatrows);
          b6.resize(imatrows);
          b7.resize(imatrows);
          xv.resize(imatcols);
          bv.resize(imatrows);

          duration<long long, ratio<1L, 1000000000L> >
               sp_grad4_time(0),
               sp_grad4m_time(0),
               sp_grad4sp_time(0),
               sp_grad4sm_time(0),
               c_full_time(0);

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               for (auto& ai: A) {
                    sp_grad_rand_gen(ai, randnz, randdof, randval, gen);
                    anz += SpGradientTraits<TA>::iGetSize(ai);
               }

               for (auto& xi: x) {
                    sp_grad_rand_gen(xi, randnz, randdof, randval, gen);
                    xnz += SpGradientTraits<TX>::iGetSize(xi);
               }

               auto sp_grad4_start = high_resolution_clock::now();

               func_mat_mul4(A, x, b4);

               sp_grad4_time += high_resolution_clock::now() - sp_grad4_start;

               for (const auto& b4i: b4) {
                    b4nz += SpGradientTraits<TB>::iGetSize(b4i);
               }

               auto sp_grad4m_start = high_resolution_clock::now();

               func_mat_mul4m(A, x, b5);

               sp_grad4m_time += high_resolution_clock::now() - sp_grad4m_start;

               auto sp_grad4sp_start = high_resolution_clock::now();

               func_mat_mul4s(A, x, b6);

               sp_grad4sp_time += high_resolution_clock::now() - sp_grad4sp_start;

               auto sp_grad4sm_start = high_resolution_clock::now();

               func_mat_mul4sm(A, x, b7);

               sp_grad4sm_time += high_resolution_clock::now() - sp_grad4sm_start;

               for (const auto& b5i: b5) {
                    b5unz += SpGradientTraits<TB>::iGetSize(b5i);
               }

               for (auto& b5i: b5) {
                    SpGradientTraits<TB>::Sort(b5i);
               }

               for (const auto& b5i: b5) {
                    b5nz += SpGradientTraits<TB>::iGetSize(b5i);
               }

               SpGradDofStat s;

               for (const auto& xi: x) {
                    SpGradientTraits<TX>::GetDofStat(xi, s);
               }

               const index_type nbdirs = s.iNumNz ? s.iMaxDof : 0;

               Ad.resize(imatrows * imatcols * nbdirs);
               xd.resize(nbdirs * imatcols);
               bd.resize(nbdirs * imatrows);

               for (index_type i = 0; i < imatrows * imatcols; ++i) {
                    Av[i] = SpGradientTraits<TA>::dGetValue(A[i]);

                    for (index_type j = 1; j <= nbdirs; ++j) {
                         Ad[i + (j - 1) * imatcols * imatrows] = SpGradientTraits<TA>::dGetDeriv(A[i], j);
                    }
               }

               for (index_type i = 0; i < imatcols; ++i) {
                    xv[i] = SpGradientTraits<TX>::dGetValue(x[i]);

                    for (index_type j = 1; j <= nbdirs; ++j) {
                         xd[i + (j - 1) * imatcols] = SpGradientTraits<TX>::dGetDeriv(x[i], j);
                    }
               }

               auto c_full_start = high_resolution_clock::now();

               func_mat_mul4<TA, TX>(imatrows,
                                     imatcols,
                                     nbdirs,
                                     front(Av),
                                     front(Ad),
                                     front(xv),
                                     front(xd),
                                     front(bv),
                                     front(bd));

               c_full_time += high_resolution_clock::now() - c_full_start;

               for (index_type i = 0; i < imatrows; ++i) {
                    for (index_type j = 1; j <= nbdirs; ++j) {
                         MBDYN_TESTSUITE_ASSERT(fabs(bd[i + (j - 1) * imatrows] - SpGradientTraits<TB>::dGetDeriv(b4[i], j)) / max(1., fabs(bd[i + (j - 1) * imatrows])) < dTol);
                         MBDYN_TESTSUITE_ASSERT(fabs(bd[i + (j - 1) * imatrows] - SpGradientTraits<TB>::dGetDeriv(b5[i], j)) / max(1., fabs(bd[i + (j - 1) * imatrows])) < dTol);
                         MBDYN_TESTSUITE_ASSERT(fabs(bd[i + (j - 1) * imatrows] - SpGradientTraits<TB>::dGetDeriv(b6[i], j)) / max(1., fabs(bd[i + (j - 1) * imatrows])) < dTol);
                         MBDYN_TESTSUITE_ASSERT(fabs(bd[i + (j - 1) * imatrows] - SpGradientTraits<TB>::dGetDeriv(b7[i], j)) / max(1., fabs(bd[i + (j - 1) * imatrows])) < dTol);
                    }
               }

               for (index_type i = 0; i < imatrows; ++i) {
                    MBDYN_TESTSUITE_ASSERT(fabs(bv[i] - SpGradientTraits<TB>::dGetValue(b4[i])) / max(1., fabs(bv[i])) < dTol);
                    MBDYN_TESTSUITE_ASSERT(fabs(bv[i] - SpGradientTraits<TB>::dGetValue(b5[i])) / max(1., fabs(bv[i])) < dTol);
                    MBDYN_TESTSUITE_ASSERT(fabs(bv[i] - SpGradientTraits<TB>::dGetValue(b6[i])) / max(1., fabs(bv[i])) < dTol);
                    MBDYN_TESTSUITE_ASSERT(fabs(bv[i] - SpGradientTraits<TB>::dGetValue(b7[i])) / max(1., fabs(bv[i])) < dTol);
               }
          }

          auto sp_grad4_time_ns = duration_cast<nanoseconds>(sp_grad4_time).count();
          auto sp_grad4m_time_ns = duration_cast<nanoseconds>(sp_grad4m_time).count();
          auto sp_grad4sp_time_ns = duration_cast<nanoseconds>(sp_grad4sp_time).count();
          auto sp_grad4sm_time_ns = duration_cast<nanoseconds>(sp_grad4sm_time).count();
          auto c_full_time_ns = duration_cast<nanoseconds>(c_full_time).count();

          cerr << "test4: sp_grad4_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad4_time_ns) / 1e9
               << " bnz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(b4nz) / (imatrows * inumloops))
               << " anz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(anz) / (imatcols * inumloops))
               << " xnz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(xnz) / (imatcols * inumloops))
               << endl;
          cerr << "test4: sp_grad4m_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad4m_time_ns) / 1e9
               << " bnz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(b5nz) / (imatrows * inumloops))
               << " bunz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(b5unz) / (imatrows * inumloops))
               << endl;
          cerr << "test4: sp_grad4sp_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad4sp_time_ns) / 1e9
               << endl;
          cerr << "test4: sp_grad4sm_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad4sm_time_ns) / 1e9
               << endl;
          cerr << "test4: c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(c_full_time_ns) / 1e9
               << endl;
          cerr << "test4: sp_grad4_time / c_full_time = "
               << static_cast<doublereal>(sp_grad4_time_ns) / max<int64_t>(1L, c_full_time_ns) // Avoid division by zero
               << endl;
          cerr << "test4: sp_grad4m_time / c_full_time = "
               << static_cast<doublereal>(sp_grad4m_time_ns) / max<int64_t>(1L, c_full_time_ns) // Avoid division by zero
               << endl;
          cerr << "test4: sp_grad4sp_time / c_full_time = "
               << static_cast<doublereal>(sp_grad4sp_time_ns) / max<int64_t>(1L, c_full_time_ns) // Avoid division by zero
               << endl;
          cerr << "test4: sp_grad4sm_time / c_full_time = "
               << static_cast<doublereal>(sp_grad4sm_time_ns) / max<int64_t>(1L, c_full_time_ns) // Avoid division by zero
               << endl;
     }

     void test6(index_type inumloops, index_type inumnz, index_type inumdof, index_type imatrows, index_type imatcols)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-1., 1.);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);
          uniform_int_distribution<index_type> randresizecnt(0, imatrows * imatcols - 1);
          uniform_int_distribution<index_type> randresizeidx(0, imatrows * imatcols - 1);

          gen.seed(0);

          std::vector<SpGradient> rgVec;

          for (index_type iloop = 0; iloop < std::min<index_type>(10, inumloops); ++iloop) {
               SpMatrixBase<SpGradient> A(imatrows, imatcols, inumnz);

               for (auto& g: A) {
                    sp_grad_rand_gen(g, randnz, randdof, randval, gen);
               }


               SpMatrixBase<SpGradient> A2(A.iGetNumRows(), A.iGetNumCols(), inumnz);

               std::copy(A.begin(), A.end(), A2.begin());

               rgVec.clear();

               rgVec.reserve(A.iGetNumRows() * A.iGetNumCols() * 5);

               std::copy(A.begin(), A.end(), std::back_inserter(rgVec));

               SpMatrixBase<SpGradient> B(std::move(A));

               std::copy(B.begin(), B.end(), std::back_inserter(rgVec));

               SpMatrixBase<SpGradient> C, D, E;

               C = std::move(B);

               std::copy(C.begin(), C.end(), std::back_inserter(rgVec));

               D = std::move(C);

               std::copy(D.begin(), D.end(), std::back_inserter(rgVec));

               E = std::move(D);

               std::copy(E.begin(), E.end(), std::back_inserter(rgVec));

               index_type irescnt = randresizecnt(gen);

               for (index_type i = 0; i < irescnt; ++i) {
                    rgVec[randresizeidx(gen)] += EvalUnique(rgVec[randresizeidx(gen)] * randval(gen) * rgVec[randresizeidx(gen)]);
                    rgVec[randresizeidx(gen)] *= EvalUnique(rgVec[randresizeidx(gen)] * randval(gen) - rgVec[randresizeidx(gen)] * randval(gen));
                    rgVec[randresizeidx(gen)] -= EvalUnique(rgVec[randresizeidx(gen)] * randval(gen) / (rgVec[randresizeidx(gen)] * randval(gen) + randval(gen)));
                    rgVec[randresizeidx(gen)] /= EvalUnique((rgVec[randresizeidx(gen)] * randval(gen) + randval(gen)));

                    rgVec[randresizeidx(gen)] *= randval(gen);
                    rgVec[randresizeidx(gen)] /= randval(gen);
                    rgVec[randresizeidx(gen)] += randval(gen);
                    rgVec[randresizeidx(gen)] -= randval(gen);
                    rgVec[randresizeidx(gen)] = rgVec[randresizeidx(gen)];
                    rgVec[randresizeidx(gen)] = EvalUnique(rgVec[randresizeidx(gen)] * (1 + randval(gen)) / (2. + randval(gen) + pow(rgVec[randresizeidx(gen)], 2)));
               }

               for (auto& vi: rgVec) {
                    vi.Sort();
               }

               A = std::move(E);

               MBDYN_TESTSUITE_ASSERT(A2.iGetNumRows() == A.iGetNumRows());
               MBDYN_TESTSUITE_ASSERT(A2.iGetNumCols() == A.iGetNumCols());

               for (index_type i = 0; i < A.iGetNumRows() * A.iGetNumCols(); ++i) {
                    const SpGradient& ai = *(A.begin() + i);
                    const SpGradient& a2i = *(A2.begin() + i);

                    MBDYN_TESTSUITE_ASSERT(ai.dGetValue() == a2i.dGetValue());

                    for (index_type j = 0; j < inumdof; ++j) {
                         MBDYN_TESTSUITE_ASSERT(ai.dGetDeriv(j) == a2i.dGetDeriv(j));
                    }
               }

               for (index_type i = 0; i < irescnt; ++i) {
                    *(A.begin() + randresizeidx(gen)) += *(A.begin() + randresizeidx(gen)) * randval(gen) * *(A.begin() + randresizeidx(gen));
                    *(A.begin() + randresizeidx(gen)) *= *(A.begin() + randresizeidx(gen)) * randval(gen) - *(A.begin() + randresizeidx(gen)) * randval(gen);
                    *(A.begin() + randresizeidx(gen)) -= *(A.begin() + randresizeidx(gen)) * randval(gen) / (*(A.begin() + randresizeidx(gen)) * randval(gen) + randval(gen));
                    *(A.begin() + randresizeidx(gen)) /= (*(A.begin() + randresizeidx(gen)) * randval(gen) + randval(gen));

                    *(A.begin() + randresizeidx(gen)) *= randval(gen);
                    *(A.begin() + randresizeidx(gen)) /= randval(gen);
                    *(A.begin() + randresizeidx(gen)) += randval(gen);
                    *(A.begin() + randresizeidx(gen)) -= randval(gen);
                    *(A.begin() + randresizeidx(gen)) = *(A.begin() + randresizeidx(gen));
                    *(A.begin() + randresizeidx(gen)) = EvalUnique(*(A.begin() + randresizeidx(gen)) * (1 + randval(gen)) / (2. + randval(gen) + pow(*(A.begin() + randresizeidx(gen)), 2)));

                    *(A.begin() + randresizeidx(gen)) += EvalUnique(rgVec[randresizeidx(gen)] * randval(gen) * *(A.begin() + randresizeidx(gen)));
                    rgVec[randresizeidx(gen)] *= *(A.begin() + randresizeidx(gen)) * randval(gen) - rgVec[randresizeidx(gen)] * randval(gen);
                    *(A.begin() + randresizeidx(gen)) -= rgVec[randresizeidx(gen)] * randval(gen) / (*(A.begin() + randresizeidx(gen)) * randval(gen) + randval(gen));
                    *(A.begin() + randresizeidx(gen)) /= (rgVec[randresizeidx(gen)] * randval(gen) + randval(gen));

                    *(A.begin() + randresizeidx(gen)) *= randval(gen);
                    *(A.begin() + randresizeidx(gen)) = rgVec[randresizeidx(gen)];
                    *(A.begin() + randresizeidx(gen)) = rgVec[randresizeidx(gen)] * (1 + randval(gen)) / (2. + randval(gen) + pow(rgVec[randresizeidx(gen)], 2));
               }
          }
     }

     template <typename TA, typename TB, typename TC, index_type NumRows = SpMatrixSize::DYNAMIC, index_type NumCols = SpMatrixSize::DYNAMIC>
     void test7(const index_type inumloops,
                const index_type inumnz,
                const index_type inumdof,
                const index_type imatrows,
                const index_type imatcols)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";
          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-1., 1.);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          const doublereal dTol = pow(numeric_limits<doublereal>::epsilon(), 0.5);

          typedef typename util::ResultType<typename util::ResultType<TA, TB>::Type, TC>::Type TD;
          SpGradExpDofMapHelper<TD> oDofMap;
          SpMatrixBase<TA, NumRows, NumCols> A(imatrows, imatcols, inumnz);
          SpMatrixBase<TB, NumRows, NumCols> B(imatrows, imatcols, inumnz);
          SpMatrixBase<TC, NumRows, NumCols> C(imatrows, imatcols, inumnz);
          SpMatrixBase<TD, NumRows, NumCols> D(imatrows, imatcols, 3 * inumnz);
          SpMatrixBase<TD, NumRows, NumCols> Da(imatrows, imatcols, 3 * inumnz);
          SpMatrixBase<TD, NumRows, NumCols> DMap(imatrows, imatcols, inumnz);

          vector<doublereal> Av(imatrows * imatcols), Ad,
               Bv(imatrows * imatcols), Bd,
               Cv(imatrows * imatcols), Cd,
               Dv(imatrows * imatcols), Dd;

          Ad.reserve(imatrows * imatcols * inumnz);
          Bd.reserve(imatrows * imatcols * inumnz);
          Cd.reserve(imatrows * imatcols * inumnz);
          Dd.reserve(imatrows * imatcols * inumnz);

          duration<long long, ratio<1L, 1000000000L> > sp_grad_time(0), sp_grad_a_time(0), c_full_time(0), sp_grad_compr_time(0);

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               SpGradDofStat s;

               for (auto& ai: A) {
                    sp_grad_rand_gen(ai, randnz, randdof, randval, gen);
                    SpGradientTraits<TA>::GetDofStat(ai, s);
               }

               for (auto& bi: B) {
                    sp_grad_rand_gen(bi, randnz, randdof, randval, gen);
                    SpGradientTraits<TB>::GetDofStat(bi, s);
               }

               for (auto& ci: C) {
                    sp_grad_rand_gen(ci, randnz, randdof, randval, gen);
                    SpGradientTraits<TC>::GetDofStat(ci, s);
               }

               oDofMap.ResetDofStat();
               oDofMap.GetDofStat(A);
               oDofMap.GetDofStat(B);
               oDofMap.GetDofStat(C);
               oDofMap.Reset();
               oDofMap.InsertDof(A);
               oDofMap.InsertDof(B);
               oDofMap.InsertDof(C);
               oDofMap.InsertDone();

               const index_type nbdirs = s.iNumNz ? s.iMaxDof : 0;

               Ad.resize(imatrows * imatcols * nbdirs);
               Bd.resize(imatrows * imatcols * nbdirs);
               Cd.resize(imatrows * imatcols * nbdirs);
               Dd.resize(imatrows * imatcols * nbdirs);

               for (index_type i = 0; i < imatrows * imatcols; ++i) {
                    Av[i] = SpGradientTraits<TA>::dGetValue(A.GetElem(i + 1));

                    for (index_type j = 1; j <= s.iMaxDof; ++j) {
                         Ad[i + (j - 1) * imatcols * imatrows] = SpGradientTraits<TA>::dGetDeriv(A.GetElem(i + 1), j);
                    }
               }

               for (index_type i = 0; i < imatrows * imatcols; ++i) {
                    Bv[i] = SpGradientTraits<TB>::dGetValue(B.GetElem(i + 1));

                    for (index_type j = 1; j <= s.iMaxDof; ++j) {
                         Bd[i + (j - 1) * imatcols * imatrows] = SpGradientTraits<TB>::dGetDeriv(B.GetElem(i + 1), j);
                    }
               }

               for (index_type i = 0; i < imatrows * imatcols; ++i) {
                    Cv[i] = SpGradientTraits<TC>::dGetValue(C.GetElem(i + 1));

                    for (index_type j = 1; j <= s.iMaxDof; ++j) {
                         Cd[i + (j - 1) * imatcols * imatrows] = SpGradientTraits<TC>::dGetDeriv(C.GetElem(i + 1), j);
                    }
               }

               auto start = high_resolution_clock::now();

               func_mat_add7<TA, TB, TC>(imatrows * imatcols,
                                         nbdirs,
                                         front(Av),
                                         front(Ad),
                                         front(Bv),
                                         front(Bd),
                                         front(Cv),
                                         front(Cd),
                                         front(Dv),
                                         front(Dd));

               c_full_time += high_resolution_clock::now() - start;

               start = high_resolution_clock::now();

               func_mat_add7(A, B, C, D);

               sp_grad_time += high_resolution_clock::now() - start;

               start = high_resolution_clock::now();

               func_mat_add7a(A, B, C, Da);

               sp_grad_a_time += high_resolution_clock::now() - start;

               start = high_resolution_clock::now();

               func_mat_add7(A, B, C, DMap, oDofMap);

               sp_grad_compr_time += high_resolution_clock::now() - start;

               for (index_type j = 1; j <= imatcols; ++j) {
                    for (index_type i = 1; i <= imatrows; ++i) {
                         const doublereal aij = SpGradientTraits<TA>::dGetValue(A.GetElem(i, j));
                         const doublereal bij = SpGradientTraits<TB>::dGetValue(B.GetElem(i, j));
                         const doublereal cij = SpGradientTraits<TC>::dGetValue(C.GetElem(i, j));
                         const doublereal dijr1 = SpGradientTraits<TD>::dGetValue(D.GetElem(i, j));
                         const doublereal dijr2 = Dv[(j - 1) * imatrows + i - 1];
                         const doublereal dijr3 = SpGradientTraits<TD>::dGetValue(DMap.GetElem(i, j));
                         const doublereal dij = -aij / 3. + (bij * 5. - cij / 4.) * 1.5;

                         MBDYN_TESTSUITE_ASSERT(fabs(dij - dijr1) / std::max(1., fabs(dij)) < dTol);
                         MBDYN_TESTSUITE_ASSERT(fabs(dij - dijr2) / std::max(1., fabs(dij)) < dTol);
                         MBDYN_TESTSUITE_ASSERT(fabs(dij - dijr3) / std::max(1., fabs(dij)) < dTol);

                         for (index_type k = s.iMinDof; k <= s.iMaxDof; ++k) {
                              const doublereal daij = SpGradientTraits<TA>::dGetDeriv(A.GetElem(i, j), k);
                              const doublereal dbij = SpGradientTraits<TB>::dGetDeriv(B.GetElem(i, j), k);
                              const doublereal dcij = SpGradientTraits<TC>::dGetDeriv(C.GetElem(i, j), k);
                              const doublereal ddijr1 = SpGradientTraits<TD>::dGetDeriv(D.GetElem(i, j), k);
                              const doublereal ddijr2 = Dd[((j - 1) * imatrows + (i - 1) + (k - 1) * imatrows * imatcols)];
                              const doublereal ddijr3 = SpGradientTraits<TD>::dGetDeriv(DMap.GetElem(i, j), k);
                              const doublereal ddijr4 = SpGradientTraits<TD>::dGetDeriv(Da.GetElem(i, j), k);

                              const doublereal ddij = -daij / 3. + (dbij * 5. - dcij / 4.) * 1.5;

                              MBDYN_TESTSUITE_ASSERT(fabs(ddij - ddijr1) / std::max(1., fabs(ddij)) < dTol);
                              MBDYN_TESTSUITE_ASSERT(fabs(ddij - ddijr2) / std::max(1., fabs(ddij)) < dTol);
                              MBDYN_TESTSUITE_ASSERT(fabs(ddij - ddijr3) / std::max(1., fabs(ddij)) < dTol);
                              MBDYN_TESTSUITE_ASSERT(fabs(ddij - ddijr4) / std::max(1., fabs(ddij)) < dTol);
                         }
                    }
               }
          }

          auto sp_grad_time_ns = duration_cast<nanoseconds>(sp_grad_time).count();
          auto sp_grad_a_time_ns = duration_cast<nanoseconds>(sp_grad_a_time).count();
          auto sp_grad_compr_time_ns = duration_cast<nanoseconds>(sp_grad_compr_time).count();
          auto c_full_time_ns = duration_cast<nanoseconds>(c_full_time).count();

          cerr << "test7: sp_grad_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_ns) / 1e9
               << endl;
          cerr << "test7: sp_grad_a_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_a_time_ns) / 1e9
               << endl;
          cerr << "test7: c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(c_full_time_ns) / 1e9
               << endl;
          cerr << "test7: sp_grad_time / c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_ns) / static_cast<doublereal>(c_full_time_ns)
               << endl;
          cerr << "test7: sp_grad_compr_time / c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_compr_time_ns) / static_cast<doublereal>(c_full_time_ns)
               << endl;
          cerr << "test7: sp_grad_a_time / c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_a_time_ns) / static_cast<doublereal>(c_full_time_ns)
               << endl;
     }

     template <typename TA, typename TB, typename TC, index_type NumRows = SpMatrixSize::DYNAMIC, index_type NumCols = SpMatrixSize::DYNAMIC>
     void test8(const index_type inumloops,
                const index_type inumnz,
                const index_type inumdof,
                const index_type imatrows,
                const index_type imatcols)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";
          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-1., 1.);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          const doublereal dTol = pow(numeric_limits<doublereal>::epsilon(), 0.5);

          typedef typename util::ResultType<typename util::ResultType<TA, TB>::Type, TC>::Type TD;

          SpMatrixBase<TA, NumRows, NumCols> A(imatrows, imatcols, inumnz);
          SpMatrixBase<TB, NumRows, NumCols> B(imatrows, imatcols, inumnz);
          SpMatrixBase<TB, NumCols, NumRows> B_T(imatcols, imatrows, inumnz);
          TC C;
          SpMatrixBase<TD, NumRows, NumCols> D(imatrows, imatcols, 3 * inumnz);
          SpMatrixBase<TD, NumRows, NumCols> Da(imatrows, imatcols, 3 * inumnz);
          SpMatrixBase<TD, NumRows, NumCols> Db(imatrows, imatcols, 3 * inumnz);
          SpMatrixBase<TD, NumCols, NumRows> D_T(imatcols, imatrows, 3 * inumnz);
          vector<doublereal> Av(imatrows * imatcols), Ad,
               Bv(imatrows * imatcols), Bd,
               Cd,
               Dv(imatrows * imatcols), Dd;
          doublereal Cv;

          Ad.reserve(imatrows * imatcols * inumnz);
          Bd.reserve(imatrows * imatcols * inumnz);
          Cd.reserve(inumnz);
          Dd.reserve(imatrows * imatcols * inumnz);

          duration<long long, ratio<1L, 1000000000L> > sp_grad_time(0), sp_grad_a_time(0), sp_grad_b_time(0), sp_grad_time9(0), c_full_time(0);

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               SpGradDofStat s;

               for (auto& ai: A) {
                    sp_grad_rand_gen(ai, randnz, randdof, randval, gen);
                    SpGradientTraits<TA>::GetDofStat(ai, s);
               }

               for (auto& bi: B) {
                    sp_grad_rand_gen(bi, randnz, randdof, randval, gen);
                    SpGradientTraits<TB>::GetDofStat(bi, s);
               }

               B_T = Transpose(B);

               sp_grad_rand_gen(C, randnz, randdof, randval, gen);

               SpGradientTraits<TC>::GetDofStat(C, s);

               const index_type nbdirs = s.iNumNz ? s.iMaxDof : 0;

               Ad.resize(imatrows * imatcols * nbdirs);
               Bd.resize(imatrows * imatcols * nbdirs);
               Cd.resize(nbdirs);
               Dd.resize(imatrows * imatcols * nbdirs);

               for (index_type i = 0; i < imatrows * imatcols; ++i) {
                    Av[i] = SpGradientTraits<TA>::dGetValue(A.GetElem(i + 1));

                    for (index_type j = 1; j <= nbdirs; ++j) {
                         Ad[i + (j - 1) * imatcols * imatrows] = SpGradientTraits<TA>::dGetDeriv(A.GetElem(i + 1), j);
                    }
               }

               for (index_type i = 0; i < imatrows * imatcols; ++i) {
                    Bv[i] = SpGradientTraits<TB>::dGetValue(B.GetElem(i + 1));

                    for (index_type j = 1; j <= nbdirs; ++j) {
                         Bd[i + (j - 1) * imatcols * imatrows] = SpGradientTraits<TB>::dGetDeriv(B.GetElem(i + 1), j);
                    }
               }


               Cv = SpGradientTraits<TC>::dGetValue(C);

               for (index_type j = 1; j <= nbdirs; ++j) {
                    Cd[j - 1] = SpGradientTraits<TC>::dGetDeriv(C, j);
               }

               auto start = high_resolution_clock::now();

               func_mat_add8(imatrows * imatcols,
                             nbdirs,
                             front(Av),
                             front(Ad),
                             front(Bv),
                             front(Bd),
                             Cv,
                             front(Cd),
                             front(Dv),
                             front(Dd));

               c_full_time += high_resolution_clock::now() - start;

               start = high_resolution_clock::now();

               func_mat_add8(A, B, C, D);

               sp_grad_time += high_resolution_clock::now() - start;

               start = high_resolution_clock::now();

               func_mat_add8a(A, B, C, Da);

               sp_grad_a_time += high_resolution_clock::now() - start;

               start = high_resolution_clock::now();

               func_mat_add8b(A, B, C, Db);

               sp_grad_b_time += high_resolution_clock::now() - start;

               start = high_resolution_clock::now();

               func_mat_add9(A, B_T, C, D_T);

               sp_grad_time9 += high_resolution_clock::now() - start;

               for (index_type j = 1; j <= imatcols; ++j) {
                    for (index_type i = 1; i <= imatrows; ++i) {
                         const doublereal dij1 = SpGradientTraits<TD>::dGetValue(D.GetElem(i, j));
                         const doublereal dij2 = Dv[(j - 1) * imatrows + i - 1];
                         const doublereal dij3 = SpGradientTraits<TD>::dGetValue(D_T.GetElem(j, i));
                         const doublereal dij4 = SpGradientTraits<TD>::dGetValue(Da.GetElem(i, j));
                         const doublereal dij5 = SpGradientTraits<TD>::dGetValue(Db.GetElem(i, j));

                         MBDYN_TESTSUITE_ASSERT(fabs(dij1 - dij2) / std::max(1., fabs(dij2)) < dTol);
                         MBDYN_TESTSUITE_ASSERT(fabs(dij3 - dij2) / std::max(1., fabs(dij2)) < dTol);
                         MBDYN_TESTSUITE_ASSERT(fabs(dij4 - dij2) / std::max(1., fabs(dij2)) < dTol);
                         MBDYN_TESTSUITE_ASSERT(fabs(dij5 - dij2) / std::max(1., fabs(dij2)) < dTol);

                         for (index_type k = 1; k <= nbdirs; ++k) {
                              const doublereal ddij1 = SpGradientTraits<TD>::dGetDeriv(D.GetElem(i, j), k);
                              const doublereal ddij2 = Dd[((j - 1) * imatrows + (i - 1) + (k - 1) * imatrows * imatcols)];
                              const doublereal ddij3 = SpGradientTraits<TD>::dGetDeriv(D_T.GetElem(j, i), k);
                              const doublereal ddij4 = SpGradientTraits<TD>::dGetDeriv(Da.GetElem(i, j), k);
                              const doublereal ddij5 = SpGradientTraits<TD>::dGetDeriv(Db.GetElem(i, j), k);
                              MBDYN_TESTSUITE_ASSERT(fabs(ddij1 - ddij2) / std::max(1., fabs(ddij2)) < dTol);
                              MBDYN_TESTSUITE_ASSERT(fabs(ddij3 - ddij2) / std::max(1., fabs(ddij2)) < dTol);
                              MBDYN_TESTSUITE_ASSERT(fabs(ddij4 - ddij2) / std::max(1., fabs(ddij2)) < dTol);
                              MBDYN_TESTSUITE_ASSERT(fabs(ddij5 - ddij2) / std::max(1., fabs(ddij2)) < dTol);
                         }
                    }
               }
          }

          auto sp_grad_time_ns = duration_cast<nanoseconds>(sp_grad_time).count();
          auto sp_grad_a_time_ns = duration_cast<nanoseconds>(sp_grad_a_time).count();
          auto sp_grad_b_time_ns = duration_cast<nanoseconds>(sp_grad_b_time).count();
          auto sp_grad_time9_ns = duration_cast<nanoseconds>(sp_grad_time9).count();
          auto c_full_time_ns = duration_cast<nanoseconds>(c_full_time).count();

          cerr << "test8: sp_grad_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_ns) / 1e9
               << endl;
          cerr << "test8: sp_grad_a_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_a_time_ns) / 1e9
               << endl;
          cerr << "test8: sp_grad_b_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_b_time_ns) / 1e9
               << endl;
          cerr << "test8: sp_grad_time9 = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time9_ns) / 1e9
               << endl;
          cerr << "test8: c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(c_full_time_ns) / 1e9
               << endl;
          cerr << "test8: sp_grad_time / c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_ns) / static_cast<doublereal>(c_full_time_ns)
               << endl;
          cerr << "test8: sp_grad_time9 / c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time9_ns) / static_cast<doublereal>(c_full_time_ns)
               << endl;
     }

     template <typename TA, typename TB>
     void test10(const index_type inumloops,
                 const index_type inumnz,
                 const index_type inumdof,
                 const index_type iamatrows,
                 const index_type iamatcols,
                 const index_type ibmatcols)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";
          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-1., 1.);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          const doublereal dTol = pow(numeric_limits<doublereal>::epsilon(), 0.5);

          typedef typename util::ResultType<TA, TB>::Type TC;
          SpMatrixBase<TA> A(iamatrows, iamatcols, inumnz);
          SpMatrixBase<TB> B(iamatcols, ibmatcols, inumnz);
          SpMatrixBase<TC> C(iamatrows, ibmatcols, 2 * inumnz);
          SpMatrixBase<TC> C_T(iamatrows, ibmatcols, 2 * inumnz);
          SpMatrixBase<TC> C_TA(iamatrows, ibmatcols, 2 * inumnz);
          vector<doublereal> Av(iamatrows * iamatcols), Ad,
               Bv(iamatcols * ibmatcols), Bd,
               Cv(iamatrows * ibmatcols), Cd;

          Ad.reserve(iamatrows * iamatcols * inumnz);
          Bd.reserve(iamatcols * ibmatcols * inumnz);
          Cd.reserve(iamatrows * ibmatcols * inumnz);

          duration<long long, ratio<1L, 1000000000L> > sp_grad_time(0),
               sp_grad_trans_time(0),
               sp_grad_trans_add_time(0),
               c_full_time(0);

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               SpGradDofStat s;

               for (auto& ai: A) {
                    sp_grad_rand_gen(ai, randnz, randdof, randval, gen);
                    SpGradientTraits<TA>::GetDofStat(ai, s);
               }

               for (auto& bi: B) {
                    sp_grad_rand_gen(bi, randnz, randdof, randval, gen);
                    SpGradientTraits<TB>::GetDofStat(bi, s);
               }

               const index_type nbdirs = s.iNumNz ? s.iMaxDof : 0;

               Ad.resize(iamatrows * iamatcols * nbdirs);
               Bd.resize(iamatcols * ibmatcols * nbdirs);
               Cd.resize(iamatrows * ibmatcols * nbdirs);

               for (index_type i = 0; i < iamatrows * iamatcols; ++i) {
                    Av[i] = SpGradientTraits<TA>::dGetValue(A.GetElem(i + 1));

                    for (index_type j = 1; j <= nbdirs; ++j) {
                         Ad[i + (j - 1) * iamatcols * iamatrows] = SpGradientTraits<TA>::dGetDeriv(A.GetElem(i + 1), j);
                    }
               }

               for (index_type i = 0; i < iamatcols * ibmatcols; ++i) {
                    Bv[i] = SpGradientTraits<TB>::dGetValue(B.GetElem(i + 1));

                    for (index_type j = 1; j <= nbdirs; ++j) {
                         Bd[i + (j - 1) * iamatcols * ibmatcols] = SpGradientTraits<TB>::dGetDeriv(B.GetElem(i + 1), j);
                    }
               }

               auto start = high_resolution_clock::now();

               func_mat_mul10(iamatrows,
                              iamatcols,
                              ibmatcols,
                              nbdirs,
                              front(Av),
                              front(Ad),
                              front(Bv),
                              front(Bd),
                              front(Cv),
                              front(Cd));

               c_full_time += high_resolution_clock::now() - start;

               start = high_resolution_clock::now();

               func_mat_mul10(A, B, C);

               sp_grad_time += high_resolution_clock::now() - start;

               start = high_resolution_clock::now();

               func_mat_mul10_trans(A, B, C_T);

               sp_grad_trans_time += high_resolution_clock::now() - start;

               start = high_resolution_clock::now();

               func_mat_mul10_trans_add(A, B, C_TA);

               sp_grad_trans_add_time += high_resolution_clock::now() - start;

               for (index_type j = 1; j <= ibmatcols; ++j) {
                    for (index_type i = 1; i <= iamatrows; ++i) {
                         const doublereal cij1 = SpGradientTraits<TC>::dGetValue(C.GetElem(i, j));
                         const doublereal cij2 = Cv[(j - 1) * iamatrows + i - 1];
                         const doublereal cij3 = SpGradientTraits<TC>::dGetValue(C_T.GetElem(i, j));
                         const doublereal cij4 = SpGradientTraits<TC>::dGetValue(C_TA.GetElem(i, j));

                         MBDYN_TESTSUITE_ASSERT(fabs(cij1 - cij2) / std::max(1., fabs(cij2)) < dTol);
                         MBDYN_TESTSUITE_ASSERT(fabs(cij3 - cij2) / std::max(1., fabs(cij2)) < dTol);
                         MBDYN_TESTSUITE_ASSERT(fabs(cij4 - cij2) / std::max(1., fabs(cij2)) < dTol);

                         for (index_type k = 1; k <= nbdirs; ++k) {
                              const doublereal dcij1 = SpGradientTraits<TC>::dGetDeriv(C.GetElem(i, j), k);
                              const doublereal dcij2 = Cd[((j - 1) * iamatrows + (i - 1) + (k - 1) * iamatrows * ibmatcols)];
                              const doublereal dcij3 = SpGradientTraits<TC>::dGetDeriv(C_T.GetElem(i, j), k);
                              const doublereal dcij4 = SpGradientTraits<TC>::dGetDeriv(C_TA.GetElem(i, j), k);

                              MBDYN_TESTSUITE_ASSERT(fabs(dcij1 - dcij2) / std::max(1., fabs(dcij2)) < dTol);
                              MBDYN_TESTSUITE_ASSERT(fabs(dcij3 - dcij2) / std::max(1., fabs(dcij2)) < dTol);
                              MBDYN_TESTSUITE_ASSERT(fabs(dcij4 - dcij2) / std::max(1., fabs(dcij2)) < dTol);
                         }
                    }
               }
          }

          auto sp_grad_time_ns = duration_cast<nanoseconds>(sp_grad_time).count();
          auto sp_grad_trans_time_ns = duration_cast<nanoseconds>(sp_grad_trans_time).count();
          auto sp_grad_trans_add_time_ns = duration_cast<nanoseconds>(sp_grad_trans_add_time).count();
          auto c_full_time_ns = duration_cast<nanoseconds>(c_full_time).count();

          cerr << "test10: sp_grad_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_ns) / 1e9
               << endl;

          cerr << "test10: sp_grad_trans_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_trans_time_ns) / 1e9
               << endl;

          cerr << "test10: sp_grad_trans_add_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_trans_add_time_ns) / 1e9
               << endl;

          cerr << "test10: c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(c_full_time_ns) / 1e9
               << endl;

          cerr << "test10: sp_grad_time / c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_ns) / static_cast<doublereal>(c_full_time_ns)
               << endl;

          cerr << "test10: sp_grad_trans_time / c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_trans_time_ns) / static_cast<doublereal>(c_full_time_ns)
               << endl;

          cerr << "test10: sp_grad_trans_add_time / c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_trans_add_time_ns) / static_cast<doublereal>(c_full_time_ns)
               << endl;
     }


     template <typename TA, typename TB, typename TC>
     void test11(const index_type inumloops,
                 const index_type inumnz,
                 const index_type inumdof,
                 const index_type imatrows)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";
          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-1., 1.);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          const doublereal dTol = pow(numeric_limits<doublereal>::epsilon(), 0.5);

          typedef typename util::ResultType<typename util::ResultType<TA, TB>::Type, TC>::Type TD;

          const index_type imatcols = imatrows; // Needed for func_mat_mul11

          SpMatrixBase<TA> A(imatrows, imatcols, inumnz);
          SpMatrixBase<TB> B(imatcols, imatcols, inumnz);
          SpMatrixBase<TC> C(imatrows, imatcols, inumnz);
          SpMatrixBase<TD> D(imatrows, imatcols, inumnz * 4);
          SpMatrixBase<TD> D_T(imatrows, imatcols, inumnz * 4);

          vector<doublereal>
               Av(imatrows * imatcols), Ad,
               Bv(imatrows * imatcols), Bd,
               Cv(imatrows * imatcols), Cd,
               Dv(imatrows * imatcols), Dd,
               Tmp1(imatrows * imatcols), Tmp1d,
               Tmp2(imatrows * imatcols), Tmp2d;

          duration<long long, ratio<1L, 1000000000L> > sp_grad_time(0), sp_grad_trans_time(0), c_full_time(0);

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               SpGradDofStat s;

               for (auto& ai: A) {
                    sp_grad_rand_gen(ai, randnz, randdof, randval, gen);
                    SpGradientTraits<TA>::GetDofStat(ai, s);
               }

               for (auto& bi: B) {
                    sp_grad_rand_gen(bi, randnz, randdof, randval, gen);
                    SpGradientTraits<TB>::GetDofStat(bi, s);
               }

               for (auto& ci: C) {
                    sp_grad_rand_gen(ci, randnz, randdof, randval, gen);
                    SpGradientTraits<TC>::GetDofStat(ci, s);
               }

               const index_type nbdirs = s.iNumNz ? s.iMaxDof : 0;

               Ad.resize(imatrows * imatcols * nbdirs);
               Bd.resize(imatrows * imatcols * nbdirs);
               Cd.resize(imatrows * imatcols * nbdirs);
               Dd.resize(imatrows * imatcols * nbdirs);
               Tmp1d.resize(imatrows * imatcols * nbdirs);
               Tmp2d.resize(imatrows * imatcols * nbdirs);

               for (index_type i = 0; i < imatrows * imatcols; ++i) {
                    Av[i] = SpGradientTraits<TA>::dGetValue(A.GetElem(i + 1));

                    for (index_type j = 1; j <= nbdirs; ++j) {
                         Ad[i + (j - 1) * imatcols * imatrows] = SpGradientTraits<TA>::dGetDeriv(A.GetElem(i + 1), j);
                    }
               }

               for (index_type i = 0; i < imatcols * imatcols; ++i) {
                    Bv[i] = SpGradientTraits<TB>::dGetValue(B.GetElem(i + 1));

                    for (index_type j = 1; j <= nbdirs; ++j) {
                         Bd[i + (j - 1) * imatcols * imatcols] = SpGradientTraits<TB>::dGetDeriv(B.GetElem(i + 1), j);
                    }
               }

               for (index_type i = 0; i < imatcols * imatcols; ++i) {
                    Cv[i] = SpGradientTraits<TC>::dGetValue(C.GetElem(i + 1));

                    for (index_type j = 1; j <= nbdirs; ++j) {
                         Cd[i + (j - 1) * imatcols * imatcols] = SpGradientTraits<TC>::dGetDeriv(C.GetElem(i + 1), j);
                    }
               }

               auto start = high_resolution_clock::now();

               func_mat_mul11(imatrows,
                              nbdirs,
                              front(Av),
                              front(Ad),
                              front(Bv),
                              front(Bd),
                              front(Cv),
                              front(Cd),
                              front(Dv),
                              front(Dd),
                              front(Tmp1),
                              front(Tmp1d),
                              front(Tmp2),
                              front(Tmp2d));

               c_full_time += high_resolution_clock::now() - start;

               start = high_resolution_clock::now();

               func_mat_mul11(A, B, C, D);

               sp_grad_time += high_resolution_clock::now() - start;

               start = high_resolution_clock::now();

               func_mat_mul11_trans(A, B, C, D_T);

               sp_grad_trans_time += high_resolution_clock::now() - start;

               for (index_type j = 1; j <= imatcols; ++j) {
                    for (index_type i = 1; i <= imatrows; ++i) {
                         const doublereal dij1 = SpGradientTraits<TD>::dGetValue(D.GetElem(i, j));
                         const doublereal dij2 = Dv[(j - 1) * imatrows + i - 1];
                         const doublereal dij3 = SpGradientTraits<TD>::dGetValue(D_T.GetElem(i, j));

                         MBDYN_TESTSUITE_ASSERT(fabs(dij1 - dij2) / std::max(1., fabs(dij2)) < dTol);
                         MBDYN_TESTSUITE_ASSERT(fabs(dij3 - dij2) / std::max(1., fabs(dij2)) < dTol);

                         for (index_type k = 1; k <= nbdirs; ++k) {
                              const doublereal ddij1 = SpGradientTraits<TD>::dGetDeriv(D.GetElem(i, j), k);
                              const doublereal ddij2 = Dd[((j - 1) * imatrows + (i - 1) + (k - 1) * imatrows * imatcols)];
                              const doublereal ddij3 = SpGradientTraits<TD>::dGetDeriv(D_T.GetElem(i, j), k);
                              MBDYN_TESTSUITE_ASSERT(fabs(ddij1 - ddij2) / std::max(1., fabs(ddij2)) < dTol);
                              MBDYN_TESTSUITE_ASSERT(fabs(ddij3 - ddij2) / std::max(1., fabs(ddij2)) < dTol);
                         }
                    }
               }
          }

          auto sp_grad_time_ns = duration_cast<nanoseconds>(sp_grad_time).count();
          auto sp_grad_trans_time_ns = duration_cast<nanoseconds>(sp_grad_trans_time).count();
          auto c_full_time_ns = duration_cast<nanoseconds>(c_full_time).count();

          cerr << "test11: sp_grad_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_ns) / 1e9
               << endl;

          cerr << "test11: sp_grad_trans_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_trans_time_ns) / 1e9
               << endl;

          cerr << "test11: c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(c_full_time_ns) / 1e9
               << endl;

          cerr << "test11: sp_grad_time / c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_ns) / static_cast<doublereal>(c_full_time_ns)
               << endl;

          cerr << "test11: sp_grad_trans_time / c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_trans_time_ns) / static_cast<doublereal>(c_full_time_ns)
               << endl;
     }

     template <typename TA, typename TB, typename TC>
     void test12(const index_type inumloops,
                 const index_type inumnz,
                 const index_type inumdof,
                 const index_type imatrowsa,
                 const index_type imatcolsa,
                 const index_type imatcolsb,
                 const index_type imatcolsc)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";
          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-1., 1.);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          const doublereal dTol = pow(numeric_limits<doublereal>::epsilon(), 0.5);

          typedef typename util::ResultType<typename util::ResultType<TA, TB>::Type, TC>::Type TD;

          SpMatrixBase<TA> A(imatrowsa, imatcolsa, inumnz);
          SpMatrixBase<TB> B(imatcolsa, imatcolsb, inumnz);
          SpMatrixBase<TC> C(imatcolsb, imatcolsc, inumnz);
          SpMatrixBase<TD> Da(imatcolsc, imatrowsa, 3 * inumnz),
               Db(imatcolsc, imatrowsa, 3 * inumnz),
               Dc(imatcolsc, imatrowsa, 3 * inumnz);

          vector<doublereal>
               Av(imatrowsa * imatcolsa), Ad,
               Bv(imatcolsa * imatcolsb), Bd,
               Cv(imatcolsb * imatcolsc), Cd,
               Dv(imatrowsa * imatcolsc), Dd,
               Tmp1(imatrowsa * imatcolsb), Tmp1d;

          duration<long long, ratio<1L, 1000000000L> > sp_grad_time_a(0),
               sp_grad_time_b(0),
               sp_grad_time_c(0),
               c_full_time(0);

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               SpGradDofStat s;

               for (auto& ai: A) {
                    sp_grad_rand_gen(ai, randnz, randdof, randval, gen);
                    SpGradientTraits<TA>::GetDofStat(ai, s);
               }

               for (auto& bi: B) {
                    sp_grad_rand_gen(bi, randnz, randdof, randval, gen);
                    SpGradientTraits<TB>::GetDofStat(bi, s);
               }

               for (auto& ci: C) {
                    sp_grad_rand_gen(ci, randnz, randdof, randval, gen);
                    SpGradientTraits<TC>::GetDofStat(ci, s);
               }

               const index_type nbdirs = s.iNumNz ? s.iMaxDof : 0;

               Ad.resize(imatrowsa * imatcolsa * nbdirs);
               Bd.resize(imatcolsa * imatcolsb * nbdirs);
               Cd.resize(imatcolsb * imatcolsc * nbdirs);
               Dd.resize(imatrowsa * imatcolsc * nbdirs);
               Tmp1d.resize(imatrowsa * imatcolsb * nbdirs);

               for (index_type i = 0; i < imatrowsa * imatcolsa; ++i) {
                    Av[i] = SpGradientTraits<TA>::dGetValue(A.GetElem(i + 1));

                    for (index_type j = 1; j <= nbdirs; ++j) {
                         Ad[i + (j - 1) * imatcolsa * imatrowsa] = SpGradientTraits<TA>::dGetDeriv(A.GetElem(i + 1), j);
                    }
               }

               for (index_type i = 0; i < imatcolsa * imatcolsb; ++i) {
                    Bv[i] = SpGradientTraits<TB>::dGetValue(B.GetElem(i + 1));

                    for (index_type j = 1; j <= nbdirs; ++j) {
                         Bd[i + (j - 1) * imatcolsa * imatcolsb] = SpGradientTraits<TB>::dGetDeriv(B.GetElem(i + 1), j);
                    }
               }

               for (index_type i = 0; i < imatcolsb * imatcolsc; ++i) {
                    Cv[i] = SpGradientTraits<TC>::dGetValue(C.GetElem(i + 1));

                    for (index_type j = 1; j <= nbdirs; ++j) {
                         Cd[i + (j - 1) * imatcolsb * imatcolsc] = SpGradientTraits<TC>::dGetDeriv(C.GetElem(i + 1), j);
                    }
               }

               auto start = high_resolution_clock::now();

               func_mat_mul12(imatrowsa,
                              imatcolsa,
                              imatcolsb,
                              imatcolsc,
                              nbdirs,
                              front(Av),
                              front(Ad),
                              front(Bv),
                              front(Bd),
                              front(Cv),
                              front(Cd),
                              front(Dv),
                              front(Dd),
                              front(Tmp1),
                              front(Tmp1d));

               c_full_time += high_resolution_clock::now() - start;

               start = high_resolution_clock::now();

               func_mat_mul12a(A, B, C, Da);

               sp_grad_time_a += high_resolution_clock::now() - start;

               start = high_resolution_clock::now();

               func_mat_mul12b(A, B, C, Db);

               sp_grad_time_b += high_resolution_clock::now() - start;

               start = high_resolution_clock::now();

               func_mat_mul12c(A, B, C, Dc);

               sp_grad_time_c += high_resolution_clock::now() - start;

               for (index_type j = 1; j <= imatcolsc; ++j) {
                    for (index_type i = 1; i <= imatrowsa; ++i) {
                         const doublereal dija = SpGradientTraits<TD>::dGetValue(Da.GetElem(j, i));
                         const doublereal dijb = SpGradientTraits<TD>::dGetValue(Db.GetElem(j, i));
                         const doublereal dijc = SpGradientTraits<TD>::dGetValue(Dc.GetElem(j, i));
                         const doublereal dij2 = Dv[(j - 1) * imatrowsa + i - 1];

                         MBDYN_TESTSUITE_ASSERT(fabs(dija - dij2) / std::max(1., fabs(dij2)) < dTol);
                         MBDYN_TESTSUITE_ASSERT(fabs(dijb - dij2) / std::max(1., fabs(dij2)) < dTol);
                         MBDYN_TESTSUITE_ASSERT(fabs(dijc - dij2) / std::max(1., fabs(dij2)) < dTol);

                         for (index_type k = 1; k <= nbdirs; ++k) {
                              const doublereal ddija = SpGradientTraits<TD>::dGetDeriv(Da.GetElem(j, i), k);
                              const doublereal ddijb = SpGradientTraits<TD>::dGetDeriv(Db.GetElem(j, i), k);
                              const doublereal ddijc = SpGradientTraits<TD>::dGetDeriv(Dc.GetElem(j, i), k);
                              const doublereal ddij2 = Dd[((j - 1) * imatrowsa + (i - 1) + (k - 1) * imatrowsa * imatcolsc)];
                              MBDYN_TESTSUITE_ASSERT(fabs(ddija - ddij2) / std::max(1., fabs(ddij2)) < dTol);
                              MBDYN_TESTSUITE_ASSERT(fabs(ddijb - ddij2) / std::max(1., fabs(ddij2)) < dTol);
                              MBDYN_TESTSUITE_ASSERT(fabs(ddijc - ddij2) / std::max(1., fabs(ddij2)) < dTol);
                         }
                    }
               }
          }

          auto sp_grad_time_a_ns = duration_cast<nanoseconds>(sp_grad_time_a).count();
          auto sp_grad_time_b_ns = duration_cast<nanoseconds>(sp_grad_time_b).count();
          auto sp_grad_time_c_ns = duration_cast<nanoseconds>(sp_grad_time_c).count();
          auto c_full_time_ns = duration_cast<nanoseconds>(c_full_time).count();

          cerr << "test12: sp_grad_time_a = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_a_ns) / 1e9
               << endl;

          cerr << "test12: sp_grad_time_b = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_b_ns) / 1e9
               << endl;

          cerr << "test12: sp_grad_time_c = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_c_ns) / 1e9
               << endl;

          cerr << "test12: c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(c_full_time_ns) / 1e9
               << endl;

          cerr << "test12: sp_grad_time_a / c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_a_ns) / static_cast<doublereal>(c_full_time_ns)
               << endl;

          cerr << "test12: sp_grad_time_b / c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_b_ns) / static_cast<doublereal>(c_full_time_ns)
               << endl;

          cerr << "test12: sp_grad_time_c / c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_c_ns) / static_cast<doublereal>(c_full_time_ns)
               << endl;
     }

     template <typename T>
     void test13(const index_type inumloops,
                 const index_type inumnz,
                 const index_type inumdof)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-1., 1.);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          duration<long long, ratio<1L, 1000000000L> > sp_grad_time(0);

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               SpMatrix<T, 3, 4> A(3, 4, inumnz);

               for (index_type i = 1; i <= A.iGetNumRows(); ++i) {
                    for (index_type j = 1; j <= A.iGetNumCols(); ++j) {
                         sp_grad_rand_gen(A(i, j), randnz, randdof, randval, gen);
                    }
               }

               SpMatrix<T, 4, 2> B(4, 2, inumnz);

               for (index_type i = 1; i <= B.iGetNumRows(); ++i) {
                    for (index_type j = 1; j <= B.iGetNumCols(); ++j) {
                         sp_grad_rand_gen(B(i, j), randnz, randdof, randval, gen);
                    }
               }

               SpColVector<T, 4> v1(4, inumnz);

               for (index_type i = 1; i <= v1.iGetNumRows(); ++i) {
                    sp_grad_rand_gen(v1(i), randnz, randdof, randval, gen);
               }

               SpColVector<T, 3> v2(3, inumnz);

               for (index_type i = 1; i <= v2.iGetNumRows(); ++i) {
                    sp_grad_rand_gen(v2(i), randnz, randdof, randval, gen);
               }

               auto start = high_resolution_clock::now();

               SpMatrix<T, 3, 2> C = A * B;
               SpMatrix<T, 2, 3> D = Transpose(A * B);
               SpMatrix<T, 2, 3> E = Transpose(B) * Transpose(A);
               SpMatrix<T, 3, 3> F = A * B * Transpose(A * B);
               SpMatrix<T, 3, 3> G = A * B * Transpose(B) * Transpose(A);
               SpMatrix<T, 3, 3> H = C * Transpose(C);
               SpMatrix<T, 2, 2> I = Transpose(C) * C;
               SpMatrix<T, 3, 3> J = Transpose(A * B * Transpose(A * B));
               SpColVector<T, 3> w = F * v2;
               SpRowVector<T, 1> u = Transpose(v1) * v1;
               SpMatrix<T, 4, 4> t = v1 * Transpose(v1);
               SpRowVector<T, 3> s = Transpose(v1) * Transpose(A);
               SpRowVector<T, 3> r = Transpose(A * v1);
               SpColVector<T, 3> x = C.GetCol(1) + C.GetCol(2);
               SpColVector<T, 3> vm1 = Cross(Transpose(r), Transpose(s));
               SpColVector<T, 3> vm2 = EvalUnique(Cross(Transpose(r), Transpose(s)));
               SpColVector<T, 3> vr1(3, 0), vr2(3, 0);

               const T x1 = r(1), y1 = r(2), z1 = r(3);
               const T x2 = s(1), y2 = s(2), z2 = s(3);

               vr1(1) = y1 * z2 - y2 * z1;
               vr1(2) = -(x1 * z2 - x2 * z1);
               vr1(3) = x1 * y2 - x2 * y1;

               vr2(1) = EvalUnique(y1 * z2 - y2 * z1);
               vr2(2) = -EvalUnique(x1 * z2 - x2 * z1);
               vr2(3) = EvalUnique(x1 * y2 - x2 * y1);

               SpMatrix<T, 3, 3> y(3, 3, 0);

               for (index_type i = 1; i <= F.iGetNumRows(); ++i) {
                    for (index_type j = 1; j <= F.iGetNumCols(); ++j) {
                         y = F.GetCol(j) * F.GetRow(i);
                    }
               }

               for (index_type i = 1; i <= F.iGetNumRows(); ++i) {
                    for (index_type j = 1; j <= F.iGetNumCols(); ++j) {
                         y = Transpose(F.GetRow(i)) * Transpose(F.GetCol(j));
                    }
               }

               SpMatrix<T, decltype(A)::iNumRowsStatic, decltype(B)::iNumColsStatic> K(A.iNumRowsStatic, B.iNumColsStatic, 0);
               SpMatrix<T, decltype(A)::iNumRowsStatic, decltype(B)::iNumColsStatic> L(A.iNumRowsStatic, B.iNumColsStatic, 0);

               for (index_type j = 1; j <= B.iGetNumCols(); ++j) {
                    for (index_type i = 1; i <= A.iGetNumRows(); ++i) {
                         K(i, j) = Dot(Transpose(A.GetRow(i)), B.GetCol(j));
                         L(i, j) = Dot(B.GetCol(j), Transpose(A.GetRow(i)));
                    }
               }

               T q1 = Dot(v2, F * v2);
               T q2 = Dot(v2, w);
               T q3 = Dot(F * v2, v2);

               sp_grad_time += high_resolution_clock::now() - start;

               const doublereal dTol = sqrt(std::numeric_limits<doublereal>::epsilon());

               sp_grad_assert_equal(q1, q2, dTol);
               sp_grad_assert_equal(q1, q3, dTol);

               MBDYN_TESTSUITE_ASSERT(SpGradientTraits<T>::dGetValue(q1) >= 0.);

               for (index_type i = 1; i <= 3; ++i) {
                    for (index_type j = 1; j <= 2; ++j) {
                         sp_grad_assert_equal(C(i, j), D(j, i), dTol);
                         sp_grad_assert_equal(C(i, j), E(j, i), dTol);
                         sp_grad_assert_equal(C(i, j), K(i, j), dTol);
                         sp_grad_assert_equal(C(i, j), L(i, j), dTol);
                    }
               }

               for (index_type i = 1; i <= 3; ++i) {
                    for (index_type j = 1; j <= 3; ++j) {
                         sp_grad_assert_equal(F(i, j), G(i, j), dTol);
                         sp_grad_assert_equal(F(i, j), H(i, j), dTol);
                         sp_grad_assert_equal(F(i, j), J(i, j), dTol);
                    }
               }

               for (index_type i = 1; i <= H.iGetNumRows(); ++i) {
                    for (index_type j = i; j <= H.iGetNumCols(); ++j) {
                         sp_grad_assert_equal(H(i, j), H(j, i), dTol);
                    }
               }

               for (index_type i = 1; i <= I.iGetNumRows(); ++i) {
                    for (index_type j = i; j <= I.iGetNumCols(); ++j) {
                         sp_grad_assert_equal(I(i, j), I(j, i), dTol);
                    }
               }

               for (index_type i = 1; i <= t.iGetNumRows(); ++i) {
                    for (index_type j = i; j <= t.iGetNumCols(); ++j) {
                         sp_grad_assert_equal(t(i, j), t(j, i), dTol);
                    }
               }

               for (index_type i = 1; i <= 3; ++i) {
                    sp_grad_assert_equal(vm1(i), vr1(i), dTol);
                    sp_grad_assert_equal(vm2(i), vr1(i), dTol);
                    sp_grad_assert_equal(vr2(i), vr1(i), dTol);
               }

               start = high_resolution_clock::now();

               I = Transpose(C) * C;
               H = C * Transpose(C);
               F = A * B * Transpose(A * B);
               G = A * B * Transpose(B) * Transpose(A);
               C = A * B;
               D = Transpose(A * B);
               E = Transpose(B) * Transpose(A);
               w = F * v2;
               r = Transpose(w);

               sp_grad_time += high_resolution_clock::now() - start;

               for (index_type i = 1; i <= 3; ++i) {
                    for (index_type j = 1; j <= 2; ++j) {
                         sp_grad_assert_equal(C(i, j), D(j, i), dTol);
                         sp_grad_assert_equal(C(i, j), E(j, i), dTol);
                    }
               }

               for (index_type i = 1; i <= 3; ++i) {
                    for (index_type j = 1; j <= 3; ++j) {
                         sp_grad_assert_equal(F(i, j), G(i, j), dTol);
                         sp_grad_assert_equal(F(i, j), H(i, j), dTol);
                    }
               }
          }

          auto sp_grad_time_ns = duration_cast<nanoseconds>(sp_grad_time).count();

          cerr << "test13: sp_grad_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_ns) / 1e9
               << endl;
     }

     template <typename T>
     void test15(const index_type inumloops,
                 const index_type inumnz,
                 const index_type inumdof)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-M_PI, M_PI);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          const doublereal dTol = pow(numeric_limits<doublereal>::epsilon(), 0.5);

          SpMatrix<T, 3, 3> R(3, 3, inumnz), R2(3, 3, inumnz);
          SpColVector<T, 3> Phi(3, inumnz), Phi2(3, inumnz);

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               for (auto& phii: Phi) {
                    sp_grad_rand_gen(phii, randnz, randdof, randval, gen);
               }

               R = MatRotVec(Phi);
               Phi2 = VecRotMat(R);
               R2 = MatRotVec(Phi2);

               for (index_type i = 1; i <= 3; ++i) {
                    if (sqrt(Dot(Phi, Phi)) < M_PI) {
                         sp_grad_assert_equal(Phi2(i), Phi(i), dTol);
                    }

                    for (index_type j = 1; j <= 3; ++j) {
                         sp_grad_assert_equal(R2(i, j), R(i, j), dTol);
                    }
               }

          }
     }

     void test16(index_type inumloops, index_type inumnz, index_type inumdof, index_type imatrows, index_type imatcols)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-1., 1.);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);
          uniform_int_distribution<index_type> randadd(1, 10);

          const integer iNumRows = imatrows;
          const integer iNumCols = imatcols;

          const doublereal dTol = std::pow(std::numeric_limits<doublereal>::epsilon(), 0.9);

          uniform_int_distribution<index_type> randdof(1, iNumCols);

          gen.seed(0);

          SpMatrix<doublereal> A(iNumRows, iNumCols, 0), B(iNumRows, iNumCols, 0);
          SpColVector<SpGradient> X1(iNumCols, 0), X2(iNumCols, 0);
          SpGradientSubMatrixHandler oSubMat1(A.iGetNumRows()), oSubMat2(A.iGetNumRows());
          SpGradientSparseMatrixHandler oSpMatHd(iNumRows, iNumCols);
          FullMatrixHandler oFullMatHd(iNumRows, iNumCols);
          MyVectorHandler X(iNumCols), Y(iNumRows), Z1(iNumRows), W1(iNumCols), Z2(iNumRows), W2(iNumCols);
#ifdef USE_TRILINOS
          Epetra_SerialComm Comm;
          EpetraVectorHandler Xep(iNumCols, Comm), Yep(iNumRows, Comm), Zep(iNumRows, Comm), Wep(iNumCols, Comm);
          EpetraSparseMatrixHandler oEpMatHd(iNumRows, iNumCols, 5 * inumnz, Comm);
#endif

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               integer k = randadd(gen);

               doublereal a, b;

               sp_grad_rand_gen(a, randnz, randdof, randval, gen);
               sp_grad_rand_gen(b, randnz, randdof, randval, gen);

               for (auto& aij: A) {
                    sp_grad_rand_gen(aij, randnz, randdof, randval, gen);
               }

               for (auto& bij: B) {
                    sp_grad_rand_gen(bij, randnz, randdof, randval, gen);
               }

               for (integer j = 1; j <= iNumCols; ++j) {
                    sp_grad_rand_gen(X(j), randnz, randdof, randval, gen);
#ifdef USE_TRILINOS
                    Xep(j) = X(j);
#endif
               }

               for (integer i = 1; i <= iNumRows; ++i) {
                    sp_grad_rand_gen(Y(i), randnz, randdof, randval, gen);
#ifdef USE_TRILINOS
                    Yep(i) = Y(i);
#endif
               }

               for (index_type i = 1; i <= X1.iGetNumRows(); ++i) {
                    X1(i).Reset(randval(gen), i, a);
               }

               for (index_type i = 1; i <= X2.iGetNumRows(); ++i) {
                    X2(i).Reset(randval(gen), i, b);
               }

               const SpColVector<SpGradient> f1 = A * X1;
               const SpColVector<SpGradient> f2 = B * -X2;

               oSubMat1.Reset();

               for (index_type iRow = 1; iRow <= f1.iGetNumRows(); ++iRow) {
                    oSubMat1.AddItem(iRow, f1(iRow));
               }

               oSubMat2.Reset();

               for (index_type iRow = 1; iRow <= f2.iGetNumRows(); ++iRow) {
                    oSubMat2.AddItem(iRow, f2(iRow));
               }

               oSpMatHd.Reset();
               oFullMatHd.Reset();
#ifdef USE_TRILINOS
               oEpMatHd.Reset();
#endif
               for (integer i = 0; i < k; ++i) {
                    oSpMatHd += oSubMat1;
                    oFullMatHd += oSubMat1;
#ifdef USE_TRILINOS
                    oEpMatHd += oSubMat1;
#endif
                    oSpMatHd += oSubMat2;
                    oFullMatHd += oSubMat2;
#ifdef USE_TRILINOS
                    oEpMatHd += oSubMat2;
#endif
               }

               std::vector<integer> Ai, Ap;
               std::vector<doublereal> Ax;

               oSpMatHd.MakeCompressedColumnForm(Ax, Ai, Ap, 1);

               const auto& oSpMatHdConst = oSpMatHd; // FIXME: operator() is not defined for non constant matrix

#ifdef USE_TRILINOS
               std::vector<integer> Aepi, Aepp;
               std::vector<doublereal> Aepx;

               oEpMatHd.MakeCompressedColumnForm(Aepx, Aepi, Aepp, 1);

               const auto& oEpMatHdConst = oEpMatHd; // FIXME: operator() is not defined for non constant matrix
#endif

               for (index_type j = 1; j <= oFullMatHd.iGetNumCols(); ++j) {
                    for (index_type i = 1; i <= oFullMatHd.iGetNumRows(); ++i) {
                         sp_grad_assert_equal(oFullMatHd(i, j), k * (a * A(i, j) - b * B(i, j)), dTol);
                         sp_grad_assert_equal(oSpMatHdConst(i, j), k * (a * A(i, j) - b * B(i, j)), dTol);
#ifdef USE_TRILINOS
                         sp_grad_assert_equal(oEpMatHdConst(i, j), k * (a * A(i, j) - b * B(i, j)), dTol);
#endif
                    }
               }
#ifdef USE_TRILINOS
               for (size_t i = 0; i < Ax.size(); ++i) {
                    sp_grad_assert_equal(Aepx[i], Ax[i], dTol);
                    MBDYN_TESTSUITE_ASSERT(Ai[i] == Aepi[i]);
               }

               for (size_t i = 0; i < Ap.size(); ++i) {
                    MBDYN_TESTSUITE_ASSERT(Ap[i] == Aepp[i]);
               }
#endif
               const CColMatrixHandler<1> oMatCC1(Ax, Ai, Ap);

               for (index_type j = 1; j <= oMatCC1.iGetNumCols(); ++j) {
                    // FIXME: CColMatrixHandler always assumes a square matrix
                    for (index_type i = 1; i <= std::min(A.iGetNumRows(), oMatCC1.iGetNumRows()); ++i) {
                         sp_grad_assert_equal(oMatCC1(i, j), k * (a * A(i, j) - b * B(i, j)), dTol);
                    }
               }

               oSpMatHd.MakeCompressedColumnForm(Ax, Ai, Ap, 0);

               const CColMatrixHandler<0> oMatCC0(Ax, Ai, Ap);

               for (index_type j = 1; j <= oMatCC0.iGetNumCols(); ++j) {
                    // FIXME: CColMatrixHandler always assumes a square matrix
                    for (index_type i = 1; i <= std::min(A.iGetNumRows(), oMatCC0.iGetNumRows()); ++i) {
                         sp_grad_assert_equal(oMatCC0(i, j), k * (a * A(i, j) - b * B(i, j)), dTol);
                    }
               }

#ifdef USE_TRILINOS
               const CColMatrixHandler<1> oMatCCep1(Aepx, Aepi, Aepp);

               for (index_type j = 1; j <= oMatCCep1.iGetNumCols(); ++j) {
                    // FIXME: CColMatrixHandler always assumes a square matrix
                    for (index_type i = 1; i <= std::min(A.iGetNumRows(), oMatCCep1.iGetNumRows()); ++i) {
                         sp_grad_assert_equal(oMatCCep1(i, j), k * (a * A(i, j) - b * B(i, j)), dTol);
                    }
               }
#endif
               oSpMatHd.MatVecMul(Z1, X);

               oFullMatHd.MatVecMul(Z2, X);

#ifdef USE_TRILINOS
               oEpMatHd.MatVecMul(Zep, Xep);
#endif
               oSpMatHd.MatTVecMul(W1, Y);

               oFullMatHd.MatTVecMul(W2, Y);

#ifdef USE_TRILINOS
               oEpMatHd.MatTVecMul(Wep, Yep);
#endif
               for (integer i = 1; i <= iNumRows; ++i) {
                    sp_grad_assert_equal(Z1(i), Z2(i), dTol * Z2.Norm());
#ifdef USE_TRILINOS
                    sp_grad_assert_equal(Zep(i), Z2(i), dTol * Z2.Norm());
#endif
               }

               for (integer j = 1; j <= iNumCols; ++j) {
                    sp_grad_assert_equal(W1(j), W2(j), dTol * W2.Norm());
#ifdef USE_TRILINOS
                    sp_grad_assert_equal(Wep(j), W2(j), dTol * W2.Norm());
#endif
               }
          }

          std::cout.precision(4);
          std::cout.setf(std::ios::fixed | std::ios::dec);
          std::cout << "gradient matrix handler:\n" << oSpMatHd << std::endl;
          std::cout << "full matrix handler:\n" << oFullMatHd << std::endl;
#ifdef USE_TRILINOS
          std::cout << "epetra matrix handler:\n" << oEpMatHd << std::endl;
#endif
     }

     void test18(index_type inumloops, index_type inumnz, index_type inumdof)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-0.5, 0.5);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          SpGradient u, v, w, f, fc;

          doublereal e = randval(gen), fVal;
          index_type unz = 0, vnz = 0, wnz = 0;
          index_type fnz = 0, funz = 0, fcnz = 0;
          const doublereal dTol = sqrt(std::numeric_limits<doublereal>::epsilon());
          vector<doublereal> ud, vd, wd, fd, work;
          duration<long long, ratio<1L, 1000000000L> > sp_grad_time(0), sp_grad_compr_time(0), c_full_time(0);

          // decltype(sp_grad_time) sp_grad_s_time(0);

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               sp_grad_rand_gen(u, randnz, randdof, randval, gen);
               sp_grad_rand_gen(v, randnz, randdof, randval, gen);
               sp_grad_rand_gen(w, randnz, randdof, randval, gen);

               unz += u.iGetSize();
               vnz += v.iGetSize();
               wnz += w.iGetSize();

               SpGradDofStat s;

               u.GetDofStat(s);
               v.GetDofStat(s);
               w.GetDofStat(s);

               const index_type nbdirs = s.iNumNz ? s.iMaxDof : 0;

               func_scalar3(u.dGetValue(), v.dGetValue(), w.dGetValue(), e, fVal);

               auto sp_grad_start = high_resolution_clock::now();

               func_scalar3(u, v, w, e, f);

               sp_grad_time += high_resolution_clock::now() - sp_grad_start;

               funz += f.iGetSize();

               f.Sort();

               fnz += f.iGetSize();

               auto sp_grad_compr_start = high_resolution_clock::now();

               func_scalar3_compressed(u, v, w, e, fc);

               sp_grad_compr_time += high_resolution_clock::now() - sp_grad_compr_start;

               fcnz += fc.iGetSize();

               MBDYN_TESTSUITE_ASSERT(fabs(f.dGetValue() / fVal - 1.) < dTol);

               MBDYN_TESTSUITE_ASSERT(fabs(fc.dGetValue() / fVal - 1.) < dTol);

               ud.clear();
               vd.clear();
               wd.clear();
               fd.clear();
               work.clear();
               ud.resize(nbdirs);
               vd.resize(nbdirs);
               wd.resize(nbdirs);
               fd.resize(nbdirs);
               work.resize(0);

               for (index_type i = 1; i <= s.iMaxDof; ++i) {
                    ud[i - 1] = u.dGetDeriv(i);
                    vd[i - 1] = v.dGetDeriv(i);
                    wd[i - 1] = w.dGetDeriv(i);
               }

               auto c_full_start = high_resolution_clock::now();

               func_scalar3_dv(nbdirs,
                               u.dGetValue(),
                               front(ud),
                               v.dGetValue(),
                               front(vd),
                               w.dGetValue(),
                               front(wd),
                               e,
                               fVal,
                               front(fd),
                               front(work));

               c_full_time += high_resolution_clock::now() - c_full_start;

               SP_GRAD_TRACE("fref f\n");
               SP_GRAD_TRACE(fVal << " " << f.dGetValue() << endl);

               MBDYN_TESTSUITE_ASSERT(fabs(f.dGetValue() - fVal) < dTol * max(1., fabs(fVal)));

               for (index_type i = 1; i <= s.iMaxDof; ++i) {
                    SP_GRAD_TRACE(fd[i - 1] << " " << f.dGetDeriv(i) << endl);
                    MBDYN_TESTSUITE_ASSERT(fabs(fd[i - 1] - f.dGetDeriv(i)) < dTol * max(1.0, fabs(fd[i - 1])));
                    MBDYN_TESTSUITE_ASSERT(fabs(fd[i - 1] - fc.dGetDeriv(i)) < dTol * max(1.0, fabs(fd[i - 1])));
               }
          }

          auto sp_grad_time_ns = duration_cast<nanoseconds>(sp_grad_time).count();
          auto sp_grad_compr_time_ns = duration_cast<nanoseconds>(sp_grad_compr_time).count();
          auto c_full_time_ns = duration_cast<nanoseconds>(c_full_time).count();

          cerr << "test2: test passed with tolerance "
               << scientific << setprecision(6)
               << dTol
               << " and nz=" << inumnz
               << " unz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(unz) / inumloops)
               << " vnz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(vnz) / inumloops)
               << " wnz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(wnz) / inumloops)
               << endl;

          cerr << "test2: sp_grad_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_ns) / 1e9
               << " nz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(fnz) / inumloops)
               << " unz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(funz) / inumloops)
               << endl;
          cerr << "test2: sp_grad_compressed_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_compr_time_ns) / 1e9
               << " nz=" << fixed << setprecision(0) << ceil(static_cast<doublereal>(fcnz) / inumloops)
               << endl;
          cerr << "test2: c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(c_full_time_ns) / 1e9 << endl;
          cerr << "test2: sp_grad_time / c_full_time = "
               << static_cast<doublereal>(sp_grad_time_ns) / max<int64_t>(1L, c_full_time_ns) // Avoid division by zero
               << endl;
          cerr << "test2: sp_grad_compr_time / c_full_time = "
               << static_cast<doublereal>(sp_grad_compr_time_ns) / max<int64_t>(1L, c_full_time_ns) // Avoid division by zero
               << endl;
     }

     void test18gp(index_type inumloops, index_type inumnz, index_type inumdof)
     {
          using namespace std;
          using namespace std::chrono;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randval(-0.5, 0.5);
          uniform_int_distribution<index_type> randdof(1, inumdof);
          uniform_int_distribution<index_type> randnz(0, inumnz - 1);

          gen.seed(0);

          GpGradProd u, v, w, f;

          doublereal e = randval(gen), fVal;
          const doublereal dTol = sqrt(std::numeric_limits<doublereal>::epsilon());
          vector<doublereal> ud, vd, wd, fd, work;
          duration<long long, ratio<1L, 1000000000L> > sp_grad_time(0), c_full_time(0);

          // decltype(sp_grad_time) sp_grad_s_time(0);

          for (index_type iloop = 0; iloop < inumloops; ++iloop) {
               sp_grad_rand_gen(u, randnz, randdof, randval, gen);
               sp_grad_rand_gen(v, randnz, randdof, randval, gen);
               sp_grad_rand_gen(w, randnz, randdof, randval, gen);

               const index_type nbdirs = 1;

               func_scalar3(u.dGetValue(), v.dGetValue(), w.dGetValue(), e, fVal);

               auto sp_grad_start = high_resolution_clock::now();

               func_scalar3(u, v, w, e, f);

               sp_grad_time += high_resolution_clock::now() - sp_grad_start;


               MBDYN_TESTSUITE_ASSERT(fabs(f.dGetValue() / fVal - 1.) < dTol);

               ud.clear();
               vd.clear();
               wd.clear();
               fd.clear();
               work.clear();
               ud.resize(nbdirs);
               vd.resize(nbdirs);
               wd.resize(nbdirs);
               fd.resize(nbdirs);
               work.resize(0);

               ud[0] = u.dGetDeriv();
               vd[0] = v.dGetDeriv();
               wd[0] = w.dGetDeriv();

               auto c_full_start = high_resolution_clock::now();

               func_scalar3_dv(nbdirs,
                               u.dGetValue(),
                               front(ud),
                               v.dGetValue(),
                               front(vd),
                               w.dGetValue(),
                               front(wd),
                               e,
                               fVal,
                               front(fd),
                               front(work));

               c_full_time += high_resolution_clock::now() - c_full_start;

               SP_GRAD_TRACE("fref f\n");
               SP_GRAD_TRACE(fVal << " " << f.dGetValue() << endl);

               MBDYN_TESTSUITE_ASSERT(fabs(f.dGetValue() - fVal) < dTol * max(1., fabs(fVal)));

               SP_GRAD_TRACE(fd[0] << " " << f.dGetDeriv() << endl);
               MBDYN_TESTSUITE_ASSERT(fabs(fd[0] - f.dGetDeriv()) < dTol * max(1.0, fabs(fd[0])));
          }

          auto sp_grad_time_ns = duration_cast<nanoseconds>(sp_grad_time).count();
          auto c_full_time_ns = duration_cast<nanoseconds>(c_full_time).count();

          cerr << "test2: test passed with tolerance "
               << scientific << setprecision(6)
               << dTol
               << endl;
          cerr << "test2: sp_grad_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(sp_grad_time_ns) / 1e9
               << endl;
          cerr << "test2: c_full_time = " << fixed << setprecision(6)
               << static_cast<doublereal>(c_full_time_ns) / 1e9 << endl;
          cerr << "test2: sp_grad_time / c_full_time = "
               << static_cast<doublereal>(sp_grad_time_ns) / max<int64_t>(1L, c_full_time_ns) // Avoid division by zero
               << endl;
     }

     void test19()
     {
          using namespace std;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          SpColVector<SpGradient, 3> u(3, 1), v(3, 1);

          for (index_type i = 1; i <= 3; ++i) {
               u(i).Reset(10. * i, i, 7.);
               v(i).Reset(3. * i, i + 3, -4.);
          }

          SpMatrix<SpGradient, 3, 2> vv(3, 2, 1);

          for (index_type i = 1; i <= 3; ++i) {
               for (index_type j = 1; j <= 2; ++j) {
                    vv(i, j) = v(i) * j;
               }
          }

          SpMatrix<SpGradient, 2, 3> vvT = Transpose(vv);
          SpRowVector<SpGradient, 3> vT = Transpose(v);

          SpGradient f1 = Dot(u, v);
          SpGradient f2 = Dot(v, u);
          SpGradient f3 = Dot(2. * u, v) / 2.;
          SpGradient f4 = Dot(2. * v, u) / 2.;
          SpGradient f5 = Dot(2. * u, 3. * v) / 6.;
          SpGradient f6 = Dot(3. * v, 2. * u) / 6.;
          SpGradient f7 = (1./6.) * Dot(u * 2., v * 3.);
          SpGradient f8 = (1./6.) * Dot(v * 3., u * 2.);

          SpGradient f9 = Dot(u, Transpose(vT));
          SpGradient f10 = Dot(Transpose(vT), u);
          SpGradient f11 = Dot(2. * u, Transpose(vT)) / 2.;
          SpGradient f12 = Dot(2. * Transpose(vT), u) / 2.;
          SpGradient f13 = Dot(2. * u, 3. * Transpose(vT)) / 6.;
          SpGradient f14 = Dot(3. * Transpose(vT), 2. * u) / 6.;
          SpGradient f15 = (1./6.) * Dot(u * 2., Transpose(vT) * 3.);
          SpGradient f16 = (1./6.) * Dot(Transpose(vT) * 3., u * 2.);


          SpGradient f17 = Dot(EvalUnique(u), v);
          SpGradient f18 = Dot(EvalUnique(v), u);
          SpGradient f19 = Dot(2. * EvalUnique(u), v) / 2.;
          SpGradient f20 = Dot(2. * EvalUnique(v), u) / 2.;
          SpGradient f21 = Dot(2. * EvalUnique(u), 3. * v) / 6.;
          SpGradient f22 = Dot(3. * EvalUnique(v), 2. * u) / 6.;
          SpGradient f23 = (1./6.) * Dot(EvalUnique(u) * 2., v * 3.);
          SpGradient f24 = (1./6.) * Dot(EvalUnique(v) * 3., u * 2.);

          SpGradient f25 = Dot(EvalUnique(u), Transpose(vT));
          SpGradient f26 = Dot(Transpose(vT), EvalUnique(u));
          SpGradient f27 = Dot(2. * u, EvalUnique(Transpose(vT))) / 2.;
          SpGradient f28 = Dot(2. * Transpose(vT), EvalUnique(u)) / 2.;
          SpGradient f29 = Dot(2. * EvalUnique(u), 3. * Transpose(vT)) / 6.;
          SpGradient f30 = Dot(3. * Transpose(vT), 2. * u) / 6.;
          SpGradient f31 = (1./6.) * Dot(EvalUnique(u) * 2., Transpose(vT) * 3.);
          SpGradient f32 = (1./6.) * EvalUnique(Dot(Transpose(vT) * 3., u * 2.));

          SpGradient f33 = Dot(vv.GetCol(1), u);
          SpGradient f34 = Dot(vv.GetCol(2), u) / 2.;
          SpGradient f35 = Dot(u, vv.GetCol(1));
          SpGradient f36 = Dot(u, vv.GetCol(2)) / 2.;
          SpGradient f37 = Dot(u / 2., vv.GetCol(2));
          SpGradient f38 = Dot(u, vv.GetCol(2) / 2.);

          SpGradient f39 = Dot(Transpose(vvT.GetRow(1)), u);
          SpGradient f40 = Dot(Transpose(vvT.GetRow(2)), u / 2.);
          SpGradient f41 = Dot(u, Transpose(vvT.GetRow(1)));
          SpGradient f42 = Dot(u / 2., Transpose(vvT.GetRow(2)));
          SpGradient f43 = Dot(Transpose(vvT.GetRow(1)) / 2., u * 2.);
          SpGradient f44 = Dot(Transpose(vvT.GetRow(2)) / 2., u);
          SpGradient f45 = Dot(u / 2., Transpose(vvT.GetRow(1)) * 2.);
          SpGradient f46 = Dot(u, Transpose(vvT.GetRow(2)) / 2.);

          SpGradient f47 = Dot(Transpose(SubMatrix<1,1,1,1,1,3>(Transpose(vv))), u);
          SpGradient f48 = Dot(Transpose(SubMatrix<2,1,1,1,1,3>(Transpose(vv))), u) / 2.;
          SpGradient f49 = Dot(u, Transpose(SubMatrix<1,1,1,1,1,3>(Transpose(vv))));
          SpGradient f50 = Dot(u, Transpose(SubMatrix<2,1,1,1,1,3>(Transpose(vv)))) / 2.;
          SpGradient f51 = Dot(u / 2., Transpose(SubMatrix<1,1,1,1,1,3>(Transpose(vv))) * 2.);
          SpGradient f52 = Dot(u, Transpose(SubMatrix<2,1,1,1,1,3>(Transpose(vv))) / 2.);

          std::vector<SpDerivRec> rgDer;
          rgDer.reserve(6);

          doublereal fval = 0.;

          for (index_type i = 1; i <= 3; ++i) {
               const doublereal ui = 10. * i;
               const doublereal vi = 3. * i;
               const doublereal ud = 7.;
               const doublereal vd = -4.;

               fval += ui * vi;

               rgDer.emplace_back(i, ud * vi);
               rgDer.emplace_back(i + 3, ui * vd);
          }

          const doublereal dTol = sqrt(std::numeric_limits<doublereal>::epsilon());

          SpGradient fref(fval, rgDer);

          sp_grad_assert_equal(f1, fref, dTol);
          sp_grad_assert_equal(f2, fref, dTol);
          sp_grad_assert_equal(f3, fref, dTol);
          sp_grad_assert_equal(f4, fref, dTol);
          sp_grad_assert_equal(f5, fref, dTol);
          sp_grad_assert_equal(f6, fref, dTol);
          sp_grad_assert_equal(f7, fref, dTol);
          sp_grad_assert_equal(f8, fref, dTol);
          sp_grad_assert_equal(f9, fref, dTol);
          sp_grad_assert_equal(f10, fref, dTol);
          sp_grad_assert_equal(f11, fref, dTol);
          sp_grad_assert_equal(f12, fref, dTol);
          sp_grad_assert_equal(f13, fref, dTol);
          sp_grad_assert_equal(f14, fref, dTol);
          sp_grad_assert_equal(f15, fref, dTol);
          sp_grad_assert_equal(f16, fref, dTol);
          sp_grad_assert_equal(f17, fref, dTol);
          sp_grad_assert_equal(f18, fref, dTol);
          sp_grad_assert_equal(f19, fref, dTol);
          sp_grad_assert_equal(f20, fref, dTol);
          sp_grad_assert_equal(f21, fref, dTol);
          sp_grad_assert_equal(f22, fref, dTol);
          sp_grad_assert_equal(f23, fref, dTol);
          sp_grad_assert_equal(f24, fref, dTol);
          sp_grad_assert_equal(f25, fref, dTol);
          sp_grad_assert_equal(f26, fref, dTol);
          sp_grad_assert_equal(f27, fref, dTol);
          sp_grad_assert_equal(f28, fref, dTol);
          sp_grad_assert_equal(f29, fref, dTol);
          sp_grad_assert_equal(f30, fref, dTol);
          sp_grad_assert_equal(f31, fref, dTol);
          sp_grad_assert_equal(f32, fref, dTol);
          sp_grad_assert_equal(f33, fref, dTol);
          sp_grad_assert_equal(f34, fref, dTol);
          sp_grad_assert_equal(f35, fref, dTol);
          sp_grad_assert_equal(f36, fref, dTol);
          sp_grad_assert_equal(f37, fref, dTol);
          sp_grad_assert_equal(f38, fref, dTol);
          sp_grad_assert_equal(f39, fref, dTol);
          sp_grad_assert_equal(f40, fref, dTol);
          sp_grad_assert_equal(f41, fref, dTol);
          sp_grad_assert_equal(f42, fref, dTol);
          sp_grad_assert_equal(f43, fref, dTol);
          sp_grad_assert_equal(f44, fref, dTol);
          sp_grad_assert_equal(f45, fref, dTol);
          sp_grad_assert_equal(f46, fref, dTol);
          sp_grad_assert_equal(f47, fref, dTol);
          sp_grad_assert_equal(f48, fref, dTol);
          sp_grad_assert_equal(f49, fref, dTol);
          sp_grad_assert_equal(f50, fref, dTol);
          sp_grad_assert_equal(f51, fref, dTol);
          sp_grad_assert_equal(f52, fref, dTol);
     }

     void test19b()
     {
          using namespace std;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          SpColVector<SpGradient, 3> u(3, 1);
          SpColVector<doublereal, 3> v(3, 1);

          for (index_type i = 1; i <= 3; ++i) {
               u(i).Reset(10. * i, i, 7.);
               v(i) = 3. * i;
          }

          SpRowVector<doublereal, 3> vT = Transpose(v);

          SpGradient f1 = Dot(u, v);
          SpGradient f2 = Dot(v, u);
          SpGradient f3 = Dot(2. * u, v) / 2.;
          SpGradient f4 = Dot(2. * v, u) / 2.;
          SpGradient f5 = Dot(2. * u, 3. * v) / 6.;
          SpGradient f6 = Dot(3. * v, 2. * u) / 6.;
          SpGradient f7 = (1./6.) * Dot(u * 2., v * 3.);
          SpGradient f8 = (1./6.) * Dot(v * 3., u * 2.);
          SpGradient f9 = Dot(u, Transpose(vT));
          SpGradient f10 = Dot(Transpose(vT), u);
          SpGradient f11 = Dot(2. * u, Transpose(vT)) / 2.;
          SpGradient f12 = Dot(2. * Transpose(vT), u) / 2.;
          SpGradient f13 = Dot(2. * u, 3. * Transpose(vT)) / 6.;
          SpGradient f14 = Dot(3. * Transpose(vT), 2. * u) / 6.;
          SpGradient f15 = (1./6.) * Dot(u * 2., Transpose(vT) * 3.);
          SpGradient f16 = (1./6.) * Dot(Transpose(vT) * 3., u * 2.);

          SpGradient f17 = Dot(EvalUnique(u), v);
          SpGradient f18 = Dot(EvalUnique(v), u);
          SpGradient f19 = Dot(2. * EvalUnique(u), v) / 2.;
          SpGradient f20 = Dot(2. * EvalUnique(v), u) / 2.;
          SpGradient f21 = Dot(2. * EvalUnique(u), 3. * v) / 6.;
          SpGradient f22 = Dot(3. * EvalUnique(v), 2. * u) / 6.;
          SpGradient f23 = (1./6.) * Dot(EvalUnique(u) * 2., v * 3.);
          SpGradient f24 = (1./6.) * Dot(EvalUnique(v) * 3., u * 2.);

          SpGradient f25 = Dot(EvalUnique(u), Transpose(vT));
          SpGradient f26 = Dot(Transpose(vT), EvalUnique(u));
          SpGradient f27 = Dot(2. * u, EvalUnique(Transpose(vT))) / 2.;
          SpGradient f28 = Dot(2. * Transpose(vT), EvalUnique(u)) / 2.;
          SpGradient f29 = Dot(2. * EvalUnique(u), 3. * Transpose(vT)) / 6.;
          SpGradient f30 = Dot(3. * Transpose(vT), 2. * u) / 6.;
          SpGradient f31 = (1./6.) * Dot(EvalUnique(u) * 2., Transpose(vT) * 3.);
          SpGradient f32 = (1./6.) * EvalUnique(Dot(Transpose(vT) * 3., u * 2.));

          std::vector<SpDerivRec> rgDer;
          rgDer.reserve(6);

          doublereal fval = 0.;

          for (index_type i = 1; i <= 3; ++i) {
               const doublereal ui = 10. * i;
               const doublereal vi = 3. * i;
               const doublereal ud = 7.;
               const doublereal vd = 0.;

               fval += ui * vi;

               rgDer.emplace_back(i, ud * vi);
               rgDer.emplace_back(i + 3, ui * vd);
          }

          const doublereal dTol = sqrt(std::numeric_limits<doublereal>::epsilon());

          SpGradient fref(fval, rgDer);

          sp_grad_assert_equal(f1, fref, dTol);
          sp_grad_assert_equal(f2, fref, dTol);
          sp_grad_assert_equal(f3, fref, dTol);
          sp_grad_assert_equal(f4, fref, dTol);
          sp_grad_assert_equal(f5, fref, dTol);
          sp_grad_assert_equal(f6, fref, dTol);
          sp_grad_assert_equal(f7, fref, dTol);
          sp_grad_assert_equal(f8, fref, dTol);
          sp_grad_assert_equal(f9, fref, dTol);
          sp_grad_assert_equal(f10, fref, dTol);
          sp_grad_assert_equal(f11, fref, dTol);
          sp_grad_assert_equal(f12, fref, dTol);
          sp_grad_assert_equal(f13, fref, dTol);
          sp_grad_assert_equal(f14, fref, dTol);
          sp_grad_assert_equal(f15, fref, dTol);
          sp_grad_assert_equal(f16, fref, dTol);

          sp_grad_assert_equal(f17, fref, dTol);
          sp_grad_assert_equal(f18, fref, dTol);
          sp_grad_assert_equal(f19, fref, dTol);
          sp_grad_assert_equal(f20, fref, dTol);
          sp_grad_assert_equal(f21, fref, dTol);
          sp_grad_assert_equal(f22, fref, dTol);
          sp_grad_assert_equal(f23, fref, dTol);
          sp_grad_assert_equal(f24, fref, dTol);
          sp_grad_assert_equal(f25, fref, dTol);
          sp_grad_assert_equal(f26, fref, dTol);
          sp_grad_assert_equal(f27, fref, dTol);
          sp_grad_assert_equal(f28, fref, dTol);
          sp_grad_assert_equal(f29, fref, dTol);
          sp_grad_assert_equal(f30, fref, dTol);
          sp_grad_assert_equal(f31, fref, dTol);
          sp_grad_assert_equal(f32, fref, dTol);
     }

     void test19c()
     {
          using namespace std;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          SpColVector<SpGradient> u(3, 1), v(3, 1);

          for (index_type i = 1; i <= 3; ++i) {
               u(i).Reset(10. * i, i, 7.);
               v(i).Reset(3. * i, i + 3, -4.);
          }

          SpMatrix<SpGradient> vv(3, 2, 1);

          for (index_type i = 1; i <= 3; ++i) {
               for (index_type j = 1; j <= 2; ++j) {
                    vv(i, j) = v(i) * j;
               }
          }

          SpMatrix<SpGradient> vvT = Transpose(vv);
          SpRowVector<SpGradient> vT = Transpose(v);

          SpGradient f1 = Dot(u, v);
          SpGradient f2 = Dot(v, u);
          SpGradient f3 = Dot(2. * u, v) / 2.;
          SpGradient f4 = Dot(2. * v, u) / 2.;
          SpGradient f5 = Dot(2. * u, 3. * v) / 6.;
          SpGradient f6 = Dot(3. * v, 2. * u) / 6.;
          SpGradient f7 = (1./6.) * Dot(u * 2., v * 3.);
          SpGradient f8 = (1./6.) * Dot(v * 3., u * 2.);

          SpGradient f9 = Dot(u, Transpose(vT));
          SpGradient f10 = Dot(Transpose(vT), u);
          SpGradient f11 = Dot(2. * u, Transpose(vT)) / 2.;
          SpGradient f12 = Dot(2. * Transpose(vT), u) / 2.;
          SpGradient f13 = Dot(2. * u, 3. * Transpose(vT)) / 6.;
          SpGradient f14 = Dot(3. * Transpose(vT), 2. * u) / 6.;
          SpGradient f15 = (1./6.) * Dot(u * 2., Transpose(vT) * 3.);
          SpGradient f16 = (1./6.) * Dot(Transpose(vT) * 3., u * 2.);

          SpGradient f17 = Dot(EvalUnique(u), v);
          SpGradient f18 = Dot(EvalUnique(v), u);
          SpGradient f19 = Dot(2. * EvalUnique(u), v) / 2.;
          SpGradient f20 = Dot(2. * EvalUnique(v), u) / 2.;
          SpGradient f21 = Dot(2. * EvalUnique(u), 3. * v) / 6.;
          SpGradient f22 = Dot(3. * EvalUnique(v), 2. * u) / 6.;
          SpGradient f23 = (1./6.) * Dot(EvalUnique(u) * 2., v * 3.);
          SpGradient f24 = (1./6.) * Dot(EvalUnique(v) * 3., u * 2.);

          SpGradient f25 = Dot(EvalUnique(u), Transpose(vT));
          SpGradient f26 = Dot(Transpose(vT), EvalUnique(u));
          SpGradient f27 = Dot(2. * u, EvalUnique(Transpose(vT))) / 2.;
          SpGradient f28 = Dot(2. * Transpose(vT), EvalUnique(u)) / 2.;
          SpGradient f29 = Dot(2. * EvalUnique(u), 3. * Transpose(vT)) / 6.;
          SpGradient f30 = Dot(3. * Transpose(vT), 2. * u) / 6.;
          SpGradient f31 = (1./6.) * Dot(EvalUnique(u) * 2., Transpose(vT) * 3.);
          SpGradient f32 = (1./6.) * EvalUnique(Dot(Transpose(vT) * 3., u * 2.));

          SpGradient f33 = Dot(vv.GetCol(1), u);
          SpGradient f34 = Dot(vv.GetCol(2), u) / 2.;
          SpGradient f35 = Dot(u, vv.GetCol(1));
          SpGradient f36 = Dot(u, vv.GetCol(2)) / 2.;
          SpGradient f37 = Dot(u / 2., vv.GetCol(2));
          SpGradient f38 = Dot(u, vv.GetCol(2) / 2.);

          SpGradient f39 = Dot(Transpose(vvT.GetRow(1)), u);
          SpGradient f40 = Dot(Transpose(vvT.GetRow(2)), u / 2.);
          SpGradient f41 = Dot(u, Transpose(vvT.GetRow(1)));
          SpGradient f42 = Dot(u / 2., Transpose(vvT.GetRow(2)));
          SpGradient f43 = Dot(Transpose(vvT.GetRow(1)) / 2., u * 2.);
          SpGradient f44 = Dot(Transpose(vvT.GetRow(2)) / 2., u);
          SpGradient f45 = Dot(u / 2., Transpose(vvT.GetRow(1)) * 2.);
          SpGradient f46 = Dot(u, Transpose(vvT.GetRow(2)) / 2.);

          std::vector<SpDerivRec> rgDer;
          rgDer.reserve(6);

          doublereal fval = 0.;

          for (index_type i = 1; i <= 3; ++i) {
               const doublereal ui = 10. * i;
               const doublereal vi = 3. * i;
               const doublereal ud = 7.;
               const doublereal vd = -4.;

               fval += ui * vi;

               rgDer.emplace_back(i, ud * vi);
               rgDer.emplace_back(i + 3, ui * vd);
          }

          const doublereal dTol = sqrt(std::numeric_limits<doublereal>::epsilon());

          SpGradient fref(fval, rgDer);

          sp_grad_assert_equal(f1, fref, dTol);
          sp_grad_assert_equal(f2, fref, dTol);
          sp_grad_assert_equal(f3, fref, dTol);
          sp_grad_assert_equal(f4, fref, dTol);
          sp_grad_assert_equal(f5, fref, dTol);
          sp_grad_assert_equal(f6, fref, dTol);
          sp_grad_assert_equal(f7, fref, dTol);
          sp_grad_assert_equal(f8, fref, dTol);
          sp_grad_assert_equal(f9, fref, dTol);
          sp_grad_assert_equal(f10, fref, dTol);
          sp_grad_assert_equal(f11, fref, dTol);
          sp_grad_assert_equal(f12, fref, dTol);
          sp_grad_assert_equal(f13, fref, dTol);
          sp_grad_assert_equal(f14, fref, dTol);
          sp_grad_assert_equal(f15, fref, dTol);
          sp_grad_assert_equal(f16, fref, dTol);
          sp_grad_assert_equal(f17, fref, dTol);
          sp_grad_assert_equal(f18, fref, dTol);
          sp_grad_assert_equal(f19, fref, dTol);
          sp_grad_assert_equal(f20, fref, dTol);
          sp_grad_assert_equal(f21, fref, dTol);
          sp_grad_assert_equal(f22, fref, dTol);
          sp_grad_assert_equal(f23, fref, dTol);
          sp_grad_assert_equal(f24, fref, dTol);
          sp_grad_assert_equal(f25, fref, dTol);
          sp_grad_assert_equal(f26, fref, dTol);
          sp_grad_assert_equal(f27, fref, dTol);
          sp_grad_assert_equal(f28, fref, dTol);
          sp_grad_assert_equal(f29, fref, dTol);
          sp_grad_assert_equal(f30, fref, dTol);
          sp_grad_assert_equal(f31, fref, dTol);
          sp_grad_assert_equal(f32, fref, dTol);
          sp_grad_assert_equal(f33, fref, dTol);
          sp_grad_assert_equal(f34, fref, dTol);
          sp_grad_assert_equal(f35, fref, dTol);
          sp_grad_assert_equal(f36, fref, dTol);
          sp_grad_assert_equal(f37, fref, dTol);
          sp_grad_assert_equal(f38, fref, dTol);
          sp_grad_assert_equal(f39, fref, dTol);
          sp_grad_assert_equal(f40, fref, dTol);
          sp_grad_assert_equal(f41, fref, dTol);
          sp_grad_assert_equal(f42, fref, dTol);
          sp_grad_assert_equal(f43, fref, dTol);
          sp_grad_assert_equal(f44, fref, dTol);
          sp_grad_assert_equal(f45, fref, dTol);
          sp_grad_assert_equal(f46, fref, dTol);
     }


     void test20()
     {
          using namespace std;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          Mat3x3 A1;

          for (index_type i = 1; i <= A1.iGetNumRows(); ++i) {
               for (index_type j = 1; j <= A1.iGetNumCols(); ++j) {
                    A1(i, j) = 10. * i + j;
               }
          }

          Vec3 C1 = A1.GetCol(2);
          Vec3 D1 = A1.GetCol(3);

          SpMatrix<doublereal, 3, 3> A2(A1);

          SpColVector<doublereal, 3> C2(A2.GetCol(2));
          SpColVector<doublereal, 3> D2(A2.GetCol(3));

          Vec3 C3(C2);
          Vec3 D3(D2);

          MBDYN_TESTSUITE_ASSERT(C3.IsExactlySame(C1));
          MBDYN_TESTSUITE_ASSERT(D3.IsExactlySame(D1));
          MBDYN_TESTSUITE_ASSERT(Dot(C2, D2) == C1.Dot(D1));

          Mat3x3 A3(A2);

          MBDYN_TESTSUITE_ASSERT(A3.IsExactlySame(A1));

          Mat3x3 B1 = A1 * A1.Transpose();
          SpMatrix<doublereal, 3, 3> B2 = A2 * Transpose(A2);
          Mat3x3 B3(B2);

          MBDYN_TESTSUITE_ASSERT(B3.IsExactlySame(B1));
     }

     void test20a()
     {
          using namespace std;

          cerr << __PRETTY_FUNCTION__ << ":\n";

          Mat6x6 A1;

          for (index_type i = 1; i <= A1.iGetNumRows(); ++i) {
               for (index_type j = 1; j <= A1.iGetNumCols(); ++j) {
                    A1(i, j) = 10. * i + j;
               }
          }

          Vec6 C1;
          Vec6 D1;

          for (index_type i = 1; i <= A1.iGetNumRows(); ++i) {
               C1(i) = A1(i, 2);
               D1(i) = A1(i, 3);
          }

          SpMatrix<doublereal, 6, 6> A2(A1);

          SpColVector<doublereal, 6> C2(A2.GetCol(2));
          SpColVector<doublereal, 6> D2(A2.GetCol(3));

          Vec6 C3(C2);
          Vec6 D3(D2);

          MBDYN_TESTSUITE_ASSERT(C3.IsExactlySame(C1));
          MBDYN_TESTSUITE_ASSERT(D3.IsExactlySame(D1));
          MBDYN_TESTSUITE_ASSERT(Dot(C2, D2) == C1.Dot(D1));

          Mat6x6 A3(A2);

          MBDYN_TESTSUITE_ASSERT(A3.IsExactlySame(A1));

          Mat6x6 B1 = A1 * A1.Transpose();
          SpMatrix<doublereal, 6, 6> B2 = A2 * Transpose(A2);
          Mat6x6 B3(B2);

          MBDYN_TESTSUITE_ASSERT(B3.IsExactlySame(B1));
     }

     void check_PrincipalAxes(const Vec3& J_princ, const Mat3x3& R_princ, const Vec3& Jp, const Mat3x3& J_cm, const doublereal dTol)
     {
          MBDYN_TESTSUITE_ASSERT(fabs(R_princ.GetCol(1).Dot() - 1.) < dTol);
          MBDYN_TESTSUITE_ASSERT(fabs(R_princ.GetCol(2).Dot() - 1.) < dTol);
          MBDYN_TESTSUITE_ASSERT(fabs(R_princ.GetCol(3).Dot() - 1.) < dTol);
          MBDYN_TESTSUITE_ASSERT(fabs(R_princ.GetCol(1).Dot(R_princ.GetCol(2))) < dTol);
          MBDYN_TESTSUITE_ASSERT(fabs(R_princ.GetCol(1).Dot(R_princ.GetCol(3))) < dTol);
          MBDYN_TESTSUITE_ASSERT(fabs(R_princ.GetCol(2).Dot(R_princ.GetCol(3))) < dTol);
          MBDYN_TESTSUITE_ASSERT(fabs(J_cm.Trace() - Mat3x3(Mat3x3Diag, J_princ).Trace()) < dTol * fabs(J_cm.Trace()));
          MBDYN_TESTSUITE_ASSERT(R_princ.GetCol(1).Cross(R_princ.GetCol(2)).IsSame(R_princ.GetCol(3), dTol));
          MBDYN_TESTSUITE_ASSERT(R_princ.GetCol(3).Cross(R_princ.GetCol(1)).IsSame(R_princ.GetCol(2), dTol));
          MBDYN_TESTSUITE_ASSERT(R_princ.GetCol(2).Cross(R_princ.GetCol(3)).IsSame(R_princ.GetCol(1), dTol));
          MBDYN_TESTSUITE_ASSERT(R_princ.MulTM(R_princ).IsSame(Eye3, dTol));
          MBDYN_TESTSUITE_ASSERT(R_princ.MulMT(R_princ).IsSame(Eye3, dTol));
          MBDYN_TESTSUITE_ASSERT(R_princ.MulTM(J_cm * R_princ).IsSame(Mat3x3(Mat3x3Diag, J_princ), dTol * J_princ.Norm()) || (R_princ*J_cm.MulMT(R_princ)).IsSame(Mat3x3(Mat3x3Diag, J_princ), dTol * J_princ.Norm()));
          MBDYN_TESTSUITE_ASSERT(R_princ.IsSame(RotManip::Rot(RotManip::VecRot(R_princ)), dTol * M_PI));
          MBDYN_TESTSUITE_ASSERT(J_princ.IsSame(Jp, dTol * Jp.Norm()));
     }

     void test21(index_type num_loops)
     {
          using namespace std;

          random_device rd;
          mt19937 gen(rd());
          uniform_real_distribution<doublereal> randnorm(-1., 1.);
          uniform_real_distribution<doublereal> randphi(-M_PI, M_PI);

          const Vec3 Jp(1., 2., 3.);
          const Mat3x3 J0(Mat3x3Diag, Jp);
          const doublereal dTol = pow(std::numeric_limits<doublereal>::epsilon(), 0.9);

          for (index_type iloop = 0; iloop < num_loops; ++iloop) {
               Vec3 n;

               for (index_type i = 1; i <= 3; ++i) {
                    n(i) = randnorm(gen);
               }

               if (n.Norm() == 0.) {
                    continue;
               }

               n /= n.Norm();

               const doublereal phi = randphi(gen);

               const Mat3x3 R1 = RotManip::Rot(n * phi);
               const Mat3x3 R2 = R1.Transpose();
               const Mat3x3 J1 = R1.MulTM(J0 * R1).Symm();
               const Mat3x3 J2 = R2.MulTM(J0 * R2).Symm();

               static constexpr doublereal s1[] = {1., -1., -1., 1.};
               static constexpr doublereal s2[] = {1., 1., -1., -1.};

               Mat3x3 R_princ1[4], R_princ2[4];
               Vec3 J_princ1, J_princ2;

               const bool status1 = J1.PrincipalAxes(J_princ1, R_princ1[0]);
               const bool status2 = J2.PrincipalAxes(J_princ2, R_princ2[0]);

               for (index_type i = 1; i < 4; ++i) {
                    for (index_type j = 1; j <= 3; ++j) {
                         R_princ1[i](j, 1) = R_princ1[0](j, 1) * s1[i];
                         R_princ1[i](j, 2) = R_princ1[0](j, 2) * s2[i];
                         R_princ1[i](j, 3) = R_princ1[0](j, 3);

                         R_princ2[i](j, 1) = R_princ2[0](j, 1) * s1[i];
                         R_princ2[i](j, 2) = R_princ2[0](j, 2) * s2[i];
                         R_princ2[i](j, 3) = R_princ2[0](j, 3);
                    }

                    Vec3 e1 = R_princ1[i].GetCol(1);
                    Vec3 e2 = R_princ1[i].GetCol(2);
                    Vec3 e3 = R_princ1[i].GetCol(3);

                    if ((e1.Cross(e2) - e3).Norm() > (e1.Cross(e2) + e3).Norm()) {
                         for (index_type j = 1; j <= 3; ++j) {
                              R_princ1[i](j, 3) *= -1;
                         }
                    }

                    e1 = R_princ2[i].GetCol(1);
                    e2 = R_princ2[i].GetCol(2);
                    e3 = R_princ2[i].GetCol(3);

                    if ((e1.Cross(e2) - e3).Norm() > (e1.Cross(e2) + e3).Norm()) {
                         for (index_type j = 1; j <= 3; ++j) {
                              R_princ2[i](j, 3) *= -1;
                         }
                    }
               }

               MBDYN_TESTSUITE_ASSERT(status1);
               MBDYN_TESTSUITE_ASSERT(status2);

               for (index_type i = 0; i < 4; ++i) {
                    check_PrincipalAxes(J_princ1, R_princ1[i], Jp, J1, dTol);
                    check_PrincipalAxes(J_princ2, R_princ2[i], Jp, J2, dTol);
               }

               bool bValid1 = false;

               Mat3x3 R_princ1_v;

               for (index_type i = 0; i < 4; ++i) {
                    if (R_princ1[i].IsSame(R1, dTol)) {
                         R_princ1_v = R_princ1[i];
                         bValid1 = true;
                         break;
                    }

                    if (R_princ1[i].Transpose().IsSame(R1, dTol)) {
                         R_princ1_v = R_princ1[i].Transpose();
                         bValid1 = true;
                         break;
                    }
               }

               MBDYN_TESTSUITE_ASSERT(bValid1);

               bool bValid2 = false;

               Mat3x3 R_princ2_v;

               for (index_type i = 0; i < 4; ++i) {
                    if (R_princ2[i].IsSame(R2, dTol)) {
                         R_princ2_v = R_princ2[i];
                         bValid2 = true;
                         break;
                    }

                    if (R_princ2[i].Transpose().IsSame(R2, dTol)) {
                         R_princ2_v = R_princ2[i].Transpose();
                         bValid2 = true;
                         break;
                    }
               }

               MBDYN_TESTSUITE_ASSERT(bValid2);

               check_PrincipalAxes(J_princ1, R_princ1_v, Jp, J1, dTol);
               check_PrincipalAxes(J_princ2, R_princ2_v, Jp, J2, dTol);
          }
     }

     template <typename T>
     T constexpr sqrtint(T x) {
          T sqx = 2;

          while (4 * sqx * sqx < x) {
               sqx *= 2;
          }

          return sqx;
     }

     void test22()
     {
          bool bCaughtBadAlloc = false;

          try {
               // FIXME: Divide by two in order to avoid errors incorrectly reported by valgrind:
               constexpr size_t uSizeAlloc = std::numeric_limits<size_t>::max() / 2;
               constexpr size_t uNumDeriv = std::numeric_limits<index_type>::max();
               constexpr size_t uNumItems = (uSizeAlloc - sizeof(SpMatrix<SpGradient>)) / (uNumDeriv * sizeof(SpDerivRec) + sizeof(SpGradient));
               constexpr index_type iNumRows = sqrtint(uNumItems);
               constexpr index_type iNumCols = iNumRows;

               SpMatrix<SpGradient> A(iNumRows, iNumCols, uNumDeriv);
          } catch (const std::bad_alloc& err) {
               bCaughtBadAlloc = true;
          }

          MBDYN_TESTSUITE_ASSERT(bCaughtBadAlloc);
     }

     void test23() {
#if _POSIX_C_SOURCE >= 200809L
          constexpr index_type iNumDeriv = std::numeric_limits<index_type>::max();

          rlimit rlimprev;

          if (0 != getrlimit(RLIMIT_DATA, &rlimprev)) {
               DEBUGCERR("getrlimit failed\n");
               MBDYN_TESTSUITE_ASSERT(0);
               return;
          }

          rlimit rlimnew = rlimprev;

          rlimnew.rlim_cur = std::min(rlimnew.rlim_cur, iNumDeriv * sizeof(SpDerivRec) / 8);

          if (0 != setrlimit(RLIMIT_DATA, &rlimnew)) {
               DEBUGCERR("setrlimit failed\n");
               MBDYN_TESTSUITE_ASSERT(0);
               return;
          }

          bool bCaughtBadAlloc = false;

          try {
               SpGradient g;
               g.ResizeReset(1., iNumDeriv);
          } catch (const std::bad_alloc&) {
               bCaughtBadAlloc = true;
          }

          if (0 != setrlimit(RLIMIT_DATA, &rlimprev)) {
               DEBUGCERR("setrlimit failed\n");
               MBDYN_TESTSUITE_ASSERT(0);
          }

          MBDYN_TESTSUITE_ASSERT(bCaughtBadAlloc);
#else
          DEBUGCERR("test23 will be skipped because setrlimit is not available\n");
#endif
     }
}

namespace {
     namespace sp_gradient_test_parameters {
          using namespace sp_grad_test;
          index_type inumloops = 1;
          index_type inumnz = 100;
          index_type inumdof = 200;
          index_type imatrows = 10;
          index_type imatcols = 10;
          index_type imatcolsb = 5;
          index_type imatcolsc = 7;
     }
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test_binary_io) {
     using namespace sp_grad;

     SpMatrix<doublereal, 3, 3> A(3, 3, 0);

     for (index_type i = 1; i <= 3; ++i) {
          for (index_type j = 1; j <= 3; ++j) {
               A(i, j) = 10 * i + j;
          }
     }

     Mat3x3 B;
     Vec3 C;

     for (index_type i = 1; i <= 3; ++i) {
          C(i) = 200 * i;

          for (index_type j = 1; j <= 3; ++j) {
               B(i, j) = 20 * i + 2 * j;
          }
     }

     Vec6 D;
     Mat6x6 E;

     for (index_type i = 1; i <= 6; ++i) {
          D(i) = 200 * i;

          for (index_type j = 1; j <= 6; ++j) {
               E(i, j) = 200 * i + 20 * j;
          }
     }

     SpColVector<doublereal, 6> F(6, 0);

     for (index_type i = 1; i <= F.iGetNumRows(); ++i) {
          F(i) = 1000. * i + 1.;
     }

     std::vector<doublereal> v1;
     v1.reserve(10);

     for (integer i = 0; i < 10; ++i) {
          v1.push_back(10. * i + 0.5);
     }

     std::array<doublereal, 10> a1;

     for (integer i = 0; i < 10; ++i) {
          a1[i] = 20. * i + 0.5;
     }

     std::array<Mat3x3, 4> R1;

     for (integer i = 0; i < 4; ++i) {
          R1[i] = Eye3;
     }
     
     RestartData oRestartData1;

     oRestartData1.Sync(RestartData::NODES_STRUCT, 1u, "A", A, RestartData::RESTART_SAVE);
     oRestartData1.Sync(RestartData::NODES_STRUCT, 1u, "B", B, RestartData::RESTART_SAVE);
     oRestartData1.Sync(RestartData::NODES_STRUCT, 1u, "C", C, RestartData::RESTART_SAVE);
     oRestartData1.Sync(RestartData::NODES_STRUCT, 1u, "D", D, RestartData::RESTART_SAVE);
     oRestartData1.Sync(RestartData::NODES_STRUCT, 1u, "E", E, RestartData::RESTART_SAVE);
     oRestartData1.Sync(RestartData::ELEM_SOLIDS, 1u, "F", F, RestartData::RESTART_SAVE);
     oRestartData1.Sync(RestartData::NODES_STRUCT, 1u, "v", v1, RestartData::RESTART_SAVE);
     oRestartData1.Sync(RestartData::NODES_STRUCT, 2u, "a", a1, RestartData::RESTART_SAVE);
     oRestartData1.Sync(RestartData::NODES_STRUCT, 3u, "R", R1, RestartData::RESTART_SAVE);
     
     char szFileName[TMP_MAX];
     const std::string strFileName = tmpnam(szFileName);

     oRestartData1.WriteFile(strFileName);

     RestartData oRestartData2;

     oRestartData2.ReadFile(strFileName);

     SpMatrix<doublereal, 3, 3> Aout(3, 3, 0);
     Mat3x3 Bout;
     Vec3 Cout;
     Vec6 Dout;
     Mat6x6 Eout;
     SpColVector<doublereal, 6> Fout(6, 0);
     std::vector<doublereal> v2;
     std::array<doublereal, 10> a2;
     std::array<Mat3x3, 4> R2;
     
     oRestartData2.Sync(RestartData::NODES_STRUCT, 1u, "A", Aout, RestartData::RESTART_RESTORE);
     oRestartData2.Sync(RestartData::NODES_STRUCT, 1u, "B", Bout, RestartData::RESTART_RESTORE);
     oRestartData2.Sync(RestartData::NODES_STRUCT, 1u, "C", Cout, RestartData::RESTART_RESTORE);
     oRestartData2.Sync(RestartData::NODES_STRUCT, 1u, "D", Dout, RestartData::RESTART_RESTORE);
     oRestartData2.Sync(RestartData::NODES_STRUCT, 1u, "E", Eout, RestartData::RESTART_RESTORE);
     oRestartData2.Sync(RestartData::ELEM_SOLIDS, 1u, "F", Fout, RestartData::RESTART_RESTORE);
     oRestartData2.Sync(RestartData::NODES_STRUCT, 1u, "v", v2, RestartData::RESTART_RESTORE);
     oRestartData2.Sync(RestartData::NODES_STRUCT, 2u, "a", a2, RestartData::RESTART_RESTORE);
     oRestartData2.Sync(RestartData::NODES_STRUCT, 3u, "R", R2, RestartData::RESTART_RESTORE);
     
     for (index_type i = 1; i <= 3; ++i) {
          for (index_type j = 1; j <= 3; ++j) {
               MBDYN_TESTSUITE_ASSERT(A(i, j) == Aout(i, j));
               MBDYN_TESTSUITE_ASSERT(B(i, j) == Bout(i, j));
          }
          MBDYN_TESTSUITE_ASSERT(C(i) == Cout(i));
     }

     for (index_type i = 1; i <= 6; ++i) {
          for (index_type j = 1; j <= 6; ++j) {
               MBDYN_TESTSUITE_ASSERT(E(i, j) == Eout(i, j));
          }
          MBDYN_TESTSUITE_ASSERT(D(i) == Dout(i));
          MBDYN_TESTSUITE_ASSERT(F(i) == Fout(i));
     }

     for (size_t i = 0; i < v2.size(); ++i) {
          MBDYN_TESTSUITE_ASSERT(v2[i] == v1[i]);
     }

     for (size_t i = 0; i < a2.size(); ++i) {
          MBDYN_TESTSUITE_ASSERT(a2[i] == a1[i]);
     }

     for (size_t i = 0; i < R1.size(); ++i) {
          MBDYN_TESTSUITE_ASSERT(R2[i].IsSame(R1[i], 0.));
     }
#ifdef HAVE_UNISTD_H
     unlink(strFileName.c_str());
#endif
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, testx) {
     using namespace sp_grad_test;
     testx();
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test0) {
     using namespace sp_gradient_test_parameters;

     test0(inumloops, inumnz, inumdof);
     test0gp();
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test1) {
     using namespace sp_grad_test;

     test1();
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test2) {
     using namespace sp_gradient_test_parameters;

     test2(inumloops, inumnz, inumdof);
     test2gp(inumloops, inumnz, inumdof);
}


MBDYN_TESTSUITE_TEST(sp_gradient_test, test3) {
     using namespace sp_gradient_test_parameters;
     test3<SpGradient, SpGradient>(inumloops, inumnz, inumdof, imatrows);
     test3<doublereal, SpGradient>(inumloops, inumnz, inumdof, imatrows);
     test3<SpGradient, doublereal>(inumloops, inumnz, inumdof, imatrows);
     test3<doublereal, doublereal>(inumloops, inumnz, inumdof, imatrows);
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test4) {
     using namespace sp_gradient_test_parameters;
     test4<doublereal, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test4<doublereal, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test4<SpGradient, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test4<SpGradient, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols);
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test6) {
     using namespace sp_gradient_test_parameters;
     test6(inumloops, inumnz, inumdof, imatrows, imatcols);
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test7) {
     using namespace sp_gradient_test_parameters;
     test7<SpGradient, SpGradient, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test7<doublereal, doublereal, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test7<SpGradient, doublereal, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test7<doublereal, SpGradient, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test7<doublereal, doublereal, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test7<SpGradient, SpGradient, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test7<doublereal, SpGradient, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test7<SpGradient, doublereal, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols);

     test7<SpGradient, SpGradient, SpGradient, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);
     test7<doublereal, doublereal, doublereal, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);
     test7<SpGradient, doublereal, doublereal, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);
     test7<doublereal, SpGradient, doublereal, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);
     test7<doublereal, doublereal, SpGradient, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);
     test7<SpGradient, SpGradient, doublereal, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);
     test7<doublereal, SpGradient, SpGradient, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);
     test7<SpGradient, doublereal, SpGradient, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);

     test7<SpGradient, SpGradient, SpGradient, 7, 9>(inumloops, inumnz, inumdof, 7, 9);
     test7<doublereal, doublereal, doublereal, 7, 9>(inumloops, inumnz, inumdof, 7, 9);
     test7<SpGradient, doublereal, doublereal, 7, 9>(inumloops, inumnz, inumdof, 7, 9);
     test7<doublereal, SpGradient, doublereal, 7, 9>(inumloops, inumnz, inumdof, 7, 9);
     test7<doublereal, doublereal, SpGradient, 7, 9>(inumloops, inumnz, inumdof, 7, 9);
     test7<SpGradient, SpGradient, doublereal, 7, 9>(inumloops, inumnz, inumdof, 7, 9);
     test7<doublereal, SpGradient, SpGradient, 7, 9>(inumloops, inumnz, inumdof, 7, 9);
     test7<SpGradient, doublereal, SpGradient, 7, 9>(inumloops, inumnz, inumdof, 7, 9);
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test8) {
     using namespace sp_gradient_test_parameters;
     test8<SpGradient, SpGradient, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test8<doublereal, doublereal, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test8<SpGradient, doublereal, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test8<doublereal, SpGradient, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test8<doublereal, doublereal, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test8<SpGradient, SpGradient, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test8<doublereal, SpGradient, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols);
     test8<SpGradient, doublereal, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols);

     test8<SpGradient, SpGradient, SpGradient, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);
     test8<doublereal, doublereal, doublereal, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);
     test8<SpGradient, doublereal, doublereal, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);
     test8<doublereal, SpGradient, doublereal, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);
     test8<doublereal, doublereal, SpGradient, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);
     test8<SpGradient, SpGradient, doublereal, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);
     test8<doublereal, SpGradient, SpGradient, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);
     test8<SpGradient, doublereal, SpGradient, iNumRowsStatic1, iNumColsStatic1>(inumloops, inumnz, inumdof, iNumRowsStatic1, iNumColsStatic1);

     test8<SpGradient, SpGradient, SpGradient, iNumRowsStatic2, iNumColsStatic2>(inumloops, inumnz, inumdof, iNumRowsStatic2, iNumColsStatic2);
     test8<doublereal, doublereal, doublereal, iNumRowsStatic2, iNumColsStatic2>(inumloops, inumnz, inumdof, iNumRowsStatic2, iNumColsStatic2);
     test8<SpGradient, doublereal, doublereal, iNumRowsStatic2, iNumColsStatic2>(inumloops, inumnz, inumdof, iNumRowsStatic2, iNumColsStatic2);
     test8<doublereal, SpGradient, doublereal, iNumRowsStatic2, iNumColsStatic2>(inumloops, inumnz, inumdof, iNumRowsStatic2, iNumColsStatic2);
     test8<doublereal, doublereal, SpGradient, iNumRowsStatic2, iNumColsStatic2>(inumloops, inumnz, inumdof, iNumRowsStatic2, iNumColsStatic2);
     test8<SpGradient, SpGradient, doublereal, iNumRowsStatic2, iNumColsStatic2>(inumloops, inumnz, inumdof, iNumRowsStatic2, iNumColsStatic2);
     test8<doublereal, SpGradient, SpGradient, iNumRowsStatic2, iNumColsStatic2>(inumloops, inumnz, inumdof, iNumRowsStatic2, iNumColsStatic2);
     test8<SpGradient, doublereal, SpGradient, iNumRowsStatic2, iNumColsStatic2>(inumloops, inumnz, inumdof, iNumRowsStatic2, iNumColsStatic2);
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test10) {
     using namespace sp_gradient_test_parameters;
     test10<SpGradient, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols, imatcolsb);
     test10<doublereal, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols, imatcolsb);
     test10<SpGradient, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols, imatcolsb);
     test10<doublereal, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols, imatcolsb);
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test11) {
     using namespace sp_gradient_test_parameters;
     test11<SpGradient, SpGradient, SpGradient>(inumloops, inumnz, inumdof, imatrows);
     test11<doublereal, doublereal, doublereal>(inumloops, inumnz, inumdof, imatrows);
     test11<SpGradient, doublereal, doublereal>(inumloops, inumnz, inumdof, imatrows);
     test11<doublereal, SpGradient, doublereal>(inumloops, inumnz, inumdof, imatrows);
     test11<doublereal, doublereal, SpGradient>(inumloops, inumnz, inumdof, imatrows);
     test11<SpGradient, SpGradient, doublereal>(inumloops, inumnz, inumdof, imatrows);
     test11<doublereal, SpGradient, SpGradient>(inumloops, inumnz, inumdof, imatrows);
     test11<SpGradient, doublereal, SpGradient>(inumloops, inumnz, inumdof, imatrows);
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test12) {
     using namespace sp_gradient_test_parameters;
     test12<SpGradient, SpGradient, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols, imatcolsb, imatcolsc);
     test12<doublereal, doublereal, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols, imatcolsb, imatcolsc);
     test12<SpGradient, doublereal, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols, imatcolsb, imatcolsc);
     test12<doublereal, SpGradient, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols, imatcolsb, imatcolsc);
     test12<doublereal, doublereal, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols, imatcolsb, imatcolsc);
     test12<SpGradient, SpGradient, doublereal>(inumloops, inumnz, inumdof, imatrows, imatcols, imatcolsb, imatcolsc);
     test12<doublereal, SpGradient, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols, imatcolsb, imatcolsc);
     test12<SpGradient, doublereal, SpGradient>(inumloops, inumnz, inumdof, imatrows, imatcols, imatcolsb, imatcolsc);
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test13) {
     using namespace sp_gradient_test_parameters;
     test13<doublereal>(inumloops, inumnz, inumdof);
     test13<SpGradient>(inumloops, inumnz, inumdof);
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test_bool1) {
     using namespace sp_gradient_test_parameters;
     test_bool1(inumloops, inumnz, inumdof);
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test15) {
     using namespace sp_gradient_test_parameters;
     test15<doublereal>(inumloops, inumnz, inumdof);
     test15<SpGradient>(inumloops, inumnz, inumdof);
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test16) {
     using namespace sp_gradient_test_parameters;
     test16(inumloops, inumnz, inumdof, imatrows, imatcols);
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, testInv) {
     using namespace sp_gradient_test_parameters;
     testInv<doublereal, 2>(inumloops, inumnz, inumdof);
     testInv<doublereal, 3>(inumloops, inumnz, inumdof);
     testInv<SpGradient, 2>(inumloops, inumnz, inumdof);
     testInv<SpGradient, 3>(inumloops, inumnz, inumdof);
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test18) {
     using namespace sp_gradient_test_parameters;
     test18(inumloops, inumnz, inumdof);
     test18gp(inumloops, inumnz, inumdof);
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test19) {
     using namespace sp_gradient_test_parameters;
     test19();
     test19b();
     test19c();
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test20) {
     using namespace sp_gradient_test_parameters;
     test20();
     test20a();
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test21) {
     using namespace sp_gradient_test_parameters;
     test21(inumloops);
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test22) {
     using namespace sp_gradient_test_parameters;
     test22();
}

MBDYN_TESTSUITE_TEST(sp_gradient_test, test23) {
     using namespace sp_gradient_test_parameters;
     test23();
}

MBDYN_DEFINE_OPERATOR_NEW_DELETE

int main(int argc, char* argv[]) {
     MBDYN_TESTSUITE_INIT(&argc, argv);

#ifdef HAVE_FEENABLEEXCEPT
     feenableexcept(FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW | FE_UNDERFLOW);
#endif
     using namespace sp_grad_test;
     using namespace sp_gradient_test_parameters;

     inumloops = argc > 1 ? atoi(argv[1]) : 1;
     inumnz = argc > 2 ? atoi(argv[2]) : 100;
     inumdof = argc > 3 ? atoi(argv[3]) : 200;
     imatrows = argc > 4 ? atoi(argv[4]) : 10;
     imatcols = argc > 5 ? atoi(argv[5]) : 10;
     imatcolsb = argc > 6 ? atoi(argv[6]) : 5;
     imatcolsc = argc > 7 ? atoi(argv[7]) : 7;

     return MBDYN_RUN_ALL_TESTS();
}
