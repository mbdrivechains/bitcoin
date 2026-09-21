// Copyright (c) 2015-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>
#include <chain.h>
#include <chainparams.h>
#include <pow.h>
#include <test/util/random.h>
#include <test/util/common.h>
#include <test/util/setup_common.h>
#include <util/chaintype.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(pow_tests, BasicTestingSetup)

/* Test calculation of next difficulty target with no constraints applying */
BOOST_AUTO_TEST_CASE(get_next_work)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    int64_t nLastRetargetTime = 1261130161; // Block #30240
    CBlockIndex pindexLast;
    pindexLast.nHeight = 32255;
    pindexLast.nTime = 1262152739;  // Block #32255
    pindexLast.nBits = 0x1d00ffff;

    // Here (and below): expected_nbits is calculated in
    // CalculateNextWorkRequired(); redoing the calculation here would be just
    // reimplementing the same code that is written in pow.cpp. Rather than
    // copy that code, we just hardcode the expected result.
    unsigned int expected_nbits = 0x1d00d86aU;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), expected_nbits);
    BOOST_CHECK(PermittedDifficultyTransition(chainParams->GetConsensus(), pindexLast.nHeight+1, pindexLast.nBits, expected_nbits));
}

/* Test the constraint on the upper bound for next work */
BOOST_AUTO_TEST_CASE(get_next_work_pow_limit)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    int64_t nLastRetargetTime = 1231006505; // Block #0
    CBlockIndex pindexLast;
    pindexLast.nHeight = 2015;
    pindexLast.nTime = 1233061996;  // Block #2015
    pindexLast.nBits = 0x1d00ffff;
    unsigned int expected_nbits = 0x1d00ffffU;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), expected_nbits);
    BOOST_CHECK(PermittedDifficultyTransition(chainParams->GetConsensus(), pindexLast.nHeight+1, pindexLast.nBits, expected_nbits));
}

/* Test the constraint on the lower bound for actual time taken */
BOOST_AUTO_TEST_CASE(get_next_work_lower_limit_actual)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    int64_t nLastRetargetTime = 1279008237; // Block #66528
    CBlockIndex pindexLast;
    pindexLast.nHeight = 68543;
    pindexLast.nTime = 1279297671;  // Block #68543
    pindexLast.nBits = 0x1c05a3f4;
    unsigned int expected_nbits = 0x1c0168fdU;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), expected_nbits);
    BOOST_CHECK(PermittedDifficultyTransition(chainParams->GetConsensus(), pindexLast.nHeight+1, pindexLast.nBits, expected_nbits));
    // Test that reducing nbits further would not be a PermittedDifficultyTransition.
    unsigned int invalid_nbits = expected_nbits-1;
    BOOST_CHECK(!PermittedDifficultyTransition(chainParams->GetConsensus(), pindexLast.nHeight+1, pindexLast.nBits, invalid_nbits));
}

/* Test the constraint on the upper bound for actual time taken */
BOOST_AUTO_TEST_CASE(get_next_work_upper_limit_actual)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    int64_t nLastRetargetTime = 1263163443; // NOTE: Not an actual block time
    CBlockIndex pindexLast;
    pindexLast.nHeight = 46367;
    pindexLast.nTime = 1269211443;  // Block #46367
    pindexLast.nBits = 0x1c387f6f;
    unsigned int expected_nbits = 0x1d00e1fdU;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), expected_nbits);
    BOOST_CHECK(PermittedDifficultyTransition(chainParams->GetConsensus(), pindexLast.nHeight+1, pindexLast.nBits, expected_nbits));
    // Test that increasing nbits further would not be a PermittedDifficultyTransition.
    unsigned int invalid_nbits = expected_nbits+1;
    BOOST_CHECK(!PermittedDifficultyTransition(chainParams->GetConsensus(), pindexLast.nHeight+1, pindexLast.nBits, invalid_nbits));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_negative_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    nBits = UintToArith256(consensus.powLimit).GetCompact(true);
    hash = uint256{1};
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_overflow_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits{~0x00800000U};
    hash = uint256{1};
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_too_easy_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 nBits_arith = UintToArith256(consensus.powLimit);
    nBits_arith *= 2;
    nBits = nBits_arith.GetCompact();
    hash = uint256{1};
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_biger_hash_than_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 hash_arith = UintToArith256(consensus.powLimit);
    nBits = hash_arith.GetCompact();
    hash_arith *= 2; // hash > nBits
    hash = ArithToUint256(hash_arith);
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_zero_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 hash_arith{0};
    nBits = hash_arith.GetCompact();
    hash = ArithToUint256(hash_arith);
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(GetBlockProofEquivalentTime_test)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    std::vector<CBlockIndex> blocks(10000);
    for (int i = 0; i < 10000; i++) {
        blocks[i].pprev = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = i;
        blocks[i].nTime = 1269211443 + i * chainParams->GetConsensus().nPowTargetSpacing;
        blocks[i].nBits = 0x207fffff; /* target 0x7fffff000... */
        blocks[i].nChainWork = i ? blocks[i - 1].nChainWork + GetBlockProof(blocks[i - 1]) : arith_uint256(0);
    }

    for (int j = 0; j < 1000; j++) {
        CBlockIndex *p1 = &blocks[m_rng.randrange(10000)];
        CBlockIndex *p2 = &blocks[m_rng.randrange(10000)];
        CBlockIndex *p3 = &blocks[m_rng.randrange(10000)];

        int64_t tdiff = GetBlockProofEquivalentTime(*p1, *p2, *p3, chainParams->GetConsensus());
        BOOST_CHECK_EQUAL(tdiff, p1->GetBlockTime() - p2->GetBlockTime());
    }
}

// ---- ASERT (aserti3-2d) ----------------------------------------------------

static constexpr uint32_t ASERT_REF_BITS{0x19044b7e}; // ECX beta fork bits, difficulty ~1e9
static constexpr int64_t ASERT_HALF_LIFE{24 * 60 * 60};

BOOST_AUTO_TEST_CASE(asert_reference_vectors)
{
    // Expected values come from an independent big-integer implementation of the spec
    // (Python, exact arithmetic); this pins the C++ fixed-point and 256-bit plumbing.
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    const arith_uint256 pow_limit = UintToArith256(consensus.powLimit);
    arith_uint256 ref;
    ref.SetCompact(ASERT_REF_BITS);

    struct Vector { int64_t time_diff; int64_t height_diff; uint32_t expected_bits; };
    static const Vector vectors[] = {
    {0LL, 0LL, 0x1904463b},
    {60000LL, 99LL, 0x19044b7e},
    {146400LL, 99LL, 0x190896fc},
    {-26400LL, 99LL, 0x190225bf},
    {1209600LL, 2015LL, 0x19044b7e},
    {20160LL, 2015LL, 0x1714323e},
    {2592000LL, 10LL, 0x1d00ffff},
    {-86400LL, 5LL, 0x19021621},
    {30259200LL, 49999LL, 0x19225bf0},
    {37801944LL, 63370LL, 0x1900bb39},
    {29777357LL, 49924LL, 0x1901086b},
    {7059149LL, 11921LL, 0x19020512},
    {59733996LL, 99378LL, 0x190a19dc},
    {29669336LL, 49302LL, 0x1908ab26},
    {51257440LL, 85737LL, 0x1900f88e},
    {56315530LL, 93789LL, 0x1905fe14},
    {110193822LL, 183966LL, 0x1900f688},
    {64127900LL, 106323LL, 0x193e5e1f},
    {31008407LL, 52063LL, 0x1900adb8},
    {86705137LL, 143800LL, 0x1a008176},
    {78963726LL, 131691LL, 0x1902d77b},
    };
    for (const auto& v : vectors) {
        const arith_uint256 target = CalculateASERT(ref, consensus.nPowTargetSpacing, v.time_diff, v.height_diff, pow_limit, ASERT_HALF_LIFE);
        BOOST_CHECK_EQUAL(target.GetCompact(), v.expected_bits);
    }
}

BOOST_AUTO_TEST_CASE(asert_invariants)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    const arith_uint256 pow_limit = UintToArith256(consensus.powLimit);
    const int64_t spacing = consensus.nPowTargetSpacing;
    arith_uint256 ref;
    ref.SetCompact(ASERT_REF_BITS);

    // Exactly on schedule: the anchor target comes back untouched, at any height.
    for (int64_t h : {0, 1, 2015, 2016, 100000, 1000000}) {
        BOOST_CHECK(CalculateASERT(ref, spacing, spacing * (h + 1), h, pow_limit, ASERT_HALF_LIFE) == ref);
    }
    // One half-life behind schedule doubles the target; one ahead halves it.
    BOOST_CHECK(CalculateASERT(ref, spacing, spacing * 100 + ASERT_HALF_LIFE, 99, pow_limit, ASERT_HALF_LIFE) == ref << 1);
    BOOST_CHECK(CalculateASERT(ref, spacing, spacing * 100 - ASERT_HALF_LIFE, 99, pow_limit, ASERT_HALF_LIFE) == ref >> 1);
    // Monotone in time: a later timestamp never gives a harder target.
    arith_uint256 prev = CalculateASERT(ref, spacing, -ASERT_HALF_LIFE * 3, 0, pow_limit, ASERT_HALF_LIFE);
    for (int64_t t = -ASERT_HALF_LIFE * 3; t <= ASERT_HALF_LIFE * 3; t += 977) {
        const arith_uint256 cur = CalculateASERT(ref, spacing, t, 0, pow_limit, ASERT_HALF_LIFE);
        BOOST_CHECK(cur >= prev);
        prev = cur;
    }
    // Far behind schedule saturates at the limit; far ahead floors at 1. Both paths are
    // exercised: shifts below 256 (overflow caught by the round-trip check) and at or above it.
    BOOST_CHECK(CalculateASERT(ref, spacing, ASERT_HALF_LIFE * 40, 0, pow_limit, ASERT_HALF_LIFE) == pow_limit);
    BOOST_CHECK(CalculateASERT(ref, spacing, ASERT_HALF_LIFE * 300, 0, pow_limit, ASERT_HALF_LIFE) == pow_limit);
    BOOST_CHECK(CalculateASERT(ref, spacing, int64_t{1} << 50, 0, pow_limit, ASERT_HALF_LIFE) == pow_limit);
    BOOST_CHECK(CalculateASERT(ref, spacing, -ASERT_HALF_LIFE * 200, 0, pow_limit, ASERT_HALF_LIFE) == arith_uint256{1});
    BOOST_CHECK(CalculateASERT(ref, spacing, -ASERT_HALF_LIFE * 300, 0, pow_limit, ASERT_HALF_LIFE) == arith_uint256{1});
    BOOST_CHECK(CalculateASERT(ref, spacing, -(int64_t{1} << 50), 0, pow_limit, ASERT_HALF_LIFE) == arith_uint256{1});
    // Starting at the limit and falling behind stays at the limit.
    BOOST_CHECK(CalculateASERT(pow_limit, spacing, spacing * 10 + ASERT_HALF_LIFE, 9, pow_limit, ASERT_HALF_LIFE) == pow_limit);
}

BOOST_AUTO_TEST_CASE(asert_chain)
{
    // A synthetic chain: 2016-block rule up to the anchor, ASERT above it. Checkpoints come
    // from the Python model of the same chain (asert_ref.py chain_sim).
    Consensus::Params params = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    constexpr int ANCHOR{100};
    params.EcashAsertAnchorHeight = ANCHOR;
    params.EcashAsertHalfLife = ASERT_HALF_LIFE;
    const int64_t spacing = params.nPowTargetSpacing;

    struct Phase { int64_t gap; int count; };
    static const Phase plan[] = {{600, 50}, {10, 200}, {6000, 100}};
    int total = ANCHOR + 1;
    for (const auto& ph : plan) total += ph.count;

    std::vector<CBlockIndex> blocks(total);
    for (int h = 0; h <= ANCHOR; ++h) {
        blocks[h].pprev = h ? &blocks[h - 1] : nullptr;
        blocks[h].nHeight = h;
        blocks[h].nTime = 1600000000 + spacing * h;
        blocks[h].nBits = ASERT_REF_BITS;
        blocks[h].BuildSkip();
    }
    // Below and at the anchor the old rule answers: no ASERT, no change off a boundary.
    BOOST_CHECK(!IsAsertHeight(params, ANCHOR));
    BOOST_CHECK(IsAsertHeight(params, ANCHOR + 1));
    {
        CBlockHeader hdr;
        hdr.nTime = blocks[ANCHOR - 1].nTime + spacing;
        BOOST_CHECK_EQUAL(GetNextWorkRequired(&blocks[ANCHOR - 1], &hdr, params), ASERT_REF_BITS);
    }

    int h = ANCHOR;
    for (const auto& ph : plan) {
        for (int i = 0; i < ph.count; ++i) {
            CBlockHeader hdr;
            hdr.nTime = blocks[h].nTime + ph.gap;
            const uint32_t bits = GetNextWorkRequired(&blocks[h], &hdr, params);
            // Every step must pass the headers-sync guard.
            BOOST_CHECK(PermittedDifficultyTransition(params, h + 1, blocks[h].nBits, bits));
            ++h;
            blocks[h].pprev = &blocks[h - 1];
            blocks[h].nHeight = h;
            blocks[h].nTime = hdr.nTime;
            blocks[h].nBits = bits;
            blocks[h].BuildSkip();
        }
    }
    BOOST_CHECK_EQUAL(h, total - 1);

    struct Checkpoint { int height; uint32_t bits; };
    static const Checkpoint checkpoints[] = {
    {101, 0x19044b7e},
    {150, 0x19044b7e},
    {151, 0x19044b7e},
    {200, 0x190367e9},
    {350, 0x1901aca9},
    {351, 0x1901aaa3},
    {400, 0x190dec52},
    {450, 0x197979b2},
    };
    for (const auto& c : checkpoints) {
        BOOST_CHECK_EQUAL(blocks[c.height].nBits, c.bits);
    }

    // Shape: flat while on schedule, harder through the burst, easier through the stall.
    auto target = [&](int height) { arith_uint256 t; t.SetCompact(blocks[height].nBits); return t; };
    BOOST_CHECK(target(ANCHOR + 50) == target(ANCHOR));
    for (int i = ANCHOR + 52; i <= ANCHOR + 250; ++i) BOOST_CHECK(target(i) < target(i - 1));
    for (int i = ANCHOR + 252; i <= ANCHOR + 350; ++i) BOOST_CHECK(target(i) > target(i - 1));
    // The 6000 s stall is still far from the limit: recovery is exponential in time, not a reset.
    BOOST_CHECK(target(total - 1) < UintToArith256(params.powLimit));

    // The target depends on the previous block's time only, never on the new header's.
    {
        CBlockHeader early, late;
        early.nTime = blocks[h].nTime + 1;
        late.nTime = blocks[h].nTime + 100000;
        BOOST_CHECK_EQUAL(GetNextWorkRequired(&blocks[h], &early, params), GetNextWorkRequired(&blocks[h], &late, params));
    }
}

BOOST_AUTO_TEST_CASE(asert_permitted_difficulty_transition)
{
    Consensus::Params params = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    params.EcashAsertAnchorHeight = 1000;
    params.EcashAsertHalfLife = ASERT_HALF_LIFE;
    const int64_t height = 1001; // first ASERT block, not a 2016 boundary

    arith_uint256 old_target;
    old_target.SetCompact(ASERT_REF_BITS);
    const auto bits = [](const arith_uint256& t) { return t.GetCompact(); };

    BOOST_CHECK(PermittedDifficultyTransition(params, height, ASERT_REF_BITS, ASERT_REF_BITS));
    BOOST_CHECK(PermittedDifficultyTransition(params, height, ASERT_REF_BITS, bits(old_target >> 2)));   // 4x harder: allowed
    BOOST_CHECK(!PermittedDifficultyTransition(params, height, ASERT_REF_BITS, bits(old_target / 5)));   // 5x harder: not
    BOOST_CHECK(PermittedDifficultyTransition(params, height, ASERT_REF_BITS, bits(old_target << 10)));  // easier: always
    BOOST_CHECK(PermittedDifficultyTransition(params, height, ASERT_REF_BITS, bits(UintToArith256(params.powLimit))));
    BOOST_CHECK(!PermittedDifficultyTransition(params, height, ASERT_REF_BITS, 0x1e00ffff));             // above powLimit
    // Below the anchor the old rule is untouched: off a boundary the bits must not move at all.
    BOOST_CHECK(!PermittedDifficultyTransition(params, 999, ASERT_REF_BITS, bits(old_target >> 2)));
    BOOST_CHECK(PermittedDifficultyTransition(params, 999, ASERT_REF_BITS, ASERT_REF_BITS));
}

void sanity_check_chainparams(const ArgsManager& args, ChainType chain_type)
{
    const auto chainParams = CreateChainParams(args, chain_type);
    const auto consensus = chainParams->GetConsensus();

    // hash genesis is correct
    BOOST_CHECK_EQUAL(consensus.hashGenesisBlock, chainParams->GenesisBlock().GetHash());

    // target timespan is an even multiple of spacing
    BOOST_CHECK_EQUAL(consensus.nPowTargetTimespan % consensus.nPowTargetSpacing, 0);

    // genesis nBits is positive, doesn't overflow and is lower than powLimit
    arith_uint256 pow_compact;
    bool neg, over;
    pow_compact.SetCompact(chainParams->GenesisBlock().nBits, &neg, &over);
    BOOST_CHECK(!neg && pow_compact != 0);
    BOOST_CHECK(!over);
    BOOST_CHECK(UintToArith256(consensus.powLimit) >= pow_compact);

    // an ASERT anchor needs a parent block and a positive half-life -- see pow.cpp:GetNextASERTWorkRequired()
    if (consensus.EcashAsertAnchorHeight != 0) {
        BOOST_CHECK(consensus.EcashAsertAnchorHeight >= 1);
        BOOST_CHECK(consensus.EcashAsertHalfLife > 0);
    }

    // check max target * 4*nPowTargetTimespan doesn't overflow -- see pow.cpp:CalculateNextWorkRequired()
    if (!consensus.fPowNoRetargeting) {
        arith_uint256 targ_max{UintToArith256(uint256{"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"})};
        targ_max /= consensus.nPowTargetTimespan*4;
        BOOST_CHECK(UintToArith256(consensus.powLimit) < targ_max);
    }
}

BOOST_AUTO_TEST_CASE(ChainParams_MAIN_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::MAIN);
}

BOOST_AUTO_TEST_CASE(ChainParams_REGTEST_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::REGTEST);
}

BOOST_AUTO_TEST_CASE(ChainParams_TESTNET_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::TESTNET);
}

BOOST_AUTO_TEST_CASE(ChainParams_TESTNET4_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::TESTNET4);
}

BOOST_AUTO_TEST_CASE(ChainParams_SIGNET_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::SIGNET);
}

BOOST_AUTO_TEST_SUITE_END()
