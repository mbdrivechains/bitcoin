# alphanet-bridge: feed ECX from Bitcoin *blocks*, not Bitcoin mempool gossip

Patches: `0001-bridge-feed-from-bitcoin-blocks.patch` + `0002-bridge-fix-pass.patch` (review
fixes, see the last section), on top of `ecash-com/alphanet-bridge` (commit 1072a090c6,
"Implement BTC -> ECX P2P bridge"). Branch `bridge-block-feed`.

## What a seed operator has to change

Nothing except the binary. `-bitcoinpeer=<ip>` keeps its meaning; the node keeps
fetching the pre-fork chain from that peer. Past `EcashHeight` it now also follows
Bitcoin's header chain from that peer, downloads Bitcoin's blocks, and offers every
transaction in them to the ECX mempool. A small file `bridgefeed.dat` appears in the
network datadir (the last Bitcoin block fed plus up to 2000 of its ancestors, so a
restart resumes instead of re-feeding from the fork and a Bitcoin reorg across the
restart is recognised). One `bridge: Bitcoin block ...` info line is logged per Bitcoin
block; a `bridge: ... NOT fed` *warning* is logged whenever a Bitcoin block's
transactions could not be obtained (pruned peer, block never served) -- that is the
only way the feed ever skips anything, and the operator is told.

If `-bitcoinpeer` points at a **pruned** Bitcoin node, only the blocks inside its
288-block window can ever be fetched; everything older is skipped with one warning.
Use an unpruned peer for the first start (the fork is thousands of blocks back).

## Why

The current bridge is a one-way tap on the Bitcoin peer's *transaction gossip*.
Anything the peer never announces (its mempool at connect time, sub-feefilter
transactions, out-of-band submissions, parents that confirmed before the child
arrived, anything lost while the bridge was in IBD or restarting) never reaches ECX,
and a Bitcoin peer never re-announces. The set of transactions that *matter* for
replay is exactly the set that Bitcoin confirms, and Bitcoin's blocks are the
complete, ordered, re-fetchable record of it. So: ignore gossip, follow blocks.

## What changes (all gated on `m_bitcoin_magic`; non-bridge nodes are unaffected)

1. **No transaction gossip from Bitcoin peers.** Our `version` to a Bitcoin peer sets
   `fRelay=false`, so a Core peer sends no tx `inv`s at all; tx/wtx `inv`s that arrive
   anyway are ignored (no `AddTxAnnouncement`, no disconnect), unsolicited `tx`
   messages are dropped, and we no longer send `feefilter` to such peers. All of this
   (and everything below) applies only when `EcashHeight > 0`, i.e. when the block feed
   exists; a chain without a fork height (regtest default) treats a `-bitcoinpeer`
   exactly as before the patch.

2. **Bitcoin's post-fork headers are tracked outside the block index.** For a
   `headers` message from a Bitcoin peer, `BridgeSplitHeaders` derives the height of
   the first header (its parent must be in our block index, or a foreign header we
   already hold), hands the part below `EcashHeight` to the normal path (pre-fork
   sync, unchanged) and records the rest in the peer's `ForeignChain` (hash -> prev,
   height, header) after the existing `CheckHeadersPoW` (each header's own `nBits`;
   no ECX difficulty rules). A foreign header must link to the fork parent (our block
   at `EcashHeight-1`) or to a recorded foreign header; a header building on one of
   *our* post-fork blocks is dropped. Full batches (2000) trigger another
   `getheaders` with a locator built from the foreign tip, the persisted anchor and
   the fork parent, so the peer keeps serving post-fork headers. Block `inv`s and
   compact blocks from the peer are treated as header announcements only.
   `getheaders` *from* a Bitcoin peer is answered with pre-fork headers only, so the
   bridge never offers its own post-fork chain to Bitcoin's network.

3. **Blocks are fetched in height order, 16 in flight** (`BridgeRequestBlocks`, from
   `SendMessages`), with `getdata MSG_WITNESS_BLOCK`. A 120 s poll `getheaders` guards
   against a missed announcement. Fetching only starts once our own chain has the fork
   parent connected (before that the UTXO set cannot judge post-fork transactions).
   Bitcoin Core sends no `notfound` for blocks, so a block it will not serve is
   handled by policy: re-requested after 60 s, three times; then it leaves the fetch
   window (the blocks above it keep flowing) and is retried every 15 min; after 10
   requests in total (~2 h) it is **given up on** -- marked processed without feeding,
   with a warning that its transactions are not fed (info-level only if the peer no
   longer builds on it). A block off the peer's chain that is 12+ blocks below its tip
   is dropped as soon as it stalls. A `notfound` from a non-Core peer moves the block
   straight to the slow track. Against a **pruned peer** (`NODE_NETWORK_LIMITED`)
   blocks 286 or more below its tip are never requested (Core would disconnect us at 291);
   they are skipped with one aggregated warning giving the height range.

4. **A fetched foreign block never reaches `ProcessNewBlock`** (`BridgeProcessBlock`).
   After the merkle/witness-commitment check (`IsBlockMutated`), every non-coinbase
   transaction, in block order, goes through `ChainstateManager::ProcessTransaction`
   (ATMP) exactly like `sendrawtransaction` does, i.e. accepted transactions are
   added to the unbroadcast set and announced to ECX peers. Per block we log the
   counts: accepted / already known / missing inputs / held / other, with the
   reject-reason strings of the "held" and "other" classes. Transactions rejected for
   a reason that may clear later -- `non-final` / `non-BIP68-final` (our chain has not
   reached their locktime yet), `too-long-mempool-chain`, `mempool full`, `mempool min
   fee not met` -- are **held** (at most 2000, for at most 24 h) and re-offered whenever
   our tip moves or every 10 min (`BridgeRetryHeldTxs`); one info line reports each
   retry pass. Blocks below `EcashHeight` from the same peer keep the normal path.

5. **Bitcoin reorgs, any depth, in session or across a restart.** A Bitcoin header is
   skipped only if its *hash* is known to have been fed (this session's `m_bridge_fed`,
   or the persisted ancestry below); every other header is fetched and fed, whatever
   its height. Competing branches are therefore always fed in full; feeding is
   idempotent (duplicates are `txn-already-in-mempool`, conflicts resolve by the
   mempool's RBF rules). The anchor follows the branch the peer builds on.

6. **Restart.** `bridgefeed.dat` holds the highest Bitcoin block whose whole post-fork
   ancestry was fed (or given up on), followed by up to 2000 of its ancestors, one
   `<hash> <height>` per line. On the next connect the foreign chain is seeded with all
   of them (processed), so headers building on any of them connect, our locator is
   dense over the last 2000 Bitcoin blocks, and the peer's `getheaders` answer starts
   exactly where its chain leaves ours: nothing if no reorg, the new branch only
   otherwise. A reorg deeper than 2000 blocks across a restart would re-feed from the
   fork parent (harmless). Without the file (first start) everything since the fork is
   fed once.

7. **Test knobs, regtest only:** `-bitcoinpeermagic=<8 hex>` (magic for
   `-bitcoinpeer` connections; refused on any other chain) and `-ecashheight=<n>`
   (sets `consensus.EcashHeight`; the default there stays 0, which turns the whole
   bridge feed off, so a plain regtest bridge behaves exactly as before the patch,
   gossip included). Mainnet parameters are untouched.

## What it does not fix (by design)

Transactions that ATMP rejects on ECX for good still do not enter the mempool: those
spending post-fork Bitcoin coinbases or the repurposed P2PK outputs (`missing inputs`),
non-standard or zero-fee transactions, and RBF losers already confirmed on ECX. Held
transactions that never become acceptable are dropped after 24 h. The per-block log
counts show exactly how many fall in each class.

Blocks the peer cannot provide are not fed and are *warned about*, never silently
skipped: everything below a pruned peer's window, and a block the peer refuses to
serve for ~2 h. A given-up block is part of the persisted ancestry, so a later restart
does not fetch it either; delete `bridgefeed.dat` to re-feed from the fork through a
better peer.

## Resource footprint

Per Bitcoin peer: one `ForeignChain` (a few hundred bytes per header, processed
headers forgotten 2000 heights below the tip, capped at 60,000 headers). Globally: the
last 20,000 fed block hashes (dedup across several `-bitcoinpeer`s), up to 2000 held
transactions, and `bridgefeed.dat` (~130 KB, rewritten atomically once per fed Bitcoin
block). Bandwidth: Bitcoin's blocks (~1.5-4 MB each) instead of its gossip.

## Review map

* `src/net_processing.cpp`: `ForeignChain`, `CNodeState::m_foreign`, the `Bridge*`
  methods (one contiguous block before `CheckHeadersPoW`), and small gated hunks in
  `PushNodeVersion`, `InitializeNode`, the `inv`/`tx`/`getheaders`/`cmpctblock`/
  `headers`/`block` handlers, `MaybeSendFeefilter` and `SendMessages`.
* `src/net.h`, `src/net.cpp`, `src/init.cpp`: `-bitcoinpeermagic` plumbing
  (`CConnman::Options::m_bitcoin_magic` replaces the hardcoded constant's use).
* `src/chainparams*.cpp`, `src/kernel/chainparams.{h,cpp}`: `-ecashheight` for regtest.

Tests: `test/test_bridge_feed.py` (vanilla Core 29 regtest as "Bitcoin", this build
as ECX with `-ecashheight=150`; see the header of the script for the checks);
`test/adv_mock.py` (scripted hostile Bitcoin peer: withheld blocks, garbage, batches
spanning the fork, 61,000 headers) and `test/adv_vanilla.py` (vanilla Core 29: deep
reorg, taller Bitcoin chain + locktimes, pruned peer, regtest default) -- the review
harnesses, updated in the fix pass to assert the fixed behaviour.

## Fix pass (review findings)

* **Unserved blocks pinned the feed** (major): a block the peer never serves held a
  fetch-window slot and was re-requested every 60 s forever; 16 such blocks stalled
  the feed, and the anchor never advanced. Now: fast/slow retry tracks, give-up after
  10 requests with a warning, stale-branch drop, `notfound` handling
  (`BridgeRequestBlocks`, `BridgeNotFound`, `Entry::attempts/skipped`).
* **Pruned peer disconnect loop** (major): the oldest pending block was always
  requested first, and a `NODE_NETWORK_LIMITED` peer disconnects on any request more
  than 290 below its tip. Now: blocks at or below `tip - 286` are never requested from
  a limited peer; they are skipped with one warning naming the height range.
* **Reorg deeper than 6 lost transactions** (major): new-branch headers at heights
  `<= anchor - 6` were marked processed without being fed (in session and after a
  restart). Now: only known-fed *hashes* are skipped; `bridgefeed.dat` carries the
  anchor's ancestry (2000) so a restart recognises the old chain; the anchor walk
  (`BridgeAncestryFed`) uses seeded entries and a per-peer `fed_floor` for forgotten
  parents instead of a height margin.
* **clang -Wthread-safety** (minor): the anchor walk is a member with
  `EXCLUSIVE_LOCKS_REQUIRED(cs_main)` instead of a lambda; `clang++ -fsyntax-only
  -Wthread-safety src/net_processing.cpp` is clean.
* **"regtest bridge behaves as before" was false** (minor): every bridge hunk is now
  gated on `BridgeFeeds(peer)` (= `m_bitcoin_magic && EcashHeight > 0`), so without a
  fork height gossip, `getheaders` answers and block relay are untouched.
* **No retry for transient rejects** (minor): held-transaction queue, see item 4.
