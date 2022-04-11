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
#include <particleintegrals/inhouseaointegral.hpp>
#include <util/matout.hpp>
#include <cqlinalg/blas1.hpp>
#include <cqlinalg/blasext.hpp>
#include <cqlinalg/blasutil.hpp>
#include <physcon.hpp>

#include <util/threads.hpp>
#include <util/mpi.hpp>
#include <util/timer.hpp>
#include <util/math.hpp>

#include <particleintegrals/twopints/gtodirectreleri.hpp>
#include <particleintegrals/contract/direct.hpp>

#include <chrono>


#define _SHZ_SCREEN_4C

#define _CONTRACTION_


#include <libcint.hpp>

namespace ChronusQ {


  /**************************/
  /* Libcint 4C-direct Implementation */
  /**************************/
  // For DCB Hamiltonian,
  // 12 density matrices upon input stored as
  // LL(MS,MX,MY,MZ), SS(MS,MX,MY,MZ), LS(MS,MX,MY,MZ)
  //
  // 12 contrated matrices upon output stored as
  // LL(MS,MX,MY,MZ), SS(MS,MX,MY,MZ), LS(MS,MX,MY,MZ)
  //
  //
  // Density Matrices are assumed as Hermitian here
  //
 
  template <typename MatsT, typename IntsT>
  void GTODirectRelERIContraction<MatsT,IntsT>::directScaffoldLibcint(
    MPI_Comm comm, const bool screen,
    std::vector<TwoBodyContraction<MatsT>> &matList) const {

    if (not matList[0].HER) 
      CErr("Non-Hermitian Density in 4C Contraction (Couloumb + Exchange) is NYI");
    
    DirectTPI<IntsT> &originalERI =
        dynamic_cast<DirectTPI<IntsT>&>(this->ints_);
    CQMemManager& memManager_ = originalERI.memManager();
    BasisSet& originalBasisSet_ = originalERI.basisSet();
    Molecule& molecule_ = originalERI.molecule();

    if (originalBasisSet_.forceCart)
      CErr("Libcint + cartesian GTO NYI.");

    BasisSet basisSet_ = originalBasisSet_.groupGeneralContractionBasis();

    size_t buffSize = std::max_element(basisSet_.shells.begin(),
                                       basisSet_.shells.end(),
                                       [](libint2::Shell &a, libint2::Shell &b) {
                                         return a.size() < b.size();
                                       })->size();

    DirectTPI<IntsT> &eri = originalERI;
    //DirectTPI<IntsT> eri(memManager_, basisSet_, molecule_, originalERI.threshSchwarz());
    
    // Determine the number of OpenMP threads
    size_t nThreads  = GetNumThreads();
    size_t LAThreads = GetLAThreads();
    SetLAThreads(1); // Turn off parallelism in LA functions



    /****************************/
    /* Format Basis for Libcint */
    /****************************/

    int nAtoms = molecule_.nAtoms;
    int nShells = basisSet_.nShell;
    int iAtom, iShell, off;

    // ATM_SLOTS = 6; BAS_SLOTS = 8;
    int *atm = memManager_.template malloc<int>(nAtoms * ATM_SLOTS);
    int *bas = memManager_.template malloc<int>(nShells * BAS_SLOTS);
    double *env = memManager_.template malloc<double>(basisSet_.getLibcintEnvLength(molecule_));


    basisSet_.setLibcintEnv(molecule_, atm, bas, env);


    size_t cache_size = 0;
    for (int i = 0; i < nShells; i++) {
      size_t n;
      int shls[4]{i,i,i,i};
      if(matList[0].contType == TWOBODY_CONTRACTION_TYPE::BARE_COULOMB or
         matList[0].contType == TWOBODY_CONTRACTION_TYPE::LLLL or
         matList[0].contType == TWOBODY_CONTRACTION_TYPE::LLSS) {
        n = int2e_sph(nullptr, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, nullptr);
        cache_size = std::max(cache_size, n);
      }
      if(matList[0].contType == TWOBODY_CONTRACTION_TYPE::SSSS or
         matList[0].contType == TWOBODY_CONTRACTION_TYPE::LLLL or
         matList[0].contType == TWOBODY_CONTRACTION_TYPE::LLSS) {
        n = int2e_ipvip1ipvip2_sph(nullptr, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, nullptr);
        cache_size = std::max(cache_size, n);
      }
      if(matList[0].contType == TWOBODY_CONTRACTION_TYPE::LLLL or
         matList[0].contType == TWOBODY_CONTRACTION_TYPE::LLSS) {
        n = int2e_ipvip1_sph(nullptr, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, nullptr);
        cache_size = std::max(cache_size, n);
      }
      if(matList[0].contType == TWOBODY_CONTRACTION_TYPE::GAUNT) {
        n = int2e_ip1ip2_sph(nullptr, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, nullptr);
        cache_size = std::max(cache_size, n);
      }
      if(matList[0].contType == TWOBODY_CONTRACTION_TYPE::GAUGE) {
        n = int2e_gauge_r1_ssp1sps2_sph(nullptr, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, nullptr);
        cache_size = std::max(cache_size, n);
        n = int2e_gauge_r2_ssp1sps2_sph(nullptr, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, nullptr);
        cache_size = std::max(cache_size, n);
        n = int2e_gauge_r1_ssp1ssp2_sph(nullptr, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, nullptr);
        cache_size = std::max(cache_size, n);
        n = int2e_gauge_r2_ssp1ssp2_sph(nullptr, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, nullptr);
        cache_size = std::max(cache_size, n);
      }
    }


    enum ERI_2ND_DERIV {
      AxBx,
      AxBy,
      AxBz,
      AyBx,
      AyBy,
      AyBz,
      AzBx,
      AzBy,
      AzBz
    };

    /*-------------------------------------*/
    /* End of Basis Formatting for Libcint */
    /*-------------------------------------*/



    // Get threads result buffer
    size_t buffN4 = buffSize*buffSize*buffSize*buffSize;

    // 81 is for fourth-derivative; 9 for second derivative
    int nERI;
    double *buffAll = nullptr;
    double *cacheAll = nullptr;
    double *ERIBuffer = nullptr;

    size_t maxShellSize = buffSize;
    size_t NB  = maxShellSize*4;
    size_t NB2 = NB*NB;
    size_t NB3 = NB2*NB;
    size_t NB4 = NB2*NB2;
    size_t NB4_2 = 2*NB4;
    size_t NB4_3 = 3*NB4;

    const size_t nBasis   = basisSet_.nBasis;
    const size_t nMat     = matList.size();
    const size_t nShell   = basisSet_.nShell;


    enum DIRAC_PAULI_SPINOR_COMP {
      CLLMS,
      XLLMS,
      XLLMX,
      XLLMY,
      XLLMZ,
      CSSMS,
      CSSMX,
      CSSMY,
      CSSMZ,
      XSSMS,
      XSSMX,
      XSSMY,
      XSSMZ,
      XLSMS,
      XLSMX,
      XLSMY,
      XLSMZ,
//    Gaunt
      CLSMS,
      CLSMX,
      CLSMY,
      CLSMZ,
      CSLMS,
      CSLMX,
      CSLMY,
      CSLMZ,
      XSLMS,
      XSLMX,
      XSLMY,
      XSLMZ
    };


    for(auto iMat = 0; iMat < nMat; iMat++)
      memset(matList[iMat].AX,0.,nBasis*nBasis*sizeof(MatsT));

    // Set up scratch space
    std::vector<std::vector<MatsT*>> AXthreads;

    MatsT *AXRaw = nullptr;
    AXRaw = memManager_.malloc<MatsT>(nThreads*nMat*nBasis*nBasis);    

    for(auto iThread = 0; iThread < nThreads; iThread++) {
      AXthreads.emplace_back();
      for(auto iMat = 0; iMat < nMat; iMat++) 
        AXthreads.back().push_back(AXRaw + iThread*nMat*nBasis*nBasis + iMat*nBasis*nBasis);
    }




    /************************************/
    /*                                  */
    /* Preparation of Schwarz Screening */
    /*                                  */
    /************************************/

    double *SchwarzSSSS = nullptr;

    double *SchwarzERI = nullptr;

    if(matList[0].contType == TWOBODY_CONTRACTION_TYPE::BARE_COULOMB or
       matList[0].contType == TWOBODY_CONTRACTION_TYPE::LLLL or
       matList[0].contType == TWOBODY_CONTRACTION_TYPE::LLSS) {
//      if(SchwarzERI == nullptr) eri.computeSchwarz();
  
#ifdef _REPORT_INTEGRAL_TIMINGS
      auto topERIchwarz = tick();
#endif
  
      nERI = 1;
      buffAll = memManager_.malloc<double>(nERI*buffN4*nThreads);
      cacheAll = memManager_.malloc<double>(cache_size*nThreads);
      SchwarzERI = memManager_.malloc<double>(nShell*nShell);
      memset(SchwarzERI,0,nShell*nShell*sizeof(double));
  
      #pragma omp parallel
      {
  
        size_t thread_id = GetThreadID();
        size_t n1,n2;
  
        int shls[4];
        double *buff = &buffAll[nERI*buffN4*thread_id];
        double *cache = cacheAll+cache_size*thread_id;
  
        // set up Schwarz screening
        for(size_t s1(0), bf1_s(0), s12(0); s1 < basisSet_.nShell; bf1_s+=n1, s1++) { 
  
          n1 = basisSet_.shells[s1].size(); // Size of Shell 1
  
        for(size_t s2(0), bf2_s(0); s2 <= s1; bf2_s+=n2, s2++, s12++) {
  
          n2 = basisSet_.shells[s2].size(); // Size of Shell 2
  
          // Round Robbin work distribution
          #ifdef _OPENMP
          if( s12 % nThreads != thread_id ) continue;
          #endif
  
          shls[0] = int(s1);
          shls[1] = int(s2);
          shls[2] = int(s1);
          shls[3] = int(s2);
  
          auto nQuad = n1*n2*n1*n2;
  
          if(int2e_sph(buff, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, cache)==0) continue;

          for(size_t iERI(0); iERI < nERI*nQuad; iERI++) 
            SchwarzERI[s1+s2*nShell] = std::max(SchwarzERI[s1+s2*nShell], std::abs(buff[iERI]));
  
          SchwarzERI[s1+s2*nShell] = std::sqrt(SchwarzERI[s1+s2*nShell]);
          SchwarzERI[s2+s1*nShell] = SchwarzERI[s1+s2*nShell];
  
        }
        }
  
      };

      memManager_.free(buffAll, cacheAll);
  
#ifdef _REPORT_INTEGRAL_TIMINGS
      auto durERIchwarz = tock(topERIchwarz);
//      std::cout << "ERI Schwarz took " <<  durERIchwarz << " s\n"; 
  
      //std::cout << std::endl;
#endif
  
    };
 

    if(matList[0].contType == TWOBODY_CONTRACTION_TYPE::SSSS or 
       matList[0].contType == TWOBODY_CONTRACTION_TYPE::LLLL or
       matList[0].contType == TWOBODY_CONTRACTION_TYPE::LLSS) {
  
#ifdef _REPORT_INTEGRAL_TIMINGS
      auto topSSSSSchwarz = tick();
#endif
  
      nERI = 81;
      buffAll = memManager_.malloc<double>(nERI*buffN4*nThreads);
      cacheAll = memManager_.malloc<double>(cache_size*nThreads);
      SchwarzSSSS = memManager_.malloc<double>(nShell*nShell);
      memset(SchwarzSSSS,0,nShell*nShell*sizeof(double));
  
      #pragma omp parallel
      {
  
        double C2 = 1./(4*SpeedOfLight*SpeedOfLight);
        size_t thread_id = GetThreadID();
        size_t n1,n2;
  
        int shls[4];
        double *buff = &buffAll[nERI*buffN4*thread_id];
        double *cache = cacheAll+cache_size*thread_id;
  
        // set up Schwarz screening
        for(size_t s1(0), bf1_s(0), s12(0); s1 < basisSet_.nShell; bf1_s+=n1, s1++) { 
  
          n1 = basisSet_.shells[s1].size(); // Size of Shell 1
  
        for(size_t s2(0), bf2_s(0); s2 <= s1; bf2_s+=n2, s2++, s12++) {
  
          n2 = basisSet_.shells[s2].size(); // Size of Shell 2
  
          // Round Robbin work distribution
          #ifdef _OPENMP
          if( s12 % nThreads != thread_id ) continue;
          #endif
  
          shls[0] = int(s1);
          shls[1] = int(s2);
          shls[2] = int(s1);
          shls[3] = int(s2);
  
          auto nQuad = n1*n2*n1*n2;
  
          if(int2e_ipvip1ipvip2_sph(buff, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, cache)==0) continue;

          for(size_t iERI(0); iERI < nERI*nQuad; iERI++) 
            SchwarzSSSS[s1+s2*nShell] = std::max(SchwarzSSSS[s1+s2*nShell], std::abs(buff[iERI]));
  
          SchwarzSSSS[s1+s2*nShell] = C2*std::sqrt(SchwarzSSSS[s1+s2*nShell]);
          SchwarzSSSS[s2+s1*nShell] = SchwarzSSSS[s1+s2*nShell];
  
        }
        }
  
      };

      memManager_.free(buffAll, cacheAll);
  
#ifdef _REPORT_INTEGRAL_TIMINGS
      auto durSSSSSchwarz = tock(topSSSSSchwarz);
//      std::cout << "∇A∇B∇C∇D Schwarz took " <<  durSSSSSchwarz << " s\n"; 
  
      //std::cout << std::endl;
#endif
  
    };
    /****************************************/
    /*                                      */
    /* End Preparation of Schwarz Screening */
    /*                                      */
    /****************************************/










    

    /***********************************/
    /*                                 */
    /* Start of Bare-Coulomb-Exchange  */
    /*                                 */
    /***********************************/

    if( matList[0].contType == TWOBODY_CONTRACTION_TYPE::BARE_COULOMB ) {

#ifdef _REPORT_INTEGRAL_TIMINGS
      auto topDirect = tick();
#endif

#ifdef _SHZ_SCREEN_4C
      // Compute shell block norms (∞-norm) of matList.X
      // CLLMS, XLLMS, XLLMX, XLLMY, XLLMZ Densitry matrices
      int mMat = 5;
      double *ShBlkNorms_raw = memManager_.malloc<double>(mMat*nShell*nShell);
      std::vector<double*> ShBlkNorms;
  
      for(auto iMat = 0, iOff = 0; iMat < mMat; iMat++, iOff += nShell*nShell ) {
  
        ShellBlockNorm(basisSet_.shells,matList[iMat].X,nBasis,ShBlkNorms_raw + iOff);
        ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      }
  
      // Get the max over all the matricies for the shell block ∞-norms
      for(auto k = 0; k < nShell*nShell; k++) {
  
        double mx = std::abs(ShBlkNorms[0][k]);
        for(auto iMat = 0; iMat < mMat; iMat++)
          mx = std::max(mx,std::abs(ShBlkNorms[iMat][k]));
        ShBlkNorms[0][k] = mx;
   
      }

#endif


      // Keeping track of number of integrals skipped
      std::vector<size_t> nSkip(nThreads,0);

      nERI = 1;
      buffAll = memManager_.malloc<double>(nERI*buffN4*nThreads);
      cacheAll = memManager_.malloc<double>(cache_size*nThreads);
      memset(AXRaw,0,nThreads*nMat*nBasis*nBasis*sizeof(MatsT));

      #pragma omp parallel
      {
        int thread_id = GetThreadID();
  
        auto &AX_loc = AXthreads[thread_id];
  
        size_t n1,n2,n3,n4,m,n,k,l,mnkl,bf1,bf2,bf3,bf4;
        size_t s4_max;
        int shls[4];
        double *buff = &buffAll[buffN4*thread_id];
        double *cache = cacheAll+cache_size*thread_id;
  
        for(size_t s1(0), bf1_s(0), s1234(0); s1 < nShells; 
            bf1_s+=n1, s1++) { 
  
          n1 = basisSet_.shells[s1].size(); // Size of Shell 1
  
        for(size_t s2(0), bf2_s(0); s2 <= s1; bf2_s+=n2, s2++) {
  
          n2 = basisSet_.shells[s2].size(); // Size of Shell 2
          // Deneneracy factor for s1,s2 pair
          double s12_deg = (s1 == s2) ? 1.0 : 2.0;

#ifdef _SHZ_SCREEN_4C
          double shMax12 = ShBlkNorms[0][s1 + s2*nShell];
#endif
  
        for(size_t s3(0), bf3_s(0); s3 <= s1; bf3_s+=n3, s3++) {
  
          n3 = basisSet_.shells[s3].size(); // Size of Shell 3
          s4_max = (s1 == s3) ? s2 : s3; // Determine the unique max of Shell 4

#ifdef _SHZ_SCREEN_4C
          double shMax123 = std::max(ShBlkNorms[0][s1 + s3*nShell], 
                                     ShBlkNorms[0][s2 + s3*nShell]);

          shMax123 = std::max(shMax123,shMax12);
#endif 

        for(size_t s4(0), bf4_s(0); s4 <= s4_max; bf4_s+=n4, s4++, s1234++) {
  
          n4 = basisSet_.shells[s4].size(); // Size of Shell 4
  
          // Round Robbin work distribution
          #ifdef _OPENMP
          if( s1234 % nThreads != thread_id ) continue;
          #endif
  
#ifdef _SHZ_SCREEN_4C
          double shMax = std::max(ShBlkNorms[0][s1 + s4*nShell],
                                  std::max(ShBlkNorms[0][s2 + s4*nShell],
                                           ShBlkNorms[0][s3 + s4*nShell]));

          shMax = std::max(shMax,shMax123);

          if((shMax*SchwarzERI[s1+s2*nShell]*SchwarzERI[s3+s4*nShell]) <
          //if((SchwarzERI[s1+s2*nShell]*SchwarzERI[s3+s4*nShell]) <
             eri.threshSchwarz()) { nSkip[thread_id]++; continue; }
#endif 

          // Degeneracy factor for s3,s4 pair
          double s34_deg = (s3 == s4) ? 1.0 : 2.0;
          // Degeneracy factor for s1, s2, s3, s4 quartet
          double s12_34_deg = (s1 == s3) ? (s2 == s4 ? 1.0 : 2.0) : 2.0;
          // Total degeneracy factor
          double s1234_deg = s12_deg * s34_deg * s12_34_deg;
   
          auto nQuad = n1*n2*n3*n4;

          shls[0] = int(s1);
          shls[1] = int(s2);
          shls[2] = int(s3);
          shls[3] = int(s4);

          if(int2e_sph(buff, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, cache)==0) continue;

          auto ADCLLMS = AX_loc[CLLMS];
          auto ADXLLMS = AX_loc[XLLMS];
          auto ADXLLMX = AX_loc[XLLMX];
          auto ADXLLMY = AX_loc[XLLMY];
          auto ADXLLMZ = AX_loc[XLLMZ];
          auto DCLLMS  = matList[CLLMS].X;
          auto DXLLMS  = matList[XLLMS].X;
          auto DXLLMX  = matList[XLLMX].X;
          auto DXLLMY  = matList[XLLMY].X;
          auto DXLLMZ  = matList[XLLMZ].X;
 
          // Scale the buffer by the degeneracy factor and store
          for(auto i = 0ul; i < nQuad; i++) buff[i] *= 0.5*s1234_deg;

          for(l = 0ul, bf4 = bf4_s, mnkl=0ul ; l < n4; ++l, bf4++) {
  
          for(k = 0ul, bf3 = bf3_s           ; k < n3; ++k, bf3++) {
            auto bf43 = bf4 + bf3*nBasis;
            auto bf34 = bf3 + bf4*nBasis;
  
          for(n = 0ul, bf2 = bf2_s           ; n < n2; ++n, bf2++) {
            auto bf42 = bf4 + bf2*nBasis;
            auto bf23 = bf2 + bf3*nBasis;
            auto bf24 = bf2 + bf4*nBasis;
  
          for(m = 0ul, bf1 = bf1_s           ; m < n1; ++m, bf1++, ++mnkl) {
            auto bf21 = bf2 + bf1*nBasis;
            auto bf12 = bf1 + bf2*nBasis;
            auto bf14 = bf1 + bf4*nBasis;
            auto bf13 = bf1 + bf3*nBasis;
 
            // Start of Coulomb
            ADCLLMS[bf12] += buff[mnkl]*DCLLMS[bf43].real();
            ADCLLMS[bf34] += buff[mnkl]*DCLLMS[bf21].real();

            // Start of Exchange
            buff[mnkl] *= 0.5;
            ADXLLMS[bf14] += buff[mnkl]*DXLLMS[bf23];
            ADXLLMX[bf14] += buff[mnkl]*DXLLMX[bf23];
            ADXLLMY[bf14] += buff[mnkl]*DXLLMY[bf23];
            ADXLLMZ[bf14] += buff[mnkl]*DXLLMZ[bf23];
  
            ADXLLMS[bf13] += buff[mnkl]*DXLLMS[bf24];
            ADXLLMX[bf13] += buff[mnkl]*DXLLMX[bf24];
            ADXLLMY[bf13] += buff[mnkl]*DXLLMY[bf24];
            ADXLLMZ[bf13] += buff[mnkl]*DXLLMZ[bf24];
  
            ADXLLMS[bf24] += buff[mnkl]*DXLLMS[bf13];
            ADXLLMX[bf24] += buff[mnkl]*DXLLMX[bf13];
            ADXLLMY[bf24] += buff[mnkl]*DXLLMY[bf13];
            ADXLLMZ[bf24] += buff[mnkl]*DXLLMZ[bf13];
  
            ADXLLMS[bf23] += buff[mnkl]*DXLLMS[bf14];
            ADXLLMX[bf23] += buff[mnkl]*DXLLMX[bf14];
            ADXLLMY[bf23] += buff[mnkl]*DXLLMY[bf14];
            ADXLLMZ[bf23] += buff[mnkl]*DXLLMZ[bf14];

          }; // bf1
          }; // bf2
          }; // bf3
          }; // bf4  Contraction loop
  
        }; // s4
        }; // s3
        }; // s2
        }; // s1
  
      }; // OpenMP context
  
  
      MatsT* SCR = memManager_.malloc<MatsT>(nBasis * nBasis);

      // Take care of the Hermitian symmetry for CLLMS, XLLMS, XLLMX, XLLMY, XLLMZ
      for( auto iMat = 0; iMat < 5;  iMat++ ) 
      for( auto iTh  = 0; iTh < nThreads; iTh++) {
        MatAdd('N','C',nBasis,nBasis,MatsT(0.5),AXthreads[iTh][iMat],nBasis,MatsT(0.5),
          AXthreads[iTh][iMat],nBasis,SCR,nBasis);
        MatAdd('N','N',nBasis,nBasis,MatsT(1.), SCR,nBasis, 
          MatsT(1.), matList[iMat].AX,nBasis,matList[iMat].AX,nBasis);
      }
   
      memManager_.free(SCR);
      memManager_.free(buffAll, cacheAll);
#ifdef _SHZ_SCREEN_4C
      if(ShBlkNorms_raw!=nullptr) memManager_.free(ShBlkNorms_raw);
#endif

#ifdef _REPORT_INTEGRAL_TIMINGS
      size_t nIntSkip = std::accumulate(nSkip.begin(),nSkip.end(),0);
      std::cout << "Bare-Coulomb-Exchange Screened " << nIntSkip << std::endl;
  
      auto durDirect = tock(topDirect);
      std::cout << "Bare-Coulomb-Exchange AO Direct Contraction took " <<  durDirect << " s\n"; 
  
      std::cout << std::endl;
#endif
  

    };

    /*********************************/
    /*                               */
    /* End of Bare-Coulomb-Exchange  */
    /*                               */
    /*********************************/














    /******************************************/
    /*                                        */
    /* Start of Dirac-Coulomb LL and C(2)-SS  */
    /*                                        */
    /******************************************/

    if( matList[0].contType == TWOBODY_CONTRACTION_TYPE::LLLL ) {

 
#ifdef _REPORT_INTEGRAL_TIMINGS
      auto topDirectLL = tick();
#endif

#ifdef _SHZ_SCREEN_4C
      // Compute shell block norms (∞-norm) of matList.X
      // CLLMS, CSSMS, CSSMX, CSSMY, CSSMZ Densitry matrices
      int mMat = 5;
      double *ShBlkNorms_raw = memManager_.malloc<double>(mMat*nShell*nShell);
      std::vector<double*> ShBlkNorms;
  
      auto iOff = 0;
      ShellBlockNorm(basisSet_.shells,matList[CLLMS].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[CSSMS].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[CSSMX].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[CSSMY].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[CSSMZ].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      // Get the max over all the matricies for the shell block ∞-norms
      for(auto k = 0; k < nShell*nShell; k++) {
  
        double mx = std::abs(ShBlkNorms[0][k]);
        for(auto iMat = 0; iMat < mMat; iMat++)
          mx = std::max(mx,std::abs(ShBlkNorms[iMat][k]));
        ShBlkNorms[0][k] = mx;
   
      }
#endif

      // 81 is for fourth-derivative; 9 for second derivative
      nERI = 9;
      buffAll = memManager_.malloc<double>(nERI*buffN4*nThreads);
      cacheAll = memManager_.malloc<double>(cache_size*nThreads);
      ERIBuffer = memManager_.malloc<double>(2*4*NB4*nThreads);

      // Keeping track of number of integrals skipped
      std::vector<size_t> nSkipLL(nThreads,0);

      memset(AXRaw,0,nThreads*nMat*nBasis*nBasis*sizeof(MatsT));

      #pragma omp parallel
      {

        double C2 = 1./(4*SpeedOfLight*SpeedOfLight);
  
        size_t thread_id = GetThreadID();
  
        auto &AX_loc = AXthreads[thread_id];
  
        double *ERIBuffAB   = &ERIBuffer[thread_id*4*NB4];
        double *ERIBuffCD   = &ERIBuffer[nThreads*4*NB4 + thread_id*4*NB4];
  
        size_t n1,n2,n3,n4,m,n,k,l,mnkl,bf1,bf2,bf3,bf4;
        size_t s4_max;
  
        int shls[4];
        double *buff = &buffAll[nERI*buffN4*thread_id];
        double *cache = cacheAll+cache_size*thread_id;
  
        for(size_t s1(0), bf1_s(0), s1234(0); s1 < nShell; bf1_s+=n1, s1++) { 
  
          n1 = basisSet_.shells[s1].size(); // Size of Shell 1
  
        for(size_t s2(0), bf2_s(0); s2 <= s1; bf2_s+=n2, s2++) {
  
          n2 = basisSet_.shells[s2].size(); // Size of Shell 2
          // Deneneracy factor for s1,s2 pair
          double s12_deg = (s1 == s2) ? 1.0 : 2.0;
  
#ifdef _SHZ_SCREEN_4C
//          double shMax12 = ShBlkNorms[0][s1 + s2*nShell];
#endif
  
        for(size_t s3(0), bf3_s(0); s3 < nShell; bf3_s+=n3, s3++) {
  
          n3 = basisSet_.shells[s3].size(); // Size of Shell 3
          s4_max = (s1 == s3) ? s2 : s3; // Determine the unique max of Shell 4
  
#ifdef _SHZ_SCREEN_4C
//          double shMax123 = std::max(ShBlkNorms[0][s1 + s3*nShell], ShBlkNorms[0][s2 + s3*nShell]);
//          shMax123 = std::max(shMax123,shMax12);
#endif
   
        for(size_t s4(0), bf4_s(0); s4 <= s3; bf4_s+=n4, s4++, s1234++) {
  
          n4 = basisSet_.shells[s4].size(); // Size of Shell 4

          // Round Robbin work distribution
          #ifdef _OPENMP
          if( s1234 % nThreads != thread_id ) continue;
          #endif

#ifdef _SHZ_SCREEN_4C
//          double shMax = std::max(ShBlkNorms[0][s1 + s4*nShell],
//                                  std::max(ShBlkNorms[0][s2 + s4*nShell],
//                                           ShBlkNorms[0][s3 + s4*nShell]));

//          shMax = std::max(shMax,shMax123);

//          if((shMax*SchwarzSSSS[s1+s2*nShell]*SchwarzERI[s3 + s4*nShell]) <
//             eri.threshSchwarz()) { nSkipLL[thread_id]++; continue; }

          double shMax = std::max(ShBlkNorms[0][s1 + s2*nShell],ShBlkNorms[0][s3 + s4*nShell]);

          if((shMax*SchwarzSSSS[s1+s2*nShell]*SchwarzERI[s3 + s4*nShell]) <
             eri.threshSchwarz()) { nSkipLL[thread_id]++; continue; }
#endif

          // Degeneracy factor for s3,s4 pair
          double s34_deg = (s3 == s4) ? 1.0 : 2.0;

          auto nQuad = n1*n2*n3*n4;
  
          shls[0] = int(s1);
          shls[1] = int(s2);
          shls[2] = int(s3);
          shls[3] = int(s4);
  
          if(int2e_ipvip1_sph(buff, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, cache)==0) continue;

 
          for(l = 3*maxShellSize, mnkl = 0ul; l < 3*maxShellSize + n4; ++l) {
            auto lNB3 = l*NB3;
            auto lNB  = l*NB;

          for(k = 2*maxShellSize            ; k < 2*maxShellSize + n3; ++k) {
            auto kNB2lNB3 = k*NB2 + lNB3;
            auto klNB  = k + lNB;

          for(n =   maxShellSize            ; n <   maxShellSize + n2; ++n) {
            auto nNBkNB2lNB3 = n*NB + kNB2lNB3;
            auto klNBnNB3    = klNB + n*NB3;

          for(m = 0                         ; m <                  n1; ++m, ++mnkl) {
   
            /* Dirac-Coulomb */
            // ∇A∙∇B(mn|kl)
            auto dAdotdB = buff[AxBx*nQuad+mnkl] + buff[AyBy*nQuad+mnkl] + buff[AzBz*nQuad+mnkl];
            // ∇Ax∇B(mn|kl)
            auto dAcrossdB_x =  buff[AyBz*nQuad+mnkl] - buff[AzBy*nQuad+mnkl];
            auto dAcrossdB_y = -buff[AxBz*nQuad+mnkl] + buff[AzBx*nQuad+mnkl];
            auto dAcrossdB_z =  buff[AxBy*nQuad+mnkl] - buff[AyBx*nQuad+mnkl];
  
            //auto MNKL = m + n*NB + k*NB2 + l*NB3;
            auto MNKL = m + nNBkNB2lNB3;
            //auto KLMN = k + l*NB + m*NB2 + n*NB3;
            auto KLMN = m*NB2 + klNBnNB3;
  
  
            // ∇A∙∇B(mn|kl) followed by ∇Ax∇B(mn|kl) X, Y, and Z
            // (mn|kl)
            ERIBuffAB[       MNKL] =  (double)s34_deg*dAdotdB;
            ERIBuffAB[   NB4+MNKL] =  (double)s34_deg*dAcrossdB_x;
            ERIBuffAB[ NB4_2+MNKL] =  (double)s34_deg*dAcrossdB_y;
            ERIBuffAB[ NB4_3+MNKL] =  (double)s34_deg*dAcrossdB_z;
  
            // ∇C∙∇D(kl|nm) followed by ∇Cx∇D(kl|nm) X, Y, and Z
            // (kl|mn)
            ERIBuffCD[       KLMN] =  (double)s12_deg*dAdotdB;
            ERIBuffCD[   NB4+KLMN] =  (double)s12_deg*dAcrossdB_x;
            ERIBuffCD[ NB4_2+KLMN] =  (double)s12_deg*dAcrossdB_y;
            ERIBuffCD[ NB4_3+KLMN] =  (double)s12_deg*dAcrossdB_z;
 
          }
          }
          } 
          } // ∇A∇B integral preparation loop
  


#ifdef _CONTRACTION_ // Contraction

          auto ADCLLMS = AX_loc[CLLMS];
          auto ADCSSMS = AX_loc[CSSMS];
          auto ADCSSMX = AX_loc[CSSMX];
          auto ADCSSMY = AX_loc[CSSMY];
          auto ADCSSMZ = AX_loc[CSSMZ];

          auto DCLLMS  = matList[CLLMS].X;
          auto DCSSMS  = matList[CSSMS].X;
          auto DCSSMX  = matList[CSSMX].X;
          auto DCSSMY  = matList[CSSMY].X;
          auto DCSSMZ  = matList[CSSMZ].X;
 

          for(m = 0ul,            bf1 = bf1_s; m <                  n1; ++m, bf1++) {
            auto mNB2 = m*NB2;
            auto bf1nB = bf1*nBasis;

          for(n =   maxShellSize, bf2 = bf2_s; n <   maxShellSize + n2; ++n, bf2++) {
            auto mNB2nNB3 = mNB2 + n*NB3;
            auto mnNB     = m + n*NB;
            auto bf21 = bf2 + bf1nB;
            auto bf12 = bf1 + bf2*nBasis;

          for(k = 2*maxShellSize, bf3 = bf3_s; k < 2*maxShellSize + n3; ++k, bf3++) {
            auto mnNBkNB2  = mnNB + k*NB2;
            auto kmNB2nNB3 = k + mNB2nNB3;
            auto bf3nB = bf3*nBasis;

          for(l = 3*maxShellSize, bf4 = bf4_s; l < 3*maxShellSize + n4; ++l, bf4++) {
  
            //auto MNKL = m + n*NB + k*NB2 + l*NB3;
            auto MNKL = mnNBkNB2 + l*NB3;
            //auto KLMN = k + l*NB + m*NB2 + n*NB3;
            auto KLMN = kmNB2nNB3 + l*NB;
  
            auto bf43 = bf4 + bf3nB;
            auto bf34 = bf3 + bf4*nBasis;
  
            auto DotPrdMNKL = MNKL;
            auto CrossXMNKL = MNKL+NB4;
            auto CrossYMNKL = MNKL+NB4_2;
            auto CrossZMNKL = MNKL+NB4_3;
  
            auto DotPrdKLMN = KLMN;
            auto CrossXKLMN = KLMN+NB4;
            auto CrossYKLMN = KLMN+NB4_2;
            auto CrossZKLMN = KLMN+NB4_3;
  
  

            /*++++++++++++++++++++++++++++++++++++++++++++*/
            /* Start of Dirac-Coulomb (LL|LL) Contraction */
            /*++++++++++++++++++++++++++++++++++++++++++++*/
  
            //KLMN
            if(bf3 >= bf4 ) {
              ADCLLMS[bf34] +=  ERIBuffCD[DotPrdKLMN]*DCSSMS[bf21].real()
                               -ERIBuffCD[CrossZKLMN]*DCSSMZ[bf21].imag()
                               -ERIBuffCD[CrossXKLMN]*DCSSMX[bf21].imag()
                               -ERIBuffCD[CrossYKLMN]*DCSSMY[bf21].imag();
            }
           
            /*------------------------------------------*/
            /* End of Dirac-Coulomb (LL|LL) Contraction */
            /*------------------------------------------*/
  
  
  
  
  
            /*+++++++++++++++++++++++++++++++++++++++++++++++++*/
            /* Start of Dirac-Coulomb C(2)-(SS|SS) Contraction */
            /*+++++++++++++++++++++++++++++++++++++++++++++++++*/
  
  
            /* MNKL */
            if (bf1 >= bf2) {
              ADCSSMS[bf12] += ERIBuffAB[DotPrdMNKL]*DCLLMS[bf43].real();
              ADCSSMX[bf12] += ERIBuffAB[CrossXMNKL]*DCLLMS[bf43].real();
              ADCSSMY[bf12] += ERIBuffAB[CrossYMNKL]*DCLLMS[bf43].real();
              ADCSSMZ[bf12] += ERIBuffAB[CrossZMNKL]*DCLLMS[bf43].real();
            } 
  
            /*-----------------------------------------------*/
            /* End of Dirac-Coulomb C(2)-(SS|SS) Contraction */
            /*-----------------------------------------------*/
          };
          };
          };  
          }; // contraction loop
  
#endif // Contraction
  
  
        }; // loop s4
        }; // loop s3
        }; // loop s2
        }; // loop s1
  
  
      } // OpenMP context
 
      dcomplex iscale = dcomplex(0.0, 1.0);

      for( auto iTh  = 0; iTh < nThreads; iTh++) {
 
        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][CLLMS],nBasis,MatsT(1.0),
           matList[CLLMS].AX,nBasis,matList[CLLMS].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][CSSMS],nBasis,MatsT(1.0),
           matList[CSSMS].AX,nBasis,matList[CSSMS].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis, iscale, AXthreads[iTh][CSSMX],nBasis,MatsT(1.0),
           matList[CSSMX].AX,nBasis,matList[CSSMX].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis, iscale, AXthreads[iTh][CSSMY],nBasis,MatsT(1.0),
           matList[CSSMY].AX,nBasis,matList[CSSMY].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis, iscale, AXthreads[iTh][CSSMZ],nBasis,MatsT(1.0),
           matList[CSSMZ].AX,nBasis,matList[CSSMZ].AX,nBasis);

      };

#if 1
      // Take care of the Hermitian symmetry in the LL and SS blocks
      auto ADCLLMS = matList[CLLMS].AX;
      auto ADCSSMS = matList[CSSMS].AX;
      auto ADCSSMX = matList[CSSMX].AX;
      auto ADCSSMY = matList[CSSMY].AX;
      auto ADCSSMZ = matList[CSSMZ].AX;

      for( auto i = 0; i < nBasis; i++ ) 
      for( auto j = 0; j < i; j++ ) {
        ADCLLMS[j + i*nBasis] = std::conj(ADCLLMS[i + j*nBasis]);
        ADCSSMS[j + i*nBasis] = std::conj(ADCSSMS[i + j*nBasis]);
        ADCSSMX[j + i*nBasis] = std::conj(ADCSSMX[i + j*nBasis]);
        ADCSSMY[j + i*nBasis] = std::conj(ADCSSMY[i + j*nBasis]);
        ADCSSMZ[j + i*nBasis] = std::conj(ADCSSMZ[i + j*nBasis]);
      }
#endif

      memManager_.free(ERIBuffer);
      memManager_.free(buffAll, cacheAll);
#ifdef _SHZ_SCREEN_4C
      if(ShBlkNorms_raw!=nullptr) memManager_.free(ShBlkNorms_raw);
#endif

#ifdef _REPORT_INTEGRAL_TIMINGS
      size_t nIntSkipLL = std::accumulate(nSkipLL.begin(),nSkipLL.end(),0);
      std::cout << "Dirac-Coulomb-LL/C(2)-SS Screened " << nIntSkipLL << std::endl;

      auto durDirectLL = tock(topDirectLL);
      std::cout << "Dirac-Coulomb-LL/C(2)-SS AO Direct Contraction took " <<  durDirectLL << " s\n"; 

      std::cout << std::endl;
#endif
    

    } // if(LLLL)
   
    /******************************************/
    /*                                        */
    /*   End of Dirac-Coulomb LL and C(2)-SS  */
    /*                                        */
    /******************************************/







    /***************************/
    /*                         */
    /* Start of (LLSS)/(SSLL)  */
    /*                         */
    /***************************/

    if( matList[0].contType == TWOBODY_CONTRACTION_TYPE::LLSS ) {


#ifdef _REPORT_INTEGRAL_TIMINGS
      auto topDirectLS = tick();
#endif

#ifdef _SHZ_SCREEN_4C
      // Compute shell block norms (∞-norm) of matList.X
      // XLSMS, XLSMX, XLSMY, XLSMZ Densitry matrices
      int mMat = 4;
      double *ShBlkNorms_raw = memManager_.malloc<double>(mMat*nShell*nShell);
      std::vector<double*> ShBlkNorms;
  
      auto iOff = 0;
      ShellBlockNorm(basisSet_.shells,matList[XLSMS].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLSMX].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLSMY].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLSMZ].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      // Get the max over all the matricies for the shell block ∞-norms
      for(auto k = 0; k < nShell*nShell; k++) {
  
        double mx = std::abs(ShBlkNorms[0][k]);
        for(auto iMat = 0; iMat < mMat; iMat++)
          mx = std::max(mx,std::abs(ShBlkNorms[iMat][k]));
        ShBlkNorms[0][k] = mx;
   
      }
#endif

      nERI = 9;
      buffAll = memManager_.malloc<double>(nERI*buffN4*nThreads);
      cacheAll = memManager_.malloc<double>(cache_size*nThreads);
      ERIBuffer = memManager_.malloc<double>(2*4*NB4*nThreads);
      memset(AXRaw,0,nThreads*nMat*nBasis*nBasis*sizeof(MatsT));
      std::vector<size_t> nSkipLS(nThreads,0);
  
      #pragma omp parallel
      {
  
        dcomplex iscale = dcomplex(0.0, 1.0);
  
        size_t thread_id = GetThreadID();
  
        auto &AX_loc = AXthreads[thread_id];
  
        double *ERIBuffAB   = &ERIBuffer[thread_id*4*NB4];
        double *ERIBuffCD   = &ERIBuffer[nThreads*4*NB4 + thread_id*4*NB4];
  
        size_t n1,n2,n3,n4,m,n,k,l,mnkl,bf1,bf2,bf3,bf4;
        size_t s4_max;
  
        int shls[4];
        double *buff = &buffAll[nERI*buffN4*thread_id];
        double *cache = cacheAll+cache_size*thread_id;
  
        for(size_t s1(0), bf1_s(0), s1234(0); s1 < nShell; bf1_s+=n1, s1++) { 
  
          n1 = basisSet_.shells[s1].size(); // Size of Shell 1
  
        for(size_t s2(0), bf2_s(0); s2 <= s1; bf2_s+=n2, s2++) {
  
          n2 = basisSet_.shells[s2].size(); // Size of Shell 2
  
#ifdef _SHZ_SCREEN_4C
          double shMax12 = ShBlkNorms[0][s1 + s2*nShell];
#endif
  
        for(size_t s3(0), bf3_s(0); s3 < nShell; bf3_s+=n3, s3++) {
  
          n3 = basisSet_.shells[s3].size(); // Size of Shell 3
          s4_max = (s1 == s3) ? s2 : s3; // Determine the unique max of Shell 4
  
#ifdef _SHZ_SCREEN_4C
          double shMax123 = std::max(ShBlkNorms[0][s1 + s3*nShell], ShBlkNorms[0][s2 + s3*nShell]);
          shMax123 = std::max(shMax123,shMax12);
#endif
   
  
        for(size_t s4(0), bf4_s(0); s4 <= s3; bf4_s+=n4, s4++, s1234++) {
  
          n4 = basisSet_.shells[s4].size(); // Size of Shell 4
  
          // Round Robbin work distribution
          #ifdef _OPENMP
          if( s1234 % nThreads != thread_id ) continue;
          #endif
  
#ifdef _SHZ_SCREEN_4C
  
          double shMax = std::max(ShBlkNorms[0][s1 + s4*nShell],
                                  std::max(ShBlkNorms[0][s2 + s4*nShell],
                                           ShBlkNorms[0][s3 + s4*nShell]));
  
          shMax = std::max(shMax,shMax123);

          if((shMax*SchwarzSSSS[s3+s4*nShell]*SchwarzERI[s1 + s2*nShell]) <
             eri.threshSchwarz()) { nSkipLS[thread_id]++; continue; }
#endif
  
          auto nQuad = n1*n2*n3*n4;
  
          shls[0] = int(s3);
          shls[1] = int(s4);
          shls[2] = int(s1);
          shls[3] = int(s2);
  
          if(int2e_ipvip1_sph(buff, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, cache)==0) continue;
  
          for(n =   maxShellSize, mnkl = 0ul; n <   maxShellSize + n2; ++n) 
          for(m = 0                         ; m <                  n1; ++m) 
          for(l = 3*maxShellSize            ; l < 3*maxShellSize + n4; ++l)
          for(k = 2*maxShellSize            ; k < 2*maxShellSize + n3; ++k, ++mnkl) {
   
            auto MNKL = m + n*NB + k*NB2 + l*NB3;
            auto KLMN = k + l*NB + m*NB2 + n*NB3;
  
            /* Dirac-Coulomb */
            // ∇C∙∇D(mn|kl)
            auto dCdotdD = buff[AxBx*nQuad+mnkl] + buff[AyBy*nQuad+mnkl] + buff[AzBz*nQuad+mnkl];
            // ∇Ax∇B(mn|kl)
            auto dCcrossdD_x =  buff[AyBz*nQuad+mnkl] - buff[AzBy*nQuad+mnkl];
            auto dCcrossdD_y = -buff[AxBz*nQuad+mnkl] + buff[AzBx*nQuad+mnkl];
            auto dCcrossdD_z =  buff[AxBy*nQuad+mnkl] - buff[AyBx*nQuad+mnkl];
  
  
            // ∇A∙∇B(kl|mn) followed by ∇Ax∇B(kl|mn) X, Y, and Z
            // (kl|mn)
            ERIBuffAB[      KLMN] =  dCdotdD;
            ERIBuffAB[  NB4+KLMN] =  dCcrossdD_x;
            ERIBuffAB[NB4_2+KLMN] =  dCcrossdD_y;
            ERIBuffAB[NB4_3+KLMN] =  dCcrossdD_z;
  
            // ∇C∙∇D(mn|kl) followed by ∇Cx∇D(mn|kl) X, Y, and Z
            // (mn|kl)
            ERIBuffCD[      MNKL] =  dCdotdD;
            ERIBuffCD[  NB4+MNKL] =  dCcrossdD_x;
            ERIBuffCD[NB4_2+MNKL] =  dCcrossdD_y;
            ERIBuffCD[NB4_3+MNKL] =  dCcrossdD_z;
  
        } // ∇C∙∇D integral preparation loop
  
  
  
  
#ifdef _CONTRACTION_ // Contraction

          auto ADXLSMS  = AX_loc[XLSMS];
          auto ADXLSMX  = AX_loc[XLSMX];
          auto ADXLSMY  = AX_loc[XLSMY];
          auto ADXLSMZ  = AX_loc[XLSMZ];
          auto DXLSMS = matList[XLSMS].X;
          auto DXLSMX = matList[XLSMX].X;
          auto DXLSMY = matList[XLSMY].X;
          auto DXLSMZ = matList[XLSMZ].X;
 
          for(m = 0ul,            bf1 = bf1_s; m <                  n1; ++m, bf1++) {
          for(n =   maxShellSize, bf2 = bf2_s; n <   maxShellSize + n2; ++n, bf2++) {
            auto mn1 = m + n*NB;
            auto mn2 = m*NB2 + n*NB3;
            auto bf1nB = bf1*nBasis;
            auto bf2nB = bf2*nBasis;

          for(k = 2*maxShellSize, bf3 = bf3_s; k < 2*maxShellSize + n3; ++k, bf3++) {
            auto bf3nB = bf3*nBasis;
            auto kmn1 = mn1 + k*NB2;
            auto kmn2 = k + mn2;

          for(l = 3*maxShellSize, bf4 = bf4_s; l < 3*maxShellSize + n4; ++l, bf4++) {
            auto MNKL = kmn1 + l*NB3;
            auto KLMN = kmn2 + l*NB;

            auto bf4nB= bf4*nBasis;  

            auto bf14 = bf1 + bf4nB;
            auto bf24 = bf2 + bf4nB;
            auto bf23 = bf2 + bf3nB;
            auto bf13 = bf1 + bf3nB;
            auto bf41 = bf4 + bf1nB;
            auto bf31 = bf3 + bf1nB;
            auto bf32 = bf3 + bf2nB;
            auto bf42 = bf4 + bf2nB;
  
            auto DotPrdMNKL = MNKL;
            auto CrossXMNKL = MNKL+NB4;
            auto CrossYMNKL = MNKL+NB4_2;
            auto CrossZMNKL = MNKL+NB4_3;
  
            auto DotPrdKLMN = KLMN;
            auto CrossXKLMN = KLMN+NB4;
            auto CrossYKLMN = KLMN+NB4_2;
            auto CrossZKLMN = KLMN+NB4_3;
  
  
            /*++++++++++++++++++++++++++++++++++++++++++*/
            /* Start of Dirac-Coulomb (LL|SS) / (SS|LL) */
            /*++++++++++++++++++++++++++++++++++++++++++*/
  
  
 
            //MNKL
            ADXLSMS[bf14]+= -ERIBuffCD[DotPrdMNKL]*DXLSMS[bf23]
                            -( ERIBuffCD[CrossZMNKL]*DXLSMZ[bf23]
                              +ERIBuffCD[CrossXMNKL]*DXLSMX[bf23]
                              +ERIBuffCD[CrossYMNKL]*DXLSMY[bf23])*iscale;
  
            ADXLSMX[bf14]+= -ERIBuffCD[CrossXMNKL]*DXLSMS[bf23]*iscale
                            -ERIBuffCD[CrossYMNKL]*DXLSMZ[bf23]
                            -ERIBuffCD[DotPrdMNKL]*DXLSMX[bf23]
                            +ERIBuffCD[CrossZMNKL]*DXLSMY[bf23];
  
            ADXLSMY[bf14]+= -ERIBuffCD[CrossYMNKL]*DXLSMS[bf23]*iscale
                            +ERIBuffCD[CrossXMNKL]*DXLSMZ[bf23]
                            -ERIBuffCD[DotPrdMNKL]*DXLSMY[bf23]
                            -ERIBuffCD[CrossZMNKL]*DXLSMX[bf23];
  
            ADXLSMZ[bf14]+= -ERIBuffCD[CrossZMNKL]*DXLSMS[bf23]*iscale
                            -ERIBuffCD[DotPrdMNKL]*DXLSMZ[bf23]
                            +ERIBuffCD[CrossYMNKL]*DXLSMX[bf23]
                            -ERIBuffCD[CrossXMNKL]*DXLSMY[bf23];
  
            //MNLK
            if(bf3_s!=bf4_s) {
  
              ADXLSMS[bf13]+= -ERIBuffCD[DotPrdMNKL]*DXLSMS[bf24]
                              +( ERIBuffCD[CrossZMNKL]*DXLSMZ[bf24]
                                +ERIBuffCD[CrossXMNKL]*DXLSMX[bf24]
                                +ERIBuffCD[CrossYMNKL]*DXLSMY[bf24])*iscale;
  
              ADXLSMX[bf13]+=  ERIBuffCD[CrossXMNKL]*DXLSMS[bf24]*iscale
                              +ERIBuffCD[CrossYMNKL]*DXLSMZ[bf24]
                              -ERIBuffCD[DotPrdMNKL]*DXLSMX[bf24]
                              -ERIBuffCD[CrossZMNKL]*DXLSMY[bf24];
  
              ADXLSMY[bf13]+=  ERIBuffCD[CrossYMNKL]*DXLSMS[bf24]*iscale
                              -ERIBuffCD[CrossXMNKL]*DXLSMZ[bf24]
                              +ERIBuffCD[CrossZMNKL]*DXLSMX[bf24]
                              -ERIBuffCD[DotPrdMNKL]*DXLSMY[bf24];
  
              ADXLSMZ[bf13]+=  ERIBuffCD[CrossZMNKL]*DXLSMS[bf24]*iscale
                              -ERIBuffCD[DotPrdMNKL]*DXLSMZ[bf24]
                              -ERIBuffCD[CrossYMNKL]*DXLSMX[bf24]
                              +ERIBuffCD[CrossXMNKL]*DXLSMY[bf24];
  
            }
  
            //NMKL
            if(bf1_s!=bf2_s){
              ADXLSMS[bf24]+= -ERIBuffCD[DotPrdMNKL]*DXLSMS[bf13]
                              -( ERIBuffCD[CrossZMNKL]*DXLSMZ[bf13]
                                +ERIBuffCD[CrossXMNKL]*DXLSMX[bf13]
                                +ERIBuffCD[CrossYMNKL]*DXLSMY[bf13])*iscale;
  
              ADXLSMX[bf24]+= -ERIBuffCD[CrossXMNKL]*DXLSMS[bf13]*iscale
                              -ERIBuffCD[CrossYMNKL]*DXLSMZ[bf13]
                              -ERIBuffCD[DotPrdMNKL]*DXLSMX[bf13]
                              +ERIBuffCD[CrossZMNKL]*DXLSMY[bf13];
  
              ADXLSMY[bf24]+= -ERIBuffCD[CrossYMNKL]*DXLSMS[bf13]*iscale
                              +ERIBuffCD[CrossXMNKL]*DXLSMZ[bf13]
                              -ERIBuffCD[CrossZMNKL]*DXLSMX[bf13]
                              -ERIBuffCD[DotPrdMNKL]*DXLSMY[bf13];
  
              ADXLSMZ[bf24]+= -ERIBuffCD[CrossZMNKL]*DXLSMS[bf13]*iscale
                              -ERIBuffCD[DotPrdMNKL]*DXLSMZ[bf13]
                              +ERIBuffCD[CrossYMNKL]*DXLSMX[bf13]
                              -ERIBuffCD[CrossXMNKL]*DXLSMY[bf13];
  
              if(bf3_s!=bf4_s) {
  
                ADXLSMS[bf23]+= -ERIBuffCD[DotPrdMNKL]*DXLSMS[bf14]
                                +( ERIBuffCD[CrossZMNKL]*DXLSMZ[bf14]
                                  +ERIBuffCD[CrossXMNKL]*DXLSMX[bf14]
                                  +ERIBuffCD[CrossYMNKL]*DXLSMY[bf14])*iscale;
  
                ADXLSMX[bf23]+= +ERIBuffCD[CrossXMNKL]*DXLSMS[bf14]*iscale
                                +ERIBuffCD[CrossYMNKL]*DXLSMZ[bf14]
                                -ERIBuffCD[DotPrdMNKL]*DXLSMX[bf14]
                                -ERIBuffCD[CrossZMNKL]*DXLSMY[bf14];
  
                ADXLSMY[bf23]+= +ERIBuffCD[CrossYMNKL]*DXLSMS[bf14]*iscale
                                -ERIBuffCD[CrossXMNKL]*DXLSMZ[bf14]
                                +ERIBuffCD[CrossZMNKL]*DXLSMX[bf14]
                                -ERIBuffCD[DotPrdMNKL]*DXLSMY[bf14];
  
                ADXLSMZ[bf23]+= +ERIBuffCD[CrossZMNKL]*DXLSMS[bf14]*iscale
                                -ERIBuffCD[DotPrdMNKL]*DXLSMZ[bf14]
                                -ERIBuffCD[CrossYMNKL]*DXLSMX[bf14]
                                +ERIBuffCD[CrossXMNKL]*DXLSMY[bf14];
  
              }
            }
  
            /*------------------------------------------*/
            /*   End of Dirac-Coulomb (LL|SS) / (SS|LL) */
            /*------------------------------------------*/
          }
        }
        }  
          } // contraction loop
  
#endif // Contraction
  
  
        }; // loop s4
        }; // loop s3
        }; // loop s2
        }; // loop s1
  
  
      } // OpenMP context
  
  
      for( auto iTh  = 0; iTh < nThreads; iTh++) {
   
        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLSMS],nBasis,MatsT(1.0),
           matList[XLSMS].AX,nBasis,matList[XLSMS].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLSMX],nBasis,MatsT(1.0),
           matList[XLSMX].AX,nBasis,matList[XLSMX].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLSMY],nBasis,MatsT(1.0),
           matList[XLSMY].AX,nBasis,matList[XLSMY].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLSMZ],nBasis,MatsT(1.0),
           matList[XLSMZ].AX,nBasis,matList[XLSMZ].AX,nBasis);
  
      };
   
      memManager_.free(ERIBuffer);
      memManager_.free(buffAll, cacheAll);
#ifdef _SHZ_SCREEN_4C
      if(ShBlkNorms_raw!=nullptr) memManager_.free(ShBlkNorms_raw);
#endif
  
#ifdef _REPORT_INTEGRAL_TIMINGS
      size_t nIntSkipLS = std::accumulate(nSkipLS.begin(),nSkipLS.end(),0);
      std::cout << "Dirac-Coulomb-LS Screened " << nIntSkipLS << std::endl;
  
      auto durDirectLS = tock(topDirectLS);
      std::cout << "Dirac-Coulomb-LS AO Direct Contraction took " <<  durDirectLS << " s\n"; 
  
      std::cout << std::endl;
#endif

    } // if(LLSS)

    /***************************/
    /*                         */
    /*   End of (LLSS)/(SSLL)  */
    /*                         */
    /***************************/











    /****************************************/
    /*                                      */
    /* Start of Dirac-Coulomb C(4)-SSSS     */
    /*                                      */
    /****************************************/

    if( matList[0].contType == TWOBODY_CONTRACTION_TYPE::SSSS ) {
  
      enum ERI_4TH_DERIV {
        AxBxCxDx,
        AxBxCxDy,
        AxBxCxDz,
        AxBxCyDx,
        AxBxCyDy,
        AxBxCyDz,
        AxBxCzDx,
        AxBxCzDy,
        AxBxCzDz,
        AxByCxDx,
        AxByCxDy,
        AxByCxDz,
        AxByCyDx,
        AxByCyDy,
        AxByCyDz,
        AxByCzDx,
        AxByCzDy,
        AxByCzDz,
        AxBzCxDx,
        AxBzCxDy,
        AxBzCxDz,
        AxBzCyDx,
        AxBzCyDy,
        AxBzCyDz,
        AxBzCzDx,
        AxBzCzDy,
        AxBzCzDz,
        AyBxCxDx,
        AyBxCxDy,
        AyBxCxDz,
        AyBxCyDx,
        AyBxCyDy,
        AyBxCyDz,
        AyBxCzDx,
        AyBxCzDy,
        AyBxCzDz,
        AyByCxDx,
        AyByCxDy,
        AyByCxDz,
        AyByCyDx,
        AyByCyDy,
        AyByCyDz,
        AyByCzDx,
        AyByCzDy,
        AyByCzDz,
        AyBzCxDx,
        AyBzCxDy,
        AyBzCxDz,
        AyBzCyDx,
        AyBzCyDy,
        AyBzCyDz,
        AyBzCzDx,
        AyBzCzDy,
        AyBzCzDz,
        AzBxCxDx,
        AzBxCxDy,
        AzBxCxDz,
        AzBxCyDx,
        AzBxCyDy,
        AzBxCyDz,
        AzBxCzDx,
        AzBxCzDy,
        AzBxCzDz,
        AzByCxDx,
        AzByCxDy,
        AzByCxDz,
        AzByCyDx,
        AzByCyDy,
        AzByCyDz,
        AzByCzDx,
        AzByCzDy,
        AzByCzDz,
        AzBzCxDx,
        AzBzCxDy,
        AzBzCxDz,
        AzBzCyDx,
        AzBzCyDy,
        AzBzCyDz,
        AzBzCzDx,
        AzBzCzDy,
        AzBzCzDz,
      };
  
  
#ifdef _REPORT_INTEGRAL_TIMINGS
      auto topDirectSSSS = tick();
#endif

#ifdef _SHZ_SCREEN_4C
      // Compute shell block norms (∞-norm) of matList.X
      // CSSMS, CSSMX, CSSMY, CSSMZ Densitry matrices
      int mMat = 4;
      double *ShBlkNorms_raw = memManager_.malloc<double>(mMat*nShell*nShell);
      std::vector<double*> ShBlkNorms;
  
      auto iOff = 0;
      ShellBlockNorm(basisSet_.shells,matList[CSSMS].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[CSSMX].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[CSSMY].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[CSSMZ].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      // Get the max over all the matricies for the shell block ∞-norms
      for(auto k = 0; k < nShell*nShell; k++) {
  
        double mx = std::abs(ShBlkNorms[0][k]);
        for(auto iMat = 0; iMat < mMat; iMat++)
          mx = std::max(mx,std::abs(ShBlkNorms[iMat][k]));
        ShBlkNorms[0][k] = mx;
   
      }
#endif



      nERI = 81;
      buffAll = memManager_.malloc<double>(nERI*buffN4*nThreads);
      cacheAll = memManager_.malloc<double>(cache_size*nThreads);
      ERIBuffer = memManager_.malloc<double>(16*NB4*nThreads);
  
      memset(AXRaw,0,nThreads*nMat*nBasis*nBasis*sizeof(MatsT));
  
      // Keeping track of number of integrals skipped
      std::vector<size_t> nSkipSSSS(nThreads,0);

      auto NB4_4  = 4*NB4;  
      auto NB4_5  = 5*NB4;  
      auto NB4_6  = 6*NB4;  
      auto NB4_7  = 7*NB4;  
      auto NB4_8  = 8*NB4;  
      auto NB4_9  = 9*NB4;  
      auto NB4_10 =10*NB4;  
      auto NB4_11 =11*NB4;  
      auto NB4_12 =12*NB4;  
      auto NB4_13 =13*NB4;  
      auto NB4_14 =14*NB4;  
      auto NB4_15 =15*NB4;  
  
  
      #pragma omp parallel
      {
  
        dcomplex iscale = dcomplex(0.0, 1.0);
  
        size_t thread_id = GetThreadID();
  
        auto &AX_loc = AXthreads[thread_id];
  
        double *ERIBuffABCD = &ERIBuffer[thread_id*16*NB4];
  
        size_t n1,n2,n3,n4,m,n,k,l,mnkl,bf1,bf2,bf3,bf4;
        size_t s4_max;
  
        int shls[4];
        double *buff = &buffAll[nERI*buffN4*thread_id];
        double *cache = cacheAll+cache_size*thread_id;
  
  
        for(size_t s1(0), bf1_s(0), s1234(0); s1 < basisSet_.nShell; 
            bf1_s+=n1, s1++) { 
  
          n1 = basisSet_.shells[s1].size(); // Size of Shell 1
  
        for(size_t s2(0), bf2_s(0); s2 <= s1; bf2_s+=n2, s2++) {
  
          n2 = basisSet_.shells[s2].size(); // Size of Shell 2
  
#ifdef _SHZ_SCREEN_4C
          double shMax12 = ShBlkNorms[0][s1 + s2*nShell];
#endif
  
        for(size_t s3(0), bf3_s(0); s3 <= s1; bf3_s+=n3, s3++) {
  
          n3 = basisSet_.shells[s3].size(); // Size of Shell 3
          s4_max = (s1 == s3) ? s2 : s3; // Determine the unique max of Shell 4
  
#ifdef _SHZ_SCREEN_4C
          double shMax123 = std::max(ShBlkNorms[0][s1 + s3*nShell], ShBlkNorms[0][s2 + s3*nShell]);
          shMax123 = std::max(shMax123,shMax12);
#endif

        for(size_t s4(0), bf4_s(0); s4 <= s4_max; bf4_s+=n4, s4++, s1234++) {
  
          n4 = basisSet_.shells[s4].size(); // Size of Shell 4
  
          // Round Robbin work distribution
          #ifdef _OPENMP
          if( s1234 % nThreads != thread_id ) continue;
          #endif

#ifdef _SHZ_SCREEN_4C
          double shMax = std::max(ShBlkNorms[0][s1 + s4*nShell],
                                  std::max(ShBlkNorms[0][s2 + s4*nShell],
                                           ShBlkNorms[0][s3 + s4*nShell]));

          shMax = std::max(shMax,shMax123);

          if((shMax*SchwarzSSSS[s1+s2*nShell]*SchwarzSSSS[s3 + s4*nShell]) <
             eri.threshSchwarz()) { nSkipSSSS[thread_id]++; continue; }
#endif


          auto nQuad = n1*n2*n3*n4;
  
          shls[0] = int(s1);
          shls[1] = int(s2);
          shls[2] = int(s3);
          shls[3] = int(s4);
  
          if(int2e_ipvip1ipvip2_sph(buff, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, cache)==0) continue;
  
          for(l = 3*maxShellSize, mnkl = 0ul; l < 3*maxShellSize + n4; ++l)
          for(k = 2*maxShellSize            ; k < 2*maxShellSize + n3; ++k) 
          for(n =   maxShellSize            ; n <   maxShellSize + n2; ++n) 
          for(m = 0                         ; m <                  n1; ++m, ++mnkl) {
  
            // (∇A∙∇B)(∇C∙∇D)(mnkl)
            auto dAdotdBdCdotdD =  buff[AxBxCxDx*nQuad+mnkl] + buff[AxBxCyDy*nQuad+mnkl] + buff[AxBxCzDz*nQuad+mnkl]
                                 + buff[AyByCxDx*nQuad+mnkl] + buff[AyByCyDy*nQuad+mnkl] + buff[AyByCzDz*nQuad+mnkl]
                                 + buff[AzBzCxDx*nQuad+mnkl] + buff[AzBzCyDy*nQuad+mnkl] + buff[AzBzCzDz*nQuad+mnkl];
  
            // (∇Ax∇B)(∇C∙∇D)(mnkl)
            auto dAcrossdB_xdCdotdD =  buff[AyBzCxDx*nQuad+mnkl] - buff[AzByCxDx*nQuad+mnkl]
                                     + buff[AyBzCyDy*nQuad+mnkl] - buff[AzByCyDy*nQuad+mnkl]
                                     + buff[AyBzCzDz*nQuad+mnkl] - buff[AzByCzDz*nQuad+mnkl];
  
            auto dAcrossdB_ydCdotdD = -buff[AxBzCxDx*nQuad+mnkl] + buff[AzBxCxDx*nQuad+mnkl]
                                      -buff[AxBzCyDy*nQuad+mnkl] + buff[AzBxCyDy*nQuad+mnkl]
                                      -buff[AxBzCzDz*nQuad+mnkl] + buff[AzBxCzDz*nQuad+mnkl];
                                      
            auto dAcrossdB_zdCdotdD =  buff[AxByCxDx*nQuad+mnkl] - buff[AyBxCxDx*nQuad+mnkl]
                                     + buff[AxByCyDy*nQuad+mnkl] - buff[AyBxCyDy*nQuad+mnkl]
                                     + buff[AxByCzDz*nQuad+mnkl] - buff[AyBxCzDz*nQuad+mnkl];
    
            // (∇A∙∇B)(∇Cx∇D)(mnkl)
            auto dAdotdBdCcrossdD_x =  buff[AxBxCyDz*nQuad+mnkl] - buff[AxBxCzDy*nQuad+mnkl]
                                     + buff[AyByCyDz*nQuad+mnkl] - buff[AyByCzDy*nQuad+mnkl]
                                     + buff[AzBzCyDz*nQuad+mnkl] - buff[AzBzCzDy*nQuad+mnkl];
  
            auto dAdotdBdCcrossdD_y = -buff[AxBxCxDz*nQuad+mnkl] + buff[AxBxCzDx*nQuad+mnkl]
                                      -buff[AyByCxDz*nQuad+mnkl] + buff[AyByCzDx*nQuad+mnkl]
                                      -buff[AzBzCxDz*nQuad+mnkl] + buff[AzBzCzDx*nQuad+mnkl];
                                      
            auto dAdotdBdCcrossdD_z =  buff[AxBxCxDy*nQuad+mnkl] - buff[AxBxCyDx*nQuad+mnkl]
                                     + buff[AyByCxDy*nQuad+mnkl] - buff[AyByCyDx*nQuad+mnkl]
                                     + buff[AzBzCxDy*nQuad+mnkl] - buff[AzBzCyDx*nQuad+mnkl];
  
            // (∇Ax∇B)(∇Cx∇D)(mnkl)
            auto dAcrossdB_xdCcrossdD_x =  buff[AyBzCyDz*nQuad+mnkl] - buff[AzByCyDz*nQuad+mnkl]
                                         - buff[AyBzCzDy*nQuad+mnkl] + buff[AzByCzDy*nQuad+mnkl];
  
            auto dAcrossdB_xdCcrossdD_y =  buff[AyBzCzDx*nQuad+mnkl] - buff[AzByCzDx*nQuad+mnkl]
                                         - buff[AyBzCxDz*nQuad+mnkl] + buff[AzByCxDz*nQuad+mnkl];
  
            auto dAcrossdB_xdCcrossdD_z =  buff[AyBzCxDy*nQuad+mnkl] - buff[AzByCxDy*nQuad+mnkl]
                                         - buff[AyBzCyDx*nQuad+mnkl] + buff[AzByCyDx*nQuad+mnkl];
  
            auto dAcrossdB_ydCcrossdD_x =  buff[AzBxCyDz*nQuad+mnkl] - buff[AxBzCyDz*nQuad+mnkl]
                                         - buff[AzBxCzDy*nQuad+mnkl] + buff[AxBzCzDy*nQuad+mnkl];
  
            auto dAcrossdB_ydCcrossdD_y =  buff[AzBxCzDx*nQuad+mnkl] - buff[AxBzCzDx*nQuad+mnkl]
                                         - buff[AzBxCxDz*nQuad+mnkl] + buff[AxBzCxDz*nQuad+mnkl];
  
            auto dAcrossdB_ydCcrossdD_z =  buff[AzBxCxDy*nQuad+mnkl] - buff[AxBzCxDy*nQuad+mnkl]
                                         - buff[AzBxCyDx*nQuad+mnkl] + buff[AxBzCyDx*nQuad+mnkl];
  
            auto dAcrossdB_zdCcrossdD_x =  buff[AxByCyDz*nQuad+mnkl] - buff[AyBxCyDz*nQuad+mnkl]
                                         - buff[AxByCzDy*nQuad+mnkl] + buff[AyBxCzDy*nQuad+mnkl];
  
            auto dAcrossdB_zdCcrossdD_y =  buff[AxByCzDx*nQuad+mnkl] - buff[AyBxCzDx*nQuad+mnkl]
                                         - buff[AxByCxDz*nQuad+mnkl] + buff[AyBxCxDz*nQuad+mnkl];
  
            auto dAcrossdB_zdCcrossdD_z =  buff[AxByCxDy*nQuad+mnkl] - buff[AyBxCxDy*nQuad+mnkl]
                                         - buff[AxByCyDx*nQuad+mnkl] + buff[AyBxCyDx*nQuad+mnkl];
  
  
            auto MNKL = m + n*NB + k*NB2 + l*NB3;
            auto KLMN = k + l*NB + m*NB2 + n*NB3;

            // (mn|kl)
            ERIBuffABCD[         MNKL] =  dAdotdBdCdotdD;
            ERIBuffABCD[   NB4 + MNKL] =  dAcrossdB_xdCdotdD;
            ERIBuffABCD[ NB4_2 + MNKL] =  dAcrossdB_ydCdotdD;
            ERIBuffABCD[ NB4_3 + MNKL] =  dAcrossdB_zdCdotdD;
            ERIBuffABCD[ NB4_4 + MNKL] =  dAdotdBdCcrossdD_x;
            ERIBuffABCD[ NB4_5 + MNKL] =  dAdotdBdCcrossdD_y;
            ERIBuffABCD[ NB4_6 + MNKL] =  dAdotdBdCcrossdD_z;
            ERIBuffABCD[ NB4_7 + MNKL] =  dAcrossdB_xdCcrossdD_x;
            ERIBuffABCD[ NB4_8 + MNKL] =  dAcrossdB_xdCcrossdD_y;
            ERIBuffABCD[ NB4_9 + MNKL] =  dAcrossdB_xdCcrossdD_z;
            ERIBuffABCD[NB4_10 + MNKL] =  dAcrossdB_ydCcrossdD_x;
            ERIBuffABCD[NB4_11 + MNKL] =  dAcrossdB_ydCcrossdD_y;
            ERIBuffABCD[NB4_12 + MNKL] =  dAcrossdB_ydCcrossdD_z;
            ERIBuffABCD[NB4_13 + MNKL] =  dAcrossdB_zdCcrossdD_x; 
            ERIBuffABCD[NB4_14 + MNKL] =  dAcrossdB_zdCcrossdD_y;
            ERIBuffABCD[NB4_15 + MNKL] =  dAcrossdB_zdCcrossdD_z;
  
            // (kl|mn)
            ERIBuffABCD[         KLMN] =  dAdotdBdCdotdD;
            ERIBuffABCD[   NB4 + KLMN] =  dAdotdBdCcrossdD_x;
            ERIBuffABCD[ NB4_2 + KLMN] =  dAdotdBdCcrossdD_y;
            ERIBuffABCD[ NB4_3 + KLMN] =  dAdotdBdCcrossdD_z;
            ERIBuffABCD[ NB4_4 + KLMN] =  dAcrossdB_xdCdotdD;
            ERIBuffABCD[ NB4_5 + KLMN] =  dAcrossdB_ydCdotdD;
            ERIBuffABCD[ NB4_6 + KLMN] =  dAcrossdB_zdCdotdD;
            ERIBuffABCD[ NB4_7 + KLMN] =  dAcrossdB_xdCcrossdD_x;
            ERIBuffABCD[ NB4_8 + KLMN] =  dAcrossdB_ydCcrossdD_x;
            ERIBuffABCD[ NB4_9 + KLMN] =  dAcrossdB_zdCcrossdD_x;
            ERIBuffABCD[NB4_10 + KLMN] =  dAcrossdB_xdCcrossdD_y;
            ERIBuffABCD[NB4_11 + KLMN] =  dAcrossdB_ydCcrossdD_y;
            ERIBuffABCD[NB4_12 + KLMN] =  dAcrossdB_zdCcrossdD_y;
            ERIBuffABCD[NB4_13 + KLMN] =  dAcrossdB_xdCcrossdD_z; 
            ERIBuffABCD[NB4_14 + KLMN] =  dAcrossdB_ydCcrossdD_z;
            ERIBuffABCD[NB4_15 + KLMN] =  dAcrossdB_zdCcrossdD_z;
  
          } // ∇A∇B∇C∇D (SSSS) integrals 
  
  
  
  
  
  
  
#ifdef _CONTRACTION_ // Contraction
  
          auto DCSSMS = matList[CSSMS].X;
          auto DCSSMX = matList[CSSMX].X;
          auto DCSSMY = matList[CSSMY].X;
          auto DCSSMZ = matList[CSSMZ].X;
  
          auto ADCSSMS  = AX_loc[CSSMS];
          auto ADCSSMX  = AX_loc[CSSMX];
          auto ADCSSMY  = AX_loc[CSSMY];
          auto ADCSSMZ  = AX_loc[CSSMZ];
  
          auto DXSSMS = matList[XSSMS].X;
          auto DXSSMX = matList[XSSMX].X;
          auto DXSSMY = matList[XSSMY].X;
          auto DXSSMZ = matList[XSSMZ].X;
  
          auto ADXSSMS  = AX_loc[XSSMS];
          auto ADXSSMX  = AX_loc[XSSMX];
          auto ADXSSMY  = AX_loc[XSSMY];
          auto ADXSSMZ  = AX_loc[XSSMZ];
  
          for(m = 0ul,            bf1 = bf1_s; m <                  n1; ++m, bf1++) {

          for(n =   maxShellSize, bf2 = bf2_s; n <   maxShellSize + n2; ++n, bf2++) {
            auto bf1nB = bf1*nBasis;
            auto bf2nB = bf2*nBasis;
            auto bf21 = bf2 + bf1nB;
            auto bf12 = bf1 + bf2nB;
            auto mn1 = m + n*NB;
            auto mn2 = m*NB2 + n*NB3;

          for(k = 2*maxShellSize, bf3 = bf3_s; k < 2*maxShellSize + n3; ++k, bf3++) {
            auto bf3nB = bf3*nBasis;
            auto kmn1 = mn1 + k*NB2;
            auto kmn2 = k + mn2;

          for(l = 3*maxShellSize, bf4 = bf4_s; l < 3*maxShellSize + n4; ++l, bf4++) {
            auto MNKL = kmn1 + l*NB3;
            auto KLMN = kmn2 + l*NB;

            auto bf4nB = bf4*nBasis;
  
            auto bf14 = bf1 + bf4nB;
            auto bf24 = bf2 + bf4nB;
            auto bf23 = bf2 + bf3nB;
            auto bf13 = bf1 + bf3nB;

            auto bf41 = bf4 + bf1nB;
            auto bf31 = bf3 + bf1nB;
            auto bf32 = bf3 + bf2nB;
            auto bf42 = bf4 + bf2nB;
 

            auto bf43 = bf4 + bf3nB;
            auto bf34 = bf3 + bf4nB;
  
 

            /*********************************/
            /* Dirac-Coulomb (SSSS) Integral */
            /*********************************/
      
  
            auto MNKLdAdotdBdCdotdD          = ERIBuffABCD[         MNKL];
            auto MNKLdAcrossdB_xdCdotdD      = ERIBuffABCD[   NB4 + MNKL];
            auto MNKLdAcrossdB_ydCdotdD      = ERIBuffABCD[ NB4_2 + MNKL];
            auto MNKLdAcrossdB_zdCdotdD      = ERIBuffABCD[ NB4_3 + MNKL];
            auto MNKLdAdotdBdCcrossdD_x      = ERIBuffABCD[ NB4_4 + MNKL];
            auto MNKLdAdotdBdCcrossdD_y      = ERIBuffABCD[ NB4_5 + MNKL];
            auto MNKLdAdotdBdCcrossdD_z      = ERIBuffABCD[ NB4_6 + MNKL];
            auto MNKLdAcrossdB_xdCcrossdD_x  = ERIBuffABCD[ NB4_7 + MNKL];
            auto MNKLdAcrossdB_xdCcrossdD_y  = ERIBuffABCD[ NB4_8 + MNKL];
            auto MNKLdAcrossdB_xdCcrossdD_z  = ERIBuffABCD[ NB4_9 + MNKL];
            auto MNKLdAcrossdB_ydCcrossdD_x  = ERIBuffABCD[NB4_10 + MNKL];
            auto MNKLdAcrossdB_ydCcrossdD_y  = ERIBuffABCD[NB4_11 + MNKL];
            auto MNKLdAcrossdB_ydCcrossdD_z  = ERIBuffABCD[NB4_12 + MNKL];
            auto MNKLdAcrossdB_zdCcrossdD_x  = ERIBuffABCD[NB4_13 + MNKL];
            auto MNKLdAcrossdB_zdCcrossdD_y  = ERIBuffABCD[NB4_14 + MNKL];
            auto MNKLdAcrossdB_zdCcrossdD_z  = ERIBuffABCD[NB4_15 + MNKL];

            auto KLMNdAdotdBdCdotdD          = ERIBuffABCD[         KLMN];
            auto KLMNdAcrossdB_xdCdotdD      = ERIBuffABCD[   NB4 + KLMN];
            auto KLMNdAcrossdB_ydCdotdD      = ERIBuffABCD[ NB4_2 + KLMN];
            auto KLMNdAcrossdB_zdCdotdD      = ERIBuffABCD[ NB4_3 + KLMN];
            auto KLMNdAdotdBdCcrossdD_x      = ERIBuffABCD[ NB4_4 + KLMN];
            auto KLMNdAdotdBdCcrossdD_y      = ERIBuffABCD[ NB4_5 + KLMN];
            auto KLMNdAdotdBdCcrossdD_z      = ERIBuffABCD[ NB4_6 + KLMN];
            auto KLMNdAcrossdB_xdCcrossdD_x  = ERIBuffABCD[ NB4_7 + KLMN];
            auto KLMNdAcrossdB_xdCcrossdD_y  = ERIBuffABCD[ NB4_8 + KLMN];
            auto KLMNdAcrossdB_xdCcrossdD_z  = ERIBuffABCD[ NB4_9 + KLMN];
            auto KLMNdAcrossdB_ydCcrossdD_x  = ERIBuffABCD[NB4_10 + KLMN];
            auto KLMNdAcrossdB_ydCcrossdD_y  = ERIBuffABCD[NB4_11 + KLMN];
            auto KLMNdAcrossdB_ydCcrossdD_z  = ERIBuffABCD[NB4_12 + KLMN];
            auto KLMNdAcrossdB_zdCcrossdD_x  = ERIBuffABCD[NB4_13 + KLMN];
            auto KLMNdAcrossdB_zdCcrossdD_y  = ERIBuffABCD[NB4_14 + KLMN];
            auto KLMNdAcrossdB_zdCcrossdD_z  = ERIBuffABCD[NB4_15 + KLMN];
 
  
            /* Hermitian Density Matrices */
            auto ScaleF = 2.0;
            auto iScaleF = dcomplex(0.0, 2.0);
  
            if(bf3_s==bf4_s) {
              ScaleF = 1.0;
              iScaleF = dcomplex(0.0, 1.0);
            }
  
            //
            // COULOMB
            // MNKL
  
            if(bf1 >= bf2) {
  
              /* Equation 70 in the paper */
              ADCSSMS[bf12] +=
                ( DCSSMS[bf43].real()*MNKLdAdotdBdCdotdD
                 -DCSSMZ[bf43].imag()*MNKLdAdotdBdCcrossdD_z
                 -DCSSMX[bf43].imag()*MNKLdAdotdBdCcrossdD_x
                 -DCSSMY[bf43].imag()*MNKLdAdotdBdCcrossdD_y
              )*ScaleF;
    
              /* Equation 71 in the paper */
              ADCSSMZ[bf12] +=
                ( DCSSMS[bf43].real()*MNKLdAcrossdB_zdCdotdD
                 -DCSSMX[bf43].imag()*MNKLdAcrossdB_zdCcrossdD_x
                 -DCSSMY[bf43].imag()*MNKLdAcrossdB_zdCcrossdD_y
                 -DCSSMZ[bf43].imag()*MNKLdAcrossdB_zdCcrossdD_z
                )*iScaleF;
        
              /* Equation 72 in the paper */
              ADCSSMX[bf12] +=
                ( DCSSMS[bf43].real()*MNKLdAcrossdB_xdCdotdD
                 -DCSSMX[bf43].imag()*MNKLdAcrossdB_xdCcrossdD_x
                 -DCSSMY[bf43].imag()*MNKLdAcrossdB_xdCcrossdD_y
                 -DCSSMZ[bf43].imag()*MNKLdAcrossdB_xdCcrossdD_z
                )*iScaleF;
        
              /* Equation 73 in the paper */
              ADCSSMY[bf12] +=
                ( DCSSMS[bf43].real()*MNKLdAcrossdB_ydCdotdD
                 -DCSSMX[bf43].imag()*MNKLdAcrossdB_ydCcrossdD_x
                 -DCSSMY[bf43].imag()*MNKLdAcrossdB_ydCcrossdD_y
                 -DXSSMZ[bf43].imag()*MNKLdAcrossdB_ydCcrossdD_z
                )*iScaleF;
  
            }
  
  
  
  
            /* KLMN */
            if((bf1_s!=bf3_s or bf2_s!=bf4_s) and bf3 >= bf4){
 
              ScaleF = 2.0;
              iScaleF = dcomplex(0.0, 2.0);
  
              if(bf1_s==bf2_s) {
                ScaleF = 1.0;
                iScaleF = dcomplex(0.0, 1.0);
              }
  
              /* Equation 70 in the paper */
              ADCSSMS[bf34] +=
                ( DCSSMS[bf21].real()*KLMNdAdotdBdCdotdD
                 -DCSSMZ[bf21].imag()*KLMNdAdotdBdCcrossdD_z
                 -DCSSMX[bf21].imag()*KLMNdAdotdBdCcrossdD_x
                 -DCSSMY[bf21].imag()*KLMNdAdotdBdCcrossdD_y
              )*ScaleF;
  
              /* Equation 71 in the paper */
              ADCSSMZ[bf34] +=
                ( DCSSMS[bf21].real()*KLMNdAcrossdB_zdCdotdD
                 -DCSSMX[bf21].imag()*KLMNdAcrossdB_zdCcrossdD_x
                 -DCSSMY[bf21].imag()*KLMNdAcrossdB_zdCcrossdD_y
                 -DCSSMZ[bf21].imag()*KLMNdAcrossdB_zdCcrossdD_z
              )*iScaleF;
      
              /* Equation 72 in the paper */
              ADCSSMX[bf34] +=
                ( DCSSMS[bf21].real()*KLMNdAcrossdB_xdCdotdD
                 -DCSSMX[bf21].imag()*KLMNdAcrossdB_xdCcrossdD_x
                 -DCSSMY[bf21].imag()*KLMNdAcrossdB_xdCcrossdD_y
                 -DCSSMZ[bf21].imag()*KLMNdAcrossdB_xdCcrossdD_z
              )*iScaleF;
      
              /* Equation 73 in the paper */
              ADCSSMY[bf34] +=
                ( DCSSMS[bf21].real()*KLMNdAcrossdB_ydCdotdD
                 -DCSSMX[bf21].imag()*KLMNdAcrossdB_ydCcrossdD_x
                 -DCSSMY[bf21].imag()*KLMNdAcrossdB_ydCcrossdD_y
                 -DCSSMZ[bf21].imag()*KLMNdAcrossdB_ydCcrossdD_z
              )*iScaleF;
  
            }
  
    
#if 1
            /* EXCHANGE */
 
  
            // MNKL
            if(bf1 >= bf4) {
  
              /* Equation 70 in the paper */
              ADXSSMS[bf14] +=
                  DXSSMS[bf23]*(-MNKLdAdotdBdCdotdD +MNKLdAcrossdB_xdCcrossdD_x +MNKLdAcrossdB_ydCcrossdD_y +MNKLdAcrossdB_zdCcrossdD_z)
               +( DXSSMX[bf23]*(-MNKLdAcrossdB_xdCdotdD -MNKLdAdotdBdCcrossdD_x -MNKLdAcrossdB_ydCcrossdD_z +MNKLdAcrossdB_zdCcrossdD_y)
                 +DXSSMY[bf23]*(-MNKLdAcrossdB_ydCdotdD -MNKLdAdotdBdCcrossdD_y -MNKLdAcrossdB_zdCcrossdD_x +MNKLdAcrossdB_xdCcrossdD_z)
                 +DXSSMZ[bf23]*(-MNKLdAcrossdB_zdCdotdD -MNKLdAdotdBdCcrossdD_z -MNKLdAcrossdB_xdCcrossdD_y +MNKLdAcrossdB_ydCcrossdD_x))*iscale;
    
              /* Equation 71 in the paper */
              ADXSSMZ[bf14] +=
                DXSSMZ[bf23]*(-MNKLdAdotdBdCdotdD +MNKLdAcrossdB_zdCcrossdD_z -MNKLdAcrossdB_xdCcrossdD_x -MNKLdAcrossdB_ydCcrossdD_y)
               +DXSSMS[bf23]*(-MNKLdAdotdBdCcrossdD_z -MNKLdAcrossdB_zdCdotdD +MNKLdAcrossdB_xdCcrossdD_y -MNKLdAcrossdB_ydCcrossdD_x)*iscale
               +DXSSMY[bf23]*(-MNKLdAdotdBdCcrossdD_x +MNKLdAcrossdB_xdCdotdD +MNKLdAcrossdB_ydCcrossdD_z +MNKLdAcrossdB_zdCcrossdD_y)
               +DXSSMX[bf23]*( MNKLdAdotdBdCcrossdD_y -MNKLdAcrossdB_ydCdotdD +MNKLdAcrossdB_xdCcrossdD_z +MNKLdAcrossdB_zdCcrossdD_x);
          
              /* Equation 72 in the paper */
              ADXSSMX[bf14] += 
                DXSSMX[bf23]*(-MNKLdAdotdBdCdotdD +MNKLdAcrossdB_xdCcrossdD_x -MNKLdAcrossdB_ydCcrossdD_y -MNKLdAcrossdB_zdCcrossdD_z)
               +DXSSMS[bf23]*(-MNKLdAdotdBdCcrossdD_x -MNKLdAcrossdB_xdCdotdD +MNKLdAcrossdB_ydCcrossdD_z -MNKLdAcrossdB_zdCcrossdD_y)*iscale
               +DXSSMY[bf23]*( MNKLdAdotdBdCcrossdD_z -MNKLdAcrossdB_zdCdotdD +MNKLdAcrossdB_xdCcrossdD_y +MNKLdAcrossdB_ydCcrossdD_x)
               +DXSSMZ[bf23]*(-MNKLdAdotdBdCcrossdD_y +MNKLdAcrossdB_ydCdotdD +MNKLdAcrossdB_xdCcrossdD_z +MNKLdAcrossdB_zdCcrossdD_x);
        
              /* Equation 73 in the paper */
              ADXSSMY[bf14] += 
                DXSSMY[bf23]*(-MNKLdAdotdBdCdotdD +MNKLdAcrossdB_ydCcrossdD_y -MNKLdAcrossdB_xdCcrossdD_x -MNKLdAcrossdB_zdCcrossdD_z)
               +DXSSMS[bf23]*(-MNKLdAdotdBdCcrossdD_y -MNKLdAcrossdB_ydCdotdD -MNKLdAcrossdB_xdCcrossdD_z +MNKLdAcrossdB_zdCcrossdD_x)*iscale
               +DXSSMX[bf23]*(-MNKLdAdotdBdCcrossdD_z +MNKLdAcrossdB_zdCdotdD +MNKLdAcrossdB_xdCcrossdD_y +MNKLdAcrossdB_ydCcrossdD_x)
               +DXSSMZ[bf23]*( MNKLdAdotdBdCcrossdD_x -MNKLdAcrossdB_xdCdotdD +MNKLdAcrossdB_ydCcrossdD_z +MNKLdAcrossdB_zdCcrossdD_y);
    
            }
  
            //MNLK
            if(bf3_s!=bf4_s and bf1 >= bf3) {
  
              ADXSSMS[bf13] +=
                  DXSSMS[bf24]*(-MNKLdAdotdBdCdotdD -MNKLdAcrossdB_xdCcrossdD_x -MNKLdAcrossdB_ydCcrossdD_y -MNKLdAcrossdB_zdCcrossdD_z)
               +( DXSSMX[bf24]*(-MNKLdAcrossdB_xdCdotdD +MNKLdAdotdBdCcrossdD_x +MNKLdAcrossdB_ydCcrossdD_z -MNKLdAcrossdB_zdCcrossdD_y)
                 +DXSSMY[bf24]*(-MNKLdAcrossdB_ydCdotdD +MNKLdAdotdBdCcrossdD_y +MNKLdAcrossdB_zdCcrossdD_x -MNKLdAcrossdB_xdCcrossdD_z)
                 +DXSSMZ[bf24]*(-MNKLdAcrossdB_zdCdotdD +MNKLdAdotdBdCcrossdD_z +MNKLdAcrossdB_xdCcrossdD_y -MNKLdAcrossdB_ydCcrossdD_x))*iscale;
   
              ADXSSMZ[bf13] +=
                DXSSMZ[bf24]*(-MNKLdAdotdBdCdotdD -MNKLdAcrossdB_zdCcrossdD_z +MNKLdAcrossdB_xdCcrossdD_x +MNKLdAcrossdB_ydCcrossdD_y)
               +DXSSMS[bf24]*( MNKLdAdotdBdCcrossdD_z -MNKLdAcrossdB_zdCdotdD -MNKLdAcrossdB_xdCcrossdD_y +MNKLdAcrossdB_ydCcrossdD_x)*iscale
               +DXSSMY[bf24]*( MNKLdAdotdBdCcrossdD_x +MNKLdAcrossdB_xdCdotdD -MNKLdAcrossdB_ydCcrossdD_z -MNKLdAcrossdB_zdCcrossdD_y)
               +DXSSMX[bf24]*(-MNKLdAdotdBdCcrossdD_y -MNKLdAcrossdB_ydCdotdD -MNKLdAcrossdB_xdCcrossdD_z -MNKLdAcrossdB_zdCcrossdD_x);
          
              ADXSSMX[bf13] += 
                DXSSMX[bf24]*(-MNKLdAdotdBdCdotdD -MNKLdAcrossdB_xdCcrossdD_x +MNKLdAcrossdB_ydCcrossdD_y +MNKLdAcrossdB_zdCcrossdD_z)
               +DXSSMS[bf24]*( MNKLdAdotdBdCcrossdD_x -MNKLdAcrossdB_xdCdotdD -MNKLdAcrossdB_ydCcrossdD_z +MNKLdAcrossdB_zdCcrossdD_y)*iscale
               +DXSSMY[bf24]*(-MNKLdAdotdBdCcrossdD_z -MNKLdAcrossdB_zdCdotdD -MNKLdAcrossdB_xdCcrossdD_y -MNKLdAcrossdB_ydCcrossdD_x)
               +DXSSMZ[bf24]*( MNKLdAdotdBdCcrossdD_y +MNKLdAcrossdB_ydCdotdD -MNKLdAcrossdB_xdCcrossdD_z -MNKLdAcrossdB_zdCcrossdD_x);
        
              ADXSSMY[bf13] += 
                DXSSMY[bf24]*(-MNKLdAdotdBdCdotdD -MNKLdAcrossdB_ydCcrossdD_y +MNKLdAcrossdB_xdCcrossdD_x +MNKLdAcrossdB_zdCcrossdD_z)
               +DXSSMS[bf24]*( MNKLdAdotdBdCcrossdD_y -MNKLdAcrossdB_ydCdotdD +MNKLdAcrossdB_xdCcrossdD_z -MNKLdAcrossdB_zdCcrossdD_x)*iscale
               +DXSSMX[bf24]*( MNKLdAdotdBdCcrossdD_z +MNKLdAcrossdB_zdCdotdD -MNKLdAcrossdB_xdCcrossdD_y -MNKLdAcrossdB_ydCcrossdD_x)
               +DXSSMZ[bf24]*(-MNKLdAdotdBdCcrossdD_x -MNKLdAcrossdB_xdCdotdD -MNKLdAcrossdB_ydCcrossdD_z -MNKLdAcrossdB_zdCcrossdD_y);
  
            }
  
  
  
            //NMKL
            if(bf1_s!=bf2_s){
              
              if(bf2 >= bf4) {
  
                ADXSSMS[bf24] +=
                    DXSSMS[bf13]*(-MNKLdAdotdBdCdotdD -MNKLdAcrossdB_xdCcrossdD_x -MNKLdAcrossdB_ydCcrossdD_y -MNKLdAcrossdB_zdCcrossdD_z)
                 +( DXSSMX[bf13]*( MNKLdAcrossdB_xdCdotdD -MNKLdAdotdBdCcrossdD_x +MNKLdAcrossdB_ydCcrossdD_z -MNKLdAcrossdB_zdCcrossdD_y)
                   +DXSSMY[bf13]*( MNKLdAcrossdB_ydCdotdD -MNKLdAdotdBdCcrossdD_y +MNKLdAcrossdB_zdCcrossdD_x -MNKLdAcrossdB_xdCcrossdD_z)
                   +DXSSMZ[bf13]*( MNKLdAcrossdB_zdCdotdD -MNKLdAdotdBdCcrossdD_z +MNKLdAcrossdB_xdCcrossdD_y -MNKLdAcrossdB_ydCcrossdD_x))*iscale;
    
                ADXSSMZ[bf24] +=
                  DXSSMZ[bf13]*(-MNKLdAdotdBdCdotdD -MNKLdAcrossdB_zdCcrossdD_z +MNKLdAcrossdB_xdCcrossdD_x +MNKLdAcrossdB_ydCcrossdD_y)
                 +DXSSMS[bf13]*(-MNKLdAdotdBdCcrossdD_z +MNKLdAcrossdB_zdCdotdD -MNKLdAcrossdB_xdCcrossdD_y +MNKLdAcrossdB_ydCcrossdD_x)*iscale
                 +DXSSMY[bf13]*(-MNKLdAdotdBdCcrossdD_x -MNKLdAcrossdB_xdCdotdD -MNKLdAcrossdB_ydCcrossdD_z -MNKLdAcrossdB_zdCcrossdD_y)
                 +DXSSMX[bf13]*( MNKLdAdotdBdCcrossdD_y +MNKLdAcrossdB_ydCdotdD -MNKLdAcrossdB_xdCcrossdD_z -MNKLdAcrossdB_zdCcrossdD_x);
            
                ADXSSMX[bf24] += 
                  DXSSMX[bf13]*(-MNKLdAdotdBdCdotdD -MNKLdAcrossdB_xdCcrossdD_x +MNKLdAcrossdB_ydCcrossdD_y +MNKLdAcrossdB_zdCcrossdD_z)
                 +DXSSMS[bf13]*(-MNKLdAdotdBdCcrossdD_x +MNKLdAcrossdB_xdCdotdD -MNKLdAcrossdB_ydCcrossdD_z +MNKLdAcrossdB_zdCcrossdD_y)*iscale
                 +DXSSMY[bf13]*( MNKLdAdotdBdCcrossdD_z +MNKLdAcrossdB_zdCdotdD -MNKLdAcrossdB_xdCcrossdD_y -MNKLdAcrossdB_ydCcrossdD_x)
                 +DXSSMZ[bf13]*(-MNKLdAdotdBdCcrossdD_y -MNKLdAcrossdB_ydCdotdD -MNKLdAcrossdB_xdCcrossdD_z -MNKLdAcrossdB_zdCcrossdD_x);
          
                ADXSSMY[bf24] += 
                  DXSSMY[bf13]*(-MNKLdAdotdBdCdotdD -MNKLdAcrossdB_ydCcrossdD_y +MNKLdAcrossdB_xdCcrossdD_x +MNKLdAcrossdB_zdCcrossdD_z)
                 +DXSSMS[bf13]*(-MNKLdAdotdBdCcrossdD_y +MNKLdAcrossdB_ydCdotdD +MNKLdAcrossdB_xdCcrossdD_z -MNKLdAcrossdB_zdCcrossdD_x)*iscale
                 +DXSSMX[bf13]*(-MNKLdAdotdBdCcrossdD_z -MNKLdAcrossdB_zdCdotdD -MNKLdAcrossdB_xdCcrossdD_y -MNKLdAcrossdB_ydCcrossdD_x)
                 +DXSSMZ[bf13]*( MNKLdAdotdBdCcrossdD_x +MNKLdAcrossdB_xdCdotdD -MNKLdAcrossdB_ydCcrossdD_z -MNKLdAcrossdB_zdCcrossdD_y);
  
              }
    
  
              if(bf3_s!=bf4_s and bf2 >= bf3) {
  
                ADXSSMS[bf23] +=
                    DXSSMS[bf14]*(-MNKLdAdotdBdCdotdD +MNKLdAcrossdB_xdCcrossdD_x +MNKLdAcrossdB_ydCcrossdD_y +MNKLdAcrossdB_zdCcrossdD_z)
                 +( DXSSMX[bf14]*( MNKLdAcrossdB_xdCdotdD +MNKLdAdotdBdCcrossdD_x -MNKLdAcrossdB_ydCcrossdD_z +MNKLdAcrossdB_zdCcrossdD_y)
                   +DXSSMY[bf14]*( MNKLdAcrossdB_ydCdotdD +MNKLdAdotdBdCcrossdD_y -MNKLdAcrossdB_zdCcrossdD_x +MNKLdAcrossdB_xdCcrossdD_z)
                   +DXSSMZ[bf14]*( MNKLdAcrossdB_zdCdotdD +MNKLdAdotdBdCcrossdD_z -MNKLdAcrossdB_xdCcrossdD_y +MNKLdAcrossdB_ydCcrossdD_x))*iscale;
  
                ADXSSMZ[bf23] +=
                  DXSSMZ[bf14]*(-MNKLdAdotdBdCdotdD +MNKLdAcrossdB_zdCcrossdD_z -MNKLdAcrossdB_xdCcrossdD_x -MNKLdAcrossdB_ydCcrossdD_y)
                 +DXSSMS[bf14]*( MNKLdAdotdBdCcrossdD_z +MNKLdAcrossdB_zdCdotdD +MNKLdAcrossdB_xdCcrossdD_y -MNKLdAcrossdB_ydCcrossdD_x)*iscale
                 +DXSSMY[bf14]*( MNKLdAdotdBdCcrossdD_x -MNKLdAcrossdB_xdCdotdD +MNKLdAcrossdB_ydCcrossdD_z +MNKLdAcrossdB_zdCcrossdD_y)
                 +DXSSMX[bf14]*(-MNKLdAdotdBdCcrossdD_y +MNKLdAcrossdB_ydCdotdD +MNKLdAcrossdB_xdCcrossdD_z +MNKLdAcrossdB_zdCcrossdD_x);
            
                ADXSSMX[bf23] += 
                  DXSSMX[bf14]*(-MNKLdAdotdBdCdotdD +MNKLdAcrossdB_xdCcrossdD_x -MNKLdAcrossdB_ydCcrossdD_y -MNKLdAcrossdB_zdCcrossdD_z)
                 +DXSSMS[bf14]*( MNKLdAdotdBdCcrossdD_x +MNKLdAcrossdB_xdCdotdD +MNKLdAcrossdB_ydCcrossdD_z -MNKLdAcrossdB_zdCcrossdD_y)*iscale
                 +DXSSMY[bf14]*(-MNKLdAdotdBdCcrossdD_z +MNKLdAcrossdB_zdCdotdD +MNKLdAcrossdB_xdCcrossdD_y +MNKLdAcrossdB_ydCcrossdD_x)
                 +DXSSMZ[bf14]*( MNKLdAdotdBdCcrossdD_y -MNKLdAcrossdB_ydCdotdD +MNKLdAcrossdB_xdCcrossdD_z +MNKLdAcrossdB_zdCcrossdD_x);
          
                ADXSSMY[bf23] += 
                  DXSSMY[bf14]*(-MNKLdAdotdBdCdotdD +MNKLdAcrossdB_ydCcrossdD_y -MNKLdAcrossdB_xdCcrossdD_x -MNKLdAcrossdB_zdCcrossdD_z)
                 +DXSSMS[bf14]*( MNKLdAdotdBdCcrossdD_y +MNKLdAcrossdB_ydCdotdD -MNKLdAcrossdB_xdCcrossdD_z +MNKLdAcrossdB_zdCcrossdD_x)*iscale
                 +DXSSMX[bf14]*( MNKLdAdotdBdCcrossdD_z -MNKLdAcrossdB_zdCdotdD +MNKLdAcrossdB_xdCcrossdD_y +MNKLdAcrossdB_ydCcrossdD_x)
                 +DXSSMZ[bf14]*(-MNKLdAdotdBdCcrossdD_x +MNKLdAcrossdB_xdCdotdD +MNKLdAcrossdB_ydCcrossdD_z +MNKLdAcrossdB_zdCcrossdD_y);
  
      
              }
            }
  
  
  
  
            if(bf1_s!=bf3_s or bf2_s!=bf4_s){
 
              if(bf3 >= bf2 ) {
  
                ADXSSMS[bf32] +=
                    DXSSMS[bf41]*(-KLMNdAdotdBdCdotdD +KLMNdAcrossdB_xdCcrossdD_x +KLMNdAcrossdB_ydCcrossdD_y +KLMNdAcrossdB_zdCcrossdD_z)
                 +( DXSSMX[bf41]*(-KLMNdAcrossdB_xdCdotdD -KLMNdAdotdBdCcrossdD_x -KLMNdAcrossdB_ydCcrossdD_z +KLMNdAcrossdB_zdCcrossdD_y)
                   +DXSSMY[bf41]*(-KLMNdAcrossdB_ydCdotdD -KLMNdAdotdBdCcrossdD_y -KLMNdAcrossdB_zdCcrossdD_x +KLMNdAcrossdB_xdCcrossdD_z)
                   +DXSSMZ[bf41]*(-KLMNdAcrossdB_zdCdotdD -KLMNdAdotdBdCcrossdD_z -KLMNdAcrossdB_xdCcrossdD_y +KLMNdAcrossdB_ydCcrossdD_x))*iscale;
    
                ADXSSMZ[bf32] +=
                  DXSSMZ[bf41]*(-KLMNdAdotdBdCdotdD +KLMNdAcrossdB_zdCcrossdD_z -KLMNdAcrossdB_xdCcrossdD_x -KLMNdAcrossdB_ydCcrossdD_y)
                 +DXSSMS[bf41]*(-KLMNdAdotdBdCcrossdD_z -KLMNdAcrossdB_zdCdotdD +KLMNdAcrossdB_xdCcrossdD_y -KLMNdAcrossdB_ydCcrossdD_x)*iscale
                 +DXSSMY[bf41]*(-KLMNdAdotdBdCcrossdD_x +KLMNdAcrossdB_xdCdotdD +KLMNdAcrossdB_ydCcrossdD_z +KLMNdAcrossdB_zdCcrossdD_y)
                 +DXSSMX[bf41]*( KLMNdAdotdBdCcrossdD_y -KLMNdAcrossdB_ydCdotdD +KLMNdAcrossdB_xdCcrossdD_z +KLMNdAcrossdB_zdCcrossdD_x);
            
                ADXSSMX[bf32] += 
                  DXSSMX[bf41]*(-KLMNdAdotdBdCdotdD +KLMNdAcrossdB_xdCcrossdD_x -KLMNdAcrossdB_ydCcrossdD_y -KLMNdAcrossdB_zdCcrossdD_z)
                 +DXSSMS[bf41]*(-KLMNdAdotdBdCcrossdD_x -KLMNdAcrossdB_xdCdotdD +KLMNdAcrossdB_ydCcrossdD_z -KLMNdAcrossdB_zdCcrossdD_y)*iscale
                 +DXSSMY[bf41]*( KLMNdAdotdBdCcrossdD_z -KLMNdAcrossdB_zdCdotdD +KLMNdAcrossdB_xdCcrossdD_y +KLMNdAcrossdB_ydCcrossdD_x)
                 +DXSSMZ[bf41]*(-KLMNdAdotdBdCcrossdD_y +KLMNdAcrossdB_ydCdotdD +KLMNdAcrossdB_xdCcrossdD_z +KLMNdAcrossdB_zdCcrossdD_x);
          
                ADXSSMY[bf32] += 
                  DXSSMY[bf41]*(-KLMNdAdotdBdCdotdD +KLMNdAcrossdB_ydCcrossdD_y -KLMNdAcrossdB_xdCcrossdD_x -KLMNdAcrossdB_zdCcrossdD_z)
                 +DXSSMS[bf41]*(-KLMNdAdotdBdCcrossdD_y -KLMNdAcrossdB_ydCdotdD -KLMNdAcrossdB_xdCcrossdD_z +KLMNdAcrossdB_zdCcrossdD_x)*iscale
                 +DXSSMX[bf41]*(-KLMNdAdotdBdCcrossdD_z +KLMNdAcrossdB_zdCdotdD +KLMNdAcrossdB_xdCcrossdD_y +KLMNdAcrossdB_ydCcrossdD_x)
                 +DXSSMZ[bf41]*( KLMNdAdotdBdCcrossdD_x -KLMNdAcrossdB_xdCdotdD +KLMNdAcrossdB_ydCcrossdD_z +KLMNdAcrossdB_zdCcrossdD_y);
  
              }
   
              
              if(bf1_s!=bf2_s and bf3>=bf1) {
  
                ADXSSMS[bf31] +=
                    DXSSMS[bf42]*(-KLMNdAdotdBdCdotdD -KLMNdAcrossdB_xdCcrossdD_x -KLMNdAcrossdB_ydCcrossdD_y -KLMNdAcrossdB_zdCcrossdD_z)
                 +( DXSSMX[bf42]*(-KLMNdAcrossdB_xdCdotdD +KLMNdAdotdBdCcrossdD_x +KLMNdAcrossdB_ydCcrossdD_z -KLMNdAcrossdB_zdCcrossdD_y)
                   +DXSSMY[bf42]*(-KLMNdAcrossdB_ydCdotdD +KLMNdAdotdBdCcrossdD_y +KLMNdAcrossdB_zdCcrossdD_x -KLMNdAcrossdB_xdCcrossdD_z)
                   +DXSSMZ[bf42]*(-KLMNdAcrossdB_zdCdotdD +KLMNdAdotdBdCcrossdD_z +KLMNdAcrossdB_xdCcrossdD_y -KLMNdAcrossdB_ydCcrossdD_x))*iscale;
     
                ADXSSMZ[bf31] +=
                  DXSSMZ[bf42]*(-KLMNdAdotdBdCdotdD -KLMNdAcrossdB_zdCcrossdD_z +KLMNdAcrossdB_xdCcrossdD_x +KLMNdAcrossdB_ydCcrossdD_y)
                 +DXSSMS[bf42]*( KLMNdAdotdBdCcrossdD_z -KLMNdAcrossdB_zdCdotdD -KLMNdAcrossdB_xdCcrossdD_y +KLMNdAcrossdB_ydCcrossdD_x)*iscale
                 +DXSSMY[bf42]*( KLMNdAdotdBdCcrossdD_x +KLMNdAcrossdB_xdCdotdD -KLMNdAcrossdB_ydCcrossdD_z -KLMNdAcrossdB_zdCcrossdD_y)
                 +DXSSMX[bf42]*(-KLMNdAdotdBdCcrossdD_y -KLMNdAcrossdB_ydCdotdD -KLMNdAcrossdB_xdCcrossdD_z -KLMNdAcrossdB_zdCcrossdD_x);
            
                ADXSSMX[bf31] += 
                  DXSSMX[bf42]*(-KLMNdAdotdBdCdotdD -KLMNdAcrossdB_xdCcrossdD_x +KLMNdAcrossdB_ydCcrossdD_y +KLMNdAcrossdB_zdCcrossdD_z)
                 +DXSSMS[bf42]*( KLMNdAdotdBdCcrossdD_x -KLMNdAcrossdB_xdCdotdD -KLMNdAcrossdB_ydCcrossdD_z +KLMNdAcrossdB_zdCcrossdD_y)*iscale
                 +DXSSMY[bf42]*(-KLMNdAdotdBdCcrossdD_z -KLMNdAcrossdB_zdCdotdD -KLMNdAcrossdB_xdCcrossdD_y -KLMNdAcrossdB_ydCcrossdD_x)
                 +DXSSMZ[bf42]*( KLMNdAdotdBdCcrossdD_y +KLMNdAcrossdB_ydCdotdD -KLMNdAcrossdB_xdCcrossdD_z -KLMNdAcrossdB_zdCcrossdD_x);
          
                ADXSSMY[bf31] += 
                  DXSSMY[bf42]*(-KLMNdAdotdBdCdotdD -KLMNdAcrossdB_ydCcrossdD_y +KLMNdAcrossdB_xdCcrossdD_x +KLMNdAcrossdB_zdCcrossdD_z)
                 +DXSSMS[bf42]*( KLMNdAdotdBdCcrossdD_y -KLMNdAcrossdB_ydCdotdD +KLMNdAcrossdB_xdCcrossdD_z -KLMNdAcrossdB_zdCcrossdD_x)*iscale
                 +DXSSMX[bf42]*( KLMNdAdotdBdCcrossdD_z +KLMNdAcrossdB_zdCdotdD -KLMNdAcrossdB_xdCcrossdD_y -KLMNdAcrossdB_ydCcrossdD_x)
                 +DXSSMZ[bf42]*(-KLMNdAdotdBdCcrossdD_x -KLMNdAcrossdB_xdCdotdD -KLMNdAcrossdB_ydCcrossdD_z -KLMNdAcrossdB_zdCcrossdD_y);
     
     
              }
    
              //NMKL
              if(bf3_s!=bf4_s and bf4>=bf2){
    
                ADXSSMS[bf42] +=
                    DXSSMS[bf31]*(-KLMNdAdotdBdCdotdD -KLMNdAcrossdB_xdCcrossdD_x -KLMNdAcrossdB_ydCcrossdD_y -KLMNdAcrossdB_zdCcrossdD_z)
                 +( DXSSMX[bf31]*( KLMNdAcrossdB_xdCdotdD -KLMNdAdotdBdCcrossdD_x +KLMNdAcrossdB_ydCcrossdD_z -KLMNdAcrossdB_zdCcrossdD_y)
                   +DXSSMY[bf31]*( KLMNdAcrossdB_ydCdotdD -KLMNdAdotdBdCcrossdD_y +KLMNdAcrossdB_zdCcrossdD_x -KLMNdAcrossdB_xdCcrossdD_z)
                   +DXSSMZ[bf31]*( KLMNdAcrossdB_zdCdotdD -KLMNdAdotdBdCcrossdD_z +KLMNdAcrossdB_xdCcrossdD_y -KLMNdAcrossdB_ydCcrossdD_x))*iscale;
     
                ADXSSMZ[bf42] +=
                  DXSSMZ[bf31]*(-KLMNdAdotdBdCdotdD -KLMNdAcrossdB_zdCcrossdD_z +KLMNdAcrossdB_xdCcrossdD_x +KLMNdAcrossdB_ydCcrossdD_y)
                 +DXSSMS[bf31]*(-KLMNdAdotdBdCcrossdD_z +KLMNdAcrossdB_zdCdotdD -KLMNdAcrossdB_xdCcrossdD_y +KLMNdAcrossdB_ydCcrossdD_x)*iscale
                 +DXSSMY[bf31]*(-KLMNdAdotdBdCcrossdD_x -KLMNdAcrossdB_xdCdotdD -KLMNdAcrossdB_ydCcrossdD_z -KLMNdAcrossdB_zdCcrossdD_y)
                 +DXSSMX[bf31]*( KLMNdAdotdBdCcrossdD_y +KLMNdAcrossdB_ydCdotdD -KLMNdAcrossdB_xdCcrossdD_z -KLMNdAcrossdB_zdCcrossdD_x);
            
                ADXSSMX[bf42] += 
                  DXSSMX[bf31]*(-KLMNdAdotdBdCdotdD -KLMNdAcrossdB_xdCcrossdD_x +KLMNdAcrossdB_ydCcrossdD_y +KLMNdAcrossdB_zdCcrossdD_z)
                 +DXSSMS[bf31]*(-KLMNdAdotdBdCcrossdD_x +KLMNdAcrossdB_xdCdotdD -KLMNdAcrossdB_ydCcrossdD_z +KLMNdAcrossdB_zdCcrossdD_y)*iscale
                 +DXSSMY[bf31]*( KLMNdAdotdBdCcrossdD_z +KLMNdAcrossdB_zdCdotdD -KLMNdAcrossdB_xdCcrossdD_y -KLMNdAcrossdB_ydCcrossdD_x)
                 +DXSSMZ[bf31]*(-KLMNdAdotdBdCcrossdD_y -KLMNdAcrossdB_ydCdotdD -KLMNdAcrossdB_xdCcrossdD_z -KLMNdAcrossdB_zdCcrossdD_x);
          
                ADXSSMY[bf42] += 
                  DXSSMY[bf31]*(-KLMNdAdotdBdCdotdD -KLMNdAcrossdB_ydCcrossdD_y +KLMNdAcrossdB_xdCcrossdD_x +KLMNdAcrossdB_zdCcrossdD_z)
                 +DXSSMS[bf31]*(-KLMNdAdotdBdCcrossdD_y +KLMNdAcrossdB_ydCdotdD +KLMNdAcrossdB_xdCcrossdD_z -KLMNdAcrossdB_zdCcrossdD_x)*iscale
                 +DXSSMX[bf31]*(-KLMNdAdotdBdCcrossdD_z -KLMNdAcrossdB_zdCdotdD -KLMNdAcrossdB_xdCcrossdD_y -KLMNdAcrossdB_ydCcrossdD_x)
                 +DXSSMZ[bf31]*( KLMNdAdotdBdCcrossdD_x +KLMNdAcrossdB_xdCdotdD -KLMNdAcrossdB_ydCcrossdD_z -KLMNdAcrossdB_zdCcrossdD_y);
     
      
    
                if(bf1_s!=bf2_s and bf4>=bf1) {
    
                  ADXSSMS[bf41] +=
                      DXSSMS[bf32]*(-KLMNdAdotdBdCdotdD +KLMNdAcrossdB_xdCcrossdD_x +KLMNdAcrossdB_ydCcrossdD_y +KLMNdAcrossdB_zdCcrossdD_z)
                   +( DXSSMX[bf32]*( KLMNdAcrossdB_xdCdotdD +KLMNdAdotdBdCcrossdD_x -KLMNdAcrossdB_ydCcrossdD_z +KLMNdAcrossdB_zdCcrossdD_y)
                     +DXSSMY[bf32]*( KLMNdAcrossdB_ydCdotdD +KLMNdAdotdBdCcrossdD_y -KLMNdAcrossdB_zdCcrossdD_x +KLMNdAcrossdB_xdCcrossdD_z)
                     +DXSSMZ[bf32]*( KLMNdAcrossdB_zdCdotdD +KLMNdAdotdBdCcrossdD_z -KLMNdAcrossdB_xdCcrossdD_y +KLMNdAcrossdB_ydCcrossdD_x))*iscale;
  
                  ADXSSMZ[bf41] +=
                    DXSSMZ[bf32]*(-KLMNdAdotdBdCdotdD +KLMNdAcrossdB_zdCcrossdD_z -KLMNdAcrossdB_xdCcrossdD_x -KLMNdAcrossdB_ydCcrossdD_y)
                   +DXSSMS[bf32]*( KLMNdAdotdBdCcrossdD_z +KLMNdAcrossdB_zdCdotdD +KLMNdAcrossdB_xdCcrossdD_y -KLMNdAcrossdB_ydCcrossdD_x)*iscale
                   +DXSSMY[bf32]*( KLMNdAdotdBdCcrossdD_x -KLMNdAcrossdB_xdCdotdD +KLMNdAcrossdB_ydCcrossdD_z +KLMNdAcrossdB_zdCcrossdD_y)
                   +DXSSMX[bf32]*(-KLMNdAdotdBdCcrossdD_y +KLMNdAcrossdB_ydCdotdD +KLMNdAcrossdB_xdCcrossdD_z +KLMNdAcrossdB_zdCcrossdD_x);
              
                  ADXSSMX[bf41] += 
                    DXSSMX[bf32]*(-KLMNdAdotdBdCdotdD +KLMNdAcrossdB_xdCcrossdD_x -KLMNdAcrossdB_ydCcrossdD_y -KLMNdAcrossdB_zdCcrossdD_z)
                   +DXSSMS[bf32]*( KLMNdAdotdBdCcrossdD_x +KLMNdAcrossdB_xdCdotdD +KLMNdAcrossdB_ydCcrossdD_z -KLMNdAcrossdB_zdCcrossdD_y)*iscale
                   +DXSSMY[bf32]*(-KLMNdAdotdBdCcrossdD_z +KLMNdAcrossdB_zdCdotdD +KLMNdAcrossdB_xdCcrossdD_y +KLMNdAcrossdB_ydCcrossdD_x)
                   +DXSSMZ[bf32]*( KLMNdAdotdBdCcrossdD_y -KLMNdAcrossdB_ydCdotdD +KLMNdAcrossdB_xdCcrossdD_z +KLMNdAcrossdB_zdCcrossdD_x);
            
                  ADXSSMY[bf41] += 
                    DXSSMY[bf32]*(-KLMNdAdotdBdCdotdD +KLMNdAcrossdB_ydCcrossdD_y -KLMNdAcrossdB_xdCcrossdD_x -KLMNdAcrossdB_zdCcrossdD_z)
                   +DXSSMS[bf32]*( KLMNdAdotdBdCcrossdD_y +KLMNdAcrossdB_ydCdotdD -KLMNdAcrossdB_xdCcrossdD_z +KLMNdAcrossdB_zdCcrossdD_x)*iscale
                   +DXSSMX[bf32]*( KLMNdAdotdBdCcrossdD_z -KLMNdAcrossdB_zdCdotdD +KLMNdAcrossdB_xdCcrossdD_y +KLMNdAcrossdB_ydCcrossdD_x)
                   +DXSSMZ[bf32]*(-KLMNdAdotdBdCcrossdD_x +KLMNdAcrossdB_xdCdotdD +KLMNdAcrossdB_ydCcrossdD_z +KLMNdAcrossdB_zdCcrossdD_y);
        
                }
              }
 
            }
#endif  
	  }
	  }
	  }
          } // contraction loop
  
#endif // Contraction
  
  
      }; // loop s4
      }; // loop s3
      }; // loop s2
      }; // loop s1
  
  
      }; // OpenMP context
  
  
      dcomplex iscale = dcomplex(0.0, 1.0);
      for( auto iTh  = 0; iTh < nThreads; iTh++) {
   
        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][CSSMS],nBasis,MatsT(1.0),
           matList[CSSMS].AX,nBasis,matList[CSSMS].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][CSSMX],nBasis,MatsT(1.0),
           matList[CSSMX].AX,nBasis,matList[CSSMX].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][CSSMY],nBasis,MatsT(1.0),
           matList[CSSMY].AX,nBasis,matList[CSSMY].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][CSSMZ],nBasis,MatsT(1.0),
           matList[CSSMZ].AX,nBasis,matList[CSSMZ].AX,nBasis);
 
        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XSSMS],nBasis,MatsT(1.0),
           matList[XSSMS].AX,nBasis,matList[XSSMS].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XSSMX],nBasis,MatsT(1.0),
           matList[XSSMX].AX,nBasis,matList[XSSMX].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XSSMY],nBasis,MatsT(1.0),
           matList[XSSMY].AX,nBasis,matList[XSSMY].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XSSMZ],nBasis,MatsT(1.0),
           matList[XSSMZ].AX,nBasis,matList[XSSMZ].AX,nBasis);
  
      };
      
#if 1
      // Take care of the Hermitian symmetry in the LL and SS blocks
      auto ADCSSMS = matList[CSSMS].AX;
      auto ADCSSMX = matList[CSSMX].AX;
      auto ADCSSMY = matList[CSSMY].AX;
      auto ADCSSMZ = matList[CSSMZ].AX;
      auto ADXSSMS = matList[XSSMS].AX;
      auto ADXSSMX = matList[XSSMX].AX;
      auto ADXSSMY = matList[XSSMY].AX;
      auto ADXSSMZ = matList[XSSMZ].AX;

      for( auto i = 0; i < nBasis; i++ )
      for( auto j = 0; j < i; j++ ) {
        ADCSSMS[j + i*nBasis] = std::conj(ADCSSMS[i + j*nBasis]);
        ADCSSMX[j + i*nBasis] = std::conj(ADCSSMX[i + j*nBasis]);
        ADCSSMY[j + i*nBasis] = std::conj(ADCSSMY[i + j*nBasis]);
        ADCSSMZ[j + i*nBasis] = std::conj(ADCSSMZ[i + j*nBasis]);
        ADXSSMS[j + i*nBasis] = std::conj(ADXSSMS[i + j*nBasis]);
        ADXSSMX[j + i*nBasis] = std::conj(ADXSSMX[i + j*nBasis]);
        ADXSSMY[j + i*nBasis] = std::conj(ADXSSMY[i + j*nBasis]);
        ADXSSMZ[j + i*nBasis] = std::conj(ADXSSMZ[i + j*nBasis]);
      }
#endif


      memManager_.free(ERIBuffer);
      memManager_.free(buffAll, cacheAll);
#ifdef _SHZ_SCREEN_4C
      if(ShBlkNorms_raw!=nullptr) memManager_.free(ShBlkNorms_raw);
#endif

#ifdef _REPORT_INTEGRAL_TIMINGS
      size_t nIntSkipSSSS = std::accumulate(nSkipSSSS.begin(),nSkipSSSS.end(),0);
      std::cout << "Dirac-Coulomb-SSSS Screened " << nIntSkipSSSS << std::endl;
  
      auto durDirectSSSS = tock(topDirectSSSS);
      std::cout << "Dirac-Coulomb-SSSS AO Direct Contraction took " <<  durDirectSSSS << " s\n"; 
  
      std::cout << std::endl;
#endif
  
    } // if(SSSS)

    /*************************************/
    /*                                   */
    /* End of Dirac-Coulomb C(4)-SSSS    */
    /*                                   */
    /*************************************/









    /*******************/
    /*                 */
    /* Start of Gaunt  */
    /*                 */
    /*******************/

    if( matList[0].contType == TWOBODY_CONTRACTION_TYPE::GAUNT ) {

      enum Gaunt_2ND_DERIV_AC {
        AxCx,
        AxCy,
        AxCz,
        AyCx,
        AyCy,
        AyCz,
        AzCx,
        AzCy,
        AzCz
      };
  
      enum Gaunt_2ND_DERIV_BC {
        BxCx,
        BxCy,
        BxCz,
        ByCx,
        ByCy,
        ByCz,
        BzCx,
        BzCy,
        BzCz
      };
  
      auto NB4_4  = 4*NB4;  
      auto NB4_5  = 5*NB4;  
      auto NB4_6  = 6*NB4;  
      auto NB4_7  = 7*NB4;  
      auto NB4_8  = 8*NB4;  
      auto NB4_9  = 9*NB4;  
      auto NB4_10 =10*NB4;  
      auto NB4_11 =11*NB4;  
      auto NB4_12 =12*NB4;  
      auto NB4_13 =13*NB4;  
      auto NB4_14 =14*NB4;  
      auto NB4_15 =15*NB4;  
      auto NB4_16 =16*NB4;  
      auto NB4_17 =17*NB4;  
      auto NB4_18 =18*NB4;  
   

#ifdef _REPORT_INTEGRAL_TIMINGS
      auto topDirectGaunt = tick();
#endif

      nERI = 9;
      buffAll = memManager_.malloc<double>(nERI*buffN4*nThreads);
      cacheAll = memManager_.malloc<double>(cache_size*nThreads);


#ifdef _SHZ_SCREEN_4C
  
      double *SchwarzGaunt = memManager_.malloc<double>(nShell*nShell);
      memset(SchwarzGaunt,0,nShell*nShell*sizeof(double));
  
      #pragma omp parallel
      {
  
        double C1 = 1./(2*SpeedOfLight);
        size_t thread_id = GetThreadID();
        size_t n1,n2;
  
        int shls[4];
        double *buff = &buffAll[nERI*buffN4*thread_id];
        double *cache = cacheAll+cache_size*thread_id;
  
        // set up Schwarz screening
        for(size_t s1(0), bf1_s(0), s12(0); s1 < nShell; bf1_s+=n1, s1++) { 
  
          n1 = basisSet_.shells[s1].size(); // Size of Shell 1
  
        for(size_t s2(0), bf2_s(0); s2 < nShell; bf2_s+=n2, s2++, s12++) {
  
          n2 = basisSet_.shells[s2].size(); // Size of Shell 2
  
          // Round Robbin work distribution
          #ifdef _OPENMP
          if( s12 % nThreads != thread_id ) continue;
          #endif
  
          shls[0] = int(s1);
          shls[1] = int(s2);
          shls[2] = int(s1);
          shls[3] = int(s2);
  
          auto nQuad = n1*n2*n1*n2;
  
          if(int2e_ip1ip2_sph(buff, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, cache)==0) continue;

          for(size_t iERI(0); iERI < nERI*nQuad; iERI++) 
              SchwarzGaunt[s1+s2*nShell] = std::max(SchwarzGaunt[s1+s2*nShell], std::abs(buff[iERI]));
  
          SchwarzGaunt[s1+s2*nShell] = C1*std::sqrt(SchwarzGaunt[s1+s2*nShell]);
  
        }
        }
  
      };

  

      // Compute shell block norms (∞-norm) of matList.X
      int mMat = 16;
      double *ShBlkNorms_raw = memManager_.malloc<double>(mMat*nShell*nShell);
      std::vector<double*> ShBlkNorms;
  
      auto iOff = 0;
      ShellBlockNorm(basisSet_.shells,matList[XLSMS].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLSMX].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLSMY].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLSMZ].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);

      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSLMS].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSLMX].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSLMY].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSLMZ].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);

      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLLMS].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLLMX].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLLMY].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLLMZ].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
 
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSSMS].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSSMX].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSSMY].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSSMZ].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      // Get the max over all the matricies for the shell block ∞-norms
      for(auto k = 0; k < nShell*nShell; k++) {
  
        double mx = std::abs(ShBlkNorms[0][k]);
        for(auto iMat = 0; iMat < mMat; iMat++)
          mx = std::max(mx,std::abs(ShBlkNorms[iMat][k]));
        ShBlkNorms[0][k] = mx;
   
      }
#endif

      int nSave = 19;
      ERIBuffer = memManager_.malloc<double>(nSave*NB4*nThreads);
      memset(AXRaw,0,nThreads*nMat*nBasis*nBasis*sizeof(MatsT));
      std::vector<size_t> nSkipGaunt(nThreads,0);
  
      #pragma omp parallel
      {
  
        dcomplex iS = dcomplex(0.0, 1.0);
  
        size_t thread_id = GetThreadID();
  
        auto &AX_loc = AXthreads[thread_id];
  
        double *BC   = &ERIBuffer[thread_id*nSave*NB4];
  
        size_t n1,n2,n3,n4,m,n,k,l,mnkl,bf1,bf2,bf3,bf4;
        size_t s4_max;
  
        int shls[4];
        double *buff = &buffAll[nERI*buffN4*thread_id];
        double *cache = cacheAll+cache_size*thread_id;
  
        for(size_t s1(0), bf1_s(0), s1234(0); s1 < nShell; bf1_s+=n1, s1++) { 
  
          n1 = basisSet_.shells[s1].size(); // Size of Shell 1
  
        for(size_t s2(0), bf2_s(0); s2 < nShell; bf2_s+=n2, s2++) {
  
          n2 = basisSet_.shells[s2].size(); // Size of Shell 2
  
#ifdef _SHZ_SCREEN_4C
          double shMax12 = ShBlkNorms[0][s2 + s1*nShell];
#endif
  
        for(size_t s3(0), bf3_s(0); s3 <= s2 ; bf3_s+=n3, s3++) {
  
          n3 = basisSet_.shells[s3].size(); // Size of Shell 3
          s4_max = (s2 == s3) ? s1 : nShell-1; // Determine the unique max of Shell 4
  
#ifdef _SHZ_SCREEN_4C
          double shMax123 = std::max(ShBlkNorms[0][s1 + s3*nShell], ShBlkNorms[0][s2 + s3*nShell]);
          shMax123 = std::max(shMax123,shMax12);
#endif
   
  
        for(size_t s4(0), bf4_s(0); s4 <= s4_max; bf4_s+=n4, s4++, s1234++) {
  
          n4 = basisSet_.shells[s4].size(); // Size of Shell 4
  
          // Round Robbin work distribution
          #ifdef _OPENMP
          if( s1234 % nThreads != thread_id ) continue;
          #endif
  
#ifdef _SHZ_SCREEN_4C
  
          double shMax = std::max(ShBlkNorms[0][s1 + s4*nShell],
                                  std::max(ShBlkNorms[0][s2 + s4*nShell],
                                           ShBlkNorms[0][s3 + s4*nShell]));
  
          shMax = std::max(shMax,shMax123);

          if((shMax*SchwarzGaunt[s3+s4*nShell]*SchwarzGaunt[s2 + s1*nShell]) <
             eri.threshSchwarz()) { nSkipGaunt[thread_id]++; continue; }
#endif
  
  
          shls[0] = int(s2);
          shls[1] = int(s1);
          shls[2] = int(s3);
          shls[3] = int(s4);

          if(int2e_ip1ip2_sph(buff, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, cache)==0) continue;
 
          auto nQuad = n1*n2*n3*n4;

          for(l = 3*maxShellSize, mnkl = 0ul; l < 3*maxShellSize + n4; ++l) 
          for(k = 2*maxShellSize            ; k < 2*maxShellSize + n3; ++k) 
          for(m = 0                         ; m <                  n1; ++m)
          for(n =   maxShellSize            ; n <   maxShellSize + n2; ++n, ++mnkl) {

  
            /* Gaunt */
            // ∇A∙∇C(mn|kl)
            auto dAdotdC = buff[AxCx*nQuad+mnkl] + buff[AyCy*nQuad+mnkl] + buff[AzCz*nQuad+mnkl];
            // ∇Ax∇C(mn|kl)
            auto dAcrossdC_x =  buff[AyCz*nQuad+mnkl] - buff[AzCy*nQuad+mnkl];
            auto dAcrossdC_y = -buff[AxCz*nQuad+mnkl] + buff[AzCx*nQuad+mnkl];
            auto dAcrossdC_z =  buff[AxCy*nQuad+mnkl] - buff[AyCx*nQuad+mnkl];


            // Change the index so that we do ∇B∙∇C(ij|kl) using the ∇B∇C engine
            auto MNKL = m + n*NB + k*NB2 + l*NB3;
            auto LKNM = l + k*NB + n*NB2 + m*NB3;
  
            // ∇B∙∇C(ij|kl) followed by ∇Bx∇C(ij|kl) X, Y, and Z
            // (kl|mn)
            BC[      MNKL] = dAdotdC;    
            BC[  NB4+MNKL] = dAcrossdC_x;
            BC[NB4_2+MNKL] = dAcrossdC_y;
            BC[NB4_3+MNKL] = dAcrossdC_z;

            // (lk|nm)
            BC[      LKNM] = dAdotdC;
            BC[  NB4+LKNM] = -dAcrossdC_x;
            BC[NB4_2+LKNM] = -dAcrossdC_y;
            BC[NB4_3+LKNM] = -dAcrossdC_z;
  

            // ∇B_x∇C_y(mn|kl) + ∇B_y∇C_x(mn|kl)
            // (mn|kl)
            BC[NB4_4+MNKL] = buff[BxCy*nQuad+mnkl] + buff[ByCx*nQuad+mnkl];
            // (lk|nm)
            BC[NB4_4+LKNM] = buff[BxCy*nQuad+mnkl] + buff[ByCx*nQuad+mnkl];
  
  
            // ∇B_y∇C_x(mn|kl)
            // (mn|kl)
            BC[NB4_5+MNKL] = buff[ByCx*nQuad+mnkl];
            // (lk|nm)
            BC[NB4_5+LKNM] = buff[BxCy*nQuad+mnkl];
  
  
            // ∇B_x∇C_z(mn|kl) + ∇B_z∇C_x(mn|kl)
            // (mn|kl)
            BC[NB4_6+MNKL] = buff[BxCz*nQuad+mnkl] + buff[BzCx*nQuad+mnkl];
            // (lk|nm)
            BC[NB4_6+LKNM] = buff[BxCz*nQuad+mnkl] + buff[BzCx*nQuad+mnkl];
  
  
            // ∇B_z∇C_x(mn|kl)
            // (mn|kl)
            BC[NB4_7+MNKL] = buff[BzCx*nQuad+mnkl];
            // (lk|nm)
            BC[NB4_7+LKNM] = buff[BxCz*nQuad+mnkl];
  
  
            // ∇B_y∇C_z(mn|kl) + ∇B_z∇C_y(mn|kl)
            // (mn|kl)
            BC[NB4_8+MNKL] = buff[ByCz*nQuad+mnkl] + buff[BzCy*nQuad+mnkl];
            // (lk|nm)
            BC[NB4_8+LKNM] = buff[ByCz*nQuad+mnkl] + buff[BzCy*nQuad+mnkl];
  
  
            // ∇B_z∇C_y(mn|kl)
            // (mn|kl)
            BC[NB4_9+MNKL] = buff[BzCy*nQuad+mnkl];
            // (lk|nm)
            BC[NB4_9+LKNM] = buff[ByCz*nQuad+mnkl];
  
  
            // - ∇B_x∇C_x(mn|kl) - ∇B_y∇C_y(mn|kl) + ∇B_z∇C_z(mn|kl)
            // (mn|kl)
            BC[NB4_10+MNKL] = - buff[BxCx*nQuad+mnkl] - buff[ByCy*nQuad+mnkl] + buff[BzCz*nQuad+mnkl];
            // (lk|nm)
            BC[NB4_10+LKNM] = - buff[BxCx*nQuad+mnkl] - buff[ByCy*nQuad+mnkl] + buff[BzCz*nQuad+mnkl];
  
  
            // ∇B_x∇C_x(mn|kl) - ∇B_y∇C_y(mn|kl) - ∇B_z∇C_z(mn|kl)
            // (mn|kl)
            BC[NB4_11+MNKL] = buff[BxCx*nQuad+mnkl] - buff[ByCy*nQuad+mnkl] - buff[BzCz*nQuad+mnkl];
            // (lk|nm)
            BC[NB4_11+LKNM] = buff[BxCx*nQuad+mnkl] - buff[ByCy*nQuad+mnkl] - buff[BzCz*nQuad+mnkl];
  
  
            // - ∇B_x∇C_x(mn|kl) + ∇B_y∇C_y(mn|kl) - ∇B_z∇C_z(mn|kl)
            // (mn|kl)
            BC[NB4_12+MNKL] = - buff[BxCx*nQuad+mnkl] + buff[ByCy*nQuad+mnkl] - buff[BzCz*nQuad+mnkl];
            // (lk|nm)
            BC[NB4_12+LKNM] = - buff[BxCx*nQuad+mnkl] + buff[ByCy*nQuad+mnkl] - buff[BzCz*nQuad+mnkl];
  
  
            // ∇B_x∇C_x(mn|kl)
            // (mn|kl)
            BC[NB4_13+MNKL] = buff[BxCx*nQuad+mnkl];
            // (lk|nm)
            BC[NB4_13+LKNM] = buff[BxCx*nQuad+mnkl];
  
  
            // ∇B_x∇C_y(mn|kl)
            // (mn|kl)
            BC[NB4_14+MNKL] = buff[BxCy*nQuad+mnkl];
            // (lk|nm)
            BC[NB4_14+LKNM] = buff[ByCx*nQuad+mnkl];
  
  
            // ∇B_x∇C_z(mn|kl)
            // (mn|kl)
            BC[NB4_15+MNKL] = buff[BxCz*nQuad+mnkl];
            // (lk|nm)
            BC[NB4_15+LKNM] = buff[BzCx*nQuad+mnkl];
  

            // ∇B_y∇C_y(mn|kl)
            // (mn|kl)
            BC[NB4_16+MNKL] = buff[ByCy*nQuad+mnkl];
            // (lk|nm)
            BC[NB4_16+LKNM] = buff[ByCy*nQuad+mnkl];
  

            // ∇B_y∇C_z(mn|kl)
            // (mn|kl)
            BC[NB4_17+MNKL] = buff[ByCz*nQuad+mnkl];
            // (lk|nm)
            BC[NB4_17+LKNM] = buff[BzCy*nQuad+mnkl];
  
  
            // ∇B_z∇C_z(mn|kl)
            // (mn|kl)
            BC[NB4_18+MNKL] = buff[BzCz*nQuad+mnkl];
            // (lk|nm)
            BC[NB4_18+LKNM] = buff[BzCz*nQuad+mnkl];
  
          } // ∇C∙∇D integral preparation loop
  
#if   0
      std::cout << std::scientific << std::setprecision(16);
  
      for(m = 0ul,            bf1 = bf1_s; m <                  n1; ++m, bf1++) 
      for(n =   maxShellSize, bf2 = bf2_s; n <   maxShellSize + n2; ++n, bf2++) 
      for(k = 2*maxShellSize, bf3 = bf3_s; k < 2*maxShellSize + n3; ++k, bf3++) 
      for(l = 3*maxShellSize, bf4 = bf4_s; l < 3*maxShellSize + n4; ++l, bf4++) {
        auto MNKL = m + n*NB + k*NB2 + l*NB3;

    std::cout << "ERI04-07: ∇B∙∇C(ab|cd)  ∇Bx∇C(ab|cd)-X  ∇Bx∇C(ab|cd)-Y  ∇Bx∇C(ab|cd)-Z" << std::endl;
        std::cout << "(" << bf1 << "," << bf2 << "|" << bf3 << "," << bf4 << ")  ";
        std::cout << BC[MNKL];
        std::cout << "   ";
        std::cout << BC[MNKL+NB4];
        std::cout << "   ";
        std::cout << BC[MNKL+NB4_2];
        std::cout << "   ";
        std::cout << BC[MNKL+NB4_3] << std::endl;
      };

#endif
  
  
  
#ifdef _CONTRACTION_ // Contraction

          auto ADXLLMS  = AX_loc[XLLMS];
          auto ADXLLMX  = AX_loc[XLLMX];
          auto ADXLLMY  = AX_loc[XLLMY];
          auto ADXLLMZ  = AX_loc[XLLMZ];
          auto DXLLMS = matList[XLLMS].X;
          auto DXLLMX = matList[XLLMX].X;
          auto DXLLMY = matList[XLLMY].X;
          auto DXLLMZ = matList[XLLMZ].X;

          auto ADXSSMS  = AX_loc[XSSMS];
          auto ADXSSMX  = AX_loc[XSSMX];
          auto ADXSSMY  = AX_loc[XSSMY];
          auto ADXSSMZ  = AX_loc[XSSMZ];
          auto DXSSMS = matList[XSSMS].X;
          auto DXSSMX = matList[XSSMX].X;
          auto DXSSMY = matList[XSSMY].X;
          auto DXSSMZ = matList[XSSMZ].X;

          auto ADCLSMS  = AX_loc[CLSMS];
          auto ADCLSMX  = AX_loc[CLSMX];
          auto ADCLSMY  = AX_loc[CLSMY];
          auto ADCLSMZ  = AX_loc[CLSMZ];
          auto ADXLSMS  = AX_loc[XLSMS];
          auto ADXLSMX  = AX_loc[XLSMX];
          auto ADXLSMY  = AX_loc[XLSMY];
          auto ADXLSMZ  = AX_loc[XLSMZ];

          auto DXLSMS = matList[XLSMS].X;
          auto DXLSMX = matList[XLSMX].X;
          auto DXLSMY = matList[XLSMY].X;
          auto DXLSMZ = matList[XLSMZ].X;
          auto DXSLMS = matList[XSLMS].X;
          auto DXSLMX = matList[XSLMX].X;
          auto DXSLMY = matList[XSLMY].X;
          auto DXSLMZ = matList[XSLMZ].X;

          auto DCLSMS = matList[CLSMS].X;
          auto DCLSMX = matList[CLSMX].X;
          auto DCLSMY = matList[CLSMY].X;
          auto DCLSMZ = matList[CLSMZ].X;
          auto DCSLMS = matList[CSLMS].X;
          auto DCSLMX = matList[CSLMX].X;
          auto DCSLMY = matList[CSLMY].X;
          auto DCSLMZ = matList[CSLMZ].X;
 
          for(m = 0ul,            bf1 = bf1_s; m <                  n1; ++m, bf1++) {
          for(n =   maxShellSize, bf2 = bf2_s; n <   maxShellSize + n2; ++n, bf2++) {
            auto bf1nB = bf1*nBasis;
            auto bf2nB = bf2*nBasis;

          for(k = 2*maxShellSize, bf3 = bf3_s; k < 2*maxShellSize + n3; ++k, bf3++) {
            auto bf3nB = bf3*nBasis;

          for(l = 3*maxShellSize, bf4 = bf4_s; l < 3*maxShellSize + n4; ++l, bf4++) {
            auto MNKL = m + n*NB + k*NB2 + l*NB3;
            auto LKNM = l + k*NB + n*NB2 + m*NB3;

            auto bf4nB= bf4*nBasis;
            auto bf14 = bf1 + bf4nB;
            auto bf24 = bf2 + bf4nB;
            auto bf23 = bf2 + bf3nB;
            auto bf13 = bf1 + bf3nB;
            auto bf41 = bf4 + bf1nB;
            auto bf31 = bf3 + bf1nB;
            auto bf32 = bf3 + bf2nB;
            auto bf42 = bf4 + bf2nB;
            auto bf34 = bf3 + bf4nB;
            auto bf43 = bf4 + bf3nB;
            auto bf12 = bf1 + bf2nB;
            auto bf21 = bf2 + bf1nB;

            //ERI0 :   ∇B∙∇C(mn|kl)
            //ERI1 :   ∇Bx∇C(mn|kl)-X
            //ERI2 :   ∇Bx∇C(mn|kl)-Y
            //ERI3 :   ∇Bx∇C(mn|kl)-Z
            //ERI4 :   ∇B_x∇C_y(mn|kl) + ∇B_y∇C_x(mn|kl)
            //ERI5 :   ∇B_y∇C_x(mn|kl)
            //ERI6 :   ∇B_x∇C_z(mn|kl) + ∇B_z∇C_x(mn|kl)
            //ERI7 :   ∇B_z∇C_x(mn|kl)
            //ERI8 :   ∇B_y∇C_z(mn|kl) + ∇B_z∇C_y(mn|kl)
            //ERI9 :   ∇B_z∇C_y(mn|kl)
            //ERI10: - ∇B_x∇C_x(mn|kl) - ∇B_y∇C_y(mn|kl) + ∇B_z∇C_z(mn|kl)
            //ERI11:   ∇B_x∇C_x(mn|kl) - ∇B_y∇C_y(mn|kl) - ∇B_z∇C_z(mn|kl)
            //ERI12: - ∇B_x∇C_x(mn|kl) + ∇B_y∇C_y(mn|kl) - ∇B_z∇C_z(mn|kl)
            //ERI13:   ∇B_x∇C_x(mn|kl)
            //ERI14:   ∇B_x∇C_y(mn|kl)
            //ERI15:   ∇B_x∇C_z(mn|kl)
            //ERI16:   ∇B_y∇C_y(mn|kl)
            //ERI17:   ∇B_y∇C_z(mn|kl)
            //ERI18:   ∇B_z∇C_z(mn|kl)
            
            
            
            /*++++++++++++++++++++++++*/
            /* Start of Gaunt (LL|LL) */
            /*++++++++++++++++++++++++*/
            
            // Exchange
            // MNKL
            ADXLLMS[bf14] += 3.0*( DXSSMS[bf23]*BC[MNKL       ] -iS*(DXSSMX[bf23]*BC[MNKL+NB4  ] +DXSSMY[bf23]*BC[MNKL+NB4_2 ] +DXSSMZ[bf23]*BC[MNKL+NB4_3 ]));
            ADXLLMZ[bf14] +=      -DXSSMZ[bf23]*BC[MNKL+NB4_10] -iS*DXSSMS[bf23]*BC[MNKL+NB4_3 ] -DXSSMX[bf23]*BC[MNKL+NB4_6 ] -DXSSMY[bf23]*BC[MNKL+NB4_8 ];
            ADXLLMX[bf14] +=      -DXSSMX[bf23]*BC[MNKL+NB4_11] -iS*DXSSMS[bf23]*BC[MNKL+NB4   ] -DXSSMY[bf23]*BC[MNKL+NB4_4 ] -DXSSMZ[bf23]*BC[MNKL+NB4_6 ];
            ADXLLMY[bf14] +=      -DXSSMY[bf23]*BC[MNKL+NB4_12] -iS*DXSSMS[bf23]*BC[MNKL+NB4_2 ] -DXSSMX[bf23]*BC[MNKL+NB4_4 ] -DXSSMZ[bf23]*BC[MNKL+NB4_8 ];
            
            // LKNM
            if(bf1_s!=bf4_s or bf2_s!=bf3_s) {
              ADXLLMS[bf41] += 3.0*( DXSSMS[bf32]*BC[LKNM       ] -iS*(DXSSMX[bf32]*BC[LKNM+NB4  ] +DXSSMY[bf32]*BC[LKNM+NB4_2 ] +DXSSMZ[bf32]*BC[LKNM+NB4_3 ]));
              ADXLLMZ[bf41] +=      -DXSSMZ[bf32]*BC[LKNM+NB4_10] -iS*DXSSMS[bf32]*BC[LKNM+NB4_3 ] -DXSSMX[bf32]*BC[LKNM+NB4_6 ] -DXSSMY[bf32]*BC[LKNM+NB4_8 ];
              ADXLLMX[bf41] +=      -DXSSMX[bf32]*BC[LKNM+NB4_11] -iS*DXSSMS[bf32]*BC[LKNM+NB4   ] -DXSSMY[bf32]*BC[LKNM+NB4_4 ] -DXSSMZ[bf32]*BC[LKNM+NB4_6 ];
              ADXLLMY[bf41] +=      -DXSSMY[bf32]*BC[LKNM+NB4_12] -iS*DXSSMS[bf32]*BC[LKNM+NB4_2 ] -DXSSMX[bf32]*BC[LKNM+NB4_4 ] -DXSSMZ[bf32]*BC[LKNM+NB4_8 ];
            }
            
            /*++++++++++++++++++++++++*/
            /* Start of Gaunt (SS|SS) */
            /*++++++++++++++++++++++++*/
            
            // MNKL
            ADXSSMS[bf32]+= 3.0*DXLLMS[bf41]*BC[MNKL       ]    -iS*(DXLLMX[bf41]*BC[MNKL+NB4   ] +DXLLMY[bf41]*BC[MNKL+NB4_2 ] +DXLLMZ[bf41]*BC[MNKL+NB4_3 ]);
            ADXSSMZ[bf32]+=    -DXLLMZ[bf41]*BC[MNKL+NB4_10] -3.0*iS*DXLLMS[bf41]*BC[MNKL+NB4_3 ] -DXLLMX[bf41]*BC[MNKL+NB4_6 ] -DXLLMY[bf41]*BC[MNKL+NB4_8 ];
            ADXSSMX[bf32]+=    -DXLLMX[bf41]*BC[MNKL+NB4_11] -3.0*iS*DXLLMS[bf41]*BC[MNKL+NB4   ] -DXLLMZ[bf41]*BC[MNKL+NB4_6 ] -DXLLMY[bf41]*BC[MNKL+NB4_4 ];
            ADXSSMY[bf32]+=    -DXLLMY[bf41]*BC[MNKL+NB4_12] -3.0*iS*DXLLMS[bf41]*BC[MNKL+NB4_2 ] -DXLLMX[bf41]*BC[MNKL+NB4_4 ] -DXLLMZ[bf41]*BC[MNKL+NB4_8 ];
            
            // LKNM
            if(bf1_s!=bf4_s or bf2_s!=bf3_s) {
              ADXSSMS[bf23]+= 3.0*DXLLMS[bf14]*BC[LKNM       ]    -iS*(DXLLMX[bf14]*BC[LKNM+NB4   ] +DXLLMY[bf14]*BC[LKNM+NB4_2 ] +DXLLMZ[bf14]*BC[LKNM+NB4_3 ]);
              ADXSSMZ[bf23]+=    -DXLLMZ[bf14]*BC[LKNM+NB4_10] -3.0*iS*DXLLMS[bf14]*BC[LKNM+NB4_3 ] -DXLLMX[bf14]*BC[LKNM+NB4_6 ] -DXLLMY[bf14]*BC[LKNM+NB4_8 ];
              ADXSSMX[bf23]+=    -DXLLMX[bf14]*BC[LKNM+NB4_11] -3.0*iS*DXLLMS[bf14]*BC[LKNM+NB4   ] -DXLLMZ[bf14]*BC[LKNM+NB4_6 ] -DXLLMY[bf14]*BC[LKNM+NB4_4 ];
              ADXSSMY[bf23]+=    -DXLLMY[bf14]*BC[LKNM+NB4_12] -3.0*iS*DXLLMS[bf14]*BC[LKNM+NB4_2 ] -DXLLMX[bf14]*BC[LKNM+NB4_4 ] -DXLLMZ[bf14]*BC[LKNM+NB4_8 ];
            }
            
            
            /*++++++++++++++++++++++++*/
            /* Start of Gaunt (LL|SS) */
            /*++++++++++++++++++++++++*/
            
            // MNKL
            // Coulomb //TRANSKL
            ADCLSMS[bf12]+=   -DCLSMS[bf43]*BC[MNKL       ] +iS*(DCLSMX[bf43]*BC[MNKL+NB4  ] +DCLSMY[bf43]*BC[MNKL+NB4_2 ] +DCLSMZ[bf43]*BC[MNKL+NB4_3 ])
                              +DCSLMS[bf34]*BC[MNKL       ] +iS*(DCSLMX[bf34]*BC[MNKL+NB4  ] +DCSLMY[bf34]*BC[MNKL+NB4_2 ] +DCSLMZ[bf34]*BC[MNKL+NB4_3 ]);
            
            ADCLSMZ[bf12]+= iS*DCLSMS[bf43]*BC[MNKL+NB4_3 ] -DCLSMZ[bf43]*BC[MNKL       ] +DCLSMX[bf43]*BC[MNKL+NB4_15] +DCLSMY[bf43]*BC[MNKL+NB4_17] +DCLSMZ[bf43]*BC[MNKL+NB4_18]
                           -iS*DCSLMS[bf34]*BC[MNKL+NB4_3 ] -DCSLMZ[bf34]*BC[MNKL       ] +DCSLMX[bf34]*BC[MNKL+NB4_15] +DCSLMY[bf34]*BC[MNKL+NB4_17] +DCSLMZ[bf34]*BC[MNKL+NB4_18];
            
            ADCLSMX[bf12]+= iS*DCLSMS[bf43]*BC[MNKL+NB4   ] -DCLSMX[bf43]*BC[MNKL       ] +DCLSMX[bf43]*BC[MNKL+NB4_13] +DCLSMY[bf43]*BC[MNKL+NB4_5 ] +DCLSMZ[bf43]*BC[MNKL+NB4_7 ]
                           -iS*DCSLMS[bf34]*BC[MNKL+NB4   ] -DCSLMX[bf34]*BC[MNKL       ] +DCSLMX[bf34]*BC[MNKL+NB4_13] +DCSLMY[bf34]*BC[MNKL+NB4_5 ] +DCSLMZ[bf34]*BC[MNKL+NB4_7 ];
            
            ADCLSMY[bf12]+= iS*DCLSMS[bf43]*BC[MNKL+NB4_2 ] -DCLSMY[bf43]*BC[MNKL       ] +DCLSMX[bf43]*BC[MNKL+NB4_14] +DCLSMY[bf43]*BC[MNKL+NB4_16] +DCLSMZ[bf43]*BC[MNKL+NB4_9 ]
                           -iS*DCSLMS[bf34]*BC[MNKL+NB4_2 ] -DCSLMY[bf34]*BC[MNKL       ] +DCSLMX[bf34]*BC[MNKL+NB4_14] +DCSLMY[bf34]*BC[MNKL+NB4_16] +DCSLMZ[bf34]*BC[MNKL+NB4_9 ];
            
            
            
            // LKNM
            // Coulomb //TRANSKL
            if(bf1_s!=bf4_s or bf2_s!=bf3_s) {
              ADCLSMS[bf43]+= -DCLSMS[bf12]*BC[LKNM      ] +iS*(DCLSMX[bf12]*BC[LKNM+NB4  ] +DCLSMY[bf12]*BC[LKNM+NB4_2 ] +DCLSMZ[bf12]*BC[LKNM+NB4_3 ])
                              +DCSLMS[bf21]*BC[LKNM      ] +iS*(DCSLMX[bf21]*BC[LKNM+NB4  ] +DCSLMY[bf21]*BC[LKNM+NB4_2 ] +DCSLMZ[bf21]*BC[LKNM+NB4_3 ]);
            
              ADCLSMZ[bf43]+= iS*DCLSMS[bf12]*BC[LKNM+NB4_3 ] -DCLSMZ[bf12]*BC[LKNM       ] +DCLSMX[bf12]*BC[LKNM+NB4_15] +DCLSMY[bf12]*BC[LKNM+NB4_17] +DCLSMZ[bf12]*BC[LKNM+NB4_18]
                             -iS*DCSLMS[bf21]*BC[LKNM+NB4_3 ] -DCSLMZ[bf21]*BC[LKNM       ] +DCSLMX[bf21]*BC[LKNM+NB4_15] +DCSLMY[bf21]*BC[LKNM+NB4_17] +DCSLMZ[bf21]*BC[LKNM+NB4_18];
            
              ADCLSMX[bf43]+= iS*DCLSMS[bf12]*BC[LKNM+NB4   ] -DCLSMX[bf12]*BC[LKNM       ] +DCLSMX[bf12]*BC[LKNM+NB4_13] +DCLSMY[bf12]*BC[LKNM+NB4_5 ] +DCLSMZ[bf12]*BC[LKNM+NB4_7 ]
                             -iS*DCSLMS[bf21]*BC[LKNM+NB4   ] -DCSLMX[bf21]*BC[LKNM       ] +DCSLMX[bf21]*BC[LKNM+NB4_13] +DCSLMY[bf21]*BC[LKNM+NB4_5 ] +DCSLMZ[bf21]*BC[LKNM+NB4_7 ];
            
              ADCLSMY[bf43]+= iS*DCLSMS[bf12]*BC[LKNM+NB4_2 ] -DCLSMY[bf12]*BC[LKNM       ] +DCLSMX[bf12]*BC[LKNM+NB4_14] +DCLSMY[bf12]*BC[LKNM+NB4_16] +DCLSMZ[bf12]*BC[LKNM+NB4_9 ]
                             -iS*DCSLMS[bf21]*BC[LKNM+NB4_2 ] -DCSLMY[bf21]*BC[LKNM       ] +DCSLMX[bf21]*BC[LKNM+NB4_14] +DCSLMY[bf21]*BC[LKNM+NB4_16] +DCSLMZ[bf21]*BC[LKNM+NB4_9 ];
            }
            
            
            //Exchange //TRANSKL
            ADXLSMS[bf13]+= DXSLMS[bf24]*BC[MNKL      ] -iS*(DXSLMX[bf24]*BC[MNKL+NB4   ] +DXSLMY[bf24]*BC[MNKL+NB4_2 ] +DXSLMZ[bf24]*BC[MNKL+NB4_3 ]);
             
            ADXLSMZ[bf13]+=-2.0*( DXSLMZ[bf24]*BC[MNKL       ] +DXSLMY[bf24]*BC[MNKL+NB4   ] -DXSLMX[bf24]*BC[MNKL+NB4_2 ]) +iS*DXSLMS[bf24]*BC[MNKL+NB4_3 ]
                                 -DXSLMZ[bf24]*BC[MNKL+NB4_10] -DXSLMX[bf24]*BC[MNKL+NB4_6 ] -DXSLMY[bf24]*BC[MNKL+NB4_8 ];
            
            ADXLSMX[bf13]+=-2.0*( DXSLMX[bf24]*BC[MNKL       ] -DXSLMY[bf24]*BC[MNKL+NB4_3 ] +DXSLMZ[bf24]*BC[MNKL+NB4_2 ]) +iS*DXSLMS[bf24]*BC[MNKL+NB4   ]
                                 -DXSLMX[bf24]*BC[MNKL+NB4_11] -DXSLMY[bf24]*BC[MNKL+NB4_4 ] -DXSLMZ[bf24]*BC[MNKL+NB4_6 ];
            
            ADXLSMY[bf13]+=-2.0*( DXSLMY[bf24]*BC[MNKL       ] +DXSLMX[bf24]*BC[MNKL+NB4_3 ] -DXSLMZ[bf24]*BC[MNKL+NB4   ]) +iS*DXSLMS[bf24]*BC[MNKL+NB4_2 ]
                                 -DXSLMY[bf24]*BC[MNKL+NB4_12] -DXSLMX[bf24]*BC[MNKL+NB4_4 ] -DXSLMZ[bf24]*BC[MNKL+NB4_8 ];
            
            
            //Exchange //TRANSKL
            if(bf1_s!=bf4_s or bf2_s!=bf3_s) {
              ADXLSMS[bf42]+= DXSLMS[bf31]*BC[LKNM       ] -iS*(DXSLMX[bf31]*BC[LKNM+NB4   ] +DXSLMY[bf31]*BC[LKNM+NB4_2 ] +DXSLMZ[bf31]*BC[LKNM+NB4_3 ]);
              
              ADXLSMZ[bf42]+=-2.0*( DXSLMZ[bf31]*BC[LKNM      ] +DXSLMY[bf31]*BC[LKNM+NB4   ] -DXSLMX[bf31]*BC[LKNM+NB4_2 ]) +iS*DXSLMS[bf31]*BC[LKNM+NB4_3 ]
                                  -DXSLMZ[bf31]*BC[LKNM+NB4_10] -DXSLMX[bf31]*BC[LKNM+NB4_6 ] -DXSLMY[bf31]*BC[LKNM+NB4_8 ];
            
              ADXLSMX[bf42]+=-2.0*( DXSLMX[bf31]*BC[LKNM       ] -DXSLMY[bf31]*BC[LKNM+NB4_3 ] +DXSLMZ[bf31]*BC[LKNM+NB4_2 ]) +iS*DXSLMS[bf31]*BC[LKNM+NB4  ]
                                   -DXSLMX[bf31]*BC[LKNM+NB4_11] -DXSLMY[bf31]*BC[LKNM+NB4_4 ] -DXSLMZ[bf31]*BC[LKNM+NB4_6 ];
            
              ADXLSMY[bf42]+=-2.0*( DXSLMY[bf31]*BC[LKNM       ] +DXSLMX[bf31]*BC[LKNM+NB4_3 ] -DXSLMZ[bf31]*BC[LKNM+NB4   ]) +iS*DXSLMS[bf31]*BC[LKNM+NB4_2 ]
                                   -DXSLMY[bf31]*BC[LKNM+NB4_12] -DXSLMX[bf31]*BC[LKNM+NB4_4 ] -DXSLMZ[bf31]*BC[LKNM+NB4_8 ];
            }

          }
          }
          }
          } // contraction loop
  
#endif // Contraction
  
  
        }; // loop s4
        }; // loop s3
        }; // loop s2
        }; // loop s1
  
  
      } // OpenMP context
  
  
      for( auto iTh  = 0; iTh < nThreads; iTh++) {

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLLMS],nBasis,MatsT(1.0),
           matList[XLLMS].AX,nBasis,matList[XLLMS].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLLMX],nBasis,MatsT(1.0),
           matList[XLLMX].AX,nBasis,matList[XLLMX].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLLMY],nBasis,MatsT(1.0),
           matList[XLLMY].AX,nBasis,matList[XLLMY].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLLMZ],nBasis,MatsT(1.0),
           matList[XLLMZ].AX,nBasis,matList[XLLMZ].AX,nBasis);



        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XSSMS],nBasis,MatsT(1.0),
           matList[XSSMS].AX,nBasis,matList[XSSMS].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XSSMX],nBasis,MatsT(1.0),
           matList[XSSMX].AX,nBasis,matList[XSSMX].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XSSMY],nBasis,MatsT(1.0),
           matList[XSSMY].AX,nBasis,matList[XSSMY].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XSSMZ],nBasis,MatsT(1.0),
           matList[XSSMZ].AX,nBasis,matList[XSSMZ].AX,nBasis);



        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][CLSMS],nBasis,MatsT(1.0),
           matList[CLSMS].AX,nBasis,matList[CLSMS].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][CLSMX],nBasis,MatsT(1.0),
           matList[CLSMX].AX,nBasis,matList[CLSMX].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][CLSMY],nBasis,MatsT(1.0),
           matList[CLSMY].AX,nBasis,matList[CLSMY].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][CLSMZ],nBasis,MatsT(1.0),
           matList[CLSMZ].AX,nBasis,matList[CLSMZ].AX,nBasis);
 

  
        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLSMS],nBasis,MatsT(1.0),
           matList[XLSMS].AX,nBasis,matList[XLSMS].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLSMX],nBasis,MatsT(1.0),
           matList[XLSMX].AX,nBasis,matList[XLSMX].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLSMY],nBasis,MatsT(1.0),
           matList[XLSMY].AX,nBasis,matList[XLSMY].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLSMZ],nBasis,MatsT(1.0),
           matList[XLSMZ].AX,nBasis,matList[XLSMZ].AX,nBasis);
  
      };
   
      memManager_.free(ERIBuffer);
      memManager_.free(buffAll, cacheAll);
#ifdef _SHZ_SCREEN_4C
      if(ShBlkNorms_raw!=nullptr) memManager_.free(ShBlkNorms_raw);
      if(SchwarzGaunt!=nullptr) memManager_.free(SchwarzGaunt);
#endif
  
#ifdef _REPORT_INTEGRAL_TIMINGS
      size_t nIntSkipGaunt = std::accumulate(nSkipGaunt.begin(),nSkipGaunt.end(),0);
      std::cout << "Gaunt Screened " << nIntSkipGaunt << std::endl;
  
      auto durDirectGaunt = tock(topDirectGaunt);
      std::cout << "Gaunt AO Direct Contraction took " <<  durDirectGaunt << " s\n"; 
  
      std::cout << std::endl;
#endif

    } // if(Gaunt)

    /*******************/
    /*                 */
    /*   End of Gaunt  */
    /*                 */
    /*******************/





    /*******************/
    /*                 */
    /* Start of Gauge  */
    /*                 */
    /*******************/

    if( matList[0].contType == TWOBODY_CONTRACTION_TYPE::GAUGE ) {

      enum Gauge_ERI {
        SxSx, // σ_x * σ_x     0
        SySx, // σ_y * σ_x     1
        SzSx, // σ_z * σ_x     2
        ISx,  // I   * σ_x     3
        SxSy, // σ_x * σ_y     4
        SySy, // σ_y * σ_y     5
        SzSy, // σ_z * σ_y     6
        ISy,  // I   * σ_y     7
        SxSz, // σ_x * σ_z     8
        SySz, // σ_y * σ_z     9
        SzSz, // σ_z * σ_z     10
        ISz,  // I   * σ_z     11
        SxI,  // σ_x * I       12
        SyI,  // σ_y * I       13
        SzI,  // σ_z * I       14
        II    // I   * I       15
      };
  
      auto NB4_4  = 4*NB4;  
      auto NB4_5  = 5*NB4;  
      auto NB4_6  = 6*NB4;  
      auto NB4_7  = 7*NB4;  
      auto NB4_8  = 8*NB4;  
      auto NB4_9  = 9*NB4;  
      auto NB4_10 =10*NB4;  
      auto NB4_11 =11*NB4;  
      auto NB4_12 =12*NB4;  
      auto NB4_13 =13*NB4;  
      auto NB4_14 =14*NB4;  
      auto NB4_15 =15*NB4;  
      auto NB4_16 =16*NB4;  
      auto NB4_17 =17*NB4;  
      auto NB4_18 =18*NB4;  
      auto NB4_19 =19*NB4;  
      auto NB4_20 =20*NB4;  
      auto NB4_21 =21*NB4;  
      auto NB4_22 =22*NB4;  
      auto NB4_23 =23*NB4;  
      auto NB4_24 =24*NB4;  
      auto NB4_25 =25*NB4;  

      auto ss = 0     ;        // ERI00 (ss)(ij|kl)
      auto sx = NB4   ;        // ERI01 (sσ)_x(ijkl)
      auto sy = NB4_2 ;        // ERI02 (sσ)_y(ijkl)
      auto sz = NB4_3 ;        // ERI03 (sσ)_z(ijkl)
      auto xs = NB4_4 ;        // ERI04 (σs)_x(ijkl)
      auto ys = NB4_5 ;        // ERI05 (σs)_y(ijkl)
      auto zs = NB4_6 ;        // ERI06 (σs)_z(ijkl)
      auto dot = NB4_7 ;       // ERI07 (σ∙σ)(ijkl)
      auto crossx = NB4_8 ;    // ERI08 (σxσ)_x(ijkl)
      auto crossy = NB4_9 ;    // ERI09 (σxσ)_y(ijkl)
      auto crossz = NB4_10;    // ERI10 (σxσ)_z(ijkl)
      auto pxxmyymzz = NB4_11; // ERI11 (σσ)(xx - yy - zz)(ijkl)
      auto mxxpyymzz = NB4_12; // ERI12 (σσ)(-xx + yy - zz)(ijkl)
      auto mxxmyypzz = NB4_13; // ERI13 (σσ)(-xx - yy + zz)(ijkl)
      auto xypyx = NB4_14;     // ERI14 (σ_x σ_y + σ_y σ_x)(ijkl)
      auto zxpxz = NB4_15;     // ERI15 (σ_z σ_x + σ_x σ_z)(ijkl)
      auto yzpzy = NB4_16;     // ERI16 (σ_y σ_z + σ_z σ_y)(ijkl)
      auto xx = NB4_17;        // ERI17 (σ_x σ_x)(ijkl)
      auto xy = NB4_18;        // ERI18 (σ_x σ_y)(ijkl)
      auto xz = NB4_19;        // ERI19 (σ_x σ_z)(ijkl)
      auto yx = NB4_20;        // ERI20 (σ_y σ_x)(ijkl)
      auto yy = NB4_21;        // ERI21 (σ_y σ_y)(ijkl)
      auto yz = NB4_22;        // ERI22 (σ_y σ_z)(ijkl)
      auto zx = NB4_23;        // ERI23 (σ_z σ_x)(ijkl)
      auto zy = NB4_24;        // ERI24 (σ_z σ_y)(ijkl)
      auto zz = NB4_25;        // ERI25 (σ_z σ_z)(ijkl)
   

#ifdef _REPORT_INTEGRAL_TIMINGS
      auto topDirectGauge = tick();
#endif

      nERI = 16;
      buffAll = memManager_.malloc<double>(2*nERI*buffN4*nThreads);
      cacheAll = memManager_.malloc<double>(cache_size*nThreads);


#ifdef _SHZ_SCREEN_4C
  
      double *SchwarzGauge = memManager_.malloc<double>(nShell*nShell);
      memset(SchwarzGauge,0,nShell*nShell*sizeof(double));
  
      #pragma omp parallel
      {
  
        double C1 = 1./(2*SpeedOfLight);
        size_t thread_id = GetThreadID();
        size_t n1,n2;
        int skiperi1,skiperi2;
  
        int shls[4];
        double *buffr1 = buffAll+nERI*buffN4*thread_id;
        double *buffr2 = buffAll+nERI*nThreads*buffN4+nERI*buffN4*thread_id;
        double *cache = cacheAll+cache_size*thread_id;
  
        // set up Schwarz screening
        for(size_t s1(0), bf1_s(0), s12(0); s1 < nShell; bf1_s+=n1, s1++) { 
  
          n1 = basisSet_.shells[s1].size(); // Size of Shell 1
  
        for(size_t s2(0), bf2_s(0); s2 < nShell; bf2_s+=n2, s2++, s12++) {
  
          n2 = basisSet_.shells[s2].size(); // Size of Shell 2
  
          // Round Robbin work distribution
          #ifdef _OPENMP
          if( s12 % nThreads != thread_id ) continue;
          #endif
  
          shls[0] = int(s1);
          shls[1] = int(s2);
          shls[2] = int(s1);
          shls[3] = int(s2);
  
          auto nQuad = n1*n2*n1*n2;

          //∇A∇C
          //int2e_gauge_r1_sps1sps2_sph(buffr1, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, cache);
          //int2e_gauge_r2_sps1sps2_sph(buffr2, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, cache);
	  
          //∇B∇D
          skiperi1 = int2e_gauge_r1_ssp1ssp2_sph(buffr1, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, cache);
          skiperi2 = int2e_gauge_r2_ssp1ssp2_sph(buffr2, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, cache);

          if(skiperi1==0 and skiperi2==0) continue;

          for(size_t iERI(0); iERI < nERI*nQuad; iERI++) 
              SchwarzGauge[s1+s2*nShell] = std::max(SchwarzGauge[s1+s2*nShell], std::abs(buffr1[iERI]-buffr2[iERI]));
  
          SchwarzGauge[s1+s2*nShell] = C1*std::sqrt(SchwarzGauge[s1+s2*nShell]);
  
        }
        }
  
      };

  

      // Compute shell block norms (∞-norm) of matList.X
      int mMat = 16;
      double *ShBlkNorms_raw = memManager_.malloc<double>(mMat*nShell*nShell);
      std::vector<double*> ShBlkNorms;
  
      auto iOff = 0;
      ShellBlockNorm(basisSet_.shells,matList[XLSMS].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLSMX].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLSMY].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLSMZ].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);

      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSLMS].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSLMX].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSLMY].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSLMZ].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);

      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLLMS].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLLMX].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLLMY].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XLLMZ].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
 
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSSMS].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSSMX].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSSMY].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      iOff += nShell*nShell;
      ShellBlockNorm(basisSet_.shells,matList[XSSMZ].X,nBasis,ShBlkNorms_raw + iOff);
      ShBlkNorms.emplace_back(ShBlkNorms_raw + iOff);
  
      // Get the max over all the matricies for the shell block ∞-norms
      for(auto k = 0; k < nShell*nShell; k++) {
  
        double mx = std::abs(ShBlkNorms[0][k]);
        for(auto iMat = 0; iMat < mMat; iMat++)
          mx = std::max(mx,std::abs(ShBlkNorms[iMat][k]));
        ShBlkNorms[0][k] = mx;
   
      }

#endif

      int nSave = 26;
      ERIBuffer = memManager_.malloc<double>(nSave*NB4*nThreads);
      memset(AXRaw,0,nThreads*nMat*nBasis*nBasis*sizeof(MatsT));
      std::vector<size_t> nSkipGauge(nThreads,0);
  
      #pragma omp parallel
      {
  
        dcomplex iS = dcomplex(0.0, 1.0);
  
        size_t thread_id = GetThreadID();
  
        auto &AX_loc = AXthreads[thread_id];
  
        double *BC   = &ERIBuffer[thread_id*nSave*NB4];
  
        size_t n1,n2,n3,n4,m,n,k,l,mnkl,bf1,bf2,bf3,bf4;
        size_t s4_max;

        int skiperi1,skiperi2;
  
        int shls[4];
        double *buffr1 = buffAll+nERI*buffN4*thread_id;
        double *buffr2 = buffAll+nERI*nThreads*buffN4+nERI*buffN4*thread_id;
        double *cache = cacheAll+cache_size*thread_id;
  
        for(size_t s1(0), bf1_s(0), s1234(0); s1 < nShell; bf1_s+=n1, s1++) { 
  
          n1 = basisSet_.shells[s1].size(); // Size of Shell 1
  
        for(size_t s2(0), bf2_s(0); s2 < nShell; bf2_s+=n2, s2++) {
  
          n2 = basisSet_.shells[s2].size(); // Size of Shell 2
  
#ifdef _SHZ_SCREEN_4C
          double shMax12 = ShBlkNorms[0][s2 + s1*nShell];
#endif
  
        for(size_t s3(0), bf3_s(0); s3 <= s2 ; bf3_s+=n3, s3++) {
        //for(size_t s3(0), bf3_s(0); s3 < nShell ; bf3_s+=n3, s3++) {
  
          n3 = basisSet_.shells[s3].size(); // Size of Shell 3
          s4_max = (s2 == s3) ? s1 : nShell-1; // Determine the unique max of Shell 4
  
#ifdef _SHZ_SCREEN_4C
          double shMax123 = std::max(ShBlkNorms[0][s1 + s3*nShell], ShBlkNorms[0][s2 + s3*nShell]);
          shMax123 = std::max(shMax123,shMax12);
#endif
   
  
        //for(size_t s4(0), bf4_s(0); s4 < nShell; bf4_s+=n4, s4++, s1234++) {
        for(size_t s4(0), bf4_s(0); s4 <= s4_max; bf4_s+=n4, s4++, s1234++) {
  
          n4 = basisSet_.shells[s4].size(); // Size of Shell 4
  
          // Round Robbin work distribution
          #ifdef _OPENMP
          if( s1234 % nThreads != thread_id ) continue;
          #endif
  
#ifdef _SHZ_SCREEN_4C
  
          double shMax = std::max(ShBlkNorms[0][s1 + s4*nShell],
                                  std::max(ShBlkNorms[0][s2 + s4*nShell],
                                           ShBlkNorms[0][s3 + s4*nShell]));
  
          shMax = std::max(shMax,shMax123);

          if((shMax*SchwarzGauge[s1+s2*nShell]*SchwarzGauge[s4+s3*nShell]) <
             eri.threshSchwarz()) { nSkipGauge[thread_id]++; continue; }
#endif
  
  
          shls[0] = int(s1);
          shls[1] = int(s2);
          shls[2] = int(s3);
          shls[3] = int(s4);

          //∇B∇C
          skiperi1 = int2e_gauge_r1_ssp1sps2_sph(buffr1, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, cache);
          skiperi2 = int2e_gauge_r2_ssp1sps2_sph(buffr2, nullptr, shls, atm, nAtoms, bas, nShells, env, nullptr, cache);

          if(skiperi1==0 and skiperi2==0) continue;

          auto nQuad = n1*n2*n3*n4;

	  mnkl = 0ul;
          for(l = 3*maxShellSize; l < 3*maxShellSize + n4; ++l) 
          for(k = 2*maxShellSize; k < 2*maxShellSize + n3; ++k) 
          for(n =   maxShellSize; n <   maxShellSize + n2; ++n)
          for(m = 0             ; m <                  n1; ++m) {

            auto MNKL = m + n*NB + k*NB2 + l*NB3;
            auto LKNM = l + k*NB + n*NB2 + m*NB3;

            //enum Gauge_ERI {
            //  SxSx,  // σ_x * σ_x     0
            //  SySx,  // σ_y * σ_x     1
            //  SzSx,  // σ_z * σ_x     2
            //  ISx,   // I   * σ_x     3
            //  SxSy,  // σ_x * σ_y     4
            //  SySy,  // σ_y * σ_y     5
            //  SzSy,  // σ_z * σ_y     6
            //  ISy,   // I   * σ_y     7
            //  SxSz,  // σ_x * σ_z     8
            //  SySz,  // σ_y * σ_z     9
            //  SzSz,  // σ_z * σ_z     10
            //  ISz,   // I   * σ_z     11
            //  SxI,   // σ_x * I       12
            //  SyI,   // σ_y * I       13
            //  SzI,   // σ_z * I       14
            //  II     // I   * I       15
            //};
            
            // (ss)
            BC[MNKL] = buffr1[15*nQuad+mnkl] - buffr2[15*nQuad+mnkl];
            BC[LKNM] = buffr1[15*nQuad+mnkl] - buffr2[15*nQuad+mnkl];
            
            // (sσ)_x
            BC[NB4+MNKL] = buffr1[3*nQuad+mnkl] - buffr2[3*nQuad+mnkl];
            BC[NB4+LKNM] = buffr2[12*nQuad+mnkl] - buffr1[12*nQuad+mnkl];
            
            // (sσ)_y
            BC[NB4_2+MNKL] = buffr1[7*nQuad+mnkl] - buffr2[7*nQuad+mnkl];
            BC[NB4_2+LKNM] = buffr2[13*nQuad+mnkl] - buffr1[13*nQuad+mnkl];
            
            // (sσ)_z
            BC[NB4_3+MNKL] = buffr1[11*nQuad+mnkl] - buffr2[11*nQuad+mnkl];
            BC[NB4_3+LKNM] = buffr2[14*nQuad+mnkl] - buffr1[14*nQuad+mnkl];
            
            // (σs)_x
            BC[NB4_4+MNKL] = buffr1[12*nQuad+mnkl] - buffr2[12*nQuad+mnkl];
            BC[NB4_4+LKNM] = buffr2[3*nQuad+mnkl] - buffr1[3*nQuad+mnkl];
            
            // (σs)_y
            BC[NB4_5+MNKL] = buffr1[13*nQuad+mnkl] - buffr2[13*nQuad+mnkl];
            BC[NB4_5+LKNM] = buffr2[7*nQuad+mnkl] - buffr1[7*nQuad+mnkl];
            
            // (σs)_z
            BC[NB4_6+MNKL] = buffr1[14*nQuad+mnkl] - buffr2[14*nQuad+mnkl];
            BC[NB4_6+LKNM] = buffr2[11*nQuad+mnkl] - buffr1[11*nQuad+mnkl];
            
            // (σ∙σ)
            BC[NB4_7+MNKL] =-(buffr1[mnkl] - buffr2[mnkl])
                            -(buffr1[5*nQuad+mnkl] - buffr2[5*nQuad+mnkl])
                            -(buffr1[10*nQuad+mnkl] - buffr2[10*nQuad+mnkl]);
            BC[NB4_7+LKNM] =-(buffr1[mnkl] - buffr2[mnkl])
                            -(buffr1[5*nQuad+mnkl] - buffr2[5*nQuad+mnkl])
                            -(buffr1[10*nQuad+mnkl] - buffr2[10*nQuad+mnkl]);
            
            // (σxσ)_x = σ_y*σ_z - σ_z*σ_y
            BC[NB4_8+MNKL] =-(buffr1[9*nQuad+mnkl] - buffr2[9*nQuad+mnkl])
                            +(buffr1[6*nQuad+mnkl] - buffr2[6*nQuad+mnkl]);
            BC[NB4_8+LKNM] = (buffr1[9*nQuad+mnkl] - buffr2[9*nQuad+mnkl])
                            -(buffr1[6*nQuad+mnkl] - buffr2[6*nQuad+mnkl]);
            
            // (σxσ)_y = σ_z*σ_x - σ_x*σ_z
            BC[NB4_9+MNKL] =-(buffr1[2*nQuad+mnkl] - buffr2[2*nQuad+mnkl])
                            +(buffr1[8*nQuad+mnkl] - buffr2[8*nQuad+mnkl]);
            BC[NB4_9+LKNM] = (buffr1[2*nQuad+mnkl] - buffr2[2*nQuad+mnkl])
                            -(buffr1[8*nQuad+mnkl] - buffr2[8*nQuad+mnkl]);
            
            // (σxσ)_z = σ_x*σ_y - σ_y*σ_x
            BC[NB4_10+MNKL] =-(buffr1[4*nQuad+mnkl] - buffr2[4*nQuad+mnkl])
                             +(buffr1[1*nQuad+mnkl] - buffr2[1*nQuad+mnkl]);
            BC[NB4_10+LKNM] = (buffr1[4*nQuad+mnkl] - buffr2[4*nQuad+mnkl])
                             -(buffr1[1*nQuad+mnkl] - buffr2[1*nQuad+mnkl]);
            
            // (σσ) (xx - yy - zz)
            BC[NB4_11+MNKL] =-(buffr1[mnkl] - buffr2[mnkl])
                             +(buffr1[5*nQuad+mnkl] - buffr2[5*nQuad+mnkl])
                             +(buffr1[10*nQuad+mnkl] - buffr2[10*nQuad+mnkl]);
            BC[NB4_11+LKNM] =-(buffr1[mnkl] - buffr2[mnkl])
                             +(buffr1[5*nQuad+mnkl] - buffr2[5*nQuad+mnkl])
                             +(buffr1[10*nQuad+mnkl] - buffr2[10*nQuad+mnkl]);
            
            // (σσ) (-xx + yy - zz)
            BC[NB4_12+MNKL] = (buffr1[mnkl] - buffr2[mnkl])
                             -(buffr1[5*nQuad+mnkl] - buffr2[5*nQuad+mnkl])
                             +(buffr1[10*nQuad+mnkl] - buffr2[10*nQuad+mnkl]);
            BC[NB4_12+LKNM] = (buffr1[mnkl] - buffr2[mnkl])
                             -(buffr1[5*nQuad+mnkl] - buffr2[5*nQuad+mnkl])
                             +(buffr1[10*nQuad+mnkl] - buffr2[10*nQuad+mnkl]);
            
            // (σσ) (-xx - yy + zz)
            BC[NB4_13+MNKL] = (buffr1[mnkl] - buffr2[mnkl])
                             +(buffr1[5*nQuad+mnkl] - buffr2[5*nQuad+mnkl])
                             -(buffr1[10*nQuad+mnkl] - buffr2[10*nQuad+mnkl]);
            BC[NB4_13+LKNM] = (buffr1[mnkl] - buffr2[mnkl])
                             +(buffr1[5*nQuad+mnkl] - buffr2[5*nQuad+mnkl])
                             -(buffr1[10*nQuad+mnkl] - buffr2[10*nQuad+mnkl]);
            
            //  σ_x*σ_y + σ_y*σ_x
            BC[NB4_14+MNKL] =-(buffr1[4*nQuad+mnkl] - buffr2[4*nQuad+mnkl])
                             -(buffr1[1*nQuad+mnkl] - buffr2[1*nQuad+mnkl]);
            BC[NB4_14+LKNM] =-(buffr1[4*nQuad+mnkl] - buffr2[4*nQuad+mnkl])
                             -(buffr1[1*nQuad+mnkl] - buffr2[1*nQuad+mnkl]);
            
            //  σ_z*σ_x + σ_x*σ_z
            BC[NB4_15+MNKL] =-(buffr1[2*nQuad+mnkl] - buffr2[2*nQuad+mnkl])
                             -(buffr1[8*nQuad+mnkl] - buffr2[8*nQuad+mnkl]);
            BC[NB4_15+LKNM] =-(buffr1[2*nQuad+mnkl] - buffr2[2*nQuad+mnkl])
                             -(buffr1[8*nQuad+mnkl] - buffr2[8*nQuad+mnkl]);
            
            //  σ_y*σ_z + σ_z*σ_y
            BC[NB4_16+MNKL] =-(buffr1[9*nQuad+mnkl] - buffr2[9*nQuad+mnkl])
                             -(buffr1[6*nQuad+mnkl] - buffr2[6*nQuad+mnkl]);
            BC[NB4_16+LKNM] =-(buffr1[9*nQuad+mnkl] - buffr2[9*nQuad+mnkl])
                             -(buffr1[6*nQuad+mnkl] - buffr2[6*nQuad+mnkl]);
            
            // σ_x*σ_x
            BC[NB4_17+MNKL] = -(buffr1[mnkl] - buffr2[mnkl]);
            BC[NB4_17+LKNM] = -(buffr1[mnkl] - buffr2[mnkl]);
            
            // σ_x*σ_y
            BC[NB4_18+MNKL] = -(buffr1[4*nQuad+mnkl] - buffr2[4*nQuad+mnkl]);
            BC[NB4_18+LKNM] = -(buffr1[1*nQuad+mnkl] - buffr2[1*nQuad+mnkl]);
            
            // σ_x*σ_z
            BC[NB4_19+MNKL] = -(buffr1[8*nQuad+mnkl] - buffr2[8*nQuad+mnkl]);
            BC[NB4_19+LKNM] = -(buffr1[2*nQuad+mnkl] - buffr2[2*nQuad+mnkl]);
            
            // σ_y*σ_x
            BC[NB4_20+MNKL] = -(buffr1[1*nQuad+mnkl] - buffr2[1*nQuad+mnkl]);
            BC[NB4_20+LKNM] = -(buffr1[4*nQuad+mnkl] - buffr2[4*nQuad+mnkl]);
            
            // σ_y*σ_y
            BC[NB4_21+MNKL] = -(buffr1[5*nQuad+mnkl] - buffr2[5*nQuad+mnkl]);
            BC[NB4_21+LKNM] = -(buffr1[5*nQuad+mnkl] - buffr2[5*nQuad+mnkl]);
            
            // σ_y*σ_z
            BC[NB4_22+MNKL] = -(buffr1[9*nQuad+mnkl] - buffr2[9*nQuad+mnkl]);
            BC[NB4_22+LKNM] = -(buffr1[6*nQuad+mnkl] - buffr2[6*nQuad+mnkl]);
            
            // σ_z*σ_x
            BC[NB4_23+MNKL] = -(buffr1[2*nQuad+mnkl] - buffr2[2*nQuad+mnkl]);
            BC[NB4_23+LKNM] = -(buffr1[8*nQuad+mnkl] - buffr2[8*nQuad+mnkl]);
            
            // σ_z*σ_y
            BC[NB4_24+MNKL] = -(buffr1[6*nQuad+mnkl] - buffr2[6*nQuad+mnkl]);
            BC[NB4_24+LKNM] = -(buffr1[9*nQuad+mnkl] - buffr2[9*nQuad+mnkl]);
            
            // σ_z*σ_z
            BC[NB4_25+MNKL] = -(buffr1[10*nQuad+mnkl] - buffr2[10*nQuad+mnkl]);
            BC[NB4_25+LKNM] = -(buffr1[10*nQuad+mnkl] - buffr2[10*nQuad+mnkl]);
            
            
            mnkl++;

          } // ∇B∇C integral preparation loop
  
#if 0
      // LSSL
      auto ijkl =0;
      for(auto index=0; index<26; index++){
        std::cout << "Libcint ("<<index<<")(ij|kl)" << std::endl;
        for(m = 0ul,            bf1 = bf1_s; m <                  n1; ++m, bf1++)
        for(n =   maxShellSize, bf2 = bf2_s; n <   maxShellSize + n2; ++n, bf2++)
        for(k = 2*maxShellSize, bf3 = bf3_s; k < 2*maxShellSize + n3; ++k, bf3++)
        for(l = 3*maxShellSize, bf4 = bf4_s; l < 3*maxShellSize + n4; ++l, bf4++){
          auto MNKL = m + n*NB + k*NB2 + l*NB3;
          std::cout << "(" << bf1 << "," << bf2 << "|" << bf3 << "," << bf4 << ")  ";
          std::cout << BC[index*NB4+MNKL] << std::endl;
	  ijkl++;
        };
      };
#endif


  
  
#ifdef _CONTRACTION_ // Contraction

          auto ADXLLMS  = AX_loc[XLLMS];
          auto ADXLLMX  = AX_loc[XLLMX];
          auto ADXLLMY  = AX_loc[XLLMY];
          auto ADXLLMZ  = AX_loc[XLLMZ];
          auto DXLLMS = matList[XLLMS].X;
          auto DXLLMX = matList[XLLMX].X;
          auto DXLLMY = matList[XLLMY].X;
          auto DXLLMZ = matList[XLLMZ].X;

          auto ADXSSMS  = AX_loc[XSSMS];
          auto ADXSSMX  = AX_loc[XSSMX];
          auto ADXSSMY  = AX_loc[XSSMY];
          auto ADXSSMZ  = AX_loc[XSSMZ];
          auto DXSSMS = matList[XSSMS].X;
          auto DXSSMX = matList[XSSMX].X;
          auto DXSSMY = matList[XSSMY].X;
          auto DXSSMZ = matList[XSSMZ].X;

          auto ADCLSMS  = AX_loc[CLSMS];
          auto ADCLSMX  = AX_loc[CLSMX];
          auto ADCLSMY  = AX_loc[CLSMY];
          auto ADCLSMZ  = AX_loc[CLSMZ];
          auto ADXLSMS  = AX_loc[XLSMS];
          auto ADXLSMX  = AX_loc[XLSMX];
          auto ADXLSMY  = AX_loc[XLSMY];
          auto ADXLSMZ  = AX_loc[XLSMZ];

          auto DXLSMS = matList[XLSMS].X;
          auto DXLSMX = matList[XLSMX].X;
          auto DXLSMY = matList[XLSMY].X;
          auto DXLSMZ = matList[XLSMZ].X;
          auto DXSLMS = matList[XSLMS].X;
          auto DXSLMX = matList[XSLMX].X;
          auto DXSLMY = matList[XSLMY].X;
          auto DXSLMZ = matList[XSLMZ].X;

          auto DCLSMS = matList[CLSMS].X;
          auto DCLSMX = matList[CLSMX].X;
          auto DCLSMY = matList[CLSMY].X;
          auto DCLSMZ = matList[CLSMZ].X;
          auto DCSLMS = matList[CSLMS].X;
          auto DCSLMX = matList[CSLMX].X;
          auto DCSLMY = matList[CSLMY].X;
          auto DCSLMZ = matList[CSLMZ].X;
 
          for(m = 0ul,            bf1 = bf1_s; m <                  n1; ++m, bf1++) {
          for(n =   maxShellSize, bf2 = bf2_s; n <   maxShellSize + n2; ++n, bf2++) {
            auto bf1nB = bf1*nBasis;
            auto bf2nB = bf2*nBasis;

          for(k = 2*maxShellSize, bf3 = bf3_s; k < 2*maxShellSize + n3; ++k, bf3++) {
            auto bf3nB = bf3*nBasis;

          for(l = 3*maxShellSize, bf4 = bf4_s; l < 3*maxShellSize + n4; ++l, bf4++) {
            auto MNKL = m + n*NB + k*NB2 + l*NB3;
            auto LKNM = l + k*NB + n*NB2 + m*NB3;

            auto bf4nB= bf4*nBasis;
            auto bf14 = bf1 + bf4nB;
            auto bf24 = bf2 + bf4nB;
            auto bf23 = bf2 + bf3nB;
            auto bf13 = bf1 + bf3nB;
            auto bf41 = bf4 + bf1nB;
            auto bf31 = bf3 + bf1nB;
            auto bf32 = bf3 + bf2nB;
            auto bf42 = bf4 + bf2nB;
            auto bf34 = bf3 + bf4nB;
            auto bf43 = bf4 + bf3nB;
            auto bf12 = bf1 + bf2nB;
            auto bf21 = bf2 + bf1nB;

            //      auto ss = 0     ;        // ERI00 (ss)(ij|kl)
            //      auto sx = NB4   ;        // ERI01 (sσ)_x(ijkl)
            //      auto sy = NB4_2 ;        // ERI02 (sσ)_y(ijkl)
            //      auto sz = NB4_3 ;        // ERI03 (sσ)_z(ijkl)
            //      auto xs = NB4_4 ;        // ERI04 (σs)_x(ijkl)
            //      auto ys = NB4_5 ;        // ERI05 (σs)_y(ijkl)
            //      auto zs = NB4_6 ;        // ERI06 (σs)_z(ijkl)
            //      auto dot = NB4_7 ;       // ERI07 (σ∙σ)(ijkl)
            //      auto crossx = NB4_8 ;    // ERI08 (σxσ)_x(ijkl)
            //      auto crossy = NB4_9 ;    // ERI09 (σxσ)_y(ijkl)
            //      auto crossz = NB4_10;    // ERI10 (σxσ)_z(ijkl)
            //      auto pxxmyymzz = NB4_11; // ERI11 (σσ)(xx - yy - zz)(ijkl)
            //      auto mxxpyymzz = NB4_12; // ERI12 (σσ)(-xx + yy - zz)(ijkl)
            //      auto mxxmyypzz = NB4_13; // ERI13 (σσ)(-xx - yy + zz)(ijkl)
            //      auto xypyx = NB4_14;     // ERI14 (σ_x σ_y + σ_y σ_x)(ijkl)
            //      auto zxpxz = NB4_15;     // ERI15 (σ_z σ_x + σ_x σ_z)(ijkl)
            //      auto yzpzy = NB4_16;     // ERI16 (σ_y σ_z + σ_z σ_y)(ijkl)
            //      auto xx = NB4_17;        // ERI17 (σ_x σ_x)(ijkl)
            //      auto xy = NB4_18;        // ERI18 (σ_x σ_y)(ijkl)
            //      auto xz = NB4_19;        // ERI19 (σ_x σ_z)(ijkl)
            //      auto yx = NB4_20;        // ERI20 (σ_y σ_x)(ijkl)
            //      auto yy = NB4_21;        // ERI21 (σ_y σ_y)(ijkl)
            //      auto yz = NB4_22;        // ERI22 (σ_y σ_z)(ijkl)
            //      auto zx = NB4_23;        // ERI23 (σ_z σ_x)(ijkl)
            //      auto zy = NB4_24;        // ERI24 (σ_z σ_y)(ijkl)
            //      auto zz = NB4_25;        // ERI25 (σ_z σ_z)(ijkl)
             
            /*++++++++++++++++++++++++*/
            /* Start of Gauge (LL|LL) */
            /*++++++++++++++++++++++++*/
            
            // Exchange
            // MNKL
            
            /* Equation (208) */
            ADXLLMS[bf14] +=     DXSSMS[bf23]*BC[MNKL];
            ADXLLMS[bf14] +=  iS*DXSSMX[bf23]*BC[MNKL+sx];
            ADXLLMS[bf14] +=  iS*DXSSMY[bf23]*BC[MNKL+sy];
            ADXLLMS[bf14] +=  iS*DXSSMZ[bf23]*BC[MNKL+sz];
            ADXLLMS[bf14] +=  iS*DXSSMZ[bf23]*BC[MNKL+zs];
            ADXLLMS[bf14] +=  iS*DXSSMX[bf23]*BC[MNKL+xs];
            ADXLLMS[bf14] +=  iS*DXSSMY[bf23]*BC[MNKL+ys];
            ADXLLMS[bf14] +=     DXSSMS[bf23]*BC[MNKL+dot];
            ADXLLMS[bf14] -=  iS*DXSSMZ[bf23]*BC[MNKL+crossz];
            ADXLLMS[bf14] -=  iS*DXSSMX[bf23]*BC[MNKL+crossx];
            ADXLLMS[bf14] -=  iS*DXSSMY[bf23]*BC[MNKL+crossy];
            
            /* Equation (209) */
            ADXLLMZ[bf14] +=     DXSSMZ[bf23]*BC[MNKL];
            ADXLLMZ[bf14] +=  iS*DXSSMS[bf23]*BC[MNKL+sz];
            ADXLLMZ[bf14] +=     DXSSMY[bf23]*BC[MNKL+sx];
            ADXLLMZ[bf14] -=     DXSSMX[bf23]*BC[MNKL+sy];
            ADXLLMZ[bf14] +=  iS*DXSSMS[bf23]*BC[MNKL+zs];
            ADXLLMZ[bf14] +=     DXSSMX[bf23]*BC[MNKL+ys];
            ADXLLMZ[bf14] -=     DXSSMY[bf23]*BC[MNKL+xs];
            ADXLLMZ[bf14] +=     DXSSMZ[bf23]*BC[MNKL+mxxmyypzz];
            ADXLLMZ[bf14] +=  iS*DXSSMS[bf23]*BC[MNKL+crossz];
            ADXLLMZ[bf14] +=     DXSSMX[bf23]*BC[MNKL+zxpxz];
            ADXLLMZ[bf14] +=     DXSSMY[bf23]*BC[MNKL+yzpzy];
            
            /* Equation (210) */
            ADXLLMX[bf14] +=     DXSSMX[bf23]*BC[MNKL];
            ADXLLMX[bf14] +=  iS*DXSSMS[bf23]*BC[MNKL+sx];
            ADXLLMX[bf14] -=     DXSSMY[bf23]*BC[MNKL+sz];
            ADXLLMX[bf14] +=     DXSSMZ[bf23]*BC[MNKL+sy];
            ADXLLMX[bf14] +=  iS*DXSSMS[bf23]*BC[MNKL+xs];
            ADXLLMX[bf14] +=     DXSSMY[bf23]*BC[MNKL+zs];
            ADXLLMX[bf14] -=     DXSSMZ[bf23]*BC[MNKL+ys];
            ADXLLMX[bf14] +=     DXSSMX[bf23]*BC[MNKL+pxxmyymzz];
            ADXLLMX[bf14] +=  iS*DXSSMS[bf23]*BC[MNKL+crossx];
            ADXLLMX[bf14] +=     DXSSMY[bf23]*BC[MNKL+xypyx];
            ADXLLMX[bf14] +=     DXSSMZ[bf23]*BC[MNKL+zxpxz];
            
            /* Equation (211) */
            ADXLLMY[bf14] +=     DXSSMY[bf23]*BC[MNKL];
            ADXLLMY[bf14] +=  iS*DXSSMS[bf23]*BC[MNKL+sy];
            ADXLLMY[bf14] +=     DXSSMX[bf23]*BC[MNKL+sz];
            ADXLLMY[bf14] -=     DXSSMZ[bf23]*BC[MNKL+sx];
            ADXLLMY[bf14] +=  iS*DXSSMS[bf23]*BC[MNKL+ys];
            ADXLLMY[bf14] -=     DXSSMX[bf23]*BC[MNKL+zs];
            ADXLLMY[bf14] +=     DXSSMZ[bf23]*BC[MNKL+xs];
            ADXLLMY[bf14] +=     DXSSMY[bf23]*BC[MNKL+mxxpyymzz];
            ADXLLMY[bf14] +=  iS*DXSSMS[bf23]*BC[MNKL+crossy];
            ADXLLMY[bf14] +=     DXSSMX[bf23]*BC[MNKL+xypyx];
            ADXLLMY[bf14] +=     DXSSMZ[bf23]*BC[MNKL+yzpzy];
            
            // LKNM
            if(bf1_s!=bf4_s or bf2_s!=bf3_s) {
              /* Equation (208) */
              ADXLLMS[bf41] +=     DXSSMS[bf32]*BC[LKNM];
              ADXLLMS[bf41] +=  iS*DXSSMX[bf32]*BC[LKNM+sx];
              ADXLLMS[bf41] +=  iS*DXSSMY[bf32]*BC[LKNM+sy];
              ADXLLMS[bf41] +=  iS*DXSSMZ[bf32]*BC[LKNM+sz];
              ADXLLMS[bf41] +=  iS*DXSSMZ[bf32]*BC[LKNM+zs];
              ADXLLMS[bf41] +=  iS*DXSSMX[bf32]*BC[LKNM+xs];
              ADXLLMS[bf41] +=  iS*DXSSMY[bf32]*BC[LKNM+ys];
              ADXLLMS[bf41] +=     DXSSMS[bf32]*BC[LKNM+dot];
              ADXLLMS[bf41] -=  iS*DXSSMZ[bf32]*BC[LKNM+crossz];
              ADXLLMS[bf41] -=  iS*DXSSMX[bf32]*BC[LKNM+crossx];
              ADXLLMS[bf41] -=  iS*DXSSMY[bf32]*BC[LKNM+crossy];
              
              /* Equation (209) */
              ADXLLMZ[bf41] +=     DXSSMZ[bf32]*BC[LKNM];
              ADXLLMZ[bf41] +=  iS*DXSSMS[bf32]*BC[LKNM+sz];
              ADXLLMZ[bf41] +=     DXSSMY[bf32]*BC[LKNM+sx];
              ADXLLMZ[bf41] -=     DXSSMX[bf32]*BC[LKNM+sy];
              ADXLLMZ[bf41] +=  iS*DXSSMS[bf32]*BC[LKNM+zs];
              ADXLLMZ[bf41] +=     DXSSMX[bf32]*BC[LKNM+ys];
              ADXLLMZ[bf41] -=     DXSSMY[bf32]*BC[LKNM+xs];
              ADXLLMZ[bf41] +=     DXSSMZ[bf32]*BC[LKNM+mxxmyypzz];
              ADXLLMZ[bf41] +=  iS*DXSSMS[bf32]*BC[LKNM+crossz];
              ADXLLMZ[bf41] +=     DXSSMX[bf32]*BC[LKNM+zxpxz];
              ADXLLMZ[bf41] +=     DXSSMY[bf32]*BC[LKNM+yzpzy];
              
              /* Equation (210) */
              ADXLLMX[bf41] +=     DXSSMX[bf32]*BC[LKNM];
              ADXLLMX[bf41] +=  iS*DXSSMS[bf32]*BC[LKNM+sx];
              ADXLLMX[bf41] -=     DXSSMY[bf32]*BC[LKNM+sz];
              ADXLLMX[bf41] +=     DXSSMZ[bf32]*BC[LKNM+sy];
              ADXLLMX[bf41] +=  iS*DXSSMS[bf32]*BC[LKNM+xs];
              ADXLLMX[bf41] +=     DXSSMY[bf32]*BC[LKNM+zs];
              ADXLLMX[bf41] -=     DXSSMZ[bf32]*BC[LKNM+ys];
              ADXLLMX[bf41] +=     DXSSMX[bf32]*BC[LKNM+pxxmyymzz];
              ADXLLMX[bf41] +=  iS*DXSSMS[bf32]*BC[LKNM+crossx];
              ADXLLMX[bf41] +=     DXSSMY[bf32]*BC[LKNM+xypyx];
              ADXLLMX[bf41] +=     DXSSMZ[bf32]*BC[LKNM+zxpxz];
              
              /* Equation (211) */
              ADXLLMY[bf41] +=     DXSSMY[bf32]*BC[LKNM];
              ADXLLMY[bf41] +=  iS*DXSSMS[bf32]*BC[LKNM+sy];
              ADXLLMY[bf41] +=     DXSSMX[bf32]*BC[LKNM+sz];
              ADXLLMY[bf41] -=     DXSSMZ[bf32]*BC[LKNM+sx];
              ADXLLMY[bf41] +=  iS*DXSSMS[bf32]*BC[LKNM+ys];
              ADXLLMY[bf41] -=     DXSSMX[bf32]*BC[LKNM+zs];
              ADXLLMY[bf41] +=     DXSSMZ[bf32]*BC[LKNM+xs];
              ADXLLMY[bf41] +=     DXSSMY[bf32]*BC[LKNM+mxxpyymzz];
              ADXLLMY[bf41] +=  iS*DXSSMS[bf32]*BC[LKNM+crossy];
              ADXLLMY[bf41] +=     DXSSMX[bf32]*BC[LKNM+xypyx];
              ADXLLMY[bf41] +=     DXSSMZ[bf32]*BC[LKNM+yzpzy];
            }
            
            
            /*++++++++++++++++++++++++*/
            /* Start of Gauge (SS|SS) */
            /*++++++++++++++++++++++++*/
            
            // Exchange
            // MNKL: TRANS_MN_TRANS_KL
            // If a TRANS is on a sigma, it will change the sign of the integral.
            
            /* Equation (227) */
            ADXSSMS[bf23] +=    DXLLMS[bf14]*BC[MNKL];
            ADXSSMS[bf23] -= iS*DXLLMX[bf14]*BC[MNKL+sx];
            ADXSSMS[bf23] -= iS*DXLLMY[bf14]*BC[MNKL+sy];
            ADXSSMS[bf23] -= iS*DXLLMZ[bf14]*BC[MNKL+sz];
            ADXSSMS[bf23] -= iS*DXLLMX[bf14]*BC[MNKL+xs];
            ADXSSMS[bf23] -= iS*DXLLMY[bf14]*BC[MNKL+ys];
            ADXSSMS[bf23] -= iS*DXLLMZ[bf14]*BC[MNKL+zs];
            ADXSSMS[bf23] +=    DXLLMS[bf14]*BC[MNKL+dot];
            ADXSSMS[bf23] -= iS*DXLLMX[bf14]*BC[MNKL+crossx];
            ADXSSMS[bf23] -= iS*DXLLMY[bf14]*BC[MNKL+crossy];
            ADXSSMS[bf23] -= iS*DXLLMZ[bf14]*BC[MNKL+crossz];
            
            /* Equation (228) */
            ADXSSMZ[bf23] +=    DXLLMZ[bf14]*BC[MNKL];
            ADXSSMZ[bf23] -= iS*DXLLMS[bf14]*BC[MNKL+sz];
            ADXSSMZ[bf23] +=    DXLLMX[bf14]*BC[MNKL+sy];
            ADXSSMZ[bf23] -=    DXLLMY[bf14]*BC[MNKL+sx];
            ADXSSMZ[bf23] -= iS*DXLLMS[bf14]*BC[MNKL+zs];
            ADXSSMZ[bf23] -=    DXLLMX[bf14]*BC[MNKL+ys];
            ADXSSMZ[bf23] +=    DXLLMY[bf14]*BC[MNKL+xs];
            ADXSSMZ[bf23] +=    DXLLMZ[bf14]*BC[MNKL+mxxmyypzz];
            ADXSSMZ[bf23] += iS*DXLLMS[bf14]*BC[MNKL+crossz];
            ADXSSMZ[bf23] +=    DXLLMX[bf14]*BC[MNKL+zxpxz];
            ADXSSMZ[bf23] +=    DXLLMY[bf14]*BC[MNKL+yzpzy];
            
            /* Equation (229) */
            ADXSSMX[bf23] +=    DXLLMX[bf14]*BC[MNKL];
            ADXSSMX[bf23] -= iS*DXLLMS[bf14]*BC[MNKL+sx];
            ADXSSMX[bf23] +=    DXLLMY[bf14]*BC[MNKL+sz];
            ADXSSMX[bf23] -=    DXLLMZ[bf14]*BC[MNKL+sy];
            ADXSSMX[bf23] -= iS*DXLLMS[bf14]*BC[MNKL+xs];
            ADXSSMX[bf23] -=    DXLLMY[bf14]*BC[MNKL+zs];
            ADXSSMX[bf23] +=    DXLLMZ[bf14]*BC[MNKL+ys];
            ADXSSMX[bf23] +=    DXLLMX[bf14]*BC[MNKL+pxxmyymzz];
            ADXSSMX[bf23] += iS*DXLLMS[bf14]*BC[MNKL+crossx];
            ADXSSMX[bf23] +=    DXLLMY[bf14]*BC[MNKL+xypyx];
            ADXSSMX[bf23] +=    DXLLMZ[bf14]*BC[MNKL+zxpxz];
            
            /* Equation (230) */
            ADXSSMY[bf23] +=    DXLLMY[bf14]*BC[MNKL];
            ADXSSMY[bf23] -= iS*DXLLMS[bf14]*BC[MNKL+sy];
            ADXSSMY[bf23] -=    DXLLMX[bf14]*BC[MNKL+sz];
            ADXSSMY[bf23] +=    DXLLMZ[bf14]*BC[MNKL+sx];
            ADXSSMY[bf23] -= iS*DXLLMS[bf14]*BC[MNKL+ys];
            ADXSSMY[bf23] +=    DXLLMX[bf14]*BC[MNKL+zs];
            ADXSSMY[bf23] -=    DXLLMZ[bf14]*BC[MNKL+xs];
            ADXSSMY[bf23] +=    DXLLMY[bf14]*BC[MNKL+mxxpyymzz];
            ADXSSMY[bf23] += iS*DXLLMS[bf14]*BC[MNKL+crossy];
            ADXSSMY[bf23] +=    DXLLMX[bf14]*BC[MNKL+xypyx];
            ADXSSMY[bf23] +=    DXLLMZ[bf14]*BC[MNKL+yzpzy];
            
            // LKNM, TRANS_MN_TRANS_KL
            if(bf1_s!=bf4_s or bf2_s!=bf3_s) {
              /* Equation (227) */
              ADXSSMS[bf32] +=    DXLLMS[bf41]*BC[LKNM];
              ADXSSMS[bf32] -= iS*DXLLMX[bf41]*BC[LKNM+sx];
              ADXSSMS[bf32] -= iS*DXLLMY[bf41]*BC[LKNM+sy];
              ADXSSMS[bf32] -= iS*DXLLMZ[bf41]*BC[LKNM+sz];
              ADXSSMS[bf32] -= iS*DXLLMX[bf41]*BC[LKNM+xs];
              ADXSSMS[bf32] -= iS*DXLLMY[bf41]*BC[LKNM+ys];
              ADXSSMS[bf32] -= iS*DXLLMZ[bf41]*BC[LKNM+zs];
              ADXSSMS[bf32] +=    DXLLMS[bf41]*BC[LKNM+dot];
              ADXSSMS[bf32] -= iS*DXLLMX[bf41]*BC[LKNM+crossx];
              ADXSSMS[bf32] -= iS*DXLLMY[bf41]*BC[LKNM+crossy];
              ADXSSMS[bf32] -= iS*DXLLMZ[bf41]*BC[LKNM+crossz];
              
              /* Equation (228) */
              ADXSSMZ[bf32] +=    DXLLMZ[bf41]*BC[LKNM];
              ADXSSMZ[bf32] -= iS*DXLLMS[bf41]*BC[LKNM+sz];
              ADXSSMZ[bf32] +=    DXLLMX[bf41]*BC[LKNM+sy];
              ADXSSMZ[bf32] -=    DXLLMY[bf41]*BC[LKNM+sx];
              ADXSSMZ[bf32] -= iS*DXLLMS[bf41]*BC[LKNM+zs];
              ADXSSMZ[bf32] -=    DXLLMX[bf41]*BC[LKNM+ys];
              ADXSSMZ[bf32] +=    DXLLMY[bf41]*BC[LKNM+xs];
              ADXSSMZ[bf32] +=    DXLLMZ[bf41]*BC[LKNM+mxxmyypzz];
              ADXSSMZ[bf32] += iS*DXLLMS[bf41]*BC[LKNM+crossz];
              ADXSSMZ[bf32] +=    DXLLMX[bf41]*BC[LKNM+zxpxz];
              ADXSSMZ[bf32] +=    DXLLMY[bf41]*BC[LKNM+yzpzy];
              
              /* Equation (229) */
              ADXSSMX[bf32] +=    DXLLMX[bf41]*BC[LKNM];
              ADXSSMX[bf32] -= iS*DXLLMS[bf41]*BC[LKNM+sx];
              ADXSSMX[bf32] +=    DXLLMY[bf41]*BC[LKNM+sz];
              ADXSSMX[bf32] -=    DXLLMZ[bf41]*BC[LKNM+sy];
              ADXSSMX[bf32] -= iS*DXLLMS[bf41]*BC[LKNM+xs];
              ADXSSMX[bf32] -=    DXLLMY[bf41]*BC[LKNM+zs];
              ADXSSMX[bf32] +=    DXLLMZ[bf41]*BC[LKNM+ys];
              ADXSSMX[bf32] +=    DXLLMX[bf41]*BC[LKNM+pxxmyymzz];
              ADXSSMX[bf32] += iS*DXLLMS[bf41]*BC[LKNM+crossx];
              ADXSSMX[bf32] +=    DXLLMY[bf41]*BC[LKNM+xypyx];
              ADXSSMX[bf32] +=    DXLLMZ[bf41]*BC[LKNM+zxpxz];
              
              /* Equation (230) */
              ADXSSMY[bf32] +=    DXLLMY[bf41]*BC[LKNM];
              ADXSSMY[bf32] -= iS*DXLLMS[bf41]*BC[LKNM+sy];
              ADXSSMY[bf32] -=    DXLLMX[bf41]*BC[LKNM+sz];
              ADXSSMY[bf32] +=    DXLLMZ[bf41]*BC[LKNM+sx];
              ADXSSMY[bf32] -= iS*DXLLMS[bf41]*BC[LKNM+ys];
              ADXSSMY[bf32] +=    DXLLMX[bf41]*BC[LKNM+zs];
              ADXSSMY[bf32] -=    DXLLMZ[bf41]*BC[LKNM+xs];
              ADXSSMY[bf32] +=    DXLLMY[bf41]*BC[LKNM+mxxpyymzz];
              ADXSSMY[bf32] += iS*DXLLMS[bf41]*BC[LKNM+crossy];
              ADXSSMY[bf32] +=    DXLLMX[bf41]*BC[LKNM+xypyx];
              ADXSSMY[bf32] +=    DXLLMZ[bf41]*BC[LKNM+yzpzy];
            }
             
             
            /*++++++++++++++++++++++++*/
            /* Start of Gauge (LL|SS) */
            /*++++++++++++++++++++++++*/
             
            
            // Coulomb
            // MNKL
            
            /* Equations (232) and (233) */
            ADCLSMS[bf12] -=     DXLSMS[bf43]*BC[MNKL];
            ADCLSMS[bf12] -=  iS*DXLSMX[bf43]*BC[MNKL+sx];
            ADCLSMS[bf12] -=  iS*DXLSMY[bf43]*BC[MNKL+sy];
            ADCLSMS[bf12] -=  iS*DXLSMZ[bf43]*BC[MNKL+sz];
            // TRANS_KL
            ADCLSMS[bf12] +=     DXSLMS[bf34]*BC[MNKL];
            ADCLSMS[bf12] -=  iS*DXSLMX[bf34]*BC[MNKL+sx];
            ADCLSMS[bf12] -=  iS*DXSLMY[bf34]*BC[MNKL+sy];
            ADCLSMS[bf12] -=  iS*DXSLMZ[bf34]*BC[MNKL+sz];
            
            ADCLSMX[bf12] -=  iS*DXLSMS[bf43]*BC[MNKL+xs];
            ADCLSMX[bf12] -=     DXLSMX[bf43]*BC[MNKL+xx];
            ADCLSMX[bf12] -=     DXLSMY[bf43]*BC[MNKL+xy];
            ADCLSMX[bf12] -=     DXLSMZ[bf43]*BC[MNKL+xz];
            // TRANS_KL
            ADCLSMX[bf12] +=  iS*DXSLMS[bf34]*BC[MNKL+xs];
            ADCLSMX[bf12] -=     DXSLMX[bf34]*BC[MNKL+xx];
            ADCLSMX[bf12] -=     DXSLMY[bf34]*BC[MNKL+xy];
            ADCLSMX[bf12] -=     DXSLMZ[bf34]*BC[MNKL+xz];
            
            ADCLSMY[bf12] -=  iS*DXLSMS[bf43]*BC[MNKL+ys];
            ADCLSMY[bf12] -=     DXLSMX[bf43]*BC[MNKL+yx];
            ADCLSMY[bf12] -=     DXLSMY[bf43]*BC[MNKL+yy];
            ADCLSMY[bf12] -=     DXLSMZ[bf43]*BC[MNKL+yz];
            // TRANS_KL
            ADCLSMY[bf12] +=  iS*DXSLMS[bf34]*BC[MNKL+ys];
            ADCLSMY[bf12] -=     DXSLMX[bf34]*BC[MNKL+yx];
            ADCLSMY[bf12] -=     DXSLMY[bf34]*BC[MNKL+yy];
            ADCLSMY[bf12] -=     DXSLMZ[bf34]*BC[MNKL+yz];
            
            ADCLSMZ[bf12] -=  iS*DXLSMS[bf43]*BC[MNKL+zs];
            ADCLSMZ[bf12] -=     DXLSMX[bf43]*BC[MNKL+zx];
            ADCLSMZ[bf12] -=     DXLSMY[bf43]*BC[MNKL+zy];
            ADCLSMZ[bf12] -=     DXLSMZ[bf43]*BC[MNKL+zz];
            // TRANS_KL
            ADCLSMZ[bf12] +=  iS*DXSLMS[bf34]*BC[MNKL+zs];
            ADCLSMZ[bf12] -=     DXSLMX[bf34]*BC[MNKL+zx];
            ADCLSMZ[bf12] -=     DXSLMY[bf34]*BC[MNKL+zy];
            ADCLSMZ[bf12] -=     DXSLMZ[bf34]*BC[MNKL+zz];
            
            // LKNM
            if(bf1_s!=bf4_s or bf2_s!=bf3_s) {
              ADCLSMS[bf43] -=     DXLSMS[bf12]*BC[LKNM];
              ADCLSMS[bf43] -=  iS*DXLSMX[bf12]*BC[LKNM+sx];
              ADCLSMS[bf43] -=  iS*DXLSMY[bf12]*BC[LKNM+sy];
              ADCLSMS[bf43] -=  iS*DXLSMZ[bf12]*BC[LKNM+sz];
              // TRANS_KL
              ADCLSMS[bf43] +=     DXSLMS[bf21]*BC[LKNM];
              ADCLSMS[bf43] -=  iS*DXSLMX[bf21]*BC[LKNM+sx];
              ADCLSMS[bf43] -=  iS*DXSLMY[bf21]*BC[LKNM+sy];
              ADCLSMS[bf43] -=  iS*DXSLMZ[bf21]*BC[LKNM+sz];
              
              ADCLSMX[bf43] -=  iS*DXLSMS[bf12]*BC[LKNM+xs];
              ADCLSMX[bf43] -=     DXLSMX[bf12]*BC[LKNM+xx];
              ADCLSMX[bf43] -=     DXLSMY[bf12]*BC[LKNM+xy];
              ADCLSMX[bf43] -=     DXLSMZ[bf12]*BC[LKNM+xz];
              // TRANS_KL
              ADCLSMX[bf43] +=  iS*DXSLMS[bf21]*BC[LKNM+xs];
              ADCLSMX[bf43] -=     DXSLMX[bf21]*BC[LKNM+xx];
              ADCLSMX[bf43] -=     DXSLMY[bf21]*BC[LKNM+xy];
              ADCLSMX[bf43] -=     DXSLMZ[bf21]*BC[LKNM+xz];
              
              ADCLSMY[bf43] -=  iS*DXLSMS[bf12]*BC[LKNM+ys];
              ADCLSMY[bf43] -=     DXLSMX[bf12]*BC[LKNM+yx];
              ADCLSMY[bf43] -=     DXLSMY[bf12]*BC[LKNM+yy];
              ADCLSMY[bf43] -=     DXLSMZ[bf12]*BC[LKNM+yz];
              // TRANS_KL
              ADCLSMY[bf43] +=  iS*DXSLMS[bf21]*BC[LKNM+ys];
              ADCLSMY[bf43] -=     DXSLMX[bf21]*BC[LKNM+yx];
              ADCLSMY[bf43] -=     DXSLMY[bf21]*BC[LKNM+yy];
              ADCLSMY[bf43] -=     DXSLMZ[bf21]*BC[LKNM+yz];
              
              ADCLSMZ[bf43] -=  iS*DXLSMS[bf12]*BC[LKNM+zs];
              ADCLSMZ[bf43] -=     DXLSMX[bf12]*BC[LKNM+zx];
              ADCLSMZ[bf43] -=     DXLSMY[bf12]*BC[LKNM+zy];
              ADCLSMZ[bf43] -=     DXLSMZ[bf12]*BC[LKNM+zz];
              // TRANS_KL
              ADCLSMZ[bf43] +=  iS*DXSLMS[bf21]*BC[LKNM+zs];
              ADCLSMZ[bf43] -=     DXSLMX[bf21]*BC[LKNM+zx];
              ADCLSMZ[bf43] -=     DXSLMY[bf21]*BC[LKNM+zy];
              ADCLSMZ[bf43] -=     DXSLMZ[bf21]*BC[LKNM+zz];
            }
             
            // Exchange
            // MNKL, TRANS_KL
            // If a TRANS is on a sigma, it will change the sign of the integral.
            
            /* Equations (248) - (251) */
            ADXLSMS[bf13] -=    DXSLMS[bf24]    *BC[MNKL]; 
            ADXLSMS[bf13] += iS*DXSLMX[bf24]    *BC[MNKL+sx]; 
            ADXLSMS[bf13] += iS*DXSLMY[bf24]    *BC[MNKL+sy]; 
            ADXLSMS[bf13] += iS*DXSLMZ[bf24]    *BC[MNKL+sz]; 
            ADXLSMS[bf13] -= iS*DXSLMX[bf24]    *BC[MNKL+xs]; 
            ADXLSMS[bf13] -= iS*DXSLMY[bf24]    *BC[MNKL+ys]; 
            ADXLSMS[bf13] -= iS*DXSLMZ[bf24]    *BC[MNKL+zs]; 
            ADXLSMS[bf13] +=    DXSLMS[bf24]    *BC[MNKL+dot];
            ADXLSMS[bf13] -= iS*DXSLMX[bf24]    *BC[MNKL+crossx];
            ADXLSMS[bf13] -= iS*DXSLMY[bf24]    *BC[MNKL+crossy];
            ADXLSMS[bf13] -= iS*DXSLMZ[bf24]    *BC[MNKL+crossz];
            
            ADXLSMZ[bf13] -=    DXSLMZ[bf24]    *BC[MNKL]; 
            ADXLSMZ[bf13] += iS*DXSLMS[bf24]    *BC[MNKL+sz]; 
            ADXLSMZ[bf13] -=    DXSLMX[bf24]    *BC[MNKL+sy]; 
            ADXLSMZ[bf13] +=    DXSLMY[bf24]    *BC[MNKL+sx]; 
            ADXLSMZ[bf13] -= iS*DXSLMS[bf24]    *BC[MNKL+zs]; 
            ADXLSMZ[bf13] -=    DXSLMX[bf24]    *BC[MNKL+ys]; 
            ADXLSMZ[bf13] +=    DXSLMY[bf24]    *BC[MNKL+xs]; 
            ADXLSMZ[bf13] +=    DXSLMZ[bf24]    *BC[MNKL+mxxmyypzz];
            ADXLSMZ[bf13] += iS*DXSLMS[bf24]    *BC[MNKL+crossz];
            ADXLSMZ[bf13] +=    DXSLMX[bf24]    *BC[MNKL+zxpxz];
            ADXLSMZ[bf13] +=    DXSLMY[bf24]    *BC[MNKL+yzpzy];
            
            ADXLSMX[bf13] -=    DXSLMX[bf24]    *BC[MNKL]; 
            ADXLSMX[bf13] += iS*DXSLMS[bf24]    *BC[MNKL+sx]; 
            ADXLSMX[bf13] -=    DXSLMY[bf24]    *BC[MNKL+sz]; 
            ADXLSMX[bf13] +=    DXSLMZ[bf24]    *BC[MNKL+sy]; 
            ADXLSMX[bf13] -= iS*DXSLMS[bf24]    *BC[MNKL+xs]; 
            ADXLSMX[bf13] -=    DXSLMY[bf24]    *BC[MNKL+zs]; 
            ADXLSMX[bf13] +=    DXSLMZ[bf24]    *BC[MNKL+ys]; 
            ADXLSMX[bf13] +=    DXSLMX[bf24]    *BC[MNKL+pxxmyymzz];
            ADXLSMX[bf13] += iS*DXSLMS[bf24]    *BC[MNKL+crossx];
            ADXLSMX[bf13] +=    DXSLMY[bf24]    *BC[MNKL+xypyx];
            ADXLSMX[bf13] +=    DXSLMZ[bf24]    *BC[MNKL+zxpxz];
            
            ADXLSMY[bf13] -=    DXSLMY[bf24]    *BC[MNKL]; 
            ADXLSMY[bf13] += iS*DXSLMS[bf24]    *BC[MNKL+sy]; 
            ADXLSMY[bf13] +=    DXSLMX[bf24]    *BC[MNKL+sz]; 
            ADXLSMY[bf13] -=    DXSLMZ[bf24]    *BC[MNKL+sx]; 
            ADXLSMY[bf13] -= iS*DXSLMS[bf24]    *BC[MNKL+ys]; 
            ADXLSMY[bf13] +=    DXSLMX[bf24]    *BC[MNKL+zs]; 
            ADXLSMY[bf13] -=    DXSLMZ[bf24]    *BC[MNKL+xs]; 
            ADXLSMY[bf13] +=    DXSLMY[bf24]    *BC[MNKL+mxxpyymzz];
            ADXLSMY[bf13] += iS*DXSLMS[bf24]    *BC[MNKL+crossy];
            ADXLSMY[bf13] +=    DXSLMX[bf24]    *BC[MNKL+xypyx];
            ADXLSMY[bf13] +=    DXSLMZ[bf24]    *BC[MNKL+yzpzy];
            
            // LKNM, TRANS_KL
            if(bf1_s!=bf4_s or bf2_s!=bf3_s) {
              ADXLSMS[bf42] -=    DXSLMS[bf31]    *BC[LKNM]; 
              ADXLSMS[bf42] += iS*DXSLMX[bf31]    *BC[LKNM+sx]; 
              ADXLSMS[bf42] += iS*DXSLMY[bf31]    *BC[LKNM+sy]; 
              ADXLSMS[bf42] += iS*DXSLMZ[bf31]    *BC[LKNM+sz]; 
              ADXLSMS[bf42] -= iS*DXSLMX[bf31]    *BC[LKNM+xs]; 
              ADXLSMS[bf42] -= iS*DXSLMY[bf31]    *BC[LKNM+ys]; 
              ADXLSMS[bf42] -= iS*DXSLMZ[bf31]    *BC[LKNM+zs]; 
              ADXLSMS[bf42] +=    DXSLMS[bf31]    *BC[LKNM+dot];
              ADXLSMS[bf42] -= iS*DXSLMX[bf31]    *BC[LKNM+crossx];
              ADXLSMS[bf42] -= iS*DXSLMY[bf31]    *BC[LKNM+crossy];
              ADXLSMS[bf42] -= iS*DXSLMZ[bf31]    *BC[LKNM+crossz];
              
              ADXLSMZ[bf42] -=    DXSLMZ[bf31]    *BC[LKNM]; 
              ADXLSMZ[bf42] += iS*DXSLMS[bf31]    *BC[LKNM+sz]; 
              ADXLSMZ[bf42] -=    DXSLMX[bf31]    *BC[LKNM+sy]; 
              ADXLSMZ[bf42] +=    DXSLMY[bf31]    *BC[LKNM+sx]; 
              ADXLSMZ[bf42] -= iS*DXSLMS[bf31]    *BC[LKNM+zs]; 
              ADXLSMZ[bf42] -=    DXSLMX[bf31]    *BC[LKNM+ys]; 
              ADXLSMZ[bf42] +=    DXSLMY[bf31]    *BC[LKNM+xs]; 
              ADXLSMZ[bf42] +=    DXSLMZ[bf31]    *BC[LKNM+mxxmyypzz];
              ADXLSMZ[bf42] += iS*DXSLMS[bf31]    *BC[LKNM+crossz];
              ADXLSMZ[bf42] +=    DXSLMX[bf31]    *BC[LKNM+zxpxz];
              ADXLSMZ[bf42] +=    DXSLMY[bf31]    *BC[LKNM+yzpzy];
              
              ADXLSMX[bf42] -=    DXSLMX[bf31]    *BC[LKNM]; 
              ADXLSMX[bf42] += iS*DXSLMS[bf31]    *BC[LKNM+sx]; 
              ADXLSMX[bf42] -=    DXSLMY[bf31]    *BC[LKNM+sz]; 
              ADXLSMX[bf42] +=    DXSLMZ[bf31]    *BC[LKNM+sy]; 
              ADXLSMX[bf42] -= iS*DXSLMS[bf31]    *BC[LKNM+xs]; 
              ADXLSMX[bf42] -=    DXSLMY[bf31]    *BC[LKNM+zs]; 
              ADXLSMX[bf42] +=    DXSLMZ[bf31]    *BC[LKNM+ys]; 
              ADXLSMX[bf42] +=    DXSLMX[bf31]    *BC[LKNM+pxxmyymzz];
              ADXLSMX[bf42] += iS*DXSLMS[bf31]    *BC[LKNM+crossx];
              ADXLSMX[bf42] +=    DXSLMY[bf31]    *BC[LKNM+xypyx];
              ADXLSMX[bf42] +=    DXSLMZ[bf31]    *BC[LKNM+zxpxz];
              
              ADXLSMY[bf42] -=    DXSLMY[bf31]    *BC[LKNM]; 
              ADXLSMY[bf42] += iS*DXSLMS[bf31]    *BC[LKNM+sy]; 
              ADXLSMY[bf42] +=    DXSLMX[bf31]    *BC[LKNM+sz]; 
              ADXLSMY[bf42] -=    DXSLMZ[bf31]    *BC[LKNM+sx]; 
              ADXLSMY[bf42] -= iS*DXSLMS[bf31]    *BC[LKNM+ys]; 
              ADXLSMY[bf42] +=    DXSLMX[bf31]    *BC[LKNM+zs]; 
              ADXLSMY[bf42] -=    DXSLMZ[bf31]    *BC[LKNM+xs]; 
              ADXLSMY[bf42] +=    DXSLMY[bf31]    *BC[LKNM+mxxpyymzz];
              ADXLSMY[bf42] += iS*DXSLMS[bf31]    *BC[LKNM+crossy];
              ADXLSMY[bf42] +=    DXSLMX[bf31]    *BC[LKNM+xypyx];
              ADXLSMY[bf42] +=    DXSLMZ[bf31]    *BC[LKNM+yzpzy];
            }


          }
          }
          }
          } // contraction loop
  
#endif // Contraction
  
  
        }; // loop s4
        }; // loop s3
        }; // loop s2
        }; // loop s1
  
  
      } // OpenMP context
  
  
      for( auto iTh  = 0; iTh < nThreads; iTh++) {

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLLMS],nBasis,MatsT(1.0),
           matList[XLLMS].AX,nBasis,matList[XLLMS].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLLMX],nBasis,MatsT(1.0),
           matList[XLLMX].AX,nBasis,matList[XLLMX].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLLMY],nBasis,MatsT(1.0),
           matList[XLLMY].AX,nBasis,matList[XLLMY].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLLMZ],nBasis,MatsT(1.0),
           matList[XLLMZ].AX,nBasis,matList[XLLMZ].AX,nBasis);



        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XSSMS],nBasis,MatsT(1.0),
           matList[XSSMS].AX,nBasis,matList[XSSMS].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XSSMX],nBasis,MatsT(1.0),
           matList[XSSMX].AX,nBasis,matList[XSSMX].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XSSMY],nBasis,MatsT(1.0),
           matList[XSSMY].AX,nBasis,matList[XSSMY].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XSSMZ],nBasis,MatsT(1.0),
           matList[XSSMZ].AX,nBasis,matList[XSSMZ].AX,nBasis);



        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][CLSMS],nBasis,MatsT(1.0),
           matList[CLSMS].AX,nBasis,matList[CLSMS].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][CLSMX],nBasis,MatsT(1.0),
           matList[CLSMX].AX,nBasis,matList[CLSMX].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][CLSMY],nBasis,MatsT(1.0),
           matList[CLSMY].AX,nBasis,matList[CLSMY].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][CLSMZ],nBasis,MatsT(1.0),
           matList[CLSMZ].AX,nBasis,matList[CLSMZ].AX,nBasis);
 

  
        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLSMS],nBasis,MatsT(1.0),
           matList[XLSMS].AX,nBasis,matList[XLSMS].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLSMX],nBasis,MatsT(1.0),
           matList[XLSMX].AX,nBasis,matList[XLSMX].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLSMY],nBasis,MatsT(1.0),
           matList[XLSMY].AX,nBasis,matList[XLSMY].AX,nBasis);

        MatAdd('N','N',nBasis,nBasis,MatsT(1.0),AXthreads[iTh][XLSMZ],nBasis,MatsT(1.0),
           matList[XLSMZ].AX,nBasis,matList[XLSMZ].AX,nBasis);
  
      };
   
      memManager_.free(ERIBuffer);
      memManager_.free(buffAll, cacheAll);
#ifdef _SHZ_SCREEN_4C
      if(ShBlkNorms_raw!=nullptr) memManager_.free(ShBlkNorms_raw);
      if(SchwarzGauge!=nullptr) memManager_.free(SchwarzGauge);
#endif
  
#ifdef _REPORT_INTEGRAL_TIMINGS
      size_t nIntSkipGauge = std::accumulate(nSkipGauge.begin(),nSkipGauge.end(),0);
      std::cout << "Gauge Screened " << nIntSkipGauge << std::endl;
  
      auto durDirectGauge = tock(topDirectGauge);
      std::cout << "Gauge AO Direct Contraction took " <<  durDirectGauge << " s\n"; 
  
      std::cout << std::endl;
#endif

    } // if(Gauge)

    /*******************/
    /*                 */
    /*   End of Gauge  */
    /*                 */
    /*******************/






#ifdef _SHZ_SCREEN_4C
    if(SchwarzSSSS!=nullptr) memManager_.free(SchwarzSSSS);
    if(SchwarzERI!=nullptr) memManager_.free(SchwarzERI);
#endif

    if(AXRaw!=nullptr) memManager_.free(AXRaw);

    memManager_.free(atm);
    memManager_.free(bas);
    memManager_.free(env);


    // Turn threads for LA back on
    SetLAThreads(LAThreads);

  }

  template <>
  void GTODirectRelERIContraction<double,double>::directScaffoldLibcint(
    MPI_Comm comm, const bool screen,
    std::vector<TwoBodyContraction<double>> &matList) const {
    CErr("Dirac-Coulomb + Real is an invalid option",std::cout);  
  }

  template <>
  void GTODirectRelERIContraction<dcomplex,dcomplex>::directScaffoldLibcint(
    MPI_Comm comm, const bool screen,
    std::vector<TwoBodyContraction<dcomplex>> &matList) const {
    CErr("Complex integral is is an invalid option",std::cout);  
  }

}; // namespace ChronusQ











