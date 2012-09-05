#!/bin/sh
#
#-------------------------------------------------------------------------
# This script will generate vqe filter for facility ids defined in
# include/log/syslog_facility_num.h. The reason for generating the 
# facility filter for syslog-ng conf file is to ensure there are no 
# gaps in facility numbers defined in syslog_facility_num.h file and
# the conf file. 
#
# Copyright (c) 2009-2010 by Cisco Systems, Inc.
# All rights reserved.
#-------------------------------------------------------------------------------

echo "Generating syslog-ng.conf file......"
echo
cur_path=`pwd`
hpath=${cur_path/vqes_obj*/include/log}
#path=$(dirname $1)
path=$1
#baseflag is vqes or vqetools or ist
baseflag=$(basename $1)


if [ -f ${path}/syslog-ng.conf ]; then
    rm -f ${path}/syslog-ng.conf
fi

echo '@version:3.0
#
# configuration file for syslog-ng, customized for VQE logging
#
options {
    flush_lines (0);
    time_reopen (10);
    log_fifo_size (4096);
    long_hostnames (off);
    use_dns (no);
    use_fqdn (no);
    create_dirs (yes);
    keep_hostname (yes);
};

#
# Internal logging
#
source s_internal { internal(); };
destination d_syslognglog { file("/var/log/syslog-ng.log"); };
log { source(s_internal); destination(d_syslognglog); };

#
# Local sources
#
source s_local {
	unix-dgram("/dev/log");
	file("/proc/kmsg");
};

#
# Local filters
#
filter f_messages { level(info..emerg) and not filter("f_vqefilter"); };
filter f_debugmessages { level(debug) and not filter("f_vqefilter"); };
filter f_secure { facility(authpriv); };
filter f_mail { facility(mail); };
filter f_cron { facility(cron); };
filter f_emerg { level(emerg); };
filter f_spooler { level(crit..emerg) and facility(uucp, news); };
filter f_local7 { facility(local7); };
filter f_vqefilter {' >> ${path}/syslog-ng.conf

awk '/LOG_EX_SYSLOG/{f=1;next}/LOG_NFACILITIES/{exit}f' \
${hpath}/syslog_facility_num.h | grep -v "VQEC" | \
awk -F" " '{print $3}' | sed '1d' | sed 's/<<3)/) or /g' | \
sed 's/(/  facility(/g' | sed '$s/...$//' >> ${path}/syslog-ng.conf

echo '; };' >> ${path}/syslog-ng.conf
echo '
#
# Local destinations
#
destination d_messages { file("/var/log/messages"); };
destination d_debugmessages { file("/var/log/debugmessages"); };
destination d_secure { file("/var/log/secure"); };
destination d_maillog { file("/var/log/maillog"); };
destination d_cron { file("/var/log/cron"); };
destination d_console { usertty("root"); };
destination d_spooler { file("/var/log/spooler"); };
destination d_bootlog { file("/var/log/boot.log"); };
destination d_vqe { file("/var/log/vqe/vqe.log" fsync(no)); };

#
# Local logs - order DOES matter !
#
log { source(s_local); filter(f_emerg); destination(d_console); };
log { source(s_local); filter(f_secure); destination(d_secure); flags(final); };
log { source(s_local); filter(f_mail); destination(d_maillog); flags(final); };
log { source(s_local); filter(f_cron); destination(d_cron); flags(final); };
log { source(s_local); filter(f_spooler); destination(d_spooler); };
log { source(s_local); filter(f_local7); destination(d_bootlog); };
log { source(s_local); filter(f_vqefilter); destination(d_vqe); };
log { source(s_local); filter(f_messages); destination(d_messages); };
log { source(s_local); filter(f_debugmessages); destination(d_debugmessages); };
' >>  ${path}/syslog-ng.conf
       
echo "File ${path}/syslog-ng.conf was generated successfully !!"
echo

