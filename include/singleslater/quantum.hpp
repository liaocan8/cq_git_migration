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
//#define _DEBUGGIAO 

#include <singleslater.hpp>
#include <cqlinalg/blasext.hpp>
#include <cqlinalg/blasutil.hpp>
#include <cqlinalg/blas3.hpp>
#include <quantum/properties.hpp>

namespace ChronusQ {

  /**
   *  \brief Forms the 1PDM using a set of orbitals 
   *
   *  specialization of Quantum::formDensity. Populates / overwrites
   *  onePDM storage
   * 
   *  NOTE: This function assumes the MO's are in the AO basis
   */ 
  template <typename MatsT, typename IntsT>
  void SingleSlater<MatsT,IntsT>::formDensity() {

    size_t NB  = this->nAlphaOrbital() * nC;
    bool iRO = (std::dynamic_pointer_cast<ROFock<MatsT, IntsT>>(this->fockBuilder) != nullptr);

    // ROHF copy modified orbitals to redundant set
    if( iRO ){
      std::copy_n(this->mo[0].pointer(),NB*NB,this->mo[1].pointer());
      std::copy_n(this->eps1,NB,this->eps2);
    }

    // Form the 1PDM on the root MPI process as slave processes
    // do not posses the up-to-date MO coefficients
    if( MPIRank(comm) == 0 ) {

      if(nC == 1) {

        SquareMatrix<MatsT> DA(memManager, NB);

        //this->mo[0].output(std::cout, "mo1", true);

        // DA = CA * CA**H
        blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans, blas::Op::ConjTrans, NB, NB, this->nOA, MatsT(1.), this->mo[0].pointer(), NB,
            this->mo[0].pointer(), NB, MatsT(0.), DA.pointer(), NB);

        if(iCS) {

          // DS = 2 * DA
          *this->onePDM = PauliSpinorSquareMatrices<MatsT>::spinBlockScatterBuild(DA);

        } else {

          SquareMatrix<MatsT> DB(memManager, NB);

          // DB = CB * CB**H
          blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans, blas::Op::ConjTrans, NB, NB, this->nOB, MatsT(1.), this->mo[1].pointer(), NB,
              this->mo[1].pointer(), NB, MatsT(0.), DB.pointer(), NB);

          // DS = DA + DB
          // DZ = DA - DB
          *this->onePDM = PauliSpinorSquareMatrices<MatsT>::spinBlockScatterBuild(DA,DB);

        }
      } else {

        // 2C or 4C cases
        SquareMatrix<MatsT> spinBlockForm(memManager, NB);

        if( nC == 2 ) {
          blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans, blas::Op::ConjTrans, NB, NB, this->nO, MatsT(1.), this->mo[0].pointer(), NB,
              this->mo[0].pointer(), NB, MatsT(0.), spinBlockForm.pointer(), NB);
        } else if( nC == 4 ) {
          blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans, blas::Op::ConjTrans, NB, NB, this->nO, MatsT(1.), this->mo[0].pointer()+(2*(NB/nC))*NB, NB,
              this->mo[0].pointer()+(2*(NB/nC))*NB, NB, MatsT(0.), spinBlockForm.pointer(), NB);
        }

        *this->onePDM = spinBlockForm.template spinScatter<MatsT>();

      }
      ao2orthoDen();
    }


#ifdef CQ_ENABLE_MPI

    // Broadcast the 1PDM to all MPI processes
    if( MPISize(comm) > 1 ) {
      std::cerr  << "  *** Scattering the 1PDM ***\n";
      for(auto p : this->onePDM->SZYXPointers())
        MPIBCast(p,NB*NB/nC/nC,0,comm);
    }

#endif


#if 0
      print1PDM(std::cerr);
#endif

  }; // SingleSlater<T>::formDensity


  /**
   *  \brief Computes the total field free energy of a single slater determinent
   *
   *  Given a 1PDM and a Fock matrix (specifically the core Hamiltonian and
   *  G[D]), compute the energy.
   *
   *  \warning Assumes that density and Fock matrix have the appropriate form
   *
   *  F(S) = F(A) + F(B)
   *  F(Z) = F(A) - F(B)
   *  ...
   *
   *  \f[
   *     E = \frac{1}{2} \mathrm{Tr}[\mathbf{P}(\mathbf{H} + \mathbf{F})] +
   *     V_{NN}
   *  \f]
   *
   *  Specialization of Quantum<T>::computeEnergy, populates / overwrites 
   *  OBEnergy and MBEnergy
   */ 
  template <typename MatsT, typename IntsT>
  void SingleSlater<MatsT,IntsT>::computeEnergy() {

    ROOT_ONLY(comm);

    // Scalar core hamiltonian contribution to the energy
    this->OBEnergy = 
      this->template computeOBProperty<DENSITY_TYPE::SCALAR>(
         coreH->S().pointer());

    
    
    // One body Spin Orbit
    double SOEnergy = 0.;
    if (coreH->hasZ())
      SOEnergy =
        this->template computeOBProperty<DENSITY_TYPE::MZ>(
           coreH->Z().pointer());
    if (coreH->hasXY()) {
      SOEnergy +=
        this->template computeOBProperty<DENSITY_TYPE::MY>(
           coreH->Y().pointer());
      SOEnergy +=
        this->template computeOBProperty<DENSITY_TYPE::MX>(
           coreH->X().pointer());
    }

 
    this->OBEnergy += SOEnergy;


    this->OBEnergy *= 0.5;

    // Compute many-body contribution to energy
    // *** These calls are safe as proper zeros are returned by
    // property engine ***
    this->MBEnergy = 
      this->template computeOBProperty<DENSITY_TYPE::SCALAR>(
          twoeH->S().pointer());

    if (twoeH->hasZ())
      this->MBEnergy +=
        this->template computeOBProperty<DENSITY_TYPE::MZ>(
            twoeH->Z().pointer());
    if (twoeH->hasXY()) {
      this->MBEnergy +=
        this->template computeOBProperty<DENSITY_TYPE::MY>(
            twoeH->Y().pointer());
      this->MBEnergy +=
        this->template computeOBProperty<DENSITY_TYPE::MX>(
            twoeH->X().pointer());
    }

    this->MBEnergy *= 0.25;

    // Assemble total energy
    this->totalEnergy = 
      this->OBEnergy + this->MBEnergy + this->molecule().nucRepEnergy
       + this->extraEnergy;

    // Sanity checks
    assert( not std::isnan(this->OBEnergy) );
    assert( not std::isnan(this->MBEnergy) );
    assert( not std::isinf(this->OBEnergy) );
    assert( not std::isinf(this->MBEnergy) );

  }; // SingleSlater<T>::computeEnergy

  template <typename MatsT, typename IntsT>
  std::vector<double> SingleSlater<MatsT,IntsT>::getEnergySummary() {
    std::vector<double> result = QuantumBase::getEnergySummary();
    result.push_back(this->molecule().nucRepEnergy);
    return result;
  };

  template <typename MatsT, typename IntsT>
  void SingleSlater<MatsT,IntsT>::computeMultipole(EMPerturbation &pert) {
    ROOT_ONLY(comm);
    // Compute elecric contribution to the dipoles
    for(auto iXYZ = 0; iXYZ < 3; iXYZ++) 
      this->elecDipole[iXYZ] = -this->template computeOBProperty<SCALAR>((*this->aoints.lenElectric)[iXYZ].pointer());


    // Nuclear contributions to the dipoles
    for(auto &atom : this->molecule().atoms){
    if (atom.quantum) continue;  
    MatAdd('N','N',3,1,1.,&this->elecDipole[0],3,atom.nucCharge,
        &atom.coord[0],3,&this->elecDipole[0],3);
    }
    // Electric contribution to the quadrupoles
    for(size_t iXYZ = 0, iX = 0; iXYZ < 3; iXYZ++)
    for(size_t jXYZ = iXYZ     ; jXYZ < 3; jXYZ++, iX++){

      this->elecQuadrupole[iXYZ][jXYZ] = -
        this->template computeOBProperty<SCALAR>((*this->aoints.lenElectric)[iX+3].pointer());
      
      this->elecQuadrupole[jXYZ][iXYZ] = this->elecQuadrupole[iXYZ][jXYZ]; 
    }
    
    // Nuclear contributions to the quadrupoles
    for(auto &atom : this->molecule().atoms){
    
    if (atom.quantum) continue;

    for(size_t iXYZ = 0; iXYZ < 3; iXYZ++)
    for(size_t jXYZ = 0; jXYZ < 3; jXYZ++) 
      this->elecQuadrupole[iXYZ][jXYZ] +=
        atom.nucCharge * atom.coord[iXYZ] * atom.coord[jXYZ];
    }
    // Electric contribution to the octupoles
    for(size_t iXYZ = 0, iX = 0; iXYZ < 3; iXYZ++)
    for(size_t jXYZ = iXYZ     ; jXYZ < 3; jXYZ++)
    for(size_t kXYZ = jXYZ     ; kXYZ < 3; kXYZ++, iX++){

      this->elecOctupole[iXYZ][jXYZ][kXYZ] = -
        this->template computeOBProperty<SCALAR>(
          (*this->aoints.lenElectric)[iX+9].pointer());

      this->elecOctupole[iXYZ][kXYZ][jXYZ] = this->elecOctupole[iXYZ][jXYZ][kXYZ]; 

      this->elecOctupole[jXYZ][iXYZ][kXYZ] = this->elecOctupole[iXYZ][jXYZ][kXYZ]; 

      this->elecOctupole[jXYZ][kXYZ][iXYZ] = this->elecOctupole[iXYZ][jXYZ][kXYZ]; 

      this->elecOctupole[kXYZ][iXYZ][jXYZ] = this->elecOctupole[iXYZ][jXYZ][kXYZ]; 

      this->elecOctupole[kXYZ][jXYZ][iXYZ] = this->elecOctupole[iXYZ][jXYZ][kXYZ]; 
    }

    // Nuclear contributions to the octupoles
    for(auto &atom : this->molecule().atoms){

    if (atom.quantum) continue;

    for(size_t iXYZ = 0; iXYZ < 3; iXYZ++)
    for(size_t jXYZ = 0; jXYZ < 3; jXYZ++)
    for(size_t kXYZ = 0; kXYZ < 3; kXYZ++)
      this->elecOctupole[iXYZ][jXYZ][kXYZ] +=
        atom.nucCharge * atom.coord[iXYZ] * atom.coord[jXYZ] *
        atom.coord[kXYZ];
    }
  };


  template <typename MatsT, typename IntsT>
  void SingleSlater<MatsT,IntsT>::computeSpin() {

    ROOT_ONLY(comm);

    this->SExpect[0] = 0.5 * this->template computeOBProperty<MX>(
      this->aoints.overlap->pointer());
    this->SExpect[1] = 0.5 * this->template computeOBProperty<MY>(
      this->aoints.overlap->pointer());
    this->SExpect[2] = 0.5 * this->template computeOBProperty<MZ>(
      this->aoints.overlap->pointer());

    if( not this->onePDM->hasZ() ) this->SSq = 0;
    else {
      size_t NB = this->basisSet().nBasis;
      MatsT * SCR  = this->memManager.template malloc<MatsT>(NB*NB);
      MatsT * SCR2 = this->memManager.template malloc<MatsT>(NB*NB);


      // SCR2 = S * D(S) * S
/*      
      blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::ConjTrans,NB,NB,NB,T(1.),aoints.overlap,NB,this->onePDM[SCALAR],NB,
        T(0.),SCR,NB);
      blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::ConjTrans,NB,NB,NB,T(1.),SCR,NB,aoints.overlap,NB,T(0.),SCR2,NB);
*/
      blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::NoTrans,NB,NB,NB,MatsT(1.),this->aoints.overlap->pointer(),NB,
           this->onePDM->S().pointer(),NB,MatsT(0.),SCR,NB);
      blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::ConjTrans,NB,NB,NB,MatsT(1.),this->aoints.overlap->pointer(),NB,
           SCR,NB,MatsT(0.),SCR2,NB);

      
      this->SSq = 3 * this->nO - (3./2.) * 
        this->template computeOBProperty<SCALAR>(SCR2);
  

      // SCR2 = D(Z) * S * D(Z)
      blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::NoTrans,NB,NB,NB,MatsT(1.),this->aoints.overlap->pointer(),NB,
           this->onePDM->Z().pointer(),NB,MatsT(0.),SCR,NB);
      blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::ConjTrans,NB,NB,NB,MatsT(1.),this->aoints.overlap->pointer(),NB,
           SCR,NB,MatsT(0.),SCR2,NB);
      
      this->SSq += 0.5 * this->template computeOBProperty<MZ>(SCR2);

      if( this->onePDM->hasXY() ) {
  
        // SCR2 = D(Y) * S * D(Y)
        blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::NoTrans,NB,NB,NB,MatsT(1.),this->aoints.overlap->pointer(),NB,
             this->onePDM->Y().pointer(),NB,MatsT(0.),SCR,NB);
        blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::ConjTrans,NB,NB,NB,MatsT(1.),this->aoints.overlap->pointer(),NB,
             SCR,NB,MatsT(0.),SCR2,NB);
        
        this->SSq += 0.5 * this->template computeOBProperty<MY>(SCR2);


        // SCR2 = D(X) * S * D(X)
        blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::NoTrans,NB,NB,NB,MatsT(1.),this->aoints.overlap->pointer(),NB,
             this->onePDM->X().pointer(),NB,MatsT(0.),SCR,NB);
        blas::gemm(blas::Layout::ColMajor,blas::Op::NoTrans,blas::Op::ConjTrans,NB,NB,NB,MatsT(1.),this->aoints.overlap->pointer(),NB,
             SCR,NB,MatsT(0.),SCR2,NB);
        
        this->SSq += 0.5 * this->template computeOBProperty<MX>(SCR2);

      }

      for(auto i = 0; i < 3; i++) 
        this->SSq += 4 * this->SExpect[i] * this->SExpect[i];

      this->SSq *= 0.25;

      this->memManager.free(SCR,SCR2);
    }

  };
  

}; // namespace ChronusQ

