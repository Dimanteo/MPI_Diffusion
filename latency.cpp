#include <mpi/mpi.h>

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank = 0;
    int commsz = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &commsz);
    if (commsz < 2) {
        std::cerr << "Run more then 1 node\n";
        return EXIT_FAILURE;
    }
    double blob = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Request request;
    if (rank == 0) {
        double send_ts = MPI_Wtime();
        MPI_Send(&blob, 1, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD);
        double recv_ts = MPI_Wtime();
        std::cout << "Net latency: " << recv_ts - send_ts << "\n";
    } else if (rank == 1) {
        MPI_Recv(&blob, 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    MPI_Finalize();
    return EXIT_SUCCESS;
}