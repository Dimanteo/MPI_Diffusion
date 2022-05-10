#pragma once
#include <memory>
#include <functional>
#include <vector>
#include <ostream>

namespace solnet {

using numb_t = double;

enum RET_CODES {
    ERR = -1,
    OK = 0
};

enum MSG_TAG {
    TASK = 1,
    SYN,
    LEFT_SYN,
    RIGHT_SYN,
    RESULT
};

struct MemMatrix {
    MemMatrix(size_t nrows, size_t ncols);
    ~MemMatrix();
    numb_t &operator()(size_t row, size_t col);
    numb_t at(ssize_t row, ssize_t col) const;
    size_t width() const { return m_width; };
    size_t height() const { return m_height; };
  private:
    size_t m_width;
    size_t m_height;
    numb_t *data;
};

class NetSolver {
    numb_t x_step;
    numb_t t_step;
    numb_t x_max;
    numb_t t_max;
    size_t net_width;
    size_t net_height;
    size_t task_width;
    // Indexes [from; to)
    size_t from, to;
    // Only for control thread. Worker loads decription
    std::vector<std::pair<size_t, size_t>> tasks;
    // Equation
    // du/dt + c*du/dx = f(t,x)
    numb_t param; // param "c" in equation

    using multifunc_t = std::function<numb_t(numb_t, numb_t)>;
    using func_t = std::function<numb_t(numb_t)>;
    // f(t, x)
    multifunc_t f;
    // Initial conditions
    // u(0, x)
    func_t space_init;
    // u(t, 0)
    func_t time_init;

    std::unique_ptr<MemMatrix> net_matrix;

    void printTask();
    void split_task();
    void set_range(size_t l, size_t r);
    void send_task();
    void receive_task();
    void init_net();
    void fill_layer(size_t layer);
    void synchronize(size_t layer);
    numb_t useCrossScheme(size_t t, size_t x, const MemMatrix &U);
    void gather_results();

  public:
    double solve(func_t tinit, func_t xinit, multifunc_t func);
    int parse_config(int argc, char **argv);
    numb_t get_xmax() const { return x_max; }
    numb_t get_tmax() const { return t_max; }
    numb_t get_xstep() const { return x_step; }
    numb_t get_tstep() const { return t_step; }
    void dump(std::ostream &str) const;
};

};