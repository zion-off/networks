# TCP Retransmission Timer and Congestion Control

## Project Overview

The goal of this project is to implement an adaptive retransmission timer and congestion control mechanisms for a TCP protocol using C. This project demonstrates experience with TCP internals, focusing on retransmission timeout (RTO) calculation using RTT estimation, Karn's algorithm, exponential backoff, and dynamic congestion window size (CWND) management using Slow Start, Congestion Avoidance, and Fast Retransmit.

## Features

### Retransmission Timer

#### RTT Estimation
- **RTT Measurement**: Measures the Round Trip Time (RTT) for each packet by recording the timestamp when a packet is sent and when its acknowledgment (ACK) is received.
- **RTT Estimator**: Uses the collected RTT measurements to calculate a running average (either weighted or exponential moving average) to estimate the RTT.
- **RTO Calculation**: Sets the RTO as a multiple of the estimated RTT (e.g., 2 times the estimated RTT).

#### Karn's Algorithm
- **Avoids Retransmitted Packet RTTs**: Maintains a record of retransmitted packets and skips their RTT measurements when updating the RTT estimator.

#### Exponential Backoff
- **Initial Values**: Initializes RTT as 0 seconds and RTO as 3 seconds.
- **Timeout Handling**: On a timeout, doubles the RTO for the next retransmission.
- **Upper Bound**: Keeps doubling the RTO until it reaches an upper bound of 240 seconds.

### Congestion Control

#### Dynamic CWND Management
- **CWND Initialization**: Sets CWND to a small value (typically 1 packet) when a new connection is established.
- **Slow Start**: Increases CWND by one packet for each ACK received, doubling the CWND each RTT until a threshold (ssthresh) is reached or packet loss occurs.
- **Congestion Avoidance**: When CWND reaches ssthresh, increases CWND more conservatively (e.g., additively) to avoid congestion.
- **Fast Retransmit**: Detects packet loss before a timeout using duplicate ACKs and retransmits the lost packet immediately.

## Requirements

- C programming language
- POSIX-compliant system (Linux, macOS)
- GCC compiler

### Debug Output

The program provides debug output for each RTT measurement, RTO calculation, CWND adjustment, and congestion control event, similar to:
```
[13/Mar/2022 12:23:06] Sent packet 1, RTT: 50ms, RTO: 100ms, CWND: 1
[13/Mar/2022 12:23:08] Received ACK for packet 1, CWND: 2
[13/Mar/2022 12:23:10] Sent packet 2, RTT: 60ms, RTO: 120ms, CWND: 2
[13/Mar/2022 12:23:12] Timeout occurred, doubling RTO: 240ms
[13/Mar/2022 12:23:14] Detected packet loss, setting ssthresh and entering Congestion Avoidance
```

## Implementation Details

### RTT Estimation and RTO Calculation

- **RTT Measurement**: Record the timestamp when a packet is sent and when its ACK is received.
- **RTT Estimator**: Calculate a running average RTT using an exponential moving average:
  ```c
  estimatedRTT = (1 - alpha) * estimatedRTT + alpha * sampleRTT;
  ```
  where `alpha` is a smoothing factor (e.g., 0.125).
- **RTO Calculation**: Set RTO as:
  ```c
  RTO = 2 * estimatedRTT;
  ```

### Karn's Algorithm

- Maintain a boolean flag for each packet to indicate if it was retransmitted.
- Ignore RTT measurements from retransmitted packets.

### Exponential Backoff

- On timeout, double the RTO:
  ```c
  RTO = min(RTO * 2, 240);
  ```

### Congestion Control

- **CWND Initialization**: Set CWND to 1 packet at the start of a connection.
- **Slow Start**: Double CWND each RTT until reaching ssthresh:
  ```c
  CWND++;
  ```
- **Congestion Avoidance**: After reaching ssthresh, increase CWND additively:
  ```c
  CWND += (1 / CWND);
  ```
- **Fast Retransmit**: Detect packet loss using duplicate ACKs, set ssthresh, and retransmit the lost packet:
  ```c
  if (duplicateACKs == 3) {
      ssthresh = CWND / 2;
      CWND = ssthresh + 3;
      retransmit(lostPacket);
  }
  ```
