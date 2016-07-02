Name:       ParateraDB-Rewind
Version:    0.1
Release:	3%{?dist}
Summary:	ParateraDB Utils Tools.

Group:      Databases/ParateraDB	
License:    Copyright(c) 2015	
URL:	    https://git.paratera.net/dba/ParateraDB-Utils	
Source0:    https://git.paratera.net/dba/%{name}.tar.gz	


BuildRequires: coreutils grep procps shadow-utils gcc-c++ gperf ncurses-devel perl readline-devel time zlib-devel libaio-devel bison cmake make automake sqlite-devel mongo-c-driver-libs

Requires:       openssl grep coreutils procps shadow-utils mongo-c-driver-libs sqlite

%description
    Read MySQL audit log ,Write audit log to MongoDB.

%prep
%setup -q -n %{name}


%build
make %{?_smp_mflags}


%install

RBR=$RPM_BUILD_ROOT
MBD=$RPM_BUILD_DIR/%{name}

install -D -m 0755 $MBD/rewind $RBR/%{_bindir}/rewind

%post 

%files
%{_bindir}/rewind
#%doc

%changelog

