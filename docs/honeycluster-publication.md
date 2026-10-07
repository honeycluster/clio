# Honeycluster publication changes and verification

## Scope and source provenance

This branch starts at upstream Clio 2.8.0 commit
`c64681704e7352b10fdbc4eaccb7039e2b251f0c`. It imports the three operator patches
previously maintained in `honeycluster/cron/packages/docker/clio/patches/operational/`.
The fork branch is the source of truth for subsequent Honeycluster changes;
external operator patch bundles must be kept in sync and must **not** be applied
again to this already-patched checkout.
It is not a port to upstream `develop`, not an upstream-merged fix, and not a
certificate that all reader-publication pauses are resolved.

### Runtime optimization

`TransactionFeed::pub` returns before decoding transactions, fetching offer-owner
funds, and constructing notifications only when all five transaction-related
registries are empty: transactions, proposed transactions, accounts, proposed
accounts, and books. Proposed subscriptions must count because they also receive
validated transactions. `TrackableSignalMap::empty` uses the existing mutex;
subscription metrics are not synchronization primitives.

Ledger and book-change publication remain on their existing paths. Active
transaction subscribers retain the existing funding and payload behavior.
A subscription racing with the early check retains a timing boundary: a listener
connecting after the check does not receive that transaction.

The added tests check map emptiness after disconnect and verify that a
cache-disabled feed performs zero backend object reads with no subscribers,
including after subscribe/unsubscribe.

### Test/build corrections

- For GCC 15 only, disable tree scalar replacement (`-fno-tree-sra`) for
  `tests/unit/util/ChannelTests.cpp`, where the optimizer diagnosed an
  uninitialized `shared_ptr` temporary. Keep `-Werror` and test assertions.
  No server translation unit receives this workaround. This is a scoped
  workaround, not a claim that every compiler diagnostic is spurious.
- Use counting semaphores for repeatable cluster-test callbacks, and bounded
  completion waits for independent read/write tasks instead of a fixed sleep.
  Preserve mock expectations and verify tasks have run before stopping them.
- `tools/verify-publication.sh` explicitly enables tests before building server
  and test binaries, then repeats the targeted and full enabled unit suites.
  Integration tests still require a separately provisioned environment; this
  script does not claim to run them or enable upstream-disabled tests.

## Build and test

Configure compiler, Conan profiles and dependencies using [build-clio.md](build-clio.md).
Keep the existing dependency lockfile. When installing dependencies, bound both
Conan host and build jobs, for example with `-c:h tools.build:jobs=4` and
`-c:b tools.build:jobs=4`; `CONAN_CPU_COUNT` alone is not a reliable Conan 2 limit.
Then run:

```sh
BUILD_JOBS=4 TEST_REPEATS=3 bash tools/verify-publication.sh /path/to/configured/build
```

The recorded operator qualification of these source changes passed all **3,325
enabled unit tests three times**. The repaired cluster tests also passed 50
repeats. Later explanatory source comments do not change runtime behavior.
Fresh branch verification and future changes must not be inferred solely from
those historical results.

### Fresh verification during the fork import

The changed source files were installed exactly into an isolated Linux build
container and both `clio_server` and `clio_tests` were rebuilt (four CPUs,
32 GiB memory cap, no build/test network access). This was necessary because an
older cached build image did not retain the later cluster-test source changes.

Results:

- Targeted feed/channel/cluster suites: **60 tests passed, three repetitions**.
- Full suite: **3,325 tests passed** on the first in-process repetition.
- The second in-process repetition hung at `SignalsHandlerTests.NoSignal` and
  was stopped by a **600-second timeout**. This is a recorded failure, not a pass.
  `SignalsHandlerTests.cpp` is unchanged by this branch; the cause is unresolved.
- Follow-up: `SignalsHandlerTests.NoSignal` alone passed 50 repetitions; three
  separate fresh-process full-suite invocations each passed all 3,325 tests.
  Those passes do not erase or explain the in-process repeat hang.
- Verification helper syntax, command construction, and invalid job/repeat
  argument rejection were checked with command fixtures.

Keep a CI/job wall-time limit around repeated suites. Do not remove signal tests
or weaken assertions to hide repeat-run behavior.

### Follow-up: shutdown wakeup correction and opt-in stage diagnostics

The destructor and graceful-completion path updated the condition-variable
predicate outside the waiter's mutex. Atomic state alone does not prevent a
notification from being lost between predicate evaluation and entering the wait.
Those ordinary-thread updates now hold that mutex. This is a scoped lost-wakeup
correction, not a redesign or certification of all asynchronous signal handling.

A new immediate-construction/destruction regression test runs 1,000 cycles.
Shutdown suites passed 100 repetitions (100,000 such cycles), and the complete
**3,328-test enabled suite passed three in-process repetitions** after rebuilding.
The earlier repeat-run hang above remains part of the history.

`CLIO_PUBLICATION_TIMINGS=1` enables diagnostic-only INFO records on the ETL log
channel. Use it only on isolated qualification readers initially. It records
ledger sequence, static operation/stage names, elapsed microseconds, and cumulative
microseconds, never transaction bodies or caller parameters. Logging must allow
ETL INFO to expose the records. Diagnostics are off by default.

Stages distinguish monitor cache checks, diff reads, cache updates and publication
requests; and publisher queue wait, fee/transaction reads, ledger dispatch,
metadata sorting, transaction notifications and book changes. `publish` cumulative
time includes queue wait. Individual stages can include small logging/bookkeeping
overheads; transaction notification timing still combines funding and serialization.
The instrumentation does not skip cache updates, alter ordering, or parallelize
active feeds. Sustained live qualification and a measured performance correction
remain prerequisites for further deployment.

## Recorded live qualification (2026-10-06/07)

The published operator candidate is:

`honeycluster/clio@sha256:ad72787f6c86a164d0f0311adc17f5ea1ddd22afe7924eb990fd655f49210640`

It was built before this fork branch was created and before the later comment-only
clarification; it must not be described as an image built from the new fork
commit. Normal version and `latest` tags were not replaced by this experiment.

- Controlled, cache-disabled, five-minute idle-reader A/B: patched reader
  delivered 82 contiguous events, maximum close age 10.8 seconds; original
  delivered 49 events with maximum age 256.8 seconds.
- Isolated WAN reader: 78 contiguous events, maximum age 16.3 seconds.
- Active API v1/v2 comparison: 2,061/2,059 common transaction payloads matched,
  including 793 owner-funds payloads per version. Separate account,
  proposed-account, book, proposed-transaction and book-change probes matched.
- Three historical terminal-pagination checks matched an independent reference.
  This is not proof that an entire database is free from other integrity defects.
- One idle production reader received the image with unchanged application
  configuration and resource settings. Its initial five-minute window delivered
  77 contiguous events, maximum age 8.25 seconds. Other readers and the writer
  were not upgraded.

**A later 900-second correlation failed strict freshness/inactivity thresholds.**
See [the publication-gap investigation](honeycluster-publication-gap.md). The
idle-path improvement is not a fix for all active-feed stalls, a completed
fleet rollout, or a gateway admission certificate.

## Release requirements

1. Verify exact source revision, applied changes, compiler/dependency provenance,
   tests and immutable image identity. Never silently retag an existing release.
2. Use isolated read-only readers with bounded resources and private endpoints.
   Derive credentials on their owning host; never include them in Git or logs.
3. Test cache-enabled and cache-disabled operation, active API v1/v2 delivery,
   account/book/proposed semantics, historical reads, ordering, owner funds and
   disconnect behavior. Do not disable corruption protections or force a cache full.
4. Observe actual WebSocket ledger age, sequences and inactivity alongside RPC
   tips and a source reference. A fresh RPC tip is not proof of fresh delivery.
5. Preserve failed experiments and require new, independent, clean 900-second
   gateway admission windows plus classification/distribution/failover checks.
   Promote development before production; do not increase deadlines to obtain a pass.
6. Keep writer protections and rollback/checkpoint evidence. A reader canary is
   not permission for a writer restart, database repair, checkpoint reset or writer
   fallback in a public pool.

Operator rollout details and protected evidence stay in the provision repository
and on their owning hosts, not in this public source fork.
