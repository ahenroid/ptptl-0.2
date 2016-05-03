#!/usr/bin/perl
#
# mkdoc.pl - Read a C/C++ file and use embedded Gnome-style comments
#            to generate documentation.
#

($ME = $0) =~ s|.*/||;

sub TYPE_DEFINE {0}
sub TYPE_CLASS {1}
sub TYPE_CONST {2}
sub TYPE_TYPEDEF {3}
sub TYPE_METHOD {4}
sub TYPE_VAR {5}
sub TYPE_INVALID {6}

sub INFO_TYPE {0}
sub INFO_PARMS {1}
sub INFO_SECTS {2}
sub INFO_INDEX {3}
sub INFO_GROUP {4}
sub INFO_TAG {5}

sub PARM_TYPE {0};
sub PARM_NAME {1};
sub PARM_DESC {2};

sub SECT_TYPE {0};
sub SECT_NAME {1};
sub SECT_DESC {2};

sub SECT_NAME_DESC {"DESCRIPTION"};
sub SECT_NAME_TYPE {"TYPE"};
sub SECT_NAME_SYNOPSIS {"SYNOPSIS"};
sub SECT_NAME_EXAMPLE {"EXAMPLE"};

#
# process files
#
@info = ();
if (@ARGV)
{
    foreach $file (@ARGV)
    {
	open(FILE, $file) || next;
	push(@info, &parse_file(FILE));
	close(FILE);
    }
}
else
{
    @info = &parse_file(STDIN);
}
@info = sort {(($a->[&INFO_GROUP] cmp $b->[&INFO_GROUP])
	       || ($a->[&INFO_TYPE] - $b->[&INFO_TYPE])
	       || ($a->[&INFO_TAG] - $b->[&INFO_TAG]))} @info;
&write_html(STDOUT, \@info);

exit(0);

#
# read file and parse header and prototype information
#
sub
parse_file
{
    my($in) = shift;

    my(@info) = ();
    while (<$in>)
    {
	if (/\/\*\*/)
	{
	    push(@info, &merge_parsed(&parse_hdr($in), &parse_proto($in)));
	}
    }
    return @info;
}

#
# parse method header
#
sub
parse_hdr
{
    my($in) = shift;

    my($sect) = "";
    my(@sects) = ();
    my(%sects) = ();
    my(%descs) = ();

    while (($_ = <$in>) && !/\*\//)
    {
	# strip " * " prefix
	s/^[ \t]*\*+[ \t]?//;

	if ($sect eq "" && /^\s*((\w+::)*~?\w+)\s*(:[^:])?/)
	{
	    # method name and description
	    $_ = $';
	    $sect = $1;
	    $sects{$sect} = 1;
	    push(@sects, $sect);
	}
	elsif (/^\s*@(\w+|\.\.\.)\s*(:[^:])?/)
	{
	    # parameter name and description
	    $sect = $1;
	    $_ = $';
	}
	elsif (/^\s*(\w+)\s*:([^:])/)
	{
	    # section name and description
	    $sect = uc($1);
	    $_ = "$2$'";
	    if (!defined($sects{$sect}))
	    {
		$sects{$sect} = 1;
		push(@sects, $sect);
	    }
	}

	$descs{$sect} .= $_;
    }

    return \(@sects, %descs);
}

#
# parse method prototype
#
sub
parse_proto
{
    my($in) = shift;

    my(%tokentotype) =
	("#define"=>&TYPE_DEFINE,
	 "typedef"=>&TYPE_TYPEDEF,
	 "class"=>&TYPE_CLASS,
	 "struct"=>&TYPE_CLASS,
	 ","=>&TYPE_CONST,
	 "="=>&TYPE_CONST,
	 "}"=>&TYPE_CONST);
    my($elipsus) = '\.\.\.';

    my($type) = -1;
    my(@parms) = ("");

    $_ = "";
    while ($_ || ($_ = <$in>))
    {
	s/^\s+//;
	next if($_ eq "");

	# get next token
	my($token) = "";
	if (/^((\w+::)*~?\w+|\#\w+|$elipsus|.)/)
	{
	    $_ = $';
	    $token = $1;
	}
	my($tokenlen) = length($token);

	# set item type
	if ($type < 0)
	{
	    my($t) = $tokentotype{$token};
	    $type = $t if(defined($t));
	    $type = &TYPE_VAR if(!defined($t) && $tokenlen == 1);
	}

	# push token or start new parameter
	if ($type == &TYPE_CLASS && $token eq "EXPORT")
	{
	}
	elsif ($token !~ m/^[,\(\){}:;=]/
	       || ($token eq "=" && $type != &TYPE_CONST))
	{
	    $parms[$#parms] .= " " if ($parms[$#parms] ne "");
	    $parms[$#parms] .= $token;
	}
	else
	{
	    push(@parms, "") if($parms[$#parms] ne "");
	    $type = &TYPE_METHOD if($type == &TYPE_VAR);
	}

	# handle special cases (function pointer typedefs and #defines)
	pop(@parms) if ($type == &TYPE_TYPEDEF && $parms[$#parms] eq "*");
	$type = &TYPE_INVALID if ($type == &TYPE_DEFINE && $_ =~ m/^\(/);
	$type = &TYPE_DEFINE if ($type == &TYPE_INVALID && $token eq ")");

	# terminate finished item
	last if ($type == &TYPE_CONST);
	last if ($type == &TYPE_TYPEDEF && $token eq ";");
	last if ($type == &TYPE_DEFINE && $tokenlen == 1);
	last if ($type == &TYPE_DEFINE && $token ne "#define");
	last if ($token =~ m/^[{}:;]/);
    }

    pop(@parms) if ($parms[$#parms] eq "");

    # fill in parameter types
    my(%types) = ();
    foreach (@parms)
    {
	$_ =~ m/\s*((\w+::)*~?\w+|\#\w+|$elipsus)$/;
	$_ = $1;
	my($t) = ($type == &TYPE_CONST) ? "const":$`;
	$types{$_} = $t;
    }

    return (@parms ? ($type, \@parms, \%types):());
}

#
# merge header and prototype information
#
sub
merge_parsed
{
    my($sects) = shift;
    my($descs) = shift;
    my($type) = shift;
    my($parms) = shift;
    my($types) = shift;

    my($title) = $sects->[0];
    my($group) = $title;
    $group =~ s/::~?\w+$// unless($type == &TYPE_CLASS);

    my($tag) = "TAG" . sprintf("%04d", $mkdoc_tag++);

    # aggregate parameter information
    my(@parm) = ();
    foreach (@$parms)
    {
	my($name) = @parm ? $_:$title;
	my($desc) = $descs->{$name};
	$desc =~ s/^[ \t]+//;
	$desc =~ s/\s+$//;

	my(@p) = ();
	$p[&PARM_TYPE] = $types->{$_};
	$p[&PARM_NAME] = $name;
	$p[&PARM_DESC] = $desc;
	push(@parm, \@p);
    }
    shift(@$sects);

    # aggregate section information
    my(@sect) = ();
    foreach (@$sects)
    {
	my($desc) = $descs->{$_};
	$desc =~ s/^[ \t]+//;
	$desc =~ s/\s+$//;
	$desc =~ s/^\n+//;

	if ($_ eq &SECT_NAME_TYPE)
	{
	    $parm[0]->[&PARM_TYPE] = $desc . " " . $parm[0]->[&PARM_TYPE];
	}
	else
	{
	    my(@s) = ();
	    $s[&SECT_TYPE] = ".SECTION.";
	    $s[&SECT_NAME] = $_;
	    $s[&SECT_DESC] = $desc;
	    push(@sect, \@s) if ($desc =~ m/./);
	}
    }

    # create an index for all parameters and sections
    my(%index) = ();
    map {$index{$_->[&PARM_NAME]} = $_} @parm;
    map {$index{$_->[&SECT_NAME]} = $_} @sect;

    my(@i) = ();
    $i[&INFO_TYPE] = $type;
    $i[&INFO_PARMS] = \@parm;
    $i[&INFO_SECTS] = \@sect;
    $i[&INFO_INDEX] = \%index;
    $i[&INFO_GROUP] = $group;
    $i[&INFO_TAG] = $tag;
    return \@i;
}

#
# convert type text to HTML
#
sub
type_html
{
    my($info) = shift;
    my($type) = shift;

    my($line) = $type;
    $type = "";
    while ($line =~ m/((\w+::)*~?\w+)/)
    {
	$type .= $`;
	my($name) = $1;
	$line = $';

	my($match) = 0;
	my($i);
	foreach $i (@$info)
	{
	    if ($i->[&INFO_PARMS][0][&PARM_NAME] =~ m/^(\w+::)*$name$/)
	    {
		$type .= "<A HREF=\"#$i->[5]\">$name</A>";
		$match = 1;
		last;
	    }
	}
	$type .= "$name" unless($match);
    }
    $type .= $line;
    return $type;
}

#
# convert description text to HTML
#
sub
desc_html
{
    my($info) = shift;
    my($sect) = shift;
    my($desc) = shift;

    $desc =~ s/^\@((\w+::)*~?\w+)/<I>$1<\/I>/g;
    $desc =~ s/([^\\])\@((\w+::)*~?\w+)/$1<I>$2<\/I>/g;
    $desc =~ s/^\$((\w+::)*~?\w+)/<B>$1<\/B>/g;
    $desc =~ s/([^\\])\$((\w+::)*~?\w+)/$1<B>$2<\/B>/g;
    $desc =~ s/\\([\@\$])/$1/g;

    return $desc if(uc($sect) eq &SECT_NAME_EXAMPLE);

    my($line) = " $desc";
    $desc = "";
    while ($line =~ m/([^\\])[\&\%]((\w+::)*~?\w+)/)
    {
	$desc .= "$`$1";
	my($name) = $2;
	$line = $';

	my($match) = 0;
	my($i);
	foreach $i (@$info)
	{
	    if ($i->[&INFO_PARMS][0][&PARM_NAME] =~ m/^(\w+::)*$name$/)
	    {
		$desc .= "<A HREF=\"#$i->[5]\">$name</A>";
		$match = 1;
		last;
	    }
	}
	$desc .= "<B>$name</B>" unless($match);
    }
    $desc .= $line;
    $desc =~ s/\\([\&\%])/$1/g;

    return $desc;
}

#
# write html output
#
sub
write_html
{
    my($out) = shift;
    my($info) = shift;

    my($name) = $info->[0][&INFO_PARMS][0][&PARM_NAME];
    my($desc) = $info->[0][&INFO_PARMS][0][&PARM_DESC];
    $desc = &desc_html($info, "", $desc);

    # write HTML header
    print $out "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\n";
    print $out "<HTML>\n";
    print $out "<HEAD>\n";
    
    # write title
    print $out "<TITLE>$name</TITLE>\n";
    print $out "</HEAD>\n";
    print $out "<BODY";
    print $out "  BGCOLOR=\"FFFFFF\">\n";
    print $out "<H1>$name</H1>\n";

    # write synopsis descriptions
    print $out "<H2>Synopsis</H2>\n";
    print $out "<TABLE WIDTH=\"100% CELLPADDING=\"0\">\n";
    print $out "<TR>\n";
    print $out "<TD WIDTH=\"1%\"></TD>\n";
    print $out "<TD BGCOLOR=\"#DDDDDD\">\n";
    print $out "<PRE>\n";

    my($count) = 0;
    foreach $i (@$info)
    {
	foreach (split(/\n+/, $i->[&INFO_INDEX]{'SYNOPSIS'}[&PARM_DESC]))
	{
	    s/^\s+//;
	    if ($_)
	    {
		s/</\&lt;/g;
		s/>/\&gt;/g;
		print $out "$_\n";
		$count++;
	    }
	}
    }
    print $out "\n" if($count);

    # write synopsis declarations
    my($maxtype) = 0;
    my($maxname) = 0;
    foreach $i (@info)
    {
	my($len) = length($i->[&INFO_PARMS][0][&PARM_TYPE]);
	$maxtype = $len if ($len > $maxtype);
	my($name) = $i->[&INFO_PARMS][0][&PARM_NAME];
	$len = length($name);
	$len += 2 if ($i->[&INFO_TYPE] == &TYPE_TYPEDEF
		      && $i->[&INFO_PARMS][1][&PARM_NAME]);
	$maxname = $len if ($len > $maxname);
    }

    my($group) = $info->[0][&INFO_GROUP];
    my($grouptype) = $info->[0][&INFO_TYPE];
    $count = 0;
    foreach $i (@$info)
    {
	# skip line before group change
	if ($i->[&INFO_GROUP] ne $group || $i->[&INFO_TYPE] ne $grouptype)
	{
	    $group = $i->[&INFO_GROUP];
	    $grouptype = $i->[&INFO_TYPE];
	    print $out "\n";
	}

	# write function and parameters
	my($arg) = 0;
	my($param) = $i->[&INFO_PARMS];
	my($const) = 0;
	foreach $p (@$param)
	{
	    my($type) = $p->[&PARM_TYPE];
	    my($t) = &type_html($info, $type);
	    my($name) = $p->[&PARM_NAME];
	    if ($name eq "const" && !$type)
	    {
		$const = 1;
		last;
	    }

	    if ($arg == 0)
	    {
		# write function type and name
		my($prefix) = "";
		my($suffix) = "";
		if ($i->[&INFO_TYPE] == &TYPE_TYPEDEF
		    && $i->[&INFO_PARMS][1][&PARM_NAME])
		{
		    $prefix = "(*";
		    $suffix = ")";
		}

		print $out "$t" if($type);
		print $out " "x($maxtype - length($type) + 1);
		print $out "$prefix";
		print $out "<A HREF=\"#$i->[&INFO_TAG]\">$name</A>";
		print $out "$suffix";
		print $out " "x($maxname
				- length($prefix . $name . $suffix)
				+ 1);
		print $out "(" if ($i->[&INFO_PARMS][1][&PARM_NAME]
				   || $i->[&INFO_TYPE] == &TYPE_METHOD);
	    }
	    else
	    {
		# write parameter type and name
		print $out ",\n" . " "x($maxtype + $maxname + 3) if ($arg > 1);
		print $out "$t " if ($type);
		print $out "<I>$name</I>";
	    }
	    $arg++;
	}

	# write function trailer
	print $out ")" if ($i->[&INFO_PARMS][1][&PARM_NAME]
			   || $i->[&INFO_TYPE] == &TYPE_METHOD);
	print $out " const" if ($const);
	print $out ";\n";

	$count++;
    }

    print $out "</PRE></TD></TR></TABLE>\n";

    print $out "<H2>Details</H2>\n";
    $count = 0;
    foreach $i (@$info)
    {
	# write function name
	my($parm) = $i->[&INFO_PARMS];
	my($name) = $parm->[0][&PARM_NAME];
	print $out "<BR>\n";
	print $out "<H3>";
	print $out "<A NAME=\"$i->[&INFO_TAG]\"></A>" if ($i->[&INFO_TAG]);
	print $out "$name</H3>\n";

	# write function and parameters
	print $out "<TABLE CELLSPACING=\"0\" WIDTH=\"100%\">\n";
	print $out "<TR>\n";
	print $out "<TD WIDTH=\"1%\"></TD>\n";
	print $out "<TD BGCOLOR=\"#DDDDDD\">\n";
	print $out "<PRE>\n";

	my($arg) = 0;
	my($pre) = 0;
	my($const) = 0;
	foreach $p (@$parm)
	{
	    my($type) = $p->[&PARM_TYPE];
	    my($t) = &type_html($info, $type);
	    my($name) = $p->[&PARM_NAME];
	    if ($name eq "const" && !$type)
	    {
		$const = 1;
		last;
	    }
	    
	    $t .= " " if ($type);
	    $type .= " " if ($type);
	    if ($arg == 0)
	    {
		# write function type and name
		$name =~ s/^$i->[&INFO_GROUP]:://
		    if ($i->[&INFO_TYPE] != &TYPE_CLASS);

		if ($i->[&INFO_TYPE] == &TYPE_TYPEDEF
		    && $i->[&INFO_PARMS][1][&PARM_NAME])
		{
		    $name = "(*$name)";
		}

		print $out "$t";
		print $out "$name";
		print $out " (" if ($i->[&INFO_PARMS][1][&PARM_NAME]
				    || $i->[&INFO_TYPE] == &TYPE_METHOD);
		$pre = length($type) + length($name) + 2;
	    }
	    else
	    {
		# write parameter type and name
		print $out ",\n" . " "x$pre if ($arg > 1);
		print $out "$t" if ($type);
		print $out "<I>$name</I>";
	    }
	    $arg++;
	}
	
	# write function trailer
	print $out ")" if ($i->[&INFO_PARMS][1][&PARM_NAME]
			   || $i->[&INFO_TYPE] == &TYPE_METHOD);
	print $out " const" if ($const);
	print $out ";\n";

	# write parameter descriptions
	if ($parm->[1][&PARM_NAME] ne "")
	{
	    print $out "\n";
	    $arg = 0;
	    foreach $p (@$parm)
	    {
		my($name) = $p->[&PARM_NAME];
		my($desc) = $p->[&PARM_DESC];
		last if ($name eq "const" && !$type);

		$desc = &desc_html($info, "", $desc);
		
		if ($arg)
		{
		    # write parameter name and description
		    print $out "     <I>$name</I> : $desc\n";
		}
		$arg++;
	    }
	}

	print $out "</PRE></TD></TR></TABLE>\n";

	# write description
	my($desc) = $i->[&INFO_PARMS][0][&PARM_DESC];
	if ($desc)
	{
	    $desc = &desc_html($info, "", $desc);
	    print $out "<TABLE CELLSPACING=\"0\" WIDTH=\"100%\"><BR>\n";
	    print $out "<TR>\n";
	    print $out "<TD WIDTH=\"1%\"></TD>\n";
	    print $out "<TD>\n";
	    print $out "$desc";
	    print $out "</TD></TR></TABLE></BR>\n";
	}

	# write additional sections
	my($sect) = $i->[&INFO_SECTS];
	foreach $s (@$sect)
	{
	    my($name) = $s->[&SECT_NAME];
	    my($desc) = $s->[&SECT_DESC];

	    next if(uc($name) eq "SYNOPSIS");

	    $name = uc(substr($name, 0, 1)) . lc(substr($name, 1));
	    $desc = &desc_html($info, $name, $desc);

	    print $out "<H4>$name</H4>\n";
	    print $out "<TABLE CELLSPACING=\"0\" WIDTH=\"100%\">\n";
	    print $out "<TR>\n";
	    print $out "<TD WIDTH=\"1%\"></TD>\n";
	    print $out "<TD>\n";

	    print $out ((uc($name) eq &SECT_NAME_EXAMPLE)
			? "<PRE>":"<P>");
	    print $out "\n$desc\n";
	    print $out ((uc($name) eq &SECT_NAME_EXAMPLE)
			? "</PRE>\n":"</P>\n");

	    print $out "</TD></TR></TABLE>\n";
	}

	$count++;
    }

    # write trailer
    print $out "<BR>\n"x20;
    print $out "</BODY>\n";
    print $out "</HTML>\n";
}
