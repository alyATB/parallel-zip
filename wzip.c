#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sched.h>

void compressRLE(FILE* fp){
	if (fp == NULL){
		exit(EXIT_FAILURE);
	}
	
	int curr;
	int prev = fgetc(fp);
	uint32_t count;
	
	while (prev != EOF){
		curr = fgetc(fp);
		count = 1;
		// Count matching consecutive characters
		while (curr == prev){
			count++;
			curr = fgetc(fp);
		}
		
		// Write to stdout, 4-bit integer count followed by 1 bit character
		fwrite(&count, sizeof(count), 1, stdout);
		fwrite(&prev, sizeof(char), 1, stdout);
		
		prev = curr;	// for next iteration
	}
}


int main(int argc, char* argv[]){
	if (argc <= 1){
		printf("No files selected to zip, exiting program..\n");
		exit(1);
	}

	int i;
	FILE* fp;
	
	struct timeval start;
   	struct timeval end;
	gettimeofday(&start, NULL);

	// For all file arguments, call compressRLE
	for (i = 1; i < argc; i++){
		fp = fopen(argv[i], "r");
		if (fp == NULL){
			printf("Error opening file %s, aborting program..\n", argv[i]);
			exit(EXIT_FAILURE);
		}
		compressRLE(fp);
		fclose(fp);
	}
	
	gettimeofday(&end, NULL);
    fprintf(stderr, "Linear/Sequential zip time is : %f seconds\n", (float) (end.tv_sec * 1000000 + end.tv_usec - start.tv_sec * 1000000 - start.tv_usec) / 1000000);

	return 0;
}
