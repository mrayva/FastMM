#pragma once
// RefBook: an independent reference limit order book for differential testing.
//
// Deliberately simple and slow: std::map<price, deque<order>> per side, written from the rules
// below and sharing no code with any engine. Drive it and the engine under test with the same event
// stream and compare, after every event, the fill sequence (aggressor, passive, price, quantity),
// the best bid and ask, that the book is not crossed, and the QtyLedger identity.
//
// Rules (price-time priority, as most venues):
//   * a fill prints at the RESTING order's price;
//   * Limit/Gtc rests its remainder; Limit/Ioc and Market expire it; Limit/Fok fills entirely at
//     its limit or better, or not at all (nothing partial: rejected whole);
//   * Market ignores price and never rests;
//   * cancel removes an order; modify(id, new_qty): a decrease keeps queue position, an increase
//     moves the order to the back of its level (it loses time priority); new_qty == 0 is a cancel;
//   * an order id may be reused once the order is gone; a duplicate live id is rejected.
//
// Not modelled: stops, icebergs, hidden quantity, auctions, self-trade prevention, price bands.
#include "hftkit/check/conservation.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>

namespace hftkit {

enum class RefSide : std::uint8_t { Buy, Sell };
enum class RefTif : std::uint8_t { Gtc, Ioc, Fok };
enum class RefKind : std::uint8_t { Limit, Market };

struct RefOrder {
  std::uint64_t id = 0;
  RefSide side = RefSide::Buy;
  RefKind kind = RefKind::Limit;
  RefTif tif = RefTif::Gtc;
  std::int64_t price = 0;
  std::int64_t qty = 0;
};

struct RefFill {
  std::uint64_t aggressor = 0;
  std::uint64_t passive = 0;
  std::int64_t price = 0;
  std::int64_t qty = 0;
  friend bool operator==(const RefFill&, const RefFill&) = default;
};

class RefBook {
 public:
  // Returns the fills the order caused, in the order they happen.
  std::vector<RefFill> submit(const RefOrder& in) {
    std::vector<RefFill> fills;
    if (in.qty <= 0 || index_.contains(in.id)) {
      ledger_.submitted += in.qty > 0 ? in.qty : 0;
      ledger_.rejected += in.qty > 0 ? in.qty : 0;
      return fills;
    }
    ledger_.submitted += in.qty;
    const bool market = in.kind == RefKind::Market;
    if (in.tif == RefTif::Fok && fillable(in) < in.qty) {
      ledger_.rejected += in.qty;
      return fills;
    }
    std::int64_t left = in.qty;
    if (in.side == RefSide::Buy) {
      left = match(asks_, in, market, left, fills, [](std::int64_t lvl, std::int64_t lim) {
        return lvl <= lim;
      });
    } else {
      left = match(bids_, in, market, left, fills, [](std::int64_t lvl, std::int64_t lim) {
        return lvl >= lim;
      });
    }
    if (left > 0) {
      if (!market && in.tif == RefTif::Gtc) {
        rest(in, left);
      } else {
        ledger_.expired += left;
      }
    }
    return fills;
  }

  bool cancel(std::uint64_t id) {
    const auto it = index_.find(id);
    if (it == index_.end()) return false;
    const Loc loc = it->second;
    auto& m = loc.side == RefSide::Buy ? bids_ : asks_;
    const auto lvl = m.find(loc.price);
    Level& q = lvl->second;
    const auto pos = std::find_if(q.begin(), q.end(), [&](const Rest& r) { return r.id == id; });
    const std::int64_t gone = pos->qty;
    q.erase(pos);
    if (q.empty()) m.erase(lvl);
    ledger_.cancelled += gone;
    ledger_.resting -= gone;
    index_.erase(it);
    return true;
  }

  // See the class comment. Returns false for an unknown id.
  bool modify(std::uint64_t id, std::int64_t new_qty) {
    const auto it = index_.find(id);
    if (it == index_.end()) return false;
    if (new_qty <= 0) return cancel(id);
    const Loc loc = it->second;
    auto& q = (loc.side == RefSide::Buy ? bids_ : asks_).at(loc.price);
    const auto pos = std::find_if(q.begin(), q.end(), [&](const Rest& r) { return r.id == id; });
    const std::int64_t old = pos->qty;
    if (new_qty < old) {
      pos->qty = new_qty;
      ledger_.cancelled += old - new_qty;
      ledger_.resting -= old - new_qty;
    } else if (new_qty > old) {
      Rest moved = *pos;
      moved.qty = new_qty;
      q.erase(pos);
      q.push_back(moved);
      ledger_.submitted += new_qty - old;
      ledger_.resting += new_qty - old;
    }
    return true;
  }

  // 0 when the side is empty.
  [[nodiscard]] std::int64_t best_bid() const { return bids_.empty() ? 0 : bids_.rbegin()->first; }
  [[nodiscard]] std::int64_t best_ask() const { return asks_.empty() ? 0 : asks_.begin()->first; }
  [[nodiscard]] bool crossed() const {
    return !bids_.empty() && !asks_.empty() && best_bid() >= best_ask();
  }
  [[nodiscard]] std::size_t live_orders() const { return index_.size(); }
  [[nodiscard]] const QtyLedger& ledger() const { return ledger_; }

  // Total quantity resting at `price` on `side`.
  [[nodiscard]] std::int64_t depth_at(RefSide side, std::int64_t price) const {
    const auto& m = side == RefSide::Buy ? bids_ : asks_;
    const auto it = m.find(price);
    if (it == m.end()) return 0;
    std::int64_t t = 0;
    for (const Rest& r : it->second) t += r.qty;
    return t;
  }

 private:
  struct Rest {
    std::uint64_t id;
    std::int64_t qty;
  };
  struct Loc {
    RefSide side;
    std::int64_t price;
  };
  using Level = std::deque<Rest>;

  // Quantity the order could fill right now at its limit (market: any price).
  [[nodiscard]] std::int64_t fillable(const RefOrder& in) const {
    const bool market = in.kind == RefKind::Market;
    std::int64_t total = 0;
    if (in.side == RefSide::Buy) {
      for (const auto& [px, q] : asks_) {
        if (!market && px > in.price) break;
        for (const Rest& r : q) total += r.qty;
        if (total >= in.qty) break;
      }
    } else {
      for (auto it = bids_.rbegin(); it != bids_.rend(); ++it) {
        if (!market && it->first < in.price) break;
        for (const Rest& r : it->second) total += r.qty;
        if (total >= in.qty) break;
      }
    }
    return total;
  }

  template <class Map, class Crosses>
  std::int64_t match(Map& book,
                     const RefOrder& in,
                     bool market,
                     std::int64_t left,
                     std::vector<RefFill>& fills,
                     Crosses crosses) {
    while (left > 0 && !book.empty()) {
      auto lvl = in.side == RefSide::Buy ? book.begin() : std::prev(book.end());
      if (!market && !crosses(lvl->first, in.price)) break;
      Level& q = lvl->second;
      while (left > 0 && !q.empty()) {
        Rest& r = q.front();
        const std::int64_t t = std::min(left, r.qty);
        fills.push_back(RefFill{in.id, r.id, lvl->first, t});
        ledger_.filled += t;
        ledger_.resting -= t;
        left -= t;
        r.qty -= t;
        if (r.qty == 0) {
          index_.erase(r.id);
          q.pop_front();
        }
      }
      if (q.empty()) book.erase(lvl);
    }
    return left;
  }

  void rest(const RefOrder& in, std::int64_t qty) {
    (in.side == RefSide::Buy ? bids_ : asks_)[in.price].push_back(Rest{in.id, qty});
    index_[in.id] = Loc{in.side, in.price};
    ledger_.resting += qty;
  }

  std::map<std::int64_t, Level> bids_;
  std::map<std::int64_t, Level> asks_;
  std::unordered_map<std::uint64_t, Loc> index_;
  QtyLedger ledger_;
};

}  // namespace hftkit
