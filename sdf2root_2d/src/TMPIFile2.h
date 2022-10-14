// @(#)root/io:$Id$
// Author: Amit Bashyal, August 2018

/*************************************************************************
 * Copyright (C) 1995-2009, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_TMPIFile2
#define ROOT_TMPIFile2

#include "TClientInfo2.h"
#include "TBits.h"
#include "TFileMerger.h"
#include "TMemFile.h"

#include "mpi.h"

#include <vector>

class TMPIFile2 : public TMemFile {

private:
  Int_t argc;
  Int_t fEndProcess = 0;
  Int_t fSplitLevel;
  Int_t fMPIColor;

  Int_t fMPIGlobalRank;
  Int_t fMPIGlobalSize;
  Int_t fMPILocalRank;
  Int_t fMPILocalSize;

  MPI_Comm sub_comm;
  MPI_Request fRequest = 0;

  TString fMPIFilename;

  char **argv;
  char *fSendBuf = 0; // Workers' message buffer

  struct ParallelFileMerger : public TObject {
  public:
    using ClientColl_t = std::vector<TClientInfo2>;

    TString fFilename;
    TBits fClientsContact;
    UInt_t fNClientsContact;
    ClientColl_t fClients;
    TTimeStamp fLastMerge;
    TFileMerger fMerger;
    
    ParallelFileMerger(const char *filename, Int_t compression_settings, Bool_t writeCache = kFALSE);
    virtual ~ParallelFileMerger();
    
    ULong_t Hash() const;
    const char *GetName() const;
    
    Bool_t InitialMerge(TFile *input);
    Bool_t Merge();
    Bool_t NeedMerge(Float_t clientThreshold);
    Bool_t NeedFinalMerge();
    void RegisterClient(UInt_t clientID, TFile *file);
    
    TClientInfo2 tcl;
  };

  void SetOutputName();
  void CheckSplitLevel();
  void SplitMPIComm();
  void UpdateEndProcess(); // update how many workers reached end of job

public:
  TMPIFile2(const char *name, char *buffer, Long64_t size = 0, Option_t *option = "", Int_t split = 1, const char *ftitle = "", Int_t compress = 4);
  TMPIFile2(const char *name, Option_t *option = "", Int_t split = 1, const char *ftitle = "", Int_t compress = 4); // no complete implementation
  virtual ~TMPIFile2();

  // some functions on MPI information
  Int_t GetMPIGlobalSize() const;
  Int_t GetMPILocalSize() const;
  Int_t GetMPIGlobalRank() const;
  Int_t GetMPILocalRank() const;
  Int_t GetMPIColor() const;
  Int_t GetSplitLevel() const;

  // Master Functions
  void RunCollector(Bool_t cache = kFALSE);
  void R__MigrateKey(TDirectory *destination, TDirectory *source);
  void R__DeleteObject(TDirectory *dir, Bool_t withReset);
  Bool_t R__NeedInitialMerge(TDirectory *dir);
  Bool_t IsCollector();

  // Worker Functions
  void CreateBufferAndSend();
  // Empty Buffer to signal the end of job...
  void CreateEmptyBufferAndSend();
  void Sync();

  // Finalize work and save output in disk.
  void MPIClose();

  ClassDef(TMPIFile2, 0)
};
#endif
