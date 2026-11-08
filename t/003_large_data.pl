#!/usr/bin/perl

use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

plan tests => 1;

my $node = PostgreSQL::Test::Cluster->new('main');
$node->init;
$node->start;

$node->safe_psql('postgres', 'CREATE EXTENSION pg_array_linalg;');

my $large_array = 'ARRAY[' . join(',', (1..1000)) . ']';
my $result = $node->safe_psql('postgres',
	"SELECT array_dot($large_array, $large_array);");

is($result, '333833500', 'array_dot works with large arrays');

$node->stop;
