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
#include <cxxapi/options.hpp>
#include <cerr.hpp>

namespace ChronusQ {


  void CQSCF_VALID( std::ostream &out, CQInputFile &input ) {

    // Allowed keywords
    std::vector<std::string> allowedKeywords = {
      "ENETOL",
      "DENTOL",
      "FDCTOL",
      "MAXITER",
      "INCFOCK",
      "NINCFOCK",
      "GUESS",
      "ALG",
      "EXTRAP",
      "DIIS",
      "DIISALG",
      "NKEEP",
      "DAMP",
      "DAMPPARAM",
      "DAMPERROR",
      "FIELD",
      "PRINTMOS",
      "NEO",
      "PROT_GUESS",
      "SWAPMO",
      "SWITCH",
      "NRAPPROX",
      "NRTRUST",
	  "NRLEVELSHIFT"
    };

    // Specified keywords
    std::vector<std::string> scfKeywords = input.getDataInSection("SCF");

    // Make sure all of scfKeywords in allowedKeywords

    for( auto &keyword : scfKeywords ) {
      auto ipos = std::find(allowedKeywords.begin(),allowedKeywords.end(),keyword);
      if( ipos == allowedKeywords.end() ) 
        CErr("Keyword SCF." + keyword + " is not recognized",std::cout);// Error
    }
    // Check for disallowed combinations (if any)
  }


  void HandlePostSCFRestarts(std::ostream &out, CQInputFile &input,
                             SCFControls &scfControls) {

    bool restart = false;
    
    if ( input.containsSection("RT") )
      OPTOPT( restart |= input.getData<bool>("RT.RESTART"); )
    // Add additional checks like the one below for restarting other post SCF
    // RESP restart is NYI, so this is a commented out placeholder example
    //
    // else if ( input.containsSection("RESP") )
    //   OPTOPT( restart |= input.getData<bool>("RESP.RESTART"); )

    if ( restart ) {
      out << "  *** RESTART requested -- SCF.GUESS set to READMO and SCF.ALG set to SKIP ***";
      out << std::endl;

      scfControls.guess = READMO;
      scfControls.scfAlg = _SKIP_SCF;
    }

  }

  std::unordered_map<std::string,int> SpinMap = {
    { "A" , 0  },
    { "B" , 1  }
  };

  void HandleOrbitalSwaps(std::ostream &out, CQInputFile &input,
    SingleSlaterBase &ss) {

    // MO swapping
    std::string swapMOStrings;
    OPTOPT( swapMOStrings = input.getData<std::string>("SCF.SWAPMO"));
    if ( not swapMOStrings.empty() ) {
      std::cout << "  * Manually MO Swapping Detected: " << std::endl;

      if( ss.scfControls.guess != READMO and ss.scfControls.guess != FCHKMO )
        CErr("MO swapping only for user-specified guess MOs");

      std::vector<std::string> moTokens;
      //Loop over lines of mo swapping
      std::istringstream moStream(swapMOStrings);

      for( std::string line; std::getline(moStream, line); ) {
        split(moTokens, line, " \t,");

        if( moTokens.size() == 0 ) continue;
        else if( moTokens.size() != 2 and moTokens.size() != 3 ) CErr("Need 2 or 3 entries in single line for swapping");

        // Parse spin if present
        std::string spinDir("A");
        if( moTokens.size() == 3 ) spinDir=moTokens[2];
        trim(spinDir);

        // mo[1] only present for unrestricted calcs
        if( spinDir == "B" and not (ss.nC == 1 and not ss.iCS) ) CErr("Swapping of beta MOs is only valid for open-shell 1c");

        ss.moPairs[SpinMap[spinDir]].emplace_back(std::stoul(moTokens[0]), std::stoul(moTokens[1]));
      }

    }

  }

  SCFControls CQSCFOptions(std::ostream &out, CQInputFile &input, EMPerturbation &pert) {

    SCFControls scfControls;

    // SCF section not required
    if( not input.containsSection("SCF") ) {
     
      // Restart jobs
      HandlePostSCFRestarts(out, input, scfControls);

      return scfControls;
    }

    // Optionally parse guess

    // Energy convergence tolerance
    OPTOPT( scfControls.eneConvTol =
              input.getData<double>("SCF.ENETOL"); )

    // Energy convergence tolerance
    OPTOPT( scfControls.denConvTol =
              input.getData<double>("SCF.DENTOL"); )

    // Energy Gradient convergence tolerance
    OPTOPT( scfControls.FDCConvTol =
              input.getData<double>("SCF.FDCTOL"); )

    // Maximum SCF iterations
    OPTOPT( scfControls.maxSCFIter =
              input.getData<size_t>("SCF.MAXITER"); )


    // Incremental Fock Options
    OPTOPT(
      scfControls.doIncFock = input.getData<bool>("SCF.INCFOCK");
    )
    OPTOPT(
      scfControls.nIncFock = input.getData<size_t>("SCF.NINCFOCK");
    )


    // Guess
    OPTOPT(
      std::string guessString = input.getData<std::string>("SCF.GUESS");

      if( not guessString.compare("CORE") )
        scfControls.guess = CORE;
      else if( not guessString.compare("SAD") )
        scfControls.guess = SAD;
      else if( not guessString.compare("TIGHT") )
        scfControls.guess = TIGHT;
      else if( not guessString.compare("RANDOM") )
        scfControls.guess = RANDOM;
      else if( not guessString.compare("READMO") )
        scfControls.guess = READMO;
      else if( not guessString.compare("READDEN") )
        scfControls.guess = READDEN;
      else if( not guessString.compare("FCHKMO") )
        scfControls.guess = FCHKMO;
      else
        CErr("Unrecognized entry for SCF.GUESS");
    )

    OPTOPT(
      std::string guessString = input.getData<std::string>("SCF.PROT_GUESS");

      if( not guessString.compare("CORE") )
        scfControls.prot_guess = CORE;
      else if( not guessString.compare("RANDOM") )
        scfControls.prot_guess = RANDOM;
      else if( not guessString.compare("READMO") )
        scfControls.prot_guess = READMO;
      else if( not guessString.compare("READDEN") )
        scfControls.prot_guess = READDEN;
      else
        CErr("Unrecognized entry for SCF.PROT_GUESS");
    )

    // ALGORITHM
    OPTOPT(

        std::string algString = input.getData<std::string>("SCF.ALG");
        if( not algString.compare("CONVENTIONAL") )
          scfControls.scfAlg = _CONVENTIONAL_SCF;
        else if( not algString.compare("NR") )
          scfControls.scfAlg = _NEWTON_RAPHSON_SCF;
        else if( not algString.compare("SKIP") )
          scfControls.scfAlg = _SKIP_SCF;
        else CErr("Unrecognized entry for SCF.ALG!");

    )

    // Newton-Raphson SCF Approximation
    OPTOPT(
      std::string algString = input.getData<std::string>("SCF.NRAPPROX");
      if( not algString.compare("FULL") )
        scfControls.nrAlg = FULL_NR;
      else if( not algString.compare("BFGS") )
        scfControls.nrAlg = QUASI_BFGS;
      else if( not algString.compare("SR1") )
        scfControls.nrAlg = QUASI_SR1;
      else if( not algString.compare("GRADDESCENT") )
        scfControls.nrAlg = GRAD_DESCENT;
      else CErr("Unrecognized entry for SCF.NRAPPROX");
    )

    // Newton-Raphson SCF Initial trust region and level-shift
    OPTOPT(
      scfControls.nrTrust = input.getData<double>("SCF.NRTRUST");
    )
    OPTOPT(
      scfControls.nrLevelShift = input.getData<double>("SCF.NRLEVELSHIFT");
    )


    // Restart jobs
    HandlePostSCFRestarts(out, input, scfControls);


    // Toggle extrapolation in its entireity
    OPTOPT(
      scfControls.doExtrap =
        input.getData<bool>("SCF.EXTRAP");
    )

    // Handle DIIS options
    OPTOPT(
      std::string diisAlg = input.getData<std::string>("SCF.DIISALG");
      if( diisAlg == "CDIIS")
      {
        scfControls.diisAlg = CDIIS;
      }
      else if( diisAlg == "EDIIS" )
      {
        scfControls.diisAlg = EDIIS;
      }
      else if( diisAlg == "CEDIIS" )
      {
        scfControls.diisAlg = CEDIIS;
      }
      else
      {
        scfControls.diisAlg = CDIIS;
      }
    )

    // Check if it specifically says DIIS=FALSE
    OPTOPT(
      bool doDIIS = input.getData<bool>("SCF.DIIS");
      if( not doDIIS ) 
        scfControls.diisAlg = NONE;
    );

    // Number of terms for keep for DIIS
    OPTOPT( scfControls.nKeep = input.getData<size_t>("SCF.NKEEP"); )
    if( scfControls.nKeep < 1 )
      scfControls.nKeep = 1;

    // Point at which to switch from EDIIS to CDIIS
    OPTOPT( scfControls.cediisSwitch = input.getData<double>("SCF.SWITCH"); )
    if( scfControls.cediisSwitch <= 0. )
      CErr("CEDIIS Switch is less than or equal to zero");

    // Parse Damping options
    OPTOPT(
      scfControls.doDamp = input.getData<bool>("SCF.DAMP");
    );

    OPTOPT(
      scfControls.dampStartParam =
        input.getData<double>("SCF.DAMPPARAM");
    );

    OPTOPT(
      scfControls.dampError =
        input.getData<double>("SCF.DAMPERROR");
    );


    // SCF Field
    auto handleField = [&]() {
      std::string fieldStr;
      OPTOPT(
        fieldStr = input.getData<std::string>("SCF.FIELD");
      )
      if( fieldStr.empty() ) return;

      std::vector<std::string> tokens;
      split(tokens,fieldStr);

      if( tokens.size() < 4 )
        CErr(fieldStr + "is not a valid SCF Field specification");

      std::string fieldTypeStr = tokens[0];

      EMFieldTyp fieldType;
      if( not fieldTypeStr.compare("ELECTRIC") )
        fieldType = Electric;
      else if( not fieldTypeStr.compare("MAGNETIC") )
        fieldType = Magnetic;  
      else
        CErr(fieldTypeStr + "not a valid Field type");

      if( tokens.size() == 4 ) { 

        cart_t field = {std::stod(tokens[1]), std::stod(tokens[2]), 
                        std::stod(tokens[3])};

        pert.addField(fieldType,field);

      } else
        CErr("Non Dipole fields NYI");
    };

    handleField();






    // Printing Options
    if ( input.containsData("SCF.PRINTMOS") ) {
      try { scfControls.printMOCoeffs = input.getData<size_t>("SCF.PRINTMOS"); }
      catch(...) {
        CErr("Invalid PRINTMOS input. Please use number 0 ~ 9.");
      }
    }
    if (scfControls.printMOCoeffs >= 10 ) CErr("SCF print level is not valid!");







    // Handling eqivalences in the input options


    // Setting the damp param to 0. is equivalent to
    // turning damping off
    if( scfControls.dampStartParam == 0. )
      scfControls.doDamp = false;

    // Turning off both damping and DIIS is equivalent
    // to turning off extrapolation entirely
    if( not scfControls.doDamp and
        scfControls.diisAlg == NONE )
      scfControls.doExtrap = false;


    return scfControls;

  }; // CQSCFOptions

}; // namespace ChronusQ
