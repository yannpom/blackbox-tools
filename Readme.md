# Blackbox Python decoder + Jupyter notebook to Analyse data

## Introduction

This is a fork of <https://github.com/betaflight/blackbox-tools> to:
- add a Python interface to the existing C decoder.
- provide an interactive Jupyter notebook to plot data

## Usage with Binder

Thanks to [Binder] <https://mybinder.org/>, you can run the notebook in the Cloud right now:

[Run the Notebook on Binder] <https://mybinder.org/v2/gh/yannpom/blackbox-tools/main?labpath=BlackBoxAnalyser.ipynb>

## Manual installation of Python module

```bash
pip install git+https://github.com/yannpom/blackbox-tools.git#egg=BlackBoxDecoder
```

## Usage of Python module

```python
from blackbox import BlackBoxFile

blackBoxFile = BlackBoxFile("btfl_all.bbl")
print(blackBoxFile)

for flight in blackBoxFile.flights:
    print(flight)
    # flight.data is a pandas.DataFrame with all the decoded data
```
