use 5.010001;
use ExtUtils::MakeMaker;
WriteMakefile(
    NAME              => 'Net::NSCAng::Client',
    VERSION_FROM      => 'lib/Net/NSCAng/Client.pm', # finds $VERSION, requires EU::MM from perl >= 5.5
    PREREQ_PM         => {},
    TEST_REQUIRES     => {
        'Test::More'        => 0,
        'Test::Exception'   => 0,
    },
    ABSTRACT_FROM     => 'lib/Net/NSCAng/Client.pm', # retrieve abstract from module
    AUTHOR            => 'Matthias Bethke <matthias@towiski.de>',
    LICENSE           => 'perl',
    LIBS              => ['-lssl'],
    DEFINE            => '', # e.g., '-DHAVE_SOMETHING'
    INC               => '-I.', # e.g., '-I. -I/usr/include/other'
    OBJECT            => '$(O_FILES)',
);
