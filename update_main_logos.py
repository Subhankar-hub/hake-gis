import os
import base64
from PIL import Image
import io

def generate_base64(filepath, width, height, is_jpeg=False):
    img = Image.open(filepath)
    # Ensure correct resampling
    img = img.resize((width, height), Image.Resampling.LANCZOS)
    buffer = io.BytesIO()
    if is_jpeg:
        img = img.convert("RGB")
        img.save(buffer, format="JPEG")
        mime = "image/jpeg"
    else:
        img.save(buffer, format="PNG")
        mime = "image/png"
    
    b64_str = base64.b64encode(buffer.getvalue()).decode('utf-8')
    return f'<image width="{width}" height="{height}" x="0" y="0" href="data:{mime};base64,{b64_str}" />'

def run():
    # 1. Update src/app/qml/images/qgis.svg with hake_logo.jpeg
    qml_logo_path = 'src/app/qml/images/qgis.svg'
    if os.path.exists(qml_logo_path):
        # We assume the user wants it to fit in the UI without distortion.
        # hake_logo.jpeg might not be a square. Let's get its dimensions.
        with Image.open('hake_logo.jpeg') as img:
            w, h = img.size
        # The QML sets width: 200, height: 200, preserveAspectFit.
        # We can just write the SVG with viewBox="0 0 w h"
        img_tag = generate_base64('hake_logo.jpeg', w, h, is_jpeg=True)
        svg_content = f'<?xml version="1.0" encoding="UTF-8"?>\n<svg version="1.1" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}">\n{img_tag}\n</svg>'
        with open(qml_logo_path, 'w', encoding='utf-8') as f:
            f.write(svg_content)
        print(f"Updated {qml_logo_path}")

    # 2. Update images/icons/qgis_icon.svg with hake_icon_mark.png (512x512)
    icon_path = 'images/icons/qgis_icon.svg'
    if os.path.exists(icon_path):
        img_tag = generate_base64('hake_icon_mark.png', 512, 512, is_jpeg=False)
        svg_content = f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 512 512">\n{img_tag}\n</svg>'
        with open(icon_path, 'w', encoding='utf-8') as f:
            f.write(svg_content)
        print(f"Updated {icon_path}")

if __name__ == '__main__':
    run()
