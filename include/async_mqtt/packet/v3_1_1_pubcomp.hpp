// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_PACKET_V3_1_1_PUBCOMP_HPP)
#define ASYNC_MQTT_PACKET_V3_1_1_PUBCOMP_HPP

#include <utility>
#include <numeric>

#include <boost/numeric/conversion/cast.hpp>

#include <async_mqtt/buffer_to_packet_variant_fwd.hpp>
#include <async_mqtt/exception.hpp>
#include <async_mqtt/buffer.hpp>

#include <async_mqtt/util/move.hpp>
#include <async_mqtt/util/static_vector.hpp>
#include <async_mqtt/util/endian_convert.hpp>

#include <async_mqtt/packet/packet_id_type.hpp>
#include <async_mqtt/packet/fixed_header.hpp>

/**
 * @defgroup pubcomp_v3_1_1
 * @ingroup packet_v3_1_1
 */

/**
 * @defgroup pubcomp_v3_1_1_detail
 * @ingroup pubcomp_v3_1_1
 * @brief packet internal detailes (e.g. type-aliased API's actual type information)
 */

namespace async_mqtt::v3_1_1 {

namespace as = boost::asio;

/**
 * @ingroup pubcomp_v3_1_1_detail
 * @brief MQTT PUBCOMP packet (v3.1.1)
 * @tparam PacketIdBytes size of packet_id
 *
 * If basic_endpoint::set_auto_pub_response() is called with true, then this packet is
 * automatically sent when PUBREL v3_1_1::basic_pubrel_packet is received.
 *
 * When the packet is received, the packet_id is automatically released and become reusable.
 * \n See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718058
 */
template <std::size_t PacketIdBytes>
class basic_pubcomp_packet {
public:
    using packet_id_t = typename packet_id_type<PacketIdBytes>::type;

    /**
     * @brief constructor
     * @param packet_id MQTT PacketIdentifier that is corresponding to the PUBREL packet
     */
    basic_pubcomp_packet(
        packet_id_t packet_id
    )
        : all_(all_.capacity())
    {
        all_[0] = static_cast<char>(make_fixed_header(control_packet_type::pubcomp, 0b0000));
        all_[1] = boost::numeric_cast<char>(PacketIdBytes);
        endian_store(packet_id, &all_[2]);
    }

    /**
     * @brief Get MQTT control packet type
     * @return control packet type
     */
    constexpr control_packet_type type() const {
        return control_packet_type::pubcomp;
    }

    /**
     * @brief Create const buffer sequence
     *        it is for boost asio APIs
     * @return const buffer sequence
     */
    std::vector<as::const_buffer> const_buffer_sequence() const {
        std::vector<as::const_buffer> ret;

        ret.emplace_back(as::buffer(all_.data(), all_.size()));
        return ret;
    }

    /**
     * @brief Get packet size.
     * @return packet size
     */
    std::size_t size() const {
        return all_.size();
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    static constexpr std::size_t num_of_const_buffer_sequence() {
        return 1; // all
    }

    /**
     * @brief Get packet_id.
     * @return packet_id
     */
    packet_id_t packet_id() const {
        return endian_load<packet_id_t>(&all_[2]);
    }

private:

    template <std::size_t PacketIdBytesArg>
    friend basic_packet_variant<PacketIdBytesArg>
    async_mqtt::buffer_to_basic_packet_variant(buffer buf, protocol_version ver);

#if defined(ASYNC_MQTT_UNIT_TEST_FOR_PACKET)
    friend struct ::ut_packet::v311_pubcomp;
    friend struct ::ut_packet::v311_pubcomp_pid4;
#endif // defined(ASYNC_MQTT_UNIT_TEST_FOR_PACKET)

    // private constructor for internal use
    basic_pubcomp_packet(buffer buf) {
        // fixed_header
        if (buf.empty()) {
            throw make_error(
                errc::bad_message,
                "v3_1_1::pubcomp_packet fixed_header doesn't exist"
            );
        }
        all_.push_back(buf.front());
        buf.remove_prefix(1);
        auto cpt_opt = get_control_packet_type_with_check(static_cast<std::uint8_t>(all_.back()));
        if (!cpt_opt || *cpt_opt != control_packet_type::pubcomp) {
            throw make_error(
                errc::bad_message,
                "v3_1_1::pubcomp_packet fixed_header is invalid"
            );
        }

        // remaining_length
        if (buf.empty()) {
            throw make_error(
                errc::bad_message,
                "v3_1_1::pubcomp_packet remaining_length doesn't exist"
            );
        }
        all_.push_back(buf.front());
        buf.remove_prefix(1);
        if (static_cast<std::uint8_t>(all_.back()) != PacketIdBytes) {
            throw make_error(
                errc::bad_message,
                "v3_1_1::pubcomp_packet remaining_length is invalid"
            );
        }

        // variable header
        if (buf.size() != PacketIdBytes) {
            throw make_error(
                errc::bad_message,
                "v3_1_1::pubcomp_packet variable header doesn't match its length"
            );
        }
        std::copy(buf.begin(), buf.end(), std::back_inserter(all_));
    }

private:
    boost::container::static_vector<char, 2 + PacketIdBytes> all_;
};

/**
 * @related basic_pubcomp_packet
 * @brief stream output operator
 * @param o output stream
 * @param v target
 * @return  output stream
 */
template <std::size_t PacketIdBytes>
inline std::ostream& operator<<(std::ostream& o, basic_pubcomp_packet<PacketIdBytes> const& v) {
    o <<
        "v3_1_1::pubcomp{" <<
        "pid:" << v.packet_id() <<
        "}";
    return o;
}

/**
 * @ingroup pubcomp_v3_1_1
 * @related basic_pubcomp_packet
 * @brief Type alias of basic_pubcomp_packet (PacketIdBytes=2).
 */
using pubcomp_packet = basic_pubcomp_packet<2>;

} // namespace async_mqtt::v3_1_1

#endif // ASYNC_MQTT_PACKET_V3_1_1_PUBCOMP_HPP
