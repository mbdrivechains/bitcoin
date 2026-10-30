# easynet

easynet is a public, CPU-mined test network on eCash's rules: eCash mainnet's difficulty rule (from block 288), the
timewarp fix (#4), and BIP 300/301 (drivechains) enforced from genesis by LayerTwo-Labs' current enforcer. Its coins
have no value. It is run by mbdrivechains for experiments, and **it can be restarted at any time**.

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

## The network now (reset 4, 2026-10-29, release `easynet-4`)

| | |
|---|---|
| Genesis | `000000018a449eb24e2c55744a19ad02db5928fa89296f87cbb2059784b2b211` |
| Network magic | `ea5e4e04` |
| Ports | P2P 28533, RPC 28532 |
| Seed | `163.47.9.132:28533` (built in) |
| Addresses | testnet style (`tb1...`) |

Blocks come about every two minutes at the start (genesis difficulty 0.28 against one CPU thread, about 10 MH/s), so
coins mature and sidechains activate within hours. Block 288 has a fixed difficulty (1.4255), about 10-minute blocks,
as eCash mainnet's fork block does. After it, eCash mainnet's difficulty rule: the difficulty changes only once every
288 blocks (about two days), each step moving it 1/7 of the way toward the measured block rate, at most 12% up or 30%
down. There is no 20-minute rule, so if mining stops, the chain stops until it resumes.

The previous easynet (reset 3, `easynet-3.2` and older) ended on 29 October: LayerTwo-Labs' current enforcer allows
one sidechain proposal per block, and that chain had blocks with more. With the installer, `~/easynet/easynet update`
moves the old chain aside and starts on this one.

## 1. Download

Each release has the node (`bitcoind`, `bitcoin-cli`, ...) and the enforcer, for Linux x86_64 (glibc 2.31 or newer,
e.g. Ubuntu 20.04 and later) and macOS 14 or newer (one universal file for Apple Silicon and Intel).

**Linux:**

```
T=easynet-4
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
T=easynet-4
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
`xattr -dr com.apple.quarantine bitcoin-easynet-4 bip300301_enforcer-easynet-4-universal-apple-darwin`.

The node's programs are in `bitcoin-easynet-4/bin/`; the enforcer is in the `bip300301_enforcer-easynet-4-...`
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

Sidechain nodes talk to a BIP 300/301 enforcer next to your node. Ours is LayerTwo-Labs' current bip300301_enforcer
with four small changes (https://github.com/mbdrivechains/bip300301_enforcer, branch `easynet-4`): it reads easynet's
chain name, starts its wallet from easynet's genesis, builds block templates with our mempool fixes, and waits up to
600 s on the node. Published with Paul Sztorc's OK (DAA group, 2026-10-13).

With the node from step 2 running, start it. On Linux:

```
bip300301_enforcer --fork=ecash \
  --node-rpc-addr=127.0.0.1:28532 --node-rpc-cookie-path=$HOME/.ecash/easynet/.cookie \
  --node-zmq-addr-sequence=tcp://127.0.0.1:28535 \
  --enable-wallet --wallet-auto-create --wallet-sync-source=disabled \
  --bitcoin-core-skip-version-check
```

On macOS, the same with the node's cookie in its macOS folder:

```
bip300301_enforcer --fork=ecash \
  --node-rpc-addr=127.0.0.1:28532 --node-rpc-cookie-path="$HOME/Library/Application Support/eCash/easynet/.cookie" \
  --node-zmq-addr-sequence=tcp://127.0.0.1:28535 \
  --enable-wallet --wallet-auto-create --wallet-sync-source=disabled \
  --bitcoin-core-skip-version-check
```

It runs easynet as testnet4, whose address format easynet shares, with short sidechain thresholds (5 of 10 blocks).
It keeps its data under `ecash/testnet4` in `~/.local/share/bip300301_enforcer` (Linux) or
`~/Library/Application Support/bip300301_enforcer` (macOS), and serves gRPC on `127.0.0.1:50051`.

## 5. Sidechains

easynet has the sidechains that ran on eCash's betanet (12 slots, proposed again from block 1 of this chain), with
short activation thresholds (5 of 10 blocks), so a whole activation, deposit and withdrawal cycle takes hours, not
months. Run each sidechain's
own software, pointed at your enforcer's gRPC (`127.0.0.1:50051`) and your node.

## 6. Propose your own sidechain (M1)

A sidechain starts as a proposal (M1) in a block's coinbase. A block carries your proposal only if it was built from
your own enforcer's template, and the public pool builds from ours, so you mine a block yourself, on your CPU. After
that our enforcer acks every proposal for an empty slot, and yours activates once it has more than 5 acks within 10
blocks. A block carries at most one proposal. Or post the slot number, title and description in the Telegram group, and we will propose it for you.

1. **Pick an empty slot** (0 to 255). The explorer lists the ones in use: https://easy.ecxfreebank.com/sidechains
2. **Restart the enforcer with block templates on,** paying the block to your address (step 3 shows how to get one).
   That is the command from step 4 with three more options:
   `--enable-mempool --enable-block-template-server --coinbase-recipient=tb1q...`.
   With the one-command install:

   ```
   ~/easynet/easynet stop
   EASYNET_NO_ENFORCER=1 ~/easynet/easynet start
   ~/easynet/current/bin/bip300301_enforcer --data-dir=$HOME/easynet/enforcer --fork=ecash \
     --node-rpc-addr=127.0.0.1:28532 --node-rpc-cookie-path=$HOME/easynet/node/easynet/.cookie \
     --node-zmq-addr-sequence=tcp://127.0.0.1:28535 \
     --enable-wallet --wallet-auto-create --wallet-sync-source=disabled --bitcoin-core-skip-version-check \
     --enable-mempool --enable-block-template-server --coinbase-recipient=tb1q...
   ```

   It serves `getblocktemplate` on `127.0.0.1:8122`.
3. **Submit the proposal** with grpcurl (https://github.com/fullstorydev/grpcurl). The two hash ids identify your
   sidechain's release (BIP 300); zeros are fine for a test.

   ```
   grpcurl -plaintext -d '{"sidechain_id": 200, "declaration": {"v0": {
     "title": "my-sidechain", "description": "what it does",
     "hash_id_1": {"hex": "0000000000000000000000000000000000000000000000000000000000000000"},
     "hash_id_2": {"hex": "0000000000000000000000000000000000000000"}}}}' \
     127.0.0.1:50051 cusf.mainchain.v1.BlockProducerService/SubmitSidechainProposal
   ```

   `.../GetBlockProducerState` (same address, no `-d`) lists it under `pendingProposals`.
4. **Mine.** cpuminer-opt and the enforcer don't quite fit (JSON-RPC 1.0 against 2.0, and the coinbase's segwit
   form), so run `gbt_shim.py` between them (this repo, `contrib/easynet/`; plain Python 3, nothing to install):

   ```
   curl -fsSLO https://raw.githubusercontent.com/mbdrivechains/bitcoin/easynet/contrib/easynet/gbt_shim.py
   python3 gbt_shim.py 8123 http://127.0.0.1:8122/ &
   cpuminer -a sha256d -o http://127.0.0.1:8123 -u x -p x -t 1
   ```

   Up to block 288 one thread of a desktop CPU finds a block in a few minutes on average; after it, in 10 to 15
   minutes; more threads, sooner. Your block shows on
   the explorer with your proposal in its coinbase. You can stop mining then.
5. **Watch it activate,** usually within about 10 blocks:
   `grpcurl -plaintext 127.0.0.1:50051 cusf.mainchain.v1.ValidatorService/GetSidechainProposals`, then
   `.../GetSidechains`.

## Resets and updates

A reset brings a new genesis, and the last byte of the magic goes up by one. The release tag follows it (`easynet-4`
is reset 4). An update within a reset keeps the chain (`easynet-3.2` was reset 3, update 2): replace the programs and
restart, or run `~/easynet/easynet update`; there is no data to move. After a reset, download the new release and
start from empty data: stop the node and the enforcer, and move the node's `easynet` data folder and the enforcer's
data folder aside.

## Build from source

- Node: this branch, with Bitcoin Core's build instructions (`cmake -B build && cmake --build build`).
- Enforcer: `git clone -b easynet-4 https://github.com/mbdrivechains/bip300301_enforcer && cd bip300301_enforcer && cargo build --release`.
