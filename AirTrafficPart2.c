#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>

char* filename= "planes.log";
int begtime;
int simtime = 10;
int t = 1;
double prob = 0.5;
int n = 2;
pthread_mutex_t* atc_main_lock;
pthread_cond_t* atc_main_cond;

typedef struct{							// Every plane has its own condition to be signaled individually by
	int ID;								// air traffic control
	//pthread_t tid;
	pthread_cond_t* cond;
	pthread_mutex_t* p_lock;
	time_t arrival_time;
}plane;

typedef struct queue_element{			// Elements of the queue
	plane* now;							// Every element of the queue points to the next element,
	struct queue_element* next;			// so that we don't need to store queue capacity.
}queue_element;

typedef struct queue{					// Queue implementation
        int size;						
        struct queue_element* first;
        struct queue_element* last;
}queue;


queue* Landing_Queue;					// Queue declarations
queue* TakeOff_Queue;
//queue* Emergency_Queue;

queue * createQueue(){					// Initializiation of a queue with this create method
        queue *Q;
        Q = (queue *)malloc(sizeof(queue));
        Q->size = 0;
        Q->first = NULL;
        Q->last = NULL;
        return Q;
}

plane * get(queue *Q){					// Pop from a given queue
        if(Q->size>0){
			plane* p = (plane*) (Q->first)->now;
			Q->first= (Q->first)->next;
			Q->size--;
            return p;
        }
        return NULL;
}

void put(queue *Q, plane *p){			// Push the given plane into the given queue
       
     struct queue_element* new_element = (queue_element*)malloc(sizeof(queue_element));
     new_element->now=p;
     new_element->next=NULL;
     if(Q->size==0){					// If the queue is empty, then the new element will 
		 Q->first=new_element;			// be both the first and the last element
		 Q->last=new_element;
		 Q->size++;
		 return;
       } 
       Q->size++;
       (Q->last)->next = new_element;
       Q->last= new_element;
return;
}

int pthread_sleep (int seconds)
{
   pthread_mutex_t mutex;
   pthread_cond_t conditionvar;
   struct timespec timetoexpire;
   if(pthread_mutex_init(&mutex,NULL))
    {
      return -1;
    }
   if(pthread_cond_init(&conditionvar,NULL))
    {
      return -1;
    }
   struct timeval tp;
   //When to expire is an absolute time, so get the current time and add //it to our delay time
   gettimeofday(&tp, NULL);
   timetoexpire.tv_sec = tp.tv_sec + seconds; 
   timetoexpire.tv_nsec = tp.tv_usec * 1000;

   pthread_mutex_lock (&mutex);
   int res =  pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
   pthread_mutex_unlock (&mutex);
   pthread_mutex_destroy(&mutex);
   pthread_cond_destroy(&conditionvar);

   //Upon successful completion, a value of zero shall be returned
   return res;

}

plane* p;											// Plane declaration

void *air_traffic_control(void* arg){				// Tower thread
	
	while(time(NULL)<= begtime+simtime){			
		pthread_mutex_lock(atc_main_lock);
		pthread_cond_wait(atc_main_cond, atc_main_lock); 
		if(TakeOff_Queue->size>0 && (Landing_Queue->size==0 || TakeOff_Queue->size>=3 || (time(NULL)-begtime-(TakeOff_Queue->first->now->arrival_time)) >= 10*t)){	
			plane* ready = get(TakeOff_Queue);						 
			printf("Takeoff plane with ID: %d can depart now\n", ready->ID);
			pthread_mutex_lock(ready->p_lock);							// To protect our simulation from starvation
			pthread_cond_signal(ready->cond);							// we added new checks according to the requirements
			pthread_cond_wait(ready->cond, ready->p_lock);	
			pthread_mutex_unlock(ready->p_lock);
					
		}else if(Landing_Queue->size > 0){ 						// Tower checks if there are any planes in the landing queue and then sends signal
			plane* ready = get(Landing_Queue); 
			printf("Landing plane with ID: %d can land now\n", ready->ID);
			pthread_mutex_lock(ready->p_lock);
			pthread_cond_signal(ready->cond);
			pthread_mutex_unlock(ready->p_lock);
			pthread_cond_wait(ready->cond, ready->p_lock);		
			pthread_mutex_unlock(ready->p_lock);
		}
		
		pthread_cond_signal(atc_main_cond);
		pthread_mutex_unlock(atc_main_lock);
			
	}
	pthread_exit(0);
}

void *landing (void* arg){					// landing function performs landing in 2t seconds
	plane* p = (plane*) arg;
	//pthread_mutex_lock(p->p_lock);
	pthread_cond_wait(p->cond, p->p_lock);
	pthread_sleep(2*t);
	FILE* f;
	f=fopen(filename, "a");
	fprintf(f, " %d\t L\t\t %ld\t\t %ld\t\t %ld\n", p->ID, p->arrival_time, time(NULL)-begtime, (time(NULL)-begtime)-p->arrival_time);
	fclose(f);
	printf("Landing plane with ID: %d landed at t: %ld\n", p->ID, time(NULL)-begtime);
	pthread_cond_signal(p->cond);
	pthread_mutex_unlock(p->p_lock);


	pthread_exit(0);
}

void *departing(void* arg){					// departing function performs takeoff in 2t seconds
	plane* p = (plane*) arg;
	//pthread_mutex_lock(p->p_lock);
	pthread_cond_wait(p->cond, p->p_lock);
	pthread_sleep(2*t);
	FILE* f;
	f=fopen(filename, "a");
	fprintf(f, " %d\t D\t\t %ld\t\t %ld\t\t %ld\n", p->ID, p->arrival_time, time(NULL)-begtime, (time(NULL)-begtime)-p->arrival_time);
	fclose(f);
	printf("Takeoff plane with ID: %d departed at t: %ld\n", p->ID,  time(NULL)-begtime);
	pthread_cond_signal(p->cond);
	pthread_mutex_unlock(p->p_lock);
	
pthread_exit(0);
}

void *print_queue (void* arg){								// Prints the current snapshot of both landing and departing queues
	while(time(NULL)<=begtime+simtime){						// If at second t a planes send request, we don't show that plane in
		if(time(NULL)-begtime >= n){						// that time's queue. If a plane is created at second t, that plane is
	queue_element* lelement = Landing_Queue->first;			// included in that time's queue. THIS IS OUR ASSUMPTION
	queue_element* telement = TakeOff_Queue->first;
	printf("Current Landing Queue: [");
	while(lelement != NULL){
		printf(" %d ", lelement->now->ID);
		lelement=lelement->next;
	}
	printf("] at t: %ld\n", time(NULL)-begtime);
	printf("Current TakeOff Queue: [");
	while(telement != NULL){
		printf(" %d ", telement->now->ID);
		telement=telement->next;
	}
	printf("] at t: %ld\n", time(NULL)-begtime);
}
	pthread_sleep(t);
}
	pthread_exit(0);
}	

int main(int argc, char *argv[]){			// Initially we created two planes one for landing (thy) and one for departing (pegasus)
	
	int i;
	for(i=0; i<argc; i++){
		if(strncmp(argv[i], "-s", 3)==0){
			simtime= strtol(argv[i+1], NULL, 10);
		}
		if(strncmp(argv[i], "-p", 3)==0){
			prob = strtod(argv[i+1], NULL);
		}
		if(strncmp(argv[i], "-n", 3)==0){
			n = strtol(argv[i+1], NULL, 10);
		}
	}
	
	
	srand(3);
	begtime = time(NULL);
	int odds=1;
	int evens=0;
	pthread_t* atc = (pthread_t*)malloc(sizeof(pthread_t));			 // We created 3 threads for air traffic control, landing and departing
	pthread_t* land = (pthread_t*)malloc(sizeof(pthread_t));
	pthread_t* depart = (pthread_t*)malloc(sizeof(pthread_t));
	pthread_t* printQueue = (pthread_t*)malloc(sizeof(pthread_t));
	pthread_cond_t* pegasus_cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t)); 
	pthread_cond_t* thy_cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));		
	pthread_mutex_t* pegasus_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_t* thy_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_cond_init(pegasus_cond, NULL);
	pthread_cond_init(thy_cond, NULL);
	pthread_mutex_init(pegasus_mutex, NULL);						// Initializations of planes and queues
	pthread_mutex_init(thy_mutex, NULL);
	
	atc_main_lock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	atc_main_cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));	
	pthread_cond_init(atc_main_cond, NULL);
	pthread_mutex_init(atc_main_lock, NULL);
	
	FILE* f;
	f=fopen(filename, "w");
	fprintf(f, "PlaneID\t Status\t Request Time\t Runway Time\t Turnaround Time\t\n");
	fclose(f);
	
	plane* pegasus = (plane*)malloc(sizeof(plane));
	plane* thy = (plane*)malloc(sizeof(plane));
	pegasus->ID =0;
	pegasus->arrival_time = time(NULL)-begtime;
	pegasus->cond = pegasus_cond;
	pegasus->p_lock=pegasus_mutex;
	
	thy->ID=1;
	thy->arrival_time = time(NULL)-begtime;
	thy->cond = thy_cond;
	thy->p_lock=thy_mutex;
	
	Landing_Queue = createQueue();
	TakeOff_Queue = createQueue();
	put(Landing_Queue, thy);
	put(TakeOff_Queue, pegasus);
	
	printf("INITIAL:New departing plane is created with ID: %d at t: %ld\n", pegasus->ID, pegasus->arrival_time);
	printf("INITIAL:New landing plane is created with ID: %d at t: %ld\n", thy->ID, thy->arrival_time);
	
	pthread_create(land, NULL, landing, (void*) (Landing_Queue->first)->now);
	pthread_create(depart, NULL, departing, (void*) (TakeOff_Queue->first)->now);
	pthread_create(atc, NULL, air_traffic_control, NULL);
	
	pthread_create(printQueue, NULL, print_queue, NULL);


	while(time(NULL)<= begtime+simtime){
		pthread_sleep(t);									// Tower sleeps for t seconds not to perform any other operations while new planes are arriving

		double a = (double)rand()/RAND_MAX;
															// If the chance of an incoming landing plane is less than or equal to the
		if(a<=prob){										// probability of creating a new landing plane, then a new landing plane will be created
			pthread_t* new_thread = (pthread_t*)malloc(sizeof(pthread_t));
			pthread_cond_t* new_cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
			pthread_mutex_t* new_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
			pthread_cond_init(new_cond, NULL);
			pthread_mutex_init(new_mutex, NULL);
			plane* new_p = (plane*)malloc(sizeof(plane));
			new_p->ID =odds + 2;
			new_p->arrival_time = time(NULL)-begtime;
			new_p->cond = new_cond;
			new_p->p_lock=new_mutex;
			put(Landing_Queue, new_p);						// Since the new plane is a landing one, we add this to the end of the landing queue
			odds=odds+2;
			printf("New landing plane is created with ID: %d at t: %ld\n", new_p->ID, new_p->arrival_time);
			pthread_create(new_thread, NULL, landing, (void*) new_p);
		
			}
		double b = (double)rand()/RAND_MAX;					// If the chance of an incoming departing plane is less than or equal to the
		if(b<(1-prob)){										// probability of creating a new departing plane, then a new departing plane will be created
			pthread_t* new_thread = (pthread_t*)malloc(sizeof(pthread_t));
			pthread_cond_t* new_cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
			pthread_mutex_t* new_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
			pthread_cond_init(new_cond, NULL);
			pthread_mutex_init(new_mutex, NULL);
			plane* new_p = (plane*)malloc(sizeof(plane));
			new_p->ID =evens + 2;
			new_p->arrival_time = time(NULL)-begtime;
			new_p->cond = new_cond;
			new_p->p_lock=new_mutex;
			put(TakeOff_Queue, new_p);						// Since the new plane is a departing one, we add this to the end of the takeoff queue
			evens=evens+2;
			printf("New takeoff plane is created with ID: %d at t: %ld\n", new_p->ID, new_p->arrival_time);
			pthread_create(new_thread, NULL, departing, (void*) new_p);
	
		}
		
		pthread_cond_signal(atc_main_cond);
		//pthread_mutex_lock(atc_main_lock);
		//pthread_cond_wait(atc_main_cond, atc_main_lock);
		pthread_mutex_unlock(atc_main_lock);
		
		
	}
	printf("End of simulation\n");
	return 0;
}
