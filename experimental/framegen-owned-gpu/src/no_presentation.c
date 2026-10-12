/* Compute-only diagnostic: display ownership is deliberately unsupported.
 * Copyright (C) 2026 ANHAR. GPL-3.0-or-later. */
#include "wsi_present_backend.h"
VkBool32 ps5vk_wsi_present_available(void){return VK_FALSE;}
VkResult ps5vk_wsi_present_open(VkDevice d,const VkImage i[2],struct ps5vk_wsi_present **out){(void)d;(void)i;if(out)*out=0;return VK_ERROR_FEATURE_NOT_PRESENT;}
VkResult ps5vk_wsi_present_frame(struct ps5vk_wsi_present*p,uint32_t s,uint64_t t){(void)p;(void)s;(void)t;return VK_ERROR_FEATURE_NOT_PRESENT;}
VkResult ps5vk_wsi_present_close(struct ps5vk_wsi_present*p){(void)p;return VK_ERROR_FEATURE_NOT_PRESENT;}
