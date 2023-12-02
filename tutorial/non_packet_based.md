# Non packet based APIs

Most of async_mqtt APIs are [packet based](send_recv.md#packet-based-apis) so functionalities are automatically applied by simply set packets parameters and send/recv them. However, some of functionalities cannot be explained the packets.
Such functionalities are implemented as non packet based APIs.
Here are the list of non packet based APIs. The functions should be called before send/recv is called.

endpoint member function | Effects
---|---
set_auto_pub_response|If set true, then PUBACK, PUBREC, PUBREL, and PUBCOMP will be sent automatically when the corresponding packet is received.
set_auto_ping_response|If set true, then PINGRESP will be sent automatically when PINGREQ is received.
set_auto_map_topic_alias_send|If set true, TopicAlias is automatically acquired and applied on sending PUBLISH packets. The limit is decidec by received TopicAliasMaximum property. If it is 0, no TopicAlias is used. If TopicAlias is fully used, then overwrite the oldest TopicAlias (LRU algorithm).
set_auto_replace_topic_alias_send|It is similar to set_auto_map_topic_alias but not automatically acquired. So you need to register topicalias by yourself. If set true, then TopicAlias is automatically applied if TopicAlias is already registered.
set_ping_resp_recv_timeout_ms|Set timer after sending PINGREQ packet. The timer would be cancelled when PINGRESP packet is received. If timer is fired then the connection is disconnected automatically.