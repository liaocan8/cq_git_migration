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

#include "scf.hpp"



// Li 6-31G(d) test
TEST( ROHF, Li_631Gd ) {

  CQSCFTEST( "scf/serial/rohf/li_6-31Gd_nodiis", "li_rohf_6-31Gd.bin.ref", 1e-7 );

};

// O2 6-31G(d) test
TEST( ROHF, O2_631Gd ) {

  CQSCFTEST( "scf/serial/rohf/oxygen_6-31Gd", "oxygen_rohf_6-31Gd.bin.ref" );

};

#ifdef _CQ_DO_PARTESTS

// SMP Li 6-31G(d) test
TEST( ROHF, PAR_Li_631Gd ) {

  CQSCFTEST( "scf/parallel/rohf/li_6-31Gd", "li_rohf_6-31Gd.bin.ref", 1e-7 );

};

// SMP O2 6-31G(d) test
TEST( ROHF, PAR_O2_631Gd ) {

  CQSCFTEST( "scf/parallel/rohf/oxygen_6-31Gd", "oxygen_rohf_6-31Gd.bin.ref" );

};

#endif




