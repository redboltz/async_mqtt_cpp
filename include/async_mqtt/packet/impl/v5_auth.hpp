// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_PACKET_IMPL_V5_AUTH_HPP)
#define ASYNC_MQTT_PACKET_IMPL_V5_AUTH_HPP

#include <utility>
#include <numeric>

#include <boost/numeric/conversion/cast.hpp>

#include <async_mqtt/packet/v5_auth.hpp>
#include <async_mqtt/exception.hpp>
#include <async_mqtt/util/buffer.hpp>

#include <async_mqtt/util/move.hpp>
#include <async_mqtt/util/static_vector.hpp>
#include <async_mqtt/util/endian_convert.hpp>
#include <async_mqtt/util/scope_guard.hpp>

#include <async_mqtt/packet/fixed_header.hpp>
#include <async_mqtt/packet/packet_id_type.hpp>
#include <async_mqtt/packet/reason_code.hpp>
#include <async_mqtt/packet/property_variant.hpp>
#include <async_mqtt/packet/copy_to_static_vector.hpp>

/**
 * @defgroup auth_v5
 * @ingroup packet_v5
 */

namespace async_mqtt::v5 {

namespace as = boost::asio;

inline
auth_packet::auth_packet(
    auth_reason_code reason_code,
    properties props
) : auth_packet{
        std::optional<auth_reason_code>(reason_code),
        force_move(props)
    }
{}

inline
auth_packet::auth_packet(
) : auth_packet{
        std::nullopt,
        properties{}
    }
{}

inline
auth_packet::auth_packet(
    auth_reason_code reason_code
) : auth_packet{
        std::optional<auth_reason_code>(reason_code),
        properties{}
    }
{}

inline
constexpr control_packet_type auth_packet::type() {
    return control_packet_type::auth;
}

inline
std::vector<as::const_buffer> auth_packet::const_buffer_sequence() const {
    std::vector<as::const_buffer> ret;
    ret.reserve(num_of_const_buffer_sequence());
    ret.emplace_back(as::buffer(&fixed_header_, 1));
    ret.emplace_back(as::buffer(remaining_length_buf_.data(), remaining_length_buf_.size()));

    if (reason_code_) {
        ret.emplace_back(as::buffer(&*reason_code_, 1));

        if (property_length_buf_.size() != 0) {
            ret.emplace_back(as::buffer(property_length_buf_.data(), property_length_buf_.size()));
            auto props_cbs = async_mqtt::const_buffer_sequence(props_);
            std::move(props_cbs.begin(), props_cbs.end(), std::back_inserter(ret));
        }
    }

    return ret;
}

inline
std::size_t auth_packet::size() const {
    return
        1 +                            // fixed header
        remaining_length_buf_.size() +
        remaining_length_;
}

inline
std::size_t auth_packet::num_of_const_buffer_sequence() const {
    return
        1 +                   // fixed header
        1 +                   // remaining length
        [&] () -> std::size_t {
            if (reason_code_) {
                return
                    1 +       // reason_code
                    [&] () -> std::size_t {
                        if (property_length_buf_.size() == 0) return 0;
                        return
                            1 +                   // property length
                            async_mqtt::num_of_const_buffer_sequence(props_);
                    }();
            }
            return 0;
        }();
}

inline
auth_reason_code auth_packet::code() const {
    if (reason_code_) return *reason_code_;
    return auth_reason_code::success;
}

inline
properties const& auth_packet::props() const {
    return props_;
}

inline
auth_packet::auth_packet(
    std::optional<auth_reason_code> reason_code,
    properties props
)
    : fixed_header_{
          make_fixed_header(control_packet_type::auth, 0b0000)
      },
      remaining_length_{
          0
      },
      reason_code_{reason_code},
      property_length_(async_mqtt::size(props)),
      props_(force_move(props))
{
    using namespace std::literals;

    auto guard = unique_scope_guard(
        [&] {
            auto rb = val_to_variable_bytes(boost::numeric_cast<std::uint32_t>(remaining_length_));
            for (auto e : rb) {
                remaining_length_buf_.push_back(e);
            }
        }
    );

    if (!reason_code_) return;
    remaining_length_ += 1;

    if (property_length_ == 0) return;

    auto pb = val_to_variable_bytes(boost::numeric_cast<std::uint32_t>(property_length_));
    for (auto e : pb) {
        property_length_buf_.push_back(e);
    }

    for (auto const& prop : props_) {
        auto id = prop.id();
        if (!validate_property(property_location::auth, id)) {
            throw make_error(
                errc::bad_message,
                "v5::auth_packet property "s + id_to_str(id) + " is not allowed"
            );
        }
    }

    remaining_length_ += property_length_buf_.size() + property_length_;
}

inline
auth_packet::auth_packet(buffer buf) {
    // fixed_header
    if (buf.empty()) {
        throw make_error(
            errc::bad_message,
            "v5::auth_packet fixed_header doesn't exist"
        );
    }
    fixed_header_ = static_cast<std::uint8_t>(buf.front());
    buf.remove_prefix(1);

    // remaining_length
    if (auto vl_opt = insert_advance_variable_length(buf, remaining_length_buf_)) {
        remaining_length_ = *vl_opt;
    }
    else {
        throw make_error(errc::bad_message, "v5::auth_packet remaining length is invalid");
    }

    if (remaining_length_ == 0) {
        if (!buf.empty()) {
            throw make_error(errc::bad_message, "v5::auth_packet remaining length is invalid");
        }
        return;
    }

    // connect_reason_code
    reason_code_.emplace(static_cast<auth_reason_code>(buf.front()));
    buf.remove_prefix(1);
    switch (*reason_code_) {
    case auth_reason_code::success:
    case auth_reason_code::continue_authentication:
    case auth_reason_code::re_authenticate:
        break;
    default:
        throw make_error(
            errc::bad_message,
            "v5::auth_packet connect reason_code is invalid"
        );
        break;
    }

    if (remaining_length_ == 1) {
        if (!buf.empty()) {
            throw make_error(errc::bad_message, "v5::auth_packet remaining length is invalid");
        }
        return;
    }

    // property
    auto it = buf.begin();
    if (auto pl_opt = variable_bytes_to_val(it, buf.end())) {
        property_length_ = *pl_opt;
        std::copy(buf.begin(), it, std::back_inserter(property_length_buf_));
        buf.remove_prefix(std::size_t(std::distance(buf.begin(), it)));
        if (buf.size() < property_length_) {
            throw make_error(
                errc::bad_message,
                "v5::auth_packet properties_don't match its length"
            );
        }
        auto prop_buf = buf.substr(0, property_length_);
        props_ = make_properties(prop_buf, property_location::auth);
        buf.remove_prefix(property_length_);
    }
    else {
        throw make_error(
            errc::bad_message,
            "v5::auth_packet property_length is invalid"
        );
    }

    if (!buf.empty()) {
        throw make_error(
            errc::bad_message,
            "v5::auth_packet properties don't match its length"
        );
    }
}

} // namespace async_mqtt::v5

#endif // ASYNC_MQTT_PACKET_IMPL_V5_AUTH_HPP