#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>     

#define DEVICE "/dev/video0"
#define WIDTH 320
#define HEIGHT 240
#define FRAME_INTERVAL 30

struct buffer {
    void   *start;
    size_t  length;
};

static int xioctl(int fd, int request, void *arg) {
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

int main() {
    int fd = open(DEVICE, O_RDWR);
    if (fd == -1) {
        perror("Opening video device");
        return 1;
    }

    // Set format to MJPEG
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = WIDTH;
    fmt.fmt.pix.height      = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("Setting Pixel Format");
        return 1;
    }

    // Request buffer
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("Requesting Buffer");
        return 1;
    }

    // Query buffer
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
        perror("Querying Buffer");
        return 1;
    }

    struct buffer buffer;
    buffer.length = buf.length;
    buffer.start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    if (buffer.start == MAP_FAILED) {
        perror("Memory mapping failed");
        return 1;
    }

    // Start streaming
    if (xioctl(fd, VIDIOC_STREAMON, &buf.type) == -1) {
        perror("Start Capture");
        return 1;
    }

    // Prepare output directory
    mkdir("frames", 0755);

    int frame_count = 0;

    while (1) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = 0;

        if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("Queue Buffer");
            break;
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = {2, 0}; // Timeout

        int r = select(fd + 1, &fds, NULL, NULL, &tv);
        if (r == -1) {
            perror("Waiting for Frame");
            break;
        }

        if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
            perror("Retrieving Frame");
            break;
        }

        frame_count++;

        if (frame_count % FRAME_INTERVAL == 0) {
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);

            char filename[256];
            snprintf(filename, sizeof(filename), "frames/frame_%04d%02d%02d_%02d%02d%02d.jpg",
                     tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                     tm.tm_hour, tm.tm_min, tm.tm_sec);

            FILE *fout = fopen(filename, "wb");
            if (fout) {
                fwrite(buffer.start, buf.bytesused, 1, fout);
                fclose(fout);
                printf("[SAVED] %s (%d bytes)\n", filename, buf.bytesused);
            } else {
                perror("File write error");
            }
        }
    }

    // Cleanup
    munmap(buffer.start, buffer.length);
    close(fd);
    return 0;
}
