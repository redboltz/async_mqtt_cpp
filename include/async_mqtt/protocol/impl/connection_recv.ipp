// Copyright Takatoshi Kondo 2024
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_PROTOCOL_IMPL_CONNECTION_RECV_IPP)
#define ASYNC_MQTT_PROTOCOL_IMPL_CONNECTION_RECV_IPP

#include <deque>

#include <async_mqtt/protocol/connection.hpp>
#include <async_mqtt/protocol/event_packet_received.hpp>
#include <async_mqtt/protocol/event_packet_id_released.hpp>
#include <async_mqtt/protocol/event_send.hpp>
#include <async_mqtt/protocol/event_recv.hpp>
#include <async_mqtt/protocol/event_close.hpp>
#include <async_mqtt/util/static_vector.hpp>
#include <async_mqtt/util/shared_ptr_array.hpp>
#include <async_mqtt/util/inline.hpp>

#if !defined(ASYNC_MQTT_SEPARATE_COMPILATION)
#include <async_mqtt/impl/buffer_to_packet_variant.ipp>
#endif // !defined(ASYNC_MQTT_SEPARATE_COMPILATION)


namespace async_mqtt {

namespace detail {

struct error_packet {
    error_packet(error_code ec)
        :ec{ec} {}
    error_packet(buffer packet)
        :packet{force_move(packet)} {}

    error_code ec;
    buffer packet;
};

template <role Role, std::size_t PacketIdBytes>
class
basic_connection_impl<Role, PacketIdBytes>::
recv_packet_builder {
public:
    void recv(char const* ptr, std::size_t size) {
        BOOST_ASSERT(ptr);
        while (size != 0) {
            switch (read_state_) {
            case read_state::fixed_header: {
                auto fixed_header = *ptr++;
                --size;
                header_remaining_length_buf_.push_back(fixed_header);
                read_state_ = read_state::remaining_length;
            } break;
            case read_state::remaining_length: {
                while (size != 0) {
                    auto encoded_byte = *ptr++;
                    --size;
                    header_remaining_length_buf_.push_back(encoded_byte);
                    remaining_length_ += (std::uint8_t(encoded_byte) & 0b0111'1111) * multiplier_;
                    multiplier_ *= 128;
                    if ((encoded_byte & 0b1000'0000) == 0) {
                        read_state_ = read_state::payload;
                        raw_buf_size_ = header_remaining_length_buf_.size() + remaining_length_;
                        raw_buf_ = make_shared_ptr_char_array(raw_buf_size_);
                        raw_buf_ptr_ = raw_buf_.get();
                        std::copy_n(
                            header_remaining_length_buf_.data(),
                            header_remaining_length_buf_.size(),
                            raw_buf_ptr_
                        );
                        raw_buf_ptr_ += header_remaining_length_buf_.size();
                        break;
                    }
                    if (multiplier_ == 128 * 128 * 128 * 128) {
                        read_packets_.emplace_back(make_error_code(disconnect_reason_code::packet_too_large));
                        initialize();
                        return;
                    }
                }
                if (read_state_ != read_state::payload) {
                    return;
                }
            } break;
            case read_state::payload: {
                if (size >= remaining_length_) {
                    std::copy_n(
                        ptr,
                        remaining_length_,
                        raw_buf_ptr_
                    );
                    raw_buf_ptr_ += remaining_length_;
                    size -= remaining_length_;
                    auto ptr = raw_buf_.get();
                    read_packets_.emplace_back(
                        buffer{ptr, raw_buf_size_, force_move(raw_buf_)}
                    );
                    initialize();
                }
                else {
                    std::copy_n(
                        ptr,
                        size,
                        raw_buf_ptr_
                    );
                    raw_buf_ptr_ += size;
                    remaining_length_ -= size;
                    return;
                }
            } break;
            }
        }
    }

    error_packet front() const {
        return read_packets_.front();
    }

    void pop_front() {
        read_packets_.pop_front();
    }

    bool empty() const {
        return read_packets_.empty();
    }

    void initialize() {
        read_state_ = read_state::fixed_header;
        header_remaining_length_buf_.clear();
        remaining_length_ = 0;
        multiplier_ = 1;
        raw_buf_.reset();
        raw_buf_size_ = 0;
        raw_buf_ptr_ = nullptr;
    }


private:
    enum class read_state{fixed_header, remaining_length, payload} read_state_ = read_state::fixed_header;
    std::size_t remaining_length_ = 0;
    std::size_t multiplier_ = 1;
    static_vector<char, 5> header_remaining_length_buf_;
    std::shared_ptr<char []> raw_buf_;
    std::size_t raw_buf_size_ = 0;
    char* raw_buf_ptr_ = nullptr;
    std::deque<error_packet> read_packets_;

};

template <role Role, std::size_t PacketIdBytes>
ASYNC_MQTT_HEADER_ONLY_INLINE
std::vector<basic_event_variant<PacketIdBytes>>
basic_connection_impl<Role, PacketIdBytes>::
recv(char const* ptr, std::size_t size) {
    std::vector<basic_event_variant<PacketIdBytes>> events;

    rpv_.recv(ptr, size);
    for (; !rpv_.empty(); rpv_.pop_front()) {
        auto ep = rpv_.front();
        if (ep.ec) {
            events.emplace_back(ep.ec);
            events.emplace_back(event_close{});
            return events;
        }
        auto& buf{ep.packet};

        // Checking maximum_packet_size
        if (buf.size() > maximum_packet_size_recv_) {
            // on v3.1.1 maximum_packet_size_recv_ is initialized as packet_size_no_limit
            BOOST_ASSERT(protocol_version_ == protocol_version::v5);
            events.emplace_back(
                make_error_code(
                    disconnect_reason_code::packet_too_large
                )
            );
            events.emplace_back(
                basic_event_send<PacketIdBytes>{
                    v5::disconnect_packet{
                        disconnect_reason_code::packet_too_large
                    }
                }
            );
            events.emplace_back(event_close{});
            return events;
        }

        error_code ec;
        auto v = buffer_to_basic_packet_variant<PacketIdBytes>(buf, protocol_version_, ec);
        if (ec) {
            if (protocol_version_ == protocol_version::v5) {
                if constexpr (can_send_as_server(Role)) {
                    if (ec.category() == get_connect_reason_code_category()) {
                        status_ = connection_status::connecting;
                        events.emplace_back(
                            basic_event_send<PacketIdBytes>{
                                v5::connack_packet{
                                    false, // session_present
                                    static_cast<connect_reason_code>(ec.value())
                                }
                            }
                        );
                    }
                }
                else if (ec.category() == get_disconnect_reason_code_category()) {
                    if (status_ == connection_status::connected) {
                        events.emplace_back(
                            basic_event_send<PacketIdBytes>{
                                v5::disconnect_packet{
                                    static_cast<disconnect_reason_code>(ec.value())
                                }
                            }
                        );
                    }
                }
            }
            events.emplace_back(event_close{});
            return events;
        }

        // no errors on packet creation phase
        ASYNC_MQTT_LOG("mqtt_impl", trace)
            << "recv:" << v;
        auto result = v.visit(
            // do internal protocol processing
            overload {
                [&](v3_1_1::connect_packet& p) {
                    initialize();
                    protocol_version_ = protocol_version::v3_1_1;
                    status_ = connection_status::connecting;
                    auto keep_alive = p.keep_alive();
                    if (keep_alive != 0) {
                        pingreq_recv_timeout_ms_.emplace(
                            std::chrono::milliseconds{
                                keep_alive * 1000 * 3 / 2
                            }
                        );
                    }
                    if (p.clean_session()) {
                        need_store_ = false;
                    }
                    else {
                        need_store_ = true;
                    }
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{v}
                    );
                    return true;
                },
                [&](v5::connect_packet& p) {
                    initialize();
                    protocol_version_ = protocol_version::v5;
                    status_ = connection_status::connecting;
                    auto keep_alive = p.keep_alive();
                    if (keep_alive != 0) {
                        pingreq_recv_timeout_ms_.emplace(
                            std::chrono::milliseconds{
                                keep_alive * 1000 * 3 / 2
                            }
                        );
                    }
                    for (auto const& prop : p.props()) {
                        prop.visit(
                            overload {
                                [&](property::topic_alias_maximum const& p) {
                                    if (p.val() > 0) {
                                        topic_alias_send_.emplace(p.val());
                                    }
                                },
                                [&](property::receive_maximum const& p) {
                                    BOOST_ASSERT(p.val() != 0);
                                    publish_send_max_ = p.val();
                                },
                                [&](property::maximum_packet_size const& p) {
                                    BOOST_ASSERT(p.val() != 0);
                                    maximum_packet_size_send_ = p.val();
                                },
                                [&](property::session_expiry_interval const& p) {
                                    if (p.val() != 0) {
                                        need_store_ = true;
                                    }
                                },
                                [](auto const&) {
                                }
                            }
                        );
                    }
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    return true;
                },
                [&](v3_1_1::connack_packet& p) {
                    if (p.code() == connect_return_code::accepted) {
                        status_ = connection_status::connected;
                        if (p.session_present()) {
                            send_stored(events);
                        }
                        else {
                            pid_man_.clear();
                            store_.clear();
                        }
                    }
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    return true;
                },
                [&](v5::connack_packet& p) {
                    if (p.code() == connect_reason_code::success) {
                        status_ = connection_status::connected;
                         for (auto const& prop : p.props()) {
                            prop.visit(
                                overload {
                                    [&](property::topic_alias_maximum const& p) {
                                        if (p.val() > 0) {
                                            topic_alias_send_.emplace(p.val());
                                        }
                                    },
                                    [&](property::receive_maximum const& p) {
                                        BOOST_ASSERT(p.val() != 0);
                                        publish_send_max_ = p.val();
                                    },
                                    [&](property::maximum_packet_size const& p) {
                                        BOOST_ASSERT(p.val() != 0);
                                        maximum_packet_size_send_ = p.val();
                                    },
                                    [&](property::server_keep_alive const& p) {
                                        if constexpr (can_send_as_client(Role)) {
                                            set_pingreq_send_interval(
                                                std::chrono::seconds{p.val()},
                                                events
                                            );
                                        }
                                    },
                                    [](auto const&) {
                                    }
                                }
                            );
                        }

                        if (p.session_present()) {
                            send_stored(events);
                        }
                        else {
                            pid_man_.clear();
                            store_.clear();
                        }
                    }
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    return true;
                },
                [&](v3_1_1::basic_publish_packet<PacketIdBytes>& p) {
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    switch (p.opts().get_qos()) {
                    case qos::at_least_once: {
                        auto packet_id = p.packet_id();
                        if (auto_pub_response_ &&
                            status_ == connection_status::connected) {
                            events.emplace_back(
                                event_send{
                                    v3_1_1::basic_puback_packet<PacketIdBytes>(packet_id)
                                }
                            );
                        }
                    } break;
                    case qos::exactly_once: {
                        auto packet_id = p.packet_id();
                        bool already_handled = false;
                        if (qos2_publish_handled_.find(packet_id) == qos2_publish_handled_.end()) {
                            qos2_publish_handled_.emplace(packet_id);
                        }
                        else {
                            already_handled = true;
                        }
                        if (status_ == connection_status::connected &&
                            (auto_pub_response_ ||
                             already_handled) // already_handled is true only if the pubrec packet
                        ) {                   // corresponding to the publish packet has already
                            events.emplace_back(
                                event_send{
                                    v3_1_1::basic_pubrec_packet<PacketIdBytes>(packet_id)
                                }
                            );
                            events.emplace_back(event_recv{}); // recv pubrel
                        }
                    } break;
                    default:
                        break;
                    }
                    return true;
                },
                [&](v5::basic_publish_packet<PacketIdBytes>& p) {
                    std::vector<basic_event_variant<PacketIdBytes>> additional_events;
                    switch (p.opts().get_qos()) {
                    case qos::at_least_once: {
                        auto packet_id = p.packet_id();
                        if (publish_recv_.size() == publish_recv_max_) {
                            events.emplace_back(
                                make_error_code(
                                    disconnect_reason_code::receive_maximum_exceeded
                                )
                            );
                            events.emplace_back(
                                event_send{
                                    v5::disconnect_packet{
                                        disconnect_reason_code::receive_maximum_exceeded
                                    }
                                }
                            );
                            events.emplace_back(event_close{});
                            return false;
                        }
                        publish_recv_.insert(packet_id);
                        if (auto_pub_response_ && status_ == connection_status::connected) {
                            additional_events.emplace_back(
                                event_send{
                                    v5::basic_puback_packet<PacketIdBytes>(packet_id)
                                }
                            );
                        }
                    } break;
                    case qos::exactly_once: {
                        auto packet_id = p.packet_id();
                        if (publish_recv_.size() == publish_recv_max_) {
                            events.emplace_back(
                                make_error_code(
                                    disconnect_reason_code::receive_maximum_exceeded
                                )
                            );
                            events.emplace_back(
                                event_send{
                                    v5::disconnect_packet{
                                        disconnect_reason_code::receive_maximum_exceeded
                                    }
                                }
                            );
                            events.emplace_back(event_close{});
                            return false;
                        }
                        publish_recv_.insert(packet_id);

                        bool already_handled = false;
                        if (qos2_publish_handled_.find(packet_id) == qos2_publish_handled_.end()) {
                            qos2_publish_handled_.emplace(packet_id);
                        }
                        else {
                            already_handled = true;
                        }
                        if (status_ == connection_status::connected &&
                            (auto_pub_response_ ||
                             already_handled) // already_handled is true only if the pubrec packet
                        ) {                   // corresponding to the publish packet has already
                            additional_events.emplace_back(
                                event_send{
                                    v5::basic_pubrec_packet<PacketIdBytes>(packet_id)
                                }
                            );
                            additional_events.emplace_back(event_recv{}); // recv pubrel
                        }
                    } break;
                    default:
                        break;
                    }

                    if (p.topic().empty()) {
                        if (auto ta_opt = get_topic_alias(p.props())) {
                            // extract topic from topic_alias
                            if (*ta_opt == 0 ||
                                !topic_alias_recv_ || // topic_alias_maximum is 0
                                *ta_opt > topic_alias_recv_->max()) {
                                events.emplace_back(
                                    make_error_code(
                                        disconnect_reason_code::topic_alias_invalid
                                    )
                                );
                                events.emplace_back(
                                    event_send{
                                        v5::disconnect_packet{
                                            disconnect_reason_code::topic_alias_invalid
                                        }
                                    }
                                );
                                events.emplace_back(event_close{});
                                return false;
                            }
                            BOOST_ASSERT(topic_alias_recv_);
                            auto topic = topic_alias_recv_->find(*ta_opt);
                            if (topic.empty()) {
                                ASYNC_MQTT_LOG("mqtt_impl", error)
                                    << "no matching topic alias: "
                                    << *ta_opt;
                                events.emplace_back(
                                    make_error_code(
                                        disconnect_reason_code::topic_alias_invalid
                                    )
                                );
                                events.emplace_back(
                                    event_send{
                                        v5::disconnect_packet{
                                            disconnect_reason_code::topic_alias_invalid
                                        }
                                    }
                                );
                                events.emplace_back(event_close{});
                                return false;
                            }
                            else {
                                p.add_topic(force_move(topic));
                            }
                        }
                        else {
                            ASYNC_MQTT_LOG("mqtt_impl", error)
                                << "topic is empty but topic_alias isn't set";
                            events.emplace_back(
                                make_error_code(
                                    disconnect_reason_code::topic_alias_invalid
                                )
                            );
                            events.emplace_back(
                                event_send{
                                    v5::disconnect_packet{
                                        disconnect_reason_code::topic_alias_invalid
                                    }
                                }
                            );
                            events.emplace_back(event_close{});
                            return false;
                        }
                    }
                    else {
                        if (auto ta_opt = get_topic_alias(p.props())) {
                            if (*ta_opt == 0 ||
                                !topic_alias_recv_ || // topic_alias_maximum is 0
                                *ta_opt > topic_alias_recv_->max()) {
                                events.emplace_back(
                                    make_error_code(
                                        disconnect_reason_code::topic_alias_invalid
                                    )
                                );
                                events.emplace_back(
                                    event_send{
                                        v5::disconnect_packet{
                                            disconnect_reason_code::topic_alias_invalid
                                        }
                                    }
                                );
                                events.emplace_back(event_close{});
                                return false;
                            }
                            BOOST_ASSERT(topic_alias_recv_);
                            // extract topic from topic_alias
                            topic_alias_recv_->insert_or_update(p.topic(), *ta_opt);
                        }
                    }
                    // received event first
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    // followed by additional event
                    std::move(
                        additional_events.begin(),
                        additional_events.end(),
                        std::back_inserter(events)
                    );
                    return true;
                },
                [&](v3_1_1::basic_puback_packet<PacketIdBytes>& p) {
                    auto packet_id = p.packet_id();
                    if (pid_puback_.erase(packet_id)) {
                        store_.erase(response_packet::v3_1_1_puback, packet_id);
                        pid_man_.release_id(packet_id);
                        events.emplace_back(
                            basic_event_packet_id_released<PacketIdBytes>{packet_id}
                        );
                    }
                    else {
                        ASYNC_MQTT_LOG("mqtt_impl", info)
                            << "invalid packet_id puback received packet_id:" << packet_id;
                        events.emplace_back(
                            make_error_code(
                                disconnect_reason_code::protocol_error
                            )
                        );
                        events.emplace_back(event_close{});
                        return false;
                    }
                    return true;
                },
                [&](v5::basic_puback_packet<PacketIdBytes>& p) {
                    auto packet_id = p.packet_id();
                    if (pid_puback_.erase(packet_id)) {
                        events.emplace_back(
                            basic_event_packet_received<PacketIdBytes>{p}
                        );
                        store_.erase(response_packet::v5_puback, packet_id);
                        pid_man_.release_id(packet_id);
                        events.emplace_back(
                            basic_event_packet_id_released<PacketIdBytes>{packet_id}
                        );
                        --publish_send_count_;
                    }
                    else {
                        ASYNC_MQTT_LOG("mqtt_impl", info)
                            << "invalid packet_id puback received packet_id:" << packet_id;
                        events.emplace_back(
                            make_error_code(
                                disconnect_reason_code::protocol_error
                            )
                        );
                        events.emplace_back(
                            event_send{
                                v5::disconnect_packet{
                                    disconnect_reason_code::protocol_error
                                }
                            }
                        );
                        events.emplace_back(event_close{});
                        return false;
                    }
                    return true;
                },
                [&](v3_1_1::basic_pubrec_packet<PacketIdBytes>& p) {
                    auto packet_id = p.packet_id();
                    if (pid_pubrec_.erase(packet_id)) {
                        store_.erase(response_packet::v3_1_1_pubrec, packet_id);
                        events.emplace_back(
                            basic_event_packet_received<PacketIdBytes>{p}
                        );
                        if (auto_pub_response_ && status_ == connection_status::connected) {
                            events.emplace_back(
                                event_send{
                                    v3_1_1::basic_pubrel_packet<PacketIdBytes>{packet_id}
                                }
                            );
                        }
                    }
                    else {
                        ASYNC_MQTT_LOG("mqtt_impl", info)
                            << "invalid packet_id pubrec received packet_id:" << packet_id;
                        events.emplace_back(
                            make_error_code(
                                disconnect_reason_code::protocol_error
                            )
                        );
                        events.emplace_back(event_close{});
                        return false;
                    }
                    return true;
                },
                [&](v5::basic_pubrec_packet<PacketIdBytes>& p) {
                    auto packet_id = p.packet_id();
                    if (pid_pubrec_.erase(packet_id)) {
                        store_.erase(response_packet::v5_pubrec, packet_id);
                        events.emplace_back(
                            basic_event_packet_received<PacketIdBytes>{p}
                        );
                        if (make_error_code(p.code())) {
                            pid_man_.release_id(packet_id);
                            events.emplace_back(
                                basic_event_packet_id_released<PacketIdBytes>{packet_id}
                            );
                            qos2_publish_processing_.erase(packet_id);
                            --publish_send_count_;
                        }
                        else if (auto_pub_response_ && status_ == connection_status::connected) {
                            events.emplace_back(
                                event_send{
                                    v5::basic_pubrel_packet<PacketIdBytes>{packet_id}
                                }
                            );
                        }
                    }
                    else {
                        ASYNC_MQTT_LOG("mqtt_impl", info)
                            << "invalid packet_id pubrec received packet_id:" << packet_id;
                        events.emplace_back(
                            make_error_code(
                                disconnect_reason_code::protocol_error
                            )
                        );
                        events.emplace_back(
                            event_send{
                                v5::disconnect_packet{
                                    disconnect_reason_code::protocol_error
                                }
                            }
                        );
                        events.emplace_back(event_close{});
                        return false;
                    }
                    return true;
                },
                [&](v3_1_1::basic_pubrel_packet<PacketIdBytes>& p) {
                    auto packet_id = p.packet_id();
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    qos2_publish_handled_.erase(packet_id);
                    if (auto_pub_response_ && status_ == connection_status::connected) {
                        events.emplace_back(
                            event_send{
                                v3_1_1::basic_pubcomp_packet<PacketIdBytes>{packet_id}
                            }
                        );
                    }
                    return true;
                },
                [&](v5::basic_pubrel_packet<PacketIdBytes>& p) {
                    auto packet_id = p.packet_id();
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    qos2_publish_handled_.erase(packet_id);
                    if (auto_pub_response_ && status_ == connection_status::connected) {
                        events.emplace_back(
                            event_send{
                                v5::basic_pubcomp_packet<PacketIdBytes>{packet_id}
                            }
                        );
                    }
                    return true;
                },
                [&](v3_1_1::basic_pubcomp_packet<PacketIdBytes>& p) {
                    auto packet_id = p.packet_id();
                    if (pid_pubcomp_.erase(packet_id)) {
                        store_.erase(response_packet::v3_1_1_pubcomp, packet_id);
                        events.emplace_back(
                            basic_event_packet_received<PacketIdBytes>{p}
                        );
                        pid_man_.release_id(packet_id);
                        events.emplace_back(
                            basic_event_packet_id_released<PacketIdBytes>{packet_id}
                        );
                        qos2_publish_processing_.erase(packet_id);
                    }
                    else {
                        ASYNC_MQTT_LOG("mqtt_impl", info)
                            << "invalid packet_id pubcomp received packet_id:" << packet_id;
                        events.emplace_back(
                            make_error_code(
                                disconnect_reason_code::protocol_error
                            )
                        );
                        events.emplace_back(event_close{});
                        return false;
                    }
                    return true;
                },
                [&](v5::basic_pubcomp_packet<PacketIdBytes>& p) {
                    auto packet_id = p.packet_id();
                    if (pid_pubcomp_.erase(packet_id)) {
                        store_.erase(response_packet::v5_pubcomp, packet_id);
                        events.emplace_back(
                            basic_event_packet_received<PacketIdBytes>{p}
                        );
                        pid_man_.release_id(packet_id);
                        events.emplace_back(
                            basic_event_packet_id_released<PacketIdBytes>{packet_id}
                        );
                        qos2_publish_processing_.erase(packet_id);
                        --publish_send_count_;
                    }
                    else {
                        ASYNC_MQTT_LOG("mqtt_impl", info)
                            << "invalid packet_id pubcomp received packet_id:" << packet_id;
                        events.emplace_back(
                            make_error_code(
                                disconnect_reason_code::protocol_error
                            )
                        );
                        events.emplace_back(
                            event_send{
                                v5::disconnect_packet{
                                    disconnect_reason_code::protocol_error
                                }
                            }
                        );
                        events.emplace_back(event_close{});
                        return false;
                    }
                    return true;
                },
                [&](v3_1_1::basic_subscribe_packet<PacketIdBytes>& p) {
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    return true;
                },
                [&](v5::basic_subscribe_packet<PacketIdBytes>& p) {
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    return true;
                },
                [&](v3_1_1::basic_suback_packet<PacketIdBytes>& p) {
                    auto packet_id = p.packet_id();
                    if (pid_suback_.erase(packet_id)) {
                        events.emplace_back(
                            basic_event_packet_received<PacketIdBytes>{p}
                        );
                        pid_man_.release_id(packet_id);
                        events.emplace_back(
                            basic_event_packet_id_released<PacketIdBytes>{packet_id}
                        );
                    }
                    return true;
                },
                [&](v5::basic_suback_packet<PacketIdBytes>& p) {
                    auto packet_id = p.packet_id();
                    if (pid_suback_.erase(packet_id)) {
                        events.emplace_back(
                            basic_event_packet_received<PacketIdBytes>{p}
                        );
                        pid_man_.release_id(packet_id);
                        events.emplace_back(
                            basic_event_packet_id_released<PacketIdBytes>{packet_id}
                        );
                    }
                    return true;
                },
                [&](v3_1_1::basic_unsubscribe_packet<PacketIdBytes>& p) {
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    return true;
                },
                [&](v5::basic_unsubscribe_packet<PacketIdBytes>& p) {
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    return true;
                },
                [&](v3_1_1::basic_unsuback_packet<PacketIdBytes>& p) {
                    auto packet_id = p.packet_id();
                    if (pid_unsuback_.erase(packet_id)) {
                        events.emplace_back(
                            basic_event_packet_received<PacketIdBytes>{p}
                        );
                        pid_man_.release_id(packet_id);
                        events.emplace_back(
                            basic_event_packet_id_released<PacketIdBytes>{packet_id}
                        );
                    }
                    return true;
                },
                [&](v5::basic_unsuback_packet<PacketIdBytes>& p) {
                    auto packet_id = p.packet_id();
                    if (pid_unsuback_.erase(packet_id)) {
                        events.emplace_back(
                            basic_event_packet_received<PacketIdBytes>{p}
                        );
                        pid_man_.release_id(packet_id);
                        events.emplace_back(
                            basic_event_packet_id_released<PacketIdBytes>{packet_id}
                        );
                    }
                    return true;
                },
                [&](v3_1_1::pingreq_packet& p) {
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    if constexpr(can_send_as_server(Role)) {
                        if (auto_ping_response_ &&
                            status_ == connection_status::connected) {
                            events.emplace_back(
                                event_send{
                                    v3_1_1::pingreq_packet{}
                                }
                            );
                        }
                    }
                    return true;
                },
                [&](v5::pingreq_packet& p) {
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    if constexpr(can_send_as_server(Role)) {
                        if (auto_ping_response_ &&
                            status_ == connection_status::connected) {
                            events.emplace_back(
                                event_send{
                                    v5::pingreq_packet{}
                                }
                            );
                        }
                    }
                    return true;
                },
                [&](v3_1_1::pingresp_packet& p) {
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    events.emplace_back(
                        event_timer{
                            event_timer::op_type::cancel,
                            timer::pingresp_recv
                        }
                    );
                    return true;
                },
                [&](v5::pingresp_packet& p) {
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    events.emplace_back(
                        event_timer{
                            event_timer::op_type::cancel,
                            timer::pingresp_recv
                        }
                    );
                    return true;
                },
                [&](v3_1_1::disconnect_packet& p) {
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    status_ = connection_status::disconnecting;
                    return true;
                },
                [&](v5::disconnect_packet& p) {
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    status_ = connection_status::disconnecting;
                    return true;
                },
                [&](v5::auth_packet& p) {
                    events.emplace_back(
                        basic_event_packet_received<PacketIdBytes>{p}
                    );
                    return true;
                },
                [&](std::monostate&) {
                    return false;
                }
            }
        );

        if (!result) return events;
        if (pingreq_recv_timeout_ms_) {
            events.emplace_back(
                event_timer{
                    event_timer::op_type::cancel,
                    timer::pingreq_recv
                }
            );
            if (status_ == connection_status::connecting ||
                status_ == connection_status::connected
            ) {
                events.emplace_back(
                    event_timer{
                        event_timer::op_type::set,
                        timer::pingreq_recv,
                        *pingreq_recv_timeout_ms_
                    }
                );
            }
        }
    }
    return events;
}

} // namespace detail

template <role Role, std::size_t PacketIdBytes>
ASYNC_MQTT_HEADER_ONLY_INLINE
std::vector<basic_event_variant<PacketIdBytes>>
basic_connection<Role, PacketIdBytes>::
recv(char const* ptr, std::size_t size) {
    BOOST_ASSERT(impl_);
    BOOST_ASSERT(ptr);
    return impl_->recv(ptr, size);
}

} // namespace async_mqtt

#endif // ASYNC_MQTT_PROTOCOL_IMPL_CONNECTION_RECV_IPP
