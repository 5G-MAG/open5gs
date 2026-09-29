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
| **Implements** | 3GPP TS 23.247, *Architectural enhancements for 5G multicast-broadcast services*, and the MBS parts of TS 29.532 and TS 29.518 |
| **Based on** | [Open5GS](https://github.com/open5gs/open5gs) ([open5gs.org](https://open5gs.org/)) |
| **Part of** | [3GPP RAN and Core Platforms](https://www.5g-mag.com/reference-tools/3gpp-platforms/), alongside [rt-srsRAN_Project](https://github.com/5G-MAG/rt-srsRAN_Project), [srsRAN](https://github.com/5G-MAG/srsRAN), [srsRAN_4G](https://github.com/5G-MAG/srsRAN_4G) |

## Introduction

This repository is 5G-MAG's fork of [Open5GS](https://github.com/open5gs/open5gs), hosting the
MBS-related 5GC network functions. Alongside the stock Open5GS functions it provides an MB-SMF and
an MB-UPF, and an AMF carrying the MBS procedures, so that an MBSF can establish MBS sessions and
an MBSTF can deliver on them. Everything not specific to MBS is as upstream: see the
[Open5GS documentation](https://open5gs.org/open5gs/docs/).

The MBS work is part of [5G Multicast Broadcast Services (MBS)](https://www.5g-mag.com/reference-tools/5g-mbs/).
The reference implementation of the MBS features was funded by the European Union through the
[6G-SANDBOX](https://6g-sandbox.eu/) project.

## Specification

Built against these versions:

- **3GPP TS 23.247 V18.8.0**, *Architectural enhancements for 5G multicast-broadcast services*
- **3GPP TS 29.532 V18.6.0**, *Nmbsmf service API*
- **3GPP TS 29.518 V18.14.0**, *Namf service API*, for the MBS procedures

Clause-by-clause coverage, and what is still absent, is recorded on the project page rather than
here: <https://www.5g-mag.com/reference-tools/3gpp-platforms/>

## Install dependencies

On Ubuntu 24.04 or later:

```bash
sudo apt install git ninja-build build-essential meson flex bison \
  libsctp-dev libgnutls28-dev libgcrypt-dev libssl-dev libidn11-dev \
  libmongoc-dev libbson-dev libyaml-dev libnghttp2-dev libmicrohttpd-dev \
  libcurl4-gnutls-dev libtins-dev libtalloc-dev uuid-dev
```

**MongoDB** is needed as well: the UDR and PCF connect to it when they start. Install it from
[MongoDB's repository](https://www.mongodb.com/docs/manual/administration/install-on-linux/),
which provides `mongod` via `mongodb-org-server`, and have it running before those functions
start.

## Downloading

```bash
git clone --recurse-submodules -b 5mbs https://github.com/5G-MAG/open5gs.git
cd open5gs
```

## Building

```bash
meson setup --prefix="$PWD/install" build
ninja -C build
```

## Installing

```bash
ninja -C build install
export LD_LIBRARY_PATH="$PWD/install/lib64:$PWD/install/lib"
```

## Running

Each network function is a separate binary under `install/bin`. Start the NRF first, since the
others register with it:

```bash
install/bin/open5gs-nrfd
```

The functions relevant to MBS are the NRF (`open5gs-nrfd`), SCP (`open5gs-scpd`), AMF
(`open5gs-amfd`), MB-SMF (`open5gs-smfd`) and MB-UPF (`open5gs-upfd`). The MB-UPF needs root, for
its TUN device.

## Configuration

Configuration files are installed under `install/etc/open5gs/`. For a worked end-to-end
deployment, including the configuration each function needs, see
[rt-mbs-examples](https://github.com/5G-MAG/rt-mbs-examples).

## Contributing

Contributions to this fork are welcome. How to raise an issue, fork the repository and open a pull
request, and the Contributor License Agreement required before code can be merged, are described at
<https://www.5g-mag.com/contributing>.

Contributions to upstream Open5GS go to [open5gs/open5gs](https://github.com/open5gs/open5gs),
which has its own [Contributor License Agreement](https://open5gs.org/open5gs/cla/).

## License

Open5GS is made available under the GNU Affero General Public License v3.0, and this fork keeps
that licence. See [LICENSE](LICENSE).
