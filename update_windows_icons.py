import os
from PIL import Image

def convert_to_ico(png_path, dest_paths):
    # Standard sizes for Windows icons
    sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    
    img = Image.open(png_path)
    
    for dest in dest_paths:
        if os.path.exists(dest):
            img.save(dest, format='ICO', sizes=sizes)
            print(f"Updated {dest}")
        else:
            print(f"File not found: {dest}")

if __name__ == "__main__":
    png_source = "hake_icon_mark.png"
    
    target_icons = [
        "platform/windows/rc/qgis.ico",
        "platform/windows/rc/qgis-qml.ico",
        "platform/windows/rc/qgis-qpt.ico",
        "platform/windows/rc/qgis-qlr.ico",
        "platform/windows/rc/qgis-qgs.ico",
        "platform/windows/rc/qgis-mime.ico"
    ]
    
    convert_to_ico(png_source, target_icons)
