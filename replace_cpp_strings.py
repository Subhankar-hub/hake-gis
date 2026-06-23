import os
import re

def process_cpp_file(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        return # Skip non-utf8 files

    def replace_tr_content(match):
        full_tr = match.group(0)
        # Only replace QGIS with Hake Geospatial inside the matched tr() block
        new_tr = full_tr.replace('QGIS', 'Hake Geospatial')
        return new_tr

    # Matches tr("..."), qsTr("..."), and tr("..." "...") including newlines
    new_content = re.sub(r'\b(?:tr|qsTr)\s*\(\s*(?:"(?:[^"\\]|\\.)*"\s*)+\)', replace_tr_content, content)

    # We also have places where QStringLiteral or QString is used for UI text.
    # A safe heuristic: if it's QStringLiteral("... QGIS ...") where it contains spaces, it's likely UI.
    def replace_qstring_content(match):
        full_str = match.group(0)
        if ' QGIS ' in full_str or 'QGIS ' in full_str or ' QGIS' in full_str:
            return full_str.replace('QGIS', 'Hake Geospatial')
        return full_str

    new_content = re.sub(r'\bQString(?:Literal)?\s*\(\s*(?:"(?:[^"\\]|\\.)*"\s*)+\)', replace_qstring_content, new_content)

    if new_content != content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"Updated {filepath}")

def main():
    base_dir = 'src'
    for root, dirs, files in os.walk(base_dir):
        for file in files:
            if file.endswith('.cpp') or file.endswith('.h') or file.endswith('.qml'):
                filepath = os.path.join(root, file)
                process_cpp_file(filepath)

if __name__ == "__main__":
    main()
