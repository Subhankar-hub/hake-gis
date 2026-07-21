import os
import re

def process_ui_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Replace QGIS inside <string>...</string> tags
    def replace_qgis(match):
        inner_text = match.group(1)
        # We only want to replace standalone QGIS (or QGIS 3, QGIS Desktop, etc), but a simple replace is fine 
        # since it's already inside a <string> tag intended for the UI.
        new_text = inner_text.replace('QGIS', 'Hake Geospatial')
        return f'<string>{new_text}</string>'

    new_content = re.sub(r'<string>(.*?)</string>', replace_qgis, content, flags=re.DOTALL)

    if new_content != content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"Updated {filepath}")

def main():
    base_dir = 'src'
    for root, dirs, files in os.walk(base_dir):
        for file in files:
            if file.endswith('.ui'):
                filepath = os.path.join(root, file)
                process_ui_file(filepath)

if __name__ == "__main__":
    main()
