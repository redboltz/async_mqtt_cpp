// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_PACKET_V5_PUBLISH_HPP)
#define ASYNC_MQTT_PACKET_V5_PUBLISH_HPP

#include <utility>
#include <numeric>

#include <boost/numeric/conversion/cast.hpp>

#include <async_mqtt/exception.hpp>
#include <async_mqtt/buffer.hpp>
#include <async_mqtt/variable_bytes.hpp>

#include <async_mqtt/util/move.hpp>
#include <async_mqtt/util/static_vector.hpp>
#include <async_mqtt/util/endian_convert.hpp>

#include <async_mqtt/packet/packet_id_type.hpp>
#include <async_mqtt/packet/fixed_header.hpp>
#include <async_mqtt/packet/pubopts.hpp>
#include <async_mqtt/packet/copy_to_static_vector.hpp>
#include <async_mqtt/packet/property_variant.hpp>

namespace async_mqtt::v5 {

namespace as = boost::asio;

template <std::size_t PacketIdBytes>
class basic_publish_packet {
public:
    using packet_id_t = typename packet_id_type<PacketIdBytes>::type;
    template <
        typename BufferSequence,
        typename std::enable_if<
            is_buffer_sequence<BufferSequence>::value,
            std::nullptr_t
        >::type = nullptr
    >
    basic_publish_packet(
        packet_id_t packet_id,
        buffer topic_name,
        BufferSequence payloads,
        pub::opts pubopts,
        properties props
    )
        : fixed_header_{
              static_cast<std::uint8_t>(
                  make_fixed_header(control_packet_type::publish, 0b0000) |
                  std::uint8_t(pubopts)
              )
          },
          topic_name_{force_move(topic_name)},
          property_length_(
              std::accumulate(
                  props.begin(),
                  props.end(),
                  std::size_t(0U),
                  [](std::size_t total, property_variant const& pv) {
                      return total + pv.size();
                  }
              )
          ),
          props_(force_move(props)),
          remaining_length_(
              2                      // topic name length
              + topic_name_.size()   // topic name
              + (  (pubopts.get_qos() == qos::at_least_once || pubopts.get_qos() == qos::exactly_once)
                 ? PacketIdBytes // packet_id
                 : 0)
          )
    {
        topic_name_length_buf_.resize(topic_name_length_buf_.capacity());
        endian_store(
            boost::numeric_cast<std::uint16_t>(topic_name.size()),
            topic_name_length_buf_.data()
        );
        auto b = buffer_sequence_begin(payloads);
        auto e = buffer_sequence_end(payloads);
        auto num_of_payloads = static_cast<std::size_t>(std::distance(b, e));
        payloads_.reserve(num_of_payloads);
        for (; b != e; ++b) {
            auto const& payload = *b;
            remaining_length_ += payload.size();
            payloads_.push_back(payload);
        }
#if 0 // TBD
        utf8string_check(topic_name_);
#endif

        auto pb = val_to_variable_bytes(property_length_);
        for (auto e : pb) {
            property_length_buf_.push_back(e);
        }

        remaining_length_ += property_length_buf_.size() + property_length_;

        auto rb = val_to_variable_bytes(remaining_length_);
        for (auto e : rb) {
            remaining_length_buf_.push_back(e);
        }
        if (pubopts.get_qos() == qos::at_least_once ||
            pubopts.get_qos() == qos::exactly_once) {
            packet_id_.resize(packet_id_.capacity());
            endian_store(packet_id, packet_id_.data());
        }
    }

    basic_publish_packet(buffer buf) {
        // fixed_header
        if (buf.empty()) {
            throw make_error(
                errc::bad_message,
                "v5::publish_packet fixed_header doesn't exist"
            );
        }
        fixed_header_ = static_cast<std::uint8_t>(buf.front());
        qos qos_value = get_qos();
        buf.remove_prefix(1);

        // remaining_length
        if (auto vl_opt = copy_advance_variable_length(buf, remaining_length_buf_)) {
            remaining_length_ = *vl_opt;
        }
        else {
            throw make_error(errc::bad_message, "v5::publish_packet remaining length is invalid");
        }

        // topic_name_length
        if (!copy_advance(buf, topic_name_length_buf_)) {
            throw make_error(
                errc::bad_message,
                "v5::publish_packet length of topic_name is invalid"
            );
        }
        auto topic_name_length = endian_load<std::uint16_t>(topic_name_length_buf_.data());

        // topic_name
        if (buf.size() < topic_name_length) {
            throw make_error(
                errc::bad_message,
                "v5::publish_packet topic_name doesn't match its length"
            );
        }
        topic_name_ = buf.substr(0, topic_name_length);
#if 0 // TBD
        utf8string_check(topic_name_);
#endif
        buf.remove_prefix(topic_name_length);

        // packet_id
        switch (qos_value) {
        case qos::at_most_once:
            break;
        case qos::at_least_once:
        case qos::exactly_once:
            if (!copy_advance(buf, packet_id_)) {
                throw make_error(
                    errc::bad_message,
                    "v5::publish_packet packet_id doesn't exist"
                );
            }
            break;
        default:
            throw make_error(
                errc::bad_message,
                "v5::publish_packet qos is invalid"
            );
            break;
        };

        // property_length
        if (auto vl_opt = copy_advance_variable_length(buf, property_length_buf_)) {
            property_length_ = *vl_opt;
        }
        else {
            throw make_error(errc::bad_message, "v5::publish_packet property length is invalid");
        }

        // property
        props_ = make_properties(buf);
        buf.remove_prefix(property_length_);

        // payload
        if (!buf.empty()) {
            payloads_.emplace_back(force_move(buf));
        }
    }

    /**
     * @brief Create const buffer sequence
     *        it is for boost asio APIs
     * @return const buffer sequence
     */
    std::vector<as::const_buffer> const_buffer_sequence() const {
        std::vector<as::const_buffer> ret;
        ret.reserve(num_of_const_buffer_sequence());
        ret.emplace_back(as::buffer(&fixed_header_, 1));
        ret.emplace_back(as::buffer(remaining_length_buf_.data(), remaining_length_buf_.size()));
        ret.emplace_back(as::buffer(topic_name_length_buf_.data(), topic_name_length_buf_.size()));
        ret.emplace_back(as::buffer(topic_name_));
        if (!packet_id_.empty()) {
            ret.emplace_back(as::buffer(packet_id_.data(), packet_id_.size()));
        }
        ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));
        auto props_cbs = async_mqtt::const_buffer_sequence(props_);
        std::move(props_cbs.begin(), props_cbs.end(), std::back_inserter(ret));
        for (auto const& payload : payloads_) {
            ret.emplace_back(as::buffer(payload));
        }
        return ret;
    }

    /**
     * @brief Get whole size of sequence
     * @return whole size
     */
    std::size_t size() const {
        return
            1 +                            // fixed header
            remaining_length_buf_.size() +
            remaining_length_;
    }

    /**
     * @brief Get number of element of const_buffer_sequence
     * @return number of element of const_buffer_sequence
     */
    std::size_t num_of_const_buffer_sequence() const {
        return
            1 +                   // fixed header
            1 +                   // remaining length
            2 +                   // topic name length, topic name
            (packet_id_.empty() ? 0 : 1) +  // packet_id
            1 +                   // property length
            std::accumulate(
                props_.begin(),
                props_.end(),
                std::size_t(0U),
                [](std::size_t total, property_variant const& pv) {
                    return total + pv.num_of_const_buffer_sequence();
                }
            ) +
            payloads_.size();
    }

    /**
     * @brief Get packet id
     * @return packet_id
     */
    packet_id_t packet_id() const {
        return endian_load<packet_id_t>(packet_id_.data());
    }

    /**
     * @brief Get publish_options
     * @return publish_options.
     */
    constexpr pub::opts get_options() const {
        return pub::opts(fixed_header_);
    }

    /**
     * @brief Get qos
     * @return qos
     */
    constexpr qos get_qos() const {
        return pub::get_qos(fixed_header_);
    }

    /**
     * @brief Check retain flag
     * @return true if retain, otherwise return false.
     */
    constexpr bool is_retain() const {
        return pub::is_retain(fixed_header_);
    }

    /**
     * @brief Check dup flag
     * @return true if dup, otherwise return false.
     */
    constexpr bool is_dup() const {
        return pub::is_dup(fixed_header_);
    }

    /**
     * @brief Get topic name
     * @return topic name
     */
    constexpr buffer const& topic() const {
        return topic_name_;
    }

    /**
     * @brief Get payload
     * @return payload
     */
    std::vector<buffer> payload() const {
        return payloads_;
    }

    /**
     * @brief Get payload as single buffer
     * @return payload
     */
    buffer payload_as_buffer() const {
        auto size = std::accumulate(
            payloads_.begin(),
            payloads_.end(),
            std::size_t(0),
            [](std::size_t s, buffer const& payload) {
                return s += payload.size();
            }
        );

        if (size == 0) return buffer();

        auto spa = make_shared_ptr_array(size);
        auto ptr = spa.get();
        auto it = ptr;
        for (auto const& payload : payloads_) {
            auto b = payload.data();
            auto s = payload.size();;
            auto e = b + s;
            std::copy(b, e, it);
            it += s;
        }
        return buffer(ptr, size, force_move(spa));
    }

    /**
     * @brief Set dup flag
     * @param dup flag value to set
     */
    constexpr void set_dup(bool dup) {
        pub::set_dup(fixed_header_, dup);
    }

private:
    std::uint8_t fixed_header_;
    buffer topic_name_;
    static_vector<char, 2> topic_name_length_buf_;
    static_vector<char, PacketIdBytes> packet_id_;
    std::uint32_t property_length_;
    static_vector<char, 4> property_length_buf_;
    properties props_;
    std::vector<buffer> payloads_;
    std::uint32_t remaining_length_;
    static_vector<char, 4> remaining_length_buf_;
};

using publish_packet = basic_publish_packet<2>;

} // namespace async_mqtt::v5

#endif // ASYNC_MQTT_PACKET_V5_PUBLISH_HPP