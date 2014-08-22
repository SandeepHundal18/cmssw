#include <iostream>
#include <algorithm> 

#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "SimDataFormats/GeneratorProducts/interface/GenFilterInfo.h"

using namespace edm;
using namespace std;

GenFilterInfo::GenFilterInfo() :
  numPassPositiveEvents_(0),
  numPassNegativeEvents_(0),
  numTotalPositiveEvents_(0),
  numTotalNegativeEvents_(0),
  sumPassWeights_(0.),
  sumPassWeights2_(0.),
  sumFailWeights_(0.),
  sumFailWeights2_(0.)
{
}

GenFilterInfo::GenFilterInfo(unsigned int tried, unsigned int pass) :
  numPassPositiveEvents_(pass),
  numPassNegativeEvents_(0),
  numTotalPositiveEvents_(tried),
  numTotalNegativeEvents_(0),
  sumPassWeights_(pass),
  sumPassWeights2_(pass),
  sumFailWeights_(tried-pass),
  sumFailWeights2_(tried-pass)
{
}

GenFilterInfo::GenFilterInfo(unsigned int passp, unsigned int passn, unsigned int totalp, unsigned int totaln,
			     double passw, double passw2, double failw, double failw2) :
  numPassPositiveEvents_(passp),
  numPassNegativeEvents_(passn),
  numTotalPositiveEvents_(totalp),
  numTotalNegativeEvents_(totaln),
  sumPassWeights_(passw),
  sumPassWeights2_(passw2),
  sumFailWeights_(failw),
  sumFailWeights2_(failw2)
{
}

GenFilterInfo::GenFilterInfo(const GenFilterInfo& other):
  numPassPositiveEvents_(other.numPassPositiveEvents_),
  numPassNegativeEvents_(other.numPassNegativeEvents_),
  numTotalPositiveEvents_(other.numTotalPositiveEvents_),
  numTotalNegativeEvents_(other.numTotalNegativeEvents_),
  sumPassWeights_(other.sumPassWeights_),
  sumPassWeights2_(other.sumPassWeights2_),
  sumFailWeights_(other.sumFailWeights_),
  sumFailWeights2_(other.sumFailWeights2_)
{
}

GenFilterInfo::~GenFilterInfo()
{
}

bool GenFilterInfo::mergeProduct(GenFilterInfo const &other)
{
  // merging two GenFilterInfos means that the numerator and
  // denominator from the original product need to besummed with
  // those in the product we are going to merge

  numPassPositiveEvents_ += other.numPassPositiveEvents_;
  numPassNegativeEvents_ += other.numPassNegativeEvents_;
  numTotalPositiveEvents_ += other.numTotalPositiveEvents_;
  numTotalNegativeEvents_ += other.numTotalNegativeEvents_;
  sumPassWeights_        += other.sumPassWeights_;
  sumPassWeights2_       += other.sumPassWeights2_;
  sumFailWeights_        += other.sumFailWeights_;
  sumFailWeights2_       += other.sumFailWeights2_;

  return true;
}

double GenFilterInfo::filterEfficiency(int idwtup) const {
  double eff = -1;
  switch(idwtup) {
  case 3: case -3:
    eff = numEventsTotal() > 0 ? (double) numEventsPassed() / (double) numEventsTotal(): -1;
    break;
  default:
    eff = sumWeights() > 1e-6? sumPassWeights()/sumWeights(): -1;
    break;
  }
  return eff;

}

double GenFilterInfo::filterEfficiencyError(int idwtup) const {

  double efferr = -1;
  switch(idwtup) {
  case 3: case -3:
    {
      double effp  = numTotalPositiveEvents() > 1e-6? 
	(double)numPassPositiveEvents()/(double)numTotalPositiveEvents():0;
      double effp_err2 = numTotalPositiveEvents() > 1e-6?
	(1-effp)*effp/(double)numTotalPositiveEvents(): 0;

      double effn  = numTotalNegativeEvents() > 1e-6? 
	(double)numPassNegativeEvents()/(double)numTotalNegativeEvents():0;
      double effn_err2 = numTotalNegativeEvents() > 1e-6? 
	(1-effn)*effn/(double)numTotalNegativeEvents(): 0;

      efferr = numEventsTotal() > 0 ? sqrt ( 
					    ((double)numTotalPositiveEvents()*(double)numTotalPositiveEvents()*effp_err2 + 
					     (double)numTotalNegativeEvents()*(double)numTotalNegativeEvents()*effn_err2)
					    /(double)numEventsTotal()/(double)numEventsTotal()
					     ) : -1;
      break;
    }
  default:
    {
      double denominator = sumWeights()*sumWeights()*sumWeights()*sumWeights();
      double numerator =
	sumPassWeights2() * sumFailWeights()* sumFailWeights() +
	sumFailWeights2() * sumPassWeights()* sumPassWeights();
      efferr = denominator>1e-6? 
	sqrt(numerator/denominator):-1;
      break;
    }
  }

  return efferr;
}
