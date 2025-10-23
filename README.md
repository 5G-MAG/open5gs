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

---

The following is the original README provided by Open5GS.

---

<p align="center"><a href="https://open5gs.org" target="_blank" rel="noopener noreferrer"><img width="100" src="https://open5gs.org/assets/img/open5gs-logo-only.png" alt="Open5GS logo"></a></p>

## Getting Started

Please follow the [documentation](https://open5gs.org/open5gs/docs/) at [open5gs.org](https://open5gs.org/)!

## Sponsors

If you find Open5GS useful for work, please consider supporting this Open Source project by [Becoming a sponsor](https://github.com/sponsors/acetcom). To manage the funding transactions transparently, you can donate through [OpenCollective](https://opencollective.com/open5gs).

<p align="center">
  <h3 align="center">Special Sponsor</h3>
</p>

<p align="center">
  <a target="_blank" href="https://mobi.com">
  <img alt="special sponsor mobi" src="https://open5gs.org/assets/img/mobi-open5GS.png" width="400">
  </a>
</p>

<p align="center">
  <a target="_blank" href="https://open5gs.org/#sponsors">
      <img alt="sponsors" src="https://open5gs.org/assets/img/sponsors.svg">
  </a>
</p>

## Community

- Problem with Open5GS can be filed as [issues](https://github.com/open5gs/open5gs/issues) in this repository.
- Other topics related to this project are happening on the [discussions](https://github.com/open5gs/open5gs/discussions).
- Voice and text chat are available in Open5GS's [Discord](https://discordapp.com/) workspace. Use [this link](https://discord.gg/GreNkuc) to get started.

## Contributing

If you're contributing through a pull request to Open5GS project on GitHub, please read the [Contributor License Agreement](https://open5gs.org/open5gs/cla/) in advance.

## License

- Open5GS Open Source files are made available under the terms of the GNU Affero General Public License ([GNU AGPL v3.0](https://www.gnu.org/licenses/agpl-3.0.html)).
- [Commercial licenses](https://open5gs.org/open5gs/support/) are also available from [NewPlane](https://newplane.io/) at [sales@newplane.io](mailto:sales@newplane.io).

## Support

Technical support and customized services for Open5GS are provided by [NewPlane](https://newplane.io/) at [support@newplane.io](mailto:support@newplane.io).
