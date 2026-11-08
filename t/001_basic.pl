#!/usr/bin/perl

use strict;
use warnings;
use PostgreSQL::Test::Cluster;
use PostgreSQL::Test::Utils;
use Test::More;

plan tests => 3;

my $node = PostgreSQL::Test::Cluster->new('main');
$node->init;
$node->start;

$node->safe_psql('postgres', 'CREATE EXTENSION pg_array_linalg;');

my $result = $node->safe_psql('postgres',
	"SELECT extname FROM pg_extension WHERE extname = 'pg_array_linalg';");
is($result, 'pg_array_linalg', 'Extension is installed');

$result = $node->safe_psql('postgres',
	"SELECT array_dot(ARRAY[1,2,3], ARRAY[4,5,6]);");
is($result, '32', 'array_dot works correctly');

$result = $node->safe_psql('postgres',
	"SELECT array_dot(NULL, ARRAY[1,2,3]) IS NULL;");
is($result, 't', 'array_dot returns NULL for NULL input');

$node->stop;
