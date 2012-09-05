/********************************************************************
 * vqec_cfg_macros.h
 * 
 * Definitions of the ARR macros of the VQE Config Library.
 *
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 **********************************************************************/

#include "vqec_cfg_decl.h"

/* Clean up first */
#undef ARR_BEGIN
#undef ARR_ELEM
#undef ARR_DONE

#if defined(__DECLARE_CONFIG_ARR__)

#define ARR_BEGIN(arr) vqec_cfg_item_t arr[] = {
#define ARR_ELEM(name,code,type,desc,attribute_support, override_support, ctor, update, nsid, status)  \
    {name, type, desc},
#define ARR_DONE {NULL, 0, NULL} };

#elif defined(__DECLARE_CONFIG_NUMS__)

#define ARR_BEGIN(arr) typedef enum { 
#define ARR_ELEM(name,code,type,desc,attribute_support, override_support, ctor, update, nsid, status) code,
#define ARR_DONE } vqec_cfg_enum_t;

#elif defined(__DECLARE_CONFIG_ATTRIBUTES__)

#define ARR_BEGIN(arr) boolean vqec_cfg_attribute_support[] = {
#define ARR_ELEM(name,code,type,desc,attribute_support, override_support, ctor, update, nsid, status) attribute_support,
#define ARR_DONE FALSE };

#elif defined(__DECLARE_CONFIG_OVERRIDES__)

#define ARR_BEGIN(arr) boolean vqec_cfg_override_support[] = {
#define ARR_ELEM(name,code,type,desc,attribute_support, override_support, ctor, update, nsid, status) override_support,
#define ARR_DONE FALSE };

#elif defined(__DECLARE_CONFIG_UPDATE_TYPE__)

#define ARR_BEGIN(arr) uint32_t vqec_cfg_update_type[] = {
#define ARR_ELEM(name,code,type,desc,attribute_support, override_support, ctor, update, nsid, status) update,
#define ARR_DONE FALSE };

#elif defined(__DECLARE_CONFIG_NAMESPACE_ID__)

#define ARR_BEGIN(arr) uint32_t vqec_attributes_namespace_id[] = {
#define ARR_ELEM(name,code,type,desc,attribute_support, override_support, ctor, update, nsid, status) nsid,
#define ARR_DONE FALSE };

#elif defined(__DECLARE_CONFIG_STATUS__)

#define ARR_BEGIN(arr) boolean vqec_cfg_status[] = {
#define ARR_ELEM(name,code,type,desc,attribute_support, override_support, ctor, update, nsid, status) (status),
#define ARR_DONE FALSE };

#elif defined(__DECLARE_CONFIG_IS_STATUS_CURRENT__)

#define ARR_BEGIN(arr) boolean vqec_cfg_is_status_current[] = {
#define ARR_ELEM(name,code,type,desc,attribute_support, override_support, ctor, update, nsid, status) (status == VQEC_PARAM_STATUS_CURRENT),
#define ARR_DONE FALSE };
#endif

/* Some other include file might want to reuse us... */
#undef __DECLARE_CONFIG_ARR__
#undef __DECLARE_CONFIG_NUMS__
#undef __DECLARE_CONFIG_ATTRIBUTES__
#undef __DECLARE_CONFIG_OVERRIDES__
#undef __DECLARE_CONFIG_UPDATE_TYPE__
#undef __DECLARE_CONFIG_NAMESPACE_ID__
#undef __DECLARE_CONFIG_STATUS__
#undef __DECLARE_CONFIG_IS_STATUS_CURRENT__
