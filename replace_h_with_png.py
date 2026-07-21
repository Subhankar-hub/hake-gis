import os
import re
import base64
from PIL import Image
import io

def run():
    # 1. Generate base64 of hake_icon_mark.png at 128x128
    img = Image.open('hake_icon_mark.png')
    img = img.resize((128, 128), Image.Resampling.LANCZOS)
    buffer = io.BytesIO()
    img.save(buffer, format="PNG")
    b64_str = base64.b64encode(buffer.getvalue()).decode('utf-8')
    
    # <circle cx="4" cy="4" r="4" fill="#102B5C"/><text x="4" y="5.5" text-anchor="middle" font-size="6" font-weight="bold" font-family="sans-serif" fill="#00B8F0">H</text>
    pattern = re.compile(r'<circle cx="(\d+)" cy="(\d+)" r="(\d+)" fill="#102B5C"[^>]*/>\s*<text[^>]*>H</text>')
    
    def replace_func(match):
        cx = int(match.group(1))
        cy = int(match.group(2))
        r = int(match.group(3))
        w = 2 * r
        h = 2 * r
        x = cx - r
        y = cy - r
        return f'<image width="{w}" height="{h}" x="{x}" y="{y}" href="data:image/png;base64,{b64_str}" />'
    
    count = 0
    # Walk through images directory
    for root, dirs, files in os.walk('images'):
        for file in files:
            if file.endswith('.svg'):
                filepath = os.path.join(root, file)
                with open(filepath, 'r', encoding='utf-8') as f:
                    content = f.read()
                
                new_content, num_subs = pattern.subn(replace_func, content)
                
                if num_subs > 0:
                    with open(filepath, 'w', encoding='utf-8') as f:
                        f.write(new_content)
                    count += 1
                    
    print(f"Replaced in {count} files in 'images/' directory.")

if __name__ == '__main__':
    run()
