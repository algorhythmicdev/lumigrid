#!/usr/bin/env python3
import socket
import argparse
import time
import struct
import random

# Message types
MSG_SET_PIXEL = 0x01
MSG_SET_RANGE = 0x02
MSG_SET_ALL = 0x03
MSG_SHOW = 0x04
MSG_CLEAR = 0x05

def send_udp_message(host, port, data):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(data, (host, port))
    sock.close()

def create_header(msg_type, length):
    return struct.pack('<BH', msg_type, length)

def set_pixel(index, r, g, b):
    data = create_header(MSG_SET_PIXEL, 5)
    data += struct.pack('<HB', index, r)
    data += struct.pack('BB', g, b)
    return data

def set_all(r, g, b):
    data = create_header(MSG_SET_ALL, 3)
    data += struct.pack('BBB', r, g, b)
    return data

def show():
    return create_header(MSG_SHOW, 0)

def clear():
    return create_header(MSG_CLEAR, 0)

def main():
    parser = argparse.ArgumentParser(description='Send test frames to LumiGrid controller')
    parser.add_argument('--host', default='192.168.4.1', help='Controller IP address')
    parser.add_argument('--port', type=int, default=4210, help='UDP port')
    parser.add_argument('--mode', choices=['rainbow', 'chase', 'solid'], default='rainbow',
                      help='Animation mode')
    parser.add_argument('--count', type=int, default=100, help='Number of LEDs')
    
    args = parser.parse_args()
    
    try:
        if args.mode == 'solid':
            # Set all LEDs to red
            send_udp_message(args.host, args.port, set_all(255, 0, 0))
            send_udp_message(args.host, args.port, show())
            
        elif args.mode == 'chase':
            # Single LED chasing animation
            for i in range(50):
                pos = i % args.count
                send_udp_message(args.host, args.port, clear())
                send_udp_message(args.host, args.port, set_pixel(pos, 0, 255, 0))
                send_udp_message(args.host, args.port, show())
                time.sleep(0.05)
                
        elif args.mode == 'rainbow':
            # Rainbow pattern
            for i in range(args.count):
                hue = (i * 255) // args.count
                r, g, b = hsv_to_rgb(hue/255.0, 1.0, 1.0)
                send_udp_message(args.host, args.port, 
                                set_pixel(i, int(r*255), int(g*255), int(b*255)))
                
            send_udp_message(args.host, args.port, show())
    
    except KeyboardInterrupt:
        print("Stopping...")
        send_udp_message(args.host, args.port, clear())
        send_udp_message(args.host, args.port, show())

def hsv_to_rgb(h, s, v):
    if s == 0.0:
        return v, v, v
    
    i = int(h*6)
    f = (h*6) - i
    p = v * (1-s)
    q = v * (1-s*f)
    t = v * (1-s*(1-f))
    
    i = i % 6
    if i == 0: return v, t, p
    if i == 1: return q, v, p
    if i == 2: return p, v, t
    if i == 3: return p, q, v
    if i == 4: return t, p, v
    if i == 5: return v, p, q

if __name__ == "__main__":
    main()
