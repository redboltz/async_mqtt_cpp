// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_PROTOCOL_DETAIL_CONNECTION_IMPL_HPP)
#define ASYNC_MQTT_PROTOCOL_DETAIL_CONNECTION_IMPL_HPP

#include <set>

#include <async_mqtt/error.hpp>
#include <async_mqtt/packet/packet_id_type.hpp>
#include <async_mqtt/util/packet_id_manager.hpp>
#include <async_mqtt/util/store.hpp>
#include <async_mqtt/util/topic_alias_send.hpp>
#include <async_mqtt/util/topic_alias_recv.hpp>
#include <async_mqtt/protocol_version.hpp>
#include <async_mqtt/protocol/connection_fwd.hpp>
#include <async_mqtt/protocol/connection_status.hpp>
#include <async_mqtt/protocol/event_variant.hpp>
#include <async_mqtt/protocol/timer.hpp>

namespace async_mqtt::detail {

template <role Role, std::size_t PacketIdBytes>
class basic_connection_impl {
    using basic_pid_type = typename basic_packet_id_type<PacketIdBytes>::type;

public:
    explicit basic_connection_impl(protocol_version ver);

    void notify_handshaked();

    template <typename Packet>
    std::vector<basic_event_variant<PacketIdBytes>>
    send(Packet packet);

    template <typename Begin, typename End>
    std::vector<basic_event_variant<PacketIdBytes>> recv(Begin b, End e);

    std::vector<basic_event_variant<PacketIdBytes>>
    notify_timer_fired(timer kind);

    void
    notify_closed();

    void
    set_pingreq_send_interval(
        std::chrono::milliseconds duration,
        std::vector<basic_event_variant<PacketIdBytes>>& events
    );

    std::size_t get_receive_maximum_vacancy_for_send() const;

    void set_offline_publish(bool val);

    void set_auto_pub_response(bool val);

    void set_auto_ping_response(bool val);

    void set_auto_map_topic_alias_send(bool val);

    void set_auto_replace_topic_alias_send(bool val);

    void set_pingresp_recv_timeout(std::chrono::milliseconds duration);

    std::optional<typename basic_packet_id_type<PacketIdBytes>::type> acquire_unique_packet_id();

    bool register_packet_id(typename basic_packet_id_type<PacketIdBytes>::type packet_id);

    std::vector<basic_event_variant<PacketIdBytes>>
    release_packet_id(typename basic_packet_id_type<PacketIdBytes>::type packet_id);

    std::set<typename basic_packet_id_type<PacketIdBytes>::type> get_qos2_publish_handled_pids() const;

    void restore_qos2_publish_handled_pids(std::set<typename basic_packet_id_type<PacketIdBytes>::type> pids);

    void restore_packets(
        std::vector<basic_store_packet_variant<PacketIdBytes>> pvs
    );

    std::vector<basic_store_packet_variant<PacketIdBytes>> get_stored_packets() const;

    protocol_version get_protocol_version() const;

    error_code regulate_for_store(
        v5::basic_publish_packet<PacketIdBytes>& packet
    ) const;

    bool is_publish_processing(typename basic_packet_id_type<PacketIdBytes>::type pid) const;

    connection_status get_connection_status() const;

private:
    template <typename ActualPacket>
    bool
    process_send_packet(
        ActualPacket actual_packet,
        std::vector<basic_event_variant<PacketIdBytes>>& events
    );

    void
    send_stored(std::vector<basic_event_variant<PacketIdBytes>>& events);

    std::vector<basic_event_variant<PacketIdBytes>>
    process_recv_packet();

    void
    initialize(bool is_client);

    std::optional<std::string>
    validate_topic_alias(std::optional<topic_alias_type> ta_opt);

    bool
    validate_topic_alias_range(topic_alias_type ta);

    bool
    validate_maximum_packet_size(std::size_t size);

    std::optional<topic_alias_type>
    static get_topic_alias(properties const& props);

    static constexpr bool can_send_as_client(role r) {
        return
            static_cast<int>(r) &
            static_cast<int>(role::client);
    }
    static constexpr bool can_send_as_server(role r) {
        return
            static_cast<int>(r) &
            static_cast<int>(role::server);
    }

private:
    protocol_version protocol_version_;

    packet_id_manager<basic_pid_type> pid_man_;
    std::set<basic_pid_type> pid_suback_;
    std::set<basic_pid_type> pid_unsuback_;
    std::set<basic_pid_type> pid_puback_;
    std::set<basic_pid_type> pid_pubrec_;
    std::set<basic_pid_type> pid_pubcomp_;

    bool need_store_ = false;
    store<PacketIdBytes> store_;

    bool offline_publish_ = false;
    bool auto_pub_response_ = false;
    bool auto_ping_response_ = false;

    bool auto_map_topic_alias_send_ = false;
    bool auto_replace_topic_alias_send_ = false;
    std::optional<topic_alias_send> topic_alias_send_;
    std::optional<topic_alias_recv> topic_alias_recv_;

    receive_maximum_type publish_send_max_{receive_maximum_max};
    receive_maximum_type publish_recv_max_{receive_maximum_max};
    receive_maximum_type publish_send_count_{0};

    std::set<basic_pid_type> publish_recv_;

    std::uint32_t maximum_packet_size_send_{packet_size_no_limit};
    std::uint32_t maximum_packet_size_recv_{packet_size_no_limit};

    connection_status status_{connection_status::disconnected};

    std::optional<std::chrono::milliseconds> pingreq_send_interval_ms_;
    std::optional<std::chrono::milliseconds> pingreq_recv_timeout_ms_;
    std::optional<std::chrono::milliseconds> pingresp_recv_timeout_ms_;

    std::set<basic_pid_type> qos2_publish_handled_;
    std::set<basic_pid_type> qos2_publish_processing_;

    class recv_packet_builder;
    recv_packet_builder rpb_;
    bool is_client_ = true;
};

} // namespace async_mqtt::detail

#endif // ASYNC_MQTT_PROTOCOL_DETAIL_CONNECTION_IMPL_HPP