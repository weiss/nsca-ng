#!perl -T
use 5.10.1;
use strict;
use warnings FATAL => 'all';
use Test::More;

unless ( $ENV{RELEASE_TESTING} ) {
    plan( skip_all => "Author tests not required for installation" );
}

my $min_tcm = 0.9;
eval "use Test::CheckManifest $min_tcm";
plan skip_all => "Test::CheckManifest $min_tcm required" if $@;

ok_manifest({
        exclude => [ qw# /.git /.gitignore /Client.bs /Client.o /Client.c /client.o # ],
        filter => [ qr/\.sw[pqr]$/, qr/\.old$/, qr/\.tar.(?:bz2|gz|)$/ ],
    }
);

