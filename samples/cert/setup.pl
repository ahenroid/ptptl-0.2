#!/usr/bin/perl
#
# setup.pl - Create a set of certificate stores.  The created certificate
#            stores are named user0.pfx, user1.pfx, ...
#

($ME = $0) =~ s|.*/||;

$EXPIRE = 30;
$ISSUER = "Trusted CA";

while ($ARGV[0] =~ m/^-/)
{
    if ($ARGV[0] eq "-e" || $ARGV[0] eq "--expire")
    {
	shift(@ARGV);
	$EXPIRE = shift(@ARGV);
    }
    elsif ($ARGV[0] eq "-i" || $ARGV[0] eq "--issuer")
    {
	shift(@ARGV);
	$ISSUER = shift(@ARGV);
    }
    else
    {
	last;
    }
}

if (!@ARGV)
{
    print "Usage: $ME [OPTIONS] NAME ...\n";
    print "  -e,--expire DAYS  Set expiration time\n";
    print "  -i,--issuer NAME  Set certificate issuer name\n";
    exit(1);
}

@name = ();
$i = 0;
foreach (@ARGV)
{
    $name[$i] = $_;
    print "$ME: creating ``$name[$i]'' (user${i}.pfx)\n";
    system("./cert --setup \"$name[$i]\"");
    system("./cert --export \"$name[$i]\" ${i}.pfx");
    system("./cert --save user${i}.pfx");
    $i++;
}

system("./cert --setup \"$ISSUER\"");
system("./cert --export \"$ISSUER\" issuer.pfx");

for ($i = 0; $i <= $#name; $i++)
{
    print "$ME: signing ``$name[$i]''\n";
    system("./cert --sign $EXPIRE ${i}.pfx");
}

for ($i = 0; $i <= $#name; $i++)
{
    print "$ME: setting up ``$name[$i]''\n";
    system("./cert --load user${i}.pfx");
    system("./cert --import issuer.pfx");
    for ($j = 0; $j <= $#name; $j++)
    {
	system("./cert --import ${j}.pfx");
    }
    system("./cert --save user${i}.pfx");
}

unlink("issuer.pfx");
for ($i = 0; $i <= $#name; $i++)
{
    unlink("${i}.pfx");
}

exit(0);
