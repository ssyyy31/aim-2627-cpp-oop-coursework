#include "dispatch/scheduler.hpp"

#include <stdexcept>
#include <utility>

namespace dispatch {
namespace {

bool is_blank(char value) {
    return value == ' ' || value == '\t' || value == '\n' ||
           value == '\r' || value == '\f' || value == '\v';
}

bool clean_text(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    return !is_blank(value.front()) && !is_blank(value.back());
}

DispatchDecision rejected(const warehouse::Order& order,
                          const std::string& reason) {
    return DispatchDecision{false, order.id, std::string(), reason};
}

}  // namespace

Scheduler::Scheduler(warehouse::Inventory& inventory,
                     std::unique_ptr<DispatchPolicy> policy,
                     audit::Trace& trace)
    : inventory_(inventory), policy_(std::move(policy)), trace_(trace) {
    if (!policy_) {
        throw std::invalid_argument("dispatch policy cannot be null");
    }
}

DispatchDecision Scheduler::submit(const warehouse::Order& order) {
    // Validate before touching any state: a failed submission must be
    // completely side-effect free.
    if (!clean_text(order.id)) {
        return rejected(order, "invalid order id");
    }
    if (!clean_text(order.sku)) {
        return rejected(order, "invalid sku");
    }
    if (order.quantity <= 0) {
        return rejected(order, "invalid quantity");
    }

    const std::vector<warehouse::PickStation> candidates =
        inventory_.available_for(order);
    if (candidates.empty()) {
        return rejected(order, "no eligible station");
    }

    const std::optional<std::string> selected = policy_->select(order, candidates);
    if (!selected.has_value() || !clean_text(*selected)) {
        return rejected(order, "policy returned no station");
    }

    if (!inventory_.reserve(order, *selected)) {
        return rejected(order, "reserve rejected by inventory");
    }

    trace_.append("DISPATCH " + order.id + " " + *selected);
    return DispatchDecision{true, order.id, *selected, "accepted"};
}

std::vector<DispatchDecision> Scheduler::submit_all(
    const std::vector<warehouse::Order>& orders) {
    std::vector<DispatchDecision> decisions;
    decisions.reserve(orders.size());
    for (const warehouse::Order& order : orders) {
        decisions.push_back(submit(order));
    }
    return decisions;
}

bool Scheduler::complete(const std::string& station_id,
                         const std::string& order_id,
                         std::string& failure_reason) {
    failure_reason.clear();
    if (!clean_text(station_id)) {
        failure_reason = "invalid station id";
        return false;
    }
    if (!clean_text(order_id)) {
        failure_reason = "invalid order id";
        return false;
    }
    const std::optional<warehouse::PickStation> station =
        inventory_.station(station_id);
    if (!station.has_value()) {
        failure_reason = "unknown station";
        return false;
    }
    if (station->robot_state != warehouse::RobotState::busy) {
        failure_reason = "robot not busy";
        return false;
    }
    // station_id and order_id must both match exactly; neither is a hint
    // that can fall back to the other.
    if (station->current_order_id != order_id) {
        failure_reason = "order not held by station";
        return false;
    }
    if (!inventory_.finish(station_id, order_id)) {
        failure_reason = "inventory rejected completion";
        return false;
    }
    trace_.append("COMPLETE " + order_id + " " + station_id);
    return true;
}

const warehouse::Inventory& Scheduler::inventory() const {
    return inventory_;
}

const audit::Trace& Scheduler::trace() const {
    return trace_;
}

}  // namespace dispatch
