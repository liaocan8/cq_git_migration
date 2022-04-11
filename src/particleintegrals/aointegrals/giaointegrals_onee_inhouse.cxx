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

#include <particleintegrals/inhouseaointegral.hpp>
#include <molecule.hpp>

namespace ChronusQ {

   
  void ComplexGIAOIntEngine::computecompFmT(
    dcomplex *FmT, dcomplex T, int maxM, int minM ){
 
    double critT = 33.0;
    dcomplex expT, factor, term, sum, twoT, Tn;
    std::vector<dcomplex> tempFmT;
    tempFmT.resize(maxM+1);

    if ( std::abs(T) <= 1.0e-10 ) {
      for ( int m = 0; m <= maxM ; m++ )
        tempFmT[m] = 1.0 / ( 2.0 * m + 1 );
    } else if ( std::abs(T) > critT ) {
      tempFmT[0] = 0.5 * sqrt( M_PI/T );
      twoT = 2.0*T;
      Tn = 1.0;
      for ( int mm = 1; mm <= maxM ; mm++ ) {
        Tn *= twoT;
        tempFmT[mm] = ( static_cast<dcomplex>(2*mm-1) )/twoT * tempFmT[mm-1] ;
      }  
    } else {
      expT = exp(-T);
      factor = maxM + 0.5;
      term = 0.5 / factor;
      sum = term;
      
      while ( std::abs( term ) > 1.0e-13 ) {
        factor += 1.0;
        term   *= T / factor;
        sum    += term;  
      }  
      tempFmT[maxM] = expT * sum;
      twoT = 2.0*T;
      for ( int m = maxM-1 ; m>= 0 ; m-- )
        tempFmT[m] = ( twoT * tempFmT[m+1] + expT )/ ( static_cast<dcomplex>( 2*m + 1 ) );
    } // else
    
    // copy the part we need.
    for ( int mm = minM, m = 0 ; mm <= maxM ; mm++, m++ )  {
      FmT[m] = tempFmT[mm] ;
    }

  } // void ComplexGIAOIntEngine::computecompFmT

   
  std::vector<std::vector<dcomplex>> ComplexGIAOIntEngine::computeGIAOOverlapS(
    libint2::ShellPair &pair, libint2::Shell &shell1,libint2::Shell &shell2, double *H ){ 

   
    int nElement = cart_ang_list[shell1.contr[0].l].size() 
                   * cart_ang_list[shell2.contr[0].l].size();
                    
    // evaluate the number of integral in the shell pair with cartesian gaussian
    std::vector<dcomplex> S_cartshell;

    dcomplex S;
    int lA[3],lB[3];
    
    double ka[3],kb[3];
    // here we define k as the direct phase factor without negative sign:
    // w = G* exp[ik dot (r-R)] for both bra and ket
    // ka (bra) = -1/2  A X H
    // kb (ket) = 1/2 B X H

/*
    for ( int i = 0 ; i < 3 ; i++ ){
      ka[i] = 
      kb[i] = 
    } 
*/
// std::cout<<"H "<<H[0]<<" "<<H[1]<<" "<<H[2]<<std::endl;

    ka[0] = - 0.5*( shell1.O[1]*H[2] - shell1.O[2]*H[1] );
    ka[1] = - 0.5*( shell1.O[2]*H[0] - shell1.O[0]*H[2] );
    ka[2] = - 0.5*( shell1.O[0]*H[1] - shell1.O[1]*H[0] );

    kb[0] = 0.5*( shell2.O[1]*H[2] - shell2.O[2]*H[1] );
    kb[1] = 0.5*( shell2.O[2]*H[0] - shell2.O[0]*H[2] );
    kb[2] = 0.5*( shell2.O[0]*H[1] - shell2.O[1]*H[0] );


//std::cout<<"ka "<<ka[0]<<"  "<<ka[1]<<"  "<<ka[2]<<std::endl;
//std::cout<<"kb "<<kb[0]<<"  "<<kb[1]<<"  "<<kb[2]<<std::endl;

    double K[3];
    for ( int mu = 0 ; mu < 3 ; mu++ ) 
      K[mu] = ka[mu] + kb[mu] ; 

    auto ss_shellpair = computecompOverlapss( pair,shell1,ka,shell2,kb );

//for ( auto sselement : ss_shellpair ){
//  std::cout<<"SS in a shellpair "<<std::setprecision(12)<<sselement<<std::endl;
//}

    for(int i = 0; i < cart_ang_list[shell1.contr[0].l].size() ; i++) 
    for(int j = 0; j < cart_ang_list[shell2.contr[0].l].size() ; j++){
      for(int k = 0; k < 3; k++){
        lA[k] = cart_ang_list[shell1.contr[0].l][i][k];
        lB[k] = cart_ang_list[shell2.contr[0].l][j][k];  
      }

       S = comphRRSab( pair, shell1, shell2, K, ss_shellpair,  
                       shell1.contr[0].l , lA , shell2.contr[0].l, lB ); 

// std::cout<<"S value"<<std::setprecision(12)<<S<<std::endl;

       S_cartshell.push_back(S);

      } // loop over ij 
        

    if ( ( not shell1.contr[0].pure ) and ( not shell2.contr[0].pure ) ) {  
      // if both sides are cartesian, return cartesian gaussian integrals

      return {S_cartshell};

    }
    
    std::vector<std::vector<dcomplex>> S_shellpair_sph(1);
  
    S_shellpair_sph[0].assign(((2*shell1.contr[0].l+1)*(2*shell2.contr[0].l+1)),0.0);
  
    cart2sph_complex_transform(shell1.contr[0].l,shell2.contr[0].l,
                       S_shellpair_sph[0],S_cartshell );
  
    return S_shellpair_sph; 
  

  } // computeGIAOOverlapS  
  

   
  std::vector<std::vector<dcomplex>> ComplexGIAOIntEngine::computeGIAOKineticT(
    libint2::ShellPair &pair, libint2::Shell &shell1,libint2::Shell &shell2, double *H ){ 

   
    // int nElement = cart_ang_list[shell1.contr[0].l].size() 
    //               * cart_ang_list[shell2.contr[0].l].size();
                    
    // evaluate the number of integral in the shell pair with cartesian gaussian
    std::vector<dcomplex> T_cartshell;

    dcomplex T;
    int lA[3],lB[3];
    
    double ka[3],kb[3];
    // here we define k as the direct phase factor without negative sign:
    // w = G* exp[ik dot (r-R)] for both bra and ket
    // ka (bra) = -1/2  A X H
    // kb (ket) = 1/2 B X H

    ka[0] = - 0.5*( shell1.O[1]*H[2] - shell1.O[2]*H[1] );
    ka[1] = - 0.5*( shell1.O[2]*H[0] - shell1.O[0]*H[2] );
    ka[2] = - 0.5*( shell1.O[0]*H[1] - shell1.O[1]*H[0] );

    kb[0] = 0.5*( shell2.O[1]*H[2] - shell2.O[2]*H[1] );
    kb[1] = 0.5*( shell2.O[2]*H[0] - shell2.O[0]*H[2] );
    kb[2] = 0.5*( shell2.O[0]*H[1] - shell2.O[1]*H[0] );


//std::cout<<"ka "<<ka[0]<<"  "<<ka[1]<<"  "<<ka[2]<<std::endl;
//std::cout<<"kb "<<kb[0]<<"  "<<kb[1]<<"  "<<kb[2]<<std::endl;

    double K[3];
    for ( int mu = 0 ; mu < 3 ; mu++ ) 
      K[mu] = ka[mu] + kb[mu] ; 

//std::cout<<"lA, lB    lA "<<shell1.contr[0].l<<"\t lB"<<shell2.contr[0].l<<"\t"<<std::endl;

    auto ss_shellpair = computecompOverlapss( pair,shell1,ka,shell2,kb );

    auto ssT_shellpair = computecompKineticss( pair, shell1, ka, shell2, kb, 
      ss_shellpair );

//std::cout<<"ssT finished"<<std::endl;
//for ( auto sselement : ssT_shellpair ){
//  std::cout<<"SS in a shellpair "<<std::setprecision(12)<<sselement<<std::endl;
//}
//std::cout<<"here we start T"<<std::endl;

    for(int i = 0; i < cart_ang_list[shell1.contr[0].l].size() ; i++) 
    for(int j = 0; j < cart_ang_list[shell2.contr[0].l].size() ; j++){
      for(int k = 0; k < 3; k++){
        lA[k] = cart_ang_list[shell1.contr[0].l][i][k];
        lB[k] = cart_ang_list[shell2.contr[0].l][j][k];  
      }



       T = compRRTab( pair, shell1, shell2, ka, kb, ss_shellpair, ssT_shellpair,  
                       shell1.contr[0].l , lA , shell2.contr[0].l, lB ); 
    //   T = compvRRTab( pair, shell1, shell2, K, ss_shellpair,  
    //                   shell1.contr[0].l , lA , shell2.contr[0].l, lB ); 

       T_cartshell.push_back(T);

      } // loop over ij 
        

    if ( ( not shell1.contr[0].pure ) and ( not shell2.contr[0].pure ) ) {  
      // if both sides are cartesian, return cartesian gaussian integrals

      return {T_cartshell};

    }
    
    std::vector<std::vector<dcomplex>> T_shellpair_sph(1);
  
    T_shellpair_sph[0].assign(((2*shell1.contr[0].l+1)*(2*shell2.contr[0].l+1)),0.0);
  
    cart2sph_complex_transform(shell1.contr[0].l,shell2.contr[0].l,
                       T_shellpair_sph[0],T_cartshell );
  
    return T_shellpair_sph; 
  

  } // computeGIAOKineticT  

  
   
  std::vector<std::vector<dcomplex>> ComplexGIAOIntEngine::computeGIAOAngularL(
    libint2::ShellPair &pair, libint2::Shell &shell1,libint2::Shell &shell2, double *H ){ 

   
    // int nElement = cart_ang_list[shell1.contr[0].l].size() 
    //               * cart_ang_list[shell2.contr[0].l].size();
                    
    // evaluate the number of integral in the shell pair with cartesian gaussian
    std::vector<std::vector<dcomplex>> L_cartshell(3);

    dcomplex L[3];
    int lA[3],lB[3];
    
    double ka[3],kb[3];
    // here we define k as the direct phase factor without negative sign:
    // w = G* exp[ik dot (r-R)] for both bra and ket
    // ka (bra) = -1/2  A X H
    // kb (ket) = 1/2 B X H

    ka[0] = - 0.5*( shell1.O[1]*H[2] - shell1.O[2]*H[1] );
    ka[1] = - 0.5*( shell1.O[2]*H[0] - shell1.O[0]*H[2] );
    ka[2] = - 0.5*( shell1.O[0]*H[1] - shell1.O[1]*H[0] );

    kb[0] = 0.5*( shell2.O[1]*H[2] - shell2.O[2]*H[1] );
    kb[1] = 0.5*( shell2.O[2]*H[0] - shell2.O[0]*H[2] );
    kb[2] = 0.5*( shell2.O[0]*H[1] - shell2.O[1]*H[0] );


//std::cout<<"ka "<<ka[0]<<"  "<<ka[1]<<"  "<<ka[2]<<std::endl;
//std::cout<<"kb "<<kb[0]<<"  "<<kb[1]<<"  "<<kb[2]<<std::endl;

    double K[3];
    for ( int mu = 0 ; mu < 3 ; mu++ ) 
      K[mu] = ka[mu] + kb[mu] ; 

//std::cout<<"lA, lB    lA "<<shell1.contr[0].l<<"\t lB"<<shell2.contr[0].l<<"\t"<<std::endl;

    auto ss_shellpair = computecompOverlapss( pair,shell1,ka,shell2,kb );

//  std::cout<<"SS in a shellpair "<<std::setprecision(12)<<sselement<<std::endl;
//}

    for(int i = 0; i < cart_ang_list[shell1.contr[0].l].size() ; i++) 
    for(int j = 0; j < cart_ang_list[shell2.contr[0].l].size() ; j++){
      for(int k = 0; k < 3; k++){
        lA[k] = cart_ang_list[shell1.contr[0].l][i][k];
        lB[k] = cart_ang_list[shell2.contr[0].l][j][k];  
      }

      for ( int mu = 0 ; mu < 3 ; mu++ ) {
        L[mu] = 0.0;
        auto pairindex = 0;
        for ( auto pripair : pair.primpairs ){

          L[mu] += compLabmu( pripair, shell1, shell2, ka,kb,ss_shellpair[pairindex],
            shell1.contr[0].l , lA , shell2.contr[0].l, lB,mu );

          pairindex += 1;
        } // for ( auto pripair : pair.primpairs )  
        
        L_cartshell[mu].push_back(L[mu]);
      } // for ( int mu )
      
    } // loop over ij 
        
    if ( ( not shell1.contr[0].pure ) and ( not shell2.contr[0].pure ) ) {  
      // if both sides are cartesian, return cartesian gaussian integrals

      return {L_cartshell};

    }
    
    std::vector<std::vector<dcomplex>> Angular_shellpair_sph(3);

    auto kk = 0;

    for ( auto cartmatrix : L_cartshell ) {
      Angular_shellpair_sph[kk].assign(((2*shell1.contr[0].l+1)*(2*shell2.contr[0].l+1)),0.0);
  
      cart2sph_complex_transform(shell1.contr[0].l,shell2.contr[0].l,
                       Angular_shellpair_sph[kk], cartmatrix );

      kk++;

    } // for ( auto cartmatrix : L_cartshell )  
  
    return Angular_shellpair_sph; 
  
  } // computeGIAOAngularL  




  /**
   *  \brief Computes a shell block of the electric dipole (length gauge) matrix.
   *
   *
   *  \param [in] pair    Shell pair data for shell1, shell2
   *  \param [in] shell1  Bra shell
   *  \param [in] shell2  Ket shell
   *
   *  \returns Shell block of the electric quadrupole matrix for (shell1 | shell2)
   */ 
   
  std::vector<std::vector<dcomplex>> ComplexGIAOIntEngine::computeGIAOEDipoleE1_len(
    libint2::ShellPair &pair, libint2::Shell &shell1,libint2::Shell &shell2, double *H ){ 

   
    // int nElement = cart_ang_list[shell1.contr[0].l].size() 
    //               * cart_ang_list[shell2.contr[0].l].size();
                    
    // evaluate the number of integral in the shell pair with cartesian gaussian
    std::vector<std::vector<dcomplex>> tmpED2(3);

    dcomplex D2[3];
    int lA[3],lB[3];
    
    double ka[3],kb[3];
    // here we define k as the direct phase factor without negative sign:
    // w = G* exp[ik dot (r-R)] for both bra and ket
    // ka (bra) = -1/2  A X H
    // kb (ket) = 1/2 B X H

    ka[0] = - 0.5*( shell1.O[1]*H[2] - shell1.O[2]*H[1] );
    ka[1] = - 0.5*( shell1.O[2]*H[0] - shell1.O[0]*H[2] );
    ka[2] = - 0.5*( shell1.O[0]*H[1] - shell1.O[1]*H[0] );

    kb[0] = 0.5*( shell2.O[1]*H[2] - shell2.O[2]*H[1] );
    kb[1] = 0.5*( shell2.O[2]*H[0] - shell2.O[0]*H[2] );
    kb[2] = 0.5*( shell2.O[0]*H[1] - shell2.O[1]*H[0] );


//std::cout<<"ka "<<ka[0]<<"  "<<ka[1]<<"  "<<ka[2]<<std::endl;
//std::cout<<"kb "<<kb[0]<<"  "<<kb[1]<<"  "<<kb[2]<<std::endl;

    double K[3];
    int munu[2];
    for ( int mu = 0 ; mu < 3 ; mu++ ) 
      K[mu] = ka[mu] + kb[mu] ; 

//std::cout<<"lA, lB    lA "<<shell1.contr[0].l<<"\t lB"<<shell2.contr[0].l<<"\t"<<std::endl;

    auto ss_shellpair = computecompOverlapss( pair,shell1,ka,shell2,kb );

//  std::cout<<"SS in a shellpair "<<std::setprecision(12)<<sselement<<std::endl;
//}

    for(int i = 0; i < cart_ang_list[shell1.contr[0].l].size() ; i++) 
    for(int j = 0; j < cart_ang_list[shell2.contr[0].l].size() ; j++){
      for(int k = 0; k < 3; k++){
        lA[k] = cart_ang_list[shell1.contr[0].l][i][k];
        lB[k] = cart_ang_list[shell2.contr[0].l][j][k];  
      }




        // q loop over 3 components of dipole. 
        for ( int q = 0 ; q < 3 ; q++ ) {

          D2[q] = compDipoleD1_len( pair, shell1, shell2, K, ss_shellpair, 
                    shell1.contr[0].l, lA, shell2.contr[0].l, lB ,q);
        } 
        for (auto p = 0 ; p<3 ; p++ ){
          tmpED2[p].push_back(D2[p]);
        } 
    } //for j



        
      
        
    if ( ( not shell1.contr[0].pure ) and ( not shell2.contr[0].pure ) ) {  
      // if both sides are cartesian, return cartesian gaussian integrals

      return tmpED2;

    }
    
    std::vector<std::vector<dcomplex>> ED_shellpair_sph(3);

    auto kk = 0;

    for ( auto cartmatrix : tmpED2 ) {
      ED_shellpair_sph[kk].assign(((2*shell1.contr[0].l+1)*(2*shell2.contr[0].l+1)),0.0);
  
      cart2sph_complex_transform(shell1.contr[0].l,shell2.contr[0].l,
                       ED_shellpair_sph[kk], cartmatrix );

      kk++;

    } // for ( auto cartmatrix : tmpED2 )  
  
    return ED_shellpair_sph; 
  
  } // computeGIAOEDipoleE1_len  




  /**
   *  \brief Computes a shell block of the electric quadrupole (length gauge) matrix.
   *
   *
   *  \param [in] pair    Shell pair data for shell1, shell2
   *  \param [in] shell1  Bra shell
   *  \param [in] shell2  Ket shell
   *
   *  \returns Shell block of the electric quadrupole matrix for (shell1 | shell2)
   */ 
   
  std::vector<std::vector<dcomplex>> ComplexGIAOIntEngine::computeGIAOEQuadrupoleE2_len(
    libint2::ShellPair &pair, libint2::Shell &shell1,libint2::Shell &shell2, double *H ){ 

   
    // int nElement = cart_ang_list[shell1.contr[0].l].size() 
    //               * cart_ang_list[shell2.contr[0].l].size();
                    
    // evaluate the number of integral in the shell pair with cartesian gaussian
    std::vector<std::vector<dcomplex>> tmpEQ2(6);

    dcomplex E2[6];
    int lA[3],lB[3];
    
    double ka[3],kb[3];
    // here we define k as the direct phase factor without negative sign:
    // w = G* exp[ik dot (r-R)] for both bra and ket
    // ka (bra) = -1/2  A X H
    // kb (ket) = 1/2 B X H

    ka[0] = - 0.5*( shell1.O[1]*H[2] - shell1.O[2]*H[1] );
    ka[1] = - 0.5*( shell1.O[2]*H[0] - shell1.O[0]*H[2] );
    ka[2] = - 0.5*( shell1.O[0]*H[1] - shell1.O[1]*H[0] );

    kb[0] = 0.5*( shell2.O[1]*H[2] - shell2.O[2]*H[1] );
    kb[1] = 0.5*( shell2.O[2]*H[0] - shell2.O[0]*H[2] );
    kb[2] = 0.5*( shell2.O[0]*H[1] - shell2.O[1]*H[0] );


//std::cout<<"ka "<<ka[0]<<"  "<<ka[1]<<"  "<<ka[2]<<std::endl;
//std::cout<<"kb "<<kb[0]<<"  "<<kb[1]<<"  "<<kb[2]<<std::endl;

    double K[3];
    int munu[2];
    for ( int mu = 0 ; mu < 3 ; mu++ ) 
      K[mu] = ka[mu] + kb[mu] ; 

//std::cout<<"lA, lB    lA "<<shell1.contr[0].l<<"\t lB"<<shell2.contr[0].l<<"\t"<<std::endl;

    auto ss_shellpair = computecompOverlapss( pair,shell1,ka,shell2,kb );

//  std::cout<<"SS in a shellpair "<<std::setprecision(12)<<sselement<<std::endl;
//}

    for(int i = 0; i < cart_ang_list[shell1.contr[0].l].size() ; i++) 
    for(int j = 0; j < cart_ang_list[shell2.contr[0].l].size() ; j++){
      for(int k = 0; k < 3; k++){
        lA[k] = cart_ang_list[shell1.contr[0].l][i][k];
        lB[k] = cart_ang_list[shell2.contr[0].l][j][k];  
      }



      /* the ordering of electric quadrupole is 
                 alpha     beta
                  0          0
                  0          1
                  0          2
                  1          1
                  1          2
                  2          2
       */

        // q loop over 6 components of quadrupole. 
        // the ordering of angular momentum in L==2:
        /*
           *    x   y   z
           *    2   0   0
           *    1   1   0 
           *    1   0   1
           *    0   2   0
           *    0   1   1
           *    0   0   2
        */
        for ( int q = 0 ; q < cart_ang_list[2].size() ; q++ ) {
          int totalL = 0;
          for ( int qelement = 0 ; qelement < 3 ; qelement++ ){
            if ( cart_ang_list[2][q][qelement] == 2 ) {
              munu[0] = qelement;
              munu[1] = qelement;
              totalL += 2;
            } else if ( cart_ang_list[2][q][qelement] == 1 ) {
              munu[totalL] = qelement;
              totalL++;
            }  
          } // for qelement

          if ( totalL!= 2 ) std::cerr<<"quadrupole wrong!!"<<std::endl;
          E2[q] = compQuadrupoleE2_len( pair, shell1, shell2, K, ss_shellpair, 
                    shell1.contr[0].l, lA, shell2.contr[0].l, lB ,munu[0],munu[1]);
        } 
        for (auto p = 0 ; p<6 ; p++ ){
          tmpEQ2[p].push_back(E2[p]);
        } 
    } //for j



        
      
        
    if ( ( not shell1.contr[0].pure ) and ( not shell2.contr[0].pure ) ) {  
      // if both sides are cartesian, return cartesian gaussian integrals

      return tmpEQ2;

    }
    
    std::vector<std::vector<dcomplex>> EQ_shellpair_sph(6);

    auto kk = 0;

    for ( auto cartmatrix : tmpEQ2 ) {
      EQ_shellpair_sph[kk].assign(((2*shell1.contr[0].l+1)*(2*shell2.contr[0].l+1)),0.0);
  
      cart2sph_complex_transform(shell1.contr[0].l,shell2.contr[0].l,
                       EQ_shellpair_sph[kk], cartmatrix );

      kk++;

    } // for ( auto cartmatrix : tmpEQ2 )  
  
    return EQ_shellpair_sph; 
  
  } // computeGIAOEQuadrupoleE2_len  


  /**
   *  \brief Computes a shell block of the electric octupole (length gauge) matrix.
   *
   *
   *  \param [in] pair    Shell pair data for shell1, shell2
   *  \param [in] shell1  Bra shell
   *  \param [in] shell2  Ket shell
   *
   *  \returns Shell block of the electric quadrupole matrix for (shell1 | shell2)
   */ 
   
  std::vector<std::vector<dcomplex>> ComplexGIAOIntEngine::computeGIAOEOctupoleE3_len(
    libint2::ShellPair &pair, libint2::Shell &shell1,libint2::Shell &shell2, double *H ){ 

   
    std::vector<std::vector<dcomplex>> tmpEO3(10);

    dcomplex E3[10];
    int lA[3],lB[3];
    
    double ka[3],kb[3];
    // here we define k as the direct phase factor without negative sign:
    // w = G* exp[ik dot (r-R)] for both bra and ket
    // ka (bra) = -1/2  A X H
    // kb (ket) = 1/2 B X H

    ka[0] = - 0.5*( shell1.O[1]*H[2] - shell1.O[2]*H[1] );
    ka[1] = - 0.5*( shell1.O[2]*H[0] - shell1.O[0]*H[2] );
    ka[2] = - 0.5*( shell1.O[0]*H[1] - shell1.O[1]*H[0] );

    kb[0] = 0.5*( shell2.O[1]*H[2] - shell2.O[2]*H[1] );
    kb[1] = 0.5*( shell2.O[2]*H[0] - shell2.O[0]*H[2] );
    kb[2] = 0.5*( shell2.O[0]*H[1] - shell2.O[1]*H[0] );


//std::cout<<"ka "<<ka[0]<<"  "<<ka[1]<<"  "<<ka[2]<<std::endl;
//std::cout<<"kb "<<kb[0]<<"  "<<kb[1]<<"  "<<kb[2]<<std::endl;

    double K[3];
    int alphabetagamma[3];
    for ( int mu = 0 ; mu < 3 ; mu++ ) 
      K[mu] = ka[mu] + kb[mu] ; 

//std::cout<<"lA, lB    lA "<<shell1.contr[0].l<<"\t lB"<<shell2.contr[0].l<<"\t"<<std::endl;

    auto ss_shellpair = computecompOverlapss( pair,shell1,ka,shell2,kb );

//  std::cout<<"SS in a shellpair "<<std::setprecision(12)<<sselement<<std::endl;
//}

    for(int i = 0; i < cart_ang_list[shell1.contr[0].l].size() ; i++) 
    for(int j = 0; j < cart_ang_list[shell2.contr[0].l].size() ; j++){
      for(int k = 0; k < 3; k++){
        lA[k] = cart_ang_list[shell1.contr[0].l][i][k];
        lB[k] = cart_ang_list[shell2.contr[0].l][j][k];  
      }


/*
 * orderring of octupole
   alpha  beta  gamma
   0      0     0
   0      0     1
   0      0     2
   0      1     1 
   0      1     2
   0      2     2
   1      1     1
   1      1     2 
   1      2     2
   2      2     2
 */

        for ( int q = 0 ; q < cart_ang_list[3].size() ; q++ ) {
          int totalL = 0;
          for ( int qelement = 0 ; qelement < 3 ; qelement++ ){
            if ( cart_ang_list[3][q][qelement] == 3 ) {
              alphabetagamma[0] = qelement;
              alphabetagamma[1] = qelement;
              alphabetagamma[2] = qelement;
              totalL += 3;
            } else if ( cart_ang_list[3][q][qelement] == 2 ) {
              alphabetagamma[totalL] = qelement;
              totalL++;
              alphabetagamma[totalL] = qelement;
              totalL++;
            } else if ( cart_ang_list[3][q][qelement] == 1 ) {
              alphabetagamma[totalL] = qelement;
              totalL++;
            } 
          } // for qelement   

// std::cerr<<" alpha = "<<alphabetagamma[0]<<" beta = "<<alphabetagamma[1]<<" gamma = "<<alphabetagamma[2]<<std::endl;

          if ( totalL!= 3 ) std::cerr<<"octupole wrong!!"<<std::endl;
          E3[q] = compOctupoleE3_len( pair, shell1, shell2, K, ss_shellpair, 
                    shell1.contr[0].l, lA, shell2.contr[0].l, lB ,alphabetagamma[0],
                    alphabetagamma[1], alphabetagamma[2] );
        } // for q

        for (auto p = 0 ; p<10 ; p++ ){
          tmpEO3[p].push_back(E3[p]);
        } 
    } //for j



        
      
        
    if ( ( not shell1.contr[0].pure ) and ( not shell2.contr[0].pure ) ) {  
      // if both sides are cartesian, return cartesian gaussian integrals

      return tmpEO3;

    }
    
    std::vector<std::vector<dcomplex>> EO3_shellpair_sph(10);

    auto kk = 0;

    for ( auto cartmatrix : tmpEO3 ) {
      EO3_shellpair_sph[kk].assign(((2*shell1.contr[0].l+1)*(2*shell2.contr[0].l+1)),0.0);
  
      cart2sph_complex_transform(shell1.contr[0].l,shell2.contr[0].l,
                       EO3_shellpair_sph[kk], cartmatrix );

      kk++;

    } // for ( auto cartmatrix : tmpEO3 )  
  
    return EO3_shellpair_sph; 
  
  } // computeGIAOEOctupoleE3_len  






  /**
   *  \brief Computes a shell block of the complex nuclear potential matrix.
   *
   *
   *  \param [in] nucShell nuclear shell, give the exponents of gaussian function of nuclei
   *  \param [in] pair    Shell pair data for shell1, shell2
   *  \param [in] shell1  Bra shell
   *  \param [in] shell2  Ket shell
   *
   *  \returns Shell block of the potential integral matrix for (shell1 | shell2)
   */ 
   
  std::vector<std::vector<dcomplex>> ComplexGIAOIntEngine::computeGIAOPotentialV(
    const std::vector<libint2::Shell> &nucShell, libint2::ShellPair &pair, 
    libint2::Shell &shell1 , libint2::Shell &shell2, double *H, const Molecule& molecule){
  

    bool useFiniteWidthNuclei = nucShell.size() > 0;


    std::vector<dcomplex> potential_shellpair;
    dcomplex V,tmpV;
    int lA[3],lB[3];

    double ka[3],kb[3];

    // here calculate the phase factor in LONDON orbital

    ka[0] = - 0.5*( shell1.O[1]*H[2] - shell1.O[2]*H[1] );
    ka[1] = - 0.5*( shell1.O[2]*H[0] - shell1.O[0]*H[2] );
    ka[2] = - 0.5*( shell1.O[0]*H[1] - shell1.O[1]*H[0] );

    kb[0] = 0.5*( shell2.O[1]*H[2] - shell2.O[2]*H[1] );
    kb[1] = 0.5*( shell2.O[2]*H[0] - shell2.O[0]*H[2] );
    kb[2] = 0.5*( shell2.O[0]*H[1] - shell2.O[1]*H[0] );

    double K[3]; 
    for ( int mu = 0 ; mu < 3 ; mu++ ) 
      K[mu] = ka[mu] + kb[mu] ; 

    // here calculate the ss overlap integral 
    auto ss_shellpair = computecompOverlapss( pair,shell1,ka,shell2,kb ); 
    // here calculate the ss potential integral
    // auto ssV_shellpair = computecompPotentialss( pair,shell1,ka,shell2,kb );  

    for(int i = 0; i < cart_ang_list[shell1.contr[0].l].size() ; i++) 
    for(int j = 0; j < cart_ang_list[shell2.contr[0].l].size() ; j++){
      for(int k = 0; k < 3; k++){
        lA[k] = cart_ang_list[shell1.contr[0].l][i][k];
        lB[k] = cart_ang_list[shell2.contr[0].l][j][k];  
      };
      V = 0.0;
  
      V = comphRRVab(nucShell,pair,shell1,shell2,K,ss_shellpair,shell1.contr[0].l,lA,shell2.contr[0].l,lB,molecule);
        // calculate from comphRRVab
      
      // int iAtom;
  /*
      tmpV = 0.0;
      for ( auto pripair : pair.primpairs ){
        iAtom = 0;
        for ( auto atom : molecule_.atoms ){
          double C[3];
          for ( int mu = 0 ; mu < 3 ; mu++ ) C[mu] = atom.coord[mu];
          auto norm = shell1.contr[0].coeff[pripair.p1]* 
                      shell2.contr[0].coeff[pripair.p2];
  
          tmpV+= atom.atomicNumber * norm *
                 hRRiPPVab(pripair,shell1,shell2,shell1.contr[0].l,lA,shell2.contr[0].l,lB,C,0,iAtom);
          iAtom++;
        }  
      }
      V = tmpV;
  */  // use hRRiPPVab to calculate 
      potential_shellpair.push_back(-V);
  
    } // for j
   
    if ( ( not shell1.contr[0].pure ) and ( not shell2.contr[0].pure ) ) {  
      // if both sides are cartesian, return cartesian gaussian integrals
      return {potential_shellpair};
    }
 
    std::vector<dcomplex> V_shellpair_sph;
  
    V_shellpair_sph.assign(((2*shell1.contr[0].l+1)*(2*shell2.contr[0].l+1)),0.0);
  
    cart2sph_complex_transform(shell1.contr[0].l,shell2.contr[0].l,
                       V_shellpair_sph,potential_shellpair );
  
    return { V_shellpair_sph }; 
  
  } // computeGIAOPotentialV


  // compute uncontracted overlap of (s||s) type for a shellpair
   
  std::vector<dcomplex> ComplexGIAOIntEngine::computecompOverlapss(
    libint2::ShellPair &pair, libint2::Shell &shell1, double *ka, libint2::Shell &shell2, double *kb ) {

    std::vector<dcomplex> ss_shellpair;
    dcomplex onei;
    onei.real(0);
    onei.imag(1);
    for( auto &pripair : pair.primpairs ) { 

      dcomplex norm;
      dcomplex tmpVal;
      norm = shell1.contr[0].coeff[pripair.p1]* shell2.contr[0].coeff[pripair.p2];
  
      double realpart=0.0;
      for ( int mu = 0 ; mu < 3 ; mu++ ) {
        realpart -= pow( (ka[mu]+kb[mu]), 2 );
      } // for mu 

      realpart *= 0.25*pripair.one_over_gamma; 

      double imagpart=0.0;
      for ( int mu = 0 ; mu < 3 ; mu++ ) {
        imagpart += ka[mu]*(pripair.P[mu] - shell1.O[mu]) 
                    + kb[mu]*(pripair.P[mu] - shell2.O[mu]);
      }  // for mu 

      dcomplex z = realpart + imagpart*onei;   

      tmpVal = norm * exp ( z ) *
                  pow(sqrt(M_PI),3) * sqrt(pripair.one_over_gamma)*pripair.K ;

      ss_shellpair.push_back(tmpVal);
    }
    return ss_shellpair;

  } // computecompOverlapss

  //compute uncontracted Kinetic of (s||s) for a shellpair
   
  std::vector<dcomplex> ComplexGIAOIntEngine::computecompKineticss( libint2::ShellPair &pair,
    libint2::Shell &shell1, double *ka, libint2::Shell &shell2, double *kb, 
    std::vector<dcomplex> &ss_shellpair ) {

    std::vector<dcomplex> ssT_shellpair;
    dcomplex tmpVal ; 
    dcomplex onei;
    onei.real(0);
    onei.imag(1);

    double ABsquare = 0.0 ; 
    for ( int mu = 0 ; mu<3 ; mu++ ) ABsquare += pow( pair.AB[mu],2 );

    int counter_shellpair = 0;
    for( auto &pripair : pair.primpairs ) { 
      // here norm is included in ssS. 
      auto Xi = shell1.alpha[pripair.p1] * shell2.alpha[pripair.p2]
                  *pripair.one_over_gamma;

      auto Ksquare = 0.0;
      for ( int mu = 0 ; mu<3 ; mu++ ) {
        Ksquare += pow( ( ka[mu]*shell2.alpha[pripair.p2]
          -kb[mu]*shell1.alpha[pripair.p1] ) * pripair.one_over_gamma , 2 );
      } // for ( int mu = 0 ; mu<3 ; mu++ )
      
      double realpart = 0.0 ; 
      realpart += 0.5*Ksquare + 3*Xi -2*pow(Xi,2)*ABsquare; 

      double ABdotK = 0.0;
      for ( int mu = 0 ; mu<3 ; mu++ ) {
        ABdotK += pair.AB[mu] * ( ka[mu]*shell2.alpha[pripair.p2] 
          -kb[mu]*shell1.alpha[pripair.p1] );
      } // for mu
      double imagpart = -2.0 * Xi * pripair.one_over_gamma * ABdotK ;       
      dcomplex z = realpart + imagpart*onei ; 
      tmpVal = z * ss_shellpair[counter_shellpair];
      ssT_shellpair.push_back(tmpVal);
      counter_shellpair++;
    } // for( auto &pripair : pair.primpairs )

    return ssT_shellpair;
 
  } // ComplexGIAOIntEngine::computecompKineticss



  // here the complex recursion start
  //
  //------------------------------------//
  // overlap horizontal recursion       //
  // (a|b) = (A-B)(a|b-1) + (a+1|b-1)   //
  //------------------------------------//

  /**
   *  \brief Perform the horizontal recurrence relation for the contracted overlap integral
   *
   *  (a|b) = (A-B)(a|b-1) + (a+1|b-1)
   *
   *  where a,b are the angular momentum, A,B are the nuclear coordinates.
   *
   *  \param [in] pair    Shell pair data for shell1, shell2
   *  \param [in] shell1  Bra shell
   *  \param [in] shell2  Ket shell
   *  \param [in] LA      total Bra angular momentum
   *  \param [in] lA      Bra angular momentum vector (lAx,lAy,lAz)
   *  \param [in] LB      total Ket angular momentum
   *  \param [in] lB      Ket angular momentum vector (lBx,lBy,lBz)
   *
   *  \returns a contracted overlap integral
   *
   */ 
   dcomplex ComplexGIAOIntEngine::comphRRSab(libint2::ShellPair &pair, libint2::Shell &shell1,
    libint2::Shell &shell2, double *K, std::vector<dcomplex> &ss_shellpair, 
    int LA, int *lA ,int LB, int *lB) {
      
    int iWork,lAp1[3],lBm1[3];
    dcomplex tmpVal = 0.0;
     
    if ( LB > LA ) {     // if LB>LA, use horizontal recursion to make LA>LB. 

      for(iWork = 0 ; iWork < 3 ; iWork++ ){
        lAp1[iWork] = lA[iWork];     
        lBm1[iWork] = lB[iWork];     
      };
    
      if ( lB[0] > 0 )      iWork = 0;
      else if ( lB[1] > 0 ) iWork = 1;
      else if ( lB[2] > 0 ) iWork = 2;
      lAp1[iWork]++;
      lBm1[iWork]--;
   
      tmpVal = comphRRSab( pair , shell1, shell2, K, ss_shellpair, 
                 LA+1 , lAp1 , LB-1 , lBm1 );
      tmpVal+= pair.AB[iWork]*comphRRSab( pair , shell1, shell2, K, 
                                          ss_shellpair, LA , lA , LB-1 , lBm1 );
      return tmpVal;
    
    }  // if ( LB > LA )
   
    if(LA == 0) {
      // (s|s)
      auto pripairindex = 0;
      for( auto &pripair : pair.primpairs ) { 
        tmpVal += ss_shellpair[pripairindex];
        pripairindex++;

/*
        double norm;
        norm = shell1.contr[0].coeff[pripair.p1]* shell2.contr[0].coeff[pripair.p2];
    
        double realpart=0.0;
        for ( int mu = 0 ; mu < 3 ; mu++ ) {
          realpart -= pow( (ka[mu]+kb[mu]), 2 );
        } // for mu 
        realpart *= 0.25*pripair.one_over_gamma; 
 
        double imagpart=0.0;
        for ( int mu = 0 ; mu < 3 ; mu++ ) {
          imagpart += ka[mu]*(pripair.P[mu] - shell1.O[mu]) 
                      + kb[mu]*(pripair.P[mu] - shell2.O[mu]);
        }  // for mu 

        dcomplex z = ( realpart , imagpart );   
        
        tmpVal += norm * exp ( z ) *
                  pow(sqrt(M_PI),3) * sqrt(pripair.one_over_gamma)*pripair.K ;
*/

      }  // for pripair   
      return tmpVal;
    }  else if(LB == 0) {
      // (|s)
      auto pripairindex = 0;
      for( auto pripair : pair.primpairs ) {
      
      //tmpVal+= shell1.contr[0].coeff[pripair.p1]* shell2.contr[0].coeff[pripair.p2]*


        tmpVal +=
                  compvRRSa0( pripair, shell1, K, ss_shellpair[pripairindex], LA, lA );

        pripairindex++;

      } // for pripair 
      return tmpVal;
    }; // else if(LB == 0)

    // here LB > 0 

    for(iWork = 0;iWork < 3;iWork++){
      lAp1[iWork]=lA[iWork];     
      lBm1[iWork]=lB[iWork];     
    };
    if (lB[0] > 0)      iWork = 0;
    else if (lB[1] > 0) iWork=1;
    else if (lB[2] > 0) iWork=2;
    lAp1[iWork]++;
    lBm1[iWork]--;
  
  
    tmpVal = comphRRSab(pair,shell1,shell2,K,ss_shellpair,LA+1,lAp1,LB-1,lBm1);
    tmpVal+= (shell1.O[iWork]-shell2.O[iWork])
             *comphRRSab(pair,shell1,shell2,K,ss_shellpair,LA,lA,LB-1,lBm1);
  //  tmpVal+= pair.AB[iWork]*hRRSab(pair,shell1,shell2,LA,lA,LB-1,lBm1);
    return tmpVal;
   
  }  // comphRRSab  



  //
  //------------------------------------//
  // overlap horizontal recursion iPP   //
  // (a|b) = (A-B)(a|b-1) + (a+1|b-1)   //
  //------------------------------------//

  /**
   *  \brief Perform the horizontal recurrence relation for the contracted overlap integral
   *
   *  (a|b) = (A-B)(a|b-1) + (a+1|b-1)
   *
   *  where a,b are the angular momentum, A,B are the nuclear coordinates.
   *
   *  \param [in] pair    Shell pair data for shell1, shell2
   *  \param [in] shell1  Bra shell
   *  \param [in] shell2  Ket shell
   *  \param [in] LA      total Bra angular momentum
   *  \param [in] lA      Bra angular momentum vector (lAx,lAy,lAz)
   *  \param [in] LB      total Ket angular momentum
   *  \param [in] lB      Ket angular momentum vector (lBx,lBy,lBz)
   *
   *  \returns a contracted overlap integral
   *
   */ 
   dcomplex ComplexGIAOIntEngine::comphRRiPPSab(libint2::ShellPair::PrimPairData &pripair, 
    libint2::Shell &shell1, libint2::Shell &shell2, double *K, 
    dcomplex sspri, int LA, int *lA ,int LB, int *lB) {
      
    int iWork,lAp1[3],lBm1[3];
    dcomplex tmpVal = 0.0 ;
     
    if ( LB > LA ) {     // if LB>LA, use horizontal recursion to make LA>=LB. 

      for( int mu = 0 ; mu < 3 ; mu++ ){
        lAp1[mu] = lA[mu];     
        lBm1[mu] = lB[mu];     
      } // for mu
    
      if ( lB[0] > 0 )      iWork = 0;
      else if ( lB[1] > 0 ) iWork = 1;
      else if ( lB[2] > 0 ) iWork = 2;
      lAp1[iWork]++;
      lBm1[iWork]--;
   
      tmpVal = comphRRiPPSab( pripair , shell1, shell2, K, sspri, 
                 LA+1 , lAp1 , LB-1 , lBm1 );
      tmpVal+= (shell1.O[iWork]-shell2.O[iWork]) 
                 * comphRRiPPSab( pripair , shell1, shell2, K, 
                     sspri, LA , lA , LB-1 , lBm1 );

      return tmpVal;
    
    } // if ( LB > LA )  
   
    if(LA == 0) {
      // (s||s)
      tmpVal = sspri;

      return tmpVal;

    }  else if(LB == 0) {
      // (|s)
      
      tmpVal += compvRRSa0( pripair, shell1, K, sspri, LA, lA );

      return tmpVal;
    }; // else if(LB == 0)

    // here LB > 0 

    for( iWork = 0; iWork < 3; iWork++ ){
      lAp1[iWork]=lA[iWork];     
      lBm1[iWork]=lB[iWork];     
    } // for iWork

    if (lB[0] > 0)      iWork = 0;
    else if (lB[1] > 0) iWork=1;
    else if (lB[2] > 0) iWork=2;
    lAp1[iWork]++;
    lBm1[iWork]--;
  
    tmpVal = comphRRiPPSab( pripair, shell1, shell2, K,sspri, LA+1, lAp1, LB-1, lBm1 );
    tmpVal+= (shell1.O[iWork]-shell2.O[iWork])
             *comphRRiPPSab( pripair, shell1, shell2, K, sspri, LA, lA, LB-1, lBm1 );
  //  tmpVal+= pair.AB[iWork]*hRRSab(pair,shell1,shell2,LA,lA,LB-1,lBm1);
    return tmpVal;
   
  }  // comphRRiPPSab  

 
  //----------------------------------------------------------//
  // complex overlap vertical recursion                               //
  // (a|0) = (P-A)(a-1|0) + halfInvZeta*N_(a-1)*(a-2|0)       //
  //----------------------------------------------------------//
  /**
   *  \brief Perform the vertical recurrence relation for the uncontracted overlap integral 
   *
   *  (a|0) = (P-A)(a-1|0) + 1/2 *1/Zeta * N_(a-1)*(a-2|0)
   *
   *  where a is angular momentum, Zeta=zeta_a+zeta_b, A is bra nuclear coordinate. 
   *  P = (zeta_a*A+zeta_b*B)/Zeta
   *
   *  \param [in] pripair Primitive Shell pair data for shell1, shell2
   *  \param [in] shell1  Bra shell
   *  \param [in] LA      total Bra angular momentum
   *  \param [in] lA      Bra angular momentum vector (lAx,lAy,lAz)
   *
   *  \returns an uncontracted overlap integral
   *
   */ 
   dcomplex ComplexGIAOIntEngine::compvRRSa0(libint2::ShellPair::PrimPairData &pripair, 
    libint2::Shell &shell1, double *K, dcomplex sspri, int LA, int *lA){

  //notice: contraction coeffs are included in sspri.
    dcomplex tmpVal = 0.0;

    if (LA == 0) {   //[s||s]

      tmpVal = sspri; 
      // tmpVal = pow(sqrt(M_PI),3) * sqrt(pripair.one_over_gamma)*pripair.K ;
      return tmpVal;
    } // if (LA == 0)
  
    int iWork;
    int lAm1[3];
    for(iWork = 0;iWork < 3;iWork++) lAm1[iWork]=lA[iWork];
    if (lA[0] > 0) iWork = 0;
    else if (lA[1] > 0) iWork=1;
    else if (lA[2] > 0) iWork=2;
  
  /*
    if(LA == 1) {
     tmpVal = (pripair.P[iWork]-shell1.O[iWork])*pow(sqrt(M_PI),3) * sqrt(pripair.one_over_gamma)*pripair.K ; 
     return tmpVal;
    }
  */
  
    lAm1[iWork]--;
    dcomplex onei;
    onei.real(0);
    onei.imag(1);
    tmpVal = ( pripair.P[iWork]-shell1.O[iWork]  
              + 0.5*onei * pripair.one_over_gamma*K[iWork] )
              * compvRRSa0( pripair, shell1, K, sspri, LA-1, lAm1 ); 
  
  //  if(LA == 2&&lA[iWork] == 2) 
  //    tmpVal += 1/2*pripair.one_over_gamma
  //            *pow(sqrt(M_PI),3) * sqrt(pripair.one_over_gamma)*pripair.K;
  //  else if (lA[iWork] >=2) {
      if ( lA[iWork] >=2 ) {
        lAm1[iWork]--; 
        tmpVal += (lA[iWork]-1) * 0.5 * pripair.one_over_gamma
                  * compvRRSa0( pripair, shell1, K, sspri, LA-2, lAm1 );
  //  }
      } // if ( lA[iWork] >=2 )
    return tmpVal;
   
  } // compvRRSa0
 
  /**
   *  \brief Perform the vertical recurrence relation for the contracted kinetic integral
   *  
   *   kinetic vertical recursion                              
   *   (a|T|b) = (P-B)(a|T|b-1) + halfInvZeta*N_(b-1)(a|T|b-2) 
   *           + halfInvZeta*N_(a)(a-1|T|b-1)                  
   *           + 2*Xi*[(a|b) - halfInvzeta_b*N_(b-1)(a|b-2)]   
   *                                                           
   *   (a|T|0) = (P-A)(a-1|T|0) + halfInvZeta*N_(a-1)(a-2|T|0) 
   *           + 2*Xi*[(a|0) - halfInvZeta_a*N_(a-1)(a-2|0)]   
   *
   *  where a,b are the angular momentum, A,B are the nuclear coordinates.
   *
   *  \param [in] pair    Shell pair data for shell1, shell2
   *  \param [in] shell1  Bra shell
   *  \param [in] shell2  Ket shell
   *  \param [in] LA      total Bra angular momentum
   *  \param [in] lA      Bra angular momentum vector (lAx,lAy,lAz)
   *  \param [in] LB      total Ket angular momentum
   *  \param [in] lB      Ket angular momentum vector (lBx,lBy,lBz)
   *
   *  \returns a contracted overlap integral
   *
   */ 

/*
  dcomplex ComplexGIAOIntEngine::compvRRTab(libint2::ShellPair &pair, libint2::Shell &shell1,
    libint2::Shell &shell2, double *ka, double *kb, std::vector<dcomplex> &ssT_shellpair, 
    int LA, int *lA ,int LB, int *lB) {
 
    int iWork,lBm1[3];
    dcomplex tmpVal = 0.0;
    

    if ( (LA==0) and (LB==0) ) {
      // ss

      auto pripairindex = 0;
      for ( auto &pripair : pair.primpairs ) {
        
        tmpVal += ssT_shellpair[pripairindex];
        pripairindex++;
      }  // for pripair 
      return tmpVal; 
    }  else if(LB == 0) {

      // (|s)
      auto pripairindex = 0;
      for( auto pripair : pair.primpairs ) {
      
        tmpVal +=
          compvRRiPPTab( pripair, shell1, shell2, ka, kb, 
            ssT_shellpair[pripairindex],LA,lA,LB,lB );

        pripairindex++;

      } // for pripair 
      return tmpVal;
    }; // else if(LB == 0)

    // here LB > 0, which include LA>0 and LA==0  

    int lAm1[3];
    for(iWork = 0;iWork < 3;iWork++){
      lAm1[iWork]=lA[iWork];     
      lBm1[iWork]=lB[iWork];     
    };
    if (lB[0] > 0)      iWork = 0;
    else if (lB[1] > 0) iWork=1;
    else if (lB[2] > 0) iWork=2;
   // lAp1[iWork]++;
    lBm1[iWork]--;

    auto pripairindex = 0;
    for( auto &pripair : pair.primpairs ) { 

      auto Xi = shell1.alpha[pripair.p1] * shell2.alpha[pripair.p2]
                                    *pripair.one_over_gamma; 

      tmpVal += ( pripair.P[iWork] - shell2.O[iWork] 
         + ( ka[iWork] + kb[iWork] ) * 0.5 * pripair.one_over_gamma * 1i ) 
        * compvRRiPPTab( pripair, shell1, shell2, ka, kb, 
            ssT_shellpair[pripairindex], LA, lA, LB, lB ); 

      pripairindex++;
    } // for( auto &pripair : pair.primpairs )  

    if(lA[iWork]>0) { 
      lAm1[iWork]--;  
      auto pripairindex = 0; 
      for( auto &pripair : pair.primpairs ) 
        tmpVal += (lAm1[iWork]+1) * 0.5 * pripair.one_over_gamma
          *compvRRiPPTab( pripair, shell1, shell2, ka, kb,
             ssT_shellpair[pripairindex], LA-1, lAm1, LB-1, lBm1 ); 
        pripairindex++;
      } // for( auto &pripair : pair.primpairs ) 
    } // if(lA[iWork]>0)  

    if(lB[iWork]>=2) {  
      lBm1[iWork]--;  
      // auto tmploop=0.0; 
      auto pripairindex = 0; 
      for( auto &pripair : pair.primpairs ) {  
        tmpVal += (lBm1[iWork]+1) * 0.5 * pripair.one_over_gamma  
          * compvRRiPPTab( pripair, shell1, shell2, ka, kb,
              ssT_shellpair[pripairindex], LA, lA, LB-2, lBm1 );
        
        auto Xi = shell1.alpha[pripair.p1] * shell2.alpha[pripair.p2] 
                    *pripair.one_over_gamma; 

        tmpVal -= (lBm1[iWork]+1) * Xi / shell2.alpha[pripair.p2]
          * comp 

*/

//---------------------------------------------------------------------------------//
//complex Kinetic integral                                                         //
//
//[a|T|b]=[a|Tx|b]+[a|Ty|b]+[a|Tz|b]
//[a|Ti|b]=-2zeta_b^2[a||b+2i]+2i zeta_b kbi [a||b+1i] + {zeta_b(2LB+3)+kb^2}[a||b]//
//         -i kbi * bi [a||b-1i] -1/2*bi*(bi-1) [a||b-2i]
//---------------------------------------------------------------------------------//

   
  dcomplex ComplexGIAOIntEngine::compRRTab(
    libint2::ShellPair &pair, libint2::Shell &shell1, libint2::Shell &shell2, 
    double *ka, double *kb, 
    std::vector<dcomplex> &ss_shellpair, std::vector<dcomplex> &ssT_shellpair, 
    int LA, int *lA ,int LB, int *lB) {
 
    // int iWork,lBm1[3];
    dcomplex tmpVal = 0.0;
    dcomplex onei;
    onei.real(0);
    onei.imag(1);

/*    
    if ( (LA==0) and (LB==0) ) { 
      // (s|T|s)
      auto countershellpair = 0 ;
       
      for( auto &pripair : pair.primpairs ) {

        tmpVal += ssT_shellpair[countershellpair]; 
        countershellpair++;

      } // for( auto &pripair : pair.primpairs )
      return tmpVal;
    } // if ( (LA==0) and (LB==0) ) 
*/
    double K[3];
    for ( int mu = 0 ; mu < 3 ; mu++ ) {
      K[mu] = ka[mu]+kb[mu];
    } // for mu

    auto kbsquare = 0.0 ; 
    for ( int mu = 0 ; mu < 3 ; mu++ ) {
      kbsquare += pow( kb[mu] , 2 );
    } // for mu

/*
    auto countershellpair = 0 ;  
    for( auto &pripair : pair.primpairs ) {
 
      tmpVal += ( shell2.alpha[pripair.p2] * ( 2 * LB + 3 ) +kbsquare ) 
        * comphRRiPPSab( pripair, shell1, shell2, K, ss_shellpair[countershellpair],
            LA, lA, LB, lB );

      countershellpair++;
    } // for( auto &pripair : pair.primpairs )

*/
    
    int iWork,lBp2[3],lBp1[3],lBm[3]; 

    for( int i=0 ; i<3 ; i++ ) {
      lBm[i]=lB[i];     
      lBp2[i]=lB[i];
      lBp1[i]=lB[i];
    } // for( int i=0 ; i<3 ; i++ )  

/*
    for( iWork = 0 ; iWork < 3 ; iWork++ ) {
      if ( lB[iWork] >= 2 ) {
        lBm[iWork] = lB[iWork]-2;
        tmpVal -= 0.5 * lB[iWork]*(lB[iWork]-1) * comphRRSab( pair, shell1, shell2, K, 
          ss_shellpair, LA, lA, LB-2, lBm);
      } // if ( lB[iWork] >= 2 )
    } // for iWork 
*/
    
    for ( iWork=0 ; iWork<3 ; iWork++ ) { 
      for( int i=0 ; i<3 ; i++ ) {
        lBm[i]=lB[i];     
        lBp2[i]=lB[i];
        lBp1[i]=lB[i];
      }  // for( int i=0 ; i<3 ; i++ ) 

      lBp2[iWork] = lB[iWork]+2; 
        lBp1[iWork]=lB[iWork]+1; // now lBm = lB+1
      
      auto countershellpair = 0 ; 
      for( auto &pripair : pair.primpairs ) {
        // dcomplex tmploop = 0.0 ;

        tmpVal -= 2.0 * pow( shell2.alpha[pripair.p2], 2 ) 
          * comphRRiPPSab( pripair, shell1, shell2, K, ss_shellpair[countershellpair],
              LA, lA, LB+2, lBp2 );


        tmpVal += shell2.alpha[pripair.p2] * kb[iWork] * 2*onei * comphRRiPPSab( pripair,
          shell1, shell2, K, ss_shellpair[countershellpair], LA, lA, LB+1, lBp1 ); 


        tmpVal += ( shell2.alpha[pripair.p2] * ( 2 * lB[iWork] + 1 )
          + 0.5 * pow( kb[iWork],2 ) ) * comphRRiPPSab( pripair, shell1, shell2, K, 
            ss_shellpair[countershellpair], LA, lA, LB, lB ); 

        if ( lB[iWork] >0 ){
          lBm[iWork] = lB[iWork]-1;

          tmpVal -= kb[iWork] * lB[iWork] * comphRRiPPSab( pripair, shell1, shell2, K,
            ss_shellpair[countershellpair], LA, lA, LB-1, lBm ) * onei;
        } // if ( lB[iWork] >0 )

        if ( lB[iWork] >= 2 ) {
          lBm[iWork] = lB[iWork]-2;   
          tmpVal -= 0.5 * lB[iWork]*(lB[iWork]-1) * comphRRiPPSab( pripair, shell1, 
            shell2, K, ss_shellpair[countershellpair], LA, lA, LB-2, lBm );
        } // if ( lB[iWork] >= 2 )  

        countershellpair++;
      } // for( auto &pripair : pair.primpairs ) 
    } // for( iWork = 0 ; iWork < 3 ; iWork++ )

    return tmpVal;

  } // compRRTab


  /**
   *  \brief Perform the vertical recurrence relation for the uncontracted 
   *  angular momentum integral
   *
   *   if LB == 0,then [a|L|0]=(Pi-Ai)[a-1i|L|0]]+halfInvZeta*Ni(a-1i)[a-2i|L|0]      
   *                    +zeta_b/zeta*{1i cross(B-C)}_mu*[a-1i|b]                      
   *   if LB>0,then  [a|L|b]=(Pi-Bi)[a|L|b-1i]                                        
   *                    +halfInvZeta*Ni(b-1i)[a|L|b-2i]+halfInvZeta*Ni(a)[a-1i|L|b-1i]
   *                    -zeta_a/zeta*{1i cross(A-C)}_mu*[a|b-1i]                      
   *                    -halfInvZeta*Sum_k=x,y,z N_k(a){1i cross 1k}_mu*[a-1k|b-1i]   
   *
   *  where a,b are the angular momentum, A,B are the nuclear coordinates.
   *  halInvZeta = 1/2 * 1/Zeta
   *
   *  \param [in] pripair primitive Shell pair data for shell1, shell2
   *  \param [in] shell1  Bra shell
   *  \param [in] shell2  Ket shell
   *  \param [in] OneixAC 1_i cross AC vector is a 3 by 3 tensor
   *  \param [in] OneixBC 1_i cross BC vector is a 3 by 3 tensor
   *  \param [in] LA      total Bra angular momentum
   *  \param [in] lA      Bra angular momentum vector (lAx,lAy,lAz)
   *  \param [in] LB      total Ket angular momentum
   *  \param [in] lB      Ket angular momentum vector (lBx,lBy,lBz)
   *  \param [in] mu      index of the angular momentum integral component being calculated
   *
   *  \returns the mu component of an uncontracted angular momentum integral
   *
   */ 

   dcomplex ComplexGIAOIntEngine::compLabmu( libint2::ShellPair::PrimPairData &pripair, 
    libint2::Shell &shell1, libint2::Shell &shell2, double *ka, double *kb, 
    dcomplex ss_primitive, int LA, int *lA, int LB, int *lB, int mu){

    dcomplex tmpVal=0.0;
    dcomplex onei;
    onei.real(0);
    onei.imag(1);
    int iWork,alpha,beta; 

    double K[3];
    for ( int mu = 0 ; mu < 3 ; mu++ ) {
      K[mu] = ka[mu]+kb[mu];
    } // for mu

    // consists of two terms: (alpha)(beta)-(beta)(alpha)

    // first term start
    if ( mu == 0 ) {
      alpha = 1;
      beta  = 2;
    } else if ( mu == 1 ) {
      alpha = 2;
      beta  = 0;
    } else {
      alpha = 0;
      beta  = 1;
    } // if ( mu == 0 )

    int lAp1[3],lBp1[3];
    for ( iWork = 0 ; iWork < 3 ; iWork++ ){
      lAp1[iWork] = lA[iWork];
      lBp1[iWork] = lB[iWork];
    } // for ( iWork = 0 }
 
    lAp1[alpha] = lA[alpha] + 1;
    lBp1[beta]  = lB[beta]  +1;

    tmpVal -= 2 * shell2.alpha[pripair.p2] * comphRRiPPSab( pripair, shell1, shell2, 
      K, ss_primitive, LA+1, lAp1, LB+1, lBp1);
 
    tmpVal += onei * kb[beta] * comphRRiPPSab( pripair, shell1, shell2, K, 
      ss_primitive, LA+1, lAp1, LB, lB );  

    tmpVal -= shell1.O[alpha] * ( 2* shell2.alpha[pripair.p2] * comphRRiPPSab( 
      pripair, shell1, shell2, K, ss_primitive, LA, lA, LB+1, lBp1)
      - onei * kb[beta] * comphRRiPPSab( pripair, shell1, shell2, K, 
                                ss_primitive, LA, lA, LB, lB ) );

    int lBm1[3];
    if ( lB[beta] > 0 ) {

      for ( iWork = 0 ; iWork < 3 ; iWork++ ) {
        lBm1[iWork] = lB[iWork];
      } // for ( iWork = 0 )
      lBm1[beta] = lB[beta] - 1;
    
      tmpVal += static_cast<dcomplex>( lB[beta] ) 
        * comphRRiPPSab( pripair, shell1, shell2, K, ss_primitive, LA+1, 
        lAp1, LB-1, lBm1 );
      
      tmpVal += shell1.O[alpha] * static_cast<dcomplex>( lB[beta] ) * comphRRiPPSab( 
        pripair, shell1, shell2, K, ss_primitive, LA, lA, LB-1, lBm1);
   
    } // if ( lB[beta] > 0 )
    // first term finish

    // second term start -(beta)(alpha)
     
    for ( iWork = 0 ; iWork < 3 ; iWork++ ){
      lAp1[iWork] = lA[iWork];
      lBp1[iWork] = lB[iWork];
    } // for ( iWork = 0 }
 
    lAp1[beta]  = lA[beta]  + 1;
    lBp1[alpha] = lB[alpha] + 1;

    tmpVal += 2 * shell2.alpha[pripair.p2] * comphRRiPPSab( pripair, shell1, shell2,
      K, ss_primitive, LA+1, lAp1, LB+1, lBp1);

    tmpVal -= onei * kb[alpha] * comphRRiPPSab( pripair, shell1, shell2, K,
      ss_primitive, LA+1, lAp1, LB, lB );

    tmpVal += shell1.O[beta] * ( 2* shell2.alpha[pripair.p2] * comphRRiPPSab(
      pripair, shell1, shell2, K, ss_primitive, LA, lA, LB+1, lBp1)
      - onei * kb[alpha] * comphRRiPPSab( pripair, shell1, shell2, K,
      ss_primitive, LA, lA, LB, lB ) );  

    if ( lB[alpha] > 0 ) {

      for ( iWork = 0 ; iWork < 3 ; iWork++ ) {
        lBm1[iWork] = lB[iWork];
      } // for ( iWork = 0 )

      lBm1[alpha] = lB[alpha] - 1;
      
      tmpVal -= static_cast<dcomplex>( lB[alpha] )
         * comphRRiPPSab( pripair, shell1, shell2, K, ss_primitive,
        LA+1, lAp1, LB-1, lBm1 );

      tmpVal -= shell1.O[beta] * lB[alpha] * comphRRiPPSab( pripair, shell1, shell2,
        K, ss_primitive, LA, lA, LB-1, lBm1); 

    } // if ( lB[alpha] > 0 )

    return tmpVal;
  
  } //dcomplex ComplexGIAOIntEngine::compLabmu  




  /**
   *  \brief Decompose electric dipole integral into several contracted overlap integral
   *
   *   [a|r_\alpha|b]           
   *    = [a+1_\alpha||b]   
   *      +A_\alpha[a||b]
   *
   *  where a,b are the angular momentum, A,B are the nuclear coordinates.
   *
   *  \param [in] pair    Shell pair data for shell1, shell2
   *  \param [in] shell1  Bra shell
   *  \param [in] shell2  Ket shell
   *  \param [in] LA      total Bra angular momentum
   *  \param [in] lA      Bra angular momentum vector (lAx,lAy,lAz)
   *  \param [in] LB      total Ket angular momentum
   *  \param [in] lB      Ket angular momentum vector (lBx,lBy,lBz)
   *  \param [in] alpha   the component of nabla or r operator (x,y,z)
   *
   *  \returns the alpha component of a contracted electric dipole integral
   *
   */ 

   dcomplex ComplexGIAOIntEngine::compDipoleD1_len( libint2::ShellPair &pair, libint2::Shell &shell1, 
    libint2::Shell &shell2, double *K,std::vector<dcomplex> &ss_shellpair, 
    int LA, int *lA, int LB, int *lB, int alpha ) {
  
  
    int lAp1[3];
    dcomplex tmpVal = 0.0;  
  
    for ( auto k = 0 ; k < 3 ; k++ ) {
      lAp1[k] = lA[k];
    }
    lAp1[alpha] = lA[alpha]+1;
    
    tmpVal += comphRRSab( pair, shell1, shell2, K, ss_shellpair, LA+1,lAp1,LB,lB);
    tmpVal += shell1.O[alpha]* comphRRSab( pair, shell1, shell2, K, ss_shellpair, 
                LA, lA, LB, lB );
    
    return tmpVal;
  
  
  } // ComplexGIAOIntEngine::compDipoleD1_len 







  /**
   *  \brief Decompose electric quadrupole integral into several contracted overlap integral
   *
   *   [a|r_\alpha r_\beta|b]           
   *    = [a+1_\beta||b+1_\alpha]   
   *      +A_\alpha[a||b+1_\beta]+B_\beta[a+1_\alpha||b]
   *      +A_\alpha B_\beta [a||b]                             
   *
   *  where a,b are the angular momentum, A,B are the nuclear coordinates.
   *
   *  \param [in] pair    Shell pair data for shell1, shell2
   *  \param [in] shell1  Bra shell
   *  \param [in] shell2  Ket shell
   *  \param [in] LA      total Bra angular momentum
   *  \param [in] lA      Bra angular momentum vector (lAx,lAy,lAz)
   *  \param [in] LB      total Ket angular momentum
   *  \param [in] lB      Ket angular momentum vector (lBx,lBy,lBz)
   *  \param [in] alpha   the component of nabla or r operator (x,y,z)
   *  \param [in] beta    the component of nabla or r operator (x,y,z)
   *
   *  \returns the alpha beta component of a contracted electric quadrupole integral
   *
   */ 

   dcomplex ComplexGIAOIntEngine::compQuadrupoleE2_len( libint2::ShellPair &pair, libint2::Shell &shell1, 
    libint2::Shell &shell2, double *K,std::vector<dcomplex> &ss_shellpair, 
    int LA, int *lA, int LB, int *lB, int alpha, int beta ) {
  
  
    int lAp1[3],lBp1[3];
    dcomplex tmpVal = 0.0;  
  
    for ( auto k = 0 ; k < 3 ; k++ ) {
      lAp1[k] = lA[k];
      lBp1[k] = lB[k];
    }
    lAp1[alpha] = lA[alpha]+1;
    lBp1[beta]  = lB[beta] +1;
    
    tmpVal += comphRRSab( pair, shell1, shell2, K, ss_shellpair, LA+1,lAp1,LB+1,lBp1);
    tmpVal += shell1.O[alpha]* comphRRSab( pair, shell1, shell2, K, ss_shellpair, 
                LA, lA, LB+1, lBp1 );
    tmpVal += shell2.O[beta] * comphRRSab( pair, shell1, shell2, K, ss_shellpair,
                LA+1, lAp1, LB, lB );
    tmpVal += shell1.O[alpha] * shell2.O[beta] * comphRRSab( pair, shell1, shell2, 
                K, ss_shellpair, LA, lA, LB, lB );
    
    return tmpVal;
  
  
  } // ComplexGIAOIntEngine::compQuadrupoleE2_len



  /**
   *  \brief Decompose electric octupole integral into several contracted overlap integral
   *
   *   [a|r_\alpha r_\beta r_gamma|b]           
   *    = [a+1_\alpha+1_\beta||b+1_\gamma] +B_gamma[a+1_\alpha+1\beta||b]  
   *      +A_\beta[a+1\alpha||b+1_\gamma]+A_\beta B_gamma[a+1_\alpha||b]
   *      +A_\alpha[a+1_beta||b+1\gamma] +A_\alpha B_\gamma [a+1_\beta||b]           
   *      +A_\alpha A_\beta[a||b+1\gamma] + A_\alpha A_\beta B_\gamma [a||b]  
   *
   *  where a,b are the angular momentum, A,B are the nuclear coordinates.
   *
   *  \param [in] pair    Shell pair data for shell1, shell2
   *  \param [in] shell1  Bra shell
   *  \param [in] shell2  Ket shell
   *  \param [in] LA      total Bra angular momentum
   *  \param [in] lA      Bra angular momentum vector (lAx,lAy,lAz)
   *  \param [in] LB      total Ket angular momentum
   *  \param [in] lB      Ket angular momentum vector (lBx,lBy,lBz)
   *  \param [in] alpha   the component of nabla or r operator (x,y,z)
   *  \param [in] beta    the component of nabla or r operator (x,y,z)
   *  \param [in] gamma    the component of nabla or r operator (x,y,z)
   *
   *  \returns the alpha beta gamma component of a contracted electric octupole integral
   *
   */ 

   dcomplex ComplexGIAOIntEngine::compOctupoleE3_len( libint2::ShellPair &pair, libint2::Shell &shell1, 
    libint2::Shell &shell2, double *K,std::vector<dcomplex> &ss_shellpair, 
    int LA, int *lA, int LB, int *lB, int alpha, int beta, int gamma ) {
  
  
    int lAp1a[3],lAp1b[3],lAp2[3],lBp1[3];
    dcomplex tmpVal = 0.0;  
  
    for ( auto k = 0 ; k < 3 ; k++ ) {
      lAp1a[k] = lA[k];
      lAp1b[k] = lA[k];
      lAp2[k]  = lA[k];
      lBp1[k] = lB[k];
    }

    lAp1a[alpha] +=1;
    lAp1b[beta]  +=1;

    lAp2[alpha]  +=1;
    lAp2[beta]   +=1;

    lBp1[gamma]  +=1;
   
    tmpVal += comphRRSab( pair, shell1, shell2, K, ss_shellpair, LA+2,lAp2,LB+1,lBp1);

    tmpVal += shell2.O[gamma]* comphRRSab( pair, shell1, shell2, K, ss_shellpair, 
                LA+2,lAp2,LB,lB);

    tmpVal += shell1.O[beta]* comphRRSab( pair, shell1, shell2, K, ss_shellpair, 
                LA+1, lAp1a, LB+1, lBp1 );

    tmpVal += shell1.O[beta] * shell2.O[gamma] * comphRRSab( pair, shell1, shell2, 
                K, ss_shellpair, LA+1, lAp1a, LB, lB );

    tmpVal += shell1.O[alpha] * comphRRSab( pair, shell1, shell2, K, ss_shellpair,
                LA+1, lAp1b, LB+1, lBp1 );

    tmpVal += shell1.O[alpha] * shell2.O[gamma] * comphRRSab( pair, shell1, shell2, 
                K, ss_shellpair, LA+1, lAp1b, LB, lB );
    
    tmpVal += shell1.O[alpha] * shell1.O[beta] * comphRRSab( pair, shell1, shell2, 
                K, ss_shellpair, LA, lA, LB+1, lBp1 );

    tmpVal += shell1.O[alpha] * shell1.O[beta] * shell2.O[gamma] * 
                comphRRSab( pair, shell1, shell2,  K, ss_shellpair, LA, lA, LB, lB );

    return tmpVal;
  
  
  } // ComplexGIAOIntEngine::compOctupoleE3_len  

 


  //---------------------------------------------------------//
  // complex potential integral horizontal recursion                 //
  //  (a|0_c|b) = (a+1|0_c|b-1) + (A-B)(a|0_c|b-1)           //
  //   LA  >=  LB                                              //
  //   horizontal recursion doesn't increase (m). and it's only used once, so m=0 here. //
  //---------------------------------------------------------//

  /**
   *  \brief Perform the horizontal recurrence relation for the contracted 
   *  nuclear potential integral
   *
   *  (a|0_c|b) = (a+1|0_c|b-1) + (A-B)(a|0_c|b-1)
   *
   *  where a,b are the angular momentum, A,B are the nuclear coordinates.
   *
   *  \param [in] nucShell nuclear shell, give the exponents of gaussian function of nuclei
   *  \param [in] pair    Shell pair data for shell1, shell2
   *  \param [in] shell1  Bra shell
   *  \param [in] shell2  Ket shell
   *  \param [in] LA      total Bra angular momentum
   *  \param [in] lA      Bra angular momentum vector (lAx,lAy,lAz)
   *  \param [in] LB      total Ket angular momentum
   *  \param [in] lB      Ket angular momentum vector (lBx,lBy,lBz)
   *
<<   *  \returns a contracted nuclear potential integral
   *
   */ 
   dcomplex ComplexGIAOIntEngine::comphRRVab(const std::vector<libint2::Shell> &nucShell, 
    libint2::ShellPair &pair, libint2::Shell &shell1,
    libint2::Shell &shell2, double *K, std::vector<dcomplex> &ss_shellpair, 
    int LA, int *lA, int LB, int *lB, const Molecule& molecule){

    int iWork,iAtom;
    dcomplex tmpVal=0.0;
    dcomplex PCK[3];
    double PC[3];
    double rho;
    dcomplex squarePCK = 0.0;
    dcomplex onei;
    onei.real(0);
    onei.imag(1);
    bool useFiniteWidthNuclei = nucShell.size() > 0; // if nuclear shell is defined, use finite nuclei 
  
    if(LB == 0) {
      // (LA|s)
      auto pripairindex = 0;
      for( auto pripair : pair.primpairs ) {
      iAtom = 0;
      for( auto atom : molecule.atoms ) {
  //      std::cerr<<atom.atomicNumber<<std::endl;
  //      std::cerr<<"iAtom = "<<iAtom<<std::endl;
        squarePCK = 0.0;     
  
        for( int m=0 ; m<3 ; m++ ) {
          PC[m]  = pripair.P[m] - atom.coord[m];
          PCK[m] = PC[m] + K[m]*0.5*pripair.one_over_gamma*onei;
          squarePCK += PCK[m]*PCK[m];
        }   
        auto lTotal = shell1.contr[0].l + shell2.contr[0].l; 
        dcomplex *tmpFmT = new dcomplex[lTotal+1];
        if ( useFiniteWidthNuclei ) {
          // here we don't have finite nuclei now
          rho = (1/pripair.one_over_gamma)* nucShell[iAtom].alpha[0]
                       /(1/pripair.one_over_gamma + nucShell[iAtom].alpha[0]);

          // or use double rho = nucShell[iAtom].alpha[0]/
          // (1.0+nucShell[iAtom].alpha[0]*pripair.one_over_gamma)

          // computeFmTTaylor(tmpFmT,rho*squarePCK,lTotal,0);
        }
        else if ( !useFiniteWidthNuclei ) {
/*
if (lTotal == 0) {
std::cout<<"K value "<<std::setprecision(12)<<K[0]<<" "<<K[1]<<" "<<K[2]<<std::endl;
std::cout<<"PCK "<<PCK[0]<<" "<<PCK[1]<<" "<<PCK[2]<<std::endl;
std::cout<<"T value "<<std::setprecision(12)<<(shell1.alpha[pripair.p1]+shell2.alpha[pripair.p2])*squarePCK<<std::endl;
} 
*/
          ComplexGIAOIntEngine::computecompFmT(tmpFmT,(shell1.alpha[pripair.p1]+shell2.alpha[pripair.p2])
                     *squarePCK,lTotal,0);
        }
        if (LA == 0) {

// std::cout<<"boys function of ss type "<<std::setprecision(12)<<tmpFmT[0]<<std::endl;
          // contraction coeff are already in ss overlap integral
          auto ssS = ss_shellpair[pripairindex];

          if ( !useFiniteWidthNuclei ) {
            auto ssV = 2.0*sqrt(1.0/(pripair.one_over_gamma*M_PI))*ssS;
  
            tmpVal += static_cast<dcomplex>( atom.nucCharge ) * ssV // * tmpFmT[0];
                    * compvRRVa0(nucShell,pripair,shell1,K,tmpFmT,PC,0,LA,lA,iAtom);

//std::cout<<"tmpFmT "<<tmpFmT[0]<<std::endl;
  // std::cerr<<"actual"<<std::endl;
          }
          else if ( useFiniteWidthNuclei ) {
  //          tmpVal += (static_cast<double>(mc->atomZ[iAtom]))*math.two*sqrt(rho/math.pi)*ijSP->ss[iPP]*tmpFmT[0];
            // we don't have finite nuclei now
            // auto ssV = 2.0*sqrt(rho/M_PI)*norm*ssS;
 
            // tmpVal += atom.atomicNumber * ssV * tmpFmT[0];

//std::cout<<"tmpFmT "<<tmpFmT[0]<<std::endl;
//  std::cerr<<"no finite nuclei"<<std::endl;
          }
        } // LA == 0
        else {   // LA > 0, go into vertical recursion
  /*
          auto norm = shell1.contr[0].coeff[pripair.p1]* 
                      shell2.contr[0].coeff[pripair.p2];
          auto ssS = pow(sqrt(M_PI),3) * sqrt(pripair.one_over_gamma)*pripair.K ;
  */
          auto ssS = ss_shellpair[pripairindex];  

          if ( !useFiniteWidthNuclei ) {
            auto ssV = 2.0*sqrt(1.0/(pripair.one_over_gamma*M_PI))*ssS;  
   
            tmpVal += static_cast<dcomplex>( atom.nucCharge ) * ssV 
                      * compvRRVa0(nucShell,pripair,shell1,K,tmpFmT,PC,0,LA,lA,iAtom);
  //std::cerr<<"actual"<<std::endl;
  
          } else if ( useFiniteWidthNuclei ) {
  //          tmpVal += mc->atomZ[iAtom]*math.two*sqrt(rho/math.pi)*ijSP->ss[iPP]*ComplexGIAOIntEngine::vRRVa0(ijSP,tmpFmT,PC,0,LA,lA,iPP,iAtom);
            // now we don't have finite nuclei
/*
            auto ssV = 2.0*sqrt(rho/M_PI)*norm*ssS;
            tmpVal += atom.atomicNumber * ssV * vRRVa0(nucShell,pripair,shell1,
                                                       tmpFmT,PC,0,LA,lA,iAtom);
*/

//  std::cerr<<"no finite nuclei"<<std::endl;
          } // else if ( useFiniteWidthNuclei )
        } // else
        delete[] tmpFmT;
        iAtom++;
      } // atom
      pripairindex++;
      }  // pripair
    } // if LB  ==  0
  
     else {   // LB>0
      int lAp1[3],lBm1[3];
      for( int m=0 ; m<3 ; m++ ) {
        lAp1[m]=lA[m];
        lBm1[m]=lB[m];
      };
      if (lB[0] > 0) iWork = 0;
      else if (lB[1] > 0) iWork=1;
      else if (lB[2] > 0) iWork=2;
  
      lAp1[iWork]++;
      lBm1[iWork]--;
      tmpVal = comphRRVab(nucShell,pair ,shell1,shell2, K,ss_shellpair, LA+1,lAp1, LB-1,lBm1,molecule);
      tmpVal+= pair.AB[iWork]*comphRRVab(nucShell,pair,shell1, shell2,K,ss_shellpair, LA,lA,LB-1,lBm1,molecule);
    }
  
    return tmpVal;
  } // comphRRVab
  
  //----------------------------------------------------------------------------//
  // complex potential integral vertical recursion                              //
  //  (a|0_c|0)^(m) = (P-A+i(ka+kb)/(2zeta))(a-1|0_c|0)^(m)                     //
  //                + (i(ka+kb)/(2zeta)-(P-C))(a-1|0_c|0)^(m+1)                 //
  //                + halfInvZeta*N_(a-1)*[(a-2|0_c|0)^(m)-(a-2|0_c|0)^(m+1)]   //
  //  since LB == 0, we only decrease a                                         //
  //----------------------------------------------------------------------------//

  /**
   *  \brief Perform the vertical recurrence relation for the uncontracted 
   *    nuclear potential integral 
   *
   *   (a|0_c|0)^(m) = (P-A+i(ka+kb)/(2zeta))(a-1|0_c|0)^(m) 
   *                 + (i(ka+kb)/(2zeta)-(P-C))(a-1|0_c|0)^(m+1) 
   *                 + halfInvZeta*N_(a-1)*[(a-2|0_c|0)^(m)-(a-2|0_c|0)^(m+1)]
   *
   *  where a is angular momentum, Zeta=zeta_a+zeta_b, A is bra nuclear coordinate. 
   *  P = (zeta_a*A+zeta_b*B)/Zeta
   *
   *  \param [in] nucShell nuclear shell, give the exponents of gaussian function of nuclei
   *  \param [in] pripair Primitive Shell pair data for shell1, shell2
   *  \param [in] shell1  Bra shell
   *  \param [in] FmT     table of Boys function with different m
   *  \param [in] PC      vector P-C
   *  \param [in] m       the order of auxiliary function
   *  \param [in] LA      total Bra angular momentum
   *  \param [in] lA      Bra angular momentum vector (lAx,lAy,lAz)
   *  \param [in] iAtom   the index of the atom of the nuclei
   *
   *  \returns an uncontracted nuclear potential integral
   *
   */ 

   dcomplex ComplexGIAOIntEngine::compvRRVa0( const std::vector<libint2::Shell> &nucShell,
    libint2::ShellPair::PrimPairData &pripair, libint2::Shell &shell1,double *K,
    dcomplex *FmT, double *PC, int m, int LA, int *lA, int iAtom){
  
    // double rhoovzeta;
    bool useFiniteWidthNuclei = nucShell.size() > 0; // if nuclei shell is defined, use finite nuclei 

    if(LA == 0) {
      if ( useFiniteWidthNuclei ) {
        // don't have finite nuclei 
     /*
        rhoovzeta =  nucShell[iAtom].alpha[0] / ( nucShell[iAtom].alpha[0]
                     +(1/pripair.one_over_gamma) );

        return (pow(rhoovzeta,m)*FmT[m]);
     */
      } else if ( !useFiniteWidthNuclei ) {       
        return FmT[m];  //Z*2*sqrt[zeta/pi]*[s|s]is given in hRRVab, in ssV.
      }
    } // if(LA == 0) 
  
    dcomplex tmpVal=0.0;
    dcomplex onei;
    onei.real(0);
    onei.imag(1);
    int lAm1[3]; //means lA minus 1_i
    int iWork;
    for( iWork = 0 ; iWork < 3 ; iWork++ ) lAm1[iWork]=lA[iWork];
    if (lA[0] > 0) iWork = 0;
    else if (lA[1] > 0) iWork=1;
    else if (lA[2] > 0) iWork=2;
    lAm1[iWork]--;
 
    dcomplex PAK;
    PAK = pripair.P[iWork]-shell1.O[iWork]+K[iWork]*0.5*pripair.one_over_gamma*onei; 
    tmpVal  = PAK *
                compvRRVa0(nucShell,pripair,shell1,K,FmT,PC,m,LA-1,lAm1,iAtom);
            // (P-A+i*K/(2zeta))*(a-1i|V|0)(m)  

    dcomplex PCK;
    PCK = PC[iWork]+ K[iWork]*0.5*pripair.one_over_gamma*onei;
    tmpVal -= PCK 
             * compvRRVa0(nucShell,pripair,shell1,K,FmT,PC,m+1,LA-1,lAm1,iAtom);
            // -(P-C+i*K/(2zeta))*(a-1i|V|0)(m+1)  

    if( lAm1[iWork] >=1 ){
      lAm1[iWork]--;
      tmpVal += (lAm1[iWork]+1)*0.5*pripair.one_over_gamma *
                (compvRRVa0(nucShell,pripair,shell1,K,FmT,PC,m,LA-2,lAm1,iAtom)
                -compvRRVa0(nucShell,pripair,shell1,K,FmT,PC,m+1,LA-2,lAm1,iAtom));
    }
    return tmpVal; 
  } // ComplexGIAOIntEngine::compvRRVa0 

}; //namespace ChronusQ 

