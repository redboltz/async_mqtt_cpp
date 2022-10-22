// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_WS_FIXED_SIZE_ASYNC_READ_HPP)
#define ASYNC_MQTT_WS_FIXED_SIZE_ASYNC_READ_HPP

#include <utility>
#include <type_traits>

#include <boost/asio/buffer.hpp>
#include <boost/asio/compose.hpp>
#include <boost/system/error_code.hpp>
#include <boost/beast/websocket/stream.hpp>

namespace async_mqtt {

namespace as = boost::asio;
namespace bs = boost::beast;

namespace detail {

template <typename Stream, typename MutableBufferSequence>
struct async_read_impl {
    Stream& stream;
    MutableBufferSequence mb;
    std::size_t received = 0;
    as::executor_work_guard<typename Stream::executor_type> wg{stream.get_executor()};

    template <typename Self>
    void operator()(
        Self& self,
        boost::system::error_code const& ec = boost::system::error_code{},
        std::size_t bytes_transferred = 0
    ) {
        if (ec) {
            self.complete(ec, received);
            return;
        }
        received += bytes_transferred;
        mb += bytes_transferred;
        if (mb.size() == 0) {
            self.complete(ec, received);
        }
        else {
            stream.async_read_some(
                mb,
                std::move(self)
            );
        }
    }
};

} // namespace detail

template <
    typename NextLayer,
    typename MutableBufferSequence,
    typename CompletionToken,
    typename std::enable_if_t<
        as::is_mutable_buffer_sequence<MutableBufferSequence>::value &&
        std::is_invocable<CompletionToken, boost::system::error_code, std::size_t>::value
    >* = nullptr
>
auto async_read(
    bs::websocket::stream<NextLayer>& stream,
    MutableBufferSequence const& mb,
    CompletionToken&& token
) {
    return
        as::async_compose<
            CompletionToken,
        void(boost::system::error_code const&, std::size_t)
    >(
        detail::async_read_impl<bs::websocket::stream<NextLayer>, MutableBufferSequence>{
            stream,
            mb
        },
        token
    );
}

} // namespace async_mqtt

#endif // ASYNC_MQTT_WS_FIXED_SIZE_ASYNC_READ_HPP
