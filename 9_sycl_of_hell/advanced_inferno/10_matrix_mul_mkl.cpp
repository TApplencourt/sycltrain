//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <random>
#include <iostream>
#include <limits>
#include <CL/sycl.hpp>
#include "mkl_blas_sycl.hpp"
#include "mkl.h"

using namespace cl::sycl;

// Matrix size constants
#define SIZE     4800   // Must be a multiple of 8.
#define M        SIZE/8
#define N        SIZE/4
#define P        SIZE/2

// /**
//  * Verify results between any two matrices
//  */
// int VerifyResult(double *c_back, double *c_goldstd);

//////////////////////////////////////////////////////////////////////////////////////////

bool ValueSame(double a, double b) {
  return std::fabs(a-b) < 1.0e-08;  
}

int VerifyResult(double *c_A, double *c_B) {
  bool MismatchFound = false;

  for (size_t i=0; i < M; i++) {
    for (size_t j=0; j < P; j++) {
      if (!ValueSame(c_A[i*P+j], c_B[i*P+j])) {
	std::cout << "fail - The result is incorrect for element: [" << i << ", " << j
		  << "], expected: " << c_A[i*P+j] << " , but got: " << c_B[i*P+j]
		  << std::endl;
        MismatchFound = true;
      }
    }
  }

  if (!MismatchFound) {
    std::cout << "SUCCESS - The results are correct!" << std::endl;
    return 0;
  }
  else {
    std::cout << "FAIL - The results mis-match!" << std::endl;
    return -1;
  }

}

//////////////////////////////////////////////////////////////////////////////////////////

int main() {
  std::random_device rd;  //Will be used to obtain a seed for the random number engine
  std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
  std::uniform_real_distribution<> dis(1.0, 2.0);

  // C = alpha * op(A) * op(B)  + beta * C
  mkl::transpose transA = mkl::transpose::nontrans;
  mkl::transpose transB = mkl::transpose::nontrans;

  // matrix data sizes
  int m = M;
  int n = P;
  int k = N;

  // leading dimensions of data
  int ldA = k;
  int ldB = n;
  int ldC = n;

  // set scalar fp values
  double alpha = 1.0;
  double beta  = 0.0;

  // 1D arrays on host side
  double *A;
  double *B;
  double *C_onemkl, *C_serial, *C_cblas;

  A = new double[M*N]{};
  B = new double[N*P]{};
  C_cblas = new double[M*P]{};
  C_serial = new double[M*P]{};
  C_onemkl = new double[M*P]{};  

  // prepare matrix data with ROW-major style
  // A(M, N)
  for (size_t i=0; i<M; i++)
    for (size_t j=0; j<N; j++)
      A[i*N + j] = dis(gen);
  // B(N, P)
  for (size_t i=0; i<N; i++)
    for (size_t j=0; j<P; j++)
      B[i*P + j] = dis(gen);

  std::cout << "Problem size: c(" << M << "," << P << ") = a(" << M << "," << N << ") * b(" << N << "," << P << ")" << std::endl;

  // Resultant matrix: C_serial = A*B
  for (size_t i=0; i<M; i++) {
    for (size_t j=0; j<P; j++) {
      for(size_t d=0; d<N; d++) {
	C_serial[i*P + j] += A[i*N + d] * B[d*P + j];
      }
    }
  }

  // Resultant matrix: C_cblas
  cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, 
	      m, n, k, alpha, A, ldA, B, ldB, beta, C_cblas, ldC);

  // Resultant matrix: C_onemkl
  auto asyncHandler = [&](cl::sycl::exception_list eL) {
    for (auto& e : eL) {
      try {
        std::rethrow_exception(e);
      } catch (cl::sycl::exception& e) {
        std::cout << e.what() << std::endl;
        std::cout << "fail" << std::endl;
        std::terminate();
      }
    }
  };

  try {
    // Initializing the devices queue with the default selector
    // The device queue is used to enqueue the kernels and encapsulates
    // all the states needed for execution
    default_selector device_selector;
    queue device_queue(device_selector, asyncHandler);
    std::cout << "Device: " << device_queue.get_device().get_info<info::device::name>() << std::endl << std::endl;

    // Creating 1D buffers for matrices which are bound to host memory array
    buffer<double, 1> a{A, range<1>{M*N}};
    buffer<double, 1> b{B, range<1>{N*P}};
    buffer<double, 1> c{C_onemkl, range<1>{M*P}};

    mkl::blas::gemm(device_queue, transA, transB, n, m, k, alpha, b, ldB, a, ldA, beta, c, ldC); // row-major    
  }
  catch(cl::sycl::exception const& e) {
    std::cout << "\t\tSYCL exception during GEMM\n" << e.what() << std::endl << "OpenCL status: " << e.get_cl_code() << std::endl;
  }
  
  int result_cblas, result_serial;
  std::cout << "Verify results between OneMKL & CBLAS: ";
  result_cblas = VerifyResult(C_onemkl, C_cblas);
  std::cout << "Verify results between OneMKL & Serial: ";  
  result_serial = VerifyResult(C_onemkl, C_serial);

  delete[] A;
  delete[] B;
  delete[] C_cblas;
  delete[] C_onemkl;
  delete[] C_serial;

  return 0;
}

