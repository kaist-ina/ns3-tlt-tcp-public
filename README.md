# NS-3 simulator for TLT
This is an official Github repository for the Eurosys '21 paper "[Towards timeout-less transport in commodity datacenter networks.](https://dl.acm.org/doi/10.1145/3447786.3456227)".
* For the VMA testbed implementation, please visit [https://github.com/kaist-ina/libvma-tlt-public](https://github.com/kaist-ina/libvma-tlt-public).

This repository provides a NS-3 simulator for TLT, based on version 3.29.
This simlator includes PFC implementation from [here](https://github.com/bobzhuyb/ns3-rdma) and custom DCTCP implementation.

If you find this repository useful in your research, please consider citing:
```
@inproceedings{10.1145/3447786.3456227,
author = {Lim, Hwijoon and Bai, Wei and Zhu, Yibo and Jung, Youngmok and Han, Dongsu},
title = {Towards Timeout-Less Transport in Commodity Datacenter Networks},
year = {2021},
isbn = {9781450383349},
publisher = {Association for Computing Machinery},
address = {New York, NY, USA},
url = {https://doi.org/10.1145/3447786.3456227},
doi = {10.1145/3447786.3456227}
}
```

### Note
This simulator implements TLT for TCP and DCTCP. This may include minor We implemented TLT for RDMA on top of the different codebase. We plan to open-source the RDMA flavored version for TLT on request.

## Guide
### 0. Prerequisites
We tested the simulator on Ubuntu 18.04, but latest versions of Ubuntu should also work.
```
sudo apt install build-essential python3 libgtk-3-0 bzip2
```
### 1. Configure
```
wget https://www.nsnam.org/releases/ns-allinone-3.29.tar.bz2
tar -xvf ns-allinone-3.29.tar.bz2
cd ns-allinone-3.29
rm -rf ns-3.29
git clone https://github.com/kaist-ina/ns3-tlt-tcp-public.git ns-3.29
cd ns-3.29
./waf configure --build-profile=optimized
```

### 2. Build and Run simulations
You can run the large-scale simulation from the paper by issuing following:

```
./waf --run 'tcp-realistic-workload-bgfg config/config.txt'
```

## Configuration
You can configure the simulator by changing `config/config.txt` file. (TBU)

## Modifications from the vanilla ns-3.29

```
 .gitignore                                         |   11 +-
 README                                             |  117 -
 README.md                                          |  171 ++
 config/config.txt                                  |   73 +
 config/topology96.txt                              |  158 ++
 examples/tutorial/examples-to-run.py               |    1 +
 examples/tutorial/wscript                          |    4 +
 scratch/tcp-realistic-workload-bgfg.cc             | 1188 ++++++++++
 src/applications/model/flow-id-num-tag.cc          |   76 +
 src/applications/model/flow-id-num-tag.h           |   35 +
 src/applications/model/flow-stat-tag.cc            |   72 +
 src/applications/model/flow-stat-tag.h             |   37 +
 src/applications/model/packet-sink.cc              |    2 +-
 src/applications/model/seq-ts-header.cc            |   18 +-
 src/applications/model/seq-ts-header.h             |    4 +
 src/applications/wscript                           |    4 +
 src/broadcom/utils/broadcom-egress-queue.cc        |  776 +++++++
 src/broadcom/utils/broadcom-egress-queue.h         |  102 +
 src/broadcom/wscript                               |   15 +
 src/core/model/random-variable-stream.cc           |   14 +-
 src/internet/bindings/modulegen__gcc_ILP32.py      |    2 +-
 src/internet/bindings/modulegen__gcc_LP64.py       |    2 +-
 src/internet/helper/selective-packet-queue.cc      |  318 +++
 src/internet/helper/selective-packet-queue.h       |   76 +
 src/internet/model/ipv4-ecn-tag.cc                 |   58 +
 src/internet/model/ipv4-ecn-tag.h                  |   36 +
 src/internet/model/ipv4-global-routing.cc          |  142 +-
 src/internet/model/ipv4-global-routing.h           |    8 +-
 src/internet/model/rtt-estimator.cc                |    2 +-
 src/internet/model/tcp-congestion-ops.cc           |   20 +
 src/internet/model/tcp-congestion-ops.h            |   46 +-
 src/internet/model/tcp-dctcp.cc                    |  182 ++
 src/internet/model/tcp-dctcp.h                     |   48 +
 src/internet/model/tcp-l4-protocol.cc              |  100 +-
 src/internet/model/tcp-l4-protocol.h               |    1 +
 src/internet/model/tcp-option-ts.cc                |    6 +-
 src/internet/model/tcp-socket-base.cc              | 1593 ++++++++++++-
 src/internet/model/tcp-socket-base.h               |   84 +-
 src/internet/model/tcp-socket-state.h              |   10 +
 src/internet/model/tcp-socket.cc                   |   12 +-
 src/internet/model/tcp-tx-buffer.cc                |    4 +-
 src/internet/model/tlt-tag.cc                      |   78 +
 src/internet/model/tlt-tag.h                       |   44 +
 src/internet/wscript                               |    8 +
 src/network/model/broadcom-node.cc                 |  704 ++++++
 src/network/model/broadcom-node.h                  |  211 ++
 src/network/model/node.cc                          |   35 +-
 src/network/model/node.h                           |    9 +
 src/network/model/socket.cc                        |   41 +-
 src/network/model/socket.h                         |   21 +
 src/network/model/tcp-flow-id-tag.cc               |   57 +
 src/network/model/tcp-flow-id-tag.h                |   34 +
 src/network/utils/data-rate.cc                     |   41 +
 src/network/utils/data-rate.h                      |   12 +
 src/network/utils/pfc-experience-tag.cc            |   59 +
 src/network/utils/pfc-experience-tag.h             |   36 +
 src/network/utils/queue.cc                         |    2 +-
 src/network/utils/queue.h                          |    2 +-
 src/network/wscript                                |    6 +
 src/point-to-point/helper/qbb-helper.cc            |  323 +++
 src/point-to-point/helper/qbb-helper.h             |  188 ++
 src/point-to-point/model/cn-header.cc              |  170 ++
 src/point-to-point/model/cn-header.h               |   94 +
 src/point-to-point/model/irn-header.cc             |  108 +
 src/point-to-point/model/irn-header.h              |   64 +
 src/point-to-point/model/pause-header.cc           |  114 +
 src/point-to-point/model/pause-header.h            |   77 +
 .../model/point-to-point-net-device.h              |   17 +-
 src/point-to-point/model/qbb-channel.cc            |  164 ++
 src/point-to-point/model/qbb-channel.h             |  194 ++
 src/point-to-point/model/qbb-header.cc             |   96 +
 src/point-to-point/model/qbb-header.h              |   61 +
 src/point-to-point/model/qbb-net-device.cc         | 2401 ++++++++++++++++++++
 src/point-to-point/model/qbb-net-device.h          |  359 +++
 src/point-to-point/model/qbb-remote-channel.cc     |   80 +
 src/point-to-point/model/qbb-remote-channel.h      |   47 +
 src/point-to-point/wscript                         |   19 +-
 waf                                                |    0
 workloads/DCTCP_CDF.txt                            |   12 +
 workloads/cachefollower.txt                        |   23 +
 workloads/mining.txt                               |    7 +
 workloads/search.txt                               |   12 +
 workloads/webserver.txt                            |   13 +
 83 files changed, 11429 insertions(+), 242 deletions(-)
```

- `src/internet/tcp-socket-base.cc` and `.h`, `src/internet/tlt-tag.cc` and `.h`:

  Core TLT implementation.

- `scratch/tcp-realistic-workload-bgfg.cc`:

  The `main()` function.

- Files in `workloads` and `config`:

  Configuration for the simulation and workloads.

- Files in `src/broadcom` and `src/network`:

  Implementation for a Broadcom ASIC switch model. These includes selective dropping for TLT, dynamic buffer management, PFC and ECN triggering. Refer [here](https://github.com/bobzhuyb/ns3-rdma#what-did-we-add-exactly) for details.

  *Disclaimer: this module is purely based on authors' personal understanding of Broadcom ASIC. It does not reflect any official confirmation from either Microsoft or Broadcom.*

- Files in `src/internet`:

  Mostly DCTCP implementation, and some for DSCP marking on TLT.

- Files mentioned [here](https://github.com/bobzhuyb/ns3-rdma#what-did-we-add-exactly).

  RDMA, PFC, DCQCN implementation.

- Others

  Mostly packet tag, header for the stastics purposes.

