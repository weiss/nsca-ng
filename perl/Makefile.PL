use 5.010001;
use ExtUtils::MakeMaker;
use ExtUtils::PkgConfig;
use Devel::CheckLib;

my %openssl_info = ExtUtils::PkgConfig->find(qw/ eopenssl openssl /);
die "OpenSSL must be v1.0 or higher" unless ExtUtils::PkgConfig->atleast_version('openssl', '1.0');

my $libpath = ExtUtils::PkgConfig->libs_only_L($openssl_info{pkg});
$libpath =~ s/-L//g;
# If this is not set, non-default paths such as eopenssl's are not found on OpenBSD
$ENV{LD_LIBRARY_PATH} //= '';
$ENV{LD_LIBRARY_PATH} .= ':' if length $ENV{LD_LIBRARY_PATH};
$ENV{LD_LIBRARY_PATH} .= $libpath;

check_lib(
    lib => 'ssl',
    header => 'openssl/ssl.h',
    libpath => $libpath,
    function => '
	SSL_CTX *ctx;
	SSL_library_init();
	if(!(ctx = SSL_CTX_new(SSLv23_client_method()))) return 1;
    SSL_CTX_set_psk_client_callback(ctx, (unsigned int (*)(SSL*,const char*,char*,unsigned int,unsigned char*,unsigned int))NULL);
    return 0;
    ',
) or do {
    print STDERR <<EOF;
ERROR: your OpenSSL implementation does not include RFC4279 pre-shared key
functions, so NSCA-ng will not work. If this is LibreSSL, you probably have
a package called `eopenssl' that installs the original OpenSSL.
EOF
    exit 0;
};

WriteMakefile(
    NAME              => 'Net::NSCAng::Client',
    VERSION_FROM      => 'lib/Net/NSCAng/Client.pm',
    MIN_PERL_VERSION  => '5.10.1',
    PREREQ_PM         => {},
    CONFIGURE_REQUIRES    => {
        'ExtUtils::MakeMaker' => 6.52,
        'ExtUtils::PkgConfig' => 0,
        'Devel::CheckLib' => 0,
        'version' => 0.77,
    },
    TEST_REQUIRES     => {
        'Test::Exception' => 0,
        'Test::More' => 0,
        'Test::Pod' => 0,
        'Test::CheckManifest' => 0,
    },
    ABSTRACT_FROM     => 'lib/Net/NSCAng/Client.pm', # retrieve abstract from module
    AUTHOR            => 'Matthias Bethke <matthias@towiski.de>',
    LICENSE           => 'perl',
    LIBS              => [ ExtUtils::PkgConfig->libs($openssl_info{pkg}) ],
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
