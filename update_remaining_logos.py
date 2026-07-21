import os
import base64
from PIL import Image
import io

def generate_base64(filepath, width, height, is_jpeg=False):
    img = Image.open(filepath)
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

def update_file(path, src_img, w, h, is_jpeg):
    if os.path.exists(path):
        img_tag = generate_base64(src_img, w, h, is_jpeg)
        svg_content = f'<?xml version="1.0" encoding="UTF-8"?>\n<svg version="1.1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 {w} {h}">\n{img_tag}\n</svg>'
        with open(path, 'w', encoding='utf-8') as f:
            f.write(svg_content)
        print(f"Updated {path}")

def run():
    with Image.open('hake_logo.jpeg') as img:
        w_l, h_l = img.size

    # Update logos
    logos = [
        'images/svg/logos/qgis-logo.svg',
        'images/svg/logos/qgis-logo-made-with-color.svg',
        'images/svg/logos/qgis-logo-made-with-monochrome.svg',
    ]
    for path in logos:
        update_file(path, 'hake_logo.jpeg', w_l, h_l, True)
    
    # Update icons
    icons = [
        'images/svg/logos/qgis-icon.svg',
        'images/icons/qgis-icon-minimal-black.svg'
    ]
    for path in icons:
        update_file(path, 'hake_icon_mark.png', 512, 512, False)

if __name__ == '__main__':
    run()
