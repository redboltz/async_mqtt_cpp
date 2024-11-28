// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_PROTOCOL_DETAIL_CONNECTION_IMPL_FWD_HPP)
#define ASYNC_MQTT_PROTOCOL_DETAIL_CONNECTION_IMPL_FWD_HPP

#include <cstdlib> // for std::size_t
#include <async_mqtt/protocol/role.hpp>

namespace async_mqtt::detail {

template <role Role, std::size_t PacketIdBytes>
class basic_connection_impl;

} // namespace async_mqtt::detail

#endif // ASYNC_MQTT_PROTOCOL_DETAIL_CONNECTION_IMPL_FWD_HPP