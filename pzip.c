#define _GNU_SOURCE
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <string.h>
#include <sys/sysinfo.h>

/**************************************************************************************************/
// Custom structures //

// Single page info in a file
typedef struct bufferObject {
	char* address;
	int fileNumber;
	int pageNumber;
	int pageSize;
} bufferObject;

// Single compressed file page
typedef struct compressedPage {
	char* data;
	int* count;
	int size;
} compressedPage;


/**************************************************************************************************/
// Global Variables, Constants & Custom Functions Declaration //

// File Info
int totalFilesCount;
int totalPagesCount;
int pageSize;
int* pagesPerFile;

// Buffer queue info.
// Circular queue that's initially empty.
#define queueCapacity 10
bufferObject queue[queueCapacity];
void enqueue(bufferObject b); // Queue enqueue function
bufferObject dequeue(); // Queue dequeue function
unsigned int queueSize = 0; 
unsigned int queueFront = 0;
unsigned int queueBack = 0;

// Threads & locks
int totalThreads;
pthread_mutex_t glock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queueFull = PTHREAD_COND_INITIALIZER;
pthread_cond_t queueEmpty = PTHREAD_COND_INITIALIZER;
int producerThreadDone = 0; 

// Output info, eventually gets built by combining  multiple compressed page structs 
compressedPage* outputStream;

/**************************************************************************************************/
// Buffer Queue operations //

// Queue enqueue.
// Adds a bufferObject struct to
// the back of the buffer queue.
void enqueue(bufferObject b){
	queue[queueBack] = b;
	queueBack = (queueBack + 1) % queueCapacity;
	queueSize++;
}

// Queue dequeue.
// Removes a bufferObject struct
// from the front of the buffer queue.
bufferObject dequeue(){
	bufferObject front = queue[queueFront];
	queueFront = (queueFront + 1) % queueCapacity;
	queueSize--;
	return front;
}


/**************************************************************************************************/
// Processing Functions //
// Producer function
// Single producer thread that pefroms memory mapped IO 
// and breaks each file into pages, each stored in a bufferObject struct.
// Finally, enqueues that object into buffer.
void* producer(void* arg){
	char** files = (char**) arg;
	char* mappedMemory;
	struct stat fileStats;
	int fd;
	int numPagesInFile;
	int lastPageSize;	// Possibly less than default page size
	int i;
	int j;

	// Loop through files list
	for (i = 0; i < totalFilesCount; i++){
		fd = open(files[i], O_RDONLY);
		// Check if file opening failed
		if (fd == -1){
			printf("Error occured while attempting to open file: %s. Aborting program...\n", files[i]);
			exit(1);
		}
		numPagesInFile = 0;
		lastPageSize = 0;

		// Fetch file stats
		if (fstat(fd, &fileStats) == -1){
			printf("Error occured while attempting to fetch stats of file: %s. Ignoring said file and continuing...\n", files[i]);
			close(fd);
			continue;
		}
		if (fileStats.st_size == 0){
			printf("file: %s was 0 bytes. Ignoring said file and continuing...\n", files[i]);
			close(fd);
			continue;
		}
		
		// Run file pages calculations
		numPagesInFile = fileStats.st_size / pageSize;	// Integer division, possible remainder checked next line
		if ( ((double) fileStats.st_size / pageSize) > numPagesInFile ){
			numPagesInFile++;
			lastPageSize = fileStats.st_size % pageSize;
		}
		else {	// Perfect alignment
			lastPageSize = pageSize;
		}

		pagesPerFile[i] = numPagesInFile;
		totalPagesCount += numPagesInFile;
		
		// Memory map file contents
		mappedMemory = mmap(NULL, fileStats.st_size, PROT_READ, MAP_SHARED, fd, 0);
		if (mappedMemory == MAP_FAILED){
			printf("Failed memory mapping, aborting program...\n");
			close(fd);
			exit(1);
		}

		// Create buffer objects for all pages in file and enqueue them
		for (j = 0; j < numPagesInFile; j++){
			pthread_mutex_lock(&glock);
			// Condition variable while loop for when queue is full
			while (queueSize == queueCapacity){
				pthread_cond_broadcast(&queueFull);	// Signal all consumer threads waiting
				pthread_cond_wait(&queueEmpty, &glock);	// Wait for signal from a consumer thread
			}
			pthread_mutex_unlock(&glock);
			
			bufferObject curr;
			if (j == numPagesInFile - 1){	// Properly set last page size
				curr.pageSize = lastPageSize;
			}
			else {
				curr.pageSize = pageSize;
			}
			curr.fileNumber = i;
			curr.pageNumber = j;
			curr.address = mappedMemory;
			mappedMemory += pageSize;	// Move mapped file memory seek to next page

			// Enqueue buffer object with lock
			pthread_mutex_lock(&glock);
			enqueue(curr);
			pthread_mutex_unlock(&glock);
			pthread_cond_signal(&queueFull); // Wake single sleeping consumer thread to handle this recently enqueued object
		}

		// Close file before moving on to next one
		close(fd);
	}
	
	// Mark producer work as done and broadcast it
	producerThreadDone = 1;
	pthread_cond_broadcast(&queueFull);

	return NULL;
}

// Compression function
// Runs RLE compression algorithm on a single file page (bufferObject).
compressedPage compressPage(bufferObject b){
	compressedPage cp;
	cp.count = malloc(b.pageSize * sizeof(int));
	char* contents = malloc(b.pageSize * sizeof(pageSize));	// Initially allocate max memory for worst case scenario
	int countPointer = 0;
	int i;
	
	// Loop through page contents and run RLE on them
	for (i = 0; i < b.pageSize; i++){
		// Save character and mark as one instance
		contents[countPointer] = b.address[i];
		cp.count[countPointer] = 1;
		// Count for potential repeated instances of same char
		while ( (i+1 < b.pageSize) && (b.address[i+1] == b.address[i])){
			cp.count[countPointer]++;
			i++;
		}
		countPointer++;
	}
	
	// Now we know the newly compressed page info
	cp.size = countPointer;
	cp.data = realloc(contents, countPointer);	// Reallocate to save memory if not worst case scenario
	
	return cp;
}

// Calculates the relative file page position/index
// in final output array of compressed pages (compressedPage structs).
int getCompressedPageIndex(bufferObject b){
	int index = 0;
	int i;
	// Count all pages in all previously recorded files so far
	for (i = 0; i < b.fileNumber; i++){
		index += pagesPerFile[i];
	}
	
	// Now find page number in current file
	index += b.pageNumber;

	return index;
}
		
// Consumer threads function.
// Attempts to run the whole compression process
// on specific page of a specific file. 
// To do so, needs to call both compressedPageIndex() & compressPage()
// with proper locks and condition variables.
void* consumer(void* arg){
	do {
		pthread_mutex_lock(&glock);
		// First check if queue is empty and producer not done yet
		while (queueSize == 0 && producerThreadDone == 0){
			// Signal empty queue and wait for wake up call from producer
			pthread_cond_signal(&queueEmpty);
			pthread_cond_wait(&queueFull, &glock);
		}

		// Check if the condition that broke the while loop is actually producer done, exit then
		if (producerThreadDone == 1 && queueSize == 0){
			pthread_mutex_unlock(&glock);
			return NULL;
		}

		// Dequeue bufferObject struct to process
		bufferObject bObj = dequeue();	// Dequeuing potentially opened up space from a full queue, let producer know in next line!
		if (producerThreadDone == 0){
			pthread_cond_signal(&queueEmpty);
		}
		pthread_mutex_unlock(&glock);

		// Process buffer object
		int index = getCompressedPageIndex(bObj);
		outputStream[index] = compressPage(bObj);

	} while ( ! (producerThreadDone == 1 && queueSize == 0) );

	return NULL;
}

// Writes output to stdout stream.
// Generates final encoded string output
// by combining the compressed pages stream
// into a single string to be written as bytes to stdout.
void writeOutput(){
	// Allocate worst case biggest possible encoded string
	// Will only write actual bytes to stdout
	char* output = malloc(totalPagesCount * pageSize * (sizeof(int) + sizeof(char)));
	char* outputAnchorPointer = output;	// Mark starting memory address of output, used to calculate actual memory
	int i;
	int j;
	char character;
	int characterCount;

	for (i = 0; i < totalPagesCount; i++){
		// Final optimization; check if a page's last character is identical to the next's first, combine if so
		if (i < totalPagesCount - 1){
			if (outputStream[i].data[outputStream[i].size - 1] == outputStream[i+1].data[0]){
				// Combine by accounting for the 'next' page, and reduce current page size
				outputStream[i+1].count[0] += outputStream[i].count[outputStream[i].size - 1];
				outputStream[i].size--; 
			}
		}

		// Start writing data ino output array and move pointer along
		for (j = 0; j < outputStream[i].size; j++){
			character = outputStream[i].data[j];
			characterCount = outputStream[i].count[j];

			// Format of encoding is: 4 byte integer count followed by 1 byte character
			// Place character in memory by derefrencing, then move pointer along respective size
			*((int*)output) = characterCount;
			output += sizeof(int);

			*((char*)output)  = character;
			output += sizeof(char);
		}
	}

	// Finally, write whole array as one chunk to stdout
	fwrite(outputAnchorPointer, output - outputAnchorPointer, 1, stdout);

}


int main(int argc, char* argv[]){
	if (argc <= 1){
		printf("No files selected to zip, exiting program..\n");
		exit(1);
	}

	// Use info from arguments to initialize critical global variables
	totalFilesCount = argc - 1;
	//pageSize = getpagesize();
	pageSize = 1000000;
	pagesPerFile = malloc(totalFilesCount * sizeof(int));
	totalThreads = get_nprocs();

	int i;
	outputStream = malloc(1000000 * sizeof(compressedPage));
	
	struct timeval start;
   	struct timeval end;
	gettimeofday(&start, NULL);
	
	// Assign one CPU to producer thread, all remaining as consumer threads
	pthread_t producerID;
	pthread_create(&producerID, NULL, producer, argv+1);

	pthread_t consumerIDs[totalThreads-1];
	for(i = 0; i < totalThreads-1; i++){
		pthread_create(&consumerIDs[i], NULL, consumer, NULL);
	}

	// Wait for both consumers and producer threads to exit
	for (i = 0; i < totalThreads-1; i++){
		pthread_join(consumerIDs[i], NULL);
	}

	pthread_join(producerID, NULL);

	// Write to stdout in order of files and pages generated by consumers
	writeOutput();
	
	gettimeofday(&end, NULL);
    fprintf(stderr, "Efficient parallel zip time is : %f seconds\n", (float) (end.tv_sec * 1000000 + end.tv_usec - start.tv_sec * 1000000 - start.tv_usec) / 1000000);

/*

	// Free dynamically allocated memory
	for (i = 0; c < totalPagesCount; i++){
		free(outputStream[i].data)
		free(outputStream[i].count);
	}
	free(outputStream);
	free(pagesPerFile);

*/

	return 0;

	}






