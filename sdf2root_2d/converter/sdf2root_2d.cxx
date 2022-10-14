#include "TError.h"
#include "TFile.h"
#include "TH1D.h"
#include "TMPIFile2.h"
#include "TMemFile.h"
#include "TROOT.h"
#include "TRandom.h"
#include "TSystem.h"
#include "TTree.h"

#include "cxxopts.hpp"
#include "mpi.h"

#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

extern "C" {
  struct part{ int l_px, l_py, l_pz; double* px, *py, *pz;};    
  void read_particle(const char* cstring, int clen,
		     const char* spec,    int len_sp,
		     part* arrays);
  void init_read();
}

std::vector<std::string> split(const char *str, char c = ':')
{
  std::vector<std::string> result;
    do
    {
        const char *begin = str;
        while(*str != c && *str)
            str++;
        result.push_back(std::string(begin, str));
    } while (0 != *str++);

    return result;
}

void sdf_io(int argc, char *argv[]) {
    
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  
  std::string sdf_file;
  std::string species_name;
  
  cxxopts::Options optparse("sdf_io", "runs tests on sdf_io interface");
  optparse.add_options()(
			 "f,sdf_file", "sdf file to process",
                         cxxopts::value<std::string>(sdf_file))
			 ("s,species_name", "species_name",
			  cxxopts::value<std::string>(species_name));

  auto opts = optparse.parse(argc, argv);
  if (rank == 0 ){
    std::cout << "sdf2root_2d: file to process: " << sdf_file.c_str() << std::endl;
    std::cout << "sdf2root_2d: species name list: " << species_name.c_str() << std::endl;
  }

  // Get every species  
  std::vector<std::string> species_list = split(species_name.c_str());

  // Create the TTree
  std::string root_file=sdf_file.substr(0,sdf_file.find_last_of('.'));
  root_file += "_r"+std::to_string(rank);
  root_file +=".root";

  // Create the TTree
  TFile f(root_file.c_str(),"recreate");
  f.SetCompressionLevel(0);
  //TMPIFile2 *newfile = new TMPIFile2(root_file.c_str(), "RECREATE", 1);
  TTree tree[species_list.size()];
  // Kinematics
  Double_t px[species_list.size()];
  Double_t py[species_list.size()];
  Double_t pz[species_list.size()];

  for (size_t i=0; i < species_list.size(); i++) {
    tree[i].SetName(species_list[i].c_str());
    tree[i].SetTitle(species_list[i].c_str());
    std::string spec_px = species_list[i] + "_px";
    std::string spec_px_attr = species_list[i] + "_px/D";
    std::string spec_py = species_list[i] + "_py";
    std::string spec_py_attr = species_list[i] + "_py/D";
    std::string spec_pz = species_list[i] + "_pz";
    std::string spec_pz_attr = species_list[i] + "_pz/D";  
    
    tree[i].Branch(spec_px.c_str(),&px[i],spec_px_attr.c_str());
    tree[i].Branch(spec_py.c_str(),&py[i],spec_py_attr.c_str());
    tree[i].Branch(spec_pz.c_str(),&pz[i],spec_pz_attr.c_str());
  }

  // Initialise ONCE the setup for MPI-I/O
  init_read();

  // Loop for all species and fetch data in parallel
  for (size_t i=0; i < species_list.size(); i++) {
    std::string spec = species_list[i];
    //std::cout<< " species: " << spec << std::endl; 
    part arrays;
    read_particle(sdf_file.c_str(), strlen(sdf_file.c_str()),
                  spec.c_str(),     strlen(spec.c_str()),  
		  &arrays);    
    
    //std::cout << "species: " << spec << " size: " << arrays.l_px << std::endl;

    for (int k=0; k<arrays.l_px; k++){
      if ( ( (k%1000000) == 0 ) && (rank == 0) )
	std::cout << "rank: " << rank <<  " k: " << k << " px: " << arrays.px[k]
		  << " py: " << arrays.py[k] << " pz: " << arrays.pz[k] << std::endl; 
      px[i] = arrays.px[k];
      py[i] = arrays.py[k];
      pz[i] = arrays.pz[k];      
      tree[i].Fill();
    }    
  }//!species

  // if ( rank == 0 )
    std::cout << "writing output ROOT file: "<< root_file << std::endl;

  // Write the trees
  for (size_t i=0; i < species_list.size(); i++) tree[i].Write();
  
}


#ifndef __CINT__
int main(int argc, char *argv[]) {

  auto start = std::chrono::high_resolution_clock::now();

  int rank, size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  
  sdf_io(argc, argv);
  
  auto end = std::chrono::high_resolution_clock::now();
  double time =
      std::chrono::duration_cast<std::chrono::duration<double>>(end - start)
          .count();
  std::string msg = "Total elapsed time: ";
  msg += std::to_string(time);
  msg += "\tTotal rank: ";
  msg += std::to_string(size);
  if (rank == 0)
    Info("SDFROOT: test", msg.c_str());

  //MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();

  return 0;
}
#endif

