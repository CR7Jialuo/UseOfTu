# -*- coding: utf-8 -*-
"""生成应用图标 resources/app.ico（多尺寸），用于 Win32 资源编译。"""
import os

from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "resources")
os.makedirs(OUT, exist_ok=True)

NAVY = (31, 58, 95)
BLUE = (37, 99, 235)
RED = (220, 38, 38)
WHITE = (255, 255, 255)
SOFT = (238, 242, 247)


def draw_icon(size):
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    r = int(size * 0.18)
    # 圆角底：深蓝
    d.rounded_rectangle((0, 0, size - 1, size - 1), radius=r, fill=NAVY)
    # 地图网格线
    step = size / 6.0
    for i in range(1, 6):
        x = int(i * step)
        d.line([(x, int(size * 0.14)), (x, int(size * 0.86))], fill=(255, 255, 255, 28), width=max(1, size // 64))
        y = int(i * step)
        d.line([(int(size * 0.14), y), (int(size * 0.86), y)], fill=(255, 255, 255, 28), width=max(1, size // 64))
    # 路线折线
    pts = [(0.22, 0.74), (0.38, 0.56), (0.55, 0.62), (0.78, 0.30)]
    line = [(int(x * size), int(y * size)) for x, y in pts]
    d.line(line, fill=WHITE, width=max(2, int(size * 0.085)), joint="curve")
    d.line(line, fill=RED, width=max(1, int(size * 0.05)), joint="curve")
    # 起点 / 终点
    r0 = max(2, int(size * 0.075))
    d.ellipse((line[0][0] - r0, line[0][1] - r0, line[0][0] + r0, line[0][1] + r0),
              fill=(22, 163, 74), outline=WHITE, width=max(1, size // 64))
    r1 = max(2, int(size * 0.09))
    d.ellipse((line[-1][0] - r1, line[-1][1] - r1, line[-1][0] + r1, line[-1][1] + r1),
              fill=RED, outline=WHITE, width=max(1, size // 64))
    return im


sizes = [16, 24, 32, 48, 64, 128, 256]
imgs = [draw_icon(s) for s in sizes]
ico = os.path.join(OUT, "app.ico")
imgs[-1].save(ico, format="ICO", sizes=[(s, s) for s in sizes])
imgs[-1].save(os.path.join(OUT, "app.png"))
print("icon written:", ico)
