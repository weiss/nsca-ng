#!/usr/bin/perl
use strict;
use warnings;
use Test::More tests => 2;

BEGIN {
    use_ok('Net::NSCAng::Client');
}

isa_ok(Net::NSCAng::Client->new('bogushost.invalid', 'none', 'none'), 'Net::NSCAng::Client');


