## Overview
This repository contains the implementation of the updatable private set union (uPSU) protocol proposed in the paper "Updatable Private Set Union: Generic Construction with Efficient Instantiation".

### Build

Tested on Ubuntu 20.04.6 LTS

#### Prerequisites

```
sudo apt install build-essential cmake git python3 libgmp-dev libboost-dev
```

#### Building

The library can be cloned and built with networking support as
```
git clone https://github.com/SeongbongChoi/updatable-PSU.git
cd updatable-PSU
python3 build.py -DVOLE_PSI_ENABLE_CPSI=ON -DVOLE_PSI_ENABLE_BOOST=ON
```


### Test

Move to the frontend directory:
```
cd out/build/linux/frontend
```


List available tests:
```
./frontend -u -list
```

Available tests:
```
0 - Psu_TBZ25_perf_test    (TBZ+25, USENIX Security 2025)
1 - Psu_KLS26_perf_test    (KLS26, ACM SAC 2026)
2 - PSU_update_test        (Updatable PSU)
```

Example usage (PSU — TBZ25):
nn: the log2 size of sender's set, mm: the log2 size of receiver's set
```
./frontend -u 0 -nn 20 -mm 20
```

Example usage (PSU — KLS26):
nn: the log2 size of sender's set, mm: the log2 size of receiver's set
```
./frontend -u 1 -nn 20 -mm 20
```

Example usage (Updatable PSU):
nn: the log2 size of the sets, tt: the log2 size of the updated sets
```
./frontend -u 2 -nn 20 -tt 6
```
