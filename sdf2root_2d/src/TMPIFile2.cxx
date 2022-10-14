// @(#)root/io:$Id$
// Author: Amit Bashyal, August 2018

/*************************************************************************
 * Copyright (C) 1995-2002, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "TMPIFile2.h"
#include "TFileCacheWrite.h"
#include "TKey.h"
#include "TMath.h"
#include "THashTable.h"

#include <chrono>
#include <iostream>
#include <string>
#include <iostream>     // std::cout
#include <sstream>      // std::stringstream



ClassImp(TMPIFile2);

const Int_t MIN_FILE_NUM = 2;

TMPIFile2::TMPIFile2(const char *name, char *buffer, Long64_t size,
                   Option_t *option, Int_t split, const char *ftitle,
                   Int_t compress)
    : TMemFile(name, buffer, size, option, ftitle, compress), fSplitLevel(split), fMPIColor(0), fRequest(0), fSendBuf(0)
{
  CheckSplitLevel();
  SplitMPIComm();
}

TMPIFile2::TMPIFile2(const char *name, Option_t *option, Int_t split,
                   const char *ftitle, Int_t compress)
    : TMemFile(name, option, ftitle, compress), fSplitLevel(split), fMPIColor(0), fRequest(0), fSendBuf(0)
{
  CheckSplitLevel();
  SplitMPIComm();
}

TMPIFile2::~TMPIFile2() {
  Close();
  if (fSplitLevel > 1) {
    MPI_Comm_free(&sub_comm);
  }
}

void TMPIFile2::UpdateEndProcess()
{
  fEndProcess++;
}

void TMPIFile2::RunCollector(Bool_t cache) {
  this->SetOutputName();
  THashTable mergers;
  
  Int_t client_Id = 0;
  Int_t msg_received = 0;
  
  auto run_start = std::chrono::high_resolution_clock::now();

  std::cout << "Collector run time\t probe time\t merge time\t buffer size (MB)\t "
               "MiB/s \t msg/s \t merge counter\t "
               "while time\n";

  while (fEndProcess != fMPILocalSize - 1) {

    // check if message has been received
    MPI_Status status;
    auto probe_start = std::chrono::high_resolution_clock::now();
    MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, sub_comm, &status);
    auto probe_end = std::chrono::high_resolution_clock::now();
    double probe_time =
        std::chrono::duration_cast<std::chrono::duration<double>>(probe_end -
                                                                  probe_start)
            .count();
    std::stringstream timing_msg;

    auto while_start = std::chrono::high_resolution_clock::now();
    double run_time = std::chrono::duration_cast<std::chrono::duration<double>>(
                          while_start - run_start).count();
    timing_msg <<  "[" << this->GetMPIColor() << "] "
	       << "[" << this->GetMPILocalRank() << "] ";
    timing_msg << "C_info: r_t: " << run_time << "\t  p_t: "  <<probe_time;

    // get bytes received
    Int_t count;
    MPI_Get_count(&status, MPI_CHAR, &count);
    Int_t number_bytes = sizeof(char) * count;

    // create buffer to receive message
    char *buf = new char[number_bytes];
    Int_t source = status.MPI_SOURCE;
    Int_t tag = status.MPI_TAG;

    if (number_bytes == 0) {
      // empty buffer is a worker's last send request....
      this->UpdateEndProcess();
      MPI_Recv(buf, number_bytes, MPI_CHAR, source, tag, sub_comm,
               MPI_STATUS_IGNORE);
    } else {

      MPI_Recv(buf, number_bytes, MPI_CHAR, source, tag, sub_comm,
               MPI_STATUS_IGNORE);

      auto merge_start = std::chrono::high_resolution_clock::now();
      msg_received++;

      TMemFile *infile = new TMemFile(fMPIFilename, buf, number_bytes, "UPDATE");
      if (infile->IsZombie()) {
        exit(1);
      }
      infile->SetCompressionSettings(this->GetCompressionSettings());

      ParallelFileMerger *info = (ParallelFileMerger *)mergers.FindObject(fMPIFilename);
      if (!info) {
        info = new ParallelFileMerger(fMPIFilename, this->GetCompressionSettings(), cache);
        mergers.Add(info);
      }
      if (R__NeedInitialMerge(infile)) {
        info->InitialMerge(infile);
      }

      info->RegisterClient(client_Id, infile);
      info->Merge();
      infile = 0;

      auto merge_end = std::chrono::high_resolution_clock::now();

      double merge_time =
          std::chrono::duration_cast<std::chrono::duration<double>>(merge_end -
                                                                    merge_start)
              .count();
      run_time = std::chrono::duration_cast<std::chrono::duration<double>>(
                     merge_end - run_start)
                     .count();
      double megabytes_per_second = number_bytes / merge_time / 1024. / 1024.;
      double messages_per_second = msg_received / run_time;
      timing_msg << "\t " << " m_t: " << merge_time << "\t  b_size (MiB): "
                 << (float(number_bytes) / 1024. / 1024.) << "\t rate (MiB/s): "
                 << megabytes_per_second << "\t [msg/s] " << messages_per_second
                 << "\t [msg_rec]: " << msg_received << "\t ";
      client_Id++;
    }
    delete [] buf;

    auto while_end = std::chrono::high_resolution_clock::now();
    double while_time =
        std::chrono::duration_cast<std::chrono::duration<double>>(while_end -
                                                                  while_start)
            .count();
    timing_msg << " loop_t: " << while_time << std::endl;
    if (timing_msg.str().size() > 40)
      std::cout << timing_msg.str();
  }

  if (fEndProcess == fMPILocalSize - 1) {
    mergers.Delete();
    return;
  }
}

Bool_t TMPIFile2::R__NeedInitialMerge(TDirectory *dir) {
  if (dir == 0)
    return kFALSE;
  TIter nextkey(dir->GetListOfKeys());
  TKey *key;
  while ((key = (TKey *)nextkey())) {
    TClass *cl = TClass::GetClass(key->GetClassName());
    if (cl->InheritsFrom(TDirectory::Class())) {
      TDirectory *subdir =
          (TDirectory *)dir->GetList()->FindObject(key->GetName());
      if (!subdir) {
        subdir = (TDirectory *)key->ReadObj();
      }
      if (R__NeedInitialMerge(subdir)) {
        return kTRUE;
      }
    } else {
      if (0 != cl->GetResetAfterMerge()) {
        return kTRUE;
      }
    }
  }
  return kFALSE;
}

TMPIFile2::ParallelFileMerger::ParallelFileMerger(const char *filename,
                                                 Int_t compression_settings,
                                                 Bool_t writeCache)
    : fFilename(filename), fClientsContact(0), fMerger(kFALSE, kTRUE) {
  fMerger.SetPrintLevel(0);
  if (!fMerger.OutputFile(filename, "RECREATE"))
    exit(1);
  fMerger.GetOutputFile()->SetCompressionSettings(compression_settings);
  if (writeCache)
    new TFileCacheWrite(fMerger.GetOutputFile(), 32 * 1024 * 1024);
}

TMPIFile2::ParallelFileMerger::~ParallelFileMerger() {
  for (ClientColl_t::iterator iter = fClients.begin(); iter != fClients.end();
       ++iter)
    delete iter->GetFile();
}

ULong_t TMPIFile2::ParallelFileMerger::Hash() const { return fFilename.Hash(); }
const char *TMPIFile2::ParallelFileMerger::GetName() const { return fFilename; }

Bool_t TMPIFile2::ParallelFileMerger::InitialMerge(TFile *input) {
  // Initial merge of the input to copy the resetable object (TTree) into the
  // output and remove them from the input file.
  fMerger.AddFile(input);
  Bool_t result =
      fMerger.PartialMerge(TFileMerger::kIncremental | TFileMerger::kResetable |
                           TFileMerger::kKeepCompression);
  tcl.R__DeleteObject(input, kTRUE);
  return result;
}

Bool_t TMPIFile2::ParallelFileMerger::Merge() {
  tcl.R__DeleteObject(
      fMerger.GetOutputFile(),
      kFALSE); // removing object that cannot be incrementally merged and will
               // not be reset by the client code..
  for (UInt_t f = 0; f < fClients.size(); ++f) {
    fMerger.AddFile(fClients[f].GetFile());
  }
  Bool_t result = fMerger.PartialMerge(TFileMerger::kAllIncremental |
                                       TFileMerger::kKeepCompression);
  // Remove any 'resetable' object (like TTree) from the input file so that they
  // will not be re-merged.  Keep only the object that always need to be
  // re-merged (Histograms).
  for (UInt_t f = 0; f < fClients.size(); ++f) {
    if (fClients[f].GetFile()) {
      tcl.R__DeleteObject(fClients[f].GetFile(), kTRUE);
    } else {
      // We back up the file (probably due to memory constraint)
      TFile *file = TFile::Open(fClients[f].GetLocalName(), "UPDATE");
      if (file->IsZombie())
        exit(1);
      tcl.R__DeleteObject(file,
                          kTRUE); // Remove object that can be incrementally
                                  // merge and will be reset by the client code.
      file->Write();
      delete file;
    }
  }
  fLastMerge = TTimeStamp();
  fNClientsContact = 0;
  fClientsContact.Clear();

  return result;
}

void TMPIFile2::ParallelFileMerger::RegisterClient(UInt_t clientID,
                                                  TFile *file) {
  // Register that a client has sent a file.

  ++fNClientsContact;
  fClientsContact.SetBitNumber(clientID);
  TClientInfo2 ntcl(std::string(fFilename).c_str(), clientID);
  if (fClients.size() < clientID + 1) {
    fClients.push_back(ntcl);
  }
  fClients[clientID].SetFile(file);
}

Bool_t TMPIFile2::ParallelFileMerger::NeedMerge(Float_t clientThreshold) {
  // Return true, if enough client have reported
  // in case of TMPIFile2 this happens everytime a client/worker sends the buffer
  // (tested).

  if (fClients.size() == 0) {
    return kFALSE;
  }

  // Calculate average and rms of the time between the last 2 contacts.
  Double_t sum = 0.;
  Double_t sum2 = 0.;
  for (UInt_t c = 0; c < fClients.size(); ++c) {
    sum += fClients[c].GetTimeSincePrevContact();
    sum2 +=
        fClients[c].GetTimeSincePrevContact() * fClients[c].GetTimeSincePrevContact();
  }
  Double_t avg = sum / fClients.size();
  Double_t sigma = sum2 ? TMath::Sqrt(sum2 / fClients.size() - avg * avg) : 0;
  Double_t target = avg + 2 * sigma;
  TTimeStamp now;
  if ((now.AsDouble() - fLastMerge.AsDouble()) > target) {
    return kTRUE;
  }
  Float_t cut = clientThreshold * fClients.size();
  return fClientsContact.CountBits() > cut || fNClientsContact > 2 * cut;
}

Bool_t TMPIFile2::ParallelFileMerger::NeedFinalMerge() {
  return fClientsContact.CountBits() > 0;
}

void TMPIFile2::R__MigrateKey(TDirectory *destination, TDirectory *source) {
  if (destination == 0 || source == 0)
    return;
  TIter nextkey(source->GetListOfKeys());
  TKey *key;
  while ((key = (TKey *)nextkey())) {
    TClass *cl = TClass::GetClass(key->GetClassName());
    if (cl->InheritsFrom(TDirectory::Class())) {
      TDirectory *source_subdir =
          (TDirectory *)source->GetList()->FindObject(key->GetName());
      if (!source_subdir) {
        source_subdir = (TDirectory *)key->ReadObj();
      }
      TDirectory *destination_subdir =
          destination->GetDirectory(key->GetName());
      if (!destination_subdir) {
        destination_subdir = destination->mkdir(key->GetName());
      }
      R__MigrateKey(destination, source);
    } else {
      TKey *oldkey = destination->GetKey(key->GetName());
      if (oldkey) {
        oldkey->Delete();
        delete oldkey;
      }
      TKey *newkey = new TKey(destination, *key, 0 /* pidoffset */);
      destination->GetFile()->SumBuffer(newkey->GetObjlen());
      newkey->WriteFile(0);
      if (destination->GetFile()->TestBit(TFile::kWriteError)) {
        return;
      }
    }
  }
  destination->SaveSelf();
}

void TMPIFile2::R__DeleteObject(TDirectory *dir, Bool_t withReset) {
  if (dir == 0)
    return;

  TIter nextkey(dir->GetListOfKeys());
  TKey *key;
  while ((key = (TKey *)nextkey())) {
    TClass *cl = TClass::GetClass(key->GetClassName());
    if (cl->InheritsFrom(TDirectory::Class())) {
      TDirectory *subdir =
          (TDirectory *)dir->GetList()->FindObject(key->GetName());
      if (!subdir) {
        subdir = (TDirectory *)key->ReadObj();
      }
      R__DeleteObject(subdir, withReset);
    } else {
      Bool_t todelete = kFALSE;
      if (withReset) {
        todelete = (0 != cl->GetResetAfterMerge());
      } else {
        todelete = (0 == cl->GetResetAfterMerge());
      }
      if (todelete) {
        key->Delete();
        dir->GetListOfKeys()->Remove(key);
        delete key;
      }
    }
  }
}

Bool_t TMPIFile2::IsCollector() {
  if (this->fMPILocalRank) {
    return kFALSE;
  }
  return kTRUE;
}

void TMPIFile2::CreateBufferAndSend() {
  if (this->IsCollector()) {
    SysError("CreateBufferAndSend"," should not be called by a collector");
    exit(1);
  }
  this->Write();
  Int_t count = this->GetEND();
  fSendBuf = new char[count];
  this->CopyTo(fSendBuf, count);
  MPI_Isend(fSendBuf, count, MPI_CHAR, 0, fMPIColor, sub_comm, &fRequest);
}

void TMPIFile2::CreateEmptyBufferAndSend() {
  if (this->IsCollector()) {
    return;
  }

  if (fRequest) {
    auto start = std::chrono::high_resolution_clock::now();
    MPI_Wait(&fRequest, MPI_STATUS_IGNORE);
    auto end = std::chrono::high_resolution_clock::now();
    double time = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
    std::cout << "[" << fMPIColor << "]"
              << "[" << fMPILocalRank << "] wait time: "
              << time << std::endl;
    fRequest = 0;
    delete [] fSendBuf;
  }
  Int_t sent = MPI_Send(fSendBuf, 0, MPI_CHAR, 0, fMPIColor, sub_comm);
  if (sent) {
    delete [] fSendBuf;
  }
}

// Synching defines the communication method between worker/collector
void TMPIFile2::Sync() {
  // check if the previous send request is accepted by master.
  if (!fRequest) { // if accpeted create and send current batch
    CreateBufferAndSend();
  } else {
    // if not accepted wait until received by master
    auto start = std::chrono::high_resolution_clock::now();
    MPI_Wait(&fRequest, MPI_STATUS_IGNORE);
    auto end = std::chrono::high_resolution_clock::now();
    double time = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
    std::cout << "[" << fMPIColor << "]"
              << "[" << fMPILocalRank << "] wait time: "
              << time << std::endl;
    delete[] fSendBuf; // empty the buffer once received by master
    // Reset the frequest once accepted by master and send new buffer
    fRequest = 0;
    CreateBufferAndSend();
  }
  this->ResetAfterMerge((TFileMergeInfo *)0);
}

void TMPIFile2::MPIClose() {
  CreateEmptyBufferAndSend();
  this->Close();
}

void TMPIFile2::SetOutputName() {
  std::string _filename = this->GetName();

  ULong_t found = _filename.rfind(".root");
  if (found != std::string::npos) {
    _filename.resize(found);
  }
  fMPIFilename = _filename;
  fMPIFilename += "_";
  fMPIFilename += fMPIColor;
  fMPIFilename += ".root";
}

void TMPIFile2::CheckSplitLevel() {
  if (fSplitLevel < 1) {
    Error("CheckSplitLevel", "At least one collector is required instead of %d", fSplitLevel);
    exit(1);
  }
}

void TMPIFile2::SplitMPIComm() {
  // Initialize MPI if it is not already initialized...
  Int_t flag;
  MPI_Initialized(&flag);
  if (!flag) {
    MPI_Init(&argc, &argv);
  }

  MPI_Comm_size(MPI_COMM_WORLD, &fMPIGlobalSize);
  MPI_Comm_rank(MPI_COMM_WORLD, &fMPIGlobalRank);

  if (MIN_FILE_NUM * fSplitLevel > fMPIGlobalSize) {
    Error("TMPIFile2", " Number of Output File is larger than number of Processors Allocated. Number of processors should be two times larger than outpts. For %d outputs at least %d should be allocated instead of %d", fSplitLevel, MIN_FILE_NUM * fSplitLevel, fMPIGlobalSize);
    exit(1);
  }

  if (fSplitLevel == 1) {
    sub_comm = MPI_COMM_WORLD;
  } else {
    Int_t comm_size = fMPIGlobalSize / fSplitLevel;
    if (fMPIGlobalSize % fSplitLevel != 0) {
      comm_size++;
    }
    fMPIColor = fMPIGlobalRank / comm_size;
    MPI_Comm_split(MPI_COMM_WORLD, fMPIColor, fMPIGlobalRank, &sub_comm);
  }
  MPI_Comm_size(sub_comm, &fMPILocalSize);
  MPI_Comm_rank(sub_comm, &fMPILocalRank);
}

Int_t TMPIFile2::GetMPIGlobalSize() const
{
  return fMPIGlobalSize;
}

Int_t TMPIFile2::GetMPILocalSize() const
{
  return fMPILocalSize;
}

Int_t TMPIFile2::GetMPIGlobalRank() const
{
  return fMPIGlobalRank;
}

Int_t TMPIFile2::GetMPILocalRank() const
{
  return fMPILocalRank;
}

Int_t TMPIFile2::GetMPIColor() const
{
  return fMPIColor;
}

Int_t TMPIFile2::GetSplitLevel() const
{
  return fSplitLevel;
}
