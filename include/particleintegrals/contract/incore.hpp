/*
 *  This file is part of the Chronus Quantum (ChronusQ) software package
 *  
 *  Copyright (C) 2014-2022 Li Research Group (University of Washington)
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *  
 *  Contact the Developers:
 *    E-Mail: xsli@uw.edu
 *  
 */
#pragma once

#include <integrals.hpp>
#include <util/matout.hpp>
#include <util/timer.hpp>
#include <util/threads.hpp>
#include <cqlinalg/blas3.hpp>
#include <cqlinalg/blasext.hpp>
#include <particleintegrals/twopints/incore4indextpi.hpp>
#include <particleintegrals/twopints/incoreritpi.hpp>
#include <particleintegrals/twopints/incore4indexreleri.hpp>
#include <particleintegrals/gradints/incore.hpp>

// Use stupid but bullet proof incore contraction for debug
//#define _BULLET_PROOF_INCORE
//#define _REPORT_INTEGRAL_TIMINGS

namespace ChronusQ {

  /**
   *  \brief Perform various tensor contractions of the full ERI
   *  tensor in core. Wraps other helper functions and provides
   *  loop structure
   *
   *  Currently supports
   *    - Coulomb-type (34,12) contractions
   *    - Exchange-type (23,14) contractions
   *
   *  Works with both real and complex matricies
   *
   *  \param [in/out] list Contains information pertinent to the
   *    matricies to be contracted with. See TwoBodyContraction
   *    for details
   */
  template <typename MatsT, typename IntsT>
  void InCore4indexTPIContraction<MatsT, IntsT>::twoBodyContract(
      MPI_Comm comm,
      const bool,
      std::vector<TwoBodyContraction<MatsT>> &list,
      EMPerturbation&) const {
    ROOT_ONLY(comm);

    if (typeid(*this) == typeid(InCore4indexRelERIContraction<MatsT,IntsT>)
        and typeid(this->ints_) != typeid(InCore4indexRelERI<IntsT>))
      CErr("InCore4indexRelTPIContraction expect a InCore4indexRelTPI reference.");

    if (typeid(*this) == typeid(InCore4indexTPIContraction<MatsT,IntsT>))
      try {
        dynamic_cast<InCore4indexTPI<IntsT>&>(this->ints_);
      } catch (const std::bad_cast&) {
        CErr("InCore4indexTPIContraction expect a InCore4indexTPI reference.");
      }

    if (typeid(*this) == typeid(InCoreRITPIContraction<MatsT,IntsT>))
      try {
        dynamic_cast<InCoreRITPI<IntsT>&>(this->ints_);
      } catch (const std::bad_cast&) {
        CErr("InCoreRITPIContraction expect a InCoreRITPI reference.");
      }

    ProgramTimer::timeOp("Contraction Total", [&](){

      // Loop over matricies to contract with
      for(auto &C : list) {

        // Coulomb-type (34,12) ERI contraction
        // AX(mn) = (mn | kl) X(kl)
        if( C.contType == TWOBODY_CONTRACTION_TYPE::COULOMB ) {
          JContract(comm,C);
        // Exchange-type (23,12) ERI contraction
        // AX(mn) = (mk |ln) X(kl)
        } else if( C.contType == TWOBODY_CONTRACTION_TYPE::EXCHANGE ) {
          KContract(comm,C);
        }

      } // loop over matricies

    });

  } // InCore4indexTPIContraction::twoBodyContract


  /**
   *  \brief Perform a Coulomb-type (34,12) ERI contraction with
   *  a one-body operator.
   */   
  template <typename MatsT, typename IntsT>
  void InCore4indexTPIContraction<MatsT, IntsT>::JContract(
      MPI_Comm, TwoBodyContraction<MatsT> &C) const {

    ProgramTimer::tick("J Contract");

    InCore4indexTPI<IntsT> &tpi4I =
        dynamic_cast<InCore4indexTPI<IntsT>&>(this->ints_);
    CQMemManager& memManager_ = tpi4I.memManager();
    size_t NB   = tpi4I.nBasis();
    size_t NB2  = NB*NB;
    size_t sNB  = tpi4I.snBasis();
    size_t sNB2 = sNB*sNB;

    // need to swap NB and sNB if contraction is done in aux
    if (this->contractSecond) {
      std::swap(NB,sNB);
      std::swap(NB2,sNB2);
    }
    
    const bool sameIntsTMatsT = std::is_same<IntsT,MatsT>::value;

    // for Hermitian densities, output Coulomb Matrix is same type as IntsT
    if (C.HER or sameIntsTMatsT) {

      IntsT *X  = reinterpret_cast<IntsT*>(C.X);
      IntsT *AX = reinterpret_cast<IntsT*>(C.AX);

      // Extract the real part of X if X is Hermetian and if the ints are
      // real
      const bool extractRealPartX = 
        C.HER and std::is_same<IntsT,double>::value and 
        std::is_same<MatsT,dcomplex>::value;

      // Allocate scratch if IntsT and MatsT are different
      const bool allocAXScratch = not std::is_same<IntsT,MatsT>::value;

      if( extractRealPartX ) {

      X = memManager_.malloc<IntsT>(sNB2);
      for(auto k = 0ul; k < sNB2; k++) X[k] = std::real(C.X[k]);
    }

      if( allocAXScratch ) {

        AX = memManager_.malloc<IntsT>(NB2);
        std::fill_n(AX,NB2,0.);

      }


      #ifdef _BULLET_PROOF_INCORE

    size_t NB3 = NB * NB2;
    #pragma omp parallel for
    for(auto i = 0; i < NB; ++i)
    for(auto j = 0; j < NB; ++j)
    for(auto k = 0; k < sNB; ++k)
    for(auto l = 0; l < sNB; ++l)

      AX[i + j*NB] += tpi4I.pointer()[i+j*NB+k*NB2+l*NB3] * X[l + k*NB];

      #else

    if( std::is_same<IntsT,dcomplex>::value )
      blas::gemm(blas::Layout::ColMajor,blas::Op::ConjTrans,blas::Op::NoTrans,NB2,1,sNB2,IntsT(1.),tpi4I.pointer(),NB2,X,sNB2,IntsT(0.),AX,NB2);
    else 
      if (not this->contractSecond)
        blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::NoTrans,NB2,1,sNB2,IntsT(1.),tpi4I.pointer(),NB2,X,sNB2,IntsT(0.),AX,NB2);
      else
        blas::gemm(blas::Layout::ColMajor,blas::Op::ConjTrans,blas::Op::NoTrans,NB2,1,sNB2,IntsT(1.),tpi4I.pointer(),sNB2,X,sNB2,IntsT(0.),AX,NB2);

      // if Complex ints + Hermitian, conjugate
      if( std::is_same<IntsT,dcomplex>::value and C.HER )
        IMatCopy('R',NB,NB,IntsT(1.),AX,NB,NB);

      // If non-hermetian, transpose
      if( not C.HER )  {

        IMatCopy('T',NB,NB,IntsT(1.),AX,NB,NB);

      }

      #endif

      // Cleanup temporaries
      if( extractRealPartX ) memManager_.free(X);
      if( allocAXScratch ) {

        std::copy_n(AX,NB2,C.AX);
        memManager_.free(AX);

      }
    
    // for non-hermiatin and MatsT = dcomplex, IntsT = double
    } else {
      if (not this->contractSecond)
        blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::NoTrans,NB2,1,sNB2,MatsT(1.),tpi4I.pointer(),NB2,C.X,sNB2,MatsT(0.),C.AX,NB2);
      else
        blas::gemm(blas::Layout::ColMajor,blas::Op::ConjTrans,blas::Op::NoTrans,NB2,1,sNB2,MatsT(1.),tpi4I.pointer(),sNB2,C.X,sNB2,MatsT(0.),C.AX,NB2);
    }

    ProgramTimer::tock("J Contract");

  }; // InCore4indexTPIContraction::JContract

  template <typename MatsT, typename IntsT>
  void InCore4indexTPIContraction<MatsT, IntsT>::KContract(
      MPI_Comm, TwoBodyContraction<MatsT> &C) const {

    ProgramTimer::tick("K Contract");
 
    InCore4indexTPI<IntsT> &tpi4I =
        dynamic_cast<InCore4indexTPI<IntsT>&>(this->ints_);
    // check to see whether the two basis sets are different
    size_t NB = tpi4I.nBasis();
    size_t NB2 = NB*NB;
    size_t NB3 = NB * NB2;

    //#ifdef _BULLET_PROOF_INCORE
    #if 0

    std::fill_n(C.AX,NB2,0.);

    #pragma omp parallel for
    for(auto i = 0; i < NB; ++i)
    for(auto j = 0; j < NB; ++j)
    for(auto k = 0; k < NB; ++k)
    for(auto l = 0; l < NB; ++l) {
      C.AX[i + j*NB] += tpi4I.pointer()[i+l*NB+k*NB2+j*NB3] * C.X[l + k*NB];
    }

    #else

    size_t LAThreads = GetLAThreads();
    SetLAThreads(1);

    #pragma omp parallel for
    for(auto nu = 0; nu < NB; nu++) 
      blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::NoTrans,NB,1,NB2,MatsT(1.),tpi4I.pointer()+nu*NB3,NB,C.X,NB2,MatsT(0.),C.AX+nu*NB,NB);

    SetLAThreads(LAThreads);

    #endif

    ProgramTimer::tock("K Contract");

  }; // InCore4indexTPIContraction::KContract


  /**
   *  \brief Perform a Coulomb-type (34,12) ERI contraction with
   *  a one-body operator.
   */
  template <typename MatsT, typename IntsT>
  void InCore4indexRelERIContraction<MatsT, IntsT>::JContract(
      MPI_Comm, TwoBodyContraction<MatsT> &C) const {

    InCore4indexRelERI<IntsT> &tpi4I =
        dynamic_cast<InCore4indexRelERI<IntsT>&>(this->ints_);
    size_t NB = tpi4I.nBasis();
    size_t NB2 = NB * NB;
    size_t NB3 = NB * NB2;

    if( C.ERI4 == nullptr ) C.ERI4 = reinterpret_cast<double*>(tpi4I.pointer());

    memset(C.AX,0.,NB2*sizeof(MatsT));

    if( C.intTrans == TRANS_KL ) {

      // D(μν) = D(λκ)(μν|[κλ]^T) = D(λκ)(μν|λκ)
      #pragma omp parallel for
      for(auto m = 0; m < NB; ++m)
      for(auto n = 0; n < NB; ++n)
      for(auto k = 0; k < NB; ++k)
      for(auto l = 0; l < NB; ++l) {

        C.AX[m + n*NB] += C.ERI4[m + n*NB + l*NB2 + k*NB3] * C.X[l + k*NB];

      }

    } else if (C.intTrans == TRANS_MN_TRANS_KL) { 
      
      // D(μν) = D(λκ)([μν]^T|[κλ]^T) = D(λκ)(νμ|λκ)
      #pragma omp parallel for
      for(auto m = 0; m < NB; ++m)
      for(auto n = 0; n < NB; ++n)
      for(auto k = 0; k < NB; ++k)
      for(auto l = 0; l < NB; ++l) {

        C.AX[m + n*NB] += C.ERI4[n + m*NB + l*NB2 + k*NB3]*C.X[l + k*NB];

      }
    
    } else if (C.intTrans == TRANS_MN) { 
      
      // D(μν) = D(λκ)([μν]^T|κλ) = D(λκ)(νμ|κλ)
      #pragma omp parallel for
      for(auto m = 0; m < NB; ++m)
      for(auto n = 0; n < NB; ++n)
      for(auto k = 0; k < NB; ++k)
      for(auto l = 0; l < NB; ++l) {

        C.AX[m + n*NB] += C.ERI4[n + m*NB + k*NB2 + l*NB3]*C.X[l + k*NB];

      }
    
    } else if( C.intTrans == TRANS_MNKL ) {

      // D(μν) = D(λκ)(μν|κλ)^T = D(λκ)(κλ|μν)
      #pragma omp parallel for
      for(auto m = 0; m < NB; ++m)
      for(auto n = 0; n < NB; ++n)
      for(auto k = 0; k < NB; ++k)
      for(auto l = 0; l < NB; ++l) {

        C.AX[m + n*NB] += C.ERI4[k + l*NB + m*NB2 + n*NB3]*C.X[l + k*NB];

      }

    } else if( C.intTrans == TRANS_NONE ) {

      // D(μν) = D(λκ)(μν|κλ)
      #pragma omp parallel for
      for(auto m = 0; m < NB; ++m)
      for(auto n = 0; n < NB; ++n)
      for(auto k = 0; k < NB; ++k)
      for(auto l = 0; l < NB; ++l) {

        C.AX[m + n*NB] += C.ERI4[m + n*NB + k*NB2 + l*NB3] * C.X[l + k*NB];

      }

    }


  }; // InCore4indexRelERIContraction::JContract


  template <typename MatsT, typename IntsT>
  void InCore4indexRelERIContraction<MatsT, IntsT>::KContract(
      MPI_Comm, TwoBodyContraction<MatsT> &C) const {

    InCore4indexRelERI<IntsT> &tpi4I =
        dynamic_cast<InCore4indexRelERI<IntsT>&>(this->ints_);
    size_t NB = tpi4I.nBasis();
    size_t NB2 = NB * NB;
    size_t NB3 = NB * NB2;

    if( C.ERI4 == nullptr) C.ERI4 = reinterpret_cast<double*>(tpi4I.pointer());

    memset(C.AX,0.,NB2*sizeof(MatsT));

    if ( C.intTrans == TRANS_MN_TRANS_KL ) {

      // D(μν) = D(λκ)([μλ]^T|[κν]^T) = D(λκ)(λμ|νκ)
      #pragma omp parallel for
      for (auto m = 0; m < NB; ++m)
      for (auto n = 0; n < NB; ++n)
      for (auto k = 0; k < NB; ++k)
      for (auto l = 0; l < NB; ++l) {

        C.AX[m + n*NB] += C.ERI4[l + m * NB + n * NB2 + k * NB3] * C.X[l + k * NB];

      }
    } else if( C.intTrans == TRANS_KL ) {

      // D(μν) = D(λκ)(μλ|[κν]^T) = D(λκ)(μλ|νκ)
      #pragma omp parallel for
      for(auto m = 0; m < NB; ++m)
      for(auto n = 0; n < NB; ++n)
      for(auto k = 0; k < NB; ++k)
      for(auto l = 0; l < NB; ++l) {

        C.AX[m + n*NB] += C.ERI4[m + l*NB + n*NB2 + k*NB3] * C.X[l + k*NB];

      }

    } else if( C.intTrans == TRANS_MNKL ) {

      // D(μν) = D(λκ)(μλ|κν)^T = D(λκ)(κν|μλ)
      #pragma omp parallel for
      for(auto m = 0; m < NB; ++m)
      for(auto n = 0; n < NB; ++n)
      for(auto k = 0; k < NB; ++k)
      for(auto l = 0; l < NB; ++l) {

        C.AX[m + l*NB] += C.ERI4[k + l*NB + m*NB2 + n*NB3] * C.X[n + k*NB];

      }

    } else if( C.intTrans == TRANS_NONE ) {

      // D(μν) = D(λκ)(μλ|κν)
      #pragma omp parallel for
      for(auto m = 0; m < NB; ++m)
      for(auto n = 0; n < NB; ++n)
      for(auto k = 0; k < NB; ++k)
      for(auto l = 0; l < NB; ++l) {

        C.AX[m + n*NB] += C.ERI4[m + l*NB + k*NB2 + n*NB3] * C.X[l + k*NB];

      }
    }

  }; // InCore4indexRelTPIContraction::KContract


  /**
   *  \brief Perform a Coulomb-type (34,12) RI-ERI contraction with
   *  a one-body operator.
   */   
  template <typename MatsT, typename IntsT>
  void InCoreRITPIContraction<MatsT, IntsT>::JContract(
      MPI_Comm, TwoBodyContraction<MatsT> &C) const {

    InCoreRITPI<IntsT> &eri3j =
        dynamic_cast<InCoreRITPI<IntsT>&>(this->ints_);
    CQMemManager& memManager_ = eri3j.memManager();
    size_t NB = eri3j.nBasis();
    size_t NB2 = NB*NB;
    size_t NBRI = eri3j.nRIBasis();

    IntsT *X  = reinterpret_cast<IntsT*>(C.X);
    IntsT *AX = reinterpret_cast<IntsT*>(C.AX);

    // Extract the real part of X if X is Hermetian and if the ints are real
    const bool extractRealPartX = 
      C.HER and std::is_same<IntsT,double>::value and 
      std::is_same<MatsT,dcomplex>::value;

    // Allocate scratch if IntsT and MatsT are different
    const bool allocAXScratch = not std::is_same<IntsT,MatsT>::value;

    if( extractRealPartX ) {

      X = memManager_.malloc<IntsT>(NB2);
      for(auto k = 0ul; k < NB2; k++) X[k] = std::real(C.X[k]);

    }

    if( allocAXScratch ) {

      AX = memManager_.malloc<IntsT>(NB2);
      std::fill_n(AX,NB2,0.);

    }


    auto Jtemp = memManager_.malloc<IntsT>(NBRI);
    // (ij|Q)S^{-1/2} -> ERI3J
    blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::NoTrans,NBRI,1,NB2,IntsT(1.),eri3j.pointer(),NBRI,X,NB2,IntsT(0.),Jtemp,NBRI);
    blas::gemm(blas::Layout::ColMajor,blas::Op::Trans,blas::Op::NoTrans,NB2,1,NBRI,IntsT(1.),eri3j.pointer(),NBRI,Jtemp,NBRI,IntsT(0.),AX,NB2);
    memManager_.free(Jtemp);

    // if Complex ints + Hermitian, conjugate
//    if( std::is_same<IntsT,dcomplex>::value and C.HER )
//      IMatCopy('R',NB,NB,IntsT(1.),AX,NB,NB);

    // If non-hermetian, transpose
//    if( not C.HER )  {

//      IMatCopy('T',NB,NB,IntsT(1.),AX,NB,NB);

//    }

    // Cleanup temporaries
    if( extractRealPartX ) memManager_.free(X);
    if( allocAXScratch ) {

      std::copy_n(AX,NB2,C.AX);
      memManager_.free(AX);

    }

  }; // InCoreRIERIContraction::JContract

  /**
   *  \brief Perform a Exchange-type (23,14) RI-ERI contraction with
   *  a one-body operator.
   */   
  template <typename MatsT, typename IntsT>
  void InCoreRITPIContraction<MatsT, IntsT>::KContract(
      MPI_Comm, TwoBodyContraction<MatsT> &C) const {

    InCoreRITPI<IntsT> &eri3j =
        dynamic_cast<InCoreRITPI<IntsT>&>(this->ints_);
    CQMemManager& memManager_ = eri3j.memManager();
    size_t NB = eri3j.nBasis();
    size_t NBRI = eri3j.nRIBasis();
    size_t NBNBRI = NB*NBRI;
    size_t NB2NBRI= NB*NBNBRI;

    MatsT *X  = C.X;
    MatsT *AX = C.AX;

    auto Ktemp = memManager_.malloc<MatsT>(NB2NBRI);
#if 1
    // (ij|Q)S^{-1/2} -> ERI3J
    size_t LAThreads = GetLAThreads();
    SetLAThreads(1);

    #pragma omp parallel for
    for(auto nu = 0ul; nu < NB; nu++)
      blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::Trans,NBRI,NB,NB,MatsT(1.),eri3j.pointer()+nu*NBNBRI,NBRI,X,NB,MatsT(0.),Ktemp+nu*NBNBRI,NBRI);

    SetLAThreads(LAThreads);

    blas::gemm(blas::Layout::ColMajor,blas::Op::Trans,blas::Op::NoTrans,NB,NB,NBNBRI,MatsT(1.),eri3j.pointer(),NBNBRI,Ktemp,NBNBRI,MatsT(0.),AX,NB);
#else
    IMatCopy('T',NB,NBNBRI,IntsT(1.),ERI3J,NB,NBNBRI);
    blas::gemm(blas::Layout::ColMajor,blas::Op::Trans,blas::Op::NoTrans,NBNBRI,NB,NB,IntsT(1.),ERI3J,NB,X,NB,IntsT(0.),Ktemp,NBNBRI);
    blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::Trans,NB,NB,NBNBRI,IntsT(1.),Ktemp,NB,ERI3J,NB2,IntsT(0.),AX,NB);
    IMatCopy('T',NBNBRI,NB,IntsT(1.),ERI3J,NBNBRI,NB);
#endif
    memManager_.free(Ktemp);

  }; // InCoreRIERIContraction::KContract

  /**
   *  \brief Perform a Exchange-type (23,14) RI-ERI contraction with
   *  orbital coefficients.
   */
  template <typename MatsT, typename IntsT>
  void InCoreRITPIContraction<MatsT, IntsT>::KCoefContract(
      MPI_Comm comm, size_t NO, MatsT *C, MatsT *AX) const {
    ROOT_ONLY(comm);

    InCoreRITPI<IntsT> &eri3j =
        dynamic_cast<InCoreRITPI<IntsT>&>(this->ints_);
    CQMemManager& memManager_ = eri3j.memManager();
    size_t NB = eri3j.nBasis();
    size_t NBRI = eri3j.nRIBasis();
    size_t NBNBRI = NB*NBRI;
    size_t NONBRI= NO*NBRI;

    MatsT *Btemp1 = memManager_.malloc<MatsT>(NBNBRI*NO);
    MatsT *Btemp2 = memManager_.malloc<MatsT>(NBNBRI*NO);

    // 1. Bt1(i, L | nu) = C(lambda, i)^H @ B(L, lambda | nu)^T
    size_t LAThreads = GetLAThreads();
    SetLAThreads(1);
    #pragma omp parallel for
    for(auto nu = 0ul; nu < NB; nu++)
      blas::gemm(blas::Layout::ColMajor,blas::Op::ConjTrans,blas::Op::Trans,NO,NBRI,NB,MatsT(1.),C,NB,
           eri3j.pointer()+nu*NBNBRI,NBRI,
           MatsT(0.),Btemp1+nu*NONBRI,NO);
    SetLAThreads(LAThreads);

    // 2. Bt2(i, L mu) = C(sigma, i)^T @ B(L mu, sigma)^T
    blas::gemm(blas::Layout::ColMajor,blas::Op::Trans,blas::Op::Trans,NO,NBNBRI,NB,MatsT(1.),C,NB,
         reinterpret_cast<MatsT*>(eri3j.pointer()),NBNBRI,
         MatsT(0.),Btemp2,NO);

    // 3. K(mu, nu) = Bt2(i L, mu)^T @ Bt1(i L, nu)
    blas::gemm(blas::Layout::ColMajor,blas::Op::Trans,blas::Op::NoTrans,NB,NB,NONBRI,MatsT(1.),Btemp2,NONBRI,Btemp1,NONBRI,
         MatsT(0.),AX,NB);

    memManager_.free(Btemp1, Btemp2);

  }; // InCoreRIERIContraction::KCoefContract

  template <>
  void InCoreRITPIContraction<dcomplex, double>::KCoefContract(
      MPI_Comm comm, size_t NO, dcomplex *C, dcomplex *AX) const {
    ROOT_ONLY(comm);

    InCoreRITPI<double> &eri3j =
        dynamic_cast<InCoreRITPI<double>&>(this->ints_);
    CQMemManager& memManager_ = eri3j.memManager();
    size_t NB = eri3j.nBasis();
    size_t NBRI = eri3j.nRIBasis();
    size_t NBNBRI = NB*NBRI;
    size_t NONBRI= NO*NBRI;

    dcomplex *Btemp1 = memManager_.malloc<dcomplex>(NBNBRI*NO);
    dcomplex *Btemp2 = memManager_.malloc<dcomplex>(NBNBRI*NO);
    dcomplex *Btemp3 = memManager_.malloc<dcomplex>(NBNBRI*NO);

    size_t LAThreads = GetLAThreads();
    SetLAThreads(1);
    #pragma omp parallel for
    for(auto nu = 0ul; nu < NB; nu++) {
    // 1.1. Bt3(L, i | nu) = B(L, lambda | nu) @ C(lambda, i)
      blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::NoTrans,NBRI,NO,NB,dcomplex(1.),
           eri3j.pointer()+nu*NBNBRI,NBRI,C,NB,
           dcomplex(0.),Btemp3+nu*NONBRI,NBRI);
    // 1.2. Bt1(i, L | nu) = Bt3(L, i | nu)^H
      SetMat('C',NBRI,NO,dcomplex(1.),Btemp3+nu*NONBRI,NBRI,
             Btemp1+nu*NONBRI,NO);
    }
    SetLAThreads(LAThreads);

    // 2.1. Bt3(L mu, i) = B(L mu, sigma) @ C(sigma, i)
    blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::NoTrans,NBNBRI,NO,NB,dcomplex(1.),eri3j.pointer(),NBNBRI,C,NB,
         dcomplex(0.),Btemp3,NBNBRI);
    // 2.2. Bt2(i, L mu) = Bt3(L mu, i)^T
    SetMat('T',NBNBRI,NO,dcomplex(1.),Btemp3,NBNBRI,Btemp2,NO);

    // 3. K(mu, nu) = Bt2(i L, mu)^T @ Bt1(i L, nu)
    blas::gemm(blas::Layout::ColMajor,blas::Op::Trans,blas::Op::NoTrans,NB,NB,NONBRI,dcomplex(1.),Btemp2,NONBRI,Btemp1,NONBRI,
         dcomplex(0.),AX,NB);

    memManager_.free(Btemp1, Btemp2, Btemp3);

  }; // InCoreRIERIContraction::KCoefContract


  // Contraction into separate storages
  template <typename MatsT, typename IntsT>
  void InCore4indexGradContraction<MatsT,IntsT>::gradTwoBodyContract(
    MPI_Comm comm,
    const bool screen,
    std::vector<std::vector<TwoBodyContraction<MatsT>>>& list,
    EMPerturbation& pert) const {

    // Contract over each 3N gradient component
    size_t nGrad = this->grad_.size();
    assert(nGrad == list.size());

    for (auto i = 0; i < nGrad; i++) {
      auto casted = std::dynamic_pointer_cast<InCore4indexTPI<IntsT>>(this->grad_[i]);
      InCore4indexTPIContraction<MatsT,IntsT> contraction(*casted);
      contraction.contractSecond = this->contractSecond;
      contraction.twoBodyContract(comm, screen, list[i], pert);
    }

  }; // InCore4indexGradContraction::gradTwoBodyContract


}; // namespace ChronusQ
