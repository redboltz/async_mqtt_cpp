# Connect Timeout
Before think about connect timeout, we need define what is the connect. Here is typical connect sequence.

1. Resolve hostname (optional)
2. TCP handshake
3. TLS handshake (optional)
4. WebSocket handshake (optional)
5. Send MQTT CONNECT packet
6. Receive MQTT CONNACK packet

NOTE: Step1-4 is outside of async_mqtt. 

Typically, user can set timer before the Step1, and cancel the timer after Step6. If the timer is fired, then call `close()` function.

# PINGRESP timeout
After MQTT connection is established (Step6 is finished successfully), if underlying connection is disconnected, then `recv()` function returns as error. However, if the broker is freezed or disconnect is not detected but acutually disconnected, the client want to know the status of the connection.
In this case, you can use `set_pingresp_recv_timeout_ms()` and send PINGREQ packet.

## set_pingresp_recv_timeout_ms()
The parameter is milliseconds. You need to call this function before sending MQTT CONNECT packet. The timer is automatically set when PINGREQ packet is sent. If PINGRESP packet is received from the broker, then the timer is cancelled. If the timer is fired, the connection is disconnected from the client side. Finally, the client gets error from the `recv()` function.

## sending PINGREQ packet
You can create PINGREQ packet and send it. This is a manual way.
You can set CONNECT packet keep_alive value that is greater than 0, then the client automatically start sending PINGREQ packet if keep_alive seconds passed after any packet is sent.

NOTE: If keep_alive set to greater than 0, the broker also start checking the client connection stateus. If the broker doesn't receive any packet from client during keep_alive * 1.5 seconds, then the connection is closed by the broker.