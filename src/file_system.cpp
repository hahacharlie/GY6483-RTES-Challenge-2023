/*
MIT License

CopyRight (c) 2023 Charlie Wu.
Copyright (c) 2021 Haoran Wang.
Copyright (c) 2020 Steffen S.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <mbed.h>
#include "file_system.h"

BlockDevice *bd = BlockDevice::get_default_instance();
LittleFileSystem fs("fs");
FILE *f; // file handle
DigitalOut redled(LED_RED);

// erase the block device if corrupted
void erase()
{
    printf("\r\nInitializing the block device... ");
    redled = !redled;
    fflush(stdout);
    int err = bd->init();
    printf("%s\n", (err ? "Fail :(" : "OK"));
    if (err)
    {
        error("error: %s (%d)\n", strerror(-err), err);
    }

    printf("Erasing the block device... ");
    fflush(stdout);
    err = bd->erase(0, bd->size());
    printf("%s\n", (err ? "Fail :(" : "OK"));
    if (err)
    {
        error("error: %s (%d)\n", strerror(-err), err);
    }

    printf("Deinitializing the block device... ");
    fflush(stdout);
    err = bd->deinit();
    printf("%s\n", (err ? "Fail :(" : "OK"));
    if (err)
    {
        error("error: %s (%d)\n", strerror(-err), err);
    }
    redled = !redled;
}

// Mount the file system
void MountFileSystem()
{
    printf("Mounting the filesystem... ");
    fflush(stdout);
    int err = fs.mount(bd);
    printf("%s\n", (err ? "Fail :(" : "OK"));
    if (err)
    {
        // Reformat if we can't mount the filesystem
        printf("formatting... ");
        fflush(stdout);
        err = fs.reformat(bd);
        printf("%s\n", (err ? "Fail :(" : "OK"));
        if (err)
        {
            error("error: %s (%d)\n", strerror(-err), err);
        }
    }
}

// Unmount the file system
void UnmountFileSystem()
{
    printf("Unmounting... ");
    fflush(stdout);
    int err = fs.unmount();
    printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0)
    {
        error("error: %s (%d)\n", strerror(-err), err);
    }
}

// write data to file
void WriteFile(float data, int index) {
    printf("Opening \"/fs/data.txt\"... ");
    fflush(stdout);

    FILE *f = fopen("/fs/data.txt", "r+");

    printf("%s\n", (!f ? "Fail :(" : "OK"));
    if (!f) {
        printf("No file found, creating a new file... ");
        fflush(stdout);

        f = fopen("/fs/data.txt", "w+");

        printf("%s\n", (!f ? "Fail :(" : "OK"));
        if (!f) {
            error("error: %s (%d)\n", strerror(errno), -errno);
        }
    }

    printf("Seeking file... ");
    fflush(stdout);

    int err = fseek(f, index * sizeof(float), SEEK_SET);

    printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }

    // Store number
    fwrite(&data, sizeof(float), 1, f);

    // Flush between write and read on the same file
    fflush(f);

    // Close the file, which also flushes any cached writes
    printf("Closing \"/fs/data.txt\"... ");
    fflush(stdout);

    err = fclose(f);

    printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }
}

// read data from file
void ReadFile(float *record, int index) {
    printf("Opening \"/fs/data.txt\"... ");
    fflush(stdout);

    FILE *f = fopen("/fs/data.txt", "r+");

    printf("%s\n", (!f ? "Fail :(" : "OK"));
    if (!f) {
        printf("No file found, creating a new file... ");
        fflush(stdout);

        f = fopen("/fs/data.txt", "w+");

        printf("%s\n", (!f ? "Fail :(" : "OK"));
        if (!f) {
            error("error: %s (%d)\n", strerror(errno), -errno);
        }
    }

    printf("Seeking file... ");
    fflush(stdout);

    int err = fseek(f, index * sizeof(float), SEEK_SET);

    printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }

    size_t items_read = fread(record, sizeof(float), 1, f);
    if (items_read != 1) {
        printf("Failed to read value.\n");
    }

    printf("Closing \"/fs/data.txt\"... ");
    fflush(stdout);

    err = fclose(f);

    printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }
}