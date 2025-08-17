import cv2
import mediapipe as mp
import math
import struct 
import serial

ser = serial.Serial('COM5',baudrate= 115200, timeout=1) 

mp_hands = mp.solutions.hands
mp_drawing = mp.solutions.drawing_utils

hand_gesture = ["None", "Base right", "Base left", "Shoulder up", "Shoulder down", "Elbow up", "Elbow down","Wrist open","Wrist close"]

# Khởi tạo hand detector
hands = mp_hands.Hands(
    static_image_mode=False,
    max_num_hands=2,  # nhận tối đa 2 tay
    min_detection_confidence=0.7,
    min_tracking_confidence=0.7
)

def uart_send_packet(gesture_id, intensity, ser):
    gesture_type = gesture_id // 2
    gesture_direct = gesture_id % 2
    packet = ((gesture_type & 0x03) << 4) | ((intensity & 0x0C) << 1) | (gesture_direct & 0x01)
    ser.write(bytes([packet]))
    print(f"Gửi byte: 0x{packet:02X} (gesture_type={gesture_type}, gesture_direct={gesture_direct}, intensity={intensity})")


# Hàm tính khoảng cách Euclide
def euclidean_distance(p1, p2):
    return math.sqrt((p1.x - p2.x)**2 + (p1.y - p2.y)**2)

# Hàm rule-based phân loại cử chỉ
def classify_gesture(landmarks):
    # landmarks là 21 keypoints
    # rule đơn giản: dựa trên vị trí tip và pip của ngón tay
    
    finger_tips = [8, 12, 16, 20]   # trỏ, giữa, áp út, út
    finger_pips = [6, 10, 14, 18]
    
    fingers = []
    for tip, pip in zip(finger_tips, finger_pips):
        if landmarks[tip].y < landmarks[pip].y:  # ngón duỗi
            fingers.append(1)
        else:
            fingers.append(0)
    
    # Ngón cái check theo trục x
    if landmarks[4].x < landmarks[3].x:
        thumb = 1
    else:
        thumb = 0
    
    # Tạo vector [thumb, index, middle, ring, pinky]
    gesture = [thumb] + fingers

    # Map gesture cơ bản
    if gesture == [0,1,0,0,0]:
        return 1
    elif gesture == [0,1,0,0,1]:
        return 2
    elif gesture == [1,0,0,0,0]:
        return 3
    elif gesture == [1,0,0,0,1]:
        return 4
    elif gesture == [0,0,1,0,0]:
        return 5
    elif gesture == [0,0,1,0,1]:
        return 6
    elif gesture == [1,1,1,1,1]:
        return 7
    elif gesture == [0,0,0,0,0]:
        return 8
    else:
        return 0

cap = cv2.VideoCapture(0)

packet=[0,0]

while True:
    ret, frame = cap.read()
    if not ret:
        break
    
    frame = cv2.flip(frame, 1)
    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    result = hands.process(rgb)
    
    gestures = []
    intensity = None
    
    
    if result.multi_hand_landmarks and result.multi_handedness:
        for idx, hand_landmarks in enumerate(result.multi_hand_landmarks):
            label = result.multi_handedness[idx].classification[0].label  # "Left" / "Right"
            
            mp_drawing.draw_landmarks(frame, hand_landmarks, mp_hands.HAND_CONNECTIONS)
            landmarks = hand_landmarks.landmark
            
            if label == "Left":  # Tay trái để phân loại cử chỉ
                g = classify_gesture(landmarks)
                gestures.append(("Left", g , hand_gesture[g]))
                packet.append(g)
            
            elif label == "Right":  # Tay phải để đo khoảng cách intensity
                thumb_tip = landmarks[4]
                index_tip = landmarks[8]
                dist = euclidean_distance(thumb_tip, index_tip)
                intensity = dist  # raw value, có thể scale
                gestures.append(("Right", f"Intensity={dist:.2f}"))
                packet.append(int(dist * 10))  # scale to 0-100 for transmission
    
    # Hiển thị kết quả
    y = 30
    for g in gestures:
        cv2.putText(frame, f"{g[0]}: {g[1]}", (10, y), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0,255,0), 2)
        print(g[0], g[1],intensity)
        
        if g[0] == "Left":
            gesture_id = g[1]
            intensity_val = int(intensity * 10) if intensity is not None else 0
            if gesture_id !=0 and intensity_val != 0:
                uart_send_packet(gesture_id, intensity_val, ser)
        y += 30
        
   


    if intensity is not None:
        cv2.putText(frame, f"Intensity (scaled): {intensity*100:.1f}", (10, y), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0,0,255), 2)
    
    cv2.imshow("Two-Hand Gesture Control", frame)
    
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
