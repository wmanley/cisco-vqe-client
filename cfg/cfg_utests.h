/* 
 * CFG unit test program.
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 */


/*****************************************************************
 *             Channel configuration testsuite
 ****************************************************************/
int channel_testsuite_init(void);
int channel_testsuite_cleanup(void);
void test_channel_add(void);
void test_channel_delete(void);
void test_channel_copy(void);
void test_channel_compare(void);
void test_channel_get(void);
void test_parse_channels(void);
void test_parse_single_channel(void);
void test_channel_valid1(void);
void test_channel_valid2(void);
void test_channel_valid3(void);
void test_channel_valid4(void);
void test_channel_valid5(void);
void test_channel_valid6(void);
void test_channel_valid7(void);
void test_channel_valid8(void);
void test_channel_valid9(void);
void test_channel_valid10(void);
void test_channel_valid11(void);
void test_channel_valid12(void);
void test_channel_valid13(void);
void test_channel_valid14(void);
void test_channel_valid15(void);
void test_channel_valid16(void);
void test_channel_valid17(void);
void test_channel_valid18(void);
void test_channel_valid19(void);
void test_channel_valid20(void);
void test_channel_valid21(void);
void test_channel_valid22(void);
void test_channel_valid23(void);
void test_channel_valid24(void);
void test_channel_valid25(void);
void test_channel_valid26(void);
void test_channel_valid27(void);
void test_channel_valid28(void);
void test_channel_valid29(void);


/*****************************************************************
 *             Channel database testsuite
 ****************************************************************/
int database_testsuite_init(void);
int database_testsuite_cleanup(void);
void test_db_open(void);
void test_db_close(void);
void test_db_read(void);
void test_db_write(void);

/*****************************************************************
 *             Channel configuration manager testsuite
 ****************************************************************/
int cfgmgr_testsuite_init(void);
int cfgmgr_testsuite_cleanup(void);
void test_cfgmgr_init(void);
void test_cfgmgr_shutdown(void);
void test_cfgmgr_update(void);
void test_cfgmgr_save(void);
void test_cfgmgr_access(void);
void test_cfgmgr_removeFBTs(void);


