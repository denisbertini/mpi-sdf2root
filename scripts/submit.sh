export SIMDIR=/lustre/rz/dbertini/root_io/git

sbatch --nodes 20 --tasks-per-node 5 --ntasks-per-core 1 --cpus-per-task 1 --no-requeue --job-name r_mpi --mem-per-cpu 4000 --mail-type ALL --mail-user d.bertini@gsi.de --partition main --time 0-08:00:00 -D $SIMDIR/data -o %j.out.log -e %j.err.log   -- ./run-file.sh

