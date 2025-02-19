// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_PROTOCOL_PACKET_PACKET_VARIANT_FWD_HPP)
#define ASYNC_MQTT_PROTOCOL_PACKET_PACKET_VARIANT_FWD_HPP

#include <cstddef>

namespace async_mqtt {

/**
 * @brief The varaint type of all packets and system_error
 *
 * #### Thread Safety
 * @li Distinct objects: Safe
 * @li Shared objects: Unsafe
 *
 * #### variants
 * @li @ref v3_1_1::connect_packet
 * @li @ref v3_1_1::connack_packet
 * @li @ref v3_1_1::basic_publish_packet<PacketIdBytes>
 * @li @ref v3_1_1::basic_puback_packet<PacketIdBytes>
 * @li @ref v3_1_1::basic_pubrec_packet<PacketIdBytes>
 * @li @ref v3_1_1::basic_pubrel_packet<PacketIdBytes>
 * @li @ref v3_1_1::basic_pubcomp_packet<PacketIdBytes>
 * @li @ref v3_1_1::basic_subscribe_packet<PacketIdBytes>
 * @li @ref v3_1_1::basic_suback_packet<PacketIdBytes>
 * @li @ref v3_1_1::basic_unsubscribe_packet<PacketIdBytes>
 * @li @ref v3_1_1::basic_unsuback_packet<PacketIdBytes>
 * @li @ref v3_1_1::pingreq_packet
 * @li @ref v3_1_1::pingresp_packet
 * @li @ref v3_1_1::disconnect_packet
 * @li @ref v5::connect_packet
 * @li @ref v5::connack_packet
 * @li @ref v5::basic_publish_packet<PacketIdBytes>
 * @li @ref v5::basic_puback_packet<PacketIdBytes>
 * @li @ref v5::basic_pubrec_packet<PacketIdBytes>
 * @li @ref v5::basic_pubrel_packet<PacketIdBytes>
 * @li @ref v5::basic_pubcomp_packet<PacketIdBytes>
 * @li @ref v5::basic_subscribe_packet<PacketIdBytes>
 * @li @ref v5::basic_suback_packet<PacketIdBytes>
 * @li @ref v5::basic_unsubscribe_packet<PacketIdBytes>
 * @li @ref v5::basic_unsuback_packet<PacketIdBytes>
 * @li @ref v5::pingreq_packet
 * @li @ref v5::pingresp_packet
 * @li @ref v5::disconnect_packet
 * @li @ref v5::auth_packet
 *
 */
template <std::size_t PacketIdBytes>
class basic_packet_variant;

/**
 * @related basic_packet_variant
 * @brief type alias of basic_packet_variant (PacketIdBytes=2).
 *
 */
using packet_variant = basic_packet_variant<2>;

} // namespace async_mqtt

#endif // ASYNC_MQTT_PROTOCOL_PACKET_PACKET_VARIANT_FWD_HPP
