<h1 align="center">5G Core (with support for 5G Multicast Broadcast Services - 5MBS)</h1>
<p align="center">
  <img src="https://img.shields.io/badge/Status-Under_Development-yellow" alt="Under Development">
  <img src="https://img.shields.io/badge/License-AGPL_v3-blue.svg" alt="License">
</p>

## Introduction

This branch hosts MBS-related 5GC network functions.

Additional information can be found at: https://5g-mag.github.io/Getting-Started/pages/5g-multicast-broadcast-services/

### About the implementation

This implementation is based on the [Open5GS](https://github.com/open5gs) project.
For more information, please refer to the [main branch](https://github.com/5G-MAG/open5gs).

## Downloading
```
git clone --recurse-submodules -b 5mbs https://github.com/5G-MAG/open5gs.git ~/open5gs_mbs
```

## Building
```
cd ~/open5gs_mbs
meson setup --prefix=$PWD/install build
```

## Installing
```
ninja -C build install
LD_LIBRARY_PATH="$PWD/install/lib64:$PWD/install/lib" export LD_LIBRARY_PATH
```

## Running
Network functions can be run with the following command:
```
cd ~/open5gs_mbs
install/bin/open5gs-nrfd
```
Network functions relevant to MBS include NRF(pen5gs-nrfd), SCP(pen5gs-scpd), AMF(pen5gs-amfd), MB-SMF(pen5gs-smfd) and MB-UPF(pen5gs-upfd).

## Acknowledgements

The reference implementation of the MBS features was funded by the European Union through the [6G-SANDBOX](https://6g-sandbox.eu/) project.
