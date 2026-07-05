#include "course/matching_engine.hpp"

namespace course {

std::vector<Trade> MatchingEngine::submit(OrderId id, Side side, Price limit,
                                          Qty qty) {
    // TODO: the matching loop (README THEORY §1, Hints 1-3).
    (void)id;
    (void)side;
    (void)limit;
    (void)qty;
    return {};
}

bool MatchingEngine::cancel(OrderId id) {
    // TODO
    (void)id;
    return false;
}

bool MatchingEngine::reduce(OrderId id, Qty by) {
    // TODO
    (void)id;
    (void)by;
    return false;
}

}  // namespace course
