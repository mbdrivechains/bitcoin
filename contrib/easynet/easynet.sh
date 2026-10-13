#!/usr/bin/env bash
# easynet: install, run, update and remove an easynet node and enforcer (github.com/mbdrivechains/bitcoin, branch
# easynet; how it works: EASYNET.md). Linux x86_64 (glibc 2.31+) and macOS 14+.
#
#   Install:  curl -fsSL https://raw.githubusercontent.com/mbdrivechains/bitcoin/easynet/contrib/easynet/easynet.sh | bash
#   Then:     ~/easynet/easynet status | start | stop | address | cli <args> | update | remove
#
# Everything lives in ~/easynet (or $EASYNET_HOME): the programs, the node's data (its own ecash.conf, so an eCash
# node you already run is not touched), the enforcer's data and logs. Nothing runs as root and nothing starts at
# boot. Options: EASYNET_TAG=easynet-N installs that release instead of the newest; EASYNET_NO_ENFORCER=1 runs the
# node only.
set -Eeuo pipefail
trap 'printf "easynet: stopped by an error at line %s (see the messages above)\n" "$LINENO" >&2' ERR

REPO=mbdrivechains/bitcoin
RAW=https://raw.githubusercontent.com/$REPO/easynet/contrib/easynet
SCRIPT_URL=$RAW/easynet.sh
H=${EASYNET_HOME:-$HOME/easynet}
P2P=28533 RPC=28532 ZMQ=28535 GRPC=50051 ERPC=8122

say() { printf '%s\n' "$*"; }
die() { printf 'easynet: %s\n' "$*" >&2; exit 1; }

usage() {
    cat <<EOF
easynet: run a node and enforcer on easynet, the CPU-mined eCash-rules test network (no value).

  $H/easynet status          what is running, block height, peers
  $H/easynet start           start the node and the enforcer
  $H/easynet stop            stop them
  $H/easynet address         an address to mine to, and the miner command
  $H/easynet cli <args>      bitcoin-cli for this node (e.g. cli getblockchaininfo)
  $H/easynet update          install the newest release (also after an easynet reset)
  $H/easynet remove          stop everything and delete $H

Explorer: https://easy.ecxfreebank.com    Guide: https://github.com/$REPO/blob/easynet/EASYNET.md
EOF
}

platform() {
    case "$(uname -s)-$(uname -m)" in
        Linux-x86_64)
            PLAT=x86_64-linux-gnu
            local g; g=$(ldd --version 2>/dev/null | head -1 | grep -o '[0-9][0-9]*\.[0-9][0-9]*$' || echo 0)
            [ "$(printf '%s\n' 2.31 "$g" | sort -V | head -1)" = 2.31 ] || die "needs glibc 2.31 or newer (this system: ${g:-none})"
            ;;
        Darwin-arm64 | Darwin-x86_64)
            PLAT=universal-apple-darwin
            local v; v=$(sw_vers -productVersion)
            [ "${v%%.*}" -ge 14 ] || die "needs macOS 14 or newer (this Mac: $v)"
            ;;
        *) die "there is no easynet build for $(uname -s) $(uname -m) (Linux x86_64 and macOS only)" ;;
    esac
}

sha256() { if command -v sha256sum >/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }
port_busy() { (: </dev/tcp/127.0.0.1/"$1") 2>/dev/null; }
cli() { "$H/current/bin/bitcoin-cli" -datadir="$H/node" "$@"; }
node_up() { [ -x "$H/current/bin/bitcoin-cli" ] && cli getblockcount >/dev/null 2>&1; }
enf_pid() { cat "$H/enforcer.pid" 2>/dev/null || true; }
enf_up() { local p; p=$(enf_pid); [ -n "$p" ] && kill -0 "$p" 2>/dev/null; }
installed_tag() { [ -L "$H/current" ] && basename "$(readlink "$H/current")" || true; }

latest_tag() {
    if [ -n "${EASYNET_TAG:-}" ]; then echo "$EASYNET_TAG"; return; fi
    local t
    t=$(curl -fsSL "https://api.github.com/repos/$REPO/releases?per_page=30" 2>/dev/null |
        grep -o '"tag_name": *"easynet-[^"]*"' | head -1 | sed 's/.*"\(easynet-[^"]*\)"$/\1/') || t=""
    # GitHub's API allows 60 anonymous calls an hour per address; past that, the branch's pointer file.
    [ -n "$t" ] || t=$(curl -fsSL "$RAW/LATEST" 2>/dev/null | head -1) || t=""
    echo "$t"
}

# Download a release's node + enforcer for this platform, check them against the release's SHA256SUMS, unpack them
# into $H/releases/<tag>/bin.
fetch() {
    local T=$1 D=$H/releases/$1 W=$H/downloads/$1 U=https://github.com/$REPO/releases/download/$1
    if [ -x "$D/bin/bitcoind" ] && [ -x "$D/bin/bip300301_enforcer" ]; then return 0; fi
    local N=bitcoin-$T-$PLAT.tar.gz E=bip300301_enforcer-$T-$PLAT.tar.gz f
    rm -rf "$W" "$D.tmp"; mkdir -p "$W" "$D.tmp/bin"
    for f in "$N" "$E" SHA256SUMS; do
        say "  downloading $f"
        curl -fL --progress-bar -o "$W/$f" "$U/$f" || die "download failed: $U/$f"
    done
    (cd "$W" && grep -E " ($N|$E)\$" SHA256SUMS > sums && [ "$(wc -l < sums)" -eq 2 ] &&
        sha256 -c sums >/dev/null) || die "the downloads do not match the release's SHA256SUMS (kept in $W)"
    say "  checksums OK"
    tar -xzf "$W/$N" -C "$W"
    tar -xzf "$W/$E" -C "$W"
    cp "$W/bitcoin-$T/bin/bitcoind" "$W/bitcoin-$T/bin/bitcoin-cli" "$W/bip300301_enforcer-$T-$PLAT/bip300301_enforcer" "$D.tmp/bin/"
    rm -rf "$D" && mv "$D.tmp" "$D" && rm -rf "$W"
}

write_conf() {
    mkdir -p "$H/node"
    [ -f "$H/node/ecash.conf" ] && return 0
    cat > "$H/node/ecash.conf" <<EOF
# easynet node, written by the easynet installer (contrib/easynet/easynet.sh).
[easynet]
server=1
txindex=1
zmqpubsequence=tcp://127.0.0.1:$ZMQ
EOF
}

# 0 = running; 2 = this node's saved chain is from an older easynet (a reset); dies otherwise.
start_node() {
    if node_up; then say "node: already running"; return 0; fi
    local p log=$H/node/easynet/debug.log
    # P2P+1 is the node's own Tor port (and why ZMQ is not on it).
    for p in $P2P $((P2P + 1)) $RPC $ZMQ; do port_busy $p && die "port $p is in use by another program (another easynet node?)"; done
    local mark=0; [ -f "$log" ] && mark=$(wc -c < "$log")
    "$H/current/bin/bitcoind" -datadir="$H/node" -daemon >/dev/null 2>&1 || true
    for i in $(seq 1 120); do
        node_up && { say "node: started"; return 0; }
        # A reset changes the network magic and the genesis: the node stops at peers.dat or at the genesis check.
        if [ -f "$log" ] && tail -c +"$((mark + 1))" "$log" |
            grep -qE "Invalid network magic number|Incorrect or no genesis block found"; then return 2; fi
        [ -f "$H/node/easynet/bitcoind.pid" ] || [ "$i" -lt 10 ] || break
        sleep 1
    done
    die "the node did not start; see $log"
}

wait_sync() {
    say "node: syncing with easynet..."
    local b=0 h=0 c=0
    for i in $(seq 1 90); do
        b=$(cli getblockcount 2>/dev/null || echo 0)
        h=$(cli getblockchaininfo 2>/dev/null | grep '"headers"' | grep -o '[0-9][0-9]*' || echo 0)
        c=$(cli getconnectioncount 2>/dev/null || echo 0)
        if [ "$c" -gt 0 ] && [ "$b" -gt 0 ] && [ "$b" -ge "$h" ]; then
            say "node: synced, block $b, $c peer(s)"; return 0
        fi
        sleep 2
    done
    say "node: still syncing (block $b of $h, $c peer(s)); check later with: $H/easynet status"
}

start_enforcer() {
    if [ "${EASYNET_NO_ENFORCER:-}" = 1 ]; then return 0; fi
    if enf_up; then say "enforcer: already running"; return 0; fi
    local p
    for p in $GRPC $ERPC; do
        if port_busy $p; then
            say "enforcer: NOT started: port $p is in use (another enforcer, e.g. BitWindow's?)"; return 0
        fi
    done
    mkdir -p "$H/enforcer/logs"
    local mark; mark=$(cat "$H"/enforcer/logs/* 2>/dev/null | wc -c)
    nohup "$H/current/bin/bip300301_enforcer" --data-dir="$H/enforcer" --network-preset=easynet \
        --node-rpc-addr=127.0.0.1:$RPC --node-rpc-cookie-path="$H/node/easynet/.cookie" \
        --node-zmq-addr-sequence=tcp://127.0.0.1:$ZMQ \
        --enable-wallet --wallet-auto-create --wallet-sync-source=disabled \
        --bitcoin-core-skip-version-check \
        --log-level=info --max-log-files=3 --max-log-file-size-mb=20 >/dev/null 2>&1 &
    echo $! > "$H/enforcer.pid"
    for i in $(seq 1 30); do
        sleep 2
        enf_up || { say "enforcer: it stopped right after starting; see $H/enforcer/logs/"; return 1; }
        if cat "$H"/enforcer/logs/* 2>/dev/null | tail -c +"$((mark + 1))" | grep -q "Synced [0-9]* blocks"; then
            say "enforcer: synced, gRPC 127.0.0.1:$GRPC"; return 0
        fi
        # On a restart there may be nothing new to sync: then its gRPC port opening is the sign it is up.
        if [ "$i" -ge 5 ] && port_busy $GRPC; then say "enforcer: running, gRPC 127.0.0.1:$GRPC"; return 0; fi
    done
    say "enforcer: running, still syncing (logs in $H/enforcer/logs)"
}

stop_all() {
    if enf_up; then
        kill "$(enf_pid)" 2>/dev/null || true
        for i in $(seq 1 20); do enf_up || break; sleep 1; done
        if enf_up; then kill -9 "$(enf_pid)" 2>/dev/null || true; fi
        say "enforcer: stopped"
    fi
    rm -f "$H/enforcer.pid"
    if node_up; then
        cli stop >/dev/null
        for i in $(seq 1 60); do [ -f "$H/node/easynet/bitcoind.pid" ] || break; sleep 1; done
        say "node: stopped"
    fi
}

self_install() {
    local me=${BASH_SOURCE[0]:-}
    if [ -n "$me" ] && [ -f "$me" ]; then
        [ "$me" -ef "$H/easynet" ] || cp "$me" "$H/easynet"
    else
        curl -fsSL -o "$H/easynet" "$SCRIPT_URL" || die "could not download $SCRIPT_URL"
    fi
    chmod 755 "$H/easynet"
}

install() {
    platform
    local T old; T=$(latest_tag); old=$(installed_tag)
    [ -n "$T" ] || die "could not find an easynet release on github.com/$REPO (set EASYNET_TAG=easynet-N to choose one)"
    mkdir -p "$H"
    if [ "$old" = "$T" ]; then
        say "easynet $T is installed in $H"
    else
        say "installing easynet $T ($PLAT) into $H"
        fetch "$T"
        stop_all
        ln -sfn "releases/$T" "$H/current"
        [ -z "$old" ] || rm -rf "${H:?}/releases/$old"
    fi
    self_install
    write_conf
    local rc=0; start_node || rc=$?
    if [ $rc = 2 ]; then
        local A; A=$H/old/${old:-older}-$(date +%Y%m%d-%H%M%S)
        say "easynet was reset since this node last ran: moving its old chain to $A"
        for i in $(seq 1 30); do [ -f "$H/node/easynet/bitcoind.pid" ] || break; sleep 1; done
        mkdir -p "$A"
        mv "$H/node/easynet" "$A/node-easynet"
        if [ -d "$H/enforcer" ]; then mv "$H/enforcer" "$A/enforcer"; fi
        start_node || die "the node did not start after the reset; see $H/node/easynet/debug.log"
    fi
    wait_sync
    start_enforcer || true
    cat <<EOF

easynet $T is running (programs and data in $H; nothing starts at boot).
  $H/easynet status       see what is running
  $H/easynet address      get an address and the command to mine
  $H/easynet stop         stop it ($H/easynet start to start again)
  $H/easynet update       newest release, also after an easynet reset
  $H/easynet remove       stop it and delete $H
Explorer: https://easy.ecxfreebank.com
EOF
}

status() {
    local T; T=$(installed_tag); [ -n "$T" ] || die "easynet is not installed in $H"
    say "release:  $T ($H)"
    if node_up; then
        say "node:     running, block $(cli getblockcount), $(cli getconnectioncount) peer(s), chain $(cli getblockchaininfo | grep '"chain"' | cut -d'"' -f4)"
    else
        say "node:     not running"
    fi
    if enf_up; then say "enforcer: running (pid $(enf_pid)), gRPC 127.0.0.1:$GRPC"; else say "enforcer: not running"; fi
    say "explorer: https://easy.ecxfreebank.com"
}

address() {
    node_up || die "the node is not running ($H/easynet start)"
    if ! cli listwallets | grep -q '"mine"'; then
        cli loadwallet mine >/dev/null 2>&1 || cli createwallet mine >/dev/null
    fi
    local a; a=$(cli -rpcwallet=mine getnewaddress)
    cat <<EOF
address: $a
Mine to it with any SHA-256 miner, e.g. cpuminer-opt (https://github.com/JayDDee/cpuminer-opt):
  cpuminer -a sha256d -o stratum+tcp://seed.ecxfreebank.com:28334 -u $a -p x
Each block pays its finder; coins can be spent 100 blocks later. Balance: $H/easynet cli -rpcwallet=mine getbalance
EOF
}

remove() {
    [ -d "$H" ] || die "nothing is installed in $H"
    say "This stops easynet and deletes $H: the programs, the node's chain and wallet, the enforcer's data and wallet."
    say "(easynet coins have no value.)"
    if [ "${1:-}" != "--yes" ]; then
        local ans=""
        printf 'Type yes to remove it: '
        read -r ans < /dev/tty || ans=""
        [ "$ans" = yes ] || die "nothing removed"
    fi
    stop_all
    rm -rf "${H:?}"
    say "removed $H"
}

cmd=${1:-install}
[ $# -gt 0 ] && shift
case $cmd in
    install | update) install ;;
    start)
        [ -n "$(installed_tag)" ] || die "easynet is not installed in $H"
        rc=0; start_node || rc=$?
        [ $rc = 0 ] || die "this node's chain is from an older easynet: run $H/easynet update"
        wait_sync; start_enforcer || true ;;
    stop) stop_all ;;
    status) status ;;
    address) address ;;
    cli) node_up || die "the node is not running ($H/easynet start)"; cli "$@" ;;
    remove | uninstall) remove "$@" ;;
    help | -h | --help) usage ;;
    *) usage; exit 1 ;;
esac
