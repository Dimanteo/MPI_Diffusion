if [ $# -eq 1 ]
then
    mpirun -n $1 ./net config | tee out.txt
else
    mpirun -n 4 ./net config | tee out.txt
fi;