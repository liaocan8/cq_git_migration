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

#include <util/math.hpp>
#include <detstringmanager.hpp>

namespace ChronusQ {

  enum DetScheme {
    CAS,
    RAS,
    GAS,
    GENERIC_DET
  };

  struct MOSpacePartition {
    // TODO: make variables for cases beyound CAS
    //Parameters for space partition

    DetScheme scheme = CAS;

    size_t nMO=0;     /// < Total Number of Molecular Orbitals
    size_t nInact=0;  /// < Number of Uncorrelated Core Orbitals
    size_t nFVirt=0;  /// < Number of Uncorrelated Virtual Orbtals
    size_t nFCore=0;  /// < Number of Frozen Core Orbitals (won't do rotations)
    size_t nDVirt=0;  /// < Number of Virtual being discarded

    size_t nCorrO=0;  /// < Total Number of Correlated Orbitals
    size_t nCorrE=0;  /// < Total Number of Correlated Electrons

    std::vector<char> orbIndices; /// defining types of orbitals

    std::vector<size_t> nActOs;  /// < Number of Correlated Orbitals in each Active Space
    std::vector<size_t> nActEs;  /// < Number of Correlated Electrons in each Active space

    // only for 1C
    size_t nCorrEA=0;                 /// < Number of Correlated Alpha Electrons
    size_t nCorrEB=0;                 /// < Number of Correlated Beta  Electrons
    std::vector<size_t> nActEAs; /// < Number of Correlated Alpha Electrons in each Active space
    std::vector<size_t> nActEBs; /// < Number of Correlated Beta  Electrons in each Active space

    // for RAS
    size_t mxHole=0;  /// < Maximum number of holes in RAS 1 space
    size_t mxElec=0;  /// < Maximum number of electrons in RAS 3 space
    std::vector<int> fCat;  /// < Category offset for RAS string

    MOSpacePartition() = default;
  };

  /**
   *  \brief The MCWaveFunctionBase class. The abstraction of information
   *  relating to the MCWaveFunction class which are independent of storage
   *  type.
   *
   *
   *  See WaveFunction for further docs.
   */
  class MCWaveFunctionBase {

  public:

    typedef std::vector<std::vector<int>> int_matrix;

    SafeFile savFile;    ///< Data File, for restart
    CQMemManager & memManager;
    MPI_Comm comm;

    MOSpacePartition MOPartition;

    size_t NDet;         ///  < Number of Determinants
    size_t NStates = 1;  ///  < Number of States
    bool FourCompNoPair = true; /// default as true if 4C

    // TODO: DetString now only works for CASCI String
    std::shared_ptr<DetStringManager> detStr     = nullptr;
    std::shared_ptr<DetStringManager> detStrBeta = nullptr; // only for 1C

    double InactEnergy;
    std::vector<double> StateEnergy;

    size_t NosS1 = 0; // number of initial states s1 for oscillator strength
    double * osc_str = nullptr; // matrix to save oscillator strength

    bool StateAverage = false;
    std::vector<double> SAWeight;

    MCWaveFunctionBase()                           = delete;
    MCWaveFunctionBase(const MCWaveFunctionBase &) = default;
    MCWaveFunctionBase(MCWaveFunctionBase &&)      = default;

    /**
     *  MCWaveFunctionBase Constructor. Constructs a WaveFunctionBase object
     *
     *  \param [in] NS Number of States constructed by MCWaveFunction
     */
    MCWaveFunctionBase(MPI_Comm c, CQMemManager &mem, size_t NS): 
      comm(c), memManager(mem), NStates(NS) {

      alloc();

    }; // MCWaveFunctionBase Constructor.

    ~MCWaveFunctionBase() { dealloc(); };

    void partitionMOSpace(std::vector<size_t>, size_t);
    void turnOnStateAverage(std::vector<double>);
    std::vector<int> genfCat(std::vector<std::vector<size_t>>,
      std::vector<std::vector<size_t>>, size_t, size_t);
    std::vector<int> genfCat(std::vector<std::vector<size_t>>, size_t);

    // Virtural Run function
    virtual void run(EMPerturbation &) = 0;
    virtual void setMORanges() = 0;
    virtual WaveFunctionBase & referenceWaveFunction() = 0;
    virtual void swapMOs(std::vector<std::vector<std::pair<size_t, size_t>>>&, SpinType) = 0;
    void setActiveSpaceAndReOrder();

    void alloc() {
      this->StateEnergy.clear();
      this->StateEnergy.resize(this->NStates, 0.);
    }

    void dealloc () { }

  }; // class MCWaveFunctionBase

}; // namespace ChronusQ
