#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>

#define DEVICE  "/dev/video0"
//#define STOPPING

void determine_streaming_methods(int fd);
void check_mmap_streaming(int fd);
void determine_formats(int fd);

int main() {
    struct v4l2_capability vcap;
    int video_fd = -1;
    printf("Starting V4L2 test application...\n");

    printf("Using device: %s\n", DEVICE);
    video_fd = open(DEVICE, O_RDWR);
    if(video_fd < 0) {
        printf("ERROR: failed to open device! error: %d\n", video_fd);
        return 1;
    }

    printf("Querying caps...\n");
    if( ioctl(video_fd, VIDIOC_QUERYCAP, &vcap) < 0) {
        printf("ERROR: failed to query caps!\n");
        return 1;
    }
    printf("CAPS: \n");
    printf("  driver:      '%s'\n", vcap.driver);
    printf("  card:        '%s'\n", vcap.card);
    printf("  bus_info:    '%s'\n", vcap.bus_info);
    printf("  version:     %08x\n", vcap.version);
    printf("  capabilites: %08x\n", vcap.capabilities);

    if( !(vcap.capabilities & V4L2_CAP_STREAMING) ) {
        printf("This device does not support streaming!\n");
    }
    else {
        printf("This device supports streaming!\nTaking a closer look:\n");
        determine_streaming_methods(video_fd);
    }

    printf("\nZooming in on MMAP streaming...\n");
    check_mmap_streaming(video_fd);

    printf("\nChecking video formats...\n");
    determine_formats(video_fd);

    close(video_fd);

    printf("\n");
    return 0;
}

void determine_formats(int fd) {
    struct v4l2_fmtdesc format;
    struct v4l2_frmsizeenum res;
    struct v4l2_frmivalenum rate;
    int i,j,k;

    for(i = 0; ; i++) {
        memset(&format, 0, sizeof(format));
        format.index = i;
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == ioctl (fd, VIDIOC_ENUM_FMT, &format)) {
            if (errno == EINVAL) {
                printf("No more formats found!\n");
                break;
            }
            else 
                printf("ERROR: Something else went wrong!\n");
        } 
        else {
            printf("Format found!\n");
            printf("  Description: %s\n", format.description);
            printf("  Flags: %08x\n", format.flags);
            for(j = 0; ; j++) {
                memset(&res, 0, sizeof(res));
                res.index = j;
                res.pixel_format = format.pixelformat;
                if (-1 == ioctl (fd, VIDIOC_ENUM_FRAMESIZES, &res)) {
                    if (errno == EINVAL) {
                        printf("  No more framesizes found!\n");
                        break;
                    }
                    else 
                        printf("ERROR: Something else went wrong!\n");
                } 
                else {
                    printf("  Framesize found!\n");
                    printf("    Type (1=discrete): %d\n", res.type);
                    printf("    Size: %d x %d\n", res.discrete.width, res.discrete.height);
                    for(k = 0; ; k++) {
                        memset(&rate, 0, sizeof(rate));
                        rate.index = k;
                        rate.pixel_format = format.pixelformat;
                        rate.width = res.discrete.width;
                        rate.height = res.discrete.height;
                        if (-1 == ioctl (fd, VIDIOC_ENUM_FRAMEINTERVALS, &rate)) {
                            if (errno == EINVAL) {
                                printf("    No more framerates found!\n");
                                break;
                            }
                            else 
                                printf("ERROR: Something else went wrong!\n");
                        } 
                        else {
                            printf("    Framerate found!\n");
                            printf("      Type (1=discrete): %d\n", rate.type);
                            printf("      FPS: %f\n", 1.0 / (float) rate.discrete.numerator * (float) rate.discrete.denominator);
                        }
                    }
                }
            }
        }
    }
}


void determine_streaming_methods(int fd) {
    struct v4l2_requestbuffers reqbuf;
    
    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = 20;
    
    if (-1 == ioctl (fd, VIDIOC_REQBUFS, &reqbuf)) {
        if (errno == EINVAL)
            printf("Video capturing or mmap-streaming is not supported\n");
        else
            printf("ERROR: Something else went wrong!\n");
    } 
    else
        printf("MMAP streaming seems to be supported\n");

    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_USERPTR;
    
    if (-1 == ioctl (fd, VIDIOC_REQBUFS, &reqbuf)) {
        if (errno == EINVAL)
            printf("Video capturing or user pointer streaming is not supported\n");
        else
            printf("ERROR: Something else went wrong!\n");
    } 
    else
        printf("User pointer streaming seems to be supported\n");

    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_DMABUF;
    reqbuf.count = 1;
    
    if (-1 == ioctl (fd, VIDIOC_REQBUFS, &reqbuf)) {
        if (errno == EINVAL)
            printf("Video capturing or DMABUF streaming is not supported\n");
        else
            printf("ERROR: Something else went wrong!\n");
    } 
    else
        printf("DMABUF streaming seems to be supported\n");
}

typedef struct {
    void *start;
    size_t length;
} mybuf;

void check_mmap_streaming(int fd) {
    struct v4l2_requestbuffers reqbuf;
    int nr_buffers = 3;
    int i;
    mybuf *buffers;
    
    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = nr_buffers;
   
#ifdef STOPPING
    printf("Press ENTER to continue...\n");
    getchar();
#endif

    if (-1 == ioctl (fd, VIDIOC_REQBUFS, &reqbuf)) {
        if (errno == EINVAL)
            printf("Video capturing or mmap-streaming is not supported\n");
        else
            printf("ERROR: Something else went wrong!\n");
    } 

    printf("We asked for %d buffers, we got %d buffers.\n", nr_buffers, reqbuf.count);

#ifdef STOPPING
    printf("Press ENTER to continue...\n");
    getchar();
#endif

    buffers = calloc(reqbuf.count, sizeof(mybuf)); 

    for(i = 0; i < reqbuf.count; i++) {
        struct v4l2_buffer buffer;
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if(-1 == ioctl(fd, VIDIOC_QUERYBUF, &buffer)) {
            printf("Something went wrong in the QUERYBUF call!\n");
        }

        printf("buffer %d: \n", i);
        printf("  length:      '%d'\n", buffer.length);
        printf("  flags:       %08x\n", buffer.flags);
        printf("  offset:      %08x\n", buffer.m.offset);
        printf("  timeval:     %ld.%ld\n", buffer.timestamp.tv_sec, buffer.timestamp.tv_usec);

        buffers[i].length = buffer.length;
        buffers[i].start = mmap(NULL, buffer.length,
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED,
                                fd, buffer.m.offset);
        if(MAP_FAILED == buffers[i].start) {
            printf("Memory mapping failed!\n");
        }

        if(-1 == ioctl(fd, VIDIOC_QUERYBUF, &buffer)) {
            printf("Something went wrong in the QUERYBUF call!\n");
        }

        printf("buffer %d: \n", i);
        printf("  length:      '%d'\n", buffer.length);
        printf("  flags:       %08x\n", buffer.flags);
        printf("  offset:      %08x\n", buffer.m.offset);
        printf("  timeval:     %ld.%ld\n", buffer.timestamp.tv_sec, buffer.timestamp.tv_usec);
    }

    printf("Memory mapping complete!\n");

#ifdef STOPPING
    printf("Press ENTER to continue...\n");
    getchar();
#endif

    for(i=0; i < reqbuf.count; i++)
        munmap(buffers[i].start, buffers[i].length);

    printf("unmapping complete!\n");

#ifdef STOPPING
    printf("Press ENTER to continue...\n");
    getchar();
#endif

}
