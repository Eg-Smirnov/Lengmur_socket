#include <assert.h>
#include <stdint.h>

#include "../../esp/src/application/configuration_transaction.h"

namespace {

using State = ConfigurationTransaction::State;

void testCommit() {
  ConfigurationTransaction transaction;
  assert(transaction.begin(7));
  assert(!transaction.begin(8));
  assert(transaction.prepared(7, true));
  assert(transaction.state() == State::NeedCommit);
  assert(transaction.decisionSent());
  assert(transaction.state() == State::AwaitingCommit);
  assert(transaction.finish(7, State::AwaitingCommit));
  assert(!transaction.active());
}

void testRollback() {
  ConfigurationTransaction transaction;
  assert(transaction.begin(9));
  assert(transaction.prepared(9, false));
  assert(transaction.state() == State::NeedRollback);
  assert(transaction.decisionSent());
  assert(transaction.state() == State::AwaitingRollback);
  assert(transaction.finish(9, State::AwaitingRollback));
  assert(!transaction.active());
}

void testRejectedPrepare() {
  ConfigurationTransaction transaction;
  assert(transaction.begin(11));
  assert(!transaction.reject(12));
  assert(transaction.reject(11));
  assert(!transaction.active());
}

void testUnexpectedResultsDoNotAdvance() {
  ConfigurationTransaction transaction;
  assert(transaction.begin(13));
  assert(!transaction.prepared(14, true));
  assert(transaction.state() == State::AwaitingPrepare);
  assert(!transaction.finish(13, State::AwaitingCommit));
  assert(transaction.state() == State::AwaitingPrepare);
}

}  // namespace

int main() {
  testCommit();
  testRollback();
  testRejectedPrepare();
  testUnexpectedResultsDoNotAdvance();
  return 0;
}
