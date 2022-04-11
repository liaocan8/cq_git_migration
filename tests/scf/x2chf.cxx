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

// Redudant primitive test with U^91+ one-electron system
TEST( X2CHF, U_91_plus_defBasis ) {

  CQSCFTEST( "scf/serial/x2c/U^91+_defBasis",
    "U^91+_defBasis.bin.ref",1e-6,
    false, false, false, false, false, true);

};

// Water 6-311+G(d,p) (Spherical) test
TEST( X2CHF, Water_6311pGdp_sph ) {

  CQSCFTEST( "scf/serial/x2c/water_6-311+Gdp_sph", 
    "water_6-311+Gdp_sph_x2c.bin.ref",1e-6 );
 
};

// Water 6-311+G(d,p) (Cartesian) test
TEST( X2CHF, Water_6311pGdp_cart ) {

  CQSCFTEST( "scf/serial/x2c/water_6-311+Gdp_cart", 
    "water_6-311+Gdp_cart_x2c.bin.ref",1e-6 );
 
};

// Hg SAPPORO DZP DKH_2012 SP
TEST( X2CHF, Hg_SAP_DZP_DKH3_2012_SP  ) {

  CQSCFTEST( "scf/serial/x2c/hg_sap_dz_dkh3_2012_sp", 
    "hg_sap_dz_dkh3_2012_sp.bin.ref",1e-6 );
 
};

// Cd SAPPORO DZP DKH_2012 SP
TEST( X2CHF, Cd_SAP_DZP_DKH3_2012_SP  ) {

  CQSCFTEST( "scf/serial/x2c/cd_sap_dz_dkh3_2012_sp", 
    "cd_sap_dz_dkh3_2012_sp.bin.ref",1e-6 );
 
};

// Cd SAPPORO DZP DKH_2012 SP with real UHF guess
TEST( X2CHF, Cd_SAP_DZP_DKH3_2012_SP_UHFGuess  ) {

  CQSCFTEST( "scf/serial/x2c/cd_sap_dz_dkh3_2012_sp_UHFGuess",
    "cd_sap_dz_dkh3_2012_sp_UHFGuess.bin.ref",1e-6,
    true, true, true, true, true, true, false,
    "cd_sap_dz_dkh3_2012_sp_UHF.scr.bin" );

};

// Ag2 sto-3g ALH X2C
TEST( X2CHF, Ag2_sto3g_ALH_X2C ) {

  CQSCFTEST( "scf/serial/x2c/Ag2_sto-3g_ALH",
    "Ag2_sto-3g_ALH.bin.ref",1e-6 );

};

// Ag2 sto-3g ALU X2C
TEST( X2CHF, Ag2_sto3g_ALU_X2C ) {

  CQSCFTEST( "scf/serial/x2c/Ag2_sto-3g_ALU",
    "Ag2_sto-3g_ALU.bin.ref",1e-6 );

};

// AgBr sto-3g DLH X2C
TEST( X2CHF, AgBr_sto3g_DLH_X2C ) {

  CQSCFTEST( "scf/serial/x2c/AgBr_sto-3g_DLH",
    "AgBr_sto-3g_DLH.bin.ref",1e-6 );

};

// AgBr sto-3g DLU X2C
TEST( X2CHF, AgBr_sto3g_DLU_X2C ) {

  CQSCFTEST( "scf/serial/x2c/AgBr_sto-3g_DLU",
    "AgBr_sto-3g_DLU.bin.ref",1e-6 );

};

// Ne(Z=10.5)He(Z=1.5)H(Z=0.5) sto-3g test
TEST( X2CHF, NeHeH_fracZ_sto3G ) {

  CQSCFTEST( "scf/serial/x2c/NeHeH_fracZ_x2chf_sto-3G", "NeHeH_fracZ_x2chf_sto-3G.bin.ref" );

};

// UPu 183+ fock-X2C test
TEST( X2CHF, UPu_183plus_DCGGS_fockX2C ) {

  CQSCFTEST( "scf/serial/x2c/UPu_183+_P_DCGGS_fockX2C",
             "UPu_183+_P_DCGGS_fockX2C.bin.ref",1e-8,
             false, false, false, false, false, true );

};

#ifdef _CQ_DO_PARTESTS

// SMP Water 6-311+G(d,p) (Spherical) test
TEST( X2CHF, PAR_Water_6311pGdp_sph ) {

  CQSCFTEST( "scf/parallel/x2c/water_6-311+Gdp_sph", 
    "water_6-311+Gdp_sph_x2c.bin.ref",1e-6 );
 
};

// Ag2 sto-3g ALH X2C libcint version
TEST( X2CHF, PAR_Ag2_sto3g_ALH_X2C ) {

  CQSCFTEST( "scf/parallel/x2c/Ag2_sto-3g_ALH",
    "Ag2_sto-3g_ALH.bin.ref",1e-6 );

};

// Ag2 sto-3g ALU X2C libcint version
TEST( X2CHF, PAR_Ag2_sto3g_ALU_X2C ) {

  CQSCFTEST( "scf/parallel/x2c/Ag2_sto-3g_ALU",
    "Ag2_sto-3g_ALU.bin.ref",1e-6 );

};

// AgBr sto-3g DLH X2C libcint version
TEST( X2CHF, PAR_AgBr_sto3g_DLH_X2C ) {

  CQSCFTEST( "scf/parallel/x2c/AgBr_sto-3g_DLH",
    "AgBr_sto-3g_DLH.bin.ref",1e-6 );

};

// AgBr sto-3g DLU X2C libcint version
TEST( X2CHF, PAR_AgBr_sto3g_DLU_X2C ) {

  CQSCFTEST( "scf/parallel/x2c/AgBr_sto-3g_DLU",
    "AgBr_sto-3g_DLU.bin.ref",1e-6 );

};

// UPu 183+ fock-X2C test
TEST( X2CHF, PAR_UPu_183plus_DCGGS_fockX2C ) {

  CQSCFTEST( "scf/parallel/x2c/UPu_183+_P_DCGGS_fockX2C",
             "UPu_183+_P_DCGGS_fockX2C.bin.ref",1e-8,
             false, false, false, false, false, true );

};

/*
// SMP Hg SAPPORO DZP DKH_2012 SP
TEST( X2CHF, PAR_Hg_SAP_DZP_DKH3_2012_SP  ) {

  CQSCFTEST( scf/parallel/x2c/hg_sap_dz_dkh3_2012_sp, 
    hg_sap_dz_dkh3_2012_sp.bin.ref );
 
};

// SMP Zn SAPPORO DZP DKH_2012 SP
TEST( X2CHF, PAR_Zn_SAP_DZP_DKH3_2012_SP  ) {

  CQSCFTEST( scf/parallel/x2c/zn_sap_dz_dkh3_2012_sp, 
    zn_sap_dz_dkh3_2012_sp.bin.ref );
 
};

// SMP Cd SAPPORO DZP DKH_2012 SP
TEST( X2CHF, PAR_Cd_SAP_DZP_DKH3_2012_SP  ) {

  CQSCFTEST( scf/parallel/x2c/cd_sap_dz_dkh3_2012_sp, 
    cd_sap_dz_dkh3_2012_sp.bin.ref );
 
};
*/


#endif



