import csv
import numpy as np
import matplotlib.pyplot as plt
import json

class Config:
    def __init__(self) -> None:
        self.x_step = 0
        self.t_step = 0
        self.x_max = 0
        self.t_max = 0
        self.param = 0


def read_config(file):
    config = Config()
    with open(file, 'r') as f:
        js = json.load(f)
        config.x_step = js['x']
        config.t_step = js['t']
        config.x_max  = js['X']
        config.t_max  = js['T']
        config.param  = js['c']
    return config


def get_data(file):
    data = []
    with open(file, 'r') as f:
        table = csv.reader(f, delimiter=',')
        for row in table:
            floatrow = []
            for val in row:
                floatrow.append(float(val))
            data.append(floatrow)
    return data


def plot_data(data, config):
    t_size = len(data)
    x_size = len(data[0])
    x_axis = []
    t_axis = []
    for i in range(x_size):
        x_axis.append(config.x_step * i)
    for i in range(t_size):
        t_axis.append(config.t_step * i)
    xgrid, ygrid = np.meshgrid(x_axis, t_axis)
    fig = plt.figure(figsize=[12, 8])
    axes = fig.add_subplot(projection='3d')
    axes.plot_surface(xgrid, ygrid, np.array(data))
    plt.title('c = ' + str(config.param))
    plt.ylabel('t')
    plt.xlabel('x')
    plt.show()


def main():
    config = read_config('config')
    data = get_data('res.csv')
    plot_data(data, config)


if __name__ == '__main__':
    main()