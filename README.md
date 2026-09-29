<p align="center">
  <img src=".github/banner.svg" width="100%" alt="Reference Tools · 3GPP RAN and Core Platforms: 5G-MAG's fork of Open5GS">
</p>

<p align="center">
  5G-MAG's fork of Open5GS, carrying 5G Core network functions with support for 5G Multicast
  Broadcast Services (MBS).
</p>

<p align="center">
  <img alt="Status: under development"
    src="https://img.shields.io/badge/Status-Under%20Development-e67e22">
  <a href="https://github.com/5G-MAG/open5gs/releases"><img alt="Version"
    src="https://img.shields.io/github/v/release/5G-MAG/open5gs?label=Version"></a>
  <a href="LICENSE"><img alt="License: GNU AGPL v3.0"
    src="https://img.shields.io/badge/License-AGPL%20v3.0-blue"></a>
</p>

<p align="center">
  <a href="https://www.5g-mag.com/reference-tools/3gpp-platforms/">Project page</a> &nbsp;&middot;&nbsp;
  <a href="https://github.com/5G-MAG/open5gs/issues">Issues</a> &nbsp;&middot;&nbsp;
  <a href="https://www.5g-mag.com/contributing">Contributing</a>
</p>

---

## At a glance

|  |  |
|---|---|
| **Based on** | [Open5GS](https://github.com/open5gs/open5gs) ([open5gs.org](https://open5gs.org/)) |
| **Part of** | [3GPP RAN and Core Platforms](https://www.5g-mag.com/reference-tools/3gpp-platforms/), alongside [rt-srsRAN_Project](https://github.com/5G-MAG/rt-srsRAN_Project), [srsRAN](https://github.com/5G-MAG/srsRAN), [srsRAN_4G](https://github.com/5G-MAG/srsRAN_4G) |

## Introduction

This repository is 5G-MAG's fork of [Open5GS](https://github.com/open5gs/open5gs), hosting the
MBS-related 5GC network functions. Network functions relevant to MBS are the NRF
(`open5gs-nrfd`), SCP (`open5gs-scpd`), AMF (`open5gs-amfd`), MB-SMF (`open5gs-smfd`) and MB-UPF
(`open5gs-upfd`). Everything not specific to MBS is as upstream: see the
[Open5GS documentation](https://open5gs.org/open5gs/docs/).

The MBS work is part of [5G Multicast Broadcast Services (MBS)](https://www.5g-mag.com/reference-tools/5g-mbs/).
The reference implementation of the MBS features was funded by the European Union through the
[6G-SANDBOX](https://6g-sandbox.eu/) project.

## Downloading

```bash
git clone --recurse-submodules -b 5mbs https://github.com/5G-MAG/open5gs.git ~/open5gs_mbs
```

## Building

```bash
cd ~/open5gs_mbs
meson setup --prefix=$PWD/install build
```

## Installing

```bash
ninja -C build install
LD_LIBRARY_PATH="$PWD/install/lib64:$PWD/install/lib" export LD_LIBRARY_PATH
```

## Running

Network functions can be run with, for example:

```bash
cd ~/open5gs_mbs
install/bin/open5gs-nrfd
```

## Contributing

Contributions to this fork are welcome. How to raise an issue, fork the repository and open a pull
request, and the Contributor License Agreement required before code can be merged, are described at
<https://www.5g-mag.com/contributing>.

Contributions to upstream Open5GS go to [open5gs/open5gs](https://github.com/open5gs/open5gs),
which has its own [Contributor License Agreement](https://open5gs.org/open5gs/cla/).

## License

Open5GS is made available under the GNU Affero General Public License v3.0, and this fork keeps
that licence. See [LICENSE](LICENSE).
