#!/bin/bash
export OMP_NUM_THREADS=1
export SINGULARITY_DISABLE_CACHE=1
export SIMDIR=/lustre/rz/dbertini/root_io/git

export SDF_FILE=/lustre/rz/dbertini/collisions/data/5kev/0001.sdf

# Set environment
. $SIMDIR/scripts/setup_mpi.sh -c gcc8
mpicc -showme:version
type gcc
type mpicc
type mpirun
type srun
type root

# Set executable directory
export PATH=$SIMDIR/bin:$PATH
export LD_LIBRARY_PATH=$SIMDIR/lib:$LD_LIBRARY_PATH
export ROOT_INCLUDE_PATH=$SIMDIR/include/:$ROOT_INCLUDE_PATH
echo " "
echo "MPI-SDF2ROOT dev: setup done ... "
echo " "
#echo $PATH
#echo $LD_LIBRARY_PATH
ulimit -c 0


export OMPI_MCA_mpi_warn_on_fork=0
#export OMPI_MCA_io=romio321
#export OMPI_MCA_mpi_leave_pinned=0
#export OMPI_MCA_btl_openib_allow_ib=1
#export OMPI_MCA_btl_openib_warn_no_device_params_found=0
#export OMPI_MCA_btl_openib_rdma_pipeline_send_length=100000000
#export OMPI_MCA_btl_openib_rdma_pipeline_frag_size=100000000

#ldd $SIMDIR/sdf2root/bin/riotest
#                                            riotest     -c <n_aggreg> -r <sync_rate> -n <evt/worker>         
echo "." | srun --export=ALL -- $SIMDIR/bin/sdf2root_2d -f $SDF_FILE -s deuteron:neutron 


