#include "warehouse/domain.hpp"

#include <stdexcept>

namespace warehouse {
namespace {

bool is_blank(char value) {
    return value == ' ' || value == '\t' || value == '\n' ||
           value == '\r' || value == '\f' || value == '\v';
}

bool valid_identifier(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    return !is_blank(value.front()) && !is_blank(value.back());
}

bool valid_sku(const std::string& value) {
    return valid_identifier(value);
}

}  // namespace

void Inventory::add_station(const std::string& station_id, int stock) {
    if (!valid_identifier(station_id)) {
        throw std::invalid_argument("add_station: station id must be non-empty "
                                    "without surrounding whitespace");
    }
    if (stock < 0) {
        throw std::invalid_argument("add_station: stock must be >= 0");
    }
    if (stations_.find(station_id) != stations_.end()) {
        throw std::invalid_argument("add_station: duplicate station id");
    }
    stations_.emplace(station_id,
                      PickStation{station_id, stock, RobotState::idle,
                                  std::string()});
}

std::optional<PickStation> Inventory::station(
    const std::string& station_id) const {
    const auto found = stations_.find(station_id);
    if (found == stations_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::vector<PickStation> Inventory::stations() const {
    std::vector<PickStation> result;
    result.reserve(stations_.size());
    for (const auto& entry : stations_) {
        result.push_back(entry.second);
    }
    return result;
}

std::vector<PickStation> Inventory::available_for(const Order& order) const {
    std::vector<PickStation> result;
    if (order.quantity <= 0) {
        return result;
    }
    // stations_ is a std::map keyed by station id, so iteration order is
    // already ascending lexicographical order as required by the spec.
    for (const auto& entry : stations_) {
        const PickStation& station = entry.second;
        if (station.robot_state == RobotState::idle &&
            station.stock >= order.quantity) {
            result.push_back(station);
        }
    }
    return result;
}

bool Inventory::reserve(const Order& order, const std::string& station_id) {
    if (!valid_identifier(order.id) || !valid_sku(order.sku)) {
        return false;
    }
    if (order.quantity <= 0) {
        return false;
    }
    if (!valid_identifier(station_id)) {
        return false;
    }
    const auto found = stations_.find(station_id);
    if (found == stations_.end()) {
        return false;
    }
    if (found->second.robot_state != RobotState::idle) {
        return false;
    }
    if (found->second.stock < order.quantity) {
        return false;
    }
    // All checks passed: apply the atomic transition in one step.
    found->second.stock -= order.quantity;
    found->second.robot_state = RobotState::busy;
    found->second.current_order_id = order.id;
    return true;
}

bool Inventory::finish(const std::string& station_id,
                       const std::string& order_id) {
    if (!valid_identifier(station_id) || !valid_identifier(order_id)) {
        return false;
    }
    const auto found = stations_.find(station_id);
    if (found == stations_.end()) {
        return false;
    }
    if (found->second.robot_state != RobotState::busy) {
        return false;
    }
    if (found->second.current_order_id != order_id) {
        return false;
    }
    // Picked stock is never restored; only the robot is released.
    found->second.robot_state = RobotState::idle;
    found->second.current_order_id.clear();
    return true;
}

}  // namespace warehouse
