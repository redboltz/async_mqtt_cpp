# Topic Alias
Topic Alias is a way to reduce PUBLISH packet size.

## Notifying capacity
There are two independent Topic Alias capacities. 

### broker to client Topic Alias
The client can set `Topic Alias Maximum` property that value is greater than 0 to the CONNECT packet. This means the client can receive the PUBLISH packet with `Topic Alias` property that the value is less than or equal to `Topic Alias Maximum`. The broker could send the PUBLISH packet using `Topic Alias` property.
If the broker doesn't receive CONNECT packet with `Topic Alias Maximum` property that value is greater than 0, then the broker cannot use `Topic Alias`.

### client to broker Topic Alias
The broker can set `Topic Alias Maximum` property that value is greater than 0 to the CONNACK packet. This means the broker can receive the PUBLISH packet with `Topic Alias` property that the value is less than or equal to `Topic Alias Maximum`. The client could send the PUBLISH packet using `Topic Alias` property.
If the client doesn't receive CONNACK packet with `Topic Alias Maximum` property that value is greater than 0, then the client cannot use `Topic Alias`.

## Using Topic Alias
### Register/Overwrite
When you set TopicName and `Topic Alias` property to the PUBLISH packet, then the mapping is registered. If the `Topic Alias` is already mapped, then the mapping is overwritten.

### Use
When you set empty (zero length) TopicName and `Topic Alias` property to the PUBLISH packet, then the receiver needs to extract the TopicName that is corresponding to the `Topic Alias`.
In this case, the packet size is usually reduced. (Especially the TopicName is long).

## async_mqtt support
### setup
If you are using the async_mqtt as the client, all you need to do is set `Topic Alias Maximum` property to CONNECT packet.
If you are using the async_mqtt as the server(broker), all you need to do is set `Topic Alias Maximum` property to CONNACK packet.

Then mapping functionality is automatically set up.

### set_auto_map_topic_alias_send(bool)
When you set this function is called with argument true, then `Topic Alias` is automatically allocated and used when you send PUBLISH packet. If you run out all `Topic Alias`, the oldest mapping is automatically replaced. It is called LRU algorithm.

### set_replace_map_topic_alias_send(bool)
When you set this function is called with argument true, then `Topic Alias` is automatically used if the mapping is registered when you send PUBLISH packet.

### manual use
You can register/use Topic Alias manually by setting PUBLISH packet. This works well with above two automatic functionalities.

## Pitfall
async_mqtt has already solved this problem. This is implementation note.
If client/broker [keep the session](keep_session.md), the halfway QoS1 and QoS2 PUBLISH packet should be resent just after reconnection. What happened if the PUBLISH packet **uses** Topic Alias? In this case, the TopicName is empty. The counter part `Topic Alias Maximum` could be reduced (or removed) on reconnection. MQTT spec said that the lifetime of Topic Alias mapping should end on disconnect. In other words, the lifetime of Topic Alias mapping is the same as the lifetime of connection, **not the lifetime of the session**.
So, the client/broker sends PUBLISH packet with empty length TopicName and `Topic Alias` property on just after reconnection, it is protocol violation.

In order to solve the problem, the client/broker needs to extract TopicName from the `Topic Alias` property **on sending** and create the new PUBLISH packet for storing that is contained from the extracted TopicName and remove `Topic Alias` property form the sending PUBLISH packet. When resending, use the stored (non Topic Aliased) packet.

This process is automatically done by async_mqtt internally. Users don't need to care about this issue.
