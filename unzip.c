#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>


void decode(FILE* fp){
	if (fp == NULL){
		exit(EXIT_FAILURE);
	}

	int curr;
	uint32_t count;
	int i;
	
	while ( fread(&count, sizeof(count), 1, fp) == 1 && fread(&curr, sizeof(char), 1, fp) == 1){
		for (i = 0; i < (int)count; i++){
			printf("%c", curr);	// print to stdout when piping to another file
		}
	}
}


int main(int argc, char* argv[]){
	if (argc <= 1){
		printf("No files selected to unzip, exiting program..\n");
		exit(1);
	}

	int i;
	FILE* fp;

	for (i = 1; i < argc; i++){
		fp = fopen(argv[i], "r");
		if (fp == NULL){
			printf("Error opening file %s, aborting program..\n", argv[i]);
			exit(EXIT_FAILURE);
		}

		decode(fp);
		fclose(fp);

	}

	return 0;
}
