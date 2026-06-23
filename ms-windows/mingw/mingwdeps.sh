#!/bin/sh

# To be removed
# Workaround a bug where the initial /etc/dnf/dnf.conf file contains
# just the "tsflags=nodocs" line
printf '[main]\ngpgcheck=True\ninstallonly_limit=3\nclean_requirements_on_remove=True\nbest=False\nskip_if_unavailable=True\ntsflags=nodocs' > /etc/dnf/dnf.conf

dnf install -y --nogpgcheck \
  mingw64-dlfcn \
  mingw64-exiv2 \
  mingw64-fcgi \
  ccache \
  mingw64-gcc-c++ \
  mingw64-gdal \
  mingw64-gdb \
  mingw64-GeographicLib \
  mingw64-geos \
  mingw64-gsl \
  mingw64-libgomp \
  mingw64-libzip \
  mingw64-postgresql \
  mingw64-proj \
  mingw64-python3 \
  mingw64-python3-affine \
  mingw64-python3-chardet \
  mingw64-python3-dateutil \
  mingw64-python3-flask \
  mingw64-python3-gdal \
  mingw64-python3-geographiclib \
  mingw64-python3-homography \
  mingw64-python3-idna \
  mingw64-python3-lxml \
  mingw64-python3-markupsafe \
  mingw64-python3-numpy \
  mingw64-python3-opencv \
  mingw64-python3-owslib \
  mingw64-python3-pillow \
  mingw64-python3-psycopg2 \
  mingw64-python3-PyQt-builder \
  mingw64-python3-pytz \
  mingw64-python3-pyyaml \
  mingw64-python3-qscintilla-qt6 \
  mingw64-python3-pyqt6 \
  mingw64-python3-requests \
  mingw64-python3-shapely \
  mingw64-python3-urllib3 \
  mingw64-qca-qt6 \
  mingw64-qscintilla-qt6 \
  mingw64-qt6-qtactiveqt \
  mingw64-qt6-qtbase \
  mingw64-qt6-qtimageformats \
  mingw64-qt6-qtlocation \
  mingw64-qt6-qtmultimedia \
  mingw64-qt6-qtserialport \
  mingw64-qt6-qtsvg \
  mingw64-qt6-qttools \
  mingw64-qt6-qttranslations \
  mingw64-qtkeychain-qt6 \
  mingw64-quazip-qt6 \
  mingw64-qwt-qt6 \
  mingw64-sip \
  mingw64-spatialindex \
  mingw64-sqlite \
  mingw64-svg2svgt \
  mingw64-zstd \
  bison \
  cmake \
  findutils \
  flex \
  gcc-c++ \
  gdal-devel \
  git \
  make \
  proj-devel \
  python-devel \
  python3-pyqt6 \
  python3-qscintilla-qt6 \
  qt6-linguist \
  qt6-qtbase-devel \
  sqlite-devel \
  wget \
  xorg-x11-server-Xvfb \
  zip
