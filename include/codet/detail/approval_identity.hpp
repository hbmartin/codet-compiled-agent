#pragma once
#include "codet/core.hpp"
#include "codet/detail/diagnostic_stream.hpp"

namespace codet::detail {
inline bool approves(std::string_view answer) noexcept {
  return answer == "y" || answer == "yes";
}
struct ApprovalIdentity {
  std::string session, request;
  uint64_t turn;
  static bool valid(const Json &event) noexcept {
    if (!event.is_object())
      return false;
    const auto session = event.find("codet_session_id"), request = event.find("request_id"),
               turn = event.find("codet_turn");
    return session != event.end() && session->is_string() &&
           !session->get_ref<const std::string &>().empty() && request != event.end() &&
           request->is_string() && !request->get_ref<const std::string &>().empty() &&
           turn != event.end() &&
           (turn->is_number_unsigned() ||
            (turn->is_number_integer() && turn->get<int64_t>() > 0)) &&
           turn->get<uint64_t>() != 0;
  }
  static ApprovalIdentity from(const Json &event) {
    if (!valid(event))
      throw Error("Invalid approval request identity");
    return {event.at("codet_session_id").get<std::string>(),
            event.at("request_id").get<std::string>(), event.at("codet_turn").get<uint64_t>()};
  }
  bool matches(const Json &event) const noexcept {
    return matches(event, session, request, turn);
  }
  static bool matches(const Json &event, std::string_view session, std::string_view request,
                      uint64_t turn) noexcept {
    return valid(event) && event.at("codet_session_id").get_ref<const std::string &>() == session &&
           event.at("request_id").get_ref<const std::string &>() == request &&
           event.at("codet_turn").get<uint64_t>() == turn;
  }
};
} // namespace codet::detail
