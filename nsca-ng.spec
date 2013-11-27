# Copyright (c) 2013 Paul Richards <paul@minimoo.org>, Gunnar Beutner <gunnar@beutner.name>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

%define revision 1

%define nsca_user nsca
%define nsca_group nsca

Summary: NSCA-ng
Name: nsca-ng
Version: 1.2
Release: %{revision}%{?dist}
License: BSD
Group: Applications/System
Source: %{name}-%{version}.tar.gz
URL: https://www.nsca-ng.org/
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

BuildRequires: libconfuse-devel
BuildRequires: openssl-devel >= 1.0.0
BuildRequires: patch

%description
NSCA-ng provides a client-server pair that makes the Nagios command file accessible to remote systems.

%package server
Summary:      NSCA-ng server
Group:        Applications/System
Requires:     libconfuse
Requires:     openssl >= 1.0.0

%description server
NSCA-ng provides a client-server pair that makes the Nagios command file accessible to remote systems.

This is the server component of NSCA-ng.

%package client
Summary:      NSCA-ng client
Group:        Applications/System
Requires:     libconfuse
Requires:     openssl >= 1.0.0

%description client
NSCA-ng provides a client-server pair that makes the Nagios command file accessible to remote systems.

This is the client component of NSCA-ng.

%prep
%setup -q -n %{name}-%{version}

%build
%configure --enable-server

make %{?_smp_mflags}

%install
[ "%{buildroot}" != "/" ] && [ -d "%{buildroot}" ] && rm -rf %{buildroot}
make install \
	DESTDIR="%{buildroot}"

mkdir -p %{buildroot}%{_sysconfdir}/init.d
install contrib/nsca-ng.init %{buildroot}%{_sysconfdir}/init.d/%{name}

mkdir -p %{buildroot}%{_localstatedir}/run/%{name}

(cd %{buildroot}%{_sysconfdir} && patch <<PATCH
--- nsca-ng.cfg 2013-11-24 11:09:28.837618003 +0000
+++ nsca-ng.cfg.pkg     2013-11-24 11:09:51.199618161 +0000
@@ -25,6 +25,8 @@
 # 	timeout = 15.0                          # Default: 60.0.
 #

+user = "%{nsca_user}"
+
 #
 # Clients provide a client ID (think: user name) and a password.  The same
 # ID/password combination may be used by multiple clients.  In order to
PATCH
)

(cd %{buildroot}%{_sysconfdir}/init.d && patch <<PATCH
--- nsca-ng     2013-11-24 11:20:20.502617975 +0000
+++ nsca-ng.pkg 2013-11-24 11:20:45.532618065 +0000
@@ -15,7 +15,7 @@
 export PATH

 name='NSCA-ng'
-pid_file="\$HOME/.nsca-ng.pid"
+pid_file="%{_localstatedir}/run/%{name}/nsca-ng.pid"

 get_pid()
 {
PATCH
)

%clean
[ "%{buildroot}" != "/" ] && [ -d "%{buildroot}" ] && rm -rf %{buildroot}

%pre server
getent group %{nsca_group} >/dev/null || %{_sbindir}/groupadd -r %{nsca_group}
getent passwd %{nsca_user} >/dev/null || %{_sbindir}/useradd -c "nsca" -s /sbin/nologin -r -d %{_localstatedir}/lib/%{nsca_user} -g %{nsca_group} %{nsca_user}
exit 0

%files server
%defattr(-,root,root,-)
%doc COPYING README NEWS AUTHORS
%attr(755,-,-) %{_sysconfdir}/init.d/%{name}
%config(noreplace) %attr(0640,%{nsca_user},%{nsca_group}) %{_sysconfdir}/nsca-ng.cfg
%{_sbindir}/nsca-ng
%attr(0755,%{nsca_user},%{nsca_group}) %{_localstatedir}/run/%{name}
%{_mandir}/man5/nsca-ng.cfg.5.gz
%{_mandir}/man8/nsca-ng.8.gz

%files client
%defattr(-,root,root,-)
%doc COPYING README NEWS AUTHORS
%config(noreplace) %attr(0640,-,-) %{_sysconfdir}/send_nsca.cfg
%{_sbindir}/send_nsca
%{_mandir}/man5/send_nsca.cfg.5.gz
%{_mandir}/man8/send_nsca.8.gz

%changelog