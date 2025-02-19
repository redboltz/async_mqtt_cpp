// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_PROTOCOL_PACKET_STORE_PACKET_VARIANT_FWD_HPP)
#define ASYNC_MQTT_PROTOCOL_PACKET_STORE_PACKET_VARIANT_FWD_HPP

#include <cstddef>

namespace async_mqtt {

/**
 * @brief MQTT packet variant for store
 * @tparam PacketIdBytes MQTT spec is 2. You can use `store_packet_variant` for that.
 *
 * #### variants
 * @li @ref v3_1_1::basic_publish_packet<PacketIdBytes>
 * @li @ref v3_1_1::basic_pubrel_packet<PacketIdBytes>
 * @li @ref v5::basic_publish_packet<PacketIdBytes>
 * @li @ref v5::basic_pubrel_packet<PacketIdBytes>
 *
 */
template <std::size_t PacketIdBytes>
class basic_store_packet_variant;

/**
 * @brief Type alias of basic_store_packet_variant (PacketIdBytes=2).
 *
 */
using store_packet_variant = basic_store_packet_variant<2>;

} // namespace async_mqtt

#endif // ASYNC_MQTT_PROTOCOL_PACKET_STORE_PACKET_VARIANT_FWD_HPP
