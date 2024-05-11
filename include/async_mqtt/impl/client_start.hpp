// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_IMPL_CLIENT_START_HPP)
#define ASYNC_MQTT_IMPL_CLIENT_START_HPP

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/append.hpp>
#include <boost/asio/compose.hpp>

#include <async_mqtt/impl/client_impl.hpp>
#include <async_mqtt/exception.hpp>
#include <async_mqtt/log.hpp>

namespace async_mqtt {

template <protocol_version Version, typename NextLayer>
struct client<Version, NextLayer>::
start_op {
    this_type& cl;
    connect_packet packet;

    template <typename Self>
    void operator()(
        Self& self
    ) {
        auto& a_cl{cl};
        auto a_packet{packet};
        a_cl.ep_->send(
            force_move(a_packet),
            force_move(self)
        );
    }

    template <typename Self>
    void operator()(
        Self& self,
        system_error const& se
    ) {
        if (se) {
            self.complete(se.code(), std::nullopt);
            return;
        }

        auto tim = std::make_shared<as::steady_timer>(cl.ep_->get_executor());
        tim->expires_at(std::chrono::steady_clock::time_point::max());
        cl.pid_tim_pv_res_col_.get_tim_idx().emplace(tim);
        cl.recv_loop();
        auto& a_cl{cl};
        tim->async_wait(
            as::bind_executor(
                a_cl.get_executor(),
                as::append(
                    force_move(self),
                    tim
                )
            )
        );
    }

    template <typename Self>
    void operator()(
        Self& self,
        error_code /* ec */,
        std::shared_ptr<as::steady_timer> tim
    ) {
        auto& idx = cl.pid_tim_pv_res_col_.get_tim_idx();
        auto it = idx.find(tim);
        if (it == idx.end()) {
            self.complete(
                errc::make_error_code(sys::errc::operation_canceled),
                std::nullopt
            );
        }
        else {
            auto pv = it->pv;
            idx.erase(it);
            if (auto *p = pv->template get_if<connack_packet>()) {
                self.complete(error_code{}, *p);
            }
            else {
                self.complete(
                    errc::make_error_code(sys::errc::protocol_error),
                    std::nullopt
                );
            }
        }
    }
};


template <protocol_version Version, typename NextLayer>
template <typename CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(
    CompletionToken,
    void(error_code, std::optional<connack_packet>)
)
client<Version, NextLayer>::start(
    connect_packet packet,
    CompletionToken&& token
) {
    ASYNC_MQTT_LOG("mqtt_api", info)
        << ASYNC_MQTT_ADD_VALUE(address, this)
        << "start: " << packet;
    return
        as::async_compose<
            CompletionToken,
            void(error_code, std::optional<connack_packet>)
        >(
            start_op{
                *this,
                force_move(packet)
            },
            token
        );
}

} // namespace async_mqtt

#endif // ASYNC_MQTT_IMPL_CLIENT_START_HPP
