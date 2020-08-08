/*
 * Implements FIFO, CLOCK, and LRU page-replacement schemes
 * Simulates the actions of a virtual memory subsystem.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define LOG(var) fprintf(stderr,"[LOG] %s:%d  :: %s\n",__func__,__LINE__, var);

/*
 * Some compile-time constants.
 */

#define REPLACE_NONE 0
#define REPLACE_FIFO 1
#define REPLACE_LRU  2
#define REPLACE_CLOCK 3
#define REPLACE_OPTIMAL 4


#define TRUE 1
#define FALSE 0
#define PROGRESS_BAR_WIDTH 60
#define MAX_LINE_LEN 100


/*
 * Some function prototypes to keep the compiler happy.
 */
int setup(void);
int teardown(void);
int output_report(void);
long resolve_address(long, int);
void error_resolve_address(long, int);

/* debug stuff */
void print(const char*);
long update_table(int frame, long page, long offset, int* swap);
long debug_last_mod = 0;

/*
 * Variables used to keep track of the number of memory-system events
 * that are simulated.
 */
int page_faults = 0;
int mem_refs    = 0;
int swap_outs   = 0;
int swap_ins    = 0;

int fifo_index = 0; //keeps track of next page to replace
int lru_mod = 0; //keeps track of time stamp for lru
int clock_hand = 0; //keeps track of page currently pointed
/*
 * Page-table information. You may want to modify this in order to
 * implement schemes such as CLOCK. However, you are not required to
 * do so.
 */
struct page_table_entry *page_table = NULL;
struct page_table_entry {
    long page_num;
    int dirty; //for clock
	int modified;  //for lru
    int free;
};


/*
 * These global variables will be set in the main() function. The default
 * values here are non-sensical, but it is safer to zero out a variable
 * rather than trust to random data that might be stored in it -- this
 * helps with debugging (i.e., eliminates a possible source of randomness
 * in misbehaving programs).
 */

int size_of_frame = 0;  /* power of 2 */
int size_of_memory = 0; /* number of frames */
int page_replacement_scheme = REPLACE_NONE;


/*
 * Function to convert a logical address into its corresponding 
 * physical address. The value returned by this function is the
 * physical address (or -1 if no physical address can exist for
 * the logical address given the current page-allocation state.
 */

long resolve_address(long logical, int memwrite)
{
	///char outbuff[200];
	
    int i;
    long page, frame;
    long offset;
    long mask = 0;
    long effective;

    /* Get the page and offset */
    page = (logical >> size_of_frame);
	///sprintf(outbuff,"Program address 0x%08lx : VPN = 0x%08lx (using framesize = %d)",logical,page,size_of_frame);
    ///LOG(outbuff);
    for (i=0; i<size_of_frame; i++) {
        mask = mask << 1;
        mask |= 1;
    }
    offset = logical & mask;
	///sprintf(outbuff,"Apply 0x%08lx to 0x%08lx to get offset = 0x%08lx", mask, logical, offset);
    ///LOG(outbuff);

	
    /* Find page in the inverted page table. */
	///LOG("Does the VM have the page?");
    frame = -1;
    for ( i = 0; i < size_of_memory; i++ ) {
		//free = 1 , available
        if (!page_table[i].free && page_table[i].page_num == page) { // if free==0 at page #, ie occupied
            frame = i;  //found page in page table at frame = i
            break;
        }
    }
    /* If frame is not -1 (found page), then we can successfully resolve the
     * address and return the result. */
    if (frame != -1) {
		///sprintf(outbuff, "--> Yes! Best case scenario, page is in the page_table @ frame=%ld", frame);
        ///LOG(outbuff);
        
		effective = (frame << size_of_frame) | offset;
		
		if(page_replacement_scheme == REPLACE_LRU){
			page_table[frame].modified = lru_mod; //update modified to current timestamp since page was referenced
			lru_mod++;
			swap_ins++; //increased because page was modified
		}
		
		if(page_replacement_scheme == REPLACE_CLOCK){
			//if dirty bit is off, turn on
			if(page_table[frame].dirty == 0){
				page_table[frame].dirty = 1; //turn on dirty bit
				swap_ins++; //increased because page was modified
			}
			//note: swap_ins is not increased if page is referenced and dirty bit is already on, since not modified
		}
		///sprintf(outbuff,"==> Physical address for page 0x%08lx = 0x%ld:%ld = 0x%08lx", page, frame, offset, effective);
        ///LOG(outbuff);
        return effective;
    }


    /* If we reach this point, there was a page fault (accessed page that is not currently mapped 
	 * ie. page not in page table). Find a free frame. */
	///LOG("Damn it, page fault ... any free frames?");
    page_faults++;
	
	//this loop to fill up page table which is initially all free, ie. first faults
    for ( i = 0; i < size_of_memory; i++) {
        if (page_table[i].free) {
            frame = i;
            break;
        }
    }
    /* If we found a free frame, then patch up the
     * page table entry and compute the effective
     * address. Otherwise return -1.
     */
    if (frame != -1) {
		//INSERTING into free frame
		//don't increment swap_ins on first fault
		
        ///sprintf(outbuff, "--> Yes! Found a free frame, page is in the page_table @ frame=%ld", frame);
        ///LOG(outbuff);
		
        page_table[frame].page_num = page;
        page_table[frame].free = FALSE;
		
		if(page_replacement_scheme == REPLACE_LRU){
			page_table[frame].modified = lru_mod; //update modified to current timestamp
			lru_mod++;
		}
		
		if(page_replacement_scheme == REPLACE_CLOCK){
			page_table[frame].dirty = 1; //initialize to 1
		}
        //swap_ins++; oommented out since swap_ins excluded from first faults
        effective = (frame << size_of_frame) | offset;
		
		///sprintf(outbuff,"==> Physical address for page 0x%08lx = 0x%ld:%ld = 0x%08lx", page, frame, offset, effective);
        ///LOG(outbuff);
        return effective;
    } else {
		///LOG("Ugh !?# There's nothing free, let's find a victim.");
        ///LOG("Here is where you need to apply your page replacement scheme.");
		
		//If reached this point, page_table full. Select page to replace according to FIFO, LRU, or CLOCK
		if(page_replacement_scheme == REPLACE_FIFO){
			frame = fifo_index;
			fifo_index++;
			if(fifo_index >= size_of_memory){
				fifo_index = 0;
			}
			
			//replace page at frame with new page
			page_table[frame].page_num = page;
			page_table[frame].free = FALSE;
			swap_outs++;
			swap_ins++;
			effective = (frame << size_of_frame) | offset;
			return effective;
		}else if(page_replacement_scheme == REPLACE_LRU){
			//find frame with lowest mod
			frame = 0;
			int lowest_mod = page_table[0].modified;
			for(i = 0; i < size_of_memory; i++){
				if (lowest_mod > page_table[i].modified){
					lowest_mod = page_table[i].modified;
					frame = i;
				}
			}
			
			//replace page least recently used (lowest mod) with new page
			page_table[frame].page_num = page;
			page_table[frame].free = FALSE;
			page_table[frame].modified = lru_mod; //update modified to current timestamp
			lru_mod++;
			swap_outs++;
			swap_ins++;
			effective = (frame << size_of_frame) | offset;
			return effective;
		}else if(page_replacement_scheme == REPLACE_CLOCK){
			//find next page with dirty bit = 0, ie. will replace pages that haven't been referenced
			//for one complete revolution of the clock
			
			//while victim page not found
			int victim_found = 0;
			while(victim_found != 1){
				if(page_table[clock_hand].dirty == 0){
					//replace current page
					frame = clock_hand;
					victim_found = 1;
				}else{
					//reset dirty bit
					page_table[clock_hand].dirty = 0;
					swap_ins++; //increased because page was modified
				}
				//advance clock pointer
				clock_hand++;
				if(clock_hand >= size_of_memory){
					clock_hand = 0;
				}
			}
			
			page_table[frame].page_num = page;
			page_table[frame].free = FALSE;
			page_table[frame].dirty = 1; //initialize to 1
			swap_outs++;
			swap_ins++;
			effective = (frame << size_of_frame) | offset;
			return effective;
		}
        return -1;
    }
}

void print(const char* instruction) {
    int frame;
    long page_num;
    int dirty;
    int modified; 
    int free;

    fprintf(stderr, "-------------------------------------------------------------\n");
    fprintf(stderr, "%s   mem_refs: %d  page_faults: %d  swap_ins: %d  swap_outs: %d\n\n", 
            instruction,mem_refs, page_faults, swap_ins, swap_outs);
    fprintf(stderr, "FRAME\t|PAGE\t\t|MOD\t|DIRTY\t\n");
    fprintf(stderr, "-------------------------------------------------------------\n");
    for ( frame = 0; frame < size_of_memory; frame++) {
        free = page_table[frame].free;
        modified = page_table[frame].modified;
        page_num = page_table[frame].page_num;
        dirty = page_table[frame].dirty;
        char* updated = "";

        if(debug_last_mod == frame)
            updated = "<-";

        if(free)
            fprintf(stderr, "%d\t|FREE\t\t|%d\t|%d\t%s\n", frame, modified, dirty,updated);
        else
            fprintf(stderr," %d\t|0x%lx\t|%d\t|%d\t%s\n", frame, page_num, modified, dirty,updated);
    }
    fprintf(stderr, "-------------------------------------------------------------\n");
    fflush(stderr);
}


/*
 * Super-simple progress bar.
 */
void display_progress(int percent)
{
    int to_date = PROGRESS_BAR_WIDTH * percent / 100;
    static int last_to_date = 0;
    int i;

    if (last_to_date < to_date) {
        last_to_date = to_date;
    } else {
        return;
    }

    printf("Progress [");
    for (i=0; i<to_date; i++) {
        printf(".");
    }
    for (; i<PROGRESS_BAR_WIDTH; i++) {
        printf(" ");
    }
    printf("] %3d%%", percent);
    printf("\r");
    fflush(stdout);
}


// creates page table
int setup()
{
    int i;

    page_table = (struct page_table_entry *)malloc(
        sizeof(struct page_table_entry) * size_of_memory
    );

    if (page_table == NULL) {
        fprintf(stderr,
            "Simulator error: cannot allocate memory for page table.\n");
        exit(1);
    }

    for (i=0; i<size_of_memory; i++) {
        page_table[i].free = TRUE;
    }

    return -1;
}

//clean up for any allocated memory (ie. malloc)
int teardown()
{

    return -1;
}


void error_resolve_address(long a, int l)
{
    fprintf(stderr, "\n");
    fprintf(stderr, 
        "Simulator error: cannot resolve address 0x%lx at line %d\n",
        a, l
    );
    exit(1);
}


int output_report()
{
    printf("\n");
    printf("Memory references: %d\n", mem_refs);
    printf("Page faults: %d\n", page_faults);
    printf("Swap ins: %d\n", swap_ins);
    printf("Swap outs: %d\n", swap_outs);

    return -1;
}


int main(int argc, char **argv)
{
    /* For working with command-line arguments. */
    int i;
    char *s;

    /* For working with input file. */
    FILE *infile = NULL;
    char *infile_name = NULL;
    struct stat infile_stat;
    int  line_num = 0;
    int infile_size = 0;

    /* For processing each individual line in the input file. */
    char buffer[MAX_LINE_LEN];
    long addr;
    char addr_type;
    int  is_write;

    /* For making visible the work being done by the simulator. */
    int show_progress = FALSE;

    /* Process the command-line parameters. Note that the
     * REPLACE_OPTIMAL scheme is not required for A#3.
     */
    for (i=1; i < argc; i++) {
        if (strncmp(argv[i], "--replace=", 9) == 0) {
            s = strstr(argv[i], "=") + 1;
            if (strcmp(s, "fifo") == 0) {
                page_replacement_scheme = REPLACE_FIFO;
            } else if (strcmp(s, "lru") == 0) {
                page_replacement_scheme = REPLACE_LRU;
            } else if (strcmp(s, "clock") == 0) {
                page_replacement_scheme = REPLACE_CLOCK;
            } else if (strcmp(s, "optimal") == 0) {
                page_replacement_scheme = REPLACE_OPTIMAL;
            } else {
                page_replacement_scheme = REPLACE_NONE;
            }
        } else if (strncmp(argv[i], "--file=", 7) == 0) {
            infile_name = strstr(argv[i], "=") + 1;
        } else if (strncmp(argv[i], "--framesize=", 12) == 0) {
            s = strstr(argv[i], "=") + 1;
            size_of_frame = atoi(s);
        } else if (strncmp(argv[i], "--numframes=", 12) == 0) {
            s = strstr(argv[i], "=") + 1;
            size_of_memory = atoi(s);
        } else if (strcmp(argv[i], "--progress") == 0) {
            show_progress = TRUE;
        }
    }

    if (infile_name == NULL) {
        infile = stdin;
    } else if (stat(infile_name, &infile_stat) == 0) {
        infile_size = (int)(infile_stat.st_size);
        /* If this fails, infile will be null */
        infile = fopen(infile_name, "r");  
    }


    if (page_replacement_scheme == REPLACE_NONE ||
        size_of_frame <= 0 ||
        size_of_memory <= 0 ||
        infile == NULL)
    {
        fprintf(stderr, "usage: %s --framesize=<m> --numframes=<n>", argv[0]);
        fprintf(stderr, " --replace={fifo|lru|optimal} [--file=<filename>]\n");
        exit(1);
    }

    setup();

    while (fgets(buffer, MAX_LINE_LEN-1, infile)) {
        line_num++;
        if (strstr(buffer, ":")) {
            sscanf(buffer, "%c: %lx", &addr_type, &addr);
			
			///LOG(buffer);
			
            if (addr_type == 'W') {
                is_write = TRUE;
            } else {
                is_write = FALSE;
            }

            if (resolve_address(addr, is_write) == -1) {
                error_resolve_address(addr, line_num);
            }
            mem_refs++;
			            
            //print(buffer);
        } 

        if (show_progress) {
            display_progress(ftell(infile) * 100 / infile_size);
        }
    }
    

    teardown();
    output_report();

    fclose(infile);

    exit(0);
}
