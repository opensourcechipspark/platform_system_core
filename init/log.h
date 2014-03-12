/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _INIT_LOG_H_
#define _INIT_LOG_H_

#include <cutils/klog.h>

extern void memlog_init(void);
extern void memlog_close(void);
extern void memlog_write(int level, const char *fmt, ...);

#define MEMLOG_ERROR(tag,x...)   memlog_write(3, "<3>" tag ": " x)
#define MEMLOG_WARNING(tag,x...) memlog_write(4, "<4>" tag ": " x)
#define MEMLOG_NOTICE(tag,x...)  memlog_write(5, "<5>" tag ": " x)
#define MEMLOG_INFO(tag,x...)    memlog_write(6, "<6>" tag ": " x)
#define MEMLOG_DEBUG(tag,x...)   memlog_write(7, "<7>" tag ": " x)

#define ERROR(x...)   KLOG_ERROR("init", x)
#define NOTICE(x...)  KLOG_NOTICE("init", x)
#define INFO(x...)    KLOG_INFO("init", x)

#define ERRORLOG2MEM(x...)   MEMLOG_ERROR("init", x)

#define LOG_UEVENTS        0  /* log uevent messages if 1. verbose */

#define MEMLOG_DEFAULT_LEVEL  3  /* messages <= this level are logged */

#endif
