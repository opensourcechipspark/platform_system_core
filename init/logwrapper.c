/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "init.h"

#include "private/android_filesystem_config.h"
#include "log.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cutils/klog.h>


static int memlog_fd = -1;
#define LOG_BUF_MAX 512
static int memlog_level = MEMLOG_DEFAULT_LEVEL;
void memlog_init(void)
{
    static const char *name = "/dev/initLog";
    memlog_fd = open(name, O_WRONLY|O_CREAT, 0644);
}
void memlog_close(void)
{
    if (memlog_fd > 0)
     close(memlog_fd);
}

void memlog_write(int level, const char *fmt, ...)
{
    char buf[LOG_BUF_MAX];
    va_list ap;

    if (level > memlog_level) return;
    if (memlog_fd < 0) return;

    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_MAX, fmt, ap);
    buf[LOG_BUF_MAX - 1] = 0;
    va_end(ap);
    write(memlog_fd, buf, strlen(buf));
}


static int parent(const char *tag, int parent_read) {
    int status;
    char buffer[4096];

    int a = 0;  // start index of unprocessed data
    int b = 0;  // end index of unprocessed data
    int sz;
    while ((sz = read(parent_read, &buffer[b], sizeof(buffer) - 1 - b)) > 0) {

        sz += b;
        // Log one line at a time
        for (b = 0; b < sz; b++) {
            if (buffer[b] == '\r') {
                buffer[b] = '\0';
            } else if (buffer[b] == '\n') {
                buffer[b] = '\0';
                ERRORLOG2MEM("%s\n",&buffer[a]);
                a = b + 1;
            }
        }

        if (a == 0 && b == sizeof(buffer) - 1) {
            // buffer is full, flush
            buffer[b] = '\0';
            ERRORLOG2MEM("%s\n",&buffer[a]);
            b = 0;
        } else if (a != b) {
            // Keep left-overs
            b -= a;
            memmove(buffer, &buffer[a], b);
            a = 0;
        } else {
            a = 0;
            b = 0;
        }

    }
    // Flush remaining data
    if (a != b) {
        buffer[b] = '\0';
        ERRORLOG2MEM("%s\n",&buffer[a]);
    }
    status = 0xAAAA;
    if (wait(&status) != -1) {  // Wait for child
        if (WIFEXITED(status)) {
            if(WEXITSTATUS(status))
            {
                ERROR("%s terminated by exit(%d)\n", tag,
                    WEXITSTATUS(status));
            }
            else
            {
                ERRORLOG2MEM("%s terminated by exit(%d)\n", tag,
                        WEXITSTATUS(status));
            }
            return WEXITSTATUS(status);
        } else if (WIFSIGNALED(status))
            ERROR("%s terminated by signal %d\n", tag,
                    WTERMSIG(status));
        else if (WIFSTOPPED(status))
            ERROR("%s stopped by signal %d\n", tag,
                    WSTOPSIG(status));
    } else
        ERROR("%s wait() failed: %s (%d)\n", tag,
                strerror(errno), errno);
    return -EAGAIN;
}

static void child(int argc, char* argv[]) {
    // create null terminated argv_child array
    char* argv_child[argc + 1];
    memcpy(argv_child, argv, argc * sizeof(char *));
    argv_child[argc] = NULL;

    // XXX: PROTECT FROM VIKING KILLER
    if (execv(argv_child[0], argv_child)) {
        ERROR("executing %s failed: %s\n", argv_child[0], strerror(errno));
        exit(-1);
    }
}

static int logwrap(int argc, char* argv[])
{
    pid_t pid;

    int parent_ptty;
    int child_ptty;
    char *child_devname = NULL;

    /* Use ptty instead of socketpair so that STDOUT is not buffered */
    parent_ptty = open("/dev/ptmx", O_RDWR);
    if (parent_ptty < 0) {
	ERROR("Cannot create parent ptty\n");
	return -errno;
    }

    if (grantpt(parent_ptty) || unlockpt(parent_ptty) ||
            ((child_devname = (char*)ptsname(parent_ptty)) == 0)) {
	ERROR("Problem with /dev/ptmx\n");
	return -1;
    }

    pid = fork();
    if (pid < 0) {
	    ERROR("Failed to fork\n");
        return -errno;
    } else if (pid == 0) {
        child_ptty = open(child_devname, O_RDWR);
        if (child_ptty < 0) {
	    ERROR("Problem with child ptty\n");
            return -errno;
        }

        // redirect stdout and stderr
        close(parent_ptty);
        dup2(child_ptty, 1);
        dup2(child_ptty, 2);
        close(child_ptty);

        child(argc, argv);
    } else {
        return parent(argv[0], parent_ptty);
    }

    return 0;
}

static char E2FSCK_PATH[] = "/sbin/e2fsck";

int ext_check(const char *devpath)
{

    ERRORLOG2MEM("ext_check(%s):\n", devpath);


    if (access(E2FSCK_PATH, X_OK)) {
        ERROR("ext_check(%s): %s not found (skipping checks)\n",
             devpath, E2FSCK_PATH);
        return 0;
    }

    char *args[5];

    args[0] = E2FSCK_PATH;
    args[1] = "-v";
    args[2] = "-y";
    args[3] = devpath;
    args[4] = NULL;

    int rc = logwrap(4, args);

    if (rc == 0) {
        ERRORLOG2MEM("filesystem '%s' had no errors\n", devpath);
    } else if (rc == 1) {
        ERROR("filesystem '%s' had corrected errors\n", devpath);
        rc = 0;
    } else if (rc == 2) {
        ERROR("VOL volume '%s' had corrected errors (system should be rebooted)\n", devpath);
        rc = -EIO;
    } else if (rc == 4) {
        ERROR("VOL volume '%s' had uncorrectable errors\n", devpath);
        rc = -EIO;
    } else if (rc == 8) {
        ERROR("Operational error while checking volume '%s'\n", devpath);
        rc = -EIO;
    } else {
        ERROR("Unknown e2fsck exit code (%d)\n", rc);
        rc = -EIO;
    }
    return rc;
}

static char FSCK_MSDOS_PATH[] = "/system/bin/fsck_msdos";

int vfat_check(const char *devpath)
{

    ERRORLOG2MEM("vfat_check(%s):\n", devpath);


    if (access(FSCK_MSDOS_PATH, X_OK)) {
        ERROR("vfat_check(%s): %s not found (skipping checks)\n",
             devpath, E2FSCK_PATH);
        return 0;
    }

    char *args[5];

    args[0] = FSCK_MSDOS_PATH;
    args[1] = "-p";
    args[2] = "-f";
    args[3] = devpath;
    args[4] = NULL;

    int rc = logwrap(4, args);

    if (rc != 0) {
        ERRORLOG2MEM("filesystem '%s' had errors\n", devpath);
        rc = -EIO;
    }
    return rc;
}


