#include "dispatch/policy.hpp"

#include <cstddef>
#include <stdexcept>

namespace dispatch {
namespace {

class PriorityPolicy final : public DispatchPolicy {
public:
    std::string name() const override;
    std::optional<std::string> select(
        const warehouse::Order& order,
        const std::vector<warehouse::PickStation>& candidates) const override;
};

class StockPolicy final : public DispatchPolicy {
public:
    std::string name() const override;
    std::optional<std::string> select(
        const warehouse::Order& order,
        const std::vector<warehouse::PickStation>& candidates) const override;
};

}  // namespace

std::unique_ptr<DispatchPolicy> make_policy(const std::string& name) {
    if (name == "priority") {
        return std::unique_ptr<DispatchPolicy>(new PriorityPolicy());
    }
    if (name == "stock") {
        return std::unique_ptr<DispatchPolicy>(new StockPolicy());
    }
    throw std::invalid_argument("make_policy: unknown dispatch policy");
}

std::string PriorityPolicy::name() const { return "priority"; }

std::optional<std::string> PriorityPolicy::select(
    const warehouse::Order& order,
    const std::vector<warehouse::PickStation>& candidates) const {
    (void)order;
    if (candidates.empty()) {
        return std::nullopt;
    }
    // Candidates arrive sorted by station id, so the first one is the
    // lexicographically smallest eligible station.
    return candidates.front().id;
}

std::string StockPolicy::name() const { return "stock"; }

std::optional<std::string> StockPolicy::select(
    const warehouse::Order& order,
    const std::vector<warehouse::PickStation>& candidates) const {
    (void)order;
    if (candidates.empty()) {
        return std::nullopt;
    }
    const warehouse::PickStation* best = &candidates.front();
    for (std::size_t index = 1; index < candidates.size(); ++index) {
        const warehouse::PickStation& current = candidates[index];
        // Strictly greater keeps the earlier station on ties, and the
        // candidate list is already ordered by station id ascending.
        if (current.stock > best->stock) {
            best = &current;
        }
    }
    return best->id;
}

}  // namespace dispatch
