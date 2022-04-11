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

#include <fields.hpp>
#include <molecule.hpp>
#include <molecularoptions.hpp>

namespace ChronusQ {

  struct Molecule;

  /**
   * \brief The GeometryModifier class
   */
  class GeometryModifier {

  protected:
    MolecularOptions molecularOptions_;

  public:

    double   electronicPotentialEnergy; ///< electronic potential energy

    // Constructors
    GeometryModifier() = delete;
    GeometryModifier(MolecularOptions molecularOptions):
      molecularOptions_(molecularOptions) {}

    // Different type
    GeometryModifier(const GeometryModifier &);
    GeometryModifier(GeometryModifier &&);

    // Virtual destructor
    virtual ~GeometryModifier() {}


    // Public member functions
    const MolecularOptions& getMolecularOptions() const {
      return molecularOptions_;
    }

    virtual bool hasNext() = 0;
    virtual void update(bool, Molecule&, bool) = 0;

  };

}
