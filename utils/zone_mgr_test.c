/*------------------------------------------------------------------
 * Zone Manager Test
 *
 * Copyright (c) 2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#include "utils/queue_plus.h"
#include <utils/vam_debug.h>
#include <utils/zone_mgr.h>

int
main(int argc, const char * argv[]){
	int i;
	void *p[1002];

	struct vqe_zone *z = zone_alloc("channel1", ZONE_CLASS_LOCAL, ZONE_FLAGS_DYNAMIC, 133, 1002, 0, 0, 0);	
	for (i = 0; i < 1001; i++)
		p[i] = zone_acquire(z);
	for (i = 0; i < 1001; i++)
		zone_release(z, p[i]);
	for (i = 0; i < 1003; i++)
		p[i] = zone_acquire(z);
	for (i = 0; i < 1002; i++)
		zone_release(z, p[i]);
	zone_destroy(z);

	struct vqe_zone *z1 = zone_alloc("channel_2", ZONE_CLASS_SHARED, ZONE_FLAGS_STATIC, 133, 1002, 1, 0, 0);  
	for (i = 0; i < 1001; i++)
		p[i] = zone_acquire(z1);
	for (i = 0; i < 1001; i++)
		zone_release(z1, p[i]);
	for (i = 0; i < 1003; i++)
		p[i] = zone_acquire(z1);
	for (i = 0; i < 1002; i++)
		zone_release(z1, p[i]);

	return 1;
}
