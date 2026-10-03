#!/usr/bin/env python3
"""Render review sheets for the Hake icon catalog mappings (development helper).

Writes two PNGs:
  docs/hake-icon-coverage-sheet.png  every catalog mapping, grouped by catalog section
  docs/hake-icon-state-sheet.png     normal/disabled/checked states and 16-48 px sizes

State colours use the same string substitutions as QgsHakeIconEngine in
src/app/qgshakeicons.cpp. Requires PyQt6 with QtSvg; run with
QT_QPA_PLATFORM=offscreen on a headless machine.
"""

import sys
from collections import OrderedDict
from pathlib import Path

from PyQt6.QtCore import QByteArray, QRectF, Qt
from PyQt6.QtGui import QColor, QFont, QGuiApplication, QImage, QPainter
from PyQt6.QtSvg import QSvgRenderer

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import hake_icon_coverage as cov  # noqa: E402

OUT_COVERAGE = ROOT / "docs/hake-icon-coverage-sheet.png"
OUT_STATES = ROOT / "docs/hake-icon-state-sheet.png"

STATE_ICONS = [
    ("New", "A", "mActionNewProject"),
    ("Open", "A", "mActionOpenProject"),
    ("Pan", "A", "mActionPan"),
    ("Zoom", "A", "mActionZoomIn"),
    ("Toggle Editing", "A", "mActionToggleEditing"),
    ("Select", "A", "mActionSelectFeatures"),
    ("Add layer", "A", "mActionAddOgrLayer"),
    ("Buffer", "ALG", "native:buffer"),
    ("Rasterize", "ALG", "gdal:rasterize"),
    ("Help", "A", "mActionHelpContents"),
]
SIZES = [16, 20, 24, 32, 48]


def state_svg(data, state):
    if state == "disabled":
        return data.replace(b"#164A73", b"#A9BCCB").replace(b"#D6E4F4", b"#EEF3F7")
    if state == "checked":
        return data.replace(b"#164A73", b"#0E3858")
    return data


def renderer(res, state="normal"):
    data = (cov.ICON_DIR / res).read_bytes()
    return QSvgRenderer(QByteArray(state_svg(data, state)))


def painter_for(img):
    p = QPainter(img)
    p.setRenderHint(QPainter.RenderHint.Antialiasing)
    return p


def font(px, bold=False):
    f = QFont()
    f.setPixelSize(px)
    f.setBold(bold)
    return f


def coverage_sheet(sections):
    cols, cw, ch, head = 12, 132, 100, 30
    height = 50 + sum(
        head + ((len(items) + cols - 1) // cols) * ch for items in sections.values()
    )
    img = QImage(cols * cw + 20, height, QImage.Format.Format_ARGB32)
    img.fill(QColor("#FFFFFF"))
    p = painter_for(img)
    p.setPen(QColor("#164A73"))
    p.setFont(font(18, True))
    total = sum(len(v) for v in sections.values())
    p.drawText(
        12, 30, f"Hake icon coverage: {total} catalog mappings (48 / 16 / 24 px)"
    )
    y = 50
    for section, items in sections.items():
        p.setPen(QColor("#164A73"))
        p.setFont(font(14, True))
        p.drawText(12, y + 20, f"{section} ({len(items)})")
        y += head
        for i, (kind, key, res) in enumerate(items):
            x, cy = 10 + (i % cols) * cw, y + (i // cols) * ch
            p.fillRect(x + 6, cy + 4, 52, 52, QColor("#F3F6F9"))
            r = renderer(res)
            r.render(p, QRectF(x + 8, cy + 6, 48, 48))
            r.render(p, QRectF(x + 70, cy + 22, 16, 16))
            r.render(p, QRectF(x + 94, cy + 18, 24, 24))
            p.setPen(QColor("#333333"))
            p.setFont(font(9))
            label = key.split(":", 1)[-1] if kind == "ALG" else key
            p.drawText(
                QRectF(x + 2, cy + 60, cw - 4, 38),
                Qt.AlignmentFlag.AlignHCenter | Qt.TextFlag.TextWrapAnywhere,
                f"{kind} {label}",
            )
        y += ((len(items) + cols - 1) // cols) * ch
    p.end()
    img.save(str(OUT_COVERAGE))


def state_sheet(lookup):
    rows = [(label, lookup.get((kind, key))) for label, kind, key in STATE_ICONS]
    lw, rh = 130, 64
    states = [("normal", "#FFFFFF"), ("disabled", "#FFFFFF"), ("checked", "#DCE7F2")]
    width = lw + len(states) * 90 + 30 + sum(s + 24 for s in SIZES) + 20
    img = QImage(width, 70 + len(rows) * rh, QImage.Format.Format_ARGB32)
    img.fill(QColor("#FFFFFF"))
    p = painter_for(img)
    p.setPen(QColor("#164A73"))
    p.setFont(font(12, True))
    x = lw
    for name, _ in states:
        p.drawText(QRectF(x, 30, 90, 20), Qt.AlignmentFlag.AlignHCenter, f"{name} 24")
        x += 90
    x += 30
    for s in SIZES:
        p.drawText(QRectF(x, 30, s + 24, 20), Qt.AlignmentFlag.AlignHCenter, f"{s}")
        x += s + 24
    p.setFont(font(16, True))
    p.drawText(12, 22, "Hake icon states (engine colour substitution) and sizes")
    for i, (label, res) in enumerate(rows):
        y = 60 + i * rh
        p.setPen(QColor("#333333"))
        p.setFont(font(11))
        p.drawText(
            QRectF(10, y, lw - 12, rh),
            Qt.AlignmentFlag.AlignVCenter,
            label if res else f"{label} (unmapped)",
        )
        if not res:
            continue
        x = lw
        for name, bg in states:
            p.fillRect(x + 25, y + 12, 40, 40, QColor(bg))
            renderer(res, name).render(p, QRectF(x + 33, y + 20, 24, 24))
            x += 90
        x += 30
        for s in SIZES:
            renderer(res).render(p, QRectF(x + 12, y + (rh - s) / 2, s, s))
            x += s + 24
    p.end()
    img.save(str(OUT_STATES))


def main():
    rows = (
        cov.parse_catalog(cov.DEFAULT_CATALOG) if cov.DEFAULT_CATALOG.is_file() else []
    )
    registry = cov.parse_registry(cov.REGISTRY) or []
    lookup = {(k, key): res for k, key, res in registry}
    sections = OrderedDict()
    if rows:
        for row in rows:
            if row["id"] in cov.EXCLUDED:
                continue
            kind, key = cov.expected_mapping(row)
            if (kind, key) in lookup:
                sections.setdefault(row["section"], []).append(
                    (kind, key, lookup[(kind, key)])
                )
    else:
        for kind, key, res in registry:
            sections.setdefault(kind, []).append((kind, key, res))

    app = QGuiApplication(sys.argv)  # noqa: F841
    coverage_sheet(sections)
    state_sheet(lookup)
    print(OUT_COVERAGE.relative_to(ROOT))
    print(OUT_STATES.relative_to(ROOT))


if __name__ == "__main__":
    main()
