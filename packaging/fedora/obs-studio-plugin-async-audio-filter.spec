Name: obs-studio-plugin-async-audio-filter
Version: @VERSION@
Release: @RELEASE@%{?dist}
Summary: Asynchronous audio filter plugin for OBS Studio
License: GPLv2+

Source0: %{name}-%{version}.tar.bz2
BuildRequires: cmake, gcc, gcc-c++
BuildRequires: obs-studio-devel
BuildRequires: libsamplerate-devel

%description
This plugin attempts to fix asynchronous audio by synchronizing to the master
clock of OBS Studio.

%prep
%autosetup -p1

%build
%{cmake} \
  -DUSE_ERIKD_LIBSAMPLERATE_DEPS=OFF -DUSE_ERIKD_LIBSAMPLERATE_SYSTEM=ON \
  -DCMAKE_SKIP_RPATH:BOOL=ON \
  -DINSTALL_LICENSE_FILES:BOOL=OFF
%{cmake_build}

%install
%{cmake_install}

%files
%{_libdir}/obs-plugins/*.so
%{_datadir}/obs/obs-plugins/*/
%license LICENSE
