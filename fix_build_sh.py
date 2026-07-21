import os

filepath = 'ms-windows/mingw/build.sh'

with open(filepath, 'r') as f:
    content = f.read()

content = content.replace('qt5', 'qt6')
content = content.replace('Qt5', 'Qt6')

with open(filepath, 'w') as f:
    f.write(content)

print("Replaced all qt5 with qt6 in build.sh")
