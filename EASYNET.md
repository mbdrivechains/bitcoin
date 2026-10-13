# easynet

easynet is a public, CPU-mined test network on eCash's rules: the ASERT difficulty proposal (#2), the timewarp fix
(#4), and BIP 300/301 (drivechains) enforced from genesis. Its coins have no value. It is run by mbdrivechains for
experiments, and **it can be restarted at any time**.

- Explorer: https://easy.ecxfreebank.com
- Public pool: `stratum+tcp://seed.ecxfreebank.com:28334`
- Downloads: https://github.com/mbdrivechains/bitcoin/releases (the newest `easynet-N` release)

## Quick install (one command)

```
curl -fsSL https://raw.githubusercontent.com/mbdrivechains/bitcoin/easynet/contrib/easynet/easynet.sh | bash
```

This downloads the newest release for your system (Linux x86_64 or macOS 14+), checks it against the release's
`SHA256SUMS`, and starts a node and the enforcer, all inside `~/easynet` (an eCash node you already run is not
touched). Nothing runs as root and nothing starts at boot. Afterwards:

```
~/easynet/easynet status      # what is running, block height, peers
~/easynet/easynet address     # an address to mine to, and the miner command
~/easynet/easynet stop        # and: start
~/easynet/easynet update      # the newest release; after an easynet reset it moves the old chain aside
~/easynet/easynet remove      # stop everything and delete ~/easynet
```

To read the script before running it: `curl -fsSLO <the URL above>`, read `easynet.sh`, then `bash easynet.sh`.
Steps 1 to 4 below do the same by hand.

## The network now (reset 3, 2026-10-13, release `easynet-3.1`)

| | |
|---|---|
| Genesis | `00000001a4105dc30c636e71ac2d844caa32d6fa2e9aacc5b2225ea772dd9490` |
| Network magic | `ea5e4e03` |
| Ports | P2P 28533, RPC 28532 |
| Seed | `163.47.9.132:28533` (built in) |
| Addresses | testnet style (`tb1...`) |

Blocks come about every two minutes at the start (genesis difficulty 0.28 against about 10 MH/s of CPU mining). At
block 2016 the difficulty retargets, and from block 2017 ASERT (half-life two days) steers it toward 10-minute blocks.

Up to block 2016 the testnet 20-minute rule applies: a block more than 20 minutes after its parent may be mined at
minimum difficulty. Under ASERT there is no such rule (from release `easynet-3.1`): every block from 2017 on is at
ASERT's difficulty, so if mining stops, the chain stops until it resumes. **Run `easynet-3.1` or later before block
2017**; an `easynet-3` node splits off at the first gap of over 20 minutes after that.

## 1. Download

Each release has the node (`bitcoind`, `bitcoin-cli`, ...) and the enforcer, for Linux x86_64 (glibc 2.31 or newer,
e.g. Ubuntu 20.04 and later) and macOS 14 or newer (one universal file for Apple Silicon and Intel).

**Linux:**

```
T=easynet-3.1
U=https://github.com/mbdrivechains/bitcoin/releases/download/$T
curl -LO $U/bitcoin-$T-x86_64-linux-gnu.tar.gz
curl -LO $U/bip300301_enforcer-$T-x86_64-linux-gnu.tar.gz
curl -LO $U/SHA256SUMS
grep x86_64-linux-gnu SHA256SUMS | sha256sum -c
tar -xzf bitcoin-$T-x86_64-linux-gnu.tar.gz
tar -xzf bip300301_enforcer-$T-x86_64-linux-gnu.tar.gz
```

**macOS:**

```
T=easynet-3.1
U=https://github.com/mbdrivechains/bitcoin/releases/download/$T
curl -LO $U/bitcoin-$T-universal-apple-darwin.tar.gz
curl -LO $U/bip300301_enforcer-$T-universal-apple-darwin.tar.gz
curl -LO $U/SHA256SUMS
grep apple-darwin SHA256SUMS | shasum -a 256 -c
tar -xzf bitcoin-$T-universal-apple-darwin.tar.gz
tar -xzf bip300301_enforcer-$T-universal-apple-darwin.tar.gz
```

The macOS binaries are signed ad hoc, not notarised by Apple. Downloaded with `curl` as above they run as they are.
If you downloaded them in a browser, macOS refuses to open them; clear the download flag first:
`xattr -dr com.apple.quarantine bitcoin-easynet-3.1 bip300301_enforcer-easynet-3.1-universal-apple-darwin`.

The node's programs are in `bitcoin-easynet-3.1/bin/`; the enforcer is in the `bip300301_enforcer-easynet-3.1-...`
folder. The commands below assume you add both to your `PATH`, or type the paths.

## 2. Start a node

This build's one network is easynet: `bitcoind` starts it with no flag. (`-chain=main` still selects eCash mainnet.)

| | Linux | macOS |
|---|---|---|
| Config file | `~/.ecash/ecash.conf` | `~/Library/Application Support/eCash/ecash.conf` |
| Data | `~/.ecash/easynet/` | `~/Library/Application Support/eCash/easynet/` |

Create the config file (the folder too, if it is not there) with:

```
[easynet]
server=1
txindex=1
zmqpubsequence=tcp://127.0.0.1:28535
```

`server` lets `bitcoin-cli` and the enforcer talk to the node; `txindex` and `zmqpubsequence` are what the enforcer
needs (step 4). Keep ZMQ off port 28534: the node uses that one itself (for Tor), right after its P2P port. Then:

```
bitcoind -daemon
bitcoin-cli getblockchaininfo        # "chain": "easynet"; "blocks" reaches the explorer's height in a minute or two
```

The node finds the seed by itself. If it shows no peers (`bitcoin-cli getconnectioncount` is 0), add
`addnode=163.47.9.132:28533` under `[easynet]` and restart. Stop it with `bitcoin-cli stop`.

## 3. Mine (optional)

You need an address to be paid to. The node has a wallet:

```
bitcoin-cli createwallet mine
bitcoin-cli -rpcwallet=mine getnewaddress          # tb1q...
```

Point any SHA-256 miner at the public pool, with that address as the username. Each block pays its finder; there is
no sharing. Coins can be spent 100 blocks after the block that paid them. With cpuminer-opt
(https://github.com/JayDDee/cpuminer-opt):

```
cpuminer -a sha256d -o stratum+tcp://seed.ecxfreebank.com:28334 -u tb1q... -p x
```

## 4. Run the enforcer (for sidechains)

Sidechain nodes talk to a BIP 300/301 enforcer next to your node. Ours is a small fork of LayerTwo-Labs'
bip300301_enforcer (https://github.com/mbdrivechains/bip300301_enforcer, branch `easynet`): an easynet preset and two
block-template fixes. Published with Paul Sztorc's OK (DAA group, 2026-10-13).

With the node from step 2 running, start it. On Linux:

```
bip300301_enforcer --network-preset=easynet \
  --node-rpc-addr=127.0.0.1:28532 --node-rpc-cookie-path=$HOME/.ecash/easynet/.cookie \
  --node-zmq-addr-sequence=tcp://127.0.0.1:28535 \
  --enable-wallet --wallet-auto-create --wallet-sync-source=disabled \
  --bitcoin-core-skip-version-check
```

On macOS, the same with the node's cookie in its macOS folder:

```
bip300301_enforcer --network-preset=easynet \
  --node-rpc-addr=127.0.0.1:28532 --node-rpc-cookie-path="$HOME/Library/Application Support/eCash/easynet/.cookie" \
  --node-zmq-addr-sequence=tcp://127.0.0.1:28535 \
  --enable-wallet --wallet-auto-create --wallet-sync-source=disabled \
  --bitcoin-core-skip-version-check
```

Within a few seconds its log says `synced to tip`. It keeps its data in `~/.local/share/bip300301_enforcer` (Linux)
or `~/Library/Application Support/bip300301_enforcer` (macOS), and serves gRPC on `127.0.0.1:50051`.

## 5. Sidechains

The sidechain slots that are live on eCash's betanet are proposed on easynet too, with short activation thresholds
(5 of 10 blocks), so a whole activation, deposit and withdrawal cycle takes hours, not months. Run each sidechain's
own software, pointed at your enforcer's gRPC (`127.0.0.1:50051`) and your node.

## Resets and updates

A reset brings a new genesis, and the last byte of the magic goes up by one. The release tag follows it (`easynet-3`
is reset 3). An update within a reset keeps the chain (`easynet-3.1` is reset 3, update 1): replace the programs and
restart, or run `~/easynet/easynet update`; there is no data to move. After a reset, download the new release and start from empty data: stop the node and the enforcer, move
the node's `easynet` data folder aside, and in the enforcer's data folder move `validator/testnet4-easynet` and
`wallet/testnet4-easynet` aside (keep `wallet/testnet4-easynet/seed.json` if you want the same wallet back).

## Build from source

- Node: this branch, with Bitcoin Core's build instructions (`cmake -B build && cmake --build build`).
- Enforcer: `git clone -b easynet https://github.com/mbdrivechains/bip300301_enforcer && cd bip300301_enforcer && cargo build --release`.
