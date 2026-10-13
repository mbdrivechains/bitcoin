# easynet

easynet is a public, CPU-mined test network on eCash's rules: the ASERT difficulty proposal (#2), the timewarp fix
(#4), and BIP 300/301 (drivechains) enforced from genesis. Its coins have no value. It is run by mbdrivechains for
experiments, and **it can be restarted at any time**.

- Explorer: https://easy.ecxfreebank.com
- Public pool: `stratum+tcp://seed.ecxfreebank.com:28334`
- Downloads: the latest `easynet-N` release at https://github.com/mbdrivechains/bitcoin/releases (node for Linux
  x86_64 and macOS, enforcer for Linux x86_64), or build this branch.

## The network now (reset 3, 2026-10-13)

| | |
|---|---|
| Genesis | `00000001a4105dc30c636e71ac2d844caa32d6fa2e9aacc5b2225ea772dd9490` |
| Network magic | `ea5e4e03` |
| Ports | P2P 28533, RPC 28532 |
| Seed | `163.47.9.132:28533` (built in) |
| Addresses | testnet style (`tb1...`) |

Blocks come about every two minutes at the start (genesis difficulty 0.28 against about 10 MH/s of CPU mining). At
block 2016 the difficulty retargets, and from block 2017 ASERT (half-life two days) steers it toward 10-minute blocks.

**Resets.** A reset brings a new genesis, and the last byte of the magic goes up by one. The release tag follows it
(`easynet-3` is reset 3). After a reset, take the new release and start from empty data directories (move
`~/.ecash/easynet` and the enforcer's data directory aside).

## Run a node

This build's one network is easynet: `bitcoind` starts it with no flag. (`-chain=main` still selects eCash mainnet.)
Data directory `~/.ecash/easynet`, config file `~/.ecash/ecash.conf`. It finds the seed by itself.

```
bitcoind -daemon
bitcoin-cli getblockchaininfo        # "chain": "easynet"
```

## Mine

Point any SHA-256 miner at the public pool, with your own `tb1` address as the username. Each block pays its
finder; there is no sharing. For example, with cpuminer-opt:

```
cpuminer -a sha256d -o stratum+tcp://seed.ecxfreebank.com:28334 -u tb1q... -p x
```

## Run the enforcer (for sidechains)

Sidechain nodes talk to a BIP 300/301 enforcer next to your node. Ours is a small fork of LayerTwo-Labs'
bip300301_enforcer (https://github.com/mbdrivechains/bip300301_enforcer, branch `easynet`): an easynet preset and two
block-template fixes. Published with Paul Sztorc's OK (DAA group, 2026-10-13).

The node needs a transaction index and ZMQ. In `~/.ecash/ecash.conf`:

```
[easynet]
server=1
txindex=1
zmqpubsequence=tcp://127.0.0.1:28534
```

Then:

```
bip300301_enforcer --network-preset=easynet \
  --node-rpc-addr=127.0.0.1:28532 --node-rpc-cookie-path=$HOME/.ecash/easynet/.cookie \
  --node-zmq-addr-sequence=tcp://127.0.0.1:28534 \
  --enable-wallet --wallet-auto-create --wallet-sync-source=disabled \
  --bitcoin-core-skip-version-check
```

Sidechain software connects to the enforcer's gRPC at `127.0.0.1:50051`. The sidechain slots live on eCash's betanet
are proposed on easynet too, with short activation thresholds, so a whole activation, deposit and withdrawal cycle
takes hours, not months.
