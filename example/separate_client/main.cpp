// Copyright Takatoshi Kondo 2023
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <string>

#include <boost/asio.hpp>

#include <async_mqtt/all.hpp>

namespace as = boost::asio;
namespace am = async_mqtt;

using client_t = am::client<am::protocol_version::v5, am::protocol::mqtt>;

as::awaitable<void>
proc(
    client_t& amcl,
    std::string_view host,
    std::string_view port) {

    auto exe = co_await as::this_coro::executor;

    std::cout << "start" << std::endl;

    try {
        // all underlying layer handshaking
        // (Resolve hostname, TCP handshake)
        co_await am::async_underlying_handshake(amcl.next_layer(), host, port, as::use_awaitable);

        std::cout << "mqtt undlerlying handshaked" << std::endl;

        // prepare will message if you need.
        am::will will{
            "WillTopic1",
            "WillMessage1",
            am::qos::at_most_once,
            { // properties
                am::property::user_property{"key1", "val1"},
                am::property::content_type{"text"},
            }
        };

        // MQTT connect and receive loop start
        auto connack_opt = co_await amcl.async_start(
            am::v5::connect_packet{
                true,   // clean_start
                0x1234, // keep_alive
                "",     // Client Identifier, empty means generated by the broker
                will,   // you can pass std::nullopt if you don't want to set the will message
                "UserName1",
                "Password1"
            },
            as::use_awaitable
        );
        if (connack_opt) {
            std::cout << *connack_opt << std::endl;
        }

        // subscribe
        // MQTT send subscribe and wait suback
        std::vector<am::topic_subopts> sub_entry{
            {"topic1", am::qos::at_most_once},
            {"topic2", am::qos::at_least_once},
            {"topic3", am::qos::exactly_once},
        };
        auto suback_opt = co_await amcl.async_subscribe(
            am::v5::subscribe_packet{
                *amcl.acquire_unique_packet_id(), // sync version only works thread safe context
                am::force_move(sub_entry)
            },
            as::use_awaitable
        );
        if (suback_opt) {
            std::cout << *suback_opt << std::endl;
        }
        auto print_pubres =
            [](client_t::pubres_type const& pubres) {
                if (pubres.puback_opt) {
                    std::cout << *pubres.puback_opt << std::endl;
                }
                if (pubres.pubrec_opt) {
                    std::cout << *pubres.pubrec_opt << std::endl;
                }
                if (pubres.pubcomp_opt) {
                    std::cout << *pubres.pubcomp_opt << std::endl;
                }
            };

        // publish
        // MQTT publish QoS0 and wait response (socket write complete)
        auto pubres0 = co_await amcl.async_publish(
            am::v5::publish_packet{
                "topic1",
                "payload1",
                am::qos::at_most_once
            },
            as::use_awaitable
        );
        print_pubres(pubres0);

        // MQTT publish QoS1 and wait response (puback receive)
        auto pid_pub1 = co_await amcl.async_acquire_unique_packet_id(as::use_awaitable); // async version
        auto pubres1 = co_await amcl.async_publish(
            // you can pass v5::publish_packet constructor's args directly
            pid_pub1,
            "topic2",
            "payload2",
            am::qos::at_least_once,
            as::use_awaitable
        );
        print_pubres(pubres1);

        // recv (coroutine)
        for (int i = 0; i != 2; ++i) {
            auto pv = co_await amcl.async_recv(as::use_awaitable);
            pv.visit(
                am::overload{
                    [&](client_t::publish_packet& p) {
                        std::cout << p << std::endl;
                        std::cout << "topic   : " << p.topic() << std::endl;
                        std::cout << "payload : " << p.payload() << std::endl;
                    },
                    [&](client_t::disconnect_packet& p) {
                        std::cout << p << std::endl;
                    },
                    [](auto&) {
                    }
                }
            );
        }
        // recv (callback) before sending
        amcl.async_recv(
            [] (am::error_code ec, am::packet_variant pv) {
                std::cout << ec.message() << std::endl;
                pv.visit(
                    am::overload{
                        [&](client_t::publish_packet& p) {
                            std::cout << p << std::endl;
                            std::cout << "topic   : " << p.topic() << std::endl;
                            std::cout << "payload : " << p.payload() << std::endl;
                        },
                        [&](client_t::disconnect_packet& p) {
                            std::cout << p << std::endl;
                        },
                        [](auto&) {
                        }
                    }
                );
            }
        );

        // MQTT publish QoS2 and wait response (pubrec, pubcomp receive)
        auto pid_pub2 = co_await amcl.async_acquire_unique_packet_id_wait_until(as::use_awaitable); // async version
        auto pubres2 = co_await amcl.async_publish(
            am::v5::publish_packet{
                pid_pub2,
                "topic3",
                "payload3",
                am::qos::exactly_once
            },
            as::use_awaitable
        );
        print_pubres(pubres2);

        // MQTT send unsubscribe and wait unsuback
        std::vector<am::topic_sharename> unsub_entry{
            "topic1",
            "topic2",
            "topic3",
        };

        auto unsuback_opt = co_await amcl.async_unsubscribe(
            am::v5::unsubscribe_packet{
                *amcl.acquire_unique_packet_id(), // sync version only works thread safe context
                am::force_move(unsub_entry)
            },
            as::use_awaitable
        );
        if (unsuback_opt) {
            std::cout << *unsuback_opt << std::endl;
        }

        // disconnect
        co_await amcl.async_disconnect(
            am::v5::disconnect_packet{
                am::disconnect_reason_code::disconnect_with_will_message
            },
            as::use_awaitable
        );
        std::cout << "finished" << std::endl;
    }
    catch (boost::system::system_error const& se) {
        std::cout << se.code().message() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    am::setup_log(am::severity_level::warning);
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " host port" << std::endl;
        return -1;
    }
    as::io_context ioc;
    auto amcl = client_t{ioc.get_executor()};
    as::co_spawn(amcl.get_executor(), proc(amcl, argv[1], argv[2]), as::detached);
    ioc.run();
}
