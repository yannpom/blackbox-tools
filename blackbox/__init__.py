import _blackbox_tools
import numpy as np
import scipy
import pandas as pd


def butter_lowpass_filter(data, cutoff, fs, order=6):
    normal_cutoff = cutoff / (fs/2)
    b, a = scipy.signal.butter(order, normal_cutoff, btype='low', analog=False)
    y = scipy.signal.filtfilt(b, a, data)
    return y

class FlightTooShort(Exception):
    pass

class Flight:
    MIN_DURATION = 40.0
    SKIP_TIME_BEGIN = 5.0
    SKIP_TIME_END = 1.0
    
    def __init__(self, number, bb_data):
        self.number = number
        self.pid = bb_data["pid"]
        data = bb_data["data"]
        data["time"] = np.array(data["time"]) / 1000000
        
        time = data["time"]
        self.min_time = time[0]
        self.max_time = time[-1]
        self.duration = (self.max_time - self.min_time)
        self.samples = len(time)
        self.avg_freq = self.samples/(self.duration)

        d_time = np.diff(time)
    
        self.d_time_mean = np.mean(d_time)
        self.d_time_median = np.median(d_time)
        self.d_time_min = np.min(d_time)
        self.d_time_max = np.max(d_time)
        self.d_time_std = np.std(d_time)

        self.freq = 1/(self.d_time_median)
        
        if self.duration < self.MIN_DURATION:
            raise FlightTooShort()
            
        for name in ("gyroADC", "axisD", "axisP"):
            for i in range(3):
                input_column = f"{name}[{i}]"
                output_column = f"{name}[{i}]_LPF"
                if not input_column in data:
                    continue
                data[output_column] = butter_lowpass_filter(data[input_column], 120, fs=self.freq)
        self._data = pd.DataFrame(data, index=data["time"])
        
    @property
    def data(self):
        drop_lines_begin = int(self.SKIP_TIME_BEGIN*self.freq)
        drop_lines_end = int(self.SKIP_TIME_END*self.freq)
        return self._data.iloc[drop_lines_begin:-drop_lines_end]
    
    def __str__(self):
        return f"Flight {self.number}, {self.duration:.0f} seconds, {self.samples} samples @ {self.freq:.1f} Hz"
    
    def stats(self):
        print(f"  Fields: {', '.join(self._data.columns.values)}")
        print(f"  Frequency (avg): {self.avg_freq:.2f} Hz")
        print(f"  Time between samples:")
        print(f"    Avg:    {1000000*self.d_time_mean:.1f} us")
        print(f"    Median: {1000000*self.d_time_median:.1f} us")
        print(f"    Min:    {1000000*self.d_time_min:.1f} us")
        print(f"    Max:    {1000000*self.d_time_max:.1f} us")
        print(f"    Std:    {1000000*self.d_time_std:.1f} us")

    def prefix(self, axis):
        p = self.pid[axis]['p']
        i = self.pid[axis]['i']
        d = self.pid[axis]['d']
        ff = self.pid[axis]['ff']
        return f"{self.number:2d}_P{p}_I{i}_D{d}_FF{ff}_"


class BlackBoxFile():
    def __init__(self, filename):
        self.filename = filename
        self.flights = []
        for i, data in enumerate(_blackbox_tools.decode(filename)):
            try:
                self.flights.append(Flight(i, data))
            except FlightTooShort:
                pass

    def __str__(self):
        return f"BlackBoxFile '{self.filename}': {len(self.flights)} flights"
