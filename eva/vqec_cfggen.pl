#!/usr/bin/perl

#********************************************************************
#
# Copyright (c) 2008-2011 by cisco Systems, Inc.
# All rights reserved.
#
# This perl module takes vqec_cfg_settings.h as input and can generate:
#     (a) The group-attribute schema file.
#     (b) The database schema file.
#     (c) A pre-processor definitions file, vqec_cfg_limits.h for VQE-C.
#     (d) A html file consisting of attribute documentation.
#
#*********************************************************************/

sub Usage
{
  $help =
       "\n vqec_cfggen.pl  -in <in-file> [-group_schema <group_schema_file>]
             [-db_schema <db_schema_file>] [-client <client_limits_file>] [-html <doc_file>]

        (a) <in-file> is the input file: supply eva/vqec_cfg_settings.h here.
        (b) If a location for <group_schema_file> is provided, the group-attribute 
             will be generated and written to this file. The output should be copied to
             vcds-ems/web/WEB-INF/grp_cfg_schema.xsd.
        (c) If a location for <db_schema_file> is provided, the client database schema
             will be generated and written to this file. The output should be copied to
             vcds-ems/web/WEB-INF/client_db_schema.xsd.
        (d) If a location for <client_limits_file> is provided, the client preprocessor limit
             defines will be generated and written to this file. The output file should be
             eva/vqec_cfg_limits.h.
        (e) If a location for <doc_file> is provided, the documentation for the attributes
             supported by VQE-C will be generated.

         example: 
                    vqec_cfggen.pl -in vqec_cfg_settings.h 
                                   -group_schema grp_cfg_schema.xsd
                                   -db_schema client_db_schema.xsd
                                   -client vqec_cfg_limits.h
                                   -html attrib_doc.html\n\n";

  print STDERR $help;
}

#
# schema version - this corresponds only to the syntax of the schema
# and not to the attributes version.
#
my $schema_version = 1;

my $do_group_schema = 0;
my $do_attrib_schema = 0;
my $do_client = 0;
my $do_html = 0;
my $in_file = "";
my $group_schema_file = "";
my $attrib_schema_file = "";
my $client_file = "";
my $html_file = "";
my $attributes_version = 0;
my $attributes_major_version = 0;
my $in_post_comment;
my %namespace_id_list;

while ($#ARGV >= 0) {
    $arg = shift @ARGV;
    if ($arg eq "-group_schema") {
        $do_group_schema = 1;
        $group_schema_file = shift @ARGV;
    } elsif ($arg eq "-in")  {
        $in_file = shift @ARGV;
    } elsif ($arg eq "-db_schema") {
        $do_db_schema = 1;
        $db_schema_file = shift @ARGV;
    } elsif ($arg eq "-client") {
        $do_client = 1;
        $client_file = shift @ARGV;
    } elsif ($arg eq "-html") {
        $do_html = 1;
        $html_file = shift @ARGV;
    } else {
        &Usage;
        die "Bad option $arg\n";
    }
}

open IN,'<',$in_file || die &Usage;

if ($do_group_schema) {
    open(GROUPSCHEMA,'>',$group_schema_file) ||
      die "cannot open $group_schema_file";
}
if ($do_db_schema) {
    open(DBSCHEMA,'>',$db_schema_file) ||
      die "cannot open $db_schema_file";
}
if ($do_client) {
    open(CLIENT, '>',$client_file) || die &Usage;
}
if ($do_html) {
    open(HTML, '>',$html_file) || die &Usage;
}

# strip comments

$/ = undef;
$_ = <IN>; 

s#/\*[^*]*\*+([^/*][^*]*\*+)*/|([^/"']*("[^"\\]*(\\[\d\D][^"\\]*)*"[^/"']*|'[^'\\]*(\\[\d\D][^'\\]*)*'[^/"']*|/+[^*/][^/"']*)*)#$2#g;

#
# get "attribute-set" version
#
$in_post_comment = $_;
s/(.|\s)*(VQEC_ATTRIBUTES_VERSION\s*)((\w+)(\.\d+))((.|\s)*)/\4/;
$attributes_version = $_;
s/(\w+)\..*/\1/;
$attributes_major_version = $_;
$_ = $in_post_comment;
s/(.|\s)*($attributes_major_version)( |\t)+(\d+)(.|\s)*/\4/;
$attributes_major_version = $_;
$_ = $in_post_comment;
s/(.|\s)*(VQEC_ATTRIBUTES_VERSION\s*)((\w+)(\.)(\d+))((.|\s)*)/\6/;
$attributes_version = $_;
s/(\d+)\..*/\1/;
$attributes_minor_version = $_;
$_ = $in_post_comment;

# strip preprocessor lines (removes all \'s)
s/\s*\\\s*//g;
s/^#.*//mg;

my @parameters;
my @initializer;
@array_elems = split /ARR\_BEGIN/;
$_ = $array_elems[$#array_elems];
@array_elems = split /ARR\_ELEM\(/;
for ($i = 1; $i < $#array_elems; $i++) {

  #
  # remove end-of-str ")"
  #

  $_ = $array_elems[$i];
  s/(\s*)(.*)(\)[\s]*$)/\2/;
  $attribs = $_;

  #
  # get constructor arguments
  #
  $_ = $attribs;
  s/((\s|.)*)(VQEC_[\w]*_CONSTRUCTOR\()((.|\s)*)(\)[^\)]*)((\s|.)*)/\4/;
  $constructor_args = $_;

  #
  # get constructor type
  #
  $_ = $attribs;
  s/((\s|.)*)(VQEC_[\w]*_CONSTRUCTOR\()((.|\s)*)(\)[^\)]*)((\s|.)*)/\3/;
  $constructor_type = $_;

  #
  # remove constructor str
  # 
  $_ = $attribs;
  s/(VQEC_[\w]*_CONSTRUCTOR\((.|\s)*\))([^\)]*)/\3/;
  $attribs = $_;

  #
  # parse attributes
  #
  $_ = $attribs;
  @fields = split /,/;

  $_ = $fields[0];
  s/(\s*")((\w|\.)+)("\s*)/\2/;
  $name = $_;

  $_ = $fields[1];
  s/(\s*)(\w+)(\s*)/\2/;
  $enum = $_;

  $_ = $fields[2];
  s/(\s*)(\w+)(\s*)/\2/;
  $type = $_;

  $_ = $fields[3];
  s/(^\s*)((\w|\s)*)(\s*)/\2/;
  s/"//g;
  s/(\S*)(\s*)/\1 /g;
  s/\s*$//;
  $docum = $_;


  $_ = $fields[4];
  s/(\s*)(\w+)(\s*)/\2/;
  $is_attrib = $_;

  $_ = $fields[5];
  s/(\s*)(\w+)(\s*)/\2/;
  $is_override = $_;

  $_ = $fields[7];
  s/(\s*)(\w+)(\s*)/\2/;
  $update_type = $_;

  $_ = $fields[8];
  s/(\s*)(\w+)(\s*)/\2/;
  $namespace_id = $_;

  $_ = $fields[9];
  s/(\s*)(\w+)(\s*)/\2/;
  $status = $_;

  #
  # find value for namespace id
  #
  $namespace_id_val = 0;
  if (!exists($namespace_id_list{$namespace_id})) {
    $_ = $in_post_comment;
    s/^(.|\s)*($namespace_id)( |\t)+(\d+)(.|\s)*/\4/;
    $namespace_id_val = $_;
    $namespace_id_list{$namespace_id} = $namespace_id_val;
  } else {
    $namespace_id_val = $namespace_id_list{$namespace_id};
  }

  #
  # parse constructor type and constructor args
  #
  if ($constructor_type =~ /^(VQEC_UINT32_CONSTRUCTOR\()/ ) {
    $ctype = "UI32";
  } elsif ($constructor_type =~ /^(VQEC_UINT32_RANGE_CONSTRUCTOR\()/ ) {
    $ctype = "UI32_RANGE";
  } elsif ($constructor_type =~ /^(VQEC_BOOL_CONSTRUCTOR\()/ ) {
    $ctype = "BOOL";
  } elsif ($constructor_type =~ /^(VQEC_STRING_CONSTRUCTOR\()/ ) {
    $ctype = "STR";
  } elsif ($constructor_type =~ /^(VQEC_MULTICAST_ADDR_CONSTRUCTOR\()/ ) {
    $ctype = "MCASTADDR";
  } elsif ($constructor_type =~ /^(VQEC_STRING_LIST_CONSTRUCTOR\()/ ) {
    $ctype = "LIST";
  }

  $initializer = {TYPE => $ctype, ARGS => $constructor_args};

  #
  # save all data in a hash table
  #
  $parameters[$i] = {NAME => $name, 
		     ENUM => $enum, 
		     TYPE => $type, 
		     DOCSTR => $docum, 
		     ISATTR => $is_attrib, 
		     ISOVERRIDE => $is_override, 
		     UPDTYPE => $update_type,
		     NAMESPACEID => $namespace_id_val,
                     STATUS => $status,
		     INITIALIZER => $initializer};
}

#
# print an attribute's default, minimum or maximum value
#
sub print_attribute_value
  {
    $_ = $attribute_enum;
    s/(VQEC_CFG)(.*)/VQEC_SYSCFG_$attribute_oper\2/;
    $prn = "#define ".$_;
    print CLIENT $prn;
    for ($l = 0; $l < 60 - length($prn); $l++) {
      print CLIENT " ";
    }
    if ($attribute_type ne "VQEC_TYPE_STRING") {
      print CLIENT "(".$attribute_value.")\n";
    } else {
      print CLIENT $attribute_value."\n";
    }
  }

my $cisco_copyright =
  "/******************************************************************************
  *
  * Cisco Systems, Inc.
  *
  * Copyright (c) 2008-2011 by Cisco Systems, Inc.
  * All rights reserved.
  *
  ******************************************************************************/\n\n";
my $warn = 
  "/* THIS IS GENERATED CODE.  DO NOT EDIT. */\n\n";
#YouView2
my $static_defines = "
#define VQEC_SYSCFG_SIG_MODE_STD \"std\"
#define VQEC_SYSCFG_SIG_MODE_NAT \"nat\"
#define VQEC_SYSCFG_SIG_MODE_MUX \"mux\"
#define VQEC_SYSCFG_VOD_MODE_IPTV \"iptv\"
#define VQEC_SYSCFG_VOD_MODE_CABLE \"cable\"
#define VQEC_MAX_NAME_LEN        (255)\n\n";

if ($do_client) {
    print CLIENT $cisco_copyright;
    print CLIENT $warn;
    print CLIENT $static_defines;

    for ($i = 1; $i <= $#parameters; $i++) {
      $enum    = $parameters[$i]->{'ENUM'};
      $initializer = $parameters[$i]->{'INITIALIZER'};
      $type = $initializer->{'TYPE'};
      $_ = $initializer->{'ARGS'};
      @args = split /,/;
      for ($j = 0; $j <= $#args; $j++) {
	$_ = $args[$j];
	s/(\s*)(\w*)(\s*)/\2/;
	$args[$j] = $_;
      }

      print CLIENT "/*****\n";
      print CLIENT " * ".$parameters[$i]->{'NAME'}."\n";
      print CLIENT " ******/\n";
      
      $attribute_type = $parameters[$i]->{'TYPE'};
      $attribute_enum = $enum;
      if ($type eq "UI32") {
	$attribute_oper = "DEFAULT";
	$attribute_value = $args[0];
	&print_attribute_value;
	$attribute_oper = "MIN";
	$attribute_value = $args[1];
	&print_attribute_value;
	$attribute_oper = "MAX";
	$attribute_value = $args[2];
	&print_attribute_value;

	print CLIENT 
	  "static inline boolean is_".(lc $attribute_enum)."_valid (uint32_t val) {\n";
	$min = $args[1];
	$max = $args[2];
	if ((($min) != ($max)) && (($min) != 0)) {
	  print CLIENT "    if ((val >= (".$min.")) && (val <= (".$max."))) {\n";
	  print CLIENT "        return (TRUE);\n    }\n";
	} elsif (($min) != ($max)) {
	  print CLIENT "    if (val <= (".$max.")) {\n";
	  print CLIENT "        return (TRUE);\n    }\n";
	} else {
	  print CLIENT "    if (val == (".$min.")) {\n";
	  print CLIENT "        return (TRUE);\n    }\n";
	}	
	print CLIENT "    return (FALSE);\n}\n";

      } elsif ($type eq "BOOL") {
	$attribute_oper = "DEFAULT";
	$attribute_value = $args[0];
	&print_attribute_value;

      } elsif ($type eq "STR") {
	$attribute_oper = "DEFAULT";
	$attribute_value = $args[0];
	&print_attribute_value;
      } elsif ($type eq "MCASTADDR") {
	$attribute_oper = "DEFAULT";
	$attribute_value = $args[0];
	&print_attribute_value;
      } elsif ($type eq "LIST") {
	for ($k = 0; $k <= $#args; $k++) {
	  $_ = $args[$k];
	  if ($_ =~ /^(__DEFAULT)/ ) {
	    s/((\s|.)*__DEFAULT\s*)(w*)/\2/;
	    $attribute_oper = "DEFAULT";
	    $attribute_value = $_;
	    &print_attribute_value;
	  }
	}

	print CLIENT 
	  "static inline boolean is_".(lc $attribute_enum)."_valid (char *str) {\n";
	print CLIENT "    if (";
	for ($k = 0; $k <= $#args; $k++) {
	  $_ = $args[$k];
	  if ($_ =~ /^(__DEFAULT)/ ) {
	    s/((\s|.)*__DEFAULT\s*)(w*)/\2/;
	    $str = $_;
	  } else {
	    $str = $args[$k];
	  }
	  if ($k == 0) {
	    print CLIENT "(strcmp(".$str.", str) != 0)";  
	  } else {
	    print CLIENT " ||\n       (strcmp(".$str.", str) != 0)";  
	  }
	}
	print CLIENT ") {\n        return (FALSE);\n    }\n";
	print CLIENT "    return (TRUE);\n}\n";

      } elsif ($type eq "UI32_RANGE") {
	$attribute_oper = "DEFAULT";
	$attribute_value = $args[0];
	&print_attribute_value;

	for ($k = 1; $k <= $#args; $k++) {
	  $_ = $args[$k];
	  #
	  # split at :
	  #
	  @minmax = split /:/;
	  $_ = $minmax[0];
	  s/\s*(\S*)\s*/\1/;
	  $attribute_oper = "MIN_R".($k - 1);
	  $attribute_value = $_;
	  &print_attribute_value;
	  $_ = $minmax[1];
	  s/\s*(\S*)\s*/\1/;
	  $attribute_oper = "MAX_R".($k - 1);
	  $attribute_value = $_;
	  &print_attribute_value;
	}

	print CLIENT 
	  "static inline boolean is_".(lc $attribute_enum)."_valid (uint32_t val) {\n";
	for ($k = 1; $k <= $#args; $k++) {
	  $_ = $args[$k];
	  @minmax = split /:/;
	  $_ = $minmax[0];
	  s/\s*(\S*)\s*/\1/;
	  $min = $_;
	  $_ = $minmax[1];
	  s/\s*(\S*)\s*/\1/;
	  $max = $_;
	  if ((($min) != ($max)) && (($min) != 0)) {
	    print CLIENT "    if ((val >= (".$min.")) && (val <= (".$max."))) {\n";
	  } elsif (($min) != ($max)) {
	    print CLIENT "    if (val <= (".$max.")) {\n";
	  } else {
	    print CLIENT "    if (val == ".$min.") {\n";
	  }
	  print CLIENT "        return (TRUE);\n    }\n";
	}
	print CLIENT "    return (FALSE);\n}\n";
      }
      print CLIENT "\n";
    }
}


my $max_client_db_records = 5000000;
my $num_groups = 1024;
my $default_group_id = 0;
my $vqe_namespace = "\"http://www.cisco.com/vqe/vqec-syscfg1.0\"";
my $xml_namespace = "\"http://www.w3.org/2001/XMLSchema\"";
my $doc_linelen = 80;

my $schema_id =
  "<xs:schema xmlns:vqe=$vqe_namespace
  xmlns:xs=$xml_namespace
  targetNamespace=$vqe_namespace
  version=\"$schema_version.$attributes_major_version.$attributes_minor_version\"
  elementFormDefault=\"qualified\">\n\n";

my $schema_warn = 
  "\n\t<!-- THIS SCHEMA IS AUTO-GENERATED.  DO NOT EDIT. -->\n\n";

my @update_modes_desc = ("No update possible", 
		    "Parameter takes effect at startup",
		    "Parameter takes effect immediately",
		    "Parameter takes effect for new channel changes");

my @update_modes = ("invalid", 
		    "startup",
		    "immediate",
		    "channel-change");

sub pr_newline_with_tabs
  {
    print $output_desc "\n";
    for ($ntabs = 0; $ntabs < $current_indent_level; $ntabs++) {
      print $output_desc "\t";
    }
  }

sub pr_newline
  {
    print $output_desc "\n";
  }

sub pr_tabs
  {
    for ($ntabs = 0; $ntabs < $current_indent_level; $ntabs++) {
      print $output_desc "\t";
    }
  }

sub print_documentation
  {
    $tmp = $doc;
    for ($m = 0;$m < (length $doc);) {
      $_ = $tmp;
      s/((\S|\s){50})(\S+)(\s)(\S|\s)*/\1\3/;
      $tmp = substr($doc, $m + (length $_));
      $m += length $_;

      if ((length $_) != 0) {
	&pr_tabs;
	print $output_desc $_;
	&pr_newline;
      }
    }
  }

sub print_update_type
  {
    if ($update_how eq "VQEC_UPDATE_STARTUP") {
      print GROUPSCHEMA 
	"<!-- ".$update_modes_desc[1]." -->";
    } elsif ($update_how eq "VQEC_UPDATE_IMMEDIATE") {
      print GROUPSCHEMA 
	"<!-- ".$update_modes_desc[2]." -->";
    } elsif ($update_how eq "VQEC_UPDATE_NEWCHANCHG") {
      print GROUPSCHEMA 
	"<!-- ".$update_modes_desc[3]." -->";
    }
  }

sub print_update_str
  {
    if ($update_how eq "VQEC_UPDATE_STARTUP") {
      print GROUPSCHEMA 
	$update_modes[1];
    } elsif ($update_how eq "VQEC_UPDATE_IMMEDIATE") {
      print GROUPSCHEMA 
	$update_modes[2];
    } elsif ($update_how eq "VQEC_UPDATE_NEWCHANCHG") {
      print GROUPSCHEMA 
	$update_modes[3];
    }
  }

sub schema_uint32_elem
  {
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ************************ -->";
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ".$name." -->";
    &pr_newline_with_tabs;
    &print_update_type;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ************************ -->";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:simpleType name=\"".$name."_base\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:restriction base=\"xs:nonNegativeInteger\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:minInclusive value=\"".$uint_min."\"/>";
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:maxInclusive value=\"".$uint_max."\"/>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA  "</xs:restriction>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA  "</xs:simpleType>";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:element name=\"".$name."\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:annotation>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:documentation>";
    &pr_newline;
    $current_indent_level++;
    &print_documentation;
    $current_indent_level--;
    &pr_tabs;
    print GROUPSCHEMA "</xs:documentation>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:annotation>";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:complexType>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:simpleContent>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:extension base=\"vqe:".$name."_base\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:attribute name=\"update_type\" type=\"xs:string\" default=\"";
    &print_update_str;
    print GROUPSCHEMA "\"/>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:extension>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:simpleContent>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:complexType>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:element>";
    &pr_newline;
  }


sub schema_bool_elem
  {
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ************************ -->";
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ".$name." -->";
    &pr_newline_with_tabs;
    &print_update_type;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ************************ -->";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:simpleType name=\"".$name."_base\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:restriction base=\"xs:boolean\">";
    &pr_newline_with_tabs;
    print GROUPSCHEMA  "</xs:restriction>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA  "</xs:simpleType>";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:element name=\"".$name."\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:annotation>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:documentation>";
    &pr_newline;
    $current_indent_level++;
    &print_documentation;
    $current_indent_level--;
    &pr_tabs;
    print GROUPSCHEMA "</xs:documentation>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:annotation>";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:complexType>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:simpleContent>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:extension base=\"vqe:".$name."_base\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:attribute name=\"update_type\" type=\"xs:string\" default=\"";
    &print_update_str;
    print GROUPSCHEMA "\"/>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:extension>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:simpleContent>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:complexType>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:element>";
    &pr_newline;
  }


sub schema_str_elem
  {
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ************************ -->";
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ".$name." -->";
    &pr_newline_with_tabs;
    &print_update_type;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ************************ -->";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:simpleType name=\"".$name."_base\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:restriction base=\"xs:string\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:minLength value=\"".$strlen_min."\"/>";
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:maxLength value=\"".$strlen_max."\"/>";
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:pattern value='\"[^\\s]+\"'/>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA  "</xs:restriction>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA  "</xs:simpleType>";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:element name=\"".$name."\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:annotation>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:documentation>";
    &pr_newline;
    $current_indent_level++;
    &print_documentation;
    $current_indent_level--;
    &pr_tabs;
    print GROUPSCHEMA "</xs:documentation>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:annotation>";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:complexType>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:simpleContent>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:extension base=\"vqe:".$name."_base\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:attribute name=\"update_type\" type=\"xs:string\" default=\"";
    &print_update_str;
    print GROUPSCHEMA "\"/>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:extension>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:simpleContent>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:complexType>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:element>";
    &pr_newline;
  }


sub schema_mcastaddr_elem
  {
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ************************ -->";
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ".$name." -->";
    &pr_newline_with_tabs;
    &print_update_type;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ************************ -->";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:simpleType name=\"".$name."_base\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:restriction base=\"xs:string\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:pattern value='(\"(22[4-9]|23[0-9])\\.(([1-9]?[0-9]"
      ."|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\\.){2}([1-9]?[0-9]|1[0-9][0-9]|2[0-4]"
      ."[0-9]|25[0-5])\")|(\"0\\.0\\.0\\.0\")'/>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA  "</xs:restriction>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA  "</xs:simpleType>";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:element name=\"".$name."\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:annotation>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:documentation>";
    &pr_newline;
    $current_indent_level++;
    &print_documentation;
    $current_indent_level--;
    &pr_tabs;
    print GROUPSCHEMA "</xs:documentation>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:annotation>";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:complexType>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:simpleContent>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:extension base=\"vqe:".$name."_base\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:attribute name=\"update_type\" type=\"xs:string\" default=\"";
    &print_update_str;
    print GROUPSCHEMA "\"/>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:extension>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:simpleContent>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:complexType>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:element>";
    &pr_newline;
  }


sub schema_list_elem
  {
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ************************ -->";
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ".$name." -->";
    &pr_newline_with_tabs;
    &print_update_type;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ************************ -->";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:simpleType name=\"".$name."_base\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:restriction base=\"xs:string\">";
    $current_indent_level++;
    &pr_newline;
    for ($k = 0; $k <= $#enum; $k++) {
      &pr_tabs;
      print GROUPSCHEMA "<xs:enumeration value=\'".(lc $enum[$k])."'/>";
      &pr_newline_with_tabs;
      print GROUPSCHEMA "<xs:enumeration value=\'".(uc $enum[$k])."'/>";
      &pr_newline;
    }
    $current_indent_level--;
    &pr_tabs;
    print GROUPSCHEMA  "</xs:restriction>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA  "</xs:simpleType>";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:element name=\"".$name."\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:annotation>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:documentation>";
    &pr_newline;
    $current_indent_level++;
    &print_documentation;
    $current_indent_level--;
    &pr_tabs;
    print GROUPSCHEMA "</xs:documentation>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:annotation>";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:complexType>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:simpleContent>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:extension base=\"vqe:".$name."_base\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:attribute name=\"update_type\" type=\"xs:string\" default=\"";
    &print_update_str;
    print GROUPSCHEMA "\"/>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:extension>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:simpleContent>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:complexType>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:element>";
    &pr_newline;
  }


sub schema_ui32_range_elem
  {
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ************************ -->";
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ".$name." -->";
    &pr_newline_with_tabs;
    &print_update_type;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<!-- ************************ -->";
    &pr_newline;


    for ($x = 1; $x <= $#ranges; $x++) {
      $_ = $ranges[$x];
      #
      # split at :
      #
      @minmax = split /:/;
      $_ = $minmax[0];
      s/\s*(\S*)\s*/\1/;
      $range_min = eval $_;
      $_ = $minmax[1];
      s/\s*(\S*)\s*/\1/;
      $range_max = eval $_;

      &pr_newline_with_tabs;
      print GROUPSCHEMA "<xs:simpleType name=\"".$name."_r".$x."\">";
      $current_indent_level++;
      &pr_newline_with_tabs;
      print GROUPSCHEMA "<xs:restriction base=\"xs:nonNegativeInteger\">";
      $current_indent_level++;
      &pr_newline_with_tabs;
      print GROUPSCHEMA "<xs:minInclusive value=\"".$range_min."\"/>";
      &pr_newline_with_tabs;
      print GROUPSCHEMA "<xs:maxInclusive value=\"".$range_max."\"/>";
      $current_indent_level--;
      &pr_newline_with_tabs;
      print GROUPSCHEMA "</xs:restriction>";
      $current_indent_level--;
      &pr_newline_with_tabs;
      print GROUPSCHEMA "</xs:simpleType>\n";
    }
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:simpleType name=\"".$name."_base\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:union memberTypes=\""; 
    for ($x = 1; $x <= $#ranges; $x++) {
      print GROUPSCHEMA "vqe:".$name."_r".$x." ";
    }
    print GROUPSCHEMA "\"/>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:simpleType>";   
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:element name=\"".$name."\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:annotation>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:documentation>";
    &pr_newline;
    $current_indent_level++;
    &print_documentation;
    $current_indent_level--;
    &pr_tabs;
    print GROUPSCHEMA "</xs:documentation>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:annotation>";
    &pr_newline;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:complexType>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:simpleContent>";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:extension base=\"vqe:".$name."_base\">";
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:attribute name=\"update_type\" type=\"xs:string\" default=\"";
    &print_update_str;
    print GROUPSCHEMA "\"/>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:extension>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:simpleContent>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:complexType>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:element>";
    &pr_newline;
  }



#
# generate group-attribute schema.
#
if ($do_group_schema) {

  $output_desc = GROUPSCHEMA;
  $current_indent_level = 1;

  print GROUPSCHEMA $schema_id;  
  print GROUPSCHEMA $schema_warn;
  
  &pr_newline_with_tabs;
  print GROUPSCHEMA "<!-- STB groupAttributes file with the constraint that groups -->";
  &pr_newline_with_tabs;
  print GROUPSCHEMA "<!-- must be between ".$default_group_id." and ".($num_groups - 1)." -->";
  &pr_newline_with_tabs;
  print GROUPSCHEMA "<!-- ".$default_group_id." is the default identifier -->";
  &pr_newline;
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "<xs:element name=\"GroupFile\" type=\"vqe:GroupFileType\">";
  $current_indent_level++;
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "<xs:unique name=\"Unique-Group\">";
  $current_indent_level++;
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "<xs:selector xpath=\".//vqe:GroupAttribs\"/>";
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "<xs:field xpath=\"\@group\"/>";
  $current_indent_level--;
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "</xs:unique>";
  $current_indent_level--;
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "</xs:element>";

  &pr_newline;
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "<xs:complexType name=\"GroupFileType\">";
  $current_indent_level++;
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "<xs:sequence>";
  $current_indent_level++;
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "<xs:element ref=\"vqe:GroupAttribs\" minOccurs=\"0\" maxOccurs=\"unbounded\"/>";
  $current_indent_level--;
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "</xs:sequence>";
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "<xs:attribute name=\"version\" type=\"xs:string\" use=\"required\"/>";
  $current_indent_level--;
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "</xs:complexType>";

  &pr_newline;
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "<!-- VQE-C attributes that can be defind per group -->";
  &pr_newline;
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "<xs:element name=\"GroupAttribs\">";
  $current_indent_level++;
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "<xs:complexType>";
  $current_indent_level++;
  &pr_newline_with_tabs;  
  print GROUPSCHEMA "<xs:sequence>";
  $current_indent_level++;

  for ($i = 1; $i <= $#parameters; $i++) {
    $name = $parameters[$i]->{'NAME'};
    $is_attr = $parameters[$i]->{'ISATTR'};
    $namespace_val = $parameters[$i]->{'NAMESPACEID'};
    if (($is_attr eq "TRUE") && ($namespace_val == 1)) {
      &pr_newline_with_tabs;
      print GROUPSCHEMA "<xs:element ref=\"vqe:".$name."\" minOccurs=\"0\" maxOccurs=\"1\"/>";
    }
  }
  &pr_newline_with_tabs;
  print GROUPSCHEMA "<xs:element ref=\"vqe:extension\" minOccurs=\"0\" maxOccurs=\"1\"/>";
  &pr_newline_with_tabs;
  print GROUPSCHEMA "<xs:any namespace=\"##other\" processContents=\"lax\"";
  &pr_newline_with_tabs;
  print GROUPSCHEMA " minOccurs=\"0\" maxOccurs=\"unbounded\"/>";
  $current_indent_level--;
  &pr_newline_with_tabs;
  print GROUPSCHEMA "</xs:sequence>";
  
  &pr_newline;
  &pr_newline_with_tabs;
  print GROUPSCHEMA "<!-- Group id attribute -->";
  &pr_newline;

  &pr_newline_with_tabs;
  print GROUPSCHEMA "<xs:attribute name=\"group\" use=\"required\">";
  $current_indent_level++;
  &pr_newline_with_tabs;
  print GROUPSCHEMA "<xs:simpleType>";
  $current_indent_level++;
  &pr_newline_with_tabs;
  print GROUPSCHEMA "<xs:restriction base=\"xs:nonNegativeInteger\">";
  $current_indent_level++;
  &pr_newline_with_tabs;
  print GROUPSCHEMA "<xs:minInclusive value=\"".$default_group_id."\"/>";
  &pr_newline_with_tabs;
  print GROUPSCHEMA "<xs:maxInclusive value=\"".($num_groups - 1)."\"/>";
  $current_indent_level--;
  &pr_newline_with_tabs;
  print GROUPSCHEMA "</xs:restriction>";
  $current_indent_level--;
  &pr_newline_with_tabs;
  print GROUPSCHEMA "</xs:simpleType>";
  $current_indent_level--;
  &pr_newline_with_tabs;
  print GROUPSCHEMA "</xs:attribute>";
  $current_indent_level--;
  &pr_newline;
  &pr_newline_with_tabs;
  print GROUPSCHEMA "</xs:complexType>";
  $current_indent_level--;
  &pr_newline_with_tabs;
  print GROUPSCHEMA "</xs:element>";
  &pr_newline;

  &pr_newline;
  &pr_newline_with_tabs;
  print GROUPSCHEMA "<xs:complexType name=\"extension_type\">";
  $current_indent_level++;
  &pr_newline_with_tabs;
  print GROUPSCHEMA "<xs:sequence>";
  $current_indent_level++;
  &pr_newline_with_tabs;
  print GROUPSCHEMA "<xs:any processContents=\"lax\" minOccurs=\"1\"";
  &pr_newline_with_tabs;
  print GROUPSCHEMA "maxOccurs=\"unbounded\" namespace=\"##targetNamespace\"/>";
  $current_indent_level--;
  &pr_newline_with_tabs;
  print GROUPSCHEMA "</xs:sequence>";
  $current_indent_level--;
  &pr_newline_with_tabs;
  print GROUPSCHEMA "</xs:complexType>";

  &pr_newline;
  &pr_newline_with_tabs;
  if ($attributes_major_version == 1) {
    print GROUPSCHEMA "<xs:element name=\"extension\" type=\"vqe:extension_type\"/>";
  } else {
    print GROUPSCHEMA "<xs:element name=\"extension\" type=\"vqe:extension_type_v2\"/>";
  }

  &pr_newline;  
  for ($i = 2; $i <= $attributes_major_version; $i++) {
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:complexType name=\"extension_type_v".$i."\">";    
    $current_indent_level++;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "<xs:sequence>";
    $current_indent_level++;
    for ($j = 1; $j <= $#parameters; $j++) {
      $name = $parameters[$j]->{'NAME'};
      $is_attr = $parameters[$j]->{'ISATTR'};
      $namespace_val = $parameters[$j]->{'NAMESPACEID'};
      if (($is_attr eq "TRUE") && ($namespace_val == $i)) {
	&pr_newline_with_tabs;
	print GROUPSCHEMA "<xs:element ref=\"vqe:".$name."\" minOccurs=\"0\" maxOccurs=\"1\"/>";
	
      }
    }

    &pr_newline_with_tabs;
    if ($i != $attributes_major_version) {
      print GROUPSCHEMA "<xs:element name=\"extension\" ";
      print GROUPSCHEMA "type=\"vqe:extension_type_v".($i + 1)."\" minOccurs=\"0\"/>";
    } else {
      print GROUPSCHEMA "<xs:element name=\"extension\" ";
      print GROUPSCHEMA "type=\"vqe:extension_type\" minOccurs=\"0\"/>";
    }
	
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:sequence>";
    $current_indent_level--;
    &pr_newline_with_tabs;
    print GROUPSCHEMA "</xs:complexType>";
  }
  &pr_newline;

  for ($i = 1; $i <= $#parameters; $i++) {
    $name = $parameters[$i]->{'NAME'};
    $is_attr = $parameters[$i]->{'ISATTR'};
    $doc = $parameters[$i]->{'DOCSTR'};
    $update_how = $parameters[$i]->{'UPDTYPE'};
    $namespace_val = $parameters[$i]->{'NAMESPACEID'};
    $initializer = $parameters[$i]->{'INITIALIZER'};
    $type = $initializer->{'TYPE'};
    $_ = $initializer->{'ARGS'};
    @args = split /,/;
    for ($j = 0; $j <= $#args; $j++) {
      $_ = $args[$j];
      s/(\s*)(\w*)(\s*)/\2/;
      $args[$j] = $_;
    }
    if ($is_attr eq "TRUE") {
      if ($type eq "UI32") {
	$uint_min = eval $args[1];
	$uint_max = eval $args[2];
	&schema_uint32_elem;
      } elsif ($type eq "BOOL") {
	&schema_bool_elem;
      } elsif ($type eq "STR") {
	$strlen_min = 1;
	$strlen_max = $args[1];
	&schema_str_elem;
      } elsif ($type eq "MCASTADDR") {
	&schema_mcastaddr_elem;
      } elsif ($type eq "LIST") {
	for ($j = 0; $j <= $#args; $j++) {
	  $_ = $args[$j];
	  if ($_ =~ /^(__DEFAULT)/ ) {
	    s/((\s|.)*__DEFAULT\s*)(w*)/\2/;
	    $args[$j] = $_;
	  } 
	}
	@enum = @args;
	&schema_list_elem;
      } elsif ($type eq "UI32_RANGE") {
	@ranges = @args;
	&schema_ui32_range_elem;
      }
    }
  }


  &pr_newline;
  print GROUPSCHEMA "</xs:schema>";
  &pr_newline;
}


#
# client-database schema generation.
#
if ($do_db_schema) {
  print DBSCHEMA $schema_id;
  print DBSCHEMA $schema_warn;

  print DBSCHEMA
    "\n\t<!-- STB customerRecord file with the constraint that cname -->\n";
  print DBSCHEMA
    "\t<!-- must be unique in the file -->\n\n";
  print DBSCHEMA
    "\t<xs:element name=\"customer_file\" type=\"vqe:CustomerFileType\">\n";
  print DBSCHEMA
    "\t</xs:element>\n";

  print DBSCHEMA
    "\n\t<!-- complex type for STB customerRecord file -->\n\n";
  print DBSCHEMA
    "\t<xs:complexType name=\"CustomerFileType\">\n";
  print DBSCHEMA
    "\t\t<xs:sequence>\n";
  print DBSCHEMA
    "\t\t\t<xs:element ref=\"vqe:stbdata\" minOccurs=\"0\" maxOccurs=\"".$max_client_db_records."\"/>\n";
  print DBSCHEMA
    "\t\t</xs:sequence>\n";
  print DBSCHEMA
    "\t\t<!-- update mode must be sync for full-sync or incremental -->\n";
  print DBSCHEMA
    "\t\t<xs:attribute name=\"update_mode\" use=\"required\">\n";
  print DBSCHEMA
    "\t\t\t<xs:simpleType>\n";
  print DBSCHEMA    
    "\t\t\t\t<xs:restriction base=\"xs:string\">\n";
  print DBSCHEMA      
    "\t\t\t\t<xs:enumeration value=\"full\"/>\n";
  print DBSCHEMA
    "\t\t\t\t<xs:enumeration value=\"incremental\"/>\n";
  print DBSCHEMA
    "\t\t\t\t</xs:restriction>\n";
  print DBSCHEMA
    "\t\t\t</xs:simpleType>\n";
  print DBSCHEMA
    "\t\t</xs:attribute>\n";
  print DBSCHEMA
    "\t\t<xs:attribute name=\"version\" type=\"xs:string\" use=\"required\"/>\n";
  print DBSCHEMA
    "\t</xs:complexType>\n";

  print DBSCHEMA 
    "\n\t<!-- complex type for STB customerRecord data -->\n";
  print DBSCHEMA 
    "\t<xs:element name=\"stbdata\">\n";
  print DBSCHEMA 
    "\t\t<xs:complexType>\n";
  print DBSCHEMA 
    "\t\t\t<xs:sequence>\n";
  print DBSCHEMA 
    "\t\t\t\t<xs:element ref=\"vqe:cname\"/>\n";
  print DBSCHEMA 
    "\t\t\t\t<xs:element ref=\"vqe:group\"/>\n";
  print DBSCHEMA 
    "\t\t\t</xs:sequence>\n";
  print DBSCHEMA
    "\t\t</xs:complexType>\n";
  print DBSCHEMA
    "\t</xs:element>\n";

  print DBSCHEMA
    "\n\t<!-- cname of the STB constrained between 1 and 32 chars -->\n";
  print DBSCHEMA 
    "\t<xs:element name=\"cname\">\n";
  print DBSCHEMA  
    "\t\t<xs:simpleType>\n";
  print DBSCHEMA  
    "\t\t\t<xs:restriction base=\"xs:string\">\n";
  print DBSCHEMA  
    "\t\t\t<xs:minLength value=\"1\"/>\n";
  print DBSCHEMA
    "\t\t\t<xs:maxLength value=\"32\"/>\n";
  print DBSCHEMA  
    "\t\t\t</xs:restriction>\n";
  print DBSCHEMA  
    "\t\t</xs:simpleType>\n";
  print DBSCHEMA
    "\t</xs:element>\n";

  print DBSCHEMA
    "\n\t<!-- group of the STB constrained between ".$default_group_id." and ".($num_groups - 1)." -->\n";
  print DBSCHEMA 
    "\t<xs:element name=\"group\">\n";
  print DBSCHEMA  
    "\t\t<xs:simpleType>\n";
  print DBSCHEMA  
    "\t\t\t<xs:restriction base=\"xs:nonNegativeInteger\">\n";
  print DBSCHEMA  
    "\t\t\t<xs:minInclusive value=\"".$default_group_id."\"/>\n";
  print DBSCHEMA
    "\t\t\t<xs:maxInclusive value=\"".($num_groups - 1)."\"/>\n";
  print DBSCHEMA  
    "\t\t\t</xs:restriction>\n";
  print DBSCHEMA  
    "\t\t</xs:simpleType>\n";
  print DBSCHEMA
    "\t</xs:element>\n";

  print DBSCHEMA 
    "\n</xs:schema>\n";
}

#
# generate html
#
if ($do_html) {
  print HTML "<html><head>\n";
  print HTML "<title>VQE-C Attributes</title>\n";
  print HTML "</head>\n";
  print HTML "<body>\n";

  print HTML
    "\t<table width=\"80%\" bgcolor=\"#ffffff\" border=\"6\">\n";

  print HTML
    "\t<caption><B>ATTRIBUTES</B></caption>\n";

  print HTML "<th>Name</th>";
  print HTML "<th>Help</th>";
  print HTML "<th>Version Id</th>";
  print HTML "<th>Status</th>";
  print HTML "<th>Override Support</th>";
  print HTML "<th>Type</th>";
  print HTML "<th>Update</th>";
  print HTML "<th>Default Value</th>";
  print HTML "<th>Minimum / Enum</th>";
  print HTML "<th>Maximum</th>";

  for ($i = 1; $i <= $#parameters; $i++) {
    $name = $parameters[$i]->{'NAME'};
    $status = $parameters[$i]->{'STATUS'};
    $is_attr = $parameters[$i]->{'ISATTR'};
    $is_override = $parameters[$i]->{'ISOVERRIDE'};
    $basetype = $parameters[$i]->{'TYPE'};
    $doc = $parameters[$i]->{'DOCSTR'};
    $update_how = $parameters[$i]->{'UPDTYPE'};
    $initializer = $parameters[$i]->{'INITIALIZER'};
    $type = $initializer->{'TYPE'};
    $namespace_id_val = $parameters[$i]->{'NAMESPACEID'};
    $_ = $initializer->{'ARGS'};
    @args = split /,/;
    for ($j = 0; $j <= $#args; $j++) {
      $_ = $args[$j];
      s/(\s*)(\w*)(\s*)/\2/;
      $args[$j] = $_;
    }
    if ($is_attr eq "TRUE") {
      print HTML 
	"\t<tr>\n";
      print HTML
	"\t\t<td>".$name."</td>\n";
      print HTML 
	"\t\t<td>".$doc."</td>\n";
      print HTML 
	"\t\t<td>".$namespace_id_val."</td>\n";

      if ($status eq "VQEC_PARAM_STATUS_CURRENT") {
	print HTML 
      	  "\t\t<td>current</td>\n";
      } elsif ($status eq "VQEC_PARAM_STATUS_DEPRECATED") {
	print HTML 
      	  "\t\t<td>deprecated</td>\n";
      } elsif ($status eq "VQEC_PARAM_STATUS_OBSOLETE") {
	print HTML 
      	  "\t\t<td>obsolete</td>\n";      
      }

      print HTML
	"\t\t<td>".$is_override."</td>\n";

      if ($basetype eq "VQEC_TYPE_STRING") {
	print HTML 
      	  "\t\t<td>string</td>\n";
      } elsif ($basetype eq "VQEC_TYPE_UINT32_T") {
	print HTML 
      	  "\t\t<td>uint</td>\n";
      } elsif ($basetype eq "VQEC_TYPE_BOOLEAN") {
	print HTML 
      	  "\t\t<td>bool</td>\n";      
      }

      if ($update_how eq "VQEC_UPDATE_STARTUP") {
	print HTML 
	  "\t\t<td>".$update_modes[1]."</td>\n";
      } elsif ($update_how eq "VQEC_UPDATE_IMMEDIATE") {
	print HTML 
	  "\t\t<td>".$update_modes[2]."</td>\n";
      } elsif ($update_how eq "VQEC_UPDATE_NEWCHANCHG") {
	print HTML 
	  "\t\t<td>".$update_modes[3]."</td>\n";
      }

      if ($type eq "UI32") {
	$uint_def = eval $args[0];
	$uint_min = eval $args[1];
	$uint_max = eval $args[2];
	print HTML
	  "\t\t<td>".$uint_def."</td>\n";
	print HTML
	  "\t\t<td>".$uint_min."</td>\n";
	print HTML
	  "\t\t<td>".$uint_max."</td>\n";
      } elsif ($type eq "BOOL") {
	$uint_def = $args[0];
	print HTML
	  "\t\t<td>".$uint_def."</td>\n";
      } elsif ($type eq "STR") {
	$def_str = $args[0];
	$strlen_min = 1;
	$strlen_max = $args[1];
	print HTML
	  "\t\t<td>".$def_str."</td>\n";
	print HTML
	  "\t\t<td>".$strlen_min."</td>\n";
	print HTML
	  "\t\t<td>".$strlen_max."</td>\n";
      } elsif ($type eq "MCASTADDR") {
	$uint_def = eval $args[0];
	print HTML
	  "\t\t<td>".$uint_def."</td>\n";
      } elsif ($type eq "LIST") {
	for ($j = 0; $j <= $#args; $j++) {
	  $_ = $args[$j];
	  if ($_ =~ /^(__DEFAULT)/ ) {
	    s/((\s|.)*__DEFAULT\s*)(w*)/\2/;
	    $args[$j] = $_;
	    print HTML
	      "\t\t<td>".$_."</td>\n"; 
	  } 
	}
	print HTML
	  "\t\t<td>";
	for ($j = 0; $j <= $#args; $j++) {
	  $_ = $args[$j];
	  if ($_ =~ /^(__DEFAULT)/ ) {
	    s/((\s|.)*__DEFAULT\s*)(w*)/\2/;
	    $args[$j] = $_;
	  } 
	  print HTML $_;
	}
	print HTML
	  "</td>\n"; 
      } elsif ($type eq "UI32_RANGE") {
	$uint_def = eval $args[0];
	print HTML
	  "\t\t<td>".$uint_def."</td>\n\t\t<td>";
	for ($j = 1; $j <= $#args; $j++) {
	  print HTML $args[$j]." ";
	}
	print HTML "</td>\n"; 
      }
      print HTML 
	"\t</tr>\n";
    }
  }


  print HTML
    "\t<table width=\"80%\" bgcolor=\"#ffffff\" border=\"6\">\n";
  print HTML
    "\t<caption><B>NON-ATTRIBUTES</B></caption>\n";

  print HTML "<th>Name</th>";
  print HTML "<th>Help</th>";
  print HTML "<th>Version Id</th>";
  print HTML "<th>Status</th>";
  print HTML "<th>Type</th>";
  print HTML "<th>Default Value</th>";
  print HTML "<th>Minimum / Enum</th>";
  print HTML "<th>Maximum</th>";

  for ($i = 1; $i <= $#parameters; $i++) {
    $name = $parameters[$i]->{'NAME'};
    $status = $parameters[$i]->{'STATUS'};
    $is_attr = $parameters[$i]->{'ISATTR'};
    $basetype = $parameters[$i]->{'TYPE'};
    $doc = $parameters[$i]->{'DOCSTR'};
    $update_how = $parameters[$i]->{'UPDTYPE'};
    $initializer = $parameters[$i]->{'INITIALIZER'};
    $type = $initializer->{'TYPE'};
    $namespace_id_val = $parameters[$i]->{'NAMESPACEID'};
    $_ = $initializer->{'ARGS'};
    @args = split /,/;
    for ($j = 0; $j <= $#args; $j++) {
      $_ = $args[$j];
      s/(\s*)(\w*)(\s*)/\2/;
      $args[$j] = $_;
    }
    if ($is_attr ne "TRUE") {
      print HTML 
	"\t<tr>\n";
      print HTML
	"\t\t<td>".$name."</td>\n";
      print HTML 
	"\t\t<td>".$doc."</td>\n";
      print HTML 
	"\t\t<td>".$namespace_id_val."</td>\n";

      if ($status eq "VQEC_PARAM_STATUS_CURRENT") {
	print HTML 
      	  "\t\t<td>current</td>\n";
      } elsif ($status eq "VQEC_PARAM_STATUS_DEPRECATED") {
	print HTML 
      	  "\t\t<td>deprecated</td>\n";
      } elsif ($status eq "VQEC_PARAM_STATUS_OBSOLETE") {
	print HTML 
      	  "\t\t<td>obsolete</td>\n";      
      }

      if ($basetype eq "VQEC_TYPE_STRING") {
	print HTML 
      	  "\t\t<td>string</td>\n";
      } elsif ($basetype eq "VQEC_TYPE_UINT32_T") {
	print HTML 
      	  "\t\t<td>uint</td>\n";
      } elsif ($basetype eq "VQEC_TYPE_BOOLEAN") {
	print HTML 
      	  "\t\t<td>bool</td>\n";      
      }

      if ($update_how eq "VQEC_UPDATE_STARTUP") {
	print HTML 
	  "\t\t<td>".$update_modes[1]."</td>\n";
      } elsif ($update_how eq "VQEC_UPDATE_IMMEDIATE") {
	print HTML 
	  "\t\t<td>".$update_modes[2]."</td>\n";
      } elsif ($update_how eq "VQEC_UPDATE_NEWCHANCHG") {
	print HTML 
	  "\t\t<td>".$update_modes[3]."</td>\n";
      }

      if ($type eq "UI32") {
	$uint_def = eval $args[0];
	$uint_min = eval $args[1];
	$uint_max = eval $args[2];
	print HTML
	  "\t\t<td>".$uint_def."</td>\n";
	print HTML
	  "\t\t<td>".$uint_min."</td>\n";
	print HTML
	  "\t\t<td>".$uint_max."</td>\n";
      } elsif ($type eq "BOOL") {
	$uint_def = $args[0];
	print HTML
	  "\t\t<td>".$uint_def."</td>\n";
      } elsif ($type eq "STR") {
	$def_str = $args[0];
	$strlen_min = 1;
	$strlen_max = $args[1];
	print HTML
	  "\t\t<td>".$def_str."</td>\n";
	print HTML
	  "\t\t<td>".$strlen_min."</td>\n";
	print HTML
	  "\t\t<td>".$strlen_max."</td>\n";
      } elsif ($type eq "MCASTADDR") {
	$uint_def = eval $args[0];
	print HTML
	  "\t\t<td>".$uint_def."</td>\n";
      } elsif ($type eq "LIST") {
	for ($j = 0; $j <= $#args; $j++) {
	  $_ = $args[$j];
	  if ($_ =~ /^(__DEFAULT)/ ) {
	    s/((\s|.)*__DEFAULT\s*)(w*)/\2/;
	    $args[$j] = $_;
	    print HTML
	      "\t\t<td>".$_."</td>\n"; 
	  } 
	}
	print HTML
	  "\t\t<td>";
	for ($j = 0; $j <= $#args; $j++) {
	  $_ = $args[$j];
	  if ($_ =~ /^(__DEFAULT)/ ) {
	    s/((\s|.)*__DEFAULT\s*)(w*)/\2/;
	    $args[$j] = $_;
	  } 
	  print HTML $_;
	}
	print HTML
	  "</td>\n"; 
      } elsif ($type eq "UI32_RANGE") {
	$uint_def = eval $args[0];
	print HTML
	  "\t\t<td>".$uint_def."</td>\n\t\t<td>";
	for ($j = 1; $j <= $#args; $j++) {
	  print HTML $args[$j]." ";
	}
	print HTML "</td>\n"; 
      }
      print HTML 
	"\t</tr>\n";
    }
  }

  print HTML "</body>\n";
  print HTML "</html>\n";
}
