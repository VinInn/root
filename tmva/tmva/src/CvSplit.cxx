// @(#)root/tmva $Id$
// Author: Kim Albertsson

/*************************************************************************
 * Copyright (C) 2018, Rene Brun and Fons Rademakers.                    *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "TMVA/CvSplit.h"

#include "TMVA/DataSet.h"
#include "TMVA/DataSetInfo.h"
#include "TMVA/Event.h"
#include "TMVA/MsgLogger.h"

#include <TString.h>
#include <TFormula.h>

#include <numeric>
#include <stdexcept>

ClassImp(TMVA::CvSplit);
ClassImp(TMVA::CvSplitBootstrappedStratified);
ClassImp(TMVA::CvSplitCrossValidation);

/* =============================================================================
      TMVA::CvSplit
============================================================================= */

////////////////////////////////////////////////////////////////////////////////
///

TMVA::CvSplit::CvSplit(UInt_t numFolds) : fNumFolds(numFolds), fMakeFoldDataSet(kFALSE) {}

/* =============================================================================
      TMVA::CvSplitBootstrappedStratified
============================================================================= */

////////////////////////////////////////////////////////////////////////////////
///

TMVA::CvSplitBootstrappedStratified::CvSplitBootstrappedStratified(UInt_t numFolds, UInt_t seed, Bool_t validationSet)
   : CvSplit(numFolds), fSeed(seed), fValidationSet(validationSet)
{
}

////////////////////////////////////////////////////////////////////////////////
///

void TMVA::CvSplitBootstrappedStratified::MakeKFoldDataSet(DataSetInfo &dsi)
{
   // Remove validation set?
   // Take DataSetSplit s as arg + numFolds
   // std::vector<EventCollection> folds = s.Split();

   // No need to do it again if the sets have already been split.
   if (fMakeFoldDataSet) {
      Log() << kINFO << "Splitting in k-folds has been already done" << Endl;
      return;
   }

   fMakeFoldDataSet = kTRUE;

   // Get the original event vectors for testing and training from the dataset.
   const std::vector<TMVA::Event *> TrainingData = dsi.GetDataSet()->GetEventCollection(Types::kTraining);
   const std::vector<TMVA::Event *> TestingData = dsi.GetDataSet()->GetEventCollection(Types::kTesting);

   std::vector<TMVA::Event *> TrainSigData;
   std::vector<TMVA::Event *> TrainBkgData;
   std::vector<TMVA::Event *> TestSigData;
   std::vector<TMVA::Event *> TestBkgData;

   // Split the testing and training sets into signal and background classes.
   for (UInt_t i = 0; i < TrainingData.size(); ++i) {
      if (strncmp(dsi.GetClassInfo(TrainingData.at(i)->GetClass())->GetName(), "Signal", 6) == 0) {
         TrainSigData.push_back(TrainingData.at(i));
      } else if (strncmp(dsi.GetClassInfo(TrainingData.at(i)->GetClass())->GetName(), "Background", 10) == 0) {
         TrainBkgData.push_back(TrainingData.at(i));
      } else {
         Log() << kFATAL << "DataSets should only contain Signal and Background classes for classification, "
               << dsi.GetClassInfo(TrainingData.at(i)->GetClass())->GetName() << " is not a recognised class" << Endl;
      }
   }

   for (UInt_t i = 0; i < TestingData.size(); ++i) {
      if (strncmp(dsi.GetClassInfo(TestingData.at(i)->GetClass())->GetName(), "Signal", 6) == 0) {
         TestSigData.push_back(TestingData.at(i));
      } else if (strncmp(dsi.GetClassInfo(TestingData.at(i)->GetClass())->GetName(), "Background", 10) == 0) {
         TestBkgData.push_back(TestingData.at(i));
      } else {
         Log() << kFATAL << "DataSets should only contain Signal and Background classes for classification, "
               << dsi.GetClassInfo(TestingData.at(i)->GetClass())->GetName() << " is not a recognised class" << Endl;
      }
   }

   // Split the sets into the number of folds.
   if (fValidationSet) {
      std::vector<std::vector<TMVA::Event *>> tempSigEvents = SplitSets(TrainSigData, 2);
      std::vector<std::vector<TMVA::Event *>> tempBkgEvents = SplitSets(TrainBkgData, 2);
      fTrainSigEvents = SplitSets(tempSigEvents.at(0), fNumFolds);
      fTrainBkgEvents = SplitSets(tempBkgEvents.at(0), fNumFolds);
      fValidSigEvents = SplitSets(tempSigEvents.at(1), fNumFolds);
      fValidBkgEvents = SplitSets(tempBkgEvents.at(1), fNumFolds);
   } else {
      fTrainSigEvents = SplitSets(TrainSigData, fNumFolds);
      fTrainBkgEvents = SplitSets(TrainBkgData, fNumFolds);
   }

   fTestSigEvents = SplitSets(TestSigData, fNumFolds);
   fTestBkgEvents = SplitSets(TestBkgData, fNumFolds);
}

////////////////////////////////////////////////////////////////////////////////
///

void TMVA::CvSplitBootstrappedStratified::PrepareFoldDataSet(DataSetInfo &dsi, UInt_t foldNumber, Types::ETreeType tt)
{
   if (foldNumber >= fNumFolds) {
      Log() << kFATAL << "DataSet prepared for \"" << fNumFolds << "\" folds, requested fold \"" << foldNumber
            << "\" is outside of range." << Endl;
      return;
   }

   UInt_t numFolds = fTrainSigEvents.size();

   std::vector<TMVA::Event *> *tempTrain = new std::vector<TMVA::Event *>;
   std::vector<TMVA::Event *> *tempTest = new std::vector<TMVA::Event *>;

   UInt_t nTrain = 0;
   UInt_t nTest = 0;

   // Get the number of events so the memory can be reserved.
   for (UInt_t i = 0; i < numFolds; ++i) {
      if (tt == Types::kTraining) {
         if (i != foldNumber) {
            nTrain += fTrainSigEvents.at(i).size();
            nTrain += fTrainBkgEvents.at(i).size();
         } else {
            nTest += fTrainSigEvents.at(i).size();
            nTest += fTrainSigEvents.at(i).size();
         }
      } else if (tt == Types::kValidation) {
         if (i != foldNumber) {
            nTrain += fValidSigEvents.at(i).size();
            nTrain += fValidBkgEvents.at(i).size();
         } else {
            nTest += fValidSigEvents.at(i).size();
            nTest += fValidSigEvents.at(i).size();
         }
      } else if (tt == Types::kTesting) {
         if (i != foldNumber) {
            nTrain += fTestSigEvents.at(i).size();
            nTrain += fTestBkgEvents.at(i).size();
         } else {
            nTest += fTestSigEvents.at(i).size();
            nTest += fTestSigEvents.at(i).size();
         }
      }
   }

   // Reserve memory before filling vectors
   tempTrain->reserve(nTrain);
   tempTest->reserve(nTest);

   // Fill vectors with correct folds for testing and training.
   for (UInt_t j = 0; j < numFolds; ++j) {
      if (tt == Types::kTraining) {
         if (j != foldNumber) {
            tempTrain->insert(tempTrain->end(), fTrainSigEvents.at(j).begin(), fTrainSigEvents.at(j).end());
            tempTrain->insert(tempTrain->end(), fTrainBkgEvents.at(j).begin(), fTrainBkgEvents.at(j).end());
         } else {
            tempTest->insert(tempTest->end(), fTrainSigEvents.at(j).begin(), fTrainSigEvents.at(j).end());
            tempTest->insert(tempTest->end(), fTrainBkgEvents.at(j).begin(), fTrainBkgEvents.at(j).end());
         }
      } else if (tt == Types::kValidation) {
         if (j != foldNumber) {
            tempTrain->insert(tempTrain->end(), fValidSigEvents.at(j).begin(), fValidSigEvents.at(j).end());
            tempTrain->insert(tempTrain->end(), fValidBkgEvents.at(j).begin(), fValidBkgEvents.at(j).end());
         } else {
            tempTest->insert(tempTest->end(), fValidSigEvents.at(j).begin(), fValidSigEvents.at(j).end());
            tempTest->insert(tempTest->end(), fValidBkgEvents.at(j).begin(), fValidBkgEvents.at(j).end());
         }
      } else if (tt == Types::kTesting) {
         if (j != foldNumber) {
            tempTrain->insert(tempTrain->end(), fTestSigEvents.at(j).begin(), fTestSigEvents.at(j).end());
            tempTrain->insert(tempTrain->end(), fTestBkgEvents.at(j).begin(), fTestBkgEvents.at(j).end());
         } else {
            tempTest->insert(tempTest->end(), fTestSigEvents.at(j).begin(), fTestSigEvents.at(j).end());
            tempTest->insert(tempTest->end(), fTestBkgEvents.at(j).begin(), fTestBkgEvents.at(j).end());
         }
      }
   }

   // Assign the vectors of the events to rebuild the dataset
   dsi.GetDataSet()->SetEventCollection(tempTrain, Types::kTraining, false);
   dsi.GetDataSet()->SetEventCollection(tempTest, Types::kTesting, false);
   delete tempTest;
   delete tempTrain;
}

////////////////////////////////////////////////////////////////////////////////
/// Inverse of MakeKFoldsDataSet
///

void TMVA::CvSplitBootstrappedStratified::RecombineKFoldDataSet(DataSetInfo &, Types::ETreeType)
{
   Log() << kFATAL << "Recombination not implemented for CvSplitBootstrappedStratified" << Endl;
   return;
}

////////////////////////////////////////////////////////////////////////////////
/// Splits the input vector in to equally sized randomly sampled folds.
///

std::vector<std::vector<TMVA::Event *>>
TMVA::CvSplitBootstrappedStratified::SplitSets(std::vector<TMVA::Event *> &oldSet, UInt_t numFolds)
{
   ULong64_t nEntries = oldSet.size();
   ULong64_t foldSize = nEntries / numFolds;

   std::vector<std::vector<TMVA::Event *>> tempSets;
   tempSets.resize(numFolds);

   TRandom3 r(fSeed);

   ULong64_t inSet = 0;

   for (ULong64_t i = 0; i < nEntries; i++) {
      bool inTree = false;
      if (inSet == foldSize * numFolds) {
         break;
      } else {
         while (!inTree) {
            int s = r.Integer(numFolds);
            if (tempSets.at(s).size() < foldSize) {
               tempSets.at(s).push_back(oldSet.at(i));
               inSet++;
               inTree = true;
            }
         }
      }
   }

   return tempSets;
}

/* =============================================================================
      TMVA::CvSplitCrossValidationExpr
============================================================================= */

////////////////////////////////////////////////////////////////////////////////
///

TMVA::CvSplitCrossValidationExpr::CvSplitCrossValidationExpr(DataSetInfo &dsi, TString expr)
   : fDsi(dsi), fIdxFormulaParNumFolds(std::numeric_limits<UInt_t>::max()), fSplitFormula("", expr),
     fParValues(fSplitFormula.GetNpar())
{
   if (not fSplitFormula.IsValid()) {
      throw std::runtime_error("Split expression \"" + std::string(fSplitExpr.Data()) + "\" is not a valid TFormula.");
   }

   for (Int_t iFormulaPar = 0; iFormulaPar < fSplitFormula.GetNpar(); ++iFormulaPar) {
      TString name = fSplitFormula.GetParName(iFormulaPar);

      // std::cout << "Found variable with name \"" << name << "\"." << std::endl;

      if (name == "NumFolds" or name == "numFolds") {
         // std::cout << "NumFolds|numFolds is a reserved variable! Adding to context." << std::endl;
         fIdxFormulaParNumFolds = iFormulaPar;
      } else {
         fFormulaParIdxToDsiSpecIdx.push_back(std::make_pair(iFormulaPar, GetSpectatorIndexForName(fDsi, name)));
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
///

UInt_t TMVA::CvSplitCrossValidationExpr::Eval(UInt_t numFolds, const Event *ev)
{
   for (auto &p : fFormulaParIdxToDsiSpecIdx) {
      auto iFormulaPar = p.first;
      auto iSpectator = p.second;

      fParValues.at(iFormulaPar) = ev->GetSpectator(iSpectator);
   }

   if (fIdxFormulaParNumFolds < fSplitFormula.GetNpar()) {
      fParValues[fIdxFormulaParNumFolds] = numFolds;
   }

   Double_t iFold = fSplitFormula.EvalPar(nullptr, &fParValues[0]);

   if (fabs(iFold - (double)((UInt_t)iFold)) > 1e-5) {
      throw std::runtime_error(
         "Output of splitExpr should be a non-negative integer between 0 and numFolds-1 inclusive.");
   }

   return iFold;
}

////////////////////////////////////////////////////////////////////////////////
///

Bool_t TMVA::CvSplitCrossValidationExpr::Validate(TString expr)
{
   return TFormula("", expr).IsValid();
}

////////////////////////////////////////////////////////////////////////////////
///

UInt_t TMVA::CvSplitCrossValidationExpr::GetSpectatorIndexForName(DataSetInfo &dsi, TString name)
{
   std::vector<VariableInfo> spectatorInfos = dsi.GetSpectatorInfos();

   for (UInt_t iSpectator = 0; iSpectator < spectatorInfos.size(); ++iSpectator) {
      VariableInfo vi = spectatorInfos[iSpectator];
      if (vi.GetName() == name) {
         return iSpectator;
      } else if (vi.GetLabel() == name) {
         return iSpectator;
      } else if (vi.GetExpression() == name) {
         return iSpectator;
      }
   }

   throw std::runtime_error("Spectator \"" + std::string(name.Data()) + "\" not found.");
}

/* =============================================================================
      TMVA::CvSplitCrossValidation
============================================================================= */

////////////////////////////////////////////////////////////////////////////////
/// \brief
/// \param numFolds[in] Number of folds to split data into
/// \param splitExpr[in] Expression used to split data into folds. If `""` a
///                      random assignment will be done. Otherwise the
///                      expression is fed into a TFormula and evaluated per
///                      event. The resulting value is the the fold assignment.

TMVA::CvSplitCrossValidation::CvSplitCrossValidation(UInt_t numFolds, TString splitExpr, UInt_t seed)
   : CvSplit(numFolds), fSeed(seed), fSplitExprString(splitExpr)
{
   if (not CvSplitCrossValidationExpr::Validate(fSplitExprString) and (splitExpr != TString(""))) {
      Log() << kFATAL << "Split expression \"" << fSplitExprString << "\" is not a valid TFormula." << Endl;
   }
}

////////////////////////////////////////////////////////////////////////////////
///

void TMVA::CvSplitCrossValidation::MakeKFoldDataSet(DataSetInfo &dsi)
{
   // Validate spectator
   // fSpectatorIdx = GetSpectatorIndexForName(dsi, fSpectatorName);

   if (fSplitExprString != TString("")) {
      fSplitExpr = std::unique_ptr<CvSplitCrossValidationExpr>(new CvSplitCrossValidationExpr(dsi, fSplitExprString));
   }

   // No need to do it again if the sets have already been split.
   if (fMakeFoldDataSet) {
      Log() << kINFO << "Splitting in k-folds has been already done" << Endl;
      return;
   }

   fMakeFoldDataSet = kTRUE;

   // Get the original event vectors for testing and training from the dataset.
   std::vector<Event *> trainData = dsi.GetDataSet()->GetEventCollection(Types::kTraining);
   std::vector<Event *> testData = dsi.GetDataSet()->GetEventCollection(Types::kTesting);

   // Split the sets into the number of folds.
   fTrainEvents = SplitSets(trainData, fNumFolds);
   fTestEvents = SplitSets(testData, fNumFolds);
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Set training and test set vectors of dataset described by `dsi`.
/// \param[in] dsi DataSetInfo for data set to be split
/// \param[in] foldNumber Ordinal of fold to prepare
/// \param[in] tt The set used to prepare fold. If equal to `Types::kTraining`
///               splitting will be based off the original train set. If instead
///               equal to `Types::kTesting` the test set will be used.
///               The original training/test set is the set as defined by
///               `DataLoader::PrepareTrainingAndTestSet`.
///
/// Sets the training and test set vectors of the DataSet described by `dsi` as
/// defined by the split. If `tt` is eqal to `Types::kTraining` the split will
/// be based off of the original training set.
///
/// Note: Requires `MakeKFoldDataSet` to have been called first.
///

void TMVA::CvSplitCrossValidation::PrepareFoldDataSet(DataSetInfo &dsi, UInt_t foldNumber, Types::ETreeType tt)
{
   if (foldNumber >= fNumFolds) {
      Log() << kFATAL << "DataSet prepared for \"" << fNumFolds << "\" folds, requested fold \"" << foldNumber
            << "\" is outside of range." << Endl;
      return;
   }

   auto prepareDataSetInternal = [this, &dsi, foldNumber](std::vector<std::vector<Event *>> vec) {
      UInt_t numFolds = fTrainEvents.size();

      // Events in training set (excludes current fold)
      UInt_t nTotal = std::accumulate(vec.begin(), vec.end(), 0,
                                      [&](UInt_t sum, std::vector<TMVA::Event *> v) { return sum + v.size(); });

      UInt_t nTrain = nTotal - vec.at(foldNumber).size();
      UInt_t nTest = vec.at(foldNumber).size();

      std::vector<Event *> tempTrain;
      std::vector<Event *> tempTest;

      tempTrain.reserve(nTrain);
      tempTest.reserve(nTest);

      // Insert data into training set
      for (UInt_t i = 0; i < numFolds; ++i) {
         if (i == foldNumber) {
            continue;
         }

         tempTrain.insert(tempTrain.end(), vec.at(i).begin(), vec.at(i).end());
      }

      // Insert data into test set
      tempTest.insert(tempTest.end(), vec.at(foldNumber).begin(), vec.at(foldNumber).end());

      Log() << kDEBUG << "Fold prepared, num events in training set: " << tempTrain.size() << Endl;
      Log() << kDEBUG << "Fold prepared, num events in test     set: " << tempTest.size() << Endl;

      // Assign the vectors of the events to rebuild the dataset
      dsi.GetDataSet()->SetEventCollection(&tempTrain, Types::kTraining, false);
      dsi.GetDataSet()->SetEventCollection(&tempTest, Types::kTesting, false);
   };

   if (tt == Types::kTraining) {
      prepareDataSetInternal(fTrainEvents);
   } else if (tt == Types::kTesting) {
      prepareDataSetInternal(fTestEvents);
   } else {
      Log() << kFATAL << "PrepareFoldDataSet can only work with training and testing data sets." << std::endl;
      return;
   }
}

////////////////////////////////////////////////////////////////////////////////
///

void TMVA::CvSplitCrossValidation::RecombineKFoldDataSet(DataSetInfo &dsi, Types::ETreeType tt)
{
   if (tt != Types::kTraining) {
      Log() << kFATAL << "Only kTraining is supported for CvSplitCrossValidation::RecombineKFoldDataSet currently."
            << std::endl;
   }

   std::vector<Event *> *tempVec = new std::vector<Event *>;

   for (UInt_t i = 0; i < fNumFolds; ++i) {
      tempVec->insert(tempVec->end(), fTrainEvents.at(i).begin(), fTrainEvents.at(i).end());
   }

   dsi.GetDataSet()->SetEventCollection(tempVec, Types::kTraining, false);
   dsi.GetDataSet()->SetEventCollection(tempVec, Types::kTesting, false);

   delete tempVec;
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Generates a vector of fold assignments
/// \param nEntires[in] Number of events in range
/// \param numFolds[in] Number of folds to split data into
/// \param seed[in] Random seed
///
/// Randomly assigns events to `numFolds` folds. Each fold will hold at most
/// `nEntries / numFolds + 1` events.
///

std::vector<UInt_t>
TMVA::CvSplitCrossValidation::GetEventIndexToFoldMapping(UInt_t nEntries, UInt_t numFolds, UInt_t seed)
{
   // Generate assignment of the pattern `0, 1, 2, 0, 1, 2, 0, 1 ...` for
   // `numFolds = 3`.
   std::vector<UInt_t> fOrigToFoldMapping;
   fOrigToFoldMapping.reserve(nEntries);
   for (UInt_t iEvent = 0; iEvent < nEntries; ++iEvent) {
      fOrigToFoldMapping.push_back(iEvent % numFolds);
   }

   // Shuffle assignment
   TRandom3 rng(seed);
   auto rand = [&rng](UInt_t a) { return rng.Integer(a); };
   std::random_shuffle(fOrigToFoldMapping.begin(), fOrigToFoldMapping.end(), rand);

   return fOrigToFoldMapping;
}

////////////////////////////////////////////////////////////////////////////////
/// \brief Split sets for into k-folds
/// \param oldSet[in] Original, unsplit, events
/// \param numFolds[in] Number of folds to split data into
///

std::vector<std::vector<TMVA::Event *>>
TMVA::CvSplitCrossValidation::SplitSets(std::vector<TMVA::Event *> &oldSet, UInt_t numFolds)
{
   const ULong64_t nEntries = oldSet.size();
   const ULong64_t foldSize = nEntries / numFolds;

   std::vector<std::vector<Event *>> tempSets;
   tempSets.reserve(fNumFolds);
   for (UInt_t iFold = 0; iFold < numFolds; ++iFold) {
      tempSets.emplace_back();
      tempSets.at(iFold).reserve(foldSize);
   }

   if (fSplitExpr == nullptr or fSplitExprString == "") {
      // Random split
      std::vector<UInt_t> fOrigToFoldMapping = GetEventIndexToFoldMapping(nEntries, numFolds, fSeed);

      for (UInt_t iEvent = 0; iEvent < nEntries; ++iEvent) {
         UInt_t iFold = fOrigToFoldMapping[iEvent];
         TMVA::Event *ev = oldSet[iEvent];
         tempSets.at(iFold).push_back(ev);

         fEventToFoldMapping[ev] = iFold;
      }
   } else {
      // Deterministic split
      for (ULong64_t i = 0; i < nEntries; i++) {
         TMVA::Event *ev = oldSet[i];
         // auto val = ev->GetSpectator(fSpectatorIdx);
         // tempSets.at((UInt_t)val % (UInt_t)numFolds).push_back(ev);

         UInt_t iFold = fSplitExpr->Eval(numFolds, ev);

         // std::cout << "Eval: \"" << iFold << "\"" << std::endl;
         // std::cout << "EvalRaw: \"" << fSplitFormula.EvalPar(nullptr, par_values) << "\"" << std::endl;
         tempSets.at((UInt_t)iFold).push_back(ev);
      }
   }

   return tempSets;
}
