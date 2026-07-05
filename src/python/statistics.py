import os
import csv
import sys
import numpy as np


def load(origin, approx):
    origin_data = []
    approx_data = []

    with open(origin, 'r') as file:
        csvFile = csv.reader(file)
        for line in csvFile:
            origin_data.append([int(line[0]), float(line[1])])
        
        origin_data = sorted(origin_data, key=lambda a_entry: a_entry[0]) 
                
    with open(approx, 'r') as file:
        csvFile = csv.reader(file)
        for line in csvFile:
            approx_data.append([int(line[0]), float(line[1])])
        
        approx_data = sorted(approx_data, key=lambda a_entry: a_entry[0]) 
    
    return np.array(origin_data)[:, 1], np.array(approx_data)[:, 1]


def load_memory_baseline(file=".memory_baseline"):
    if not os.path.exists(file):
        return 0, 0

    with open(file, 'r') as file:
        line = file.readline().strip().split(',')
        if len(line) < 2:
            return 0, 0
        return int(line[0]), int(line[1])


def load_monitor(file):
    c_max_vsz, c_max_rss = float("-inf"), float("-inf")
    d_max_vsz, d_max_rss = float("-inf"), float("-inf")
    baseline_vsz, baseline_rss = load_memory_baseline()
    
    with open(file, 'r') as file:
        csvFile = csv.reader(file)
        next(csvFile, None)
        for line in csvFile:
            if len(line) < 4:
                continue

            c_vsz = int(line[2])
            c_rss = int(line[3])
            phase = line[4] if len(line) > 4 else "compression"

            if phase == "compression":
                c_max_vsz = c_max_vsz if c_max_vsz > c_vsz else c_vsz
                c_max_rss = c_max_rss if c_max_rss > c_rss else c_rss
            elif phase == "decompression":
                d_max_vsz = d_max_vsz if d_max_vsz > c_vsz else c_vsz
                d_max_rss = d_max_rss if d_max_rss > c_rss else c_rss

    if c_max_vsz == float("-inf"):
        c_max_vsz, c_max_rss = baseline_vsz, baseline_rss
    if d_max_vsz == float("-inf"):
        d_max_vsz, d_max_rss = baseline_vsz, baseline_rss

    c_max_vsz = max(c_max_vsz - baseline_vsz, 0)
    c_max_rss = max(c_max_rss - baseline_rss, 0)
    d_max_vsz = max(d_max_vsz - baseline_vsz, 0)
    d_max_rss = max(d_max_rss - baseline_rss, 0)
        
    return c_max_vsz, c_max_rss, d_max_vsz, d_max_rss


def rmse(origin_data, approx_data):
    return np.sqrt(np.square(origin_data - approx_data).mean())

def mse(origin_data, approx_data):
    return np.square(origin_data - approx_data).mean()

def mae(origin_data, approx_data):
    return np.abs(origin_data - approx_data).mean()

def snr(origin_data, approx_data, eps=1e-10):
    return 10 * np.log10(np.mean(origin_data ** 2) / (mse(origin_data, approx_data) + eps))

def psnr(origin_data, approx_data, eps=1e-10):
    return 10 * np.log10((np.max(origin_data) ** 2) / (mse(origin_data, approx_data) + eps))

def maxdiff(origin_data, approx_data):
    return np.max(np.abs(origin_data - approx_data))

def mindiff(origin_data, approx_data):
    return np.min(np.abs(origin_data - approx_data))
      
def compressratio(origin, approx):
    return os.path.getsize(origin) / os.path.getsize(approx)

def pearsoncorr(origin, approx):
    return np.corrcoef(origin, approx)[0, 1]

def ssim(origin, approx, win_size=1000, K1=0.01, K2=0.03):
    data_range = origin.max() - origin.min()

    C1 = (K1 * data_range) ** 2
    C2 = (K2 * data_range) ** 2

    w = np.ones(win_size) / win_size
    mu_x = np.convolve(origin, w, mode="valid")
    mu_y = np.convolve(approx, w, mode="valid")

    sigma_x2 = np.convolve(origin*origin, w, mode="valid") - mu_x**2
    sigma_y2 = np.convolve(approx*approx, w, mode="valid") - mu_y**2
    sigma_xy = np.convolve(origin*approx, w, mode="valid") - mu_x*mu_y

    numerator = (2 * mu_x * mu_y + C1) * (2 * sigma_xy + C2)
    denominator = (mu_x**2 + mu_y**2 + C1) * (sigma_x2 + sigma_y2 + C2)
    ssim_map = numerator / denominator

    return ssim_map.mean()

if __name__ == "__main__":
    DATA = sys.argv[1]
    DECOMPRESS = sys.argv[2]
    COMPRESS = sys.argv[3]
    MONITOR = ".mon"

    origin_data, approx_data = load(DATA, DECOMPRESS)
    c_max_vsz, c_max_rss, d_max_vsz, d_max_rss = load_monitor(MONITOR)
    count_min = min(origin_data.shape[0], approx_data.shape[0])
    
    origin_data = origin_data[:count_min]
    approx_data = approx_data[:count_min]
        
    print("Compress Ratio:", compressratio(DATA, COMPRESS))
    print("MSE:", mse(origin_data, approx_data))
    print("RMSE:", rmse(origin_data, approx_data))
    print("MAE:", mae(origin_data, approx_data))
    print("SNR:", snr(origin_data, approx_data))
    print("PSNR:", psnr(origin_data, approx_data))
    print("Max_E:", maxdiff(origin_data, approx_data))
    print("Min_E:", mindiff(origin_data, approx_data))
    print("Correlation:", pearsoncorr(origin_data, approx_data))
    print("SSIM:", ssim(origin_data, approx_data))
    print("c_max_vsz:", c_max_vsz/1024/1024)
    print("c_max_rss:", c_max_rss/1024/1024)
    print("d_max_vsz:", d_max_vsz/1024/1024)
    print("d_max_rss:", d_max_rss/1024/1024)
