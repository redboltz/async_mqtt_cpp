// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_IMPL_ENDPOINT_ACQUIRE_UNIQUE_PACKET_ID_HPP)
#define ASYNC_MQTT_IMPL_ENDPOINT_ACQUIRE_UNIQUE_PACKET_ID_HPP

#include <async_mqtt/endpoint.hpp>

namespace async_mqtt {

template <role Role, std::size_t PacketIdBytes, typename NextLayer>
struct basic_endpoint<Role, PacketIdBytes, NextLayer>::
acquire_unique_packet_id_op {
    this_type& ep;
    std::optional<packet_id_t> pid_opt = std::nullopt;
    enum { dispatch, complete } state = dispatch;

    template <typename Self>
    void operator()(
        Self& self
    ) {
        switch (state) {
        case dispatch: {
            state = complete;
            auto& a_ep{ep};
            as::dispatch(
                as::bind_executor(
                    a_ep.get_executor(),
                    force_move(self)
                )
            );
        } break;
        case complete:
            pid_opt = ep.pid_man_.acquire_unique_id();
            state = complete;
            self.complete(pid_opt);
            break;
        }
    }
};

template <role Role, std::size_t PacketIdBytes, typename NextLayer>
template <typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(std::optional<packet_id_t>)
)
basic_endpoint<Role, PacketIdBytes, NextLayer>::acquire_unique_packet_id(
    CompletionToken&& token
) {
    ASYNC_MQTT_LOG("mqtt_api", info)
        << ASYNC_MQTT_ADD_VALUE(address, this)
        << "acquire_unique_packet_id";
    return
        as::async_compose<
            CompletionToken,
            void(std::optional<packet_id_t>)
        >(
            acquire_unique_packet_id_op{
                *this
            },
            token
        );
}

// sync version

template <role Role, std::size_t PacketIdBytes, typename NextLayer>
inline
std::optional<typename basic_endpoint<Role, PacketIdBytes, NextLayer>::packet_id_t>
basic_endpoint<Role, PacketIdBytes, NextLayer>::acquire_unique_packet_id() {
    auto pid = pid_man_.acquire_unique_id();
    if (pid) {
        ASYNC_MQTT_LOG("mqtt_api", info)
            << ASYNC_MQTT_ADD_VALUE(address, this)
            << "acquire_unique_packet_id:" << *pid;
    }
    else {
        ASYNC_MQTT_LOG("mqtt_api", info)
            << ASYNC_MQTT_ADD_VALUE(address, this)
            << "acquire_unique_packet_id:full";
    }
    return pid;
}

} // namespace async_mqtt

#endif // ASYNC_MQTT_IMPL_ENDPOINT_ACQUIRE_UNIQUE_PACKET_ID_HPP
