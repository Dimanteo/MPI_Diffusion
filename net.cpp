#include "net.h"
#include <iostream>
#include <mpi/mpi.h>
#include <cmath>
#include <cassert>

namespace solnet {

MemMatrix::MemMatrix(size_t nrows, size_t ncols) :
    m_width(ncols), m_height(nrows), data(new numb_t[nrows * ncols]) {}
MemMatrix::~MemMatrix() {
    delete[](data);
}

numb_t MemMatrix::at(ssize_t row, ssize_t col) const {
    if (row < 0 || col < 0) {
        return 0;
    }
    if (row >= static_cast<ssize_t>(m_height) || col >= static_cast<ssize_t>(m_width)) {
        return 0;
    }
    return data[row * m_width + col];
}

numb_t &MemMatrix::operator()(size_t row, size_t col) {
    assert(row < m_height);
    assert(col < m_width);
    return data[row * m_width + col];
}

int NetSolver::parse_config(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config file>\n";
        return ERR;
    }
    FILE *fptr = fopen(argv[1], "r");
    if (!fptr) {
        std::cerr << "Can't open config file\n";
        return ERR;
    }
    fscanf(fptr,
    "{\n"
    "\"x\":%lf,\n"
    "\"t\":%lf,\n"
    "\"X\":%lf,\n"
    "\"T\":%lf,\n"
    "\"c\":%lf\n"
    "}",
        &x_step, &t_step, &x_max, &t_max, &param);
    fclose(fptr);
    return OK;
}

void NetSolver::printTask() {
    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    std::cout << "[" << rank << "]" << " Solving for a = " << param << "\n"
        << "Limits: max X = " << x_max << "; max T = " << t_max << ";\n"
        << "Step: dx = " << x_step << "; dt = " << t_step << ";\n"
        << "From " << from << " to " << to << "\n";
}

void NetSolver::dump(std::ostream &str) const {
    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank != 0) {
        return;
    }
    MemMatrix &U = *net_matrix.get();
    for (size_t r = 0; r < U.height(); r++) {
        str << U.at(r, 0);
        for (size_t c = 1; c < U.width(); c++) {
            str << "," << U.at(r, c);
        }
        str << "\n";
    }
}

void NetSolver::set_range(size_t l, size_t r) {
    from = l;
    to = r;
    task_width = to - from;
}

void NetSolver::split_task() {
    net_width = static_cast<size_t>(std::ceil(t_max / t_step)) + 1;
    net_height = static_cast<size_t>(std::ceil(x_max / x_step)) + 1;
    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0) {
        send_task();
    } else {
        receive_task();
    }
}

void NetSolver::send_task() {
    int comsz = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &comsz);
    tasks.resize(comsz);
    size_t load = net_width / comsz;
    size_t extra  = net_width % comsz;
    size_t range[2];
    range[0] = 0;
    range[1] = range[0] + load;
    if (extra != 0) {
        range[1] += 1;
        extra--;
    }
    set_range(range[0], range[1]);
    tasks[0] = std::make_pair(range[0], range[1]);
    MPI_Request request;
    for (int rank_i = 1; rank_i < comsz; rank_i++) {
        range[0] = range[1];
        range[1] += load;
        if (extra != 0) {
            range[1] += 1;
            extra--;
        }
        MPI_Isend(range, 2, MPI_UNSIGNED_LONG, rank_i, TASK, MPI_COMM_WORLD, &request);
        tasks[rank_i] = std::make_pair(range[0], range[1]);
    }
}

void NetSolver::receive_task() {
    size_t range[2];
    MPI_Recv(range, 2, MPI_UNSIGNED_LONG, 0, TASK, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    set_range(range[0], range[1]);
}

double NetSolver::solve(func_t tinit, func_t xinit, multifunc_t func) {
    time_init = tinit;
    space_init = xinit;
    f = func;
    split_task();
    printTask();
    init_net();
    double calc_time = MPI_Wtime();
    for (size_t layer = 1; layer < net_matrix->height(); layer++) {
        fill_layer(layer);
        synchronize(layer);
    }
    calc_time = MPI_Wtime() - calc_time;
    gather_results();
    return calc_time;
}

void NetSolver::init_net() {
    // Allocate 2 additional columns for storing values from left and right workers
    size_t alloc_width = task_width + 2;
    numb_t space_arg = static_cast<numb_t>(from - 1) * x_step;
    if (from == 0) {
        task_width--;
        // Allocate buffer with margin for all results
        alloc_width = net_width;
        space_arg = 0;
    }
    if (to == net_width && from != 0) {
        alloc_width--;
    }
    net_matrix = std::make_unique<MemMatrix>(net_height, alloc_width);

    MemMatrix &U = *net_matrix.get();
    // Apply time constraints
    if (from == 0) {
        for (size_t ti = 0; ti < U.height(); ti++) {
            U(ti, 0) = time_init(static_cast<numb_t>(ti) * t_step);
        }
    }
    // Apply space constraints
    for (size_t xi = 0; xi < U.width(); xi++) {
        U(0, xi) = space_init(space_arg);
        space_arg += x_step;
    }
}

void NetSolver::fill_layer(size_t k) {
    // k - time
    // m - space
    MemMatrix &U = *net_matrix.get();
    for (size_t m = 1; m <= task_width; m++) {
        U(k, m) = useCrossScheme(k, m, U);
    }
}

numb_t NetSolver::useCrossScheme(size_t t, size_t x, const MemMatrix &U) {
    size_t k = t - 1; // previous layer
    size_t m = x;
    numb_t time = t_step * static_cast<numb_t>(k);
    numb_t coord = x_step * static_cast<numb_t>(m);
    return U.at(k-1, m) + 2*t_step*f(time, coord) + (U.at(k, m-1) - U.at(k, m+1)) * t_step * param / x_step;
}

void NetSolver::synchronize(size_t layer) {
    int commsz = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &commsz);
    if (commsz == 1) {
        return;
    }
    MemMatrix &U = *net_matrix.get();
    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    auto send_syn = [&](int dst_rank, size_t val_idx, int tag) {
        MPI_Request request;
        numb_t sendbuf = U.at(layer, val_idx);
        MPI_Isend(&sendbuf, 1, MPI_DOUBLE, dst_rank, tag, MPI_COMM_WORLD, &request);
    };

    if (to != net_width) {
        send_syn(rank + 1, task_width, LEFT_SYN);
    }
    if (from != 0) {
        send_syn(rank - 1, 1, RIGHT_SYN);
    }

    auto recv_syn = [&](int src_rank, size_t val_idx, int tag) {
        numb_t recbuf = 0;
        MPI_Recv(&recbuf, 1, MPI_DOUBLE, src_rank, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        U(layer, val_idx) = recbuf;
    };
    if (to != net_width) {
        recv_syn(rank + 1, task_width + 1, RIGHT_SYN);
    }
    if (from != 0) {
        recv_syn(rank - 1, 0, LEFT_SYN);
    }
}

void NetSolver::gather_results() {
    MemMatrix &U = *net_matrix.get();
    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int commsz = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &commsz);
    if (commsz == 1)
        return;
    for (size_t row = 1; row < net_height; row ++) {
        if (rank == 0) {
            for (int ri = 1; ri < commsz; ri++) {
                // Only works with plain memory array
                numb_t *buf = &U(row, tasks[ri].first);
                int count = tasks[ri].second - tasks[ri].first;
                MPI_Recv(buf, count, MPI_DOUBLE, ri, RESULT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        } else {
            numb_t *buf = &U(row, 1);
            MPI_Send(buf, task_width, MPI_DOUBLE, 0, RESULT, MPI_COMM_WORLD);
        }
    }
}

};