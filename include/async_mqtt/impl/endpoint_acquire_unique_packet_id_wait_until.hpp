// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_IMPL_ENDPOINT_ACQUIRE_UNIQUE_PACKET_ID_WAIT_UNTIL_HPP)
#define ASYNC_MQTT_IMPL_ENDPOINT_ACQUIRE_UNIQUE_PACKET_ID_WAIT_UNTIL_HPP

#include <async_mqtt/impl/endpoint_impl.hpp>

namespace async_mqtt {

namespace detail {

template <role Role, std::size_t PacketIdBytes, typename NextLayer>
struct basic_endpoint_impl<Role, PacketIdBytes, NextLayer>::
acquire_unique_packet_id_wait_until_op {
    this_type_sp ep;
    this_type_wp retry_wp{ep};
    std::optional<typename basic_packet_id_type<PacketIdBytes>::type> pid_opt = std::nullopt;
    enum { dispatch, complete } state = dispatch;

    template <typename Self>
    void operator()(
        Self& self,
        error_code ec = error_code{}
    ) {
        auto& a_ep{*ep};
        if (retry_wp.expired()) return;
        switch (state) {
        case dispatch: {
            state = complete;
            as::dispatch(
                a_ep.get_executor(),
                force_move(self)
            );
        } break;
        case complete: {
            auto acq_proc =
                [&] {
                    pid_opt = a_ep.con_.acquire_unique_packet_id();
                    if (pid_opt) {
                        self.complete(error_code{}, *pid_opt);
                    }
                    else {
                        ASYNC_MQTT_LOG("mqtt_impl", warning)
                            << ASYNC_MQTT_ADD_VALUE(address, &a_ep)
                            << "packet_id is fully allocated. waiting release";
                        a_ep.packet_id_released_ = false;
                        // infinity timer. cancel is retry trigger.
                        auto ep_copy = ep;
                        async_add_retry(
                            force_move(ep_copy),
                            force_move(self)
                        );
                    }
                };

            if (ec == as::error::operation_aborted) {
                if (a_ep.packet_id_released_) {
                    a_ep.complete_retry_one();
                    acq_proc();
                }
                else {
                    self.complete(
                        ec,
                        0
                    );
                }
            }
            else if (a_ep.has_retry()) {
                ASYNC_MQTT_LOG("mqtt_impl", warning)
                    << ASYNC_MQTT_ADD_VALUE(address, &a_ep)
                    << "packet_id waiter exists. add the end of waiter queue";
                // infinity timer. cancel is retry trigger.
                auto ep_copy = ep;
                async_add_retry(
                    force_move(ep_copy),
                    force_move(self)
                );
            }
            else {
                acq_proc();
            }
        } break;
        }
    }
};

} // namespace detail

template <role Role, std::size_t PacketIdBytes, typename NextLayer>
template <typename CompletionToken>
auto
basic_endpoint<Role, PacketIdBytes, NextLayer>::async_acquire_unique_packet_id_wait_until(
    CompletionToken&& token
) {
    ASYNC_MQTT_LOG("mqtt_api", info)
        << ASYNC_MQTT_ADD_VALUE(address, this)
        << "acquire_unique_packet_id_wait_until";
    BOOST_ASSERT(impl_);
    return
        as::async_compose<
            CompletionToken,
            void(error_code, typename basic_packet_id_type<PacketIdBytes>::type)
        >(
            typename impl_type::acquire_unique_packet_id_wait_until_op{
                impl_
            },
            token,
            get_executor()
        );
}

} // namespace async_mqtt

#endif // ASYNC_MQTT_IMPL_ENDPOINT_ACQUIRE_UNIQUE_PACKET_ID_WAIT_UNTIL_HPP
