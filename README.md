# 🤖 4-DOF Robotic Arm Control System

## 💡 Ý tưởng
Dự án nhằm xây dựng một hệ thống điều khiển **cánh tay robot 4 bậc tự do** dựa trên **nhận diện cử chỉ tay và giọng nói**.  
Thay vì sử dụng tay cầm điều khiển, người dùng có thể:
- Ra **cử chỉ bằng tay** trước camera laptop.  
- Ra **lệnh bằng giọng nói** qua micro.  

Server sẽ nhận diện tín hiệu này, phân giải thành lệnh điều khiển, và gửi qua **UART** tới robot. Robot thực hiện các lệnh di chuyển tương ứng thông qua các servo.

---

## 🏗️ Kiến trúc hệ thống

[Camera Laptop] --> [Gesture Recognition (Python, pretrained model)]
[Microphone] --> [Speech Recognition (Vosk)]
│
▼
[Server Python]
(Xử lý tín hiệu, quản lý Queue UART, gửi gói tin)
│
▼
[ESP32 Robot Controller]
(Nhận UART packet, parse dữ liệu, điều khiển 4 Servo)
│
▼
[4-DOF Robotic Arm]

less
Sao chép
Chỉnh sửa

- **Server (Python)**:  
  - Module `gesture`: nhận diện 8 cử chỉ chính.  
    - Mỗi cử chỉ ánh xạ thành lệnh cho một khớp (2 cử chỉ/khớp).  
  - Module `vosk`: nhận diện giọng nói (chọn chế độ, xác nhận lệnh).  
  - Quản lý queue để đảm bảo các gói tin UART gửi xuống robot không bị nghẽn hoặc trùng lệnh.  

- **Robot (ESP32)**:  
  - Nhận gói tin UART dạng struct.  
  - Parse dữ liệu thành servo_id, góc và step delay.  
  - Điều khiển servo di chuyển theo lệnh, có cơ chế step-by-step để chuyển động mượt.

---

## 📂 Thiết kế file

### 1. Server (Python)
/server
├── main.py # Điểm vào chính, quản lý vòng lặp nhận diện
├── gesture.py # Module nhận diện cử chỉ (pretrained model)
├── speech.py # Nhận diện giọng nói với Vosk
├── uart_queue.py # Quản lý hàng đợi UART, tránh nghẽn tín hiệu
├── signal_mapper.py # Ánh xạ cử chỉ/giọng nói -> gói tin UART
└── utils.py # Hàm phụ trợ (logging, config, ...)

shell
Sao chép
Chỉnh sửa

### 2. Robot Controller (ESP32, ESP-IDF)
/robot-arm
├── main/
│ ├── main.c # Điểm vào chính
│ ├── uart_comm.c # Nhận dữ liệu từ UART, push vào queue nội bộ
│ ├── uart_comm.h
│ ├── packet_parser.c # Giải mã gói tin thành struct
│ ├── packet_parser.h
│ ├── servo_driver.c # Điều khiển servo (PWM)
│ ├── servo_driver.h
│ ├── motion.c # Hàm thực hiện chuyển động step-by-step
│ ├── motion.h
│ └── config.h # Cấu hình GPIO, UART, log
├── CMakeLists.txt
└── sdkconfig

arduino
Sao chép
Chỉnh sửa

---

## 🔗 Giao tiếp Server ↔ Robot

### Định dạng gói tin UART
```c
typedef struct {
    servo_id_t servo_id[4];    // ID 4 servo
    int angle[4];              // Góc mong muốn
    int step_delay_ms[4];      // Độ trễ từng bước (chuyển động mượt)
    int intensity;             // Cường độ (áp dụng cho tốc độ/tầm chuyển động)
} uart_packet_t;
```
## Cơ chế Queue
### Server side:

Khi nhận diện được một cử chỉ, nó không gửi ngay lập tức mà push vào queue.

Một tiến trình nền đọc queue, đảm bảo chỉ một gói tin được gửi tại một thời điểm.

Tránh tình trạng buffer overflow trên UART khi cử chỉ liên tục.

### Robot side:

Nhận gói tin từ UART, parse vào struct.

Đẩy lệnh vào queue xử lý servo nội bộ.

Mỗi servo chỉ nhận lệnh sau khi lệnh trước đó hoàn tất.

## ⚙️ Cách thức hoạt động
### Nhận diện

Người dùng thực hiện cử chỉ tay trước camera → gesture.py nhận diện → ánh xạ thành servo_id và hướng (tăng/giảm góc).

Hoặc người dùng ra lệnh giọng nói → speech.py (Vosk) nhận diện → xác định loại hành động cần làm.

### Phân tích & ánh xạ

signal_mapper.py kết hợp dữ liệu gesture/voice → xây dựng gói tin uart_packet_t.

Đưa gói tin vào UART Queue để chờ gửi.

### Truyền qua UART

uart_queue.py lấy gói tin ra, gửi tuần tự qua UART.

Đảm bảo tốc độ truyền ổn định, tránh nghẽn.

### Xử lý trên Robot

uart_comm.c nhận dữ liệu → đẩy vào queue nội bộ.

packet_parser.c parse dữ liệu → tạo command servo.

### motion.c:

Di chuyển servo theo step delay thay vì nhảy góc ngay → chuyển động mượt.

Tôn trọng intensity → nếu intensity cao, góc tăng nhanh hơn và step delay giảm.

### Servo thực hiện

servo_driver.c gửi PWM tới servo tương ứng.

Robot di chuyển đúng theo ý nghĩa của cử chỉ hoặc giọng nói.

# Đặc điểm hàm điều khiển chuyển động
motion_step(): di chuyển servo theo từng bước nhỏ.

motion_to_angle(): tính toán số bước cần để tới góc đích.

apply_intensity(): thay đổi tốc độ/góc dựa theo tín hiệu cường độ.

smooth_move(): đảm bảo servo không giật cục, mô phỏng chuyển động “người thật”.

# Kết quả mong đợi
Người dùng có thể điều khiển cánh tay robot chỉ bằng cử chỉ tay hoặc giọng nói.

Hệ thống queue UART đảm bảo độ tin cậy khi tín hiệu gửi liên tục.

Robot thực hiện chuyển động mượt mà, an toàn cho các khớp servo.

