import base64
from PIL import Image
import io

def generate_base64_image():
    # Load and resize the image
    img = Image.open('hake_icon_mark.png')
    # Resize to 64x64 with high-quality resampling (LANCZOS)
    img = img.resize((64, 64), Image.Resampling.LANCZOS)
    
    # Save to bytes buffer
    buffer = io.BytesIO()
    img.save(buffer, format="PNG")
    
    # Encode to base64
    b64_str = base64.b64encode(buffer.getvalue()).decode('utf-8')
    
    # Print the base64 string
    print(b64_str)

if __name__ == '__main__':
    generate_base64_image()
