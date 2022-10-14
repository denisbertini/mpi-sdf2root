// @(#)root/io:$Id$
// Author: Amit Bashyal, August 2018

/*************************************************************************
 * Copyright (C) 1995-2009, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_TClientInfo2
#define ROOT_TClientInfo2

#include "TFile.h"
#include "TTimeStamp.h"

class TClientInfo2 {

private:
  TFile *fFile;
  TString fLocalName;
  UInt_t fContactsCount;
  TTimeStamp fLastContact;
  Double_t fTimeSincePrevContact;

public:
  TClientInfo2();                                      // default constructor
  TClientInfo2(const char *filename, UInt_t clientID); // another constructor
  virtual ~TClientInfo2();

  TFile *GetFile() const {return fFile;}
  TString GetLocalName() const {return fLocalName;}
  Double_t GetTimeSincePrevContact() const {return fTimeSincePrevContact;}

  void SetFile(TFile *file);

  void R__MigrateKey(TDirectory *destination, TDirectory *source);
  void R__DeleteObject(TDirectory *dir, Bool_t withReset);

  ClassDef(TClientInfo2, 0);
};
#endif
