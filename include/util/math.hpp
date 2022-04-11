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

namespace ChronusQ {

  /**
   *  \brief Handle the fact that std::conj actually returns 
   *  std::complex
   */
  template <typename T>
  inline T SmartConj(const T&);
 
  template <>
  inline double SmartConj(const double &x) { return x; }

  template <>
  inline dcomplex SmartConj(const dcomplex &x) { return std::conj(x); }

  // Combinaiton
  inline size_t Comb(size_t N, size_t K){

	if (K > N) CErr("Can not do combinations of choosing a K larger than N"); 

    size_t KMin = std::min(K, N - K); 
    size_t Comb, Comb_tmp;
    
    Comb = 1ul;
    for (auto i = 1ul; i <= KMin; i ++) {
      Comb_tmp = Comb;
      Comb *= (N - KMin + i);
      Comb /= i;

      if (Comb  < Comb_tmp) CErr("Overflow of long unsign int detected in Comb"); 
    }
    
    return Comb;

  }; // Combination

}; // namespace ChronusQ
