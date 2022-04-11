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

#include <chronusq_sys.hpp>
#include <singleslater.hpp>
#include <matrix.hpp>

#include <fockbuilder/neofock.hpp>
#include <particleintegrals/twopints.hpp>
#include <particleintegrals/twopints/gtodirecttpi.hpp>
#include <particleintegrals/twopints/incore4indextpi.hpp>


namespace ChronusQ {

  // Pure virtual class for only interface functions
  struct NEOBase {
    virtual std::shared_ptr<SingleSlaterBase> getSubSSBase(std::string label) = 0;
    virtual std::vector<std::string> getLabels() = 0;
    virtual void setSubSetup() = 0;
  };

  template <typename MatsT, typename IntsT>
  class NEOSS: virtual public SingleSlater<MatsT,IntsT>, public NEOBase {

    template <typename MatsU, typename IntsU>
    friend class NEOSS;

    template <typename F>
    void applyToEach(F func) {
      if(order_.size() == subsystems.size()) {
        for(auto& label: order_) {
          func(subsystems.at(label));
        }
      }
      else {
        for(auto& system: subsystems)
          func(system.second);
      }
    }

    protected:

      // typedefs
      using SubSSPtr = std::shared_ptr<SingleSlater<MatsT,IntsT>>;

      template <typename T>
      using LabeledMap = std::unordered_map<std::string, T>;

      //
      // MAIN STORAGE
      //
      std::unordered_map<std::string,SubSSPtr> subsystems;

      // Optional order
      std::vector<std::string> order_;

      // Subsystem coulomb interactions
      // First map is the "external" particle and second map is the integrated
      //   particle.
      // EXAMPLE: interCoulomb["electron"]["proton"] is the coulomb matrix for
      //   the electronic subsystem (in the electronic basis) coming from the
      //   protonic coulomb potential.
      LabeledMap<LabeledMap<SquareMatrix<MatsT>>> interCoulomb;

      // Two particle integral objects (same storage scheme as above)
      // Boolean is contractSecond
      LabeledMap<LabeledMap<std::pair<bool, std::shared_ptr<TwoPInts<IntsT>>>>> interIntegrals;
      // Two particle gradient integrals
      LabeledMap<LabeledMap<std::pair<bool, std::shared_ptr<GradInts<TwoPInts,IntsT>>>>> gradInterInts;

      // Storage for FockBuilders (determines lifetime)
      LabeledMap<std::vector<std::shared_ptr<FockBuilder<MatsT,IntsT>>>> fockBuilders;

      // Storage for functionals (only for constructing new NEOSS)
      std::vector<std::shared_ptr<DFTFunctional>> functionals;

    public:

      // Main constructor
      // XXX: Does NOT construct electronic or protonic wavefunctions
      template <typename... Args>
      NEOSS(MPI_Comm c, CQMemManager &mem, Molecule &mol, BasisSet &basis,
                  Integrals<IntsT> &aoi, Args... args) :
        SingleSlater<MatsT,IntsT>(c,mem,mol,basis,aoi,args...),
        WaveFunctionBase(c,mem,mol,basis,args...),
        QuantumBase(c,mem,args...) { };

      // Copy/move constructors
      template <typename MatsU>
      NEOSS(const NEOSS<MatsU,IntsT>& other, int dummy = 0);

      template <typename MatsU>
      NEOSS(NEOSS<MatsU,IntsT>&& other, int dummy = 0);

      NEOSS(const NEOSS<MatsT,IntsT>& other) : NEOSS(other, 0) {};
      NEOSS(NEOSS<MatsT,IntsT>&& other) : NEOSS(other, 0) {};


      // Add a subsystem to the NEO object
      //
      // label  Unique name for this subsystem
      // ss     SingleSlater object representing this subsystem
      // ints   Two particle integrals for interacting with all other subsystems
      void addSubsystem(
        std::string label,
        std::shared_ptr<SingleSlater<MatsT,IntsT>> ss,
        const LabeledMap<std::pair<bool,std::shared_ptr<TwoPInts<IntsT>>>>& ints);

      void addGradientIntegrals(std::string label1, std::string label2,
        std::shared_ptr<GradInts<TwoPInts,IntsT>> ints, bool contractSecond);

      void setOrder(std::vector<std::string> labels) {
        order_ = labels;
      }

      // Getters
			std::vector<std::string> getLabels() {
				if (order_.size() == subsystems.size()) return order_;
				std::vector<std::string> labels;
				for(auto& entry:subsystems){
					labels.push_back(entry.first);
				}
				return labels;
			}
			
			std::pair<bool,std::shared_ptr<TwoPInts<IntsT>>> getCrossTPIs(std::string label1,std::string label2){
		  	return interIntegrals.at(label1).at(label2);
			}

      template <template <typename, typename> class T>
      std::shared_ptr<T<MatsT,IntsT>> getSubsystem(std::string label) {
        return std::dynamic_pointer_cast<T<MatsT,IntsT>>(subsystems.at(label));
      }
      std::shared_ptr<SingleSlaterBase> getSubSSBase(std::string label) {
        return std::dynamic_pointer_cast<SingleSlaterBase>(subsystems.at(label));
      }
      template <template <typename, typename> class T>
      std::vector<std::shared_ptr<T<MatsT,IntsT>>> getAllSubsystems() {
        std::vector<std::shared_ptr<T<MatsT,IntsT>>> results;
        applyToEach([&](SubSSPtr& ss){
          results.push_back(std::dynamic_pointer_cast<T<MatsT,IntsT>>(ss));
        });
        return results;
      }
      std::vector<std::shared_ptr<SingleSlaterBase>> getAllSubSSBase() {
        std::vector<std::shared_ptr<SingleSlaterBase>> results;
        applyToEach([&](SubSSPtr& ss){
          results.push_back(std::dynamic_pointer_cast<SingleSlaterBase>(ss));
        });
        return results;
      }

      const std::unordered_map<std::string, SubSSPtr>& getSubsystemMap() {
        return subsystems;
      }

      const std::vector<std::string>& getOrder() {
        return order_;
      }

      // Pass-through to each functions
      void saveCurrentState() {
        applyToEach([](SubSSPtr& ss){ ss->saveCurrentState(); });
      }

      void formGuess(const SingleSlaterOptions& ssopt) {
        applyToEach([&](SubSSPtr& ss){ ss->formGuess(ssopt); });
      }

      virtual void formFock(EMPerturbation& emPert, bool increment = false, double xHFX = 1.) {
        applyToEach([&](SubSSPtr& ss){ ss->formFock(emPert, increment, xHFX); });
      }

      void formCoreH(EMPerturbation& emPert, bool save) {
        applyToEach([&](SubSSPtr& ss){ ss->formCoreH(emPert, save); });
      }

      virtual void formDensity() {
        applyToEach([&](SubSSPtr& ss){ ss->formDensity(); });
      }

      // Propagate options that were set by value in the *Options functions
      void setSubSetup() {
        applyToEach([&](SubSSPtr& ss){
          ss->scfControls = this->scfControls;
          ss->savFile = this->savFile;
          ss->fchkFileName = this->fchkFileName;
        });

        subsystems["Protonic"]->scfControls.guess = this->scfControls.prot_guess;
      }

      std::vector<double> getGrad(EMPerturbation&, bool, bool);

      // Functions for ModifyOrbitals
      virtual void runModifyOrbitals(EMPerturbation&);
      virtual void buildModifyOrbitals();
      virtual void printProperties();
      virtual std::vector<std::shared_ptr<SquareMatrix<MatsT>>> getOnePDM();
      virtual std::vector<std::shared_ptr<SquareMatrix<MatsT>>> getFock();
      virtual std::vector<std::shared_ptr<Orthogonalization<MatsT>>> getOrtho();
      virtual double getTotalEnergy() { return this->totalEnergy; };


      // Properties
      void computeEnergy() {

        this->totalEnergy = 0.;
        applyToEach([&](SubSSPtr& ss){
          ss->computeEnergy(); 
          this->totalEnergy += ss->totalEnergy - this->molecule().nucRepEnergy;
        });

        // If we're doing EPC, we've double counted the energy, so subtract it
        //   for all non-electronic systems here...
        // XXX: If we ever have particle-particle correlation that isn't
        //   between electrons and nuclei, this will no longer work
        applyToEach([&](SubSSPtr& ss) {
          auto ks = std::dynamic_pointer_cast<KohnSham<MatsT,IntsT>>(ss);
          if( ss->particle.charge < 0 || !ks ) return;
          this->totalEnergy -= ks->XCEnergy;
        });

        this->totalEnergy += this->molecule().nucRepEnergy;

      }
    
      void computeMultipole(EMPerturbation& emPert) {
      // Zeroing our Dipole, Quadrupole, and Octopole
        for (auto iXYZ = 0; iXYZ < 3; iXYZ++) {

          this->elecDipole[iXYZ] = 0.;
          
          for (auto jXYZ = 0; jXYZ < 3; jXYZ++){

            this->elecQuadrupole[iXYZ][jXYZ] = 0.;

            for (auto kXYZ = 0; kXYZ < 3; kXYZ++){

              this->elecOctupole[iXYZ][jXYZ][kXYZ] = 0.;

            }
          }
        }

        applyToEach([&](SubSSPtr& ss){
      // Computing the Multipole for each subsystem and then adding to the overall multipoles
          ss->computeMultipole(emPert);
          for (auto iXYZ = 0; iXYZ < 3; iXYZ++) {
            this->elecDipole[iXYZ] += ss->elecDipole[iXYZ];
             
            for (auto jXYZ = 0; jXYZ < 3; jXYZ++){

              this->elecQuadrupole[iXYZ][jXYZ] += ss->elecQuadrupole[iXYZ][jXYZ];

              for (auto kXYZ = 0; kXYZ < 3; kXYZ++){

                this->elecOctupole[iXYZ][jXYZ][kXYZ] += ss->elecOctupole[iXYZ][jXYZ][kXYZ];

              }
            }
          }
        });
      // Nuclear contributions to the dipoles
        for(auto &atom : this->molecule().atoms){

          if (atom.quantum){
          continue;
          } 
          for (int iXYZ = 0; iXYZ < 3; iXYZ++){
            this->elecDipole[iXYZ] -= atom.nucCharge*atom.coord[iXYZ]*(subsystems.size()-1);
        }
        
        }

        // Nuclear contributions to the quadrupoles
        for(auto &atom : this->molecule().atoms){
          if (atom.quantum) {
            continue;
          }

        for(size_t iXYZ = 0; iXYZ < 3; iXYZ++)
        for(size_t jXYZ = 0; jXYZ < 3; jXYZ++) 
          this->elecQuadrupole[iXYZ][jXYZ] -=
             (subsystems.size()-1) * atom.nucCharge * atom.coord[iXYZ] * atom.coord[jXYZ];
        } 

        // Nuclear contributions to the octupoles
        for(auto &atom : this->molecule().atoms){
          
          if (atom.quantum){
            continue;
          }
     
        for(size_t iXYZ = 0; iXYZ < 3; iXYZ++)
        for(size_t jXYZ = 0; jXYZ < 3; jXYZ++)
        for(size_t kXYZ = 0; kXYZ < 3; kXYZ++)
          this->elecOctupole[iXYZ][jXYZ][kXYZ] -=
            (subsystems.size()-1) * atom.nucCharge * atom.coord[iXYZ] * atom.coord[jXYZ] *
            atom.coord[kXYZ];
        }
       
      };

      void computeSpin() { 
        applyToEach([](SubSSPtr& ss){ ss->computeSpin(); });      
      }

      void methodSpecificProperties() {
        applyToEach([](SubSSPtr& ss){ ss->methodSpecificProperties(); });      
      }

      // Overrides specific to a NEO-SCF
      /*
      void printSCFProg(std::ostream& out = std::cout, bool printDiff = true) {
        if( this->printLevel > 1 )
          for( auto& x: subsystems ) {
            out << "  " << std::setw(14) << std::left << x.first << " SCF: ";
            auto nIter = x.second->scfConv.nSCFIter;
            out << nIter << " iteration" << (nIter > 1 ? "s" : "") << "; ";
            out << "E = " << std::setw(14) << std::right << std::fixed;
            out << x.second->totalEnergy - this->molecule().nucRepEnergy;
            out << std::endl;
          }

        out << "  SCFIt: " << std::setw(6) << std::left;

        if( printDiff ) out << this->scfConv.nSCFMacroIter + 1;
        else            out << 0;

        // Current Total Energy
        out << std::setw(18) << std::fixed << std::setprecision(10)
                             << std::left << this->totalEnergy;

        if( printDiff ) {
          out << std::scientific << std::setprecision(7);
          out << std::setw(14) << std::right << this->scfConv.deltaEnergy;
          out << "   ";
        }
  
        out << std::endl;
      }

      bool evalConver(EMPerturbation& pert) {

        bool isConverged;

        // Compute all SCF convergence information on root process
        if( MPIRank(this->comm) == 0 ) {
          
          // Save copy of old energy
          double oldEnergy = this->totalEnergy;

          // Compute new energy
          this->computeProperties(pert);

          // Compute the difference between current and old energy
          this->scfConv.deltaEnergy = this->totalEnergy - oldEnergy;

          bool energyConv = std::abs(this->scfConv.deltaEnergy) < 
                            this->scfControls.eneConvTol;

          isConverged = energyConv;
        }

#ifdef CQ_ENABLE_MPI
        // Broadcast whether or not we're converged to ensure that all
        // MPI processes exit the NEO-SCF simultaneously
        if( MPISize(this->comm) > 1 ) MPIBCast(isConverged,0,this->comm);
#endif
        
        return isConverged;
      }

      void SCF(EMPerturbation& pert);
      */

      // Disable NR/stability for now
      MatsT* getNRCoeffs() {
        CErr("NR NYI for NEO!");
        return nullptr;
      }

      std::pair<double,MatsT*> getStab() {
        CErr("NR NYI for NEO!");
        return {0., nullptr};
      }
  };

}

//#include <singleslater/neoss/scf.hpp>

