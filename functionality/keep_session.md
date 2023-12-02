# Keep session
MQTT has the concept "session". See https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901230
It contains subscription information and halfway PUBLISH (QoS1 and QoS2 only) and PUBREL packets.
async_mqtt automatically configured when the client sends CONNECT packet and the broker receives CONNECT packet. All you need to do is setup CONNECT packet and send it.

## MQTT v3.1.1
session is controlled by clean_session flag of CONNECT packet.
Note: the broker could expire the session by implement limitation.
### CONNECT with clean_session 0
#### the broker has the previous session
- The broker returns CONNACK with session_present 1.
- The broker sends halfway PUBLISH and PUBREL after CONNACK is sent.
- The client sends halfway PUBLISH and PUBREL after CONNACK is received.
  - Note: MQTT v5.0 spec said that the client can send packets before CONNACK is received but CONNACK could be error, so waiting CONNACK is reasonable design choice.
- Both the client and the broker store session during the connection and after disconnection.
#### the broker doesn't have the previous session
- The broker returns CONNACK with session_present 0.
- The client clears halfway PUBLISH and PUBREL after CONNACK is received.
  - Note: the halfway packets are for the previous session. If the previous session doesn't exist, the client side halfway packets should be cleared.
- Both the client and the broker store session during the connection and after disconnection.
### CONNECT with clean_session 1
- Both the client and the broker session are cleared.
- The broker returns CONNACK with session_present 0.
- Both the client and the broker don't store session during the connection and after disconnection.

## MQTT v5.0
session is controlled by clean_start flag and Session Expiry Interval property of CONNECT packet.
Note: the broker could expire the session by implement limitation.

### clean_start
#### CONNECT with clean_start 0
##### the broker has the previous session
- The broker returns CONNACK with session_present 1.
- The broker sends halfway PUBLISH and PUBREL after CONNACK is sent.
- The client sends halfway PUBLISH and PUBREL after CONNACK is received.
  - Note: MQTT v5.0 spec said that the client can send packets before CONNACK is received but CONNACK could be error, so waiting CONNACK is reasonable design choice.
##### the broker doesn't have the previous session
- The broker returns CONNACK with session_present 0.
- The client clears halfway PUBLISH and PUBREL after CONNACK is received.
  - Note: the halfway packets are for the previous session. If the previous session doesn't exist, the client side halfway packets should be cleared.
#### CONNECT with clean_start 1
- Both the client and the broker session are cleared.
- The broker returns CONNACK with session_present 0.
### Session Expiry Interval
#### CONNECT with Session Expiry Interval is greater than 0
- Both the client and the broker store session during the connection and after disconnection until the `Session Expiry Interval` seconds passed.

#### CONNECT without Session Expiry Interval is greater than 0
- Both the client and the broker don't store session during the connection and after disconnection.
