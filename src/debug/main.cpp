#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace debug {

struct Station {
    int stock;
    bool busy;
    std::string order_id;
};

struct Decision {
    bool accepted;
    std::string station_id;
    std::string reason;
};

class LegacyDispatcher {
public:
    explicit LegacyDispatcher(std::map<std::string, int> stock_by_station)
        : stock_(stock_by_station) {
        for (const auto& item : stock_) {
            busy_[item.first] = false;
            orders_[item.first] = "";
        }
    }

    Decision submit(const std::string& order_id, int quantity) {
        if (quantity <= 0) {
            return {false, "", "invalid quantity"};
        }
        if (order_id.empty()) {
            return {false, "", "invalid order id"};
        }
        for (auto& item : stock_) {
            const std::string& station_id = item.first;
            if (busy_[station_id]) {
                continue;
            }
            // Validate before mutating: check the remaining stock without
            // writing it first, so a rejected order leaves no side effects.
            if (item.second < quantity) {
                return {false, "", "insufficient stock"};
            }
            item.second -= quantity;
            busy_[station_id] = true;
            orders_[station_id] = order_id;
            return {true, station_id, "accepted"};
        }
        return {false, "", "no available station"};
    }

    bool complete(const std::string& station_id) {
        if (!busy_.count(station_id) || !busy_[station_id]) {
            return false;
        }
        busy_[station_id] = false;
        orders_[station_id].clear();
        return true;
    }

    int stock(const std::string& station_id) const {
        return stock_.at(station_id);
    }

    bool busy(const std::string& station_id) const {
        return busy_.at(station_id);
    }

private:
    std::map<std::string, int> stock_;
    std::map<std::string, bool> busy_;
    std::map<std::string, std::string> orders_;
};

}  // namespace debug

int main() {
    debug::LegacyDispatcher dispatcher({{"S1", 3}, {"S2", 5}});
    const auto first = dispatcher.submit("O1", 2);
    const auto rejected = dispatcher.submit("O2", 6);
    dispatcher.complete(first.station_id);
    const auto after_failure = dispatcher.submit("O3", 1);

    std::cout << first.station_id << " "
              << (rejected.accepted ? "yes" : "no") << " "
              << dispatcher.stock("S2") << " "
              << (after_failure.accepted ? "yes" : "no") << " "
              << dispatcher.stock("S1") << '\n';
    return 0;
}
