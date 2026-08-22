# FMCW Radar ACC HILS

MATLAB/Simulink 기반 주행 시뮬레이션 환경과 Raspberry Pi의 FMCW 레이더 신호처리 파이프라인을 연결한 Adaptive Cruise Control(ACC) HILS 프로젝트입니다.

## 프로젝트 개요

본 프로젝트는 MATLAB/Simulink에서 FMCW 레이더 Raw IQ 데이터를 생성하고, 이를 UDP를 통해 Raspberry Pi로 전송한 뒤 C++ 기반 레이더 신호처리를 수행합니다.

Raspberry Pi에서는 
- 거리-도플러 처리
- CFAR 검출 -
- DBSCAN 클러스터링 
- 각도 추정, 
- Lead Target 선택
- 최종 Lead Target 정보를 다시 PC로 전송하여 Simulink의 ACC 제어에 사용합니다.



## ACC 시나리오

### Scenario 1 — 동일 차선 선행 차량

Ego 차량이 동일 차선의 선행 차량을 추종하는 시나리오입니다.

선행 차량은 다음 동작을 수행합니다.

1. 초기 속도로 주행
2. 급감속
3. 일정 시간 정지
4. 다시 가속

레이더에서 계산된 거리와 상대속도 정보가 ACC 제어기에 입력되며, Ego 차량은 선행 차량의 움직임에 맞춰 감속 및 재가속합니다.

### Scenario 2 — 인접 차선 차량

인접 차선에 정지 차량을 배치한 시나리오입니다.

레이더에서는 해당 차량이 검출될 수 있지만, Ego 차선 범위를 벗어나므로 Lead Target 선택 대상에서 제외됩니다.

따라서 인접 차선 차량의 거리 정보는 ACC 제어에 반영되지 않습니다.

## 레이더 설정

| 항목 | 값 |
|---|---:|
| Carrier Frequency | 77 GHz |
| Bandwidth | 150 MHz |
| Chirp Duration | 20 µs |
| Samples / Chirp | 128 |
| Chirps / Frame | 64 |
| Rx Channels | 2 |
| Radar Frame Period | 0.1 s |
| Range Resolution | 약 1.0 m |
| Velocity Resolution | 약 1.52 m/s |

## 레이더 신호처리 파이프라인

```text
Raw IQ
  ↓
Range FFT
  ↓
Doppler FFT
  ↓
Range-Doppler Map
  ↓
CFAR Detection
  ↓
DBSCAN Clustering
  ↓
Peak Detection
  ↓
Angle Estimation
  ↓
Target Conversion
  ↓
Lead Target Selection
```

레이더 신호처리 파이프라인은 Raspberry Pi에서 C++과 FFTW3를 사용하여 수행됩니다.

CFAR 처리 구간에는 OpenMP를 적용하여 처리시간을 단축했습니다.

## Lead Target 선택

ACC 제어에는 Ego 차량과 동일 차선에 존재하는 유효 타깃 중 가장 가까운 차량만 사용합니다.

각 타깃의 횡방향 거리는 다음과 같이 계산합니다.

```text
lateral_distance = distance × sin(angle)
```

설정된 차선 범위를 벗어난 타깃은 Lead Target 후보에서 제외합니다.

Lead Target 출력 구조는 다음과 같습니다.

```cpp
struct LeadTargetFrame
{
    uint32_t frame_id;
    uint8_t valid;
    Target target;
};
```

`Target`에는 다음 정보가 포함됩니다.

```text
distance
relative_velocity
angle
```

## UDP 통신

### PC → Raspberry Pi

Raw IQ 데이터는 UDP 패킷으로 분할되어 Raspberry Pi로 전송됩니다.

```text
PC IP  : 10.0.0.2
RPi IP : 10.0.0.3
Port   : 5000
```

각 UDP 패킷은 다음과 같은 사용자 정의 헤더를 포함합니다.

```text
magic_id
frame_id
packet_id
packet_count
payload_bytes
payload
```

### Raspberry Pi → PC

선택된 Lead Target 정보는 UDP를 통해 PC로 전송되며, Simulink의 ACC 제어 입력으로 사용됩니다.

현재 구현에서는 UDP 포트 `3000`을 사용합니다.

## 멀티스레드 구조

Raspberry Pi 소프트웨어는 각 처리 단계를 독립적인 스레드로 분리하고, ThreadSafeQueue를 통해 데이터를 전달합니다.

```text
PacketReceiver
      ↓
ThreadSafeQueue<RadarPacket>
      ↓
RadarReassembler
      ↓
ThreadSafeQueue<RadarFrame>
      ↓
RadarProcessor
      ↓
ThreadSafeQueue<TargetFrame>
      ↓
TargetSender
```

각 주요 컴포넌트는 pthread 기반으로 동작합니다.

## 의존성

Raspberry Pi 소프트웨어:

- C++17
- CMake
- pthread
- FFTW3f
- OpenMP

PC / 시뮬레이션 환경:

- MATLAB
- Simulink
- 프로젝트 모델에서 사용하는 Automated Driving Toolbox 및 3D Driving Simulation 관련 구성요소

## 빌드

```bash
cmake -S . -B build
cmake --build build
```

실행:

```bash
./build/radar_rpi
```

## 검증

Raspberry Pi의 레이더 신호처리 결과는 MATLAB에서 생성한 기준 데이터와 비교하여 검증했습니다.

주요 테스트 항목은 다음과 같습니다.

- IQ 데이터 크기 검증
- Range-Doppler Map 비교
- Peak Detection 검증
- DBSCAN Clustering 검증

