# Blackbox Python decoder

## Introduction

This is a fork of <https://github.com/cleanflight/blackbox-tools> to add a Python interface to the existing C decoder.

## Installation

```bash
pip install git+https://github.com/yannpom/blackbox-tools.git#egg=BlackBoxDecoder
```

## Usage

```python
from blackbox import BlackBoxFile

blackBoxFile = BlackBoxFile("btfl_all.bbl")
print(blackBoxFile)

for flight in blackBoxFile.flights:
    print(flight)
    # flight.data is a pandas.DataFrame with all the decoded data
```
