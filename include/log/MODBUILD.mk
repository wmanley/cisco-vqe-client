#
# Copyright (c) 2008 by Cisco Systems, Inc.
# All Rights Reserved.
#

##########
# MODBUILD.mk
##########

include ../../vqe.mk
include ../../environ.mk

COMMON_HEADERS 	= 			libdebug.h 		\
					libdebug_macros.h	\
					syslog_all_const.h	\
					syslog_facility_num.h 	\
					syslog.h		\
					syslog_macros.h		\
					syslog_macros_types.h	\
					vqe_id.h		\
					vqes_syslog_limit.h	\
					vqe_utils_syslog_def.h  \

include $(KMODOBJBASE)/kmodrules.mk
