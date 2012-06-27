/* 
 * V3 Control utility
 * (c) Jack lange, 2010
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/ioctl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <string.h>
 
#include "v3_ctrl.h"

int read_file(int fd, int size, unsigned char * buf);

int main(int argc, char* argv[]) {
    char * filename = argv[1];
    char * name = argv[2];
    int guest_fd = 0;
    int v3_fd = 0;
    struct v3_guest_img guest_img;
    struct stat guest_stats;
 
    memset(&guest_img, 0, sizeof(struct v3_guest_img));

    if (argc <= 2) {
	printf("usage: v3_ctrl <guest_img> <vm name>\n");
	return -1;
    }

    printf("Launching guest: %s\n", filename);

    guest_fd = open(filename, O_RDONLY); 

    if (guest_fd == -1) {
	printf("Error Opening guest image: %s\n", filename);
	return -1;
    }

    if (fstat(guest_fd, &guest_stats) == -1) {
	printf("ERROR: Could not stat guest image file -- %s\n", filename);
	return -1;
    }

    
    guest_img.size = guest_stats.st_size;
    
    // load guest image into user memory
    guest_img.guest_data = malloc(guest_img.size);
    if (!guest_img.guest_data) {
        printf("ERROR: Could not allocate memory for guest image\n");
        return -1;
    }

    read_file(guest_fd, guest_img.size, guest_img.guest_data);
    
    close(guest_fd);

    printf("Loaded guest image. Launching to V3Vee\n");
    
    strncpy(guest_img.name, name, 127);


    v3_fd = open(v3_dev, O_RDONLY);

    if (v3_fd == -1) {
	printf("Error opening V3Vee control device\n");
	return -1;
    }

    ioctl(v3_fd, V3_START_GUEST, &guest_img); 



    /* Close the file descriptor.  */ 
    close(v3_fd); 
 


    return 0; 
} 



int read_file(int fd, int size, unsigned char * buf) {
    int left_to_read = size;
    int have_read = 0;

    while (left_to_read != 0) {
	int bytes_read = read(fd, buf + have_read, left_to_read);

	if (bytes_read <= 0) {
	    break;
	}

	have_read += bytes_read;
	left_to_read -= bytes_read;
    }

    if (left_to_read != 0) {
	printf("Error could not finish reading file\n");
	return -1;
    }
    
    return 0;
}
