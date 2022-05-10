#include <mpi/mpi.h>
#include "net.h"
#include <fstream>

using namespace solnet;

// Equation
// du/dt + a*du/dx = f(t,x)
numb_t f(numb_t t, numb_t x) {
    return 0;
}
// Initial conditions
// u(0, x)
numb_t space_init(numb_t x) {
    return 1;
}
// u(t, 0)
numb_t time_init(numb_t t) {
    return 1;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    NetSolver net;
    if (net.parse_config(argc, argv) != OK) {
        return EXIT_FAILURE;
    }
    double duration = MPI_Wtime();
    double pure_time = net.solve(time_init, space_init, f);
    duration = MPI_Wtime() - duration;

    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    std::cout << "[" << rank <<"] Time spent: " << duration
        << "; Pure calculation: " << pure_time << "\n";

    std::ofstream of;
    of.open("res.csv");
    net.dump(of);
    of.close();

    MPI_Finalize();
    return EXIT_SUCCESS;
}
