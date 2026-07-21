import os

def replace_redirects():
    replacements = {
        'https://qgis.org/funding/membership/members/': 'https://haketech.com',
        'https://qgis.org/resources/support/bug-reporting/': 'https://haketech.com',
        'https://qgis.org/community/involve/': 'https://haketech.com',
        'https://qgis.org/funding/donate/': 'https://haketech.com',
        'https://qgis.org/resources/support/commercial-support/': 'https://haketech.com',
        'https://qgis.org"': 'https://haketech.com"',
        'https://docs.qgis.org/$qgis_short_version/$qgis_locale/docs/user_manual/': 'https://haketech.com',
        'https://www.qgis.org/en/site/forusers/visualchangelogVERSION_TOKEN/index.html': 'https://haketech.com',
        'https://qgis.org/resources/support/bug-reporting/#bugs-features-and-issues': 'https://haketech.com'
    }

    files_to_check = [
        'src/app/qgisapp.cpp',
        'src/app/options/qgsoptions.cpp',
        'src/ui/qgsfirstrundialog.ui',
        'src/crashhandler/qgscrashdialog.cpp'
    ]

    for filepath in files_to_check:
        if not os.path.exists(filepath):
            print(f"Skipping {filepath}, not found.")
            continue
        
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        new_content = content
        for old_str, new_str in replacements.items():
            new_content = new_content.replace(old_str, new_str)
        
        if new_content != content:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(new_content)
            print(f"Updated redirects in {filepath}")

if __name__ == "__main__":
    replace_redirects()
