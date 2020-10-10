Summary: Message Passing over TCP/IP, using BSD socket interface programming.
Name: messip_devel
Version: 0
Release: 9
%define src %{name}
License: BSD
Group: Networking 
URL: http://messip.sourceforge.net/
Prefix: %{_prefix}
Buildroot: %{_tmppath}/%{name}

%description
MESSIP implements Message Passing over TCP/IP, using BSD socket interface programming.
Three kinds of messages can be sent between Processes or Threads:
- Synchronous Messages (blocking messages), from 0 to 64k in size,
- Asynchronous Messages (non blocking, buffered messages), from 0 to 64k in size,
- Timer messages (one shot and repetitive)

%prep
#

%build
#

%install
#

%files
#%attr(755,root,root) %{_includedir}/*

%clean
rm -rf $RPM_BUILD_ROOT

%changelog
