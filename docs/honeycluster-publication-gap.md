# Reader publication pauses: verified findings and open investigation

Status: **not resolved or certified**. This is a public, sanitized source-facing
companion to the operator report
`honeycluster/provision/docs/clio-reader-stream-publication-gap-2026-10-06.md`.
No private endpoints, credentials, raw provider bodies or protected backup paths
are included here.

## Confirmed symptoms

A simultaneous five-minute diagnostic on the original 2.8.0 image recorded reader
inter-event gaps of **37.734 / 37.805 seconds**, while source-node progression
continued. A private gateway followed the same reader pauses. During one pause a
reader's last emitted ledger was 107481177, but its RPC tip was 107481181, aged
six seconds. Publication later resumed with contiguous, aged events.

The gateway observation localizes that sampled delay to its reader path; it does
not prove all gateway behavior fault-free or explain every historical incident.
Separate authenticated continuous gateway candidates failed their 20-second
inactivity limit after approximately 158.4 seconds, short of the required
900-second clean window. These failures were not converted to passes by changing
deadlines.

## Failed experiments and limitations

- Original-image, cache-disabled private readers passed HTTP/historical checks
  but failed subscription checks. Disabling cache alone was not a demonstrated fix.
- A fixed-ledger probe returned 733 transaction hashes in 10/265 ms and expanded
  transactions in 354/456 ms on two readers. These isolated samples do not rule
  out transient stalls or other publication work.
- A private Backend debug experiment yielded no usable parsed stage timings.
  Its configuration was restored; no operation-level cause was established.
- Earlier candidate builds failed first because tests were disabled in a cached
  source stage, then on the GCC 15 channel-test diagnostic. Later successful
  builds supersede those build failures, not the unresolved runtime incident.

## Later 900-second direct correlation

One idle remote reader ran the transaction-feed optimization; the writer and two
local readers retained the original image. This was a direct-node diagnostic,
not a gateway admission run or equal-workload A/B benchmark.

| Target | Events | Maximum close age | Maximum completed gap | Inactivity alerts |
| --- | ---: | ---: | ---: | ---: |
| Source reference | 232 | 8.670 s | 9.210 s | 0 |
| Original writer | 231 | 22.041 s | 20.707 s | 1 |
| Patched idle reader | 231 | 22.273 s | 20.286 s | 1 |
| Original reader A | 231 | 37.007 s | 24.561 s | 4 |
| Original reader B | 231 | 29.764 s | 24.453 s | 2 |

Within each stream, sequences remained contiguous. No transport or RPC-deadline
failures were recorded. Event-count differences occurred at the observation
boundary. At one reader pause, the last emitted sequence was 107486722 while
RPC already reported 107486727 at age five seconds. At a shared pause after
107486788, the writer and idle-reader RPC tips also lagged. These distinguish
additional reader publication delay from a shared upstream pause; they do not
prove a single common operation caused both.

Existing INFO logs, without serving-process configuration changes, showed:

- Reader A publication of 107486722 lasted **22.729 seconds**. Its ledger event
  arrived near the start; publication of the next ledger began immediately after
  that task finished.
- Writer publication of ledgers 107486722/107486788 lasted **16.884/20.675 seconds**,
  for 1,515/1,766 transactions. Reported DB finish-write times were only 250/46 ms.
  Next-ledger fetch completions appeared shortly after publication completion.
- The patched idle reader's corresponding tasks lasted **0.812/0.774 seconds**.
- A later writer snapshot showed no transaction-related subscribers, one ledger
  subscriber and enabled/incomplete cache. This is a later snapshot, not proof
  of subscriber state throughout every historical task.

These timings are useful localization evidence. They do not separate individual
funding reads, sorting, serialization, book processing, or scheduling delays.

## Verified pinned-source facts (not causal conclusions)

The investigated source is upstream commit
`c64681704e7352b10fdbc4eaccb7039e2b251f0c`, not an unrelated nightly checkout.
The original deployment reported short revision `c646817`; that association alone
is not complete reproducible-image provenance.

1. `ETLService::updateCache` fetches a diff when cache sequence is behind, without
   an explicit disabled-cache guard, before requesting publication. The idle-feed
   patch does **not** change this condition. Its causal contribution has not been
   measured; adding a disabled-cache guard would not bypass enabled-cache updates.
2. `etl/impl/LedgerPublisher.hpp` reads fees/transactions, dispatches the ledger
   event, sorts metadata, invokes transaction publication, then publishes book
   changes on an ordered strand. Work after one ledger event can delay the next.
3. Sorting deserializes metadata inside its comparator. This is a potential
   optimization target, not a measured explanation of the observed long tasks.
4. The last-published sequence accessor tracks requested/scheduled publication,
   not confirmation of delivery to clients.
5. The application supplies a shared ETL execution context. `io_threads` defaults
   to two, and the inspected writer omitted the setting. Starvation is a hypothesis,
   not a demonstrated cause or justification to change writer concurrency.
6. `rpc::postProcessOrderBook` already keeps per-owner running balances. Any
   funding optimization must preserve that accounting rather than naively caching
   balances across offers or ledgers.

## Reproduction and next resolution checks

Using reviewed private endpoints and an authorized existing caller:

1. Observe the source reference, affected readers and private gateway together.
   The writer may be an optional read-only diagnostic reference, never a public
   fallback backend.
2. Subscribe to ledger events and poll `server_info` every five seconds. Record
   only timestamps, sequence, close age, inter-event gap and RPC tip sequence/age.
3. Flag 20-second inactivity, stale events, missing/nonmonotonic sequences,
   transport errors and RPC deadlines. Keep diagnostics running long enough to
   observe recovery; do not confuse a continuing diagnostic with a passing gate.
4. Correlate exact ledgers with monitor/publication timestamps. Obtain separate
   timings for diff/cache update, range/header/fee/transaction reads, publication
   queue wait, metadata sorting, transaction funding/notification work, and book
   changes. Use opt-in instrumentation on isolated readers if existing logs cannot
   distinguish them; never emit credentials, request parameters or provider bodies.
5. Review a narrow correction only after measurement. Test active and idle feeds,
   enabled/disabled cache, payload parity, ordering, historical data and corruption
   safeguards using an immutable image.
6. Independently requalify gateway classification, backend distribution and both
   directions of private failover. Require clean 900-second admission windows;
   development before production. Preserve rollback evidence and earlier failures.

The runtime fix currently implemented on this branch is only the no-listener
transaction-feed early return described in [the change record](honeycluster-publication.md).
Opt-in stage-timing instrumentation is now available for private diagnosis; see
the change record for its scope and unit verification. A disabled-cache guard,
active-feed parallelization, and writer scheduling changes are **not implemented**.
No serving-reader resolution is inferred from adding instrumentation.
