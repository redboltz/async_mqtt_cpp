// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_PACKET_IMPL_V3_1_1_SUBACK_HPP)
#define ASYNC_MQTT_PACKET_IMPL_V3_1_1_SUBACK_HPP

#include <utility>
#include <numeric>

#include <boost/numeric/conversion/cast.hpp>

#include <async_mqtt/packet/v3_1_1_suback.hpp>
#include <async_mqtt/exception.hpp>
#include <async_mqtt/util/buffer.hpp>

#include <async_mqtt/util/move.hpp>
#include <async_mqtt/util/static_vector.hpp>
#include <async_mqtt/util/endian_convert.hpp>

#include <async_mqtt/packet/fixed_header.hpp>
#include <async_mqtt/packet/topic_subopts.hpp>
#include <async_mqtt/packet/suback_return_code.hpp>
#include <async_mqtt/packet/packet_id_type.hpp>
#include <async_mqtt/packet/copy_to_static_vector.hpp>

namespace async_mqtt::v3_1_1 {

template <std::size_t PacketIdBytes>
inline
basic_suback_packet<PacketIdBytes>::basic_suback_packet(
    packet_id_t packet_id,
    std::vector<suback_return_code> params
)
    : fixed_header_{make_fixed_header(control_packet_type::suback, 0b0000)},
      entries_{force_move(params)},
      remaining_length_{PacketIdBytes + entries_.size()}
{
    endian_store(packet_id, packet_id_.data());

    remaining_length_buf_ = val_to_variable_bytes(boost::numeric_cast<std::uint32_t>(remaining_length_));
}

template <std::size_t PacketIdBytes>
inline
constexpr control_packet_type basic_suback_packet<PacketIdBytes>::type() {
    return control_packet_type::suback;
}

template <std::size_t PacketIdBytes>
inline
std::vector<as::const_buffer> basic_suback_packet<PacketIdBytes>::const_buffer_sequence() const {
    std::vector<as::const_buffer> ret;
    ret.reserve(num_of_const_buffer_sequence());

    ret.emplace_back(as::buffer(&fixed_header_, 1));

    ret.emplace_back(as::buffer(remaining_length_buf_.data(), remaining_length_buf_.size()));

    ret.emplace_back(as::buffer(packet_id_.data(), packet_id_.size()));

    ret.emplace_back(as::buffer(entries_.data(), entries_.size()));

    return ret;
}

template <std::size_t PacketIdBytes>
inline
std::size_t basic_suback_packet<PacketIdBytes>::size() const {
    return
        1 +                            // fixed header
        remaining_length_buf_.size() +
        remaining_length_;
}

template <std::size_t PacketIdBytes>
inline
constexpr std::size_t basic_suback_packet<PacketIdBytes>::num_of_const_buffer_sequence() {
    return
        1 +                   // fixed header
        1 +                   // remaining length
        1 +                   // packet id
        1;                    // suback_return_code vector
}

template <std::size_t PacketIdBytes>
inline
typename basic_suback_packet<PacketIdBytes>::packet_id_t basic_suback_packet<PacketIdBytes>::packet_id() const {
    return endian_load<packet_id_t>(packet_id_.data());
}

template <std::size_t PacketIdBytes>
inline
std::vector<suback_return_code> const& basic_suback_packet<PacketIdBytes>::entries() const {
    return entries_;
}

template <std::size_t PacketIdBytes>
inline
basic_suback_packet<PacketIdBytes>::basic_suback_packet(buffer buf) {
    // fixed_header
    if (buf.empty()) {
        throw make_error(
            errc::bad_message,
            "v3_1_1::suback_packet fixed_header doesn't exist"
        );
    }
    fixed_header_ = static_cast<std::uint8_t>(buf.front());
    buf.remove_prefix(1);
    auto cpt_opt = get_control_packet_type_with_check(static_cast<std::uint8_t>(fixed_header_));
    if (!cpt_opt || *cpt_opt != control_packet_type::suback) {
        throw make_error(
            errc::bad_message,
            "v3_1_1::suback_packet fixed_header is invalid"
        );
    }

    // remaining_length
    if (auto vl_opt = insert_advance_variable_length(buf, remaining_length_buf_)) {
        remaining_length_ = *vl_opt;
    }
    else {
        throw make_error(errc::bad_message, "v3_1_1::suback_packet remaining length is invalid");
    }
    if (remaining_length_ != buf.size()) {
        throw make_error(errc::bad_message, "v3_1_1::suback_packet remaining length doesn't match buf.size()");
    }

    // packet_id
    if (!copy_advance(buf, packet_id_)) {
        throw make_error(
            errc::bad_message,
            "v3_1_1::suback_packet packet_id doesn't exist"
        );
    }

    if (remaining_length_ == 0) {
        throw make_error(errc::bad_message, "v3_1_1::suback_packet doesn't have entries");
    }

    while (!buf.empty()) {
        // suback_return_code
        if (buf.empty()) {
            throw make_error(
                errc::bad_message,
                "v3_1_1::suback_packet suback_return_code  doesn't exist"
            );
        }
        auto rc = static_cast<suback_return_code>(buf.front());
        entries_.emplace_back(rc);
        buf.remove_prefix(1);
    }
}

template <std::size_t PacketIdBytes>
inline
std::ostream& operator<<(std::ostream& o, basic_suback_packet<PacketIdBytes> const& v) {
    o <<
        "v3_1_1::suback{" <<
        "pid:" << v.packet_id() << ",[";
    auto b = v.entries().cbegin();
    auto e = v.entries().cend();
    if (b != e) {
        o << *b++;
    }
    for (; b != e; ++b) {
        o << "," << *b;
    }
    o << "]}";
    return o;
}


} // namespace async_mqtt::v3_1_1

#endif // ASYNC_MQTT_PACKET_IMPL_V3_1_1_SUBACK_HPP