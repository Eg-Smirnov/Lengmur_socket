#pragma once

#include <stdint.h>

class ConfigurationTransaction {
 public:
  enum class State : uint8_t {
    Idle,
    AwaitingPrepare,
    NeedCommit,
    AwaitingCommit,
    NeedRollback,
    AwaitingRollback,
  };

  /** Starts a transaction when no other configuration change is pending. */
  bool begin(uint32_t transaction_id) {
    if (state_ != State::Idle) {
      return false;
    }
    transaction_id_ = transaction_id;
    state_ = State::AwaitingPrepare;
    return true;
  }

  /** Records prepare completion and selects commit or rollback. */
  bool prepared(uint32_t transaction_id, bool storage_succeeded) {
    if (!matches(transaction_id, State::AwaitingPrepare)) {
      return false;
    }
    state_ = storage_succeeded ? State::NeedCommit : State::NeedRollback;
    return true;
  }

  /** Marks the currently required commit or rollback command as sent. */
  bool decisionSent() {
    if (state_ == State::NeedCommit) {
      state_ = State::AwaitingCommit;
      return true;
    }
    if (state_ == State::NeedRollback) {
      state_ = State::AwaitingRollback;
      return true;
    }
    return false;
  }

  /** Completes a transaction only from the expected waiting state. */
  bool finish(uint32_t transaction_id, State expected) {
    if (!matches(transaction_id, expected)) {
      return false;
    }
    state_ = State::Idle;
    transaction_id_ = 0;
    return true;
  }

  /** Completes a transaction rejected during its prepare phase. */
  bool reject(uint32_t transaction_id) {
    return finish(transaction_id, State::AwaitingPrepare);
  }

  /** Returns the current transaction state. */
  State state() const { return state_; }

  /** Returns the active transaction identifier or zero while idle. */
  uint32_t transactionId() const { return transaction_id_; }

  /** Reports whether a configuration transaction is in progress. */
  bool active() const { return state_ != State::Idle; }

 private:
  bool matches(uint32_t transaction_id, State expected) const {
    return state_ == expected && transaction_id_ == transaction_id;
  }

  State state_ = State::Idle;
  uint32_t transaction_id_ = 0;
};
