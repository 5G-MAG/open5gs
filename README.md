<p align="center">
  <img src=".github/banner.svg" width="100%" alt="5G-MAG Reference Tools, 3GPP RAN and Core Platforms: 5G Core with MBS">
</p>

<p align="center">
  A 5G Core with support for 5G Multicast Broadcast Services: MB-SMF, MB-UPF and an AMF
  carrying the MBS procedures of 3GPP TS 23.247.
</p>

<p align="center">
  <img alt="Status: under development"
    src="https://img.shields.io/badge/Status-Under_Development-yellow">
  <a href="https://github.com/5G-MAG/open5gs/releases"><img alt="Version"
    src="https://img.shields.io/github/v/release/5G-MAG/open5gs?label=Version&sort=semver"></a>
  <a href="LICENSE"><img alt="GNU AGPL v3.0"
    src="https://img.shields.io/badge/License-AGPL%20v3.0-blue"></a>
</p>

<p align="center">
  <a href="https://www.5g-mag.com/reference-tools/3gpp-ran-and-core-platforms">Project page</a> &nbsp;&middot;&nbsp;
  <a href="https://github.com/5G-MAG/open5gs/issues">Issues</a> &nbsp;&middot;&nbsp;
  <a href="https://www.5g-mag.com/contributing">Contributing</a>
</p>

---

## At a glance

|  |  |
|---|---|
| **Implements** | 3GPP TS 23.247, *Architectural enhancements for 5G multicast-broadcast services*, and the MBS parts of TS 29.532 and TS 29.518 |
| **Role** | MB-SMF, MB-UPF and AMF, plus the stock 5G Core network functions the demos need |
| **Built with** | C and meson |
| **Based on** | [Open5GS](https://github.com/open5gs/open5gs); see the [main branch](https://github.com/5G-MAG/open5gs) for the unmodified fork |
| **Part of** | [3GPP RAN and Core Platforms](https://www.5g-mag.com/reference-tools/3gpp-ran-and-core-platforms) |

## Introduction

This repository provides the MBS-related 5GC network functions. Alongside the stock Open5GS functions it
provides an MB-SMF and MB-UPF, and an AMF carrying the MBS procedures, so that an MBSF can
establish MBS sessions and an MBSTF can deliver on them.

## Specification

Built against these versions, named rather than referred to by release:

- **3GPP TS 23.247 V18.8.0**, *Architectural enhancements for 5G multicast-broadcast services*
- **3GPP TS 29.532 V18.6.0**, *Nmbsmf service API*
- **3GPP TS 29.518 V18.14.0**, *Namf service API*, for the MBS procedures

Clause-by-clause coverage is recorded on the project page:
<https://www.5g-mag.com/reference-tools/3gpp-ran-and-core-platforms>

## Install dependencies

Use a distribution with a current toolchain, Ubuntu 24.04 or later:

```bash
sudo apt install git ninja-build build-essential meson flex bison \
  libsctp-dev libgnutls28-dev libgcrypt-dev libssl-dev libidn11-dev \
  libmongoc-dev libbson-dev libyaml-dev libnghttp2-dev libmicrohttpd-dev \
  libcurl4-gnutls-dev libtins-dev libtalloc-dev uuid-dev
```

**MongoDB** is needed as well: the NRF and UDR store their state in it. Ubuntu's own `mongodb`
package is not what Open5GS expects; install from
[MongoDB's repository](https://www.mongodb.com/docs/manual/administration/install-on-linux/),
which provides `mongod` via `mongodb-org-server`. It must be running before the NRF starts.

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

Configuration files are installed under `install/etc/open5gs/`. For a worked end-to-end
deployment, including the configuration each function needs, see
[rt-mbs-examples](https://github.com/5G-MAG/rt-mbs-examples).

## Acknowledgements

The reference implementation of the MBS features was funded by the European Union through the [6G-SANDBOX](https://6g-sandbox.eu/) project.

## Contributing

Contributions are welcome. How to raise an issue, fork the repository and open a pull request, and
the Contributor License Agreement required before code can be merged, are described at
<https://www.5g-mag.com/contributing>.

## License

See [LICENSE](LICENSE).
