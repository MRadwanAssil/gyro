import os
import sys
import serial
import pygame
import warnings

from pygame.locals import *
from OpenGL.GL import *
from OpenGL.GLU import *

import math

os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = "hide"
warnings.filterwarnings("ignore", category=RuntimeWarning)

# =========================
# SERIAL
# =========================
PORT = "/dev/cu.usbserial-0001"
BAUD = 115200

# =========================
# CUBE GEOMETRY
# =========================
vertices = (
    (1, -1, -1), (1, 1, -1), (-1, 1, -1), (-1, -1, -1),
    (1, -1, 1), (1, 1, 1), (-1, -1, 1), (-1, 1, 1)
)

edges = (
    (0,1), (1,2), (2,3), (3,0),
    (4,5), (5,7), (7,6), (6,4),
    (0,4), (1,5), (2,7), (3,6)
)

surfaces = (
    (0,1,2,3), (3,2,7,6), (6,7,5,4),
    (4,5,1,0), (1,5,7,2), (4,0,3,6)
)

colors = (
    (0.1,0.6,0.8), (0.9,0.3,0.3), (0.2,0.8,0.2),
    (0.9,0.7,0.1), (0.6,0.2,0.8), (0.2,0.2,0.2)
)

def draw_cube():
    glBegin(GL_QUADS)
    for i, surface in enumerate(surfaces):
        glColor3fv(colors[i])
        for v in surface:
            glVertex3fv(vertices[v])
    glEnd()

    glBegin(GL_LINES)
    glColor3fv((1,1,1))
    for e in edges:
        for v in e:
            glVertex3fv(vertices[v])
    glEnd()


# =========================
# PARSE ONLY READY DATA
# =========================
def parse(line):
    try:
        text = line.decode(errors="ignore").strip()

        parts = text.split("|")
        data = {}

        for p in parts:
            if ":" in p:
                k, v = p.split(":")
                data[k.strip()] = float(v.strip())

        # 🚨 SADECE HAZIR DATA
        if "pitch" in data and "roll" in data and "yaw" in data:
            return data["roll"], data["pitch"], data["yaw"]

    except:
        pass

    return None


# =========================
# MAIN
# =========================
def main():
    pygame.init()
    display = (800, 600)
    pygame.display.set_mode(display, DOUBLEBUF | OPENGL)

    gluPerspective(45, display[0]/display[1], 0.1, 50.0)
    glTranslatef(0, 0, -5)

    ser = serial.Serial(PORT, BAUD, timeout=0.001)

    current_roll = 0
    current_pitch = 0
    current_yaw = 0

    SMOOTH = 0.08

    clock = pygame.time.Clock()

    while True:
        dt = clock.tick(60) / 1000.0

        for e in pygame.event.get():
            if e.type == pygame.QUIT:
                ser.close()
                pygame.quit()
                sys.exit()

        last = None
        while ser.in_waiting > 0:
            last = ser.readline()

        if last:
            parsed = parse(last)
            if parsed:
                target_roll, target_pitch, target_yaw = parsed

                # 🔥 SMOOTH (tek yaptığımız şey bu)
                current_roll  += (target_roll  - current_roll) * SMOOTH
                current_pitch += (target_pitch - current_pitch) * SMOOTH
                current_yaw   += (target_yaw   - current_yaw) * SMOOTH

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glEnable(GL_DEPTH_TEST)

        glPushMatrix()

        # ⚡ DIRECT ANGLES
        glRotatef(current_yaw,   0, 1, 0)
        glRotatef(current_roll,  1, 0, 0)
        glRotatef(current_pitch, 0, 0, 1)

        draw_cube()

        glPopMatrix()

        pygame.display.flip()


if __name__ == "__main__":
    main()z