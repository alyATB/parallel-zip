#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sched.h>

// Single compressed file content as a string
typedef struct compressedFile {
	char* data;
	int* count;
	int size;
} compressedFile;


void* compressRLE(void* args){
	char* fileName = (char*)args;
	FILE* fp = fopen(fileName, "r");
	if (fp == NULL){
		printf("Error opening file %s, aborting program..\n", fileName);
		exit(EXIT_FAILURE);
	}
	
	compressedFile* cf = malloc(sizeof(compressedFile));
	long long n = 100000000;
	cf->data = (char*)malloc(n * sizeof(char));
	cf->count = (int*)malloc(n * sizeof(int));
	int curr;
	int prev = fgetc(fp);
	int charCount;
	int itr = 0;

	while (prev != EOF){
		curr = fgetc(fp);
		charCount = 1;
		// Count matching consecutive characters
		while (curr == prev){
			charCount++;
			curr = fgetc(fp);
		}
		
	/*	fwrite(&count, sizeof(count), 1, stdout);
		fwrite(&prev, sizeof(char), 1, stdout);
	*/	
		// Write encoded info in relative compressedFile struct members
		cf->count[itr] = charCount;
		cf->data[itr] = prev;
		cf->size++;
		itr++;
		prev = curr;	// for next iteration
	}

	fclose(fp);

	return (void*)cf;
}


int main(int argc, char* argv[]){
	if (argc <= 1){
		printf("No files selected to zip, exiting program..\n");
		exit(1);
	}
	
	compressedFile* outputBuffer;
	int totalFiles = argc - 1;
	pthread_t threads[totalFiles];
	int i;
	
	struct timeval start;
   	struct timeval end;
	gettimeofday(&start, NULL);

	// For each file argument, call let a thread handle its compression
	for (i = 0; i < totalFiles; i++){		
		pthread_create(&threads[i], NULL, &compressRLE, argv[i+1]);
	}

	int fileSize;
	char character;
	uint32_t count;
	int j;	
	// Wait for all threads to return in order
	for (i = 0; i < totalFiles; i++){
		pthread_join(threads[i], (void**)&outputBuffer);
		fileSize = outputBuffer->size;
		// Write to stdout the returned structs (in order)
		for (j = 0; j < fileSize; j++){
			count = (uint32_t)outputBuffer->count[j];
			character = outputBuffer->data[j];
			fwrite(&count, sizeof(count), 1, stdout);
			fwrite(&character, sizeof(char), 1, stdout);
		}
		/*
		free(outputBuffer->count);
		free(outputBuffer->data);
		free(outputBuffer);
		*/
	}
	
	gettimeofday(&end, NULL);
    fprintf(stderr, "Inefficient parallel zip time is : %f seconds\n", (float) (end.tv_sec * 1000000 + end.tv_usec - start.tv_sec * 1000000 - start.tv_usec) / 1000000);
    	
	return 0;
}
