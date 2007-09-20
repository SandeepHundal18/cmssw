
// Original Author:  Jan Heyninck
//         Created:  Thu May 18 16:40:24 CEST 2006

// system include files
#include <memory>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDAnalyzer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "Utilities/General/interface/envUtil.h"

#include "DataFormats/EgammaCandidates/interface/PixelMatchGsfElectron.h"
#include "DataFormats/MuonReco/interface/Muon.h"
#include "DataFormats/JetReco/interface/CaloJet.h"
#include "DataFormats/METReco/interface/CaloMET.h"

#include "AnalysisDataFormats/TopObjects/interface/TopElectron.h" //added to work with the TopElectron and TopMuon (tmp)
#include "AnalysisDataFormats/TopObjects/interface/TopMuon.h" //added to work with the TopElectron and TopMuon (tmp)
#include "AnalysisDataFormats/TopObjects/interface/TopTau.h" //added to work with the TopElectron and TopMuon (tmp)
#include "AnalysisDataFormats/TopObjects/interface/TtGenEvent.h"
#include "TH1.h"
#include "TF1.h"
#include "TFile.h"
#include "TTree.h"
#include <vector>
#include <fstream>
#include <string>
#include <Math/VectorUtil.h>


// the following line was commented because it is already included in TopLepton.h
//typedef TopLepton<TopElectronType> TopElectron; //added to work with TopElectron
//typedef reco::PixelMatchGsfElectron electronType;
typedef reco::Muon muonType;
typedef reco::CaloJet jetType;
typedef reco::CaloMET metType;

//
// class declaration
//

class ResolutionCreator : public edm::EDAnalyzer {
   public:
      explicit ResolutionCreator(const edm::ParameterSet&);
      ~ResolutionCreator();
      virtual void analyze(const edm::Event&, const edm::EventSetup&);
      
   private:
      TFile 	*outfile;
      TH1F  	*hResEtEtaBin[10][20][20];
      TF1   	*fResEtEtaBin[10][20][20];
      TH1F  	*hResEtaBin[10][20];
      TF1   	*fResEtaBin[10][20];
      TH1F   	*hEtaBins;
      std::string	objectType_, labelName_;
      std::vector	<double> etabinVals_, eTbinVals_;
      double	minDR_;
      int 	etnrbins, etanrbins;
      int	nrFilled;
};
