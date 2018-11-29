# cn8cardsaver AMD

CN8CardSaver-amd is a high performance Monero (XMR) OpenCL AMD miner forked from [XMRig-amd](https://github.com/xmrig/xmrig-amd).

cn8cardsaver (CryptoNight V1/2) is a miner for Monero XMR with GPU temperature control support. With it you can keep your expensive cards save.
Keep it below 65 C to be on the safe side. If it gets to 80 C or above you are damaging your card.
Use the switches ```--max-gpu-temp=65 and --gpu-temp-falloff=9``` for example.

###### New feature:
The linux version can now control the fans automatically. If the temperature gets too high the fans are turned to 100% and back to auto if the temperature gets back down.
You have to use the amdgpu-pro driver.

GPU mining part based on [psychocrypt](https://github.com/psychocrypt) code used in xmr-stak-nvidia.

Temperature control:
### Command line options
```
      --max-gpu-temp=N      Maximum temperature a GPU may reach before its cooled down (default 75)
      --gpu-temp-falloff=N  Amount of temperature to cool off before mining starts again (default 10)
```

#### Table of contents
* [Features](#features)
* [Download](#download)
* [Usage](#usage)
* [Donations](#donations)
* [Contacts](#contacts)

## Features
* High performance.
* Official Windows support.
* Support for backup (failover) mining server.
* CryptoNight-Lite support for AEON.
* Automatic GPU configuration.
* GPU temperature management (option --max-gpu-temp, --gpu-temp-falloff)
* Nicehash support.
* It's open source software.

## Download
* Binary releases: https://github.com/kimxilxyong/cn8cardsaver-amd/releases
* Git tree: https://github.com/kimxilxyong/cn8cardsaver-amd.git
  * Clone with `git clone https://github.com/kimxilxyong/cn8cardsaver-amd.git`

## Usage

Example:
```
cn8cardsaver-amd.exe --max-gpu-temp=64 --gpu-temp-falloff=7 -o pool.monero.hashvault.pro:5555 -u 422KmQPiuCE7GdaAuvGxyYScin46HgBWMQo4qcRpcY88855aeJrNYWd3ZqE4BKwjhA2BJwQY7T2p6CUmvwvabs8vQqZAzLN.Monerogh -p Monero2-amd-gh --variant 2 --nicehash
```

Use [config.xmrig.com](https://config.xmrig.com/amd) to generate, edit or share configurations.

### Command line options
```
  -a, --algo=ALGO              specify the algorithm to use
                                 cryptonight
                                 cryptonight-lite
                                 cryptonight-heavy
  -o, --url=URL                URL of mining server
  -O, --userpass=U:P           username:password pair for mining server
  -u, --user=USERNAME          username for mining server
  -p, --pass=PASSWORD          password for mining server
      --rig-id=ID              rig identifier for pool-side statistics (needs pool support)
  -k, --keepalive              send keepalived for prevent timeout (needs pool support)
      --nicehash               enable nicehash.com support
      --tls                    enable SSL/TLS support (needs pool support)
      --tls-fingerprint=F      pool TLS certificate fingerprint, if set enable strict certificate pinning
  -r, --retries=N              number of times to retry before switch to backup server (default: 5)
  -R, --retry-pause=N          time to pause between retries (default: 5)
      --opencl-devices=N       list of OpenCL devices to use.
      --opencl-launch=IxW      list of launch config, intensity and worksize
      --opencl-strided-index=N list of strided_index option values for each thread
      --opencl-mem-chunk=N     list of mem_chunk option values for each thread
      --opencl-comp-mode=N     list of comp_mode option values for each thread
      --opencl-affinity=N      list of affinity GPU threads to a CPU
      --opencl-platform=N      OpenCL platform index
      --opencl-loader=N        path to OpenCL-ICD-Loader (OpenCL.dll or libOpenCL.so)
      --print-platforms        print available OpenCL platforms and exit
      --max-gpu-temp=N         Maximum temperature a GPU may reach before its cooled down (default 75)
      --gpu-temp-falloff=N     Amount of temperature to cool off before mining starts again (default 10)	  
      --no-cache               disable OpenCL cache
      --no-color               disable colored output
      --variant                algorithm PoW variant
      --donate-level=N         donate level, default 5% (5 minutes in 100 minutes)
      --user-agent             set custom user-agent string for pool
  -B, --background             run the miner in the background
  -c, --config=FILE            load a JSON-format configuration file
  -l, --log-file=FILE          log all output to a file
  -S, --syslog                 use system log for output messages
      --print-time=N           print hashrate report every N seconds
      --api-port=N             port for the miner API
      --api-access-token=T     access token for API
      --api-worker-id=ID       custom worker-id for API
      --api-id=ID              custom instance ID for API
      --api-ipv6               enable IPv6 support for API
      --api-no-restricted      enable full remote access (only if API token set)
      --dry-run                test configuration and exit
  -h, --help                   display this help and exit
  -V, --version                output version information and exit
```

## Donations
Default donation 5% (5 minutes in 100 minutes) can be reduced to 1% via option `donate-level`.

* XMR: `422KmQPiuCE7GdaAuvGxyYScin46HgBWMQo4qcRpcY88855aeJrNYWd3ZqE4BKwjhA2BJwQY7T2p6CUmvwvabs8vQqZAzLN`
* BTC: `19hNKKFu34CniRWPhGqAB76vi3U4x7DZyZ`

#### Donate to xmrig dev
* XMR: `48edfHu7V9Z84YzzMa6fUueoELZ9ZRXq9VetWzYGzKt52XU5xvqgzYnDK9URnRoJMk1j8nLwEVsaSWJ4fhdUyZijBGUicoD`
* BTC: `1P7ujsXeX7GxQwHNnJsRMgAdNkFZmNVqJT`


## Release checksums (invalid, TBD)
### SHA-256
```
f83ba339f7316cb5a31ae3311abe8291ba3b578fae959e7c0639c1076c58c57b xmrig-amd-2.8.3-xenial-amd64.tar.gz/xmrig-amd-2.8.3/xmrig-amd
12694a7a1e323ee5303e6eb3c3bbf2993e6469016c13cd75f9a1b876e36173b2 xmrig-amd-2.8.3-xenial-amd64.tar.gz/xmrig-amd-2.8.3/xmrig-amd-notls
0f727ef07bef89e107966d004e4e57b41d473b29c085da9182aed2333a64a4f7 xmrig-amd-2.8.3-win64.zip/xmrig-amd.exe
f5cff1cbad8ab43ea3506c54d6bb4e89b99540933dbd0b40282d9e76f8c9048a xmrig-amd-2.8.3-win64.zip/xmrig-amd-notls.exe
```

```

## Contacts
* kimxilxyong@gmail.com
* [reddit](https://www.reddit.com/user/kimilyong/)
