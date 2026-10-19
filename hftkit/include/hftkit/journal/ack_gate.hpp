#pragma once
// AckGate: releases acknowledgements only once their record is durable.
//
// Two watermarks per log, as in a write-ahead pipeline:
//   journaled  the highest sequence handed to the log writer; gates the engine ("may apply").
//   durable    the highest sequence an fsync has covered; gates acks ("may tell the client").
// The fsync policy changes latency and what a power failure can lose, never the log's content.
// The invariant is durable <= journaled: on_durable() refuses a durable mark above the journaled
// one rather than letting an ack escape for a record nobody wrote.
//
// Acks are queued in sequence order (push() refuses a non-increasing sequence, or a full queue),
// and release() hands over, in order, every ack whose sequence is <= durable. Fixed capacity,
// no allocation after construction.
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace hftkit {

template <std::size_t Capacity>
class AckGate {
  static_assert(std::has_single_bit(Capacity) && Capacity >= 2, "Capacity: power of two >= 2");

 public:
  struct Ack {
    std::uint64_t seq;
    std::uint64_t tag;  // caller's handle: client id, order id, ...
  };

  AckGate() : ring_(std::make_unique<Ack[]>(Capacity)) {}

  void on_journaled(std::uint64_t seq) noexcept {
    if (seq > journaled_) journaled_ = seq;
  }

  // False (and no change) if seq is above the journaled watermark. Lower values are ignored.
  bool on_durable(std::uint64_t seq) noexcept {
    if (seq > journaled_) return false;
    if (seq > durable_) durable_ = seq;
    return true;
  }

  bool push(std::uint64_t seq, std::uint64_t tag) noexcept {
    if (count_ == Capacity || seq <= last_pushed_) return false;
    ring_[(head_ + count_) & (Capacity - 1)] = Ack{seq, tag};
    ++count_;
    last_pushed_ = seq;
    return true;
  }

  // f(const Ack&) for every queued ack with seq <= durable, oldest first. Returns how many.
  template <class F>
  std::size_t release(F&& f) {
    std::size_t n = 0;
    while (count_ != 0 && ring_[head_].seq <= durable_) {
      f(ring_[head_]);
      head_ = (head_ + 1) & (Capacity - 1);
      --count_;
      ++n;
    }
    return n;
  }

  [[nodiscard]] std::uint64_t journaled() const noexcept { return journaled_; }
  [[nodiscard]] std::uint64_t durable() const noexcept { return durable_; }
  [[nodiscard]] std::size_t pending() const noexcept { return count_; }

 private:
  std::unique_ptr<Ack[]> ring_;
  std::size_t head_ = 0;
  std::size_t count_ = 0;
  std::uint64_t journaled_ = 0;
  std::uint64_t durable_ = 0;
  std::uint64_t last_pushed_ = 0;
};

}  // namespace hftkit
