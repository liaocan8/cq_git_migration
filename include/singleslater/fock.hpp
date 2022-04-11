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

#include <singleslater.hpp>
#include <corehbuilder.hpp>
#include <fockbuilder.hpp>
#include <physcon.hpp>

#include <util/timer.hpp>
#include <cqlinalg/blasext.hpp>

#include <cqlinalg.hpp>
#include <cqlinalg/svd.hpp>
#include <cqlinalg/blasutil.hpp>
#include <util/matout.hpp>
#include <util/threads.hpp>
#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <Eigen/Core>


//#define _DEBUGORTHO

namespace ChronusQ {

  /**
   *  \brief Forms the Fock matrix for a single slater determinant using
   *  the 1PDM.
   *
   *  \param [in] increment Whether or not the Fock matrix is being 
   *  incremented using a previous density
   *
   *  Populates / overwrites fock strorage
   */ 
  template <typename MatsT, typename IntsT>
  void SingleSlater<MatsT,IntsT>::formFock(
    EMPerturbation &pert, bool increment, double xHFX) {

    fockBuilder->formFock(*this, pert, increment, xHFX);

  }; // SingleSlater::fockFock


  /**
   *  \brief Compute the Core Hamiltonian.
   *
   *  \param [in] typ Which Hamiltonian to build
   */ 
  template <typename MatsT, typename IntsT>
  void SingleSlater<MatsT,IntsT>::formCoreH(EMPerturbation& emPert, bool save) {

    ROOT_ONLY(comm);

    ProgramTimer::tick("Form Core H");

    size_t NB = basisSet().nBasis;
    if( nC == 4 ) NB = 2 * NB;

    if( coreH != nullptr ) {
      coreH->clear();

    } else {

      if(not iCS and nC == 1 and basisSet().basisType == COMPLEX_GIAO)
        coreH = std::make_shared<PauliSpinorSquareMatrices<MatsT>>(memManager, NB, false);
      else if(nC == 2 or nC == 4)
        coreH = std::make_shared<PauliSpinorSquareMatrices<MatsT>>(memManager, NB, true);
      else
        coreH = std::make_shared<PauliSpinorSquareMatrices<MatsT>>(memManager, NB, false, false);

    }


    // Make a copy of HamiltonianOptions
    HamiltonianOptions hamiltonianOptions = coreHBuilder->getHamiltonianOptions();

    // Prepare one-electron integrals
    std::vector<std::pair<OPERATOR,size_t>> ops;
    if (hamiltonianOptions.basisType == REAL_GTO)
      ops = {{OVERLAP,0}, {KINETIC,0}, {NUCLEAR_POTENTIAL,0},
             {LEN_ELECTRIC_MULTIPOLE,3},
             {VEL_ELECTRIC_MULTIPOLE,3}, {MAGNETIC_MULTIPOLE,2}};
    else
      ops = {{OVERLAP,0}, {KINETIC,0}, {NUCLEAR_POTENTIAL,0},
             {LEN_ELECTRIC_MULTIPOLE,3},
             {MAGNETIC_MULTIPOLE,1}};

    // Multipole integrals NYI for 4C
    if (nC == 4) ops.resize(3);

    // In case of X2C coreHBuilder, here we only compute
    // non-relativistic one electron integrals for contracted basis functions.
    // Relativistic integrals will be computed for uncontracted basis functions
    // in X2C CoreHBuilder.
    if (hamiltonianOptions.x2cType != X2C_TYPE::OFF) {
      hamiltonianOptions.OneEScalarRelativity = false;
      hamiltonianOptions.OneESpinOrbit = false;
    }

    this->aoints.computeAOOneP(memManager,this->molecule(),
        this->basisSet(),emPert, ops, hamiltonianOptions); // compute the necessary 1e ints

    // Compute core Hamiltonian
    coreHBuilder->computeCoreH(emPert,coreH);


    // Compute Orthonormalization trasformations
    computeOrtho();


    // Save the Core Hamiltonian
    if( savFile.exists() && save) {

      const std::array<std::string,4> spinLabel =
        { "SCALAR", "MZ", "MY", "MX" };

      std::vector<MatsT*> CH(coreH->SZYXPointers());
      for(auto i = 0; i < CH.size(); i++)
        savFile.safeWriteData("INTS/CORE_HAMILTONIAN_" +
          spinLabel[i], CH[i], {NB,NB});

    }

    ProgramTimer::tock("Form Core H");

  }; // SingleSlater<MatsT,IntsT>::computeCoreH


  template <typename MatsT, typename IntsT>
  std::vector<double> SingleSlater<MatsT,IntsT>::getGrad(EMPerturbation& pert,
    bool equil, bool saveInts) {

    // Get constants
    size_t NB = basisSet().nBasis;
    size_t nSQ  = NB*NB;

    size_t nAtoms = this->molecule().nAtoms;
    size_t nGrad = 3*nAtoms;

    size_t nSp = fockMatrix->nComponent();
    bool hasXY = fockMatrix->hasXY();
    bool hasZ = fockMatrix->hasZ();


    // Total gradient
    std::vector<double> gradient(nGrad, 0.);

    HamiltonianOptions opts = this->aoints.options_;

    auto printGrad = [&](std::string name, std::vector<double>& vecgrad) {
      std::cout << name << std::endl;
      std::cout << std::setprecision(12);
      for( auto iAt = 0; iAt < nAtoms; iAt++ ) {
        std::cout << " Gradient@I = " << iAt << ":";
        for( auto iCart = 0; iCart < 3; iCart++ ) {
          std::cout << "  " << vecgrad[iAt*3 + iCart];
        }
        std::cout << std::endl;
      }
      std::cout << std::endl;
    };


    // Core H contribution
    this->aoints.computeGradInts(memManager, this->molecule_, basisSet_, pert,
      {{OVERLAP, 1},
       {KINETIC, 1},
       {NUCLEAR_POTENTIAL, 1}
       },
       opts
    );


    std::vector<double> coreGrad = coreHBuilder->getGrad(pert, *this);
    // printGrad("Core H Gradient:", coreGrad);


    // 2e contribution
    this->aoints.computeGradInts(memManager, this->molecule_, basisSet_, pert,
      {{ELECTRON_REPULSION, 1}},
      opts
    );
    std::vector<double> twoEGrad = fockBuilder->getGDGrad(*this, pert);
    std::vector<double> pulayGrad;
    std::vector<double> nucGrad;

    // Pulay contribution
    //
    // NOTE: We may want to change these methods out to use just the energy
    //   weighted density matrix - can probably get some speed up.
    if( equil ) {
      // TODO
    }
    else {

      // S^{-1/2}
      auto orthoForward = orthoAB->forwardPointer();

      // Allocate
      SquareMatrix<MatsT> vdv(memManager, NB);
      SquareMatrix<MatsT> dvv(memManager, NB);
      PauliSpinorSquareMatrices<MatsT> SCR(memManager, NB, hasXY, hasZ);

      // XXX: This requires copying the overlap gradients, but it is for
      //      copying to MatsT != IntsT
      std::vector<SquareMatrix<MatsT>> gradOverlap;
      gradOverlap.reserve(nGrad);
      for( size_t iGrad = 0; iGrad < nGrad; iGrad++ ) {
        gradOverlap.emplace_back((*this->aoints.gradOverlap)[iGrad]->matrix());
      }

      // Calculate dV
      std::vector<SquareMatrix<MatsT>> gradOrtho;
      gradOrtho.reserve(nGrad);
      for( size_t iGrad = 0; iGrad < nGrad; iGrad++ ) {
        gradOrtho.emplace_back(memManager, NB);
      }
      orthoAB->getOrthogonalizationGradients(gradOrtho, gradOverlap);

      for( size_t iGrad = 0; iGrad < nGrad; iGrad++ ) {

        // Form VdV and dVV
        blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::NoTrans,
          NB,NB,NB,MatsT(1.),orthoForward->pointer(),NB,
          gradOrtho[iGrad].pointer(),NB,MatsT(0.),vdv.pointer(),NB);
        blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::NoTrans,
          NB,NB,NB,MatsT(1.),gradOrtho[iGrad].pointer(),NB,
          orthoForward->pointer(),NB,MatsT(0.),dvv.pointer(),NB);

        // Form FVdV and dVVF
        for( auto iSp = 0; iSp < nSp; iSp++ ) {
          auto comp = static_cast<PAULI_SPINOR_COMPS>(iSp);
          blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::NoTrans,
            NB,NB,NB,MatsT(1.),(*fockMatrix)[comp].pointer(),NB,
            vdv.pointer(),NB,MatsT(0.),SCR[comp].pointer(),NB);
          blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::NoTrans,
            NB,NB,NB,MatsT(1.),dvv.pointer(),NB,
            (*fockMatrix)[comp].pointer(),NB,MatsT(1.),SCR[comp].pointer(),NB);
        }

        // Trace
        double gradVal = this->template computeOBProperty<SCALAR>(
          SCR.S().pointer()
        );

        if( hasZ )
          gradVal += this->template computeOBProperty<MZ>(
            SCR.Z().pointer()
          );
        if( hasXY ) {
          gradVal += this->template computeOBProperty<MY>(
            SCR.Y().pointer()
          );
          gradVal += this->template computeOBProperty<MX>(
            SCR.X().pointer()
          );
        }

        pulayGrad.push_back(-0.5*gradVal);
        size_t iAt = iGrad/3;
        size_t iXYZ = iGrad%3;
        gradient[iGrad] = coreGrad[iGrad] + twoEGrad[iGrad] - 0.5*gradVal
                          + this->molecule().nucRepForce[iAt][iXYZ];
        nucGrad.push_back(this->molecule().nucRepForce[iAt][iXYZ]);
      }

    }

    // printGrad("Nuclear Gradient:", nucGrad);
    // printGrad("G Gradient:", twoEGrad);
    // printGrad("Pulay Gradient:", pulayGrad);

    // this->onePDM->output(std::cout, "OnePDM in Gradient Contractions", true);

    return gradient;

  };


  /**
   *  \brief Allocate, compute and store the orthonormalization matricies 
   *  over the CGTO basis.
   *
   *  Computes either the Lowdin or Cholesky transformation matricies based
   *  on orthoType
   */ 
  template <typename MatsT, typename IntsT> 
  void SingleSlater<MatsT,IntsT>::computeOrtho() {

    size_t NB = this->basisSet().nBasis;
    if( nC == 4 ) NB = 2 * NB;
    size_t nSQ  = NB*NB;
    size_t NBC = this->nC*basisSet().nBasis;

    // Allocate scratch
    SquareMatrix<MatsT> overlapSpinor(memManager, NB);
    overlapSpinor.clear();
    SquareMatrix<MatsT> overlapAB(memManager, NBC);
    overlapAB.clear();

    // Copy the overlap over to scratch space
    if ( nC != 4 ) {
      std::copy_n(this->aoints.overlap->pointer(),nSQ,overlapSpinor.pointer());
    } else if( nC == 4 ) {
      // HBL 4C May need a Ints type check (SetMat) to capture GIAO.
      SetMatRE('N',NB/2,NB/2,1.,
               reinterpret_cast<double*>(this->aoints.overlap->pointer()),NB/2,
               overlapSpinor.pointer(),NB);
      SetMatRE('N',NB/2,NB/2,1./(2*SpeedOfLight*SpeedOfLight),
               reinterpret_cast<double*>(this->aoints.kinetic->pointer()),NB/2,
               overlapSpinor.pointer()+NB*NB/2+NB/2,NB);
      //prettyPrintSmart(std::cout,"S Metric",SCR1,NB,NB,NB);
    }
    orthoSpinor = std::make_shared<Orthogonalization<MatsT>>(overlapSpinor);

    // Copy to block diagonal for alpha/beta basis
    if( nC > 1 ){
      SetMat('N',NBC/2,NBC/2,MatsT(1.),overlapSpinor.pointer(), NBC/2, overlapAB.pointer(),NBC);
      size_t disp = NBC/2 + NBC/2*NBC;
      SetMat('N',NBC/2,NBC/2,MatsT(1.),overlapSpinor.pointer(), NBC/2, overlapAB.pointer()+disp,NBC);
    } else {
      overlapAB = overlapSpinor;
    }
    orthoAB = std::make_shared<Orthogonalization<MatsT>>(overlapAB);

  }; // computeOrtho

  template <typename MatsT, typename IntsT>
  void SingleSlater<MatsT,IntsT>::MOFOCK() {

    const size_t NB   = this->nAlphaOrbital();
    const size_t NBC  = nC * NB;

    if( fockMO.empty() ) {
      fockMO.emplace_back(memManager, NBC);
      if( nC == 1 and not iCS )
        fockMO.emplace_back(memManager, NBC);
    }

    if( MPIRank(comm) == 0 ) {

      if ( nC == 2 )
        fockMO[0] = fockMatrix->template spinGather<MatsT>();
      else if ( nC == 1 )
        fockMO = fockMatrix->template spinGatherToBlocks<MatsT>(false, not iCS);

      fockMO[0] = fockMO[0].transform('N',this->mo[0].pointer(),NBC,NBC);

      if( nC == 1 and not iCS ) {

        fockMO[1] = fockMO[1].transform('N',this->mo[1].pointer(),NBC,NBC);

      }

    }

  //prettyPrintSmart(std::cout,"MOF",fockMO[0].pointer(),NBC,NBC,NBC);

#ifdef CQ_ENABLE_MPI

    if(MPISize(comm) > 1) {
      std::cerr  << "  *** Scattering MO-FOCK ***\n";
      for(int k = 0; k < fockMO.size(); k++)
        MPIBCast(fockMO[k].pointer(),NBC*NBC,0,comm);
    }

#endif
  };

}; // namespace ChronusQ

