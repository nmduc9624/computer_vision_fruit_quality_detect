from picamera2 import Picamera2
import cv2
from edge_impulse_linux.image import ImageImpulseRunner
import time
##
import paho.mqtt.client as mqtt

MQTT_BROKER = "192.168.1.194"
MQTT_TOPIC = "esp32/control"

last_sent_time = 0
interval = 0.3

client = mqtt.Client()
client.connect(MQTT_BROKER, 1883, 60)

# =========================
# 1. LOAD MÃ” HÃŒNH EDGE IMPULSE
# =========================
print("Äang táº£i mÃ´ hÃ¬nh fruit_detech-linux-aarch64-v2.eim...")
runner = ImageImpulseRunner("./fruit_detech-linux-aarch64-v2.eim")
model_info = runner.init()

input_width = model_info['model_parameters']['image_input_width']
input_height = model_info['model_parameters']['image_input_height']

print(f"âœ… MÃ´ hÃ¬nh Ä‘Ã£ táº£i - Input size: {input_width}x{input_height}")

# =========================
# 2. Cáº¤U HÃŒNH CAMERA RPICAM (picamera2)
# =========================
picam2 = Picamera2()

# DÃ¹ng XRGB8888 Ä‘á»ƒ OpenCV nháº­n mÃ u á»•n Ä‘á»‹nh hÆ¡n trÃªn Raspberry Pi
picam2.configure(picam2.create_video_configuration(
    main={
        "size": (640, 480),
        "format": "XRGB8888"
    }
))

# Giá»¯ nguyÃªn cÃ¡c control nhÆ° code cÅ©
picam2.set_controls({
    "AwbEnable": True,
    "AeEnable": True,
    "Brightness": 0.0,
    "Contrast": 1.1,
    "Saturation": 1.3,
    "Sharpness": 1.0
})

picam2.start()

# Cho AWB/AE á»•n Ä‘á»‹nh má»™t chÃºt trÆ°á»›c khi xá»­ lÃ½
time.sleep(1.0)

print("âœ… Camera rpicam Ä‘Ã£ má»Ÿ thÃ nh cÃ´ng!")
print("Äang cháº¡y nháº­n diá»‡n... (Nháº¥n phÃ­m 'q' Ä‘á»ƒ thoÃ¡t)")

# =========================
# 3. VÃ’NG Láº¶P CHÃNH - Láº¤Y HÃŒNH + NHáº¬N DIá»†N
# =========================
last_sent = "" ##

try:
    while True:
        # Láº¥y khung hÃ¬nh tá»« rpicam
        frame = picam2.capture_array("main")

        # ====================== FIX MÃ€U (Ráº¤T QUAN TRá»ŒNG) ======================
        # Vá»›i XRGB8888: frame thÆ°á»ng cÃ³ 4 kÃªnh, 3 kÃªnh Ä‘áº§u lÃ  BGR Ä‘á»ƒ dÃ¹ng trá»±c tiáº¿p cho OpenCV
        if frame is None:
            continue

        if len(frame.shape) == 3 and frame.shape[2] == 4:
            frame_bgr = frame[:, :, :3].copy()
        elif len(frame.shape) == 3 and frame.shape[2] == 3:
            frame_bgr = frame.copy()
        else:
            raise RuntimeError(f"Äá»‹nh dáº¡ng frame khÃ´ng há»— trá»£: shape={frame.shape}")

        # Resize vá» kÃ­ch thÆ°á»›c mÃ  mÃ´ hÃ¬nh yÃªu cáº§u
        resized = cv2.resize(frame_bgr, (input_width, input_height))

        # Chuyá»ƒn sang RGB Ä‘á»ƒ gá»­i cho Edge Impulse
        img_for_model = cv2.cvtColor(resized, cv2.COLOR_BGR2RGB)

        # ====================== CHáº Y MÃ” HÃŒNH ======================
        features, _ = runner.get_features_from_image(img_for_model)
        result = runner.classify(features)

        # Láº¥y nhÃ£n cÃ³ xÃ¡c suáº¥t cao nháº¥t
        probs = result['result']['classification']
        label = max(probs, key=probs.get)
        confidence = probs[label]

        text = f"{label} ({confidence:.2f})"
        ##
        if confidence > 0.7:
            now = time.time()
            
            if now - last_sent_time > interval: 
                client.publish(MQTT_TOPIC, label)
                print(f"?? MQTT g?i: {label}")
                last_sent_time = now

        # ====================== HIá»‚N THá»Š Káº¾T QUáº¢ RA MÃ€N HÃŒNH ======================
        display_frame = frame_bgr.copy()

        # Váº½ text káº¿t quáº£
        cv2.putText(
            display_frame,
            text,
            (15, 45),
            cv2.FONT_HERSHEY_SIMPLEX,
            1.3,
            (0, 255, 0),
            3
        )

        # Hiá»ƒn thá»‹ cá»­a sá»•
        cv2.imshow("Fruit Classification - Raspberry Pi 4", display_frame)

        # Nháº¥n 'q' Ä‘á»ƒ thoÃ¡t
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

        # In ra console (dá»… theo dÃµi)
        print(f"{time.strftime('%H:%M:%S')} â†’ {text}")

except KeyboardInterrupt:
    print("\n\nâ›” Dá»«ng bá»Ÿi ngÆ°á»i dÃ¹ng")
except Exception as e:
    print(f"\nâŒ Lá»—i: {e}")
finally:
    # Dá»n dáº¹p tÃ i nguyÃªn
    runner.stop()
    picam2.stop()
    cv2.destroyAllWindows()
    print("âœ… ChÆ°Æ¡ng trÃ¬nh Ä‘Ã£ káº¿t thÃºc sáº¡ch sáº½.")
