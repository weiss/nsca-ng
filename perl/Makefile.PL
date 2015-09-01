use 5.010001;
use ExtUtils::MakeMaker;
use ExtUtils::PkgConfig;

my %openssl_info = ExtUtils::PkgConfig->find('openssl');
die "OpenSSL must be v1.0 or higher" unless ExtUtils::PkgConfig->atleast_version('openssl', '1.0');

WriteMakefile(
    NAME              => 'Net::NSCAng::Client',
    VERSION_FROM      => 'lib/Net/NSCAng/Client.pm', # finds $VERSION, requires EU::MM from perl >= 5.5
    MIN_PERL_VERSION  => '5.10.1',
    PREREQ_PM         => {},
    CONFIGURE_REQUIRES    => {
        'ExtUtils::MakeMaker' => 6.52,
        'ExtUtils::PkgConfig' => 0,
    },
    TEST_REQUIRES     => {
        'Test::More'        => 0,
        'Test::Exception'   => 0,
    },
    ABSTRACT_FROM     => 'lib/Net/NSCAng/Client.pm', # retrieve abstract from module
    AUTHOR            => 'Matthias Bethke <matthias@towiski.de>',
    LICENSE           => 'perl',
    LIBS              => [ ExtUtils::PkgConfig->libs('openssl') ],
    DEFINE            => '',
    INC               => '-I.',
    OBJECT            => '$(O_FILES)',
    META_ADD          => {
        repository => {
            type => 'git',
            url => 'git@github.com:mbethke/nsca-ng.git',
            web => 'https://github.com/mbethke/nsca-ng/tree/perl-module/perl',
        },
    },
    dist              => { COMPRESS => 'bzip2', SUFFIX => 'bz2', },
    clean             => { FILES => 'Net-NSCAng-Client-*' },
);
