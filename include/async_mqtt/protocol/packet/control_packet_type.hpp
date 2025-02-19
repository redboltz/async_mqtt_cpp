// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_PROTOCOL_PACKET_CONTROL_PACKET_TYPE_HPP)
#define ASYNC_MQTT_PROTOCOL_PACKET_CONTROL_PACKET_TYPE_HPP

#include <cstdint>
#include <ostream>

#include <optional>

namespace async_mqtt {

/**
 * @brief MQTT control packet type
 *
 */
enum class control_packet_type : std::uint8_t {

    // reserved    =  0b0000000,
    connect     = 0b00010000, ///< CONNECT(1)
    connack     = 0b00100000, ///< CONNACK(2)
    publish     = 0b00110000, ///< PUBLISH(3)
    puback      = 0b01000000, ///< PUBACK(4)
    pubrec      = 0b01010000, ///< PUBREC(5)
    pubrel      = 0b01100000, ///< PUBREL(6)
    pubcomp     = 0b01110000, ///< PUBCOMP(7)
    subscribe   = 0b10000000, ///< SUBSCRIBE(8)
    suback      = 0b10010000, ///< SUBACK(9)
    unsubscribe = 0b10100000, ///< UNSUBSCRIBE(10)
    unsuback    = 0b10110000, ///< UNSUBACK(11)
    pingreq     = 0b11000000, ///< PINGREQ(12)
    pingresp    = 0b11010000, ///< PINGRESP(13)
    disconnect  = 0b11100000, ///< DISCONNECT(14)
    auth        = 0b11110000, ///< AUTH(15)

};

/**
 * @brief stringize control_packet_type
 * @param v target
 * @return control_packet_type
 *
 */
constexpr
char const* control_packet_type_to_str(control_packet_type v) {
    switch(v)
    {
    // case control_packet_type::reserved:    return "reserved";
    case control_packet_type::connect:      return "connect";
    case control_packet_type::connack:      return "connack";
    case control_packet_type::publish:      return "publish";
    case control_packet_type::puback:       return "puback";
    case control_packet_type::pubrec:       return "pubrec";
    case control_packet_type::pubrel:       return "pubrel";
    case control_packet_type::pubcomp:      return "pubcomp";
    case control_packet_type::subscribe:    return "subscribe";
    case control_packet_type::suback:       return "suback";
    case control_packet_type::unsubscribe:  return "unsubscribe";
    case control_packet_type::unsuback:     return "unsuback";
    case control_packet_type::pingreq:      return "pingreq";
    case control_packet_type::pingresp:     return "pingresp";
    case control_packet_type::disconnect:   return "disconnect";
    case control_packet_type::auth:         return "auth";
    default:                                return "unknown_control_packet_type";
    }
}

/**
 * @brief output to the stream
 * @param o output stream
 * @param v  target
 * @return output stream
 *
 */
inline
std::ostream& operator<<(std::ostream& o, control_packet_type v)
{
    o << control_packet_type_to_str(v);
    return o;
}

constexpr control_packet_type get_control_packet_type(std::uint8_t v) {
    return static_cast<control_packet_type>(v & 0b11110000);
}

enum class control_packet_reserved_bits : std::uint8_t {
    connect     = 0b00000000,
    connack     = 0b00000000,
 // publish     = dup qos retain,
    puback      = 0b00000000,
    pubrec      = 0b00000000,
    pubrel      = 0b00000010,
    pubcomp     = 0b00000000,
    subscribe   = 0b00000010,
    suback      = 0b00000000,
    unsubscribe = 0b00000010,
    unsuback    = 0b00000000,
    pingreq     = 0b00000000,
    pingresp    = 0b00000000,
    disconnect  = 0b00000000,
    auth        = 0b00000000,
};

inline std::optional<control_packet_type> get_control_packet_type_with_check(std::uint8_t v) {
    auto cpt = static_cast<control_packet_type>(v & 0b11110000);
    auto valid =
        [&] {
            auto rsv = static_cast<control_packet_reserved_bits>(v & 0b00001111);
            switch (cpt) {
            case control_packet_type::connect:
                return rsv == control_packet_reserved_bits::connect;
            case control_packet_type::connack:
                return rsv == control_packet_reserved_bits::connack;
            case control_packet_type::publish:
                return true;
            case control_packet_type::puback:
                return rsv == control_packet_reserved_bits::puback;
            case control_packet_type::pubrec:
                return rsv == control_packet_reserved_bits::pubrec;
            case control_packet_type::pubrel:
                return rsv == control_packet_reserved_bits::pubrel;
            case control_packet_type::pubcomp:
                return rsv == control_packet_reserved_bits::pubcomp;
            case control_packet_type::subscribe:
                return rsv == control_packet_reserved_bits::subscribe;
            case control_packet_type::suback:
                return rsv == control_packet_reserved_bits::suback;
            case control_packet_type::unsubscribe:
                return rsv == control_packet_reserved_bits::unsubscribe;
            case control_packet_type::unsuback:
                return rsv == control_packet_reserved_bits::unsuback;
            case control_packet_type::pingreq:
                return rsv == control_packet_reserved_bits::pingreq;
            case control_packet_type::pingresp:
                return rsv == control_packet_reserved_bits::pingresp;
            case control_packet_type::disconnect:
                return rsv == control_packet_reserved_bits::disconnect;
            case control_packet_type::auth:
                return rsv == control_packet_reserved_bits::auth;
            default:
                return false;
            }
        } ();
    if (valid) return cpt;
    return std::nullopt;
}

} // namespace async_mqtt

#endif // ASYNC_MQTT_PROTOCOL_PACKET_CONTROL_PACKET_TYPE_HPP
